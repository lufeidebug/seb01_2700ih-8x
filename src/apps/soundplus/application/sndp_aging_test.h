#ifndef __SNDP_AGING_TEST_H__
#define __SNDP_AGING_TEST_H__

#if defined(__SNDP_AGING_TEST__)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * 整机老化测试 (Aging Test / Run In)
 *
 * 依据: doc/老化测试/耳机整机老化测试执行文档.html
 * 平台: 恒玄 BES2700IH
 *
 * 执行模型(表驱动, 与旧版的"阶段 switch"不同):
 *   1) 小项信息表 sndp_aging_sub_info_t (cpp 内)
 *        { 小项 ID, 名称, 开始测试函数, 查询结果函数, 前置等待, 开始后等待, 标志 }
 *   2) 大项信息表 sndp_aging_big_info_t (cpp 内)
 *        { 大项 ID, 名称, 小项 ID 列表, 小项个数, 进入动作, 保持时长 }
 *      执行时用"小项 ID"到小项表里比较查找, 得到小项信息后依次:
 *        打印开始 -> 前置等待 -> 开始测试 -> 开始后等待 -> 查询结果 -> 打印结果
 *   3) 大项测试数据 sndp_aging_big_result_t (cpp 内)
 *        { 大项 ID, 结果, 目标循环, 已完成循环, 失败小项 ID, 失败原因 }
 *   4) 全部测试数据 sndp_aging_test_data_t (cpp 内)
 *        { 是否老化中, 是否已测完, 累计轮次, 当前大项 ID, 当前小项 ID, 运行时长, 各大项数据数组 }
 *      flash 里存 sndp_aging_store_data_t = { magic, checksum, cfg, test_data }
 *
 *   大项一轮循环全部小项通过后才累加"已完成循环"; 达到目标循环则该大项结束并落盘。
 *   任一小项失败: 记录失败小项与原因并落盘, 跳过该大项剩余循环, 直接进入下个大项。
 *   所有大项走完即"测完"。
 *   每次开机从 flash 载入, 若"老化中且未测完"则从记录的大项继续。
 *
 * 大项顺序:
 *   1 SRAM -> 2 FLASH -> 3 软重启 -> 4 硬重启 -> 5 中负载 -> 6 高负载 -> 7 总线 -> 8 结束
 *
 * AT 指令示例(与参考表格一致):
 *   AT^RUNTEST=1,charge:1,sram:1,flash:1,soft_rst:10,hard_rst:10,mid:18,high:18,bus:10,end:1
 *   AT^RUNTEST=0                 // 退出老化
 **************************************************************************************************/

/**************************************************************************************************
 * 失败原因码
 **************************************************************************************************/
typedef enum {
    SNDP_AGING_FAIL_NONE = 0,
    SNDP_AGING_FAIL_DEVICE = 1,         /* 主控 / 器件检测失败                  */
    SNDP_AGING_FAIL_FLASH,              /* 内置 Flash 读写校验失败              */
    SNDP_AGING_FAIL_SENSOR,             /* 传感器(DA217E / SSH401A)读取失败     */
    SNDP_AGING_FAIL_VSENSOR,            /* VSENSOR 供电异常(两颗传感器共用)     */
    SNDP_AGING_FAIL_TEMPERATURE,        /* 温度超过 50℃ 红线                    */
    SNDP_AGING_FAIL_CHARGER,            /* 充电 IC 状态异常 / 充放电故障标志    */
    SNDP_AGING_FAIL_BATTERY,            /* 电池电压异常 / 电量读值异常          */
    SNDP_AGING_FAIL_AUDIO,              /* 音频环回 / THD 失败                  */
    SNDP_AGING_FAIL_RF,                 /* 射频发射老化失败                     */
    SNDP_AGING_FAIL_BUS,                /* I2C 总线通信失败                     */
    SNDP_AGING_FAIL_TIMEOUT,            /* 阶段执行超时                         */
    SNDP_AGING_FAIL_UNKNOWN,

    /* 新增原因码一律加在末尾: 数值要通过 0x7C 的 curr_fail_code 报给上位机,
     * 中间插入会改变后面所有码的含义 */
    SNDP_AGING_FAIL_AUDIO_BUSY = 13,    /* 音频通路被蓝牙播放占用(治具环境问题, 非 DUT) */
} sndp_aging_fail_e;

/**************************************************************************************************
 * 配置槽位
 *
 * 顺序与上位机协议(见 sndp_comm_cmd.h 的 0x7C / 0x7D / 0x7E)一致, 严格按 ID 从小到大:
 *   0 charge(保留)  1 sram   2 flash  3 soft_rst  4 hard_rst
 *   5 mid           6 high   7 bus    8 终检(传感器复检 + 检测充电 + 补电)
 * 大项 ID 直接复用这些下标(见 cpp 内 sndp_aging_big_id_e)。
 * 8 号终检排在最后, 目标次数一般填 1; 其中补电过程可能持续几十分钟(见 cpp 内 D4)。
 **************************************************************************************************/
#define SNDP_AGING_ITEM_CHARGE_ID       0
#define SNDP_AGING_ITEM_SRAM_ID         1
#define SNDP_AGING_ITEM_FLASH_ID        2
#define SNDP_AGING_ITEM_SOFT_RST_ID     3
#define SNDP_AGING_ITEM_HARD_RST_ID     4
#define SNDP_AGING_ITEM_MID_ID          5
#define SNDP_AGING_ITEM_HIGH_ID         6
#define SNDP_AGING_ITEM_BUS_ID          7
#define SNDP_AGING_ITEM_END_ID          8
#define SNDP_AGING_ITEM_QTY             9

typedef struct {
    uint8_t  enable;                    /* 是否测试。1 = 要测试, 0 = 不测试 */
    uint16_t target_cycle;              /* 各大项目标循环次数        */
} sndp_aging_big_cfg_t;



/**************************************************************************************************
 * 运行状态(查询用)
 **************************************************************************************************/
typedef struct {
    uint8_t  running;               /* 是否正在老化                         */
    uint16_t total_times;           /* 累计执行老化测试次数                     */
    int16_t  temperature;           /* 当前温度(℃)                        */
    uint16_t bat_voltage;           /* 当前电池电压(mV)                     */
    uint8_t  bat_percentage;        /* 当前电量(%)                        */
    uint8_t  charger_status;        /* 充电状态 sndp_dev_charging_status_e */

    uint32_t elapsed_ms;            /* 本次老化已运行时长(ms)                  */
    uint8_t  curr_big_id;           /* 当前大项 ID(0~8, 测完为 0xFF)           */
    uint8_t  curr_sub_id;           /* 当前小项 ID(0~12, 测完为 0xFF)          */
    uint8_t  curr_fail_code;        /* 失败码 sndp_aging_fail_e          */

    uint16_t target_cycle[SNDP_AGING_ITEM_QTY]; /* 各大项目标轮次            */
    uint16_t run_cycle[SNDP_AGING_ITEM_QTY];    /* 各大项已完成循环           */
    uint32_t run_time_s[SNDP_AGING_ITEM_QTY];   /* 各大项耗时(秒)             */
} sndp_aging_status_t;

/**************************************************************************************************
 * 对外接口
 **************************************************************************************************/

/* 模块初始化(开机调用, 内部负责重启后的断点恢复) */
void sndp_aging_test_init(void);

/* 下发/保存老化配置(来自配置指令), 并持久化到 flash。
 * 配置与过程数据同源(都存在 test_data.big_cfg), 所以运行中下发会立即改变
 * 各大项的 enable / 目标循环。上位机在测试期间会锁住配置区, 正常流程只在
 * "开始测试"之前下发一次。
 * 返回 0 表示成功 */
int32_t sndp_aging_test_set_cfg(sndp_aging_big_cfg_t *cfg);

/* 读取当前老化配置 */
void sndp_aging_test_get_cfg(sndp_aging_big_cfg_t *cfg);

/* 启动老化测试(使用已保存的配置)。未配置任何大项时返回 -1 */
int32_t sndp_aging_test_start(void);

/* 停止老化测试。正在运行时将 flash 中"老化中"标志清掉, 过程数据保留 */
void sndp_aging_test_stop(void);

/* 退出老化模式并清除断点记录(对应 AT^RUNTEST=0) */
void sndp_aging_test_exit(void);

/* 是否正在老化 */
bool sndp_aging_test_is_running(void);

/* 获取当前状态 */
void sndp_aging_test_get_status(sndp_aging_status_t *status);

/* 解析配置指令数据(0x7D 的 DATA 部分)
 * 固定 9 组, 每组 3 字节, 严格按大项 ID 0..8 顺序:
 *   [大项ID(1)][是否测试(1)][目标次数(1)]
 * 大项 ID: 0 charge(保留) 1 sram 2 flash 3 soft_rst 4 hard_rst
 *          5 mid 6 high 7 bus 8 end
 * 返回 0 表示解析成功 */
int32_t sndp_aging_test_cfg_parse(uint8_t *data, uint16_t data_len, sndp_aging_big_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_AGING_TEST__ */
#endif /* __SNDP_AGING_TEST_H__ */
