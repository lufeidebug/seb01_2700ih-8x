#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SEB01 整机老化测试 PC 工具。

走 pogopin 通信协议，不另起文本 AT。
帧: FLAG + FROM_TO + PATH + CMD + DLEN(大端) + DATA + CRC16(大端)
CRC 与耳机 sndp_comm_protocol_calc_crc16 一致，初值 0xA1B2。

指令(与固件 sndp_comm_cmd.h 一致):
  0x7D  配置，DATA 固定 27 字节 = 9 × [大项ID(1) + 是否测试(1) + 目标次数(1)]，
        严格按大项 ID 0..8 顺序，不管有没有勾选都一次全带；
        发 1 字节 0x00 表示清除配置并退出老化
  0x7A  启动，无 DATA，使用已下发的配置(未配置任何大项时返回执行失败)
  0x7B  停止，无 DATA，清掉"老化中"标志，已完成轮次与配置保留，下次开机不再续测
  0x7C  查询状态，应答 89 字节
  0x7E  查询配置，应答 28 字节 = error(1) + 9 × [大项ID(1) + 是否测试(1) + 目标次数(1)]

补电复核（两条路，互补）:
  1) 耳机侧 8 号"终检"大项自带: 传感器复检 -> 等进入充电(最长 30 秒) -> 补电到 80% 后停充。
     跑到这个阶段时状态栏会提示"等待进入充电"/"补电中"。
  2) 本工具的"充电检测"按钮: 不进老化流程时单独复核一遍 —— 30 秒内每秒轮询 0x7C,
     要求「在充电(充电中/已充满) 且 电量 ≥80%」，两个条件都满足即通过;
     30 秒到还没满足则判失败, 并说明是"没充上"还是"电量不够"。

大项 ID(工具与耳机共用，见下方 CFG_ORDER；固件同名 sndp_aging_test.h 的 SNDP_AGING_ITEM_xxx_ID):
  0 charge(保留)  1 sram   2 flash  3 soft_rst  4 hard_rst
  5 mid           6 high   7 bus    8 end

注意(与旧版上位机的差异):
  1) 启动不再携带参数，必须先 0x7D 配置再 0x7A 启动。
  2) 0x7C 应答由 37 字节变为 89 字节，字段顺序也变了，旧解析不兼容。
  3) 某个小项失败只跳过该大项剩余轮次并继续往后跑，不再终止整个老化；
     只有温度超过红线才立即终止。整体结果看 curr_fail_code。
  4) 大项全部走完时 curr_big_id = 0xFF。
  5) 0x7D/0x7E 改成定长数值格式(旧版是 "1,sram:1,..." 的 ASCII)，用大项 ID 而不是名称，
     而且 9 项每次都全带。旧 ASCII 只下发勾选项，没下发的项会保留耳机上的旧值，
     会出现"没勾硬重启、查询后又勾上了"的问题，新格式从结构上避免了。
"""

import json
import os
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None


FRAME_FLAG = 0xFE
DEV_ATE = 0x05
DEV_LEFT = 0x01
DEV_RIGHT = 0x02

CMD_BAT = 0x04
CMD_FW_VER = 0x4A
CMD_START = 0x7A
CMD_STOP = 0x7B
CMD_QUERY = 0x7C
CMD_SET_CFG = 0x7D
CMD_QUERY_CFG = 0x7E

CRC_INIT = 0xA1B2
# 软重启期间耳机不回应答, 允许连续丢几次再判定异常(默认 2s 轮询 => 约 20s)
MAX_QUERY_MISS = 10
# 串口断开后允许等待重连的上限(蓝牙 SPP 重连 + 耳机重启一般十几秒内恢复)
RECONNECT_TIMEOUT_S = 120
# 补电复核(充电检测): 这段时间内轮询 0x7C, 要求"在充电 且 电量 >= 80%"
CHARGE_CHECK_SECONDS = 30
CHARGE_CHECK_INTERVAL_MS = 1000
CHARGE_TARGET_PCT = 80
# sndp_dev_charging_status_e: 0 未充电 / 1 充电中 / 2 已充满
CHARGER_NAMES = {0: "未充电", 1: "充电中", 2: "已充满"}
CHARGER_OK_STATUS = (1, 2)
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "aging_tool.json")

BIG_DONE = 0xFF

# 页面大项 -> 配置参数名。charge 没有独立大项，仅占协议槽位，不在监控表里单独成行。
ITEMS = [
    ("sram", "SRAM", 1),
    ("flash", "FLASH", 1),
    ("soft_rst", "软重启", 10),
    ("hard_rst", "硬重启(固件跳过)", 10),
    ("mid", "中负载", 9),
    ("high", "高负载", 9),
    ("bus", "I2C 总线压力", 10),
    ("final", "终检(传感器/充电/补电)", 1),
]
CHARGE_KEY = "charge"

# 大项 ID -> 名称(4A.5 的表)。
# 8 号是终检: 传感器复检 -> 等进入充电(最长 30s) -> 补电到 80%
BIG_NAMES = {
    0: "charge(保留)",
    1: "SRAM",
    2: "FLASH",
    3: "软重启",
    4: "硬重启",
    5: "中负载",
    6: "高负载",
    7: "I2C 总线压力",
    8: "终检",
    BIG_DONE: "全部完成",
}
# 大项 ID -> 监控表行。硬重启当前固件跳过，仍保留行便于对照。
BIG_TO_ROW = {1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5, 7: 6, 8: 7}
# 固件强制跳过的大项 ID(见固件 AGING_SKIP_HARD_RST=1): 勾了也不会执行,
# 0x7C 里这几项的目标次数恒为 0, 界面按"跳过"显示。
FIRMWARE_SKIP_BIGS = {4}
ITEM_KEY_TO_CFG = {
    "charge": 0, "sram": 1, "flash": 2, "soft_rst": 3, "hard_rst": 4,
    "mid": 5, "high": 6, "bus": 7, "final": 8,
}
CFG_INDEX_TO_ITEM = {v: k for k, v in ITEM_KEY_TO_CFG.items()}
# 0x7D/0x7E 的固定顺序: 下标就是大项 ID, 与固件 SNDP_AGING_ITEM_xxx_ID 一一对应。
# 每一项每次都全带(不管有没有勾选), 双方都不做名称匹配。
CFG_ORDER = ("charge", "sram", "flash", "soft_rst", "hard_rst", "mid", "high", "bus", "final")
BIG_QTY = len(CFG_ORDER)
# 大项 key -> 监控表行(charge 没有独立行, 取不到)
ITEM_KEY_TO_ROW = {key: i for i, (key, _name, _default) in enumerate(ITEMS)}
# 目标次数在协议里占 1 个字节
CFG_TARGET_MAX = 255
# 顺序表与 ID 映射必须是同一份定义, 后面改错立刻能发现
assert ITEM_KEY_TO_CFG == {key: i for i, key in enumerate(CFG_ORDER)}, "CFG_ORDER 与 ITEM_KEY_TO_CFG 不一致"

# ---- 0x7C / 0x7E 报文尺寸: 全部由大项个数推导, 将来增减大项不用再改这里 ----
# 0x7C 头部 = error(1)+running(1)+total_times(2)+temperature(2)+bat_voltage(2)
#            +bat_percentage(1)+charger_status(1)+elapsed_ms(4)
#            +curr_big_id(1)+curr_sub_id(1)+curr_fail_code(1)
STATUS_HEAD_LEN = 17
STATUS_TARGET_OFF = STATUS_HEAD_LEN
STATUS_DONE_OFF = STATUS_TARGET_OFF + BIG_QTY * 2
STATUS_TIME_OFF = STATUS_DONE_OFF + BIG_QTY * 2
QUERY_DATA_LEN = STATUS_TIME_OFF + BIG_QTY * 4          # 8 个大项时 = 81
# 0x7E: error(1) + 大项数 × [大项ID(1) + 是否测试(1) + 目标次数(1)]
QUERY_CFG_DATA_LEN = 1 + BIG_QTY * 3                    # 8 个大项时 = 25

FAIL_CODES = {
    0: "无",
    1: "主控/器件",
    2: "Flash",
    3: "传感器",
    4: "VSENSOR",
    5: "温度超过 50℃",
    6: "充电异常",
    7: "电池电压/电量",
    8: "音频",
    9: "射频",
    10: "I2C 总线",
    11: "超时",
    12: "未知",
    # 13 是"音频通路被蓝牙占用": 治具环境问题(手机在放歌), 不是 DUT 音频故障。
    # 固件会先发 AVRCP PAUSE 尝试停掉手机播放, 停不下来才报这个码。
    13: "音频通路被蓝牙占用",
}
# 小项 ID(curr_sub_id, 4A.5 的表)
SUB_ITEMS = {
    0: "SRAM",
    1: "Flash",
    2: "传感器",
    3: "充电",
    4: "补电",
    5: "音频",
    6: "音量",
    7: "ANC",
    8: "32K/RTC",
    9: "I2C 总线",
    10: "射频",
    11: "软重启",
    12: "硬重启",
    13: "检测充电",
    14: "无",          # = 固件 SNDP_AGING_SUB_QTY, 表示"当前无小项"
    BIG_DONE: "无",
}
# 终检大项里的两个耗时步骤, 用于状态栏提示(值取自固件)
BIG_FINAL = 8
SUB_TOPUP = 4
SUB_CHARGE_WAIT = 13
BIG_CHARGE_WAIT_SEC = 30        # 固件 AGING_CHARGE_WAIT_SEC
TOPUP_TARGET_PCT = 80           # 固件 AGING_BAT_TOPUP_TARGET_PCT
# 与耳机 sndp_comm_path_id_e 数值一致。NONE 不进下拉，耳机会判非法通道。
PATHS = (
    (0x01, "TRACE UART"),
    (0x02, "POGOPIN UART"),
    (0x03, "SPP"),
    (0x04, "BLE"),
    (0x05, "MS"),
)

COMM_ERRORS = {
    0x00: "成功",
    0x07: "CRC 错误",
    0x19: "执行失败(未配置任何大项?)",
    0x20: "参数长度非法",
    0x21: "参数超出范围",
    0x22: "不在测试模式(指令被拦截, 常见于耳机固件过旧)",
    0x23: "电池盒电量错误",
    0x24: "蓝牙未初始化完成",
}


def crc16(data, init=CRC_INIT):
    """与 sndp_comm_protocol_calc_crc16 逐位一致。"""
    crc = init & 0xFFFF
    for b in data:
        crc = ((crc >> 8) | (crc << 8)) & 0xFFFF
        crc ^= b
        crc ^= (crc & 0xFF) >> 4
        crc ^= (crc << 12) & 0xFFFF
        crc ^= ((crc & 0xFF) << 5) & 0xFFFF
    return crc & 0xFFFF


def pack_frame(cmd, payload, ear, path):
    body = bytes([
        FRAME_FLAG,
        (DEV_ATE << 4) | ear,
        path,
        cmd,
        (len(payload) >> 8) & 0xFF,
        len(payload) & 0xFF,
    ]) + payload
    c = crc16(body)
    return body + bytes([(c >> 8) & 0xFF, c & 0xFF])


def hex_of(data):
    return " ".join("%02X" % b for b in data)


class ProtocolError(Exception):
    pass


class LinkError(ProtocolError):
    """串口链路异常(端口消失/句柄失效/发送失败)。

    与协议层错误分开: 链路异常不该按"耳机无应答"处理, 而应该走自动重连。
    """


class AgingTool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("SEB01 整机老化测试")
        # 监控表实测需要约 834px(含边框留白), 窗口比它窄时右边会被裁掉,
        # 最后一列"结果"首当其冲: "测试中"会缺字。所以最小宽度卡在 860。
        self.geometry("900x860")
        self.minsize(860, 780)
        self.configure(bg="#F3F4F6")
        style = ttk.Style(self)
        try:
            style.theme_use("vista")
        except tk.TclError:
            pass
        style.configure("TFrame", background="#F3F4F6")
        style.configure("TLabel", background="#F3F4F6")
        style.configure("TLabelframe", background="#F3F4F6")
        style.configure("TLabelframe.Label", background="#F3F4F6")
        style.configure("TCheckbutton", background="#F3F4F6")

        self.ser = None
        self.rx_buf = bytearray()
        self.rx_queue = queue.Queue()
        self.stop_reader = threading.Event()
        self.reader = None
        self.testing = False
        self.poll_job = None
        self.clock_job = None
        self.test_start = None
        self.test_elapsed = None
        self.device_elapsed_ms = None
        self.query_miss = 0
        self.link_down_since = None
        self.link_down_reason = ""
        self.link_down_last_try = 0.0
        self.reconnect_tries = 0
        self.reader_error = None
        self.port_miss = 0
        self.port_desc = ""
        self.port_hint_done = False
        self.last_reopen_try = 0.0
        self.health_job = None
        self.status_locked = False
        self.charge_check_on = False
        self.charge_check_job = None
        self.charge_check_start = 0.0
        self.charge_check_hit = None
        self.charge_seen = False
        self.charge_last_percent = None
        self.targets = [0] * len(ITEMS)
        self.row_state = ["未测"] * len(ITEMS)
        self.seen_bigs = set()

        self._build()
        self._load_config()
        self._watch_config()
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(200, self._refresh_ports)
        # 1s 一次的链路体检: 蓝牙虚拟串口会在耳机重启时消失, 这里负责发现并重连
        self.health_job = self.after(1000, self._link_watchdog)

    def _build(self):
        pad = {"padx": 12, "pady": 6}

        link = ttk.LabelFrame(self, text="串口")
        link.pack(fill="x", **pad)
        row0 = ttk.Frame(link)
        row0.pack(fill="x", padx=12, pady=8)
        ttk.Label(row0, text="端口", width=6).pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(row0, textvariable=self.port_var, width=8)
        self.port_box.pack(side="left")
        ttk.Button(row0, text="刷新", command=self._refresh_ports, width=6).pack(side="left", padx=(8, 0))
        ttk.Label(row0, text="波特率", width=6, anchor="e").pack(side="left", padx=(16, 0))
        self.baud_presets = ("115200", "921600", "1152000", "2000000")
        self.baud_choices = self.baud_presets + ("自定义",)
        self.baud_var = tk.StringVar(value="115200")
        self.baud_box = ttk.Combobox(
            row0, textvariable=self.baud_var, width=10, state="readonly",
            values=self.baud_choices,
        )
        self.baud_box.pack(side="left", padx=(6, 0))
        self.baud_box.bind("<<ComboboxSelected>>", self._on_baud_selected)
        self.link_btn = tk.Button(
            row0, text="打开", command=self._on_link_toggle,
            width=8, cursor="hand2", relief="flat", bd=0,
            font=("Microsoft YaHei UI", 9),
        )
        self.link_btn.pack(side="left", padx=(16, 0))
        self.auto_reconnect = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            row0, text="断线自动重连", variable=self.auto_reconnect,
        ).pack(side="left", padx=(10, 0))
        self._set_link_lamp(False)

        cfg = ttk.LabelFrame(self, text="配置信息 (经 0x7D 下发)")
        cfg.pack(fill="x", **pad)
        row2 = ttk.Frame(cfg)
        row2.pack(fill="x", padx=12, pady=(8, 2))
        ttk.Label(row2, text="被测耳", width=8).pack(side="left")
        self.ear_var = tk.StringVar(value="左耳")
        ttk.Combobox(
            row2, textvariable=self.ear_var, width=8, state="readonly",
            values=("左耳", "右耳"),
        ).pack(side="left")
        ttk.Label(row2, text="通道", width=6, anchor="e").pack(side="left", padx=(18, 0))
        self.path_var = tk.StringVar(value=self._path_label(0x02))
        ttk.Combobox(
            row2, textvariable=self.path_var, width=22, state="readonly",
            values=tuple(self._path_label(value) for value, _name in PATHS),
        ).pack(side="left", padx=(6, 0))
        ttk.Label(row2, text="查询间隔(s)", width=12, anchor="e").pack(side="left", padx=(12, 0))
        self.interval_var = tk.StringVar(value="2")
        ttk.Spinbox(row2, from_=1, to=30, textvariable=self.interval_var, width=5).pack(side="left", padx=(6, 0))

        row3 = ttk.Frame(cfg)
        row3.pack(fill="x", padx=12, pady=(2, 8))
        self.preset_trial = ttk.Button(row3, text="试产推荐", width=10, command=lambda: self._apply_preset("trial"))
        self.preset_trial.pack(side="left")
        self.preset_mp = ttk.Button(row3, text="量产推荐", width=10, command=lambda: self._apply_preset("mp"))
        self.preset_mp.pack(side="left", padx=(8, 0))
        self.charge_var = tk.BooleanVar(value=True)
        self.charge_chk = ttk.Checkbutton(row3, text="中/高负载时充电", variable=self.charge_var)
        self.charge_chk.pack(side="left", padx=(16, 0))
        ttk.Label(row3, text="最低电量(%)", width=11, anchor="e").pack(side="left", padx=(16, 0))
        self.min_bat_var = tk.StringVar(value="20")
        self.min_bat_spin = ttk.Spinbox(
            row3, from_=0, to=100, textvariable=self.min_bat_var, width=5)
        self.min_bat_spin.pack(side="left", padx=(6, 0))

        mon = ttk.LabelFrame(self, text="测试过程")
        mon.pack(fill="x", **pad)

        well = tk.Frame(mon, bd=1, relief="sunken", bg="#EEF1F4")
        well.pack(fill="x", padx=12, pady=8)
        table = tk.Frame(well, bg="#EEF1F4")
        table.pack(padx=16, pady=8)
        self._row_font = ("Microsoft YaHei UI", 11)
        self._head_font = ("Microsoft YaHei UI", 11, "bold")
        # 列宽(单位=字符宽, 11pt 下约 9.75px/单位)。按实际最长内容定, 总宽必须
        # 小于窗口宽度: 表格比窗口宽时会被右边裁掉, 最后一列"结果"首当其冲
        # (表现为"测试中"只剩半截)。
        for col, text, width in (
            (0, "是否测试", 9),      # 4 个汉字表头
            (1, "大项ID", 7),
            (2, "大项", 18),         # 最长 "终检(传感器/充电/补电)" ≈ 157px
            (3, "目标次数", 8),
            (4, "已执行次数", 9),    # 5 个汉字表头
            (5, "耗时", 6),
            (6, "结果", 8),          # "测试中/已停止" = 3 个汉字
        ):
            anchor = "w" if col == 2 else "center"
            tk.Label(
                table, text=text, width=width, anchor=anchor,
                font=self._head_font, bg="#EEF1F4", fg="#333333",
            ).grid(row=0, column=col, padx=4, pady=(0, 4))

        self.item_vars = []
        self.cycle_vars = []
        self.done_vars = []
        self.time_vars = []
        self.result_vars = []
        self.check_widgets = []
        self.spin_widgets = []
        self.result_labels = []
        for i, (key, name, default) in enumerate(ITEMS):
            enabled = tk.BooleanVar(value=key != "hard_rst")
            cycle = tk.StringVar(value=str(default))
            done = tk.StringVar(value="-")
            spent = tk.StringVar(value="-")
            result = tk.StringVar(value="未测")
            chk = ttk.Checkbutton(table, variable=enabled)
            chk.grid(row=i + 1, column=0, pady=2)
            tk.Label(
                table, text=str(ITEM_KEY_TO_CFG[key]), width=7, anchor="center",
                font=self._row_font, bg="#EEF1F4",
            ).grid(row=i + 1, column=1)
            tk.Label(
                table, text=name, width=18, anchor="w",
                font=self._row_font, bg="#EEF1F4",
            ).grid(row=i + 1, column=2, sticky="w", padx=4)
            spin = ttk.Spinbox(
                table, from_=0, to=CFG_TARGET_MAX, textvariable=cycle, width=5,
                justify="center", font=self._row_font,
            )
            spin.grid(row=i + 1, column=3, pady=2)
            tk.Label(
                table, textvariable=done, width=9, anchor="center",
                font=self._row_font, bg="#EEF1F4",
            ).grid(row=i + 1, column=4)
            tk.Label(
                table, textvariable=spent, width=6, anchor="center",
                font=self._row_font, bg="#EEF1F4",
            ).grid(row=i + 1, column=5)
            lab = tk.Label(
                table, textvariable=result, width=8, anchor="center",
                font=self._row_font, bg="#EEF1F4",
            )
            # 结果列是最后一列, 右侧留点空, 别贴着边框
            lab.grid(row=i + 1, column=6, padx=(4, 8))
            self.item_vars.append(enabled)
            self.cycle_vars.append(cycle)
            self.done_vars.append(done)
            self.time_vars.append(spent)
            self.result_vars.append(result)
            self.check_widgets.append(chk)
            self.spin_widgets.append(spin)
            self.result_labels.append(lab)
            self._paint_result(i, "未测")

        info = tk.Frame(well, bg="#EEF1F4")
        info.pack(fill="x", padx=16, pady=(0, 8))
        self.status_var = tk.StringVar(value="未开始")
        self.total_var = tk.StringVar(value="累计老化测试 -")
        self.fw_var = tk.StringVar(value="固件版本 -")
        self.detail_var = tk.StringVar(value="温度 -     电压 -     电量 -     充电 -")
        self.elapsed_var = tk.StringVar(value="测试耗时 00:00:00")
        self.fail_var = tk.StringVar(value="")
        tk.Label(
            info, textvariable=self.status_var, anchor="w", bg="#EEF1F4",
            font=("Microsoft YaHei UI", 12, "bold"),
        ).pack(anchor="w")
        # 累计老化次数单独一行: 它是整机累计值, 与当前大项/小项无关
        tk.Label(
            info, textvariable=self.total_var, anchor="w", bg="#EEF1F4",
            font=("Microsoft YaHei UI", 10), fg="#455A64",
        ).pack(anchor="w", pady=(2, 0))
        tk.Label(
            info, textvariable=self.fw_var, anchor="w", bg="#EEF1F4",
            font=self._row_font, fg="#1565C0",
        ).pack(anchor="w", pady=(2, 0))
        tk.Label(
            info, textvariable=self.detail_var, anchor="w", bg="#EEF1F4",
            font=self._row_font,
        ).pack(anchor="w", pady=(2, 0))
        tk.Label(
            info, textvariable=self.elapsed_var, anchor="w", bg="#EEF1F4",
            font=self._row_font,
        ).pack(anchor="w", pady=(2, 0))
        self.fail_label = tk.Label(
            info, textvariable=self.fail_var, anchor="w", bg="#EEF1F4",
            font=self._row_font, fg="#b00020",
        )
        self.fail_label.pack(anchor="w")

        act = ttk.Frame(self)
        act.pack(fill="x", **pad)
        act.columnconfigure(0, weight=1)
        act.columnconfigure(7, weight=1)
        self.start_btn = tk.Button(
            act, text="开始测试", command=self._on_start_stop,
            font=("Microsoft YaHei UI", 12, "bold"),
            width=11, height=1, cursor="hand2", relief="flat",
            bg="#1565C0", fg="white",
            activebackground="#0D47A1", activeforeground="white",
        )
        self.start_btn.grid(row=0, column=1, padx=6, pady=6)
        self.exit_btn = tk.Button(
            act, text="退出老化", command=self._on_exit,
            font=("Microsoft YaHei UI", 12, "bold"),
            width=11, height=1, cursor="hand2", relief="flat",
            bg="#546E7A", fg="white",
            activebackground="#37474F", activeforeground="white",
        )
        self.exit_btn.grid(row=0, column=2, padx=6, pady=6)
        self.query_btn = tk.Button(
            act, text="查询结果", command=self._on_query,
            font=("Microsoft YaHei UI", 12, "bold"),
            width=11, height=1, cursor="hand2", relief="flat",
            bg="#2E7D32", fg="white",
            activebackground="#1B5E20", activeforeground="white",
        )
        self.query_btn.grid(row=0, column=3, padx=6, pady=6)
        self.cfg_btn = tk.Button(
            act, text="查询配置", command=self._on_query_cfg,
            font=("Microsoft YaHei UI", 12, "bold"),
            width=11, height=1, cursor="hand2", relief="flat",
            bg="#6A1B9A", fg="white",
            activebackground="#4A148C", activeforeground="white",
        )
        self.cfg_btn.grid(row=0, column=4, padx=6, pady=6)
        self.charge_test_btn = tk.Button(
            act, text="充电检测", command=self._on_charge_check,
            font=("Microsoft YaHei UI", 12, "bold"),
            width=11, height=1, cursor="hand2", relief="flat",
            bg="#EF6C00", fg="white",
            activebackground="#E65100", activeforeground="white",
        )
        self.charge_test_btn.grid(row=0, column=5, padx=6, pady=6)

        logf = ttk.LabelFrame(self, text="通信日志")
        logf.pack(fill="both", expand=True, **pad)
        self.log = tk.Text(logf, height=8, wrap="none")
        self.log.pack(fill="both", expand=True, padx=6, pady=4)
        self.log.configure(state="disabled")

    def _log(self, text):
        stamp = time.strftime("%H:%M:%S")
        self.log.configure(state="normal")
        self.log.insert("end", "[%s] %s\n" % (stamp, text))
        self.log.see("end")
        self.log.configure(state="disabled")

    def _list_ports(self):
        found = []
        if serial is not None:
            try:
                found = list(serial.tools.list_ports.comports())
            except Exception as exc:
                self._log("pyserial 枚举失败: %s" % exc)
        if not found:
            found = self._list_ports_registry()
        return found

    @staticmethod
    def _list_ports_registry():
        """pyserial 扫不到时，从 Windows 串口注册表补一份。"""
        import winreg

        ports = []
        try:
            key = winreg.OpenKey(
                winreg.HKEY_LOCAL_MACHINE,
                r"HARDWARE\DEVICEMAP\SERIALCOMM",
            )
        except OSError:
            return ports
        try:
            index = 0
            while True:
                try:
                    _name, value, _typ = winreg.EnumValue(key, index)
                except OSError:
                    break
                index += 1
                if isinstance(value, str) and value.upper().startswith("COM"):
                    ports.append(type("Port", (), {"device": value, "description": "注册表"})())
        finally:
            winreg.CloseKey(key)
        ports.sort(key=lambda item: item.device)
        return ports

    def _config_snapshot(self):
        return {
            "port": self.port_var.get().strip(),
            "baud": self.baud_var.get().strip(),
            "auto_reconnect": bool(self.auto_reconnect.get()),
            "ear": self.ear_var.get(),
            "path": self.path_var.get(),
            "interval": self.interval_var.get().strip(),
            "charge": bool(self.charge_var.get()),
            "min_bat": self.min_bat_var.get().strip(),
            "items": [
                {"enabled": bool(self.item_vars[i].get()), "cycles": self.cycle_vars[i].get().strip()}
                for i in range(len(ITEMS))
            ],
        }

    def _load_config(self):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (OSError, ValueError):
            return
        if not isinstance(data, dict):
            return
        port = data.get("port")
        if isinstance(port, str) and port.strip():
            self.port_var.set(port.strip())
        baud = str(data.get("baud", "")).strip()
        if baud.isdigit() and int(baud) > 0:
            self._show_baud(baud)
        ear = data.get("ear")
        if ear in ("左耳", "右耳"):
            self.ear_var.set(ear)
        path = data.get("path")
        labels = tuple(self._path_label(value) for value, _name in PATHS)
        if isinstance(path, str) and path in labels:
            self.path_var.set(path)
        interval = str(data.get("interval", "")).strip()
        if interval.isdigit() and 1 <= int(interval) <= 30:
            self.interval_var.set(interval)
        if isinstance(data.get("charge"), bool):
            self.charge_var.set(data["charge"])
        if isinstance(data.get("auto_reconnect"), bool):
            self.auto_reconnect.set(data["auto_reconnect"])
        min_bat = str(data.get("min_bat", "")).strip()
        if min_bat.isdigit() and 0 <= int(min_bat) <= 100:
            self.min_bat_var.set(min_bat)
        items = data.get("items")
        if isinstance(items, list):
            for i, item in enumerate(items[:len(ITEMS)]):
                if not isinstance(item, dict):
                    continue
                if isinstance(item.get("enabled"), bool):
                    self.item_vars[i].set(item["enabled"])
                cycles = str(item.get("cycles", "")).strip()
                if cycles.isdigit() and 0 <= int(cycles) <= CFG_TARGET_MAX:
                    self.cycle_vars[i].set(cycles)
        self._saved_snapshot = self._config_snapshot()

    def _show_baud(self, baud):
        if baud not in self.baud_presets:
            choices = self.baud_presets + (baud, "自定义")
            self.baud_box["values"] = choices
        self.baud_var.set(baud)

    def _save_config(self):
        snapshot = self._config_snapshot()
        if snapshot == getattr(self, "_saved_snapshot", None):
            return
        try:
            with open(CONFIG_PATH, "w", encoding="utf-8") as handle:
                json.dump(snapshot, handle, ensure_ascii=False, indent=2)
        except OSError as exc:
            self._log("保存配置失败: %s" % exc)
            return
        self._saved_snapshot = snapshot

    def _watch_config(self):
        if not self.testing:
            self._save_config()
        self.after(500, self._watch_config)

    def _refresh_ports(self):
        if serial is None:
            self.port_box["values"] = ()
            self._log("未安装 pyserial，请先执行: pip install pyserial")
            return
        ports = self._list_ports()
        values = [p.device for p in ports]
        self.port_box["values"] = values
        current = self._selected_port()
        # 配置里记下的端口即使这次没枚举到也保留，方便手输的 COMx 下次还能用。
        if values and not current:
            self.port_var.set(values[0])
        if values:
            self._log("发现串口: %s" % ", ".join(p.device for p in ports))
        else:
            self._log("未发现串口，可直接输入 COMx 后开始测试")

    def _on_baud_selected(self, _event=None):
        if self.baud_var.get() != "自定义":
            return
        value = simpledialog.askstring("自定义波特率", "请输入波特率", parent=self)
        if value is None:
            self.baud_var.set("115200")
            return
        text = value.strip()
        try:
            baud = int(text)
        except ValueError:
            baud = 0
        if baud <= 0:
            messagebox.showerror("波特率", "请输入大于 0 的整数")
            self.baud_var.set("115200")
            return
        self._show_baud(str(baud))

    def _selected_port(self):
        text = self.port_var.get().strip()
        if not text:
            return ""
        token = text.split()[0].upper()
        if token.startswith("COM"):
            return token
        return text.split()[0]

    @staticmethod
    def _path_label(value):
        for item, name in PATHS:
            if item == value:
                return "0x%02X  %s" % (item, name)
        return "0x%02X" % value

    def _ear(self):
        return DEV_RIGHT if self.ear_var.get() == "右耳" else DEV_LEFT

    def _path(self):
        text = self.path_var.get().strip()
        token = text.split()[0] if text else ""
        try:
            value = int(token, 16)
        except ValueError:
            raise ProtocolError("请选择指令通道")
        if value not in [item for item, _name in PATHS]:
            raise ProtocolError("通道不在 sndp_comm_path_id_e 内")
        return value

    def _apply_preset(self, kind):
        cycles = {
            "trial": {"sram": 1, "flash": 1, "soft_rst": 10, "hard_rst": 10,
                      "mid": 18, "high": 18, "bus": 10, "final": 1},
            "mp": {"sram": 1, "flash": 1, "soft_rst": 10, "hard_rst": 10,
                   "mid": 9, "high": 9, "bus": 10, "final": 1},
        }[kind]
        self.charge_var.set(True)
        for i, (key, _, _) in enumerate(ITEMS):
            self.item_vars[i].set(key != "hard_rst")
            self.cycle_vars[i].set(str(cycles[key]))

    def _build_cfg_payload(self):
        """生成 0x7D 的配置数据: 固定 9 × [大项ID(1) + 是否测试(1) + 目标次数(1)]。

        按大项 ID 0..8 顺序把 9 项全部带上(不管有没有勾选), 用数值不用名称:
        耳机侧照单全收, 不会再出现"没下发的项保留耳机上旧值"的问题。
        """
        payload = bytearray()
        self.targets = []
        for big_id, key in enumerate(CFG_ORDER):
            row = ITEM_KEY_TO_ROW.get(key)
            if row is None:
                # charge 只有"中/高负载时充电"这一个开关, 没有目标次数
                enable = 1 if self.charge_var.get() else 0
                target = 0
            else:
                try:
                    target = int(self.cycle_vars[row].get())
                except ValueError:
                    raise ProtocolError("%s 目标次数不是数字" % key)
                if target < 0 or target > CFG_TARGET_MAX:
                    raise ProtocolError("%s 目标次数应在 0~%d" % (key, CFG_TARGET_MAX))
                enable = 1 if self.item_vars[row].get() else 0
                self.targets.append(target)
                if enable and target > 0 and big_id in FIRMWARE_SKIP_BIGS:
                    self._log("提示: %s 当前固件强制跳过(AGING_SKIP_HARD_RST=1)，"
                              "勾了也不会执行" % key)
            payload += bytes((big_id, enable, target))
        # 一个大项都没勾(或目标都是 0)时固件判为"无有效大项" -> 清除配置并退出老化。
        # 这里先拦下来, 避免误把耳机上的配置和断点清掉(要清除请用"退出老化")。
        if not any(self.item_vars[i].get() and self.targets[i] > 0
                   for i in range(len(ITEMS))):
            raise ProtocolError("没有勾选任何大项（或目标次数为 0），请至少勾选一项")
        return bytes(payload)

    def _open_serial(self):
        if serial is None:
            raise ProtocolError("未安装 pyserial")
        port = self._selected_port()
        if not port:
            raise ProtocolError("请先选择串口")
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            raise ProtocolError("波特率无效")
        if self.ser and self.ser.is_open:
            return
        self._open_port(port, baud)
        self._log("打开串口 %s, %d" % (port, baud))

    def _open_port(self, port, baud, tries=3, gap=0.25):
        """打开端口。蓝牙 SPP 刚恢复时第一次 open 常失败, 所以重试几次。

        注意 write_timeout: 蓝牙链路假死时, 没有写超时的 write 会一直阻塞,
        把界面线程卡死。这里统一给 1s 写超时, 超时按链路异常处理。
        """
        last = None
        for _ in range(max(1, tries)):
            try:
                self.ser = serial.Serial(
                    port, baud, bytesize=8, parity="N", stopbits=1,
                    timeout=0.05, write_timeout=1.0,
                )
                break
            except Exception as exc:
                last = exc
                time.sleep(gap)
        else:
            self.ser = None
            raise LinkError("打开 %s 失败: %s" % (port, last))
        self.port_desc = self._port_desc(port)
        self.reader_error = None
        self.port_miss = 0
        # 打开成功即认为链路恢复
        self.link_down_since = None
        self.link_down_reason = ""
        self.reconnect_tries = 0
        self.port_hint_done = False
        self.rx_buf.clear()
        self.stop_reader.clear()
        self.reader = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader.start()
        self._set_link_lamp(True)
        return True

    def _close_serial(self):
        self.stop_reader.set()
        if self.reader and self.reader.is_alive():
            self.reader.join(timeout=0.5)
        self.reader = None
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self._set_link_lamp(False)

    def _serial_open(self):
        return self.ser is not None and self.ser.is_open

    def _port_present(self, port):
        if not port:
            return False
        return any(p.device.upper() == port.upper() for p in self._list_ports())

    def _port_desc(self, port):
        for item in self._list_ports():
            if item.device.upper() == port.upper():
                return getattr(item, "description", "") or ""
        return ""

    def _reopen_serial(self):
        """耳机重启/蓝牙断链后虚拟串口会消失再出现, 关闭旧句柄后重新打开。"""
        port = self._selected_port()
        if not port or not self._port_present(port):
            return False
        self._close_serial()
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            baud = 115200
        self._open_port(port, baud, tries=1, gap=0)
        self._log("串口已重连 %s" % port)
        return True

    # ---- 链路监测 ----

    def _mark_link_down(self, reason):
        """标记链路断开: 关句柄、灭灯、记下起始时间与原因(只记第一次)。"""
        if self.link_down_since is None:
            self.link_down_since = time.time()
            self.link_down_reason = reason
            self.link_down_last_try = 0.0
            self.reconnect_tries = 0
            self.port_hint_done = False
            self._log("链路断开: %s" % reason)
        self.reader_error = None
        self._close_serial()

    def _link_down_seconds(self):
        if self.link_down_since is None:
            return 0
        return int(time.time() - self.link_down_since)

    def _try_reconnect(self, force=False):
        """尝试重连。间隔按 0.5/1/2s 退避, 失败日志抽稀, 避免刷屏。"""
        if self.link_down_since is None:
            return False
        now = time.time()
        if not force:
            gap = (0.5, 1.0, 2.0)[min(self.reconnect_tries, 2)]
            if now - self.link_down_last_try < gap:
                return False
        self.link_down_last_try = now
        self.reconnect_tries += 1
        try:
            if self._reopen_serial():
                self.link_down_since = None
                self.link_down_reason = ""
                self.reconnect_tries = 0
                self.query_miss = 0
                return True
        except Exception as exc:
            if self.reconnect_tries in (1, 5, 20):
                self._log("重连失败(%d 次): %s" % (self.reconnect_tries, exc))
        return False

    def _check_link(self):
        """链路体检一次: 判定是否断开, 并在空闲时把串口拉回来。"""
        port = self._selected_port()
        if self.ser is not None:
            if self.reader_error:
                # 接收线程报 IO 异常, 句柄已失效(蓝牙断链最常见)
                self._mark_link_down(self.reader_error)
            elif port and not self._port_present(port):
                # 枚举偶尔会漏一拍, 连续 3 次才算端口真的没了
                self.port_miss += 1
                if self.port_miss >= 3:
                    self._mark_link_down("端口 %s 已不在系统中" % port)
            else:
                self.port_miss = 0
        if self.link_down_since is None:
            return
        if not self.port_hint_done and self._link_down_seconds() >= 20:
            self.port_hint_done = True
            names = ", ".join(p.device for p in self._list_ports()) or "无"
            self._log("端口 %s 仍未出现; 当前系统串口: %s" % (port or "-", names))
            self._log("若耳机换了 COM 号, 请在上方下拉里重新选择端口。")
        # 测试进行中的重连交给 _poll(它本来就每 2s 跑一次), 这里只管空闲时
        if self.testing:
            return
        # 结论已出时不再覆盖状态文字(status_locked), 但重连一直继续
        if self.auto_reconnect.get() and self._try_reconnect():
            if not self.status_locked:
                self.status_var.set("串口已重连")
            return
        if self.ser is None and not self.status_locked:
            self.status_var.set("串口断开(%s)，等待重连 %ds" % (
                self.link_down_reason or "未知原因", self._link_down_seconds()))

    def _link_watchdog(self):
        self.health_job = None
        try:
            self._check_link()
        finally:
            self.health_job = self.after(1000, self._link_watchdog)

    def _on_link_toggle(self):
        if self._serial_open():
            if self.testing:
                messagebox.showinfo("串口", "测试进行中，请先停止测试再关闭串口。")
                return
            self._close_serial()
            self._log("串口已关闭")
            return
        try:
            self._open_serial()
        except Exception as exc:
            self._log("打开失败: %s" % exc)
            messagebox.showerror("打开失败", str(exc))
            self._close_serial()

    def _set_link_lamp(self, connected):
        # 与原先连接色块同色：未打开淡红「打开」，已打开淡绿「关闭」。
        if connected:
            fill, ink, text = "#7DCEA0", "#145A32", "关闭"
        else:
            fill, ink, text = "#E6B0AA", "#7B241C", "打开"
        self.link_btn.configure(
            text=text, bg=fill, fg=ink,
            activebackground=fill, activeforeground=ink,
        )

    def _reader_loop(self):
        while not self.stop_reader.is_set():
            try:
                data = self.ser.read(256) if self.ser else b""
            except Exception as exc:
                # 蓝牙虚拟串口在耳机重启时会失效, 记下原因交给链路监测去重连
                self.reader_error = str(exc) or exc.__class__.__name__
                self.rx_queue.put(("io", self.reader_error))
                return
            if data:
                self.rx_buf.extend(data)
                self._extract_frames()
            else:
                time.sleep(0.01)

    def _extract_frames(self):
        while True:
            if not self.rx_buf:
                return
            try:
                start = self.rx_buf.index(FRAME_FLAG)
            except ValueError:
                self.rx_buf.clear()
                return
            if start:
                del self.rx_buf[:start]
            if len(self.rx_buf) < 8:
                return
            dlen = (self.rx_buf[4] << 8) | self.rx_buf[5]
            total = 6 + dlen + 2
            if dlen > 246:
                del self.rx_buf[0]
                continue
            if len(self.rx_buf) < total:
                return
            frame = bytes(self.rx_buf[:total])
            del self.rx_buf[:total]
            body, recv_crc = frame[:-2], (frame[-2] << 8) | frame[-1]
            if crc16(body) != recv_crc:
                self.rx_queue.put(("bad_crc", frame))
                continue
            self.rx_queue.put(("frame", frame[3], frame[6:-2]))

    def _transact(self, cmd, payload, timeout=2.0):
        """单次收发。timeout 默认 2s:
        耳机在 ANC 打开等场景下音频子系统会短时卡顿, 应答可能晚到,
        1.2s 太紧会误判成超时。用户主动操作(启动/停止/退出)用 3s。

        链路已断开时直接抛 LinkError, 不去碰失效的句柄, 免得弹一堆底层异常。
        """
        if not self._serial_open():
            raise LinkError("串口未打开或已断开")
        if self.reader_error:
            reason = self.reader_error
            self._mark_link_down(reason)
            raise LinkError("串口已断开: %s" % reason)
        frame = pack_frame(cmd, payload, self._ear(), self._path())
        self._log("TX: %s" % hex_of(frame))
        while not self.rx_queue.empty():
            try:
                self.rx_queue.get_nowait()
            except queue.Empty:
                break
        try:
            self.ser.reset_input_buffer()
            self.rx_buf.clear()
            # 单线半双工: 发完再收，避免本端回环被当成应答。
            self.ser.write(frame)
            self.ser.flush()
        except Exception as exc:
            # 写失败(含 write_timeout): 链路已断, 交给链路监测去重连
            self._mark_link_down(str(exc) or exc.__class__.__name__)
            raise LinkError("发送失败: %s" % exc)
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                kind, *rest = self.rx_queue.get(timeout=0.05)
            except queue.Empty:
                continue
            if kind == "io":
                self._mark_link_down(rest[0])
                raise LinkError("串口已断开: %s" % rest[0])
            if kind == "bad_crc":
                self._log("RX: CRC 不符 %s" % hex_of(rest[0]))
                continue
            rsp_cmd, data = rest
            self._log("RX: cmd=%02X %s" % (rsp_cmd, hex_of(data)))
            if rsp_cmd != cmd:
                continue
            return data
        raise ProtocolError("等待应答超时")

    def _min_battery(self):
        try:
            value = int(self.min_bat_var.get())
        except ValueError:
            raise ProtocolError("最低电量不是数字")
        if value < 0 or value > 100:
            raise ProtocolError("最低电量应在 0~100")
        return value

    def _rsp_ok(self, data, need):
        if not data or len(data) < need or data[0] != 0:
            code = data[0] if data else 0xFF
            raise ProtocolError(COMM_ERRORS.get(code, "错误码 0x%02X" % code))
        return data

    def _precheck(self, min_bat):
        """固件版本与电量预检。

        两者都是提示性信息: 耳机忙(如 ANC 打开时音频子系统短时卡顿)读不到时
        只告警并继续, 不能因为一次超时就把整个启动流程掐掉——那样耳机还在跑,
        工具却停在未启动状态, 反而没法控制它。
        """
        try:
            fw = self._rsp_ok(self._transact(CMD_FW_VER, b"", timeout=3.0), 5)
            version = "%d.%d.%d.%d" % (fw[1], fw[2], fw[3], fw[4])
            self._log("固件版本 %s" % version)
            self.fw_var.set("固件版本 %s" % version)
        except Exception as exc:
            self._log("读固件版本失败(忽略): %s" % exc)

        try:
            bat = self._rsp_ok(self._transact(CMD_BAT, b"", timeout=3.0), 4)
            voltage = (bat[1] << 8) | bat[2]
            percent = bat[3]
            self._log("当前电量 %d%%  %d mV" % (percent, voltage))
            self.detail_var.set("温度 -     电压 %d mV     电量 %d%%     充电 -" % (voltage, percent))
        except Exception as exc:
            self._log("读电量失败(跳过电量门槛): %s" % exc)
            self.status_var.set("读电量失败，已跳过电量检查")
            return True

        if percent < min_bat:
            messagebox.showwarning(
                "电量太低",
                "当前电量 %d%%，低于设定的 %d%%。\n请充电后再开始测试。" % (percent, min_bat),
            )
            self.status_var.set("电量太低 %d%%" % percent)
            return False
        return True

    def _check_device(self):
        """启动前查询一次。返回 True 表示耳机正在老化。

        顺带校验 0x7C 应答长度: 只有含 0x7D/0x7E 的新固件才返回 89 字节。
        旧固件返回 34 字节(旧布局), 此时直接报错, 不必等 0x7D 被拒才发现。
        """
        try:
            data = self._transact(CMD_QUERY, b"", timeout=3.0)
        except Exception as exc:
            self._log("启动前查询失败(忽略): %s" % exc)
            return False
        if data and data[0] != 0:
            raise ProtocolError(COMM_ERRORS.get(data[0], "查询错误 0x%02X" % data[0]))
        if len(data) < QUERY_DATA_LEN:
            raise ProtocolError(
                "耳机固件过旧: 0x7C 应答 %d 字节, 期望 %d 字节。\n"
                "当前上位机需配合含 0x7D(配置)/0x7E(查配置) 的新固件，请重新编译烧录。"
                % (len(data), QUERY_DATA_LEN))
        return data[1] == 1

    def _ensure_link(self, action):
        """用户主动点按钮前确认链路: 已断开就先重连一次, 不行再提示。

        蓝牙虚拟串口会被耳机重启拆掉, 用户此时点按钮不应该直接抛底层异常。
        """
        if self._serial_open() and self.link_down_since is None:
            return True
        if self.link_down_since is None:
            self._mark_link_down("串口未打开")
        self.status_var.set("串口断开(%s)，正在重连…" % (self.link_down_reason or "未知原因"))
        self.update_idletasks()
        if self.auto_reconnect.get() and self._try_reconnect(force=True):
            self.status_var.set("串口已重连")
            return True
        messagebox.showinfo(
            action,
            "串口断开(%s)，自动重连 %d 秒未成功。\n"
            "请确认耳机蓝牙已连上、端口已出现在下拉框里，再重试。"
            % (self.link_down_reason or "未知原因", self._link_down_seconds()),
        )
        return False

    def _on_start_stop(self):
        if self.testing:
            if not messagebox.askokcancel(
                "停止测试",
                "确认停止老化？\n0x7B 只停止并清掉“老化中”标志，不会清除已完成轮次与配置，\n"
                "但下次开机不会再续测。",
            ):
                return
            self._stop_test()
            return
        if not self._ensure_link("开始测试"):
            return
        try:
            payload = self._build_cfg_payload()
            # 先校验固件是否支持 0x7D/0x7E, 避免旧固件在最后一步才报 0x22
            running = self._check_device()
            min_bat = self._min_battery()
            if not self._precheck(min_bat):
                return
            if running:
                if not messagebox.askokcancel(
                    "耳机正在老化",
                    "耳机当前正在老化中。\n继续会重新下发配置并重新启动，"
                    "上次进度将被丢弃。\n是否继续？",
                ):
                    return
            # 启动序列固定为: 先配置(0x7D, 不启动) -> 再启动(0x7A, 无载荷)。
            # 顺序不能颠倒: 固件端 0x7A 只按已下发配置启动, 未配置时直接返回执行失败。
            self._log("启动序列 1/2: 下发配置 (0x7D)")
            self._rsp_ok(self._transact(CMD_SET_CFG, payload, timeout=3.0), 1)
            self._log("配置已下发(%d 字节): %s" % (len(payload), hex_of(payload)))
            # 与"查询配置"同格式打印, 便于逐项核对工具与耳机是否一致
            self._log("下发: %s" % " ".join(
                "%d:%s=%d" % (i, key, payload[i * 3 + 2] if payload[i * 3 + 1] else 0)
                for i, key in enumerate(CFG_ORDER)))
            self._log("启动序列 2/2: 启动 (0x7A)")
            self._rsp_ok(self._transact(CMD_START, b"", timeout=3.0), 1)
        except Exception as exc:
            self._log("启动失败: %s" % exc)
            messagebox.showerror("启动失败", str(exc))
            return
        self.testing = True
        self.status_locked = False
        self.query_miss = 0
        self.link_down_since = None
        self.seen_bigs = set()
        self._set_start_button(False)
        self._set_config_locked(True)
        self._reset_monitor()
        self._start_clock()
        self.status_var.set("已启动，等待查询")
        self.fail_var.set("")
        self._log("已发送 0x7A 启动")
        self._schedule_poll(300)

    def _stop_test(self):
        self._cancel_poll()
        try:
            if self._ensure_link("停止测试"):
                self._rsp_ok(self._transact(CMD_STOP, b"", timeout=3.0), 1)
                self._log("已发送停止")
        except Exception as exc:
            self._log("停止失败: %s" % exc)
            messagebox.showerror("停止失败", str(exc))
        self.testing = False
        self._stop_clock()
        self._set_start_button(True)
        self._set_config_locked(False)
        self._save_config()
        if not self.fail_var.get():
            self.status_var.set("已停止")

    def _on_exit(self):
        """退出老化 = 0x7D 且 DATA 为 1 字节 0x00，清除配置与过程数据。"""
        if self.testing:
            messagebox.showinfo("退出老化", "请先停止测试，再退出。")
            return
        if not messagebox.askokcancel(
            "退出老化",
            "将清除耳机上的老化配置与过程数据（0x7D + 0x00）。\n"
            "清除后下次开机不会再续测，且需要重新下发配置才能启动。\n是否继续？",
        ):
            return
        if not self._ensure_link("退出老化"):
            return
        try:
            # 1 字节 0x00 = 清除配置并退出老化
            self._rsp_ok(self._transact(CMD_SET_CFG, b"\x00", timeout=3.0), 1)
            self._log("已退出老化，配置与过程数据已清除")
            self.status_var.set("已退出，记录已清除")
            self.fail_var.set("")
            self.seen_bigs = set()
            self.targets = [0] * len(ITEMS)
            self._reset_monitor(blank=True)
        except Exception as exc:
            self._log("退出失败: %s" % exc)
            messagebox.showerror("退出失败", str(exc))

    def _on_query(self):
        if self.testing:
            messagebox.showinfo("查询结果", "测试进行中，结果会自动刷新。")
            return
        if not self._ensure_link("查询结果"):
            return
        try:
            data = self._transact(CMD_QUERY, b"")
            self._apply_status(data, notify=False)
            self._log("已查询老化结果")
        except Exception as exc:
            self._log("查询失败: %s" % exc)
            messagebox.showerror("查询失败", str(exc))

    def _on_query_cfg(self):
        if not self._ensure_link("查询配置"):
            return
        try:
            data = self._transact(CMD_QUERY_CFG, b"")
            self._apply_cfg(data)
            self._log("已查询老化配置")
        except Exception as exc:
            self._log("查询配置失败: %s" % exc)
            messagebox.showerror("查询配置失败", str(exc))

    def _apply_cfg(self, data):
        """0x7E: error(1) + 9 × [大项ID(1) + 是否测试(1) + 目标次数(1)]"""
        if len(data) < QUERY_CFG_DATA_LEN:
            raise ProtocolError(
                "查询配置应答 %d 字节, 期望 %d 字节" % (len(data), QUERY_CFG_DATA_LEN))
        if data[0] != 0:
            raise ProtocolError(COMM_ERRORS.get(data[0], "查询错误 0x%02X" % data[0]))
        summary = []
        for i, key in enumerate(CFG_ORDER):
            big_id = data[1 + i * 3]
            enable = data[2 + i * 3]
            target = data[3 + i * 3]
            # 必须严格按大项 ID 0..8 顺序返回; 对不上说明双方定义不一致, 直接报错而不是猜
            if big_id != i or enable > 1:
                raise ProtocolError(
                    "查询配置第 %d 项异常: 大项ID=%d, 是否测试=%d" % (i, big_id, enable))
            summary.append("%d:%s=%d" % (big_id, key, target if enable else 0))
            row = ITEM_KEY_TO_ROW.get(key)
            if row is None:
                self.charge_var.set(enable != 0)
            else:
                self.item_vars[row].set(enable != 0)
                self.cycle_vars[row].set(str(target))
        self._log("当前配置: %s" % ", ".join(summary))

    # ---- 补电复核(充电检测) ----

    def _on_charge_check(self):
        """30 秒内轮询 0x7C, 检测到"充电中/已充满"即通过。

        耳机侧的"终检+补电"大项只判"插没插充电器", 不等充到目标电量,
        充上没充上真正靠这里判。检测期间需要把耳机放进充电仓。
        """
        if self.charge_check_on:
            return
        if self.testing:
            messagebox.showinfo("充电检测", "测试进行中，请先停止测试。")
            return
        if not self._ensure_link("充电检测"):
            return
        self.charge_check_on = True
        self.charge_check_start = time.time()
        self.charge_check_hit = None
        self.charge_seen = False
        self.charge_last_percent = None
        self.charge_test_btn.configure(state="disabled")
        self._log("充电检测开始: 把耳机放入充电仓, %d 秒内要求「在充电 且 电量 ≥%d%%」"
                  % (CHARGE_CHECK_SECONDS, CHARGE_TARGET_PCT))
        self.status_var.set("充电检测中 0/%d 秒…" % CHARGE_CHECK_SECONDS)
        self._charge_check_tick()

    def _charge_check_tick(self):
        if not self.charge_check_on:
            return
        elapsed = int(time.time() - self.charge_check_start)
        percent = None
        charger = None
        try:
            data = self._transact(CMD_QUERY, b"")
            if len(data) >= QUERY_DATA_LEN:
                percent = data[8]
                charger = data[9]
                self._apply_status(data, notify=False)
        except Exception as exc:
            self._log("充电检测查询失败(%ds): %s" % (elapsed, exc))
        if charger is not None:
            self.charge_last_percent = percent
            self._log("充电检测 %2ds/%ds: 电量 %d%%, 充电状态 %d(%s)" % (
                elapsed, CHARGE_CHECK_SECONDS, percent, charger,
                CHARGER_NAMES.get(charger, "未知")))
            if charger in CHARGER_OK_STATUS:
                self.charge_seen = True
                # 两个条件都满足才算过: 在充电 + 电量达标
                if percent >= CHARGE_TARGET_PCT:
                    self.charge_check_hit = charger
                    self._finish_charge_check(
                        True, "充电检测通过: 第 %d 秒 充电状态「%s」, 电量 %d%% ≥ %d%%。" % (
                            elapsed, CHARGER_NAMES.get(charger, "充电"), percent,
                            CHARGE_TARGET_PCT))
                    return
        if elapsed >= CHARGE_CHECK_SECONDS:
            last = self.charge_last_percent
            if self.charge_seen:
                why = "检测到在充电, 但电量 %s, 未达到 %d%%" % (
                    ("只有 %d%%" % last) if last is not None else "读不到", CHARGE_TARGET_PCT)
            else:
                why = "%d 秒内一次都没检测到在充电" % CHARGE_CHECK_SECONDS
            self._finish_charge_check(
                False,
                "充电检测失败: %s。\n"
                "请确认耳机已放入充电仓、仓内有电、充电触点接触良好。" % why)
            return
        self.status_var.set("充电检测中 %d/%d 秒…  电量 %s  充电 %s  目标 ≥%d%%" % (
            elapsed, CHARGE_CHECK_SECONDS,
            ("%d%%" % percent) if percent is not None else "-",
            CHARGER_NAMES.get(charger, "-") if charger is not None else "-",
            CHARGE_TARGET_PCT))
        self.charge_check_job = self.after(CHARGE_CHECK_INTERVAL_MS, self._charge_check_tick)

    def _finish_charge_check(self, ok, text):
        self.charge_check_on = False
        if self.charge_check_job is not None:
            self.after_cancel(self.charge_check_job)
            self.charge_check_job = None
        self.charge_test_btn.configure(state="normal")
        self.status_locked = True
        self._log(text.replace("\n", " "))
        if ok:
            self.status_var.set("充电检测通过")
            self.fail_var.set("充电检测通过")
            self.fail_label.configure(fg="#1B5E20")
            messagebox.showinfo("充电检测", text)
        else:
            self.status_var.set("充电检测失败")
            self.fail_var.set("充电检测失败：%s" % text.split("\n")[0])
            self.fail_label.configure(fg="#b00020")
            messagebox.showerror("充电检测", text)

    def _set_start_button(self, idle):
        if idle:
            self.start_btn.configure(
                text="开始测试", bg="#1565C0", activebackground="#0D47A1")
        else:
            self.start_btn.configure(
                text="停止测试", bg="#C62828", activebackground="#8E0000")

    def _set_config_locked(self, locked):
        state = "disabled" if locked else "normal"
        for widget in self.check_widgets + self.spin_widgets:
            widget.configure(state=state)
        self.charge_chk.configure(state=state)
        self.min_bat_spin.configure(state=state)
        self.preset_trial.configure(state=state)
        self.preset_mp.configure(state=state)

    def _paint_result(self, index, result):
        colors = {
            "通过": "#1B7A3A",
            "测试中": "#1565C0",
            "已停止": "#1565C0",
            "失败": "#B00020",
            "跳过": "#757575",
            "等待": "#8A6D00",
            "未测": "#333333",
        }
        self.result_labels[index].configure(fg=colors.get(result, "#333333"))

    def _reset_monitor(self, blank=False):
        """开测/退出时把表格复位。

        判"这项测不测"用勾选框而不是目标次数: 没勾的大项即使次数栏里还留着上次填的
        数值也不会跑, 显示"跳过"; 否则第一帧会先显示"等待", 看着像排队等它跑。
        """
        self.total_var.set("累计老化测试 -")
        self.row_state = []
        for i, (key, _name, _default) in enumerate(ITEMS):
            active = (not blank
                      and bool(self.item_vars[i].get())
                      and self.targets[i] > 0
                      and ITEM_KEY_TO_CFG[key] not in FIRMWARE_SKIP_BIGS)
            state = "未测" if blank else ("等待" if active else "跳过")
            self.row_state.append(state)
            self.done_vars[i].set("-" if blank else "0")
            self.time_vars[i].set("-")
            self.result_vars[i].set(state)
            self._paint_result(i, state)

    @staticmethod
    def _format_elapsed(seconds):
        seconds = max(0, int(seconds))
        return "%02d:%02d:%02d" % (seconds // 3600, (seconds % 3600) // 60, seconds % 60)

    @staticmethod
    def _format_duration_s(seconds):
        seconds = max(0, int(seconds))
        if seconds < 60:
            return "%ds" % seconds
        if seconds < 3600:
            return "%dm%02ds" % (seconds // 60, seconds % 60)
        return "%dh%02dm" % (seconds // 3600, (seconds % 3600) // 60)

    def _start_clock(self):
        self._stop_clock()
        self.test_start = time.time()
        self.test_elapsed = 0
        self.device_elapsed_ms = None
        self.elapsed_var.set("测试耗时 00:00:00")
        self._tick_clock()

    def _tick_clock(self):
        self.clock_job = None
        if self.test_start is None:
            return
        self.test_elapsed = time.time() - self.test_start
        # 设备累计时长更权威(跨重启累加)；没有读到之前先用本机计时兜底
        if self.device_elapsed_ms is not None:
            shown = self.device_elapsed_ms / 1000.0
        else:
            shown = self.test_elapsed
        self.elapsed_var.set("测试耗时 " + self._format_elapsed(shown))
        self.clock_job = self.after(200, self._tick_clock)

    def _stop_clock(self):
        if self.clock_job is not None:
            self.after_cancel(self.clock_job)
            self.clock_job = None
        if self.test_start is not None:
            self.test_elapsed = time.time() - self.test_start
            self.test_start = None
            shown = self.device_elapsed_ms / 1000.0 if self.device_elapsed_ms is not None else self.test_elapsed
            self.elapsed_var.set("测试耗时 " + self._format_elapsed(shown))

    def _schedule_poll(self, delay_ms):
        self._cancel_poll()
        self.poll_job = self.after(delay_ms, self._poll)

    def _cancel_poll(self):
        if self.poll_job is not None:
            self.after_cancel(self.poll_job)
            self.poll_job = None

    def _poll(self):
        self.poll_job = None
        if not self.testing:
            return
        # 链路断开: 不中断测试(耳机自己在跑), 按退避持续重连
        if self.link_down_since is not None or not self._serial_open():
            if self.link_down_since is None:
                self._mark_link_down("串口未打开")
            if self.auto_reconnect.get():
                self._try_reconnect()
            if self.link_down_since is not None:
                # 还没回来: 保持等待, 不改测试状态
                self.status_var.set("串口断开(%s)，等待重连 %ds" % (
                    self.link_down_reason or "未知原因", self._link_down_seconds()))
                if self._link_down_seconds() >= RECONNECT_TIMEOUT_S:
                    self._halt_on_error(
                        "重连超时",
                        "串口断开超过 %d 秒仍未恢复。\n耳机断点未清除，链路修好后点查询可继续看进度。"
                        % RECONNECT_TIMEOUT_S,
                        "重连超时，测试计时已停止",
                    )
                    return
                self._schedule_poll(1000)
                return
            # 刚重连上: 立刻走下面的正常轮询, 不等 1s
            self.status_var.set("串口已重连，继续轮询")
        try:
            data = self._transact(CMD_QUERY, b"")
            self.query_miss = 0
            self._apply_status(data)
        except LinkError as exc:
            # 链路异常不计入"无应答", 下一次循环转去重连
            self.query_miss = 0
            self._log("串口断开: %s" % exc)
        except Exception as exc:
            self.query_miss += 1
            self._log("查询失败(%d/%d): %s" % (self.query_miss, MAX_QUERY_MISS, exc))
            self.status_var.set("查询失败(%d/%d): %s" % (self.query_miss, MAX_QUERY_MISS, exc))
            # 蓝牙 SPP 会出现"端口还在、链路假死": 隔几拍关掉重开一次试试
            if (self.auto_reconnect.get() and self.query_miss >= 3
                    and time.time() - self.last_reopen_try >= 5.0):
                self.last_reopen_try = time.time()
                try:
                    if not self._reopen_serial():
                        self._log("端口 %s 不在系统中, 等待链路恢复" % self._selected_port())
                except Exception as reopen_exc:
                    self._mark_link_down(str(reopen_exc))
            if self.query_miss >= MAX_QUERY_MISS:
                self._halt_on_error(
                    "无响应",
                    "连续 %d 次查询无有效应答，已停止计时。\n耳机断点未清除。" % MAX_QUERY_MISS,
                    "无响应，测试计时已停止",
                )
                return
        if self.testing:
            try:
                interval = max(1, int(self.interval_var.get()))
            except ValueError:
                interval = 2
            self._schedule_poll(interval * 1000)

    def _halt_on_error(self, title, text, status):
        self.testing = False
        self.status_locked = True
        self._stop_clock()
        self._cancel_poll()
        self._set_start_button(True)
        self._set_config_locked(False)
        self._save_config()
        self.status_var.set(status)
        messagebox.showerror(title, text)

    def _finish_test(self, status, notify, title, text):
        self.testing = False
        self.status_locked = True
        self._stop_clock()
        self._cancel_poll()
        self._set_start_button(True)
        self._set_config_locked(False)
        self._save_config()
        self.status_var.set(status)
        if notify:
            messagebox.showinfo(title, text)

    def _apply_status(self, data, notify=True):
        """0x7C 应答 89 字节，见固件 sndp_aging_status_t / 4A.5。"""
        if len(data) < QUERY_DATA_LEN:
            raise ProtocolError(
                "0x7C 应答 %d 字节, 期望 %d 字节, 耳机固件可能过旧(旧格式为 34 字节)"
                % (len(data), QUERY_DATA_LEN))
        if data[0] != 0:
            raise ProtocolError(COMM_ERRORS.get(data[0], "查询错误 0x%02X" % data[0]))

        running = data[1]
        total_times = (data[2] << 8) | data[3]
        temperature = int.from_bytes(data[4:6], "big", signed=True)
        voltage = (data[6] << 8) | data[7]
        percent = data[8]
        charger = data[9]
        elapsed_ms = int.from_bytes(data[10:14], "big")
        curr_big = data[14]
        curr_sub = data[15]
        fail_code = data[16]
        target = [(data[STATUS_TARGET_OFF + i * 2] << 8) | data[STATUS_TARGET_OFF + i * 2 + 1]
                  for i in range(BIG_QTY)]
        done = [(data[STATUS_DONE_OFF + i * 2] << 8) | data[STATUS_DONE_OFF + i * 2 + 1]
                for i in range(BIG_QTY)]
        run_time_s = [int.from_bytes(
            data[STATUS_TIME_OFF + i * 4:STATUS_TIME_OFF + i * 4 + 4], "big")
            for i in range(BIG_QTY)]

        self.device_elapsed_ms = elapsed_ms

        finished = (curr_big == BIG_DONE)
        # 记录"进入过"的大项: 进入过但轮次没到目标 => 该大项失败被跳过
        if curr_big != BIG_DONE:
            self.seen_bigs.add(curr_big)
            self.seen_bigs.update(range(1, curr_big))
        else:
            # 已完成/已终止: 用实际跑过的证据补一份(防止漏采)
            for idx in range(1, 9):
                if target[idx] > 0 and (done[idx] > 0 or run_time_s[idx] > 0):
                    self.seen_bigs.add(idx)

        for i, (key, _name, _default) in enumerate(ITEMS):
            idx = ITEM_KEY_TO_CFG[key]
            tgt = target[idx]
            dn = done[idx]
            self.done_vars[i].set(str(dn))
            self.time_vars[i].set(self._format_duration_s(run_time_s[idx]))
            if tgt == 0:
                result = "跳过"
            elif dn >= tgt:
                result = "通过"
            elif idx == curr_big:
                result = "测试中" if running else "已停止"
            elif idx in self.seen_bigs:
                result = "失败"
            else:
                result = "等待"
            self.row_state[i] = result
            self.result_vars[i].set(result)
            self._paint_result(i, result)

        big_name = BIG_NAMES.get(curr_big, "未知(0x%02X)" % curr_big)
        sub_name = SUB_ITEMS.get(curr_sub, "未知(%d)" % curr_sub)
        # 终检里两个耗时步骤给明确提示, 免得看着像卡住
        hint = ""
        if running and curr_big == BIG_FINAL:
            if curr_sub == SUB_CHARGE_WAIT:
                hint = "   (等待进入充电，最长 %d 秒)" % BIG_CHARGE_WAIT_SEC
            elif curr_sub == SUB_TOPUP:
                hint = "   (补电中，充到 ≥%d%% 自动停充)" % TOPUP_TARGET_PCT
        if finished:
            head = "当前: 全部完成"
        elif running:
            head = "当前大项: %s   小项: %s%s" % (big_name, sub_name, hint)
        else:
            head = "当前大项: %s (未在跑)" % big_name
        self.status_var.set("%s    %s" % (head, "运行中" if running else "未在跑"))
        # total_times 是"耳机收到开始指令的累计次数"(固件 total_run, 每次 0x7A +1):
        # 与当前大项/小项、与轮次都无关, 所以单独一行, 免得被当成当前大项测试次数
        self.total_var.set("累计老化测试 %d 次" % total_times)
        self.detail_var.set("温度 %d℃    电压 %d mV    电量 %d%%    充电状态 %d" % (
            temperature, voltage, percent, charger))
        if self.test_start is not None:
            self.elapsed_var.set("测试耗时 " + self._format_elapsed(elapsed_ms / 1000.0))

        # 失败不再终止老化: 只提示, 继续轮询; 全部完成后统一给结论
        if fail_code:
            self.fail_var.set("发现失败项  错误码 %d %s（跳过该大项剩余轮次后继续）" % (
                fail_code, FAIL_CODES.get(fail_code, "未知")))
        else:
            self.fail_var.set("")

        if finished:
            # 注意: 从未开始 / 已被 0x7D 0x00 清除 的设备, curr_big_id 同样是 0xFF。
            # 用"跑过证据"区分: 空闲 vs 真正完成。协议里没有 finished 字段。
            ran = total_times > 0 or any(
                done[ITEM_KEY_TO_CFG[key]] > 0 for key, _n, _d in ITEMS)
            if not ran:
                self.fail_var.set("")
                if self.testing:
                    self._finish_test("空闲（耳机上无有效老化记录）", False, "", "")
                else:
                    self.status_var.set("空闲（耳机上无有效老化记录）")
            elif fail_code:
                self.fail_var.set("测试结束  存在失败项  错误码 %d %s" % (
                    fail_code, FAIL_CODES.get(fail_code, "未知")))
                self._finish_test(
                    "测试结束，存在失败项", notify, "测试结束",
                    "老化已走完全部大项，但存在失败项：\n%s" % self.fail_var.get())
            else:
                self._finish_test(
                    "测试通过", notify, "测试完成",
                    "老化已完成，未发现失败项。\n\n"
                    "请把耳机放入充电仓，再点“充电检测”按钮做 %d 秒补电复核。"
                    % CHARGE_CHECK_SECONDS)
        elif not running:
            # 被外部停止(或本机刚发过停止), 不再继续轮询
            self._finish_test("已停止", False, "", "")

    def _on_close(self):
        self.testing = False
        self._cancel_poll()
        self.charge_check_on = False
        if self.charge_check_job is not None:
            self.after_cancel(self.charge_check_job)
            self.charge_check_job = None
        if self.health_job is not None:
            self.after_cancel(self.health_job)
            self.health_job = None
        self._stop_clock()
        self._save_config()
        self._close_serial()
        self.destroy()


def main():
    if serial is None:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("缺少依赖", "请先安装 pyserial:\n\npip install pyserial")
        return
    AgingTool().mainloop()


if __name__ == "__main__":
    main()
