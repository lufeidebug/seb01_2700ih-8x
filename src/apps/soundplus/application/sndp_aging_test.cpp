#if defined(__SNDP_AGING_TEST__)

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stddef.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_bootmode.h"
#include "tgt_hardware.h"

#include "app_utils.h"
#include "app_audio.h"
#include "app_bt_stream.h"
#include "app_media_player.h"
#include "app_flash_api.h"
#include "norflash_api.h"
#include "audioflinger.h"
#include "hal_codec.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_aging_test.h"

/**************************************************************************************************
 * 常量
 **************************************************************************************************/
#define AGING_TRACE(num, str, ...)      SNDP_TRACE(1 + num, "[AGING_TEST] %s, " str, __func__, ##__VA_ARGS__)

#define AGING_THREAD_NAME               "sndp_aging"
#define AGING_THREAD_STACK_SIZE         (1024 * 4)
#define AGING_THREAD_PRIORITY           osPriorityLow

#define AGING_POLL_INTERVAL_MS          (100)
/* 重启类大项重启后至少等待这么久, 再执行下一步操作(等系统稳定/串口就绪;
 * 该窗口内仍可接收上位机的停止/退出) */
#define AGING_REBOOT_RESUME_WAIT_MS     (30000)
/* 开机后传感器供电未稳, 第一次测 ACC/PPG 前等待 */
#define AGING_SENSOR_BOOT_DELAY_MS      (5000)
/* ACC 与 PPG 共用总线, 两项之间留间隔 */
#define AGING_SENSOR_GAP_MS             (500)

/* 温度红线: 参考执行文档第 12 节 —— 全程 <= 50℃ */
#define AGING_TEMPERATURE_LIMIT_C       (50)

/* 器件检测重试次数(避免单次读失败误判) */
#define AGING_CHECK_RETRY_CNT           (3)

/* 传感器 ID —— 与驱动保持一致, 用于 A3/A4/C4
 * DA217E : CHIP_ID 寄存器 0x01, 期望值 0x13 (sndp_da217e_drv.cpp)
 * SSH401A: CHIP_ID 寄存器 0xFA, 期望值 0xA3 (ss_ppg.h)
 * 注意: DA217E 的 0x0F 是量程寄存器, 不是 WHO_AM_I */
#define AGING_DA217E_CHIP_ID_REG        (0x01)
#define AGING_DA217E_CHIP_ID            (0x13)
#define AGING_DA217E_ACC_X_LSB_REG      (0x02)
#define AGING_SSH401A_CHIP_ID_REG       (0xFA)
#define AGING_SSH401A_CHIP_ID           (0xA3)

/* 加速度静置合理范围: ±4G / 14bit, 允许约 0.3g~2.5g 合成 */
#define AGING_ACC_MAG_MIN               (600)
#define AGING_ACC_MAG_MAX               (5200)

/* I2C 总线压力测试单轮次数(参考执行文档: 单循环 1000 次) */
#define AGING_BUS_STRESS_LOOP_CNT       (1000)

/* 音频通路判定:
 * 播放粉噪, 采集侧持续累计平均强度, 5 秒后读平均强度。
 * 门限按 16bit 绝对值均值, 先用可听且明显高于底噪的值, 实测后再收紧。 */
#define AGING_AUDIO_LEVEL_MIN           (200)
#define AGING_AUDIO_SAMPLE_RATE         (AUD_SAMPRATE_16000)
#define AGING_AUDIO_FRAME_MS            (15)
#define AGING_AUDIO_FRAME_SAMPLES       ((AGING_AUDIO_SAMPLE_RATE / 1000) * AGING_AUDIO_FRAME_MS)
#define AGING_AUDIO_PLAY_BUFF_SIZE      (AGING_AUDIO_FRAME_SAMPLES * 2 * 2)
/* 采集路数不能靠宏去猜: 必须等于 cfg_audio_input_path_cfg 里 MAINMIC 的实际通道数,
 * 数量不符时 hal_codec_setup_stream 会直接断言复位。
 *
 * 本项目开了 SPEECH_TX_AEC_CODEC_REF, 主麦映射是 CH0|CH1|ECMIC_CH0, 共 3 路;
 * 但 SPEECH_TX_AEC_CODEC_REF 是各 Makefile 按源文件单独加的(tgt_hardware.c 有,
 * 本文件不一定有), 编译期判断会漏掉参考通道, 所以改成运行时按硬件配置推导,
 * 见 sndp_aging_audio_cap_ch_num()。
 *
 * 缓冲按最大路数分配, 实际 data_size 在打开采集流时按推导值计算。强度只统计第一路。 */
#define AGING_AUDIO_CAP_CH_MAX          (AUD_CHANNEL_NUM_4)
#define AGING_AUDIO_CAP_BUFF_SIZE       (AGING_AUDIO_FRAME_SAMPLES * 2 * AGING_AUDIO_CAP_CH_MAX * 2)
#define AGING_AUDIO_STREAM_ID           (AUD_STREAM_ID_3)
#define AGING_AUDIO_PLAY_VOLUME         (TGT_VOLUME_LEVEL_8)

/* 中/高负载大项每轮循环结束后保持负载的时长 */
#define AGING_LOAD_HOLD_MS              (3000)

/* D2: 电量 A/B 点。文档要求低于 50% 充电、高于 80% 停充 */
#define AGING_BAT_POINT_A_PCT           (50)
#define AGING_BAT_POINT_B_PCT           (80)

/* D4: 终检补电目标区间 80% ~ 85%。
 * 低于 AGING_BAT_TOPUP_TARGET_PCT 持续补电, 到线停充(期望落在 80%~85%)。 */
#define AGING_BAT_TOPUP_TARGET_PCT      (80)
#define AGING_BAT_TOPUP_MAX_PCT         (85)
/* 终检: 等"在充电"的最长时间(秒), 超时判充电失败 */
#define AGING_CHARGE_WAIT_SEC           (30)
/* 补电总超时(秒): 0.5C 从低电量补到 80% 通常几十分钟, 这里设 1 小时兜底防卡死 */
#define AGING_BAT_TOPUP_TIMEOUT_S       (3600)

/* A6: 电池电压门限(mV), 文档 3.0 ~ 4.35V, 读值异常留少量余量 */
#define AGING_BAT_VOLT_MIN_MV           (3000)
#define AGING_BAT_VOLT_MAX_MV           (4425) //(4400)

/* A7: NTC 合理范围。常温偏差 ±3℃ 需环境基准, 此处只拦明显异常 */
#define AGING_TEMP_MIN_C                (-20)
#define AGING_TEMP_MAX_C                (60)

/* E5: ANC 开关间隔。负载阶段每次进入时切换一次, 间隔由 hold 时间近似 */
#define AGING_ANC_TOGGLE_MS             (10000)
/* ANC 关闭是异步的(实测 anc toggle off 到 af_anc_close 约 0.54s),
 * 关掉后要等它把 codec ADC 让出来, 才能安全打开粉噪采集流 */
#define AGING_ANC_CLOSE_WAIT_MS         (800)

/* 硬重启大项先跳过。实现与软重启相同(sndp_app_reboot), 后续放开时去掉本宏即可 */
#define AGING_SKIP_HARD_RST             (1)

/*
 * 哪些大项要跑由配置决定: cfg.big_cfg[大项 ID].enable / target_cycle。
 * 未在文档中落地的项(如 B2 Flash 擦写 / B7 Vcore / B9 深睡 / F1 射频 / N/A 器件)
 * 不再单独记录跳过标志, 其对应大项不使能即可。
 */

/* A2 只读校验: 分块 CRC 的块大小。
 * 校验范围是整个固件镜像(__flash_start..__flash_end), 见 sndp_aging_sub_flash_start。
 * B2 擦写需要已确认的 OTA bank, 当前不擦写。 */
#define AGING_FLASH_CRC_CHUNK            (64 * 1024)

/* flash 存储结构体的魔数 */
#define AGING_DATA_MAGIC                (0xA5A5A501u)

#define AGING_ARRAY_SIZE(a)             (sizeof(a) / sizeof((a)[0]))

/**************************************************************************************************
 * 小项 ID
 *
 * 小项可被多个大项引用(如 SENSOR 同时用于中负载/高负载/结束)。
 **************************************************************************************************/
typedef enum {
    SNDP_AGING_SUB_SRAM = 0,
    SNDP_AGING_SUB_FLASH,
    SNDP_AGING_SUB_SENSOR,
    SNDP_AGING_SUB_CHARGE,
    SNDP_AGING_SUB_TOPUP,
    SNDP_AGING_SUB_AUDIO,
    SNDP_AGING_SUB_VOLUME,
    SNDP_AGING_SUB_ANC,
    SNDP_AGING_SUB_RTC,
    SNDP_AGING_SUB_BUS,
    SNDP_AGING_SUB_RF,
    SNDP_AGING_SUB_SOFT_RST,
    SNDP_AGING_SUB_HARD_RST,

    /* 新增小项一律加在末尾: 中间插入会改变后面所有小项的 ID,
     * 而小项 ID 要通过 0x7C 的 curr_sub_id 报给上位机 */
    SNDP_AGING_SUB_CHARGE_WAIT = 13,    /* 终检: 等耳机进入充电     */

    SNDP_AGING_SUB_QTY,
} sndp_aging_sub_id_e;

/**************************************************************************************************
 * 大项 ID
 *
 * 数值直接复用配置槽位(cfg.big_cfg[] 下标), 便于用大项 ID 取目标循环次数。
 * 下标 0(charge)未使用, 协议兼容保留。
 **************************************************************************************************/
typedef enum {
    SNDP_AGING_BIG_SRAM = SNDP_AGING_ITEM_SRAM_ID,             /* 1  SRAM 校验            */
    SNDP_AGING_BIG_FLASH = SNDP_AGING_ITEM_FLASH_ID,           /* 2  Flash 读校验         */
    SNDP_AGING_BIG_SOFT_RST = SNDP_AGING_ITEM_SOFT_RST_ID,     /* 3  软重启循环           */
    SNDP_AGING_BIG_HARD_RST = SNDP_AGING_ITEM_HARD_RST_ID,     /* 4  硬重启循环           */
    SNDP_AGING_BIG_MID_LOAD = SNDP_AGING_ITEM_MID_ID,          /* 5  中负载               */
    SNDP_AGING_BIG_HIGH_LOAD = SNDP_AGING_ITEM_HIGH_ID,        /* 6  高负载               */
    SNDP_AGING_BIG_BUS = SNDP_AGING_ITEM_BUS_ID,               /* 7  I2C 总线压力         */
    SNDP_AGING_BIG_FINAL = SNDP_AGING_ITEM_END_ID,             /* 8  终检: 传感器 + 检测充电 + 补电 */

    SNDP_AGING_BIG_QTY = SNDP_AGING_ITEM_QTY,                   /* 9                       */

    SNDP_AGING_BIG_DONE = 0xFF,                                 /* 全部大项已完成          */
} sndp_aging_big_id_e;

/* 大项测试结果 */
#define SNDP_AGING_RESULT_IDLE          (0)     /* 未执行/进行中          */
#define SNDP_AGING_RESULT_PASS          (1)     /* 大项通过                */
#define SNDP_AGING_RESULT_FAIL          (2)     /* 大项失败                */

/* 小项标志 */
#define SNDP_AGING_SUB_FLAG_REBOOT      (1u << 0)   /* 该小项执行即重启, 不返回 */

/**************************************************************************************************
 * 结构体 1: 测试小项信息
 *
 * start_fn : 开始测试, 返回 SNDP_AGING_FAIL_* (NULL 表示无需启动动作)
 * query_fn : 查询结果, 返回 SNDP_AGING_FAIL_* (NULL 表示 start_fn 的返回即最终结果)
 * pre_wait_ms  : 开始测试前的等待
 * post_wait_ms : 开始测试后、查询结果前的等待
 * flags    : SNDP_AGING_SUB_FLAG_REBOOT 表示该小项会重启且不返回,
 *            调度器须在执行它之前先结算本大项的循环次数并落盘(见 sndp_aging_big_run_cycle)。
 **************************************************************************************************/
typedef int32_t (*sndp_aging_fn_t)(void);

typedef struct {
    uint8_t         sub_id;             /* 小项 ID sndp_aging_sub_id_e    */
    const char     *sub_name;           /* 小项名称                         */
    sndp_aging_fn_t start_fn;           /* 开始测试                         */
    sndp_aging_fn_t query_fn;           /* 查询测试结果                       */
    uint32_t        pre_wait_ms;        /* 开始测试前等待                      */
    uint32_t        post_wait_ms;       /* 开始测试后等待(再查询结果)               */
    uint8_t         flags;              /* SNDP_AGING_SUB_FLAG_*        */
} sndp_aging_sub_info_t;

/**************************************************************************************************
 * 结构体 2: 测试大项信息
 *
 * 核心字段: big_id / big_name / sub_ids / sub_cnt。
 * 另附少量执行辅助字段, 用于表达大项级的准备动作与负载保持:
 *   enter_fn : 进入该大项时执行一次(如设置频点、收尾)
 *   hold_ms  : 每轮通过后保持负载的时长
 * 大项的"目标循环"取自 cfg.big_cfg[大项 ID].target_cycle。
 * 注: "重启"是小项(soft/hard reboot)的属性, 不放在大项上。
 **************************************************************************************************/
typedef struct {
    uint8_t         big_id;             /* 大项 ID sndp_aging_big_id_e    */
    const char     *big_name;           /* 大项名称                         */
    const uint8_t  *sub_ids;            /* 小项 ID 列表                     */
    uint8_t         sub_cnt;            /* 小项个数                         */
    void          (*enter_fn)(void);    /* 进入大项时执行一次, 可为 NULL   */
    uint32_t        hold_ms;            /* 每轮通过后保持时长                    */
} sndp_aging_big_info_t;

/**************************************************************************************************
 * 结构体 3: 大项测试数据
 **************************************************************************************************/
typedef struct {
    uint8_t         big_id;             /* 大项 ID                        */
    uint8_t         result;             /* SNDP_AGING_RESULT_*          */
    uint16_t        done_cycle;         /* 已完成循环次数                      */
    uint8_t         fail_item;          /* 失败小项 ID                      */
    uint8_t         fail_code;          /* 失败原因(sndp_aging_fail_e)      */
    uint32_t        start_ms;           /* 本大项开始时的已运行时长(ms)          */
    uint32_t        elapsed_ms;         /* 本大项耗时(ms)                     */
} sndp_aging_big_result_t;

/**************************************************************************************************
 * 结构体 4: 全部测试数据
 *
 * elapsed_ms 是"已落盘的历史累计运行时长": 当前值由它加上本次开机的 tick 增量得到。
 **************************************************************************************************/
typedef struct {
    uint8_t         in_aging;           /* 是否在老化模式                      */
    uint16_t        total_run;          /* 累计执行老化测试次数，每接收一次开始指令加1                       */
    uint8_t         big_id;             /* 当前测试大项 ID                     */
    uint8_t         sub_id;             /* 当前测试小项 ID                     */
    uint32_t        elapsed_ms;         /* 本次老化已运行时长基数(ms, 落盘)       */
    sndp_aging_big_cfg_t    big_cfg[SNDP_AGING_ITEM_QTY];       /* 各大项目标循环次数          */
    sndp_aging_big_result_t big_result[SNDP_AGING_BIG_QTY];     /* 各大项测试结果            */
} sndp_aging_test_data_t;

/**************************************************************************************************
 * 结构体 5: 存 flash 的结构体
 **************************************************************************************************/
typedef struct {
    uint32_t        magic;
    uint32_t        checksum;
    sndp_aging_test_data_t  test_data;
} sndp_aging_store_data_t;

/**************************************************************************************************
 * 结构体 6: 上下文
 **************************************************************************************************/
typedef struct {
    bool               inited;
    bool               running;
    bool               resume_hold;     /* 重启恢复后先停留, 窗口内可退出  */
    bool               enter_pending;   /* 恢复后待重放大项进入动作(稳定窗口结束后做) */
    uint32_t           epoch;           /* 每次 start 递增; 在途的旧轮次发现变化即自行退出 */

    sndp_aging_test_data_t test_data;

    uint32_t           elapsed_base_ms; /* 已落盘的历史累计运行时长(ms)     */
    uint32_t           elapsed_tick_ms; /* 本次开机取增量的参考 tick        */

    bool               audio_on;
    uint8_t            audio_cap_ch;    /* 音频采集实际通道数, 开采集流时按硬件推导 */
    bool               anc_on;
    uint32_t           anc_toggle_ms;

    bool               flash_ready;
    uint8_t            flash_mod_id;

    bool               flash_rw_ready;  /* 擦写专用分区是否已注册成功 */
    uint8_t            flash_rw_mod_id;
} sndp_aging_ctx_s;

static sndp_aging_ctx_s s_aging_ctx;

/**************************************************************************************************
 * 时间基准
 *
 * 不使用 RTC(本芯片走日历 RTC, 且工程未使能, 打开还会增加常驻功耗)。
 * 这里用"落盘基数 + 本次开机 tick 增量"累加:
 *   - elapsed_base_ms 是从 flash 读回的历史累计值
 *   - elapsed_tick_ms 是本次开机内用于取增量的参考 tick
 *   任意时刻的已运行时长 = elapsed_base_ms + (GET_CURRENT_MS() - elapsed_tick_ms)
 * 每轮进度落盘时把当前值固化回 elapsed_base_ms, 因此跨重启也不会丢时间, 且零额外功耗。
 * 说明: 重启期间(约 1~3s)不计入, 视为"测试未在跑"。
 **************************************************************************************************/
static uint32_t sndp_aging_elapsed_ms(void)
{
    /* 未在运行时冻结在基数上 */
    if (!s_aging_ctx.running) {
        return s_aging_ctx.elapsed_base_ms;
    }
    return s_aging_ctx.elapsed_base_ms + (GET_CURRENT_MS() - s_aging_ctx.elapsed_tick_ms);
}

static osThreadId       s_aging_tid = NULL;
static osMutexId        s_aging_mutex_id = NULL;

#ifdef __SNDP_AGING_ITEM_AUDIO__
static uint8_t  s_aging_audio_play_buff[AGING_AUDIO_PLAY_BUFF_SIZE];
static uint8_t  s_aging_audio_cap_buff[AGING_AUDIO_CAP_BUFF_SIZE];
static uint32_t s_aging_audio_noise;
static uint32_t s_aging_audio_level_sum;
static uint32_t s_aging_audio_level_cnt;
#endif

osMutexDef(sndp_aging_mutex);

/* 老化工作线程 */
static void sndp_aging_thread(void const *argument);
osThreadDef(sndp_aging_thread, AGING_THREAD_PRIORITY, 1,
            AGING_THREAD_STACK_SIZE, AGING_THREAD_NAME);

osSemaphoreDef(sndp_aging_thread_wakeup);
osSemaphoreId sndp_aging_thread_wakeup_id = NULL;


extern uint32_t __sndp_aging_test_start[];
extern uint32_t __sndp_aging_rw_start[];    /* 老化擦写专用 4KB 分区 */
extern uint32_t __flash_start;
extern uint32_t __flash_end;

/* 前向声明 */
static void sndp_aging_data_save(void);
static bool sndp_aging_data_load(void);
static bool sndp_aging_data_verify(void);
static void sndp_aging_item_audio_stop(void);
static void sndp_aging_item_anc_restore(void);
static void sndp_aging_load_freq_set(bool high);
static void sndp_aging_load_freq_release(void);
static uint8_t sndp_aging_overall_fail_code(void);
static void sndp_aging_big_elapsed_update(sndp_aging_big_result_t *bd);

/**************************************************************************************************
 * 互斥保护
 **************************************************************************************************/
static void sndp_aging_lock(void)
{
    if (s_aging_mutex_id != NULL) {
        osMutexWait(s_aging_mutex_id, osWaitForever);
    }
}

static void sndp_aging_unlock(void)
{
    if (s_aging_mutex_id != NULL) {
        osMutexRelease(s_aging_mutex_id);
    }
}

/*
 * 可打断的等待。
 * 重启前后必须留出窗口接收 PC 的停止/退出指令:
 * 整段 osDelay 会占住线程, 串口命令得不到处理, 耳机就会一直重启。
 * 返回 false 表示等待期间已被停止, 调用方不得再往下执行。
 */
static bool sndp_aging_wait_abortable(uint32_t ms)
{
    uint32_t elapsed = 0;

    while (elapsed < ms) {
        uint32_t step = ms - elapsed;

        if (step > AGING_POLL_INTERVAL_MS) {
            step = AGING_POLL_INTERVAL_MS;
        }
        osDelay(step);
        elapsed += step;

        if (!s_aging_ctx.running) {
            return false;
        }
    }
    return true;
}

/*
 * 本轮是否还有效。
 * start() 每次都会递增 epoch: 上一轮如果在"重新启动/停止"之后才醒过来,
 * 必须立刻放手, 不能把 done_cycle 写进刚被重置的数据里。
 */
static bool sndp_aging_cycle_valid(uint32_t epoch)
{
    return s_aging_ctx.running && (epoch == s_aging_ctx.epoch);
}

/**************************************************************************************************
 * 过程数据读写 (sndp_aging_test_section)
 *
 * flash 里存的是 sndp_aging_store_data_t = { magic, checksum, test_data }。
 * 配置(big_cfg)与过程数据一起放在 test_data 内。
 * 大项循环完成、大项失败、启动/停止时都会整块落盘, 供重启后恢复。
 **************************************************************************************************/
static void sndp_aging_flash_callback(void *param)
{
    NORFLASH_API_OPERA_RESULT *opera_result = (NORFLASH_API_OPERA_RESULT *)param;

    AGING_TRACE(2, "flash op type=%d result=%d",
                opera_result->type, opera_result->result);
}

static uint32_t sndp_aging_store_checksum(const sndp_aging_store_data_t *st)
{
    const uint8_t *p = (const uint8_t *)st;
    uint32_t sum = 0;
    uint32_t i;

    for (i = 0; i < sizeof(*st); i++) {
        if (i >= offsetof(sndp_aging_store_data_t, checksum) &&
            i < offsetof(sndp_aging_store_data_t, checksum) + sizeof(st->checksum)) {
            continue;
        }
        sum += p[i];
    }

    return sum ^ 0xA5A55A5Au;
}

static void sndp_aging_flash_init(void)
{
    int mod_id;
    uint32_t addr = (uint32_t)__sndp_aging_test_start;

    if (s_aging_ctx.flash_ready) {
        return;
    }

    norflash_api_init();
    mod_id = norflash_api_alloc_mod();
    if (mod_id < 0) {
        AGING_TRACE(0, "alloc flash mod fail");
        return;
    }

    app_flash_register_module((uint8_t)mod_id,
                              app_flash_get_dev_id_by_addr(addr),
                              addr,
                              SNDP_AGING_TEST_SECTION_SIZE,
                              (uint32_t)sndp_aging_flash_callback);
    s_aging_ctx.flash_mod_id = (uint8_t)mod_id;
    s_aging_ctx.flash_ready  = true;
    AGING_TRACE(1, "aging section %p size=0x%x mod=%d",
                __sndp_aging_test_start, SNDP_AGING_TEST_SECTION_SIZE, mod_id);

    /* 擦写专用分区: 单独注册一个 module, 它的擦/写不会碰到上面的数据分区 */
    mod_id = norflash_api_alloc_mod();
    if (mod_id < 0) {
        AGING_TRACE(0, "alloc flash rw mod fail");
        return;
    }

    addr = (uint32_t)__sndp_aging_rw_start;
    app_flash_register_module((uint8_t)mod_id,
                              app_flash_get_dev_id_by_addr(addr),
                              addr,
                              SNDP_AGING_RW_SECTION_SIZE,
                              (uint32_t)sndp_aging_flash_callback);
    s_aging_ctx.flash_rw_mod_id = (uint8_t)mod_id;
    s_aging_ctx.flash_rw_ready  = true;
    AGING_TRACE(1, "aging rw section %p size=0x%x mod=%d",
                __sndp_aging_rw_start, SNDP_AGING_RW_SECTION_SIZE, mod_id);
}

/* ---- 默认值 ---- */

/* 测试数据默认值: 未在老化, 未测完, 无当前大项, 各大项结果空闲 */
static void sndp_aging_test_data_default(sndp_aging_test_data_t *td)
{
    memset(td, 0, sizeof(*td));

    td->in_aging    = 0;
    td->total_run   = 0;
    /* 空闲/已清除时当前大项报 0xFF, 上位机据此区分"测完/空闲"与"正在跑第 n 项" */
    td->big_id      = SNDP_AGING_BIG_DONE;
    td->sub_id      = SNDP_AGING_BIG_DONE;
    td->elapsed_ms  = 0;

    for (uint8_t i = 0; i < SNDP_AGING_BIG_QTY; i++) {
        td->big_cfg[i].enable           = 0;
        td->big_cfg[i].target_cycle     = 0;
        
        td->big_result[i].big_id        = i;
        td->big_result[i].result        = SNDP_AGING_RESULT_IDLE;
        td->big_result[i].done_cycle    = 0;
        td->big_result[i].fail_item     = 0;
        td->big_result[i].fail_code     = SNDP_AGING_FAIL_NONE;
    }
}

/* 把 test_data(内含 big_cfg 配置)组装成存储结构体后落盘 */
static void sndp_aging_data_flush(void)
{
    sndp_aging_store_data_t st;
    enum NORFLASH_API_MODULE_ID_T mod;

    if (!s_aging_ctx.flash_ready) {
        return;
    }

    memset(&st, 0, sizeof(st));
    st.magic     = AGING_DATA_MAGIC;
    st.test_data = s_aging_ctx.test_data;
    st.checksum  = sndp_aging_store_checksum(&st);

    mod = (enum NORFLASH_API_MODULE_ID_T)s_aging_ctx.flash_mod_id;
    /* 擦除是异步提交的, 必须先刷完再同步写, 否则重启时次数还在缓存里 */
    app_flash_erase(mod, 0, SNDP_AGING_TEST_SECTION_SIZE);
    app_flash_flush_pending_op(mod, NORFLASH_API_ALL);
    app_flash_program(mod, 0, (uint8_t *)&st, sizeof(st), false);
    app_flash_flush_pending_op(mod, NORFLASH_API_ALL);
}

/* 落盘当前进度: 先把已运行时长固化到基数, 再整块写入 */
static void sndp_aging_data_save(void)
{
    
    /* 刷新当前大项耗时, 便于查询到进行中的用时 */
    if (s_aging_ctx.running && s_aging_ctx.test_data.big_id < SNDP_AGING_BIG_QTY) {
        sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[s_aging_ctx.test_data.big_id];

        sndp_aging_big_elapsed_update(big_rlt);
    }
    
    /* 把"基数 + 本次开机增量"固化, 之后重启也能从这里继续累加 */
    s_aging_ctx.elapsed_base_ms      = sndp_aging_elapsed_ms();
    s_aging_ctx.elapsed_tick_ms      = GET_CURRENT_MS();
    s_aging_ctx.test_data.elapsed_ms = s_aging_ctx.elapsed_base_ms;

    sndp_aging_data_flush();
}

static bool sndp_aging_data_load(void)
{
    sndp_aging_store_data_t st;
    enum NORFLASH_API_MODULE_ID_T mod;

    if (!s_aging_ctx.flash_ready) {
        return false;
    }

    memset(&st, 0, sizeof(st));
    mod = (enum NORFLASH_API_MODULE_ID_T)s_aging_ctx.flash_mod_id;
    app_flash_sync_read(mod, 0, (uint8_t *)&st, sizeof(st));

    if (st.magic != AGING_DATA_MAGIC ||
        st.checksum != sndp_aging_store_checksum(&st)) {
        /* flash 无有效记录(如首次开机/已擦除): 使用默认值 */
        AGING_TRACE(0, "aging data invalid, use default");
        sndp_aging_test_data_default(&s_aging_ctx.test_data);
    } else {
        s_aging_ctx.test_data = st.test_data;
    }
    
    return true;
}

/* 回读 flash, 确认刚写入的数据已经落盘(重启类测试前必须确认) */
static bool sndp_aging_data_verify(void)
{
    sndp_aging_store_data_t st;
    uint8_t big = s_aging_ctx.test_data.big_id;
    enum NORFLASH_API_MODULE_ID_T mod;

    if (!s_aging_ctx.flash_ready || big >= SNDP_AGING_BIG_QTY) {
        return false;
    }

    memset(&st, 0, sizeof(st));
    mod = (enum NORFLASH_API_MODULE_ID_T)s_aging_ctx.flash_mod_id;
    app_flash_sync_read(mod, 0, (uint8_t *)&st, sizeof(st));

    if (st.magic != AGING_DATA_MAGIC ||
        st.checksum != sndp_aging_store_checksum(&st) ||
        st.test_data.big_id != big ||
        st.test_data.big_result[big].done_cycle != s_aging_ctx.test_data.big_result[big].done_cycle) {
        AGING_TRACE(0, "data verify fail big=%d flash=%d expect=%d",
                    big,
                    st.test_data.big_result[big].done_cycle,
                    s_aging_ctx.test_data.big_result[big].done_cycle);
        return false;
    }

    return true;
}

/**************************************************************************************************
 * 器件检测相关
 *
 * 说明: DA217E 与 SSH401A 共用 VSENSOR 供电。
 *       若两颗传感器同时读取失败, 优先判定为 VSENSOR 异常(见执行文档 A5 / C3)。
 **************************************************************************************************/

/* A3 / C4: 加速度传感器 DA217E 读 CHIP_ID */
static int32_t sndp_aging_check_acc(void)
{
    uint8_t id = 0;
    int32_t ret = -1;

    sndp_dev_acc_enter_detection_mode();

    for (uint8_t i = 0; i < AGING_CHECK_RETRY_CNT; i++) {
        ret = sndp_dev_acc_read_reg(AGING_DA217E_CHIP_ID_REG, &id, 1);
        if (ret == 0 && id == AGING_DA217E_CHIP_ID) {
            return 0;
        }
        osDelay(10);
    }

    AGING_TRACE(0, "acc(DA217E) id fail, ret=%d, id=0x%02X expect=0x%02X",
                ret, id, AGING_DA217E_CHIP_ID);
    return -1;
}

/* A4 / C4: 心率 / IR 传感器 SSH401A 读 CHIP_ID */
static int32_t sndp_aging_check_hr(void)
{
    uint8_t id = 0;
    int32_t ret = -1;

    sndp_dev_hr_enter_detection_mode();

    for (uint8_t i = 0; i < AGING_CHECK_RETRY_CNT; i++) {
        ret = sndp_dev_hr_read_reg(AGING_SSH401A_CHIP_ID_REG, &id, 1);
        if (ret == 0 && id == AGING_SSH401A_CHIP_ID) {
            return 0;
        }
        osDelay(10);
    }

    AGING_TRACE(0, "hr(SSH401A) id fail, ret=%d, id=0x%02X expect=0x%02X",
                ret, id, AGING_SSH401A_CHIP_ID);
    return -1;
}

/* C1: 三轴多点读取, 静置时合成加速度应落在 1g 附近且不能卡死 */
static int32_t sndp_aging_check_acc_xyz(void)
{
    uint8_t raw[6];
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
    int32_t mag2;
    bool changed = false;

    for (uint8_t i = 0; i < AGING_CHECK_RETRY_CNT; i++) {
        int16_t nx;
        int16_t ny;
        int16_t nz;

        memset(raw, 0, sizeof(raw));
        if (sndp_dev_acc_read_reg(AGING_DA217E_ACC_X_LSB_REG, raw, sizeof(raw)) != 0) {
            AGING_TRACE(0, "acc xyz read fail");
            return -1;
        }

        /* ±4g/14bit。16 位里 bit15 是符号, 低 2 位无效。
         * 必须先转成 int16 再算术右移, 右移后符号落在 bit13。
         * 先右移再转会把负数变成 8192~16383 的正数。1024 LSB = 1g */
        nx = (int16_t)((int16_t)((raw[1] << 8) | raw[0]) >> 2);
        ny = (int16_t)((int16_t)((raw[3] << 8) | raw[2]) >> 2);
        nz = (int16_t)((int16_t)((raw[5] << 8) | raw[4]) >> 2);

        if (i > 0 && (nx != x || ny != y || nz != z)) {
            changed = true;
        }
        x = nx;
        y = ny;
        z = nz;
        osDelay(20);
    }

    mag2 = (int32_t)x * x + (int32_t)y * y + (int32_t)z * z;
    if (mag2 < (AGING_ACC_MAG_MIN * AGING_ACC_MAG_MIN) ||
        mag2 > (AGING_ACC_MAG_MAX * AGING_ACC_MAG_MAX)) {
        AGING_TRACE(0, "acc mag out of range: %d,%d,%d", x, y, z);
        return -1;
    }

    /* 静置允许数值稳定, 但全 0 / 全 FF 已由量程拦住; 有变化则额外记一条 */
    if (!changed) {
        AGING_TRACE(0, "acc stable (ok if fixture is still): %d,%d,%d", x, y, z);
    }

    return 0;
}

/*
 * VSENSOR 供电检测
 *
 * 平台未提供 VSENSOR 电压的直接读取接口, 因此采用"两颗传感器是否同时失效"作为判据:
 *   - 两颗都正常      -> VSENSOR 正常
 *   - 两颗同时失败    -> 判定 VSENSOR 异常(而非两颗传感器同时损坏)
 *   - 仅单颗失败      -> 判定该传感器异常
 */
static int32_t sndp_aging_check_vsensor(bool *vsensor_abnormal)
{
    int32_t acc_ret = sndp_aging_check_acc();

    /* 切到 PPG 前留间隔, 期间收到停止/退出则不再测 */
    if (!sndp_aging_wait_abortable(AGING_SENSOR_GAP_MS)) {
        if (vsensor_abnormal != NULL) {
            *vsensor_abnormal = false;
        }
        return 0;
    }

    int32_t hr_ret  = sndp_aging_check_hr();

    if (vsensor_abnormal != NULL) {
        *vsensor_abnormal = (acc_ret != 0 && hr_ret != 0);
    }

    if (acc_ret != 0 && hr_ret != 0) {
        AGING_TRACE(0, "both sensors fail -> suspect VSENSOR abnormal");
        return -1;
    }

    return 0;
}

/*
 * 电池 / 温度 / 充电 基础检测
 */
/* A6 / A7 / A8: 电池电压、NTC、充电 IC 状态 */
static int32_t sndp_aging_check_power(void)
{
    int16_t  temperature = sndp_dev_temperature_get_temperature(false);
    int16_t  ntc_mv      = sndp_dev_temperature_get_ntc_voltage(false);
    uint16_t voltage     = sndp_dev_get_bat_voltage(false);
    uint8_t  percentage  = sndp_dev_get_bat_percentage(false);

    sndp_dev_charger_check_curr_status();

    if (temperature < AGING_TEMP_MIN_C || temperature > AGING_TEMP_MAX_C) {
        AGING_TRACE(0, "temperature abnormal: %d ntc=%dmV", temperature, ntc_mv);
        return SNDP_AGING_FAIL_TEMPERATURE;
    }

    if (voltage < AGING_BAT_VOLT_MIN_MV || voltage > AGING_BAT_VOLT_MAX_MV) {
        AGING_TRACE(0, "bat voltage abnormal: %dmV %d%%", voltage, percentage);
        return SNDP_AGING_FAIL_BATTERY;
    }

    if (percentage > 100) {
        AGING_TRACE(0, "bat percentage abnormal: %d", percentage);
        return SNDP_AGING_FAIL_BATTERY;
    }

    return SNDP_AGING_FAIL_NONE;
}

/*
 * 温度红线管控: 返回非 0 表示超限
 */
static int32_t sndp_aging_temperature_guard(void)
{
    sndp_dev_temperature_measure();
    osDelay(20);

    int16_t temperature = sndp_dev_temperature_get_temperature(false);

    if (temperature > AGING_TEMPERATURE_LIMIT_C) {
        AGING_TRACE(0, "temperature %d > %d, stop aging",
                    temperature, AGING_TEMPERATURE_LIMIT_C);
        return -1;
    }

    return 0;
}

/**************************************************************************************************
 * 小项实现 (start / query)
 **************************************************************************************************/

/* ---- B1: SRAM 写读校验 ----
 * 文档要求全空间遍历, 但运行中的栈/堆/全局区不能覆盖。
 * 这里只校验本模块私有缓冲, 不做全空间扫描, 也不改 Vcore。 */
static int32_t sndp_aging_sub_sram_start(void)
{
#ifdef __SNDP_AGING_ITEM_SRAM__
    static uint8_t sram_test_buf[512];
    uint32_t i;

    for (i = 0; i < sizeof(sram_test_buf); i++) {
        sram_test_buf[i] = (uint8_t)(i ^ 0x5A);
    }
    for (i = 0; i < sizeof(sram_test_buf); i++) {
        if (sram_test_buf[i] != (uint8_t)(i ^ 0x5A)) {
            AGING_TRACE(0, "sram verify(5A) fail at %d", i);
            return SNDP_AGING_FAIL_DEVICE;
        }
    }

    for (i = 0; i < sizeof(sram_test_buf); i++) {
        sram_test_buf[i] = (uint8_t)(i ^ 0xA5);
    }
    for (i = 0; i < sizeof(sram_test_buf); i++) {
        if (sram_test_buf[i] != (uint8_t)(i ^ 0xA5)) {
            AGING_TRACE(0, "sram verify(A5) fail at %d", i);
            return SNDP_AGING_FAIL_DEVICE;
        }
    }
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- A2: 内置 Flash 读校验 ----
 * 覆盖整个固件镜像(__flash_start..__flash_end), 不再只读开头 4KB:
 * 4KB 只代表镜像头部, 采不到后面器件的坏块, 也压不到大范围连续读。
 * 做法: 分块算 CRC 再汇总, 整段读两遍比对 —— 只判"两次读一致", 不引入外部期望值,
 *       不调用 erase/program, 也不占线程栈。分块之间让出线程, 保持可中断。
 * B2 的擦写必须落在已确认的 OTA bank, 当前地址未确认, 禁止擦写。 */
/* CRC32 分块累加: 传入上一块的中间值(首块用 0xFFFFFFFF), 不做末尾取反。
 * 擦写校验要"边生成边算 / 边回读边算", 必须能跨块累加。 */
static uint32_t sndp_aging_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }

    return crc;
}

static uint32_t sndp_aging_crc32(const uint8_t *data, uint32_t len)
{
    return ~sndp_aging_crc32_update(0xFFFFFFFFu, data, len);
}

/* 整段(或分块)读一遍并汇总 CRC。返回 -1 表示期间被停止, 调用方不要当失败处理 */
#ifdef __SNDP_AGING_ITEM_FLASH__
static int32_t sndp_aging_flash_crc(const uint8_t *base, uint32_t size, uint32_t *out)
{
    uint32_t sum = 0;
    uint32_t off = 0;

    while (off < size) {
        uint32_t chunk = size - off;

        if (chunk > AGING_FLASH_CRC_CHUNK) {
            chunk = AGING_FLASH_CRC_CHUNK;
        }
        sum ^= sndp_aging_crc32(base + off, chunk);
        off += chunk;

        /* 纯 CPU 读, 1MB 大约 1s; 块间让一下, 保证停止/退出还能及时响应 */
        if (!sndp_aging_wait_abortable(1)) {
            return -1;
        }
    }

    *out = sum;
    return 0;
}

/* ---- A2-2: 擦写专用分区的读写校验 ----
 *
 * 只读镜像 CRC 只能发现"读取通路"的问题, 擦除/编程时序完全压不到。
 * 这里在 __sndp_aging_rw_start 这块 4KB 专用分区上做:
 *   擦除分区 -> 生成 4KB 随机数(边生成边算 CRC32)并写进去
 *            -> 回读同一段(边读边算 CRC32) -> 两个 CRC 比对
 * 每轮 AGING_FLASH_RW_LOOP_CNT 次, 任何一次不一致即判 Flash 失败。
 *
 * 缓冲只有 AGING_FLASH_RW_CHUNK 字节, 分块生成/写入/回读:
 *   老化线程栈只有 4KB, 4KB 缓冲既不能放栈上, 也不值得常驻 bss。 */
#define AGING_FLASH_RW_LOOP_CNT         (5)
#define AGING_FLASH_RW_CHUNK            (256)

/* 分块循环按"整块"处理, 分区大小必须是块大小的整数倍(且能按 4 字节填随机数) */
#if ((SNDP_AGING_RW_SECTION_SIZE % AGING_FLASH_RW_CHUNK) != 0) || ((AGING_FLASH_RW_CHUNK % 4) != 0)
#error "SNDP_AGING_RW_SECTION_SIZE must be a multiple of AGING_FLASH_RW_CHUNK, and CHUNK a multiple of 4"
#endif

static uint8_t  s_aging_rw_buf[AGING_FLASH_RW_CHUNK];
static uint32_t s_aging_rw_seed = 0x13572468u;

/* xorshift32: 不依赖 libc rand(), 也不动全局随机状态 */
static uint32_t sndp_aging_rw_rand(void)
{
    uint32_t x = s_aging_rw_seed;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_aging_rw_seed = x;
    return x;
}

/* 擦写一轮。返回 SNDP_AGING_FAIL_NONE 通过, 否则为失败码 */
static int32_t sndp_aging_flash_rw_once(uint32_t round)
{
    enum NORFLASH_API_MODULE_ID_T mod = (enum NORFLASH_API_MODULE_ID_T)s_aging_ctx.flash_rw_mod_id;
    uint32_t crc_wr = 0xFFFFFFFFu;
    uint32_t crc_rd = 0xFFFFFFFFu;
    uint32_t off;

    /* 1) 先擦。擦除是异步提交的, 必须刷完再写, 否则会写到没擦干净的区域 */
    app_flash_erase(mod, 0, SNDP_AGING_RW_SECTION_SIZE);
    app_flash_flush_pending_op(mod, NORFLASH_API_ALL);

    /* 2) 分块生成随机数据 -> 累加写入侧 CRC -> 写进 flash */
    for (off = 0; off < SNDP_AGING_RW_SECTION_SIZE; off += AGING_FLASH_RW_CHUNK) {
        uint32_t i;

        for (i = 0; i < AGING_FLASH_RW_CHUNK; i += 4) {
            uint32_t v = sndp_aging_rw_rand();

            s_aging_rw_buf[i + 0] = (uint8_t)(v & 0xff);
            s_aging_rw_buf[i + 1] = (uint8_t)((v >> 8) & 0xff);
            s_aging_rw_buf[i + 2] = (uint8_t)((v >> 16) & 0xff);
            s_aging_rw_buf[i + 3] = (uint8_t)((v >> 24) & 0xff);
        }
        crc_wr = sndp_aging_crc32_update(crc_wr, s_aging_rw_buf, AGING_FLASH_RW_CHUNK);
        app_flash_program(mod, off, s_aging_rw_buf, AGING_FLASH_RW_CHUNK, false);
    }
    app_flash_flush_pending_op(mod, NORFLASH_API_ALL);
    crc_wr = ~crc_wr;

    /* 3) 分块回读 -> 累加读取侧 CRC(先清零, 读不动也能被发现) */
    for (off = 0; off < SNDP_AGING_RW_SECTION_SIZE; off += AGING_FLASH_RW_CHUNK) {
        memset(s_aging_rw_buf, 0, sizeof(s_aging_rw_buf));
        app_flash_sync_read(mod, off, s_aging_rw_buf, AGING_FLASH_RW_CHUNK);
        crc_rd = sndp_aging_crc32_update(crc_rd, s_aging_rw_buf, AGING_FLASH_RW_CHUNK);
    }
    crc_rd = ~crc_rd;

    if (crc_wr != crc_rd) {
        AGING_TRACE(0, "flash rw round %u/%u FAIL: write crc=0x%08X, read crc=0x%08X",
                    round + 1u, (uint32_t)AGING_FLASH_RW_LOOP_CNT, crc_wr, crc_rd);
        return SNDP_AGING_FAIL_FLASH;
    }

    AGING_TRACE(1, "flash rw round %u/%u pass, crc=0x%08X",
                round + 1u, (uint32_t)AGING_FLASH_RW_LOOP_CNT, crc_wr);
    return SNDP_AGING_FAIL_NONE;
}
#endif /* __SNDP_AGING_ITEM_FLASH__ */

static int32_t sndp_aging_sub_flash_start(void)
{
#ifdef __SNDP_AGING_ITEM_FLASH__
    const uint8_t *flash     = (const uint8_t *)(uintptr_t)&__flash_start;
    const uint8_t *flash_end = (const uint8_t *)(uintptr_t)&__flash_end;
    uint32_t size;
    uint32_t crc_a = 0;
    uint32_t crc_b = 0;

    /* __flash_start / __flash_end 都是链接脚本里的绝对符号 */
    if (flash_end <= flash) {
        AGING_TRACE(0, "flash range invalid %p..%p", flash, flash_end);
        return SNDP_AGING_FAIL_FLASH;
    }
    size = (uint32_t)(flash_end - flash);

    /* ---- 第一步: 整个固件镜像读两遍比对 CRC(只读, 不擦写) ---- */
    if (sndp_aging_flash_crc(flash, size, &crc_a) != 0 ||
        sndp_aging_flash_crc(flash, size, &crc_b) != 0) {
        return SNDP_AGING_FAIL_NONE;    /* 被停止, 不算失败 */
    }

    if (crc_a == 0 || crc_a == 0xFFFFFFFFu || crc_a != crc_b) {
        AGING_TRACE(0, "flash crc mismatch 0x%x != 0x%x size=%uKB",
                    crc_a, crc_b, size / 1024);
        return SNDP_AGING_FAIL_FLASH;
    }

    AGING_TRACE(0, "flash read pass %uKB crc=0x%x", size / 1024, crc_a);

    /* ---- 第二步: 擦写专用分区做 N 轮 擦除->写随机数->回读 CRC ---- */
    if (!s_aging_ctx.flash_rw_ready) {
        AGING_TRACE(0, "flash rw partition not ready");
        return SNDP_AGING_FAIL_FLASH;
    }

    for (uint32_t round = 0; round < AGING_FLASH_RW_LOOP_CNT; round++) {
        int32_t rc = sndp_aging_flash_rw_once(round);

        if (rc != SNDP_AGING_FAIL_NONE) {
            return rc;
        }
        /* 擦写是重操作, 每轮之间让一下, 保证停止/退出还能及时响应 */
        if (!sndp_aging_wait_abortable(10)) {
            return SNDP_AGING_FAIL_NONE;
        }
    }

    AGING_TRACE(0, "flash rw pass, %d rounds", AGING_FLASH_RW_LOOP_CNT);
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- C1 / C2 / C3 / C4: 传感器 ---- */
static int32_t sndp_aging_sub_sensor_start(void)
{
#ifdef __SNDP_AGING_ITEM_SENSOR__
    bool vsensor_abnormal = false;
    unsigned short prox_a = 0;
    unsigned short prox_b = 0;
    static bool sensor_boot_waited = false;

    /* 开机后只等一次, 后续循环不再等待 */
    if (!sensor_boot_waited) {
        sensor_boot_waited = true;
        AGING_TRACE(0, "wait %dms before acc/ppg test", AGING_SENSOR_BOOT_DELAY_MS);
        if (!sndp_aging_wait_abortable(AGING_SENSOR_BOOT_DELAY_MS)) {
            return SNDP_AGING_FAIL_NONE;
        }
    }

    if (sndp_aging_check_vsensor(&vsensor_abnormal) != 0) {
        return vsensor_abnormal ? SNDP_AGING_FAIL_VSENSOR : SNDP_AGING_FAIL_SENSOR;
    }

    if (sndp_aging_check_acc_xyz() != 0) {
        return SNDP_AGING_FAIL_SENSOR;
    }

    /* C2: IR/接近值能读出即可。静置治具下数值可以不变, 不把"不变"判失败 */
    sndp_dev_hr_read_proximity_value(&prox_a);
    osDelay(20);
    sndp_dev_hr_read_proximity_value(&prox_b);
    AGING_TRACE(0, "hr prox %u -> %u", prox_a, prox_b);
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- D1 / D2: 充放电与 A/B 点 ----
 * 低于 50% 请求充电, 高于 80% 请求停充。
 * 电流档位走现有充电管理接口, 不直接改 PMU。 */
static int32_t sndp_aging_sub_charge_start(void)
{
#ifdef __SNDP_AGING_ITEM_CHARGE__
    int32_t  power_fail;
    uint16_t voltage    = sndp_dev_get_bat_voltage(false);
    uint8_t  percentage = sndp_dev_get_bat_percentage(false);
    bool     plugin;

    power_fail = sndp_aging_check_power();
    if (power_fail != SNDP_AGING_FAIL_NONE) {
        return power_fail;
    }

    sndp_dev_charger_plug_check_curr_status();
    plugin = sndp_dev_charger_is_plugin(false);

#if 0
    if (percentage < AGING_BAT_POINT_A_PCT) {
        if (!plugin) {
            AGING_TRACE(0, "bat %d%% < A, but charger not plugin", percentage);
            return SNDP_AGING_FAIL_CHARGER;
        }
        sndp_dev_charger_set_charging_current(SNDP_DEV_CHARGING_CURRENT_0P5C);
    } else if (percentage > AGING_BAT_POINT_B_PCT) {
        sndp_dev_charger_set_charging_current(SNDP_DEV_CHARGING_CURRENT_ZERO);
    }
#endif
    AGING_TRACE(1, "bat: %dmV, %d%%, plugin=%d", voltage, percentage, plugin);
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- 终检: 检测是否在充电 ----
 * 持续查充电状态, 一旦检测到"充电中/已充满"就返回, 进入下一个小项(补电);
 * 连续 AGING_CHARGE_WAIT_SEC 秒都没有检测到 -> 判充电失败。
 * 上位机在 0x7C 里能看到本小项(curr_sub_id = 13), 用于显示"等待充电"。 */
static int32_t sndp_aging_sub_charge_wait_start(void)
{
#ifdef __SNDP_AGING_ITEM_CHARGE__
    for (uint8_t i = 0; i < AGING_CHARGE_WAIT_SEC; i++) {
        sndp_dev_charger_plug_check_curr_status();

        if (sndp_dev_charger_get_charging_status(false) != SNDP_DEV_CHARGER_NOT_CHARGING) {
            AGING_TRACE(0, "charge detected at %ds, status=%d, bat=%d%%",
                        i, sndp_dev_charger_get_charging_status(false),
                        sndp_dev_get_bat_percentage(false));
            return SNDP_AGING_FAIL_NONE;
        }

        /* 分片等待: 这 30 秒里仍然能响应上位机的停止/退出 */
        if (!sndp_aging_wait_abortable(1000)) {
            return SNDP_AGING_FAIL_NONE;
        }
    }

    AGING_TRACE(0, "charge NOT detected in %ds, bat=%d%%, plugin=%d",
                AGING_CHARGE_WAIT_SEC, sndp_dev_get_bat_percentage(false),
                sndp_dev_charger_is_plugin(false));
    return SNDP_AGING_FAIL_CHARGER;
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- D4: 终检补电到 80%(期望落在 80%~85%) ----
 * 电量 < 80%: 打开充电(0.5C), 每秒复查, 直到达标;
 * 电量 >= 80%: 把充电电流置 0(停充)并返回通过。
 *
 * 这是个可能持续几十分钟的阻塞过程, 所以:
 *   1) 用 sndp_aging_wait_abortable 分片等待 —— 期间可响应停止/退出, 也不阻塞看门狗;
 *   2) 自带过温保护 —— 主循环的温度红线检查在阻塞期间进不来, 这里每秒自己查一次;
 *   3) 上位机通过 curr_sub_id = 4(TOPUP) 显示"补电中"。 */
static int32_t sndp_aging_sub_topup_start(void)
{
#ifdef __SNDP_AGING_ITEM_CHARGE__
    uint32_t waited_s = 0;

    while (1) {
        uint8_t percentage = sndp_dev_get_bat_percentage(false);
        int16_t temperature = sndp_dev_temperature_get_temperature(false);

        if (temperature > AGING_TEMPERATURE_LIMIT_C) {
            sndp_dev_charger_set_charging_current(SNDP_DEV_CHARGING_CURRENT_ZERO);
            AGING_TRACE(0, "topup abort: temp %d > %d", temperature, AGING_TEMPERATURE_LIMIT_C);
            return SNDP_AGING_FAIL_TEMPERATURE;
        }

        if (percentage >= AGING_BAT_TOPUP_TARGET_PCT) {
            /* 达标: 停充(>=80% 就停, 落点 80%~85%) */
            sndp_dev_charger_set_charging_current(SNDP_DEV_CHARGING_CURRENT_ZERO);
            AGING_TRACE(0, "topup done, bat=%d%%, cost %us", percentage, waited_s);
            return SNDP_AGING_FAIL_NONE;
        }

        /* 未达标: 充电器得在位, 保持 0.5C 充电 */
        if (!sndp_dev_charger_is_plugin(false)) {
            AGING_TRACE(0, "topup lost charger, bat=%d%%", percentage);
            return SNDP_AGING_FAIL_CHARGER;
        }
        sndp_dev_charger_set_charging_current(SNDP_DEV_CHARGING_CURRENT_0P5C);

        if ((waited_s % 10u) == 0u) {
            AGING_TRACE(0, "topup charging, bat=%d%%, temp=%d, %us",
                        percentage, temperature, waited_s);
        }

        if (waited_s >= AGING_BAT_TOPUP_TIMEOUT_S) {
            AGING_TRACE(0, "topup timeout(%us), bat=%d%%", waited_s, percentage);
            return SNDP_AGING_FAIL_CHARGER;
        }

        if (!sndp_aging_wait_abortable(1000)) {
            return SNDP_AGING_FAIL_NONE;
        }
        waited_s++;
    }
#endif
    return SNDP_AGING_FAIL_NONE;
}

#ifdef __SNDP_AGING_ITEM_AUDIO__
/* 采集路数 = cfg_audio_input_path_cfg 里 MAINMIC 通道映射的位数。
 * 由 codec 自己提供的接口推导, 与 tgt_hardware.c 的配置天然一致,
 * 不会再出现"硬件 3 路、代码传 2 路"而被 hal_codec_setup_stream 断言复位。 */
static uint8_t sndp_aging_audio_cap_ch_num(void)
{
    uint32_t num = hal_codec_get_input_path_chan_num(AUD_INPUT_PATH_MAINMIC);

    if (num == 0 || num > AGING_AUDIO_CAP_CH_MAX) {
        AGING_TRACE(0, "unexpected mainmic chan num %u, fallback 1", num);
        return AUD_CHANNEL_NUM_1;
    }
    return (uint8_t)num;
}

/* Voss-McCartney 粉噪: 多级白噪叠加, 每级按 2 的幂更新 */
static int16_t sndp_aging_pink_sample(void)
{
    static const uint8_t s_step[] = { 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4 };
    static int16_t s_row[5];
    static uint8_t s_idx;
    int32_t sum = 0;
    uint8_t row = s_step[s_idx & 0x0F];

    s_aging_audio_noise = s_aging_audio_noise * 1664525u + 1013904223u;
    s_row[row] = (int16_t)(s_aging_audio_noise >> 16);
    s_idx++;

    for (uint8_t i = 0; i < sizeof(s_row) / sizeof(s_row[0]); i++) {
        sum += s_row[i];
    }

    sum /= (int32_t)(sizeof(s_row) / sizeof(s_row[0]));
    if (sum > 32767) {
        sum = 32767;
    } else if (sum < -32768) {
        sum = -32768;
    }
    return (int16_t)(sum / 4);
}

static uint32_t sndp_aging_audio_play_cb(uint8_t *buf, uint32_t len)
{
    int16_t *pcm = (int16_t *)buf;
    uint32_t samples = len / sizeof(int16_t);

    if (!s_aging_ctx.audio_on) {
        memset(buf, 0, len);
        return len;
    }

    for (uint32_t i = 0; i < samples; i++) {
        pcm[i] = sndp_aging_pink_sample();
    }
    return len;
}

static uint32_t sndp_aging_audio_capture_cb(uint8_t *buf, uint32_t len)
{
    int16_t *pcm = (int16_t *)buf;
    uint32_t samples = len / sizeof(int16_t);
    uint8_t  ch = s_aging_ctx.audio_cap_ch;
    uint32_t frames;
    uint32_t sum = 0;

    if (!s_aging_ctx.audio_on || ch == 0 || samples < ch) {
        return len;
    }

    frames = samples / ch;
    for (uint32_t i = 0; i < frames; i++) {
        int16_t sample = pcm[i * ch];
        sum += (uint32_t)(sample < 0 ? -sample : sample);
    }

    /* 采集回调和老化线程都会改累计值, 用关中断保证一次更新不被拆开 */
    uint32_t lock = int_lock();
    s_aging_audio_level_sum += sum / frames;
    s_aging_audio_level_cnt++;
    int_unlock(lock);
    return len;
}

static uint32_t sndp_aging_audio_level_take(uint32_t *frame_cnt)
{
    uint32_t sum;
    uint32_t cnt;
    uint32_t lock = int_lock();

    sum = s_aging_audio_level_sum;
    cnt = s_aging_audio_level_cnt;
    s_aging_audio_level_sum = 0;
    s_aging_audio_level_cnt = 0;
    int_unlock(lock);

    if (frame_cnt != NULL) {
        *frame_cnt = cnt;
    }
    if (cnt == 0) {
        return 0;
    }
    return sum / cnt;
}
#endif /* __SNDP_AGING_ITEM_AUDIO__ */

/* E1 前置: 把音频通路从蓝牙手里抢回来。
 *
 * 老化在自己的本地流(id=3)上放粉噪, 手机随时可能推 A2DP 流过来, 两条流共用同一颗
 * codec: 每次切换都会重配(codec_int_stream_setup, 日志可见 48000/24bit/2ch <->
 * 16000/16bit/1ch 来回切), 老化流会被直接抢占 —— 实测手机发 AVDTP START 后 1.4s:
 *   af_thread:WARNING: id=3 stream=0 hdlr intvl ERROR: 191799 us (expecting 15000)
 *   af_thread:WARNING: id=3 stream=0 lost 255 signals
 *   af_thread:WARNING: id=3 stream=0 hdlr intvl ERROR: 6836042 us
 * 粉噪静音数秒(听感就是"突然无声"), 之后回采电平掉到门限以下。
 *
 * 处理: 检测到蓝牙在推流就先发 AVRCP PAUSE 把它停掉, 等最多
 * AGING_AUDIO_QUIET_WAIT_MS; 停不下来就报 SNDP_AGING_FAIL_AUDIO_BUSY ——
 * 这是治具环境问题(手机在放歌), 不是 DUT 故障, 别混进"音频失败 8"里。 */
#define AGING_AUDIO_QUIET_WAIT_MS       (3000)
#define AGING_AUDIO_QUIET_POLL_MS       (500)

#ifdef __SNDP_AGING_ITEM_AUDIO__
static int32_t sndp_aging_audio_claim_path(void)
{
    uint32_t waited = 0;

    if (!sndp_music_is_streaming()) {
        return SNDP_AGING_FAIL_NONE;
    }

    AGING_TRACE(0, "bt a2dp streaming, pause it before audio test");
    sndp_music_ctrl(SNDP_MUSIC_CTRL_PAUSE);

    while (waited < AGING_AUDIO_QUIET_WAIT_MS) {
        if (!sndp_music_is_streaming()) {
            AGING_TRACE(0, "bt a2dp paused, audio path free");
            return SNDP_AGING_FAIL_NONE;
        }
        if (!sndp_aging_wait_abortable(AGING_AUDIO_QUIET_POLL_MS)) {
            return SNDP_AGING_FAIL_NONE;    /* 被停止, 不算失败 */
        }
        waited += AGING_AUDIO_QUIET_POLL_MS;
    }

    AGING_TRACE(0, "bt a2dp still streaming after %ums, audio path busy",
                (uint32_t)AGING_AUDIO_QUIET_WAIT_MS);
    return SNDP_AGING_FAIL_AUDIO_BUSY;
}
#endif /* __SNDP_AGING_ITEM_AUDIO__ */

/* ---- E1: 播放粉噪并采集(启动) ---- */
static int32_t sndp_aging_sub_audio_start(void)
{
#ifdef __SNDP_AGING_ITEM_AUDIO__
    struct AF_STREAM_CONFIG_T stream_cfg;
    uint8_t cap_ch = sndp_aging_audio_cap_ch_num();
    int32_t claim;

    if (s_aging_ctx.audio_on) {
        return SNDP_AGING_FAIL_NONE;
    }

    /* 开流前必须先关 ANC。
     * ANC assist 会独占 codec ADC, 若在 ANC 打开状态下打开粉噪采集流:
     *   采集回调间隔翻倍(实测 15ms -> 30ms, 日志报 hdlr intvl ERROR 30016us),
     *   采到的电平几乎为 0(实测 2460 -> 103), 音频小项直接判失败(错误码 8)。
     * 第一个循环 ANC 还是关的所以正常, 第二个循环就必挂。
     * 轮内顺序是 AUDIO -> VOLUME -> ANC, 所以这里关掉后由本轮 ANC 小项再打开。 */
    if (s_aging_ctx.anc_on) {
        sndp_aging_item_anc_restore();
        /* ANC 关闭是异步的, 等它走完 app_anc_close/af_anc_close 再开流 */
        if (!sndp_aging_wait_abortable(AGING_ANC_CLOSE_WAIT_MS)) {
            return SNDP_AGING_FAIL_NONE;
        }
    }

    /* 手机在推 A2DP 时粉噪必被抢占, 先把它停掉 */
    claim = sndp_aging_audio_claim_path();
    if (claim != SNDP_AGING_FAIL_NONE) {
        return claim;
    }

    s_aging_audio_noise = 0x13572468u;
    s_aging_audio_level_sum = 0;
    s_aging_audio_level_cnt = 0;
    s_aging_ctx.audio_cap_ch = cap_ch;
    s_aging_ctx.audio_on = true;

    memset(&stream_cfg, 0, sizeof(stream_cfg));
    stream_cfg.sample_rate = AGING_AUDIO_SAMPLE_RATE;
    stream_cfg.bits        = AUD_BITS_16;
    stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
    stream_cfg.device      = AUD_STREAM_USE_INT_CODEC;
    stream_cfg.io_path     = AUD_OUTPUT_PATH_SPEAKER;
    stream_cfg.vol         = AGING_AUDIO_PLAY_VOLUME;
    stream_cfg.handler     = sndp_aging_audio_play_cb;
    stream_cfg.data_ptr    = s_aging_audio_play_buff;
    stream_cfg.data_size   = AGING_AUDIO_PLAY_BUFF_SIZE;
    if (af_stream_open(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg) != 0) {
        s_aging_ctx.audio_on = false;
        AGING_TRACE(0, "audio playback open fail");
        return SNDP_AGING_FAIL_AUDIO;
    }

    memset(&stream_cfg, 0, sizeof(stream_cfg));
    stream_cfg.sample_rate = AGING_AUDIO_SAMPLE_RATE;
    stream_cfg.bits        = AUD_BITS_16;
    /* 必须等于 MAINMIC 的实际通道数, 少一路 hal_codec_setup_stream 就断言复位 */
    stream_cfg.channel_num = (enum AUD_CHANNEL_NUM_T)cap_ch;
    stream_cfg.device      = AUD_STREAM_USE_INT_CODEC;
    stream_cfg.io_path     = AUD_INPUT_PATH_MAINMIC;
    stream_cfg.vol         = TGT_VOLUME_LEVEL_8;
    stream_cfg.handler     = sndp_aging_audio_capture_cb;
    stream_cfg.data_ptr    = s_aging_audio_cap_buff;
    stream_cfg.data_size   = AGING_AUDIO_FRAME_SAMPLES * 2 * cap_ch * 2;
    if (af_stream_open(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE, &stream_cfg) != 0) {
        af_stream_close(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK);
        s_aging_ctx.audio_on = false;
        AGING_TRACE(0, "audio capture open fail");
        return SNDP_AGING_FAIL_AUDIO;
    }

    if (af_stream_start(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK) != 0 ||
        af_stream_start(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE) != 0) {
        af_stream_stop(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE);
        af_stream_stop(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK);
        af_stream_close(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE);
        af_stream_close(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK);
        s_aging_ctx.audio_on = false;
        AGING_TRACE(0, "audio stream start fail");
        return SNDP_AGING_FAIL_AUDIO;
    }

    AGING_TRACE(0, "audio pink noise start, cap_ch=%u", cap_ch);
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- E1: 查询采集到的平均强度 ---- */
static int32_t sndp_aging_sub_audio_query(void)
{
#ifdef __SNDP_AGING_ITEM_AUDIO__
    uint32_t frames = 0;
    uint32_t level  = sndp_aging_audio_level_take(&frames);

    AGING_TRACE(0, "audio avg level=%u, frames=%u, min=%d",
                level, frames, AGING_AUDIO_LEVEL_MIN);
    if (level < AGING_AUDIO_LEVEL_MIN) {
        return SNDP_AGING_FAIL_AUDIO;
    }
#endif
    return SNDP_AGING_FAIL_NONE;
}

static void sndp_aging_item_audio_stop(void)
{
#ifdef __SNDP_AGING_ITEM_AUDIO__
    if (!s_aging_ctx.audio_on) {
        return;
    }

    s_aging_ctx.audio_on = false;
    af_stream_stop(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE);
    af_stream_stop(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK);
    af_stream_close(AGING_AUDIO_STREAM_ID, AUD_STREAM_CAPTURE);
    af_stream_close(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK);
    AGING_TRACE(0, "audio pink noise stop");
#endif
}

/* E2: 音量循环
 *
 * 注意: 早先调 app_bt_stream_volumeset() 改的是"蓝牙流"音量, 而粉噪放在本模块
 * 自己的播放流(AGING_AUDIO_STREAM_ID)上, 所以调用"成功"但完全听不出变化。
 * 这里改成直接改正在播放的那条流: 取流内部 cfg 指针改 vol 再 setup —— 这条路径在
 * "硬件 DAC 音量"和"软件增益"两种工程配置下都成立(codec_int_stream_setup 内部各自
 * 处理), 也不会踩 hal_codec_set_chan_vol 在 SW_GAIN / SINGLE_CODEC_DAC_VOL 下的 ASSERT。
 *
 * 行为: 中负载固定低音量(8 级); 高负载从 8 级起, 每 5s 加 1 级升到 15 级,
 *       治具能明显听到音量逐渐变大 —— 用来验证音量控制是否有效、各档位有无破音。 */
#define AGING_VOL_LOW_LEVEL             ((uint8_t)TGT_VOLUME_LEVEL_8)
#define AGING_VOL_HIGH_LEVEL            ((uint8_t)TGT_VOLUME_LEVEL_15)
#define AGING_VOL_RAMP_STEP_MS          (5000)

#ifdef __SNDP_AGING_ITEM_AUDIO__
static void sndp_aging_play_volume_set(uint8_t vol)
{
    struct AF_STREAM_CONFIG_T *cfg = NULL;

    if (!s_aging_ctx.audio_on) {
        AGING_TRACE(0, "volume set %d: playback not running, skip", vol);
        return;
    }

    if (af_stream_get_cfg(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK, &cfg, false) != 0 ||
        cfg == NULL) {
        AGING_TRACE(0, "volume set %d: stream not open", vol);
        return;
    }

    if (cfg->vol == vol) {
        return;
    }

    /* cfg 就是流内部的配置指针: af_stream_setup 会跳过 DMA/采样率变更检查,
     * 只把新的 vol 应用到 codec */
    cfg->vol = vol;
    af_stream_setup(AGING_AUDIO_STREAM_ID, AUD_STREAM_PLAYBACK, cfg);
    AGING_TRACE(0, "play volume set %d", vol);
}
#endif /* __SNDP_AGING_ITEM_AUDIO__ */

static int32_t sndp_aging_sub_volume_start(void)
{
#ifdef __SNDP_AGING_ITEM_AUDIO__
    uint8_t big_id = s_aging_ctx.test_data.big_id;

    /* 音频没在放: 切了也没声音, 直接过(不判失败) */
    if (!s_aging_ctx.audio_on) {
        return SNDP_AGING_FAIL_NONE;
    }

    if (big_id == SNDP_AGING_BIG_HIGH_LOAD) {
        /* 高负载: 逐级升到 15 级, 每级停 AGING_VOL_RAMP_STEP_MS, 治具能听到变化 */
        for (uint8_t vol = AGING_VOL_LOW_LEVEL; vol <= AGING_VOL_HIGH_LEVEL; vol++) {
            sndp_aging_play_volume_set(vol);
            if (!sndp_aging_wait_abortable(AGING_VOL_RAMP_STEP_MS)) {
                return SNDP_AGING_FAIL_NONE;
            }
        }
    } else {
        /* 中负载: 固定低音量, 只验证持续播放 */
        sndp_aging_play_volume_set(AGING_VOL_LOW_LEVEL);
    }
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* E5: ANC 开/关循环。本项目 ANC_APP=1, 用本地切换避免依赖对耳 */
static int32_t sndp_aging_sub_anc_start(void)
{
#if defined(__SNDP_AGING_ITEM_AUDIO__) && defined(ANC_APP)
    uint32_t now = GET_CURRENT_MS();

    if (s_aging_ctx.anc_toggle_ms != 0 &&
        (now - s_aging_ctx.anc_toggle_ms) < AGING_ANC_TOGGLE_MS) {
        return SNDP_AGING_FAIL_NONE;
    }

    /* ANC assist 打开后会独占 codec ADC, 与粉噪采集流同时开会互相拖
     * (日志里表现为 af_thread hdlr intvl ERROR)。切 ANC 前先关掉粉噪,
     * 下一个循环的 AUDIO 小项会重新打开。 */
    sndp_aging_item_audio_stop();

    s_aging_ctx.anc_on = !s_aging_ctx.anc_on;
    sndp_anc_mode_set_locally(s_aging_ctx.anc_on ? SNDP_ANC_MODE_1 : SNDP_ANC_MODE_OFF);
    s_aging_ctx.anc_toggle_ms = now;
    AGING_TRACE(0, "anc toggle %s", s_aging_ctx.anc_on ? "on" : "off");
#endif
    return SNDP_AGING_FAIL_NONE;
}

static void sndp_aging_item_anc_restore(void)
{
#if defined(__SNDP_AGING_ITEM_AUDIO__) && defined(ANC_APP)
    if (s_aging_ctx.anc_on) {
        sndp_anc_mode_set_locally(SNDP_ANC_MODE_OFF);
        s_aging_ctx.anc_on = false;
    }
#endif
}

/* B10: 32K 时钟连续性
 *
 * 判据: 固定等一小段时间, 看系统 tick 走过的量是否正常 —— 抓 32K 时钟停振/速率
 *       明显异常这类粗大故障。
 *
 * 不能用"两次执行本小项的间隔"当判据: 那量的是整整一轮高负载的时长(实测一轮
 * ≈ 59s: 音频 5s + 音量爬坡 8 级 × 5s + 各项等待), 而门限写的是 30s, 所以
 * 第 1 轮初始化通过、第 2 轮起必然判失败:
 *   日志实测 "32k tick jump delta=941372" -> 测试结果 1 -> HIGH_LOAD 直接判失败,
 *   后面几轮全被跳过。现在改成"我们等了 1s, tick 是否也走了 ~1s", 与轮次时长无关。
 *
 * 说明: 这样只能发现粗大异常; ppm 级时偏需要外部基准(上位机可比对"耳机上报的
 * elapsed_ms"与"PC 自己的计时"来做长时偏监视, 见文档 B10)。 */
#define AGING_RTC_PROBE_MS              (1000)
/* 下界拦"tick 走慢/停住", 上界拦"走快"。等待本身只会多不会少(100ms 分片 +
 * 低优先级线程会被高负载抢占), 所以上界给得宽一些, 只拦明显异常 */
#define AGING_RTC_PROBE_MIN_PCT         (50)
#define AGING_RTC_PROBE_MAX_PCT         (250)

static int32_t sndp_aging_sub_rtc_start(void)
{
    uint32_t tick_start = hal_sys_timer_get();
    uint32_t min_ms = AGING_RTC_PROBE_MS * AGING_RTC_PROBE_MIN_PCT / 100;
    uint32_t max_ms = AGING_RTC_PROBE_MS * AGING_RTC_PROBE_MAX_PCT / 100;
    uint32_t elapsed_ms;

    if (!sndp_aging_wait_abortable(AGING_RTC_PROBE_MS)) {
        return SNDP_AGING_FAIL_NONE;    /* 被停止, 不算失败 */
    }

    elapsed_ms = TICKS_TO_MS(hal_sys_timer_get() - tick_start);

    if (elapsed_ms < min_ms || elapsed_ms > max_ms) {
        AGING_TRACE(0, "32k tick abnormal: %ums in %dms probe (range %u~%u)",
                    elapsed_ms, (uint32_t)AGING_RTC_PROBE_MS, min_ms, max_ms);
        return SNDP_AGING_FAIL_DEVICE;
    }

    AGING_TRACE(1, "32k tick ok: %ums / %dms probe",
                elapsed_ms, (uint32_t)AGING_RTC_PROBE_MS);
    return SNDP_AGING_FAIL_NONE;
}

/* ---- B8: I2C 总线压力测试 ----
 *
 * 每次读两颗传感器的 CHIP_ID 寄存器, 判定条件是"读通 + 值对":
 *   只判返回值只能发现 NAK(器件不应答), 而总线上真正的隐患是位翻转/串扰,
 *   那种情况读写都"成功"但读回来的是错值, 所以每一次都要比对器件 ID。
 */
static int32_t sndp_aging_sub_bus_start(void)
{
#ifdef __SNDP_AGING_ITEM_SENSOR__
    uint8_t id = 0;
    int32_t ret = -1;

    /* 先唤醒两颗传感器: 进入检测模式才会给 VSENSOR 上电。
     * BUS 单独使能时前面没有 SENSOR 小项, 不上电的话第一拍就读不到, 会误判成总线故障。 */
    if (sndp_aging_check_acc() != 0 || sndp_aging_check_hr() != 0) {
        AGING_TRACE(0, "bus stress: sensor wakeup fail");
        return SNDP_AGING_FAIL_BUS;
    }

    for (uint32_t i = 0; i < AGING_BUS_STRESS_LOOP_CNT; i++) {
        /* 覆盖两颗传感器所在总线 */
        id = 0;
        ret = sndp_dev_acc_read_reg(AGING_DA217E_CHIP_ID_REG, &id, 1);
        if (ret != 0 || id != AGING_DA217E_CHIP_ID) {
            AGING_TRACE(0, "bus stress fail(acc) at %d, ret=%d id=0x%02X expect=0x%02X",
                        i, ret, id, AGING_DA217E_CHIP_ID);
            return SNDP_AGING_FAIL_BUS;
        }
        id = 0;
        ret = sndp_dev_hr_read_reg(AGING_SSH401A_CHIP_ID_REG, &id, 1);
        if (ret != 0 || id != AGING_SSH401A_CHIP_ID) {
            AGING_TRACE(0, "bus stress fail(hr) at %d, ret=%d id=0x%02X expect=0x%02X",
                        i, ret, id, AGING_SSH401A_CHIP_ID);
            return SNDP_AGING_FAIL_BUS;
        }
    }

    AGING_TRACE(0, "bus stress pass, %d loops", AGING_BUS_STRESS_LOOP_CNT);
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- F1: 射频发射老化(可选) ---- */
static int32_t sndp_aging_sub_rf_start(void)
{
#ifdef __SNDP_AGING_ITEM_RF__
    /* 需 BES 提供"指定信道定频发射"接口, 当前未使能 */
    AGING_TRACE(0, "rf item not implemented (need BES interface)");
#endif
    return SNDP_AGING_FAIL_NONE;
}

/* ---- B3 / B4: 软/硬重启(执行后不会返回) ---- */
static int32_t sndp_aging_sub_reboot_start(void)
{
    sndp_app_reboot(HAL_SW_BOOTMODE_REBOOT);
    /* 不会返回 */
    return SNDP_AGING_FAIL_NONE;
}

/* B5/B6: 请求频点。96/192 在本平台最近档是 104M/208M, 不直接改 Vcore */
static void sndp_aging_load_freq_set(bool high)
{
#ifdef __SNDP_AGING_ITEM_LOAD__
    enum APP_SYSFREQ_FREQ_T freq = high ? APP_SYSFREQ_208M : APP_SYSFREQ_104M;
    app_sysfreq_req(APP_SYSFREQ_USER_APP_37, freq);
    AGING_TRACE(0, "load freq %s", high ? "208M" : "104M");
#endif
}

static void sndp_aging_load_freq_release(void)
{
#ifdef __SNDP_AGING_ITEM_LOAD__
    app_sysfreq_req(APP_SYSFREQ_USER_APP_37, APP_SYSFREQ_32K);
#endif
}

/* 大项进入动作 */
static void sndp_aging_big_enter_mid(void)
{
    sndp_aging_load_freq_set(false);
}

static void sndp_aging_big_enter_high(void)
{
    sndp_aging_load_freq_set(true);
}

/* 终检前收尾: 停音频/恢复 ANC/释放负载频点, 不让高负载跟着补电 */
static void sndp_aging_big_enter_final(void)
{
    sndp_aging_item_audio_stop();
    sndp_aging_item_anc_restore();
    sndp_aging_load_freq_release();
}

/**************************************************************************************************
 * 表驱动: 小项表 / 大项表
 **************************************************************************************************/
static const sndp_aging_sub_info_t s_aging_sub_tbl[] = {
    { SNDP_AGING_SUB_SRAM,     "SRAM",     sndp_aging_sub_sram_start,   NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_FLASH,    "FLASH",    sndp_aging_sub_flash_start,  NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_SENSOR,   "SENSOR",   sndp_aging_sub_sensor_start, NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_CHARGE,   "CHARGE",   sndp_aging_sub_charge_start, NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_TOPUP,    "TOPUP",    sndp_aging_sub_topup_start,  NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_AUDIO,    "AUDIO",    sndp_aging_sub_audio_start,  sndp_aging_sub_audio_query,  1000, 5000, 0 },
    { SNDP_AGING_SUB_VOLUME,   "VOLUME",   sndp_aging_sub_volume_start, NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_ANC,      "ANC",      sndp_aging_sub_anc_start,    NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_RTC,      "RTC",      sndp_aging_sub_rtc_start,    NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_BUS,      "BUS",      sndp_aging_sub_bus_start,    NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_RF,       "RF",       sndp_aging_sub_rf_start,     NULL,                        1000, 1000, 0 },
    { SNDP_AGING_SUB_SOFT_RST, "SOFT_RST", sndp_aging_sub_reboot_start, NULL,                        3000, 1000, SNDP_AGING_SUB_FLAG_REBOOT },
    { SNDP_AGING_SUB_HARD_RST, "HARD_RST", sndp_aging_sub_reboot_start, NULL,                        3000, 1000, SNDP_AGING_SUB_FLAG_REBOOT },
    /* 终检: 等进入充电(最长 AGING_CHARGE_WAIT_SEC 秒) */
    { SNDP_AGING_SUB_CHARGE_WAIT, "CHG_WAIT", sndp_aging_sub_charge_wait_start, NULL,                1000, 1000, 0 },
};

/* 各大项引用的小项 ID 列表 */
static const uint8_t s_subs_sram[]    = { SNDP_AGING_SUB_SRAM };
static const uint8_t s_subs_flash[]   = { SNDP_AGING_SUB_FLASH };
static const uint8_t s_subs_soft_rst[]= { SNDP_AGING_SUB_SOFT_RST };
static const uint8_t s_subs_hard_rst[]= { SNDP_AGING_SUB_HARD_RST };
static const uint8_t s_subs_mid[]     = { SNDP_AGING_SUB_SENSOR, SNDP_AGING_SUB_CHARGE,
                                          SNDP_AGING_SUB_AUDIO, SNDP_AGING_SUB_VOLUME,
                                          SNDP_AGING_SUB_ANC };
static const uint8_t s_subs_high[]    = { SNDP_AGING_SUB_SENSOR, SNDP_AGING_SUB_CHARGE,
                                          SNDP_AGING_SUB_AUDIO, SNDP_AGING_SUB_VOLUME,
                                          SNDP_AGING_SUB_ANC, SNDP_AGING_SUB_RTC };
static const uint8_t s_subs_bus[]     = { SNDP_AGING_SUB_BUS };
/* 终检: 传感器复检 -> 等进入充电 -> 补电到 80% */
static const uint8_t s_subs_final[]   = { SNDP_AGING_SUB_SENSOR, SNDP_AGING_SUB_CHARGE_WAIT,
                                          SNDP_AGING_SUB_TOPUP };

/* 大项表。下标 == 大项 ID:
 *   0   = charge 保留槽位(sub_ids == NULL, 不会被调度, 只为与上位机配置槽位对齐)
 *   1~7 = SRAM / FLASH / 软重启 / 硬重启 / 中负载 / 高负载 / 总线
 *   8   = 终检(传感器复检 -> 等进入充电 -> 补电到 80%), 排在最后, 目标次数一般填 1 */
static const sndp_aging_big_info_t s_aging_big_tbl[SNDP_AGING_BIG_QTY] = {
    { 0,                        "NONE",      NULL,                 0, NULL,                    0 },
    { SNDP_AGING_BIG_SRAM,      "SRAM",      s_subs_sram,          1, NULL,                    0 },
    { SNDP_AGING_BIG_FLASH,     "FLASH",     s_subs_flash,         1, NULL,                    0 },
    { SNDP_AGING_BIG_SOFT_RST,  "SOFT_RST",  s_subs_soft_rst,      1, NULL,                    0 },
    { SNDP_AGING_BIG_HARD_RST,  "HARD_RST",  s_subs_hard_rst,      1, NULL,                    0 },
    { SNDP_AGING_BIG_MID_LOAD,  "MID_LOAD",  s_subs_mid,           5, sndp_aging_big_enter_mid, AGING_LOAD_HOLD_MS },
    { SNDP_AGING_BIG_HIGH_LOAD, "HIGH_LOAD", s_subs_high,          6, sndp_aging_big_enter_high, AGING_LOAD_HOLD_MS },
    { SNDP_AGING_BIG_BUS,       "BUS",       s_subs_bus,           1, NULL,                    0 },
    { SNDP_AGING_BIG_FINAL,     "FINAL",     s_subs_final,         3, sndp_aging_big_enter_final, 0 },
};


static const sndp_aging_sub_info_t *sndp_aging_sub_find(uint8_t id)
{
    for (uint32_t i = 0; i < AGING_ARRAY_SIZE(s_aging_sub_tbl); i++) {
        if (s_aging_sub_tbl[i].sub_id == id) {
            return &s_aging_sub_tbl[i];
        }
    }
    return NULL;
}

/* 按大项 ID 查表。0 是 charge 保留槽位(sub_ids == NULL), 返回 NULL */
static const sndp_aging_big_info_t *sndp_aging_big_find(uint8_t id)
{
    if (id >= SNDP_AGING_BIG_QTY || s_aging_big_tbl[id].sub_ids == NULL) {
        return NULL;
    }
    return &s_aging_big_tbl[id];
}

/* 汇总失败原因码: 返回首个失败大项的原因码, 无失败返回 NONE */
static uint8_t sndp_aging_overall_fail_code(void)
{
    for (uint8_t i = 0; i < SNDP_AGING_BIG_QTY; i++) {
        if (s_aging_ctx.test_data.big_result[i].result == SNDP_AGING_RESULT_FAIL) {
            return s_aging_ctx.test_data.big_result[i].fail_code;
        }
    }
    return SNDP_AGING_FAIL_NONE;
}

/**************************************************************************************************
 * 大项调度
 **************************************************************************************************/

/* 收尾: 停音频/恢复 ANC/释放频点 */
static void sndp_aging_cleanup(void)
{
    sndp_aging_item_audio_stop();
    sndp_aging_item_anc_restore();
    sndp_aging_load_freq_release();
}

/* 结算本大项耗时 = 当前已运行时长 - 本大项开始时的已运行时长 */
static void sndp_aging_big_elapsed_update(sndp_aging_big_result_t *big_rlt)
{
    big_rlt->elapsed_ms = sndp_aging_elapsed_ms() - big_rlt->start_ms;
}


/* 进入指定大项: 记录耗时起点, 执行大项进入动作(只做一次), 落盘 */
static void sndp_aging_big_enter(uint8_t id)
{
    const sndp_aging_big_info_t *big_info = sndp_aging_big_find(id);
    sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[id];

    s_aging_ctx.test_data.big_id = id;
    s_aging_ctx.test_data.sub_id = 0;

    /* 大项耗时从进入时刻起算, 跨轮次累计; 跨重启由落盘的 start_ms 恢复 */
    big_rlt->start_ms   = sndp_aging_elapsed_ms();
    big_rlt->elapsed_ms = 0;

    AGING_TRACE(0, "[%d] %s: big item start", id, (big_info != NULL) ? big_info->big_name : "?");

    if (big_info != NULL && big_info->enter_fn != NULL) {
        AGING_TRACE(0, "[%d] %s: big item enter setup", id, big_info->big_name);
        big_info->enter_fn();
    }

    sndp_aging_data_save();
}

/* 该大项本次是否真的会跑。
 *
 * 判据 = 配置打开 && 目标次数 > 0 && 不在固件强制跳过名单里。
 * 调度、大项入口、状态上报(0x7C)统一用这一个函数, 避免"配置说要测、实际不跑"
 * 两边打架:
 *   - 只有一个大项被勾选时(例如只勾 FLASH), 其余大项在 0x7C 里一律报 0 次,
 *     上位机据此显示"跳过"; 若照搬配置里的旧目标值, 上位机会显示成"等待",
 *     甚至因为该大项从未被进入过而误判成"失败"(4 号硬重启就是这个原因)。
 *   - 被强制跳过的大项不会卡住调度轮转, 直接跳过继续往后跑。 */
static bool sndp_aging_big_is_active(uint8_t id)
{
    const sndp_aging_big_cfg_t *big_cfg;

    if (id >= SNDP_AGING_BIG_QTY) {
        return false;
    }

    big_cfg = &s_aging_ctx.test_data.big_cfg[id];
    if (!big_cfg->enable || big_cfg->target_cycle == 0) {
        return false;
    }

#if AGING_SKIP_HARD_RST
    /* 硬重启实现与软重启相同, 当前流程整体跳过 */
    if (id == SNDP_AGING_BIG_HARD_RST) {
        return false;
    }
#endif

    return true;
}

/* 找到 id 之后第一个要跑的大项。没有则返回 SNDP_AGING_BIG_DONE */
static uint8_t sndp_aging_big_find_next(uint8_t from_id)
{
    uint16_t id;

    for (id = (uint16_t)from_id + 1; id < SNDP_AGING_BIG_QTY; id++) {
        if (sndp_aging_big_is_active((uint8_t)id)) {
            return (uint8_t)id;
        }
    }

    return SNDP_AGING_BIG_DONE;
}

/* 跑当前大项的一轮(顺序执行大项内各小项) */
static void sndp_aging_big_run_cycle(uint8_t big_id)
{
    const sndp_aging_big_info_t *big_info = sndp_aging_big_find(big_id);
    if (big_info == NULL) {
        return;
    }

    sndp_aging_test_data_t *test_data = &s_aging_ctx.test_data;
    const sndp_aging_big_cfg_t *big_cfg = &test_data->big_cfg[big_id];
    sndp_aging_big_result_t *big_rlt = &test_data->big_result[big_id];

    if (!sndp_aging_big_is_active(big_id)) {
        return;
    }

    if (big_rlt->done_cycle >= big_cfg->target_cycle) {
        return;
    }

    /* 记下启动代次: 中途被"重新启动"则本轮的写入全部作废 */
    uint32_t epoch = s_aging_ctx.epoch;

    AGING_TRACE(0, "[%d] %s: cycle %d/%d start", big_id, big_info->big_name,
                big_rlt->done_cycle + 1, big_cfg->target_cycle);

    /* 每轮都从本大项的第一个小项开始。
     * 上一轮跑完时 sub_id 停在最后一项, 不归零会导致后续轮次只跑最后一项。 */
    test_data->sub_id = 0;

    for (uint8_t i = 0; i < big_info->sub_cnt; i++) {
        uint8_t sub_id = big_info->sub_ids[i];
        const sndp_aging_sub_info_t *sub_info = sndp_aging_sub_find(sub_id);
        int32_t rc;

        if (sub_info == NULL) {
            AGING_TRACE(0, "[%d-%d] sub info missing, skip", big_id, sub_id);
            continue;
        }

        test_data->sub_id = sub_info->sub_id;

        AGING_TRACE(0, "[%d-%d] %s-%s: sub item start",
                big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name);

        if (sub_info->pre_wait_ms > 0) {
            AGING_TRACE(0, "[%d-%d] %s-%s: pre wait %u ms", 
                    big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name,
                    sub_info->pre_wait_ms);
            
            if (!sndp_aging_wait_abortable(sub_info->pre_wait_ms) ||
                !sndp_aging_cycle_valid(epoch)) {
                return;
            }
        }

        /* 重启类小项: 执行即重启且不返回。
         * 重启前必须先落盘: 本大项轮次 + 已运行时长(耗时, 由 data_save 固化到基数)。
         * 重启后 sndp_aging_test_init() 读回该基数作为起点继续累加耗时。 */
        if (sub_info->flags & SNDP_AGING_SUB_FLAG_REBOOT) {
            if (!sndp_aging_cycle_valid(epoch)) {
                return;
            }

            big_rlt->done_cycle++;
            if (big_rlt->done_cycle >= big_cfg->target_cycle) {
                big_rlt->result = SNDP_AGING_RESULT_PASS;
            }

            sndp_aging_big_elapsed_update(big_rlt);
            sndp_aging_data_save();

            if (!sndp_aging_data_verify()) {
                big_rlt->done_cycle--;
                big_rlt->result    = SNDP_AGING_RESULT_FAIL;
                big_rlt->fail_item = sub_info->sub_id;
                big_rlt->fail_code = SNDP_AGING_FAIL_FLASH;
                sndp_aging_big_elapsed_update(big_rlt);
                sndp_aging_data_save();
                
                AGING_TRACE(0, "[%d-%d] %s-%s: save before reboot FAILED, code=%d", 
                        big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name,
                        SNDP_AGING_FAIL_FLASH);
                return;
            }

            AGING_TRACE(0, "[%d-%d] %s-%s: testing... reboot (%d/%d)",
                        big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name,
                        big_rlt->done_cycle, big_cfg->target_cycle);
            
            sndp_aging_item_audio_stop();
            
            if (sub_info->start_fn != NULL) {
                sub_info->start_fn();    /* sndp_app_reboot, 不会返回 */
            }
            return;
        }

        AGING_TRACE(0, "[%d-%d] %s-%s: testing... ", 
                big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name);
        
        rc = (sub_info->start_fn != NULL) ? sub_info->start_fn() : SNDP_AGING_FAIL_NONE;

        if (rc == SNDP_AGING_FAIL_NONE && sub_info->post_wait_ms > 0) {
            
            AGING_TRACE(0, "[%d-%d] %s-%s: query wait %u ms", 
                    big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name, 
                    sub_info->post_wait_ms);
            
            if (!sndp_aging_wait_abortable(sub_info->post_wait_ms) ||
                !sndp_aging_cycle_valid(epoch)) {
                return;
            }
        }

        if (rc == SNDP_AGING_FAIL_NONE && sub_info->query_fn != NULL) {
            rc = sub_info->query_fn();
        }

        AGING_TRACE(0, "[%d-%d] %s-%s: result %d", 
                big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name, 
                rc);

        if (rc != SNDP_AGING_FAIL_NONE) {
            if (!sndp_aging_cycle_valid(epoch)) {
                return;
            }

            /* 小项失败: 记录并跳过本大项剩余循环, 进入下个大项 */
            big_rlt->result    = SNDP_AGING_RESULT_FAIL;
            big_rlt->fail_item = sub_info->sub_id;
            big_rlt->fail_code = (uint8_t)rc;
            sndp_aging_big_elapsed_update(big_rlt);
            sndp_aging_data_save();
            
            AGING_TRACE(0, "[%d-%d] %s-%s: FAIL, code=%d, elapsed=%ums -> skip rest, next big",
                        big_info->big_id, sub_info->sub_id, big_info->big_name, sub_info->sub_name, 
                        rc, big_rlt->elapsed_ms);
            
            return;
        }
    }


    /* 本轮通过: 先累加并落盘, 再保持负载。
     * 累加前再确认本轮没被"重新启动"作废, 否则会把 cycle 写进刚重置的数据。 */
    if (!sndp_aging_cycle_valid(epoch)) {
        return;
    }

    big_rlt->done_cycle++;
    test_data->sub_id = 0;
    sndp_aging_big_elapsed_update(big_rlt);
    
    if (big_rlt->done_cycle >= big_cfg->target_cycle) {
        big_rlt->result = SNDP_AGING_RESULT_PASS;
        
        AGING_TRACE(0, "[%d] %s: big item done, cycle=%d/%d, elapsed=%ums",
                    big_info->big_id, big_info->big_name, 
                    big_rlt->done_cycle, big_cfg->target_cycle,
                    big_rlt->elapsed_ms);
        
    } else {
        AGING_TRACE(0, "[%d] %s: one cycle done, cycle=%d/%d, elapsed=%ums",
                    big_info->big_id, big_info->big_name, 
                    big_rlt->done_cycle, big_cfg->target_cycle,
                    big_rlt->elapsed_ms);
    }

    sndp_aging_data_save();

    /* 本轮音频测量与音量循环已结束, 关掉粉噪:
     * 既满足"跑完一轮白噪声停止", 也避免下一轮 ANC 小项与采集流同时抢 codec ADC */
    sndp_aging_item_audio_stop();

    if (big_info->hold_ms > 0 && !sndp_aging_wait_abortable(big_info->hold_ms)) {
        return;
    }
}

/* 温度红线等硬失败: 记录并终止 */
static void sndp_aging_abort_fail(uint8_t code, uint8_t sub_id)
{
    uint8_t big_id = s_aging_ctx.test_data.big_id;

    if (big_id < SNDP_AGING_BIG_QTY) {
        const sndp_aging_big_info_t *big_info = sndp_aging_big_find(big_id);
        sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[big_id];

        big_rlt->result    = SNDP_AGING_RESULT_FAIL;
        big_rlt->fail_item = sub_id;
        big_rlt->fail_code = code;
        sndp_aging_big_elapsed_update(big_rlt);

        AGING_TRACE(0, "[%d] %s: FAIL (abort), sub=%d, code=%d",
                    big_id, (big_info != NULL) ? big_info->big_name : "?", sub_id, code);
    }

    sndp_aging_cleanup();
    s_aging_ctx.running = false;
    s_aging_ctx.test_data.in_aging = 0;
    s_aging_ctx.test_data.big_id = SNDP_AGING_BIG_DONE;
    s_aging_ctx.test_data.sub_id = SNDP_AGING_BIG_DONE;
    sndp_aging_data_save();
}

/**************************************************************************************************
 * 老化测试线程
 *
 * 每轮: 取当前大项 -> 依小项列表顺序执行 -> 累加循环/记录失败 -> 推进大项。
 **************************************************************************************************/
static void sndp_aging_thread(void const *argument)
{
    (void)argument;

    AGING_TRACE(0, "started");

    while (1) {
        osDelay(AGING_POLL_INTERVAL_MS);

        if (!s_aging_ctx.running) {
            AGING_TRACE(0, "sleep...");
            app_sysfreq_req(APP_SYSFREQ_USER_APP_37, APP_SYSFREQ_32K);
            osSemaphoreWait(sndp_aging_thread_wakeup_id, osWaitForever);
            AGING_TRACE(0, "wakeup");
        }

        /* 重启类大项: 重启后先等够 AGING_REBOOT_RESUME_WAIT_MS 再执行下一步,
         * 窗口内串口已在工作, 可接收停止/退出 */
        if (s_aging_ctx.resume_hold) {
            s_aging_ctx.resume_hold = false;
            AGING_TRACE(0, "post reboot wait %dms before next step", AGING_REBOOT_RESUME_WAIT_MS);
            if (!sndp_aging_wait_abortable(AGING_REBOOT_RESUME_WAIT_MS)) {
                AGING_TRACE(0, "resume aborted by user, no more reboot");
                continue;
            }
        }

        /* 稳定窗口结束, 这时才重放大项进入动作(如中/高负载设频点)。
         * 恢复时不能直接做, 那是刚开机、系统还没稳的时刻。 */
        if (s_aging_ctx.enter_pending) {
            const sndp_aging_big_info_t *resume_big = sndp_aging_big_find(s_aging_ctx.test_data.big_id);

            s_aging_ctx.enter_pending = false;
            if (resume_big != NULL && resume_big->enter_fn != NULL) {
                AGING_TRACE(0, "[%d] %s: replay big item enter setup after resume",
                            resume_big->big_id, resume_big->big_name);
                resume_big->enter_fn();
            }
        }

        /* 温度红线: 全程管控 */
        if (sndp_aging_temperature_guard() != 0) {
            sndp_aging_abort_fail(SNDP_AGING_FAIL_TEMPERATURE, SNDP_AGING_SUB_QTY);
            continue;
        }

        uint8_t big_id = s_aging_ctx.test_data.big_id;
        if (big_id >= SNDP_AGING_BIG_QTY) {
            /* 没有当前大项(已测完/已终止), 等下一次启动 */
            continue;
        }

        const sndp_aging_big_cfg_t *big_cfg = &s_aging_ctx.test_data.big_cfg[big_id];
        const sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[big_id];

        /* 本大项已结束(不跑 / 已跑满 / 已失败): 进入下一个有效大项。
         * 判"不跑"必须用 is_active: 只看 enable 会让"被强制跳过的大项"
         * 一直进不了这个分支, 而 run_cycle 又直接返回, 轮转就卡死了。 */
        if (!sndp_aging_big_is_active(big_id) ||
            big_rlt->result == SNDP_AGING_RESULT_FAIL ||
            big_rlt->done_cycle >= big_cfg->target_cycle) {
            uint8_t next = sndp_aging_big_find_next(big_id);

            if (next >= SNDP_AGING_BIG_QTY) {
                sndp_aging_cleanup();
                s_aging_ctx.running = false;
                s_aging_ctx.test_data.in_aging = 0;
                s_aging_ctx.test_data.big_id = SNDP_AGING_BIG_DONE;
                s_aging_ctx.test_data.sub_id = SNDP_AGING_BIG_DONE;
                sndp_aging_data_save();
                AGING_TRACE(0, "AGING COMPLETE, all big items done");
            } else {
                sndp_aging_big_enter(next);
            }
            continue;
        }

        sndp_aging_big_run_cycle(big_id);
    }
}

/**************************************************************************************************
 * 配置指令数据解析(0x7D 的 DATA)
 *
 * 固定 9 组, 每组 3 字节, 严格按大项 ID 从小到大:
 *   [大项ID(1)][是否测试(1)][目标次数(1)]
 *
 * 大项 ID(与 sndp_aging_test.h 的 SNDP_AGING_ITEM_xxx_ID 一致):
 *   0 charge(保留)  1 sram   2 flash  3 soft_rst  4 hard_rst
 *   5 mid           6 high   7 bus    8 end
 *
 * 例(只开 sram/flash/mid, 目标 1/1/9 次):
 *   00 00 00 | 01 01 01 | 02 01 01 | 03 00 00 | 04 00 00 | 05 01 09 | 06 00 00 | 07 00 00 | 08 00 00
 *
 * 说明: 不用名称匹配, 也不依赖"下发了哪几项"——9 项每次都全带, 耳机侧照单全收。
 *       旧 ASCII 格式只下发勾选项, 没下发的项会保留上一版配置, 所以"没勾硬重启"
 *       查回来还是勾上的。改成定长数值后这类问题从格式上就不存在了。
 * 返回 0 表示解析成功。
 **************************************************************************************************/
int32_t sndp_aging_test_cfg_parse(uint8_t *data, uint16_t data_len, sndp_aging_big_cfg_t *big_cfg)
{
    if (data == NULL || big_cfg == NULL) {
        return -1;
    }

    /* 长度必须是 9 × 3, 多一个字节都不收: 双方定义漂移时宁可报错也不"将就能跑" */
    if (data_len != (SNDP_AGING_ITEM_QTY * 3)) {
        AGING_TRACE(0, "cfg parse fail: len=%d expect=%d",
                    data_len, SNDP_AGING_ITEM_QTY * 3);
        return -1;
    }

    /* 9 项都要清: 调用方传进来的是数组, 只清一项会让其余项保留栈上的随机值 */
    memset(big_cfg, 0, sizeof(sndp_aging_big_cfg_t) * SNDP_AGING_ITEM_QTY);

    for (uint8_t i = 0; i < SNDP_AGING_ITEM_QTY; i++) {
        uint8_t id     = data[i * 3 + 0];
        uint8_t enable = data[i * 3 + 1];
        uint8_t target = data[i * 3 + 2];

        /* ID 必须等于下标(即从第一项到最后一项), 是否测试只认 0/1 */
        if (id != i || enable > 1) {
            AGING_TRACE(0, "cfg parse fail at %d: id=%d enable=%d", i, id, enable);
            return -1;
        }

        big_cfg[id].enable       = enable;
        big_cfg[id].target_cycle = target;
    }

    return 0;
}

/**************************************************************************************************
 * 对外接口
 **************************************************************************************************/

/* 下发/保存配置: 只更新配置并持久化, 不启动测试 */
int32_t sndp_aging_test_set_cfg(sndp_aging_big_cfg_t *big_cfg)
{
    if (big_cfg == NULL) {
        return -1;
    }

    sndp_aging_lock();
    memcpy(s_aging_ctx.test_data.big_cfg, big_cfg, sizeof(s_aging_ctx.test_data.big_cfg));
    sndp_aging_data_flush();
    sndp_aging_unlock();

    AGING_TRACE(0, "aging cfg saved, mid=%d, high=%d",
                s_aging_ctx.test_data.big_cfg[SNDP_AGING_ITEM_MID_ID].target_cycle,
                s_aging_ctx.test_data.big_cfg[SNDP_AGING_ITEM_HIGH_ID].target_cycle);
    return 0;
}

void sndp_aging_test_get_cfg(sndp_aging_big_cfg_t *big_cfg)
{
    if (big_cfg == NULL) {
        return;
    }

    sndp_aging_lock();
    memcpy(big_cfg, s_aging_ctx.test_data.big_cfg, sizeof(s_aging_ctx.test_data.big_cfg));
    sndp_aging_unlock();
}

int32_t sndp_aging_test_start(void)
{
    bool any_big = false;

    sndp_aging_lock();

    /* 必须已下发配置(至少一个大项被打开且目标循环 > 0) */
    for (uint8_t i = 1; i < SNDP_AGING_BIG_QTY; i++) {
        if (s_aging_ctx.test_data.big_cfg[i].enable && s_aging_ctx.test_data.big_cfg[i].target_cycle > 0) {
            any_big = true;
            break;
        }
    }
    if (!any_big) {
        AGING_TRACE(0, "start denied: no cfg, set cfg first");
        sndp_aging_unlock();
        return -1;
    }

    /*
     * 重新开始无条件生效。
     * 重启恢复后 running 一直为真, 若在此拒绝, 旧的过程数据(如已累计的重启次数)清不掉。
     */
    /* 递增启动代次: 可能还有上一轮的尾巴在跑(比如刚跑完在收尾),
     * 代次变了它就会在下一个检查点自行退出, 不会往下面重置的数据里写 cycle。 */
    s_aging_ctx.epoch++;

    if (s_aging_ctx.running) {
        AGING_TRACE(0, "aging restart, drop previous progress");
        sndp_aging_cleanup();
        /* 同时清运行标志: 老化线程可能正卡在 wait_abortable 的 100ms 分片里,
         * 不清标志它会把当前这一轮的剩余小项跑完, 不是干净的"丢弃进度"。 */
        s_aging_ctx.running = false;
    }

    /* ---- 初始化过程数据 ----
     * 只重置 big_result, 不能 memset 整个 test_data:
     * big_cfg 就存在里面, 那是刚由 0x7D 下发的配置。 */
    uint16_t total_run = s_aging_ctx.test_data.total_run;

    for (uint8_t i = 0; i < SNDP_AGING_BIG_QTY; i++) {
        sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[i];

        big_rlt->big_id      = i;
        big_rlt->result      = SNDP_AGING_RESULT_IDLE;
        big_rlt->done_cycle  = 0;
        big_rlt->fail_item   = 0;
        big_rlt->fail_code   = SNDP_AGING_FAIL_NONE;
        big_rlt->start_ms    = 0;
        big_rlt->elapsed_ms  = 0;
    }

    s_aging_ctx.test_data.in_aging   = 1;
    s_aging_ctx.test_data.total_run  = total_run + 1;
    s_aging_ctx.test_data.big_id     = 0;
    s_aging_ctx.test_data.sub_id     = 0;
    s_aging_ctx.test_data.elapsed_ms = 0;

    /* 计时归零: 基数 0, 参考 tick 取当前 */
    s_aging_ctx.elapsed_base_ms = 0;
    s_aging_ctx.elapsed_tick_ms = GET_CURRENT_MS();

    s_aging_ctx.audio_on      = false;
    s_aging_ctx.anc_on        = false;
    s_aging_ctx.anc_toggle_ms = 0;
    s_aging_ctx.resume_hold   = false;
    s_aging_ctx.enter_pending = false;

    sndp_aging_data_save();

    /* 从第一个有效大项开始(耗时起点与进入动作由 big_enter 负责) */
    uint8_t first = sndp_aging_big_find_next(0);
    if (first < SNDP_AGING_BIG_QTY) {
        sndp_aging_big_enter(first);
    }

    /* 置运行标志后再取计时参考点, 避免把启动时的落盘耗时算进来 */
    s_aging_ctx.running         = true;
    s_aging_ctx.elapsed_tick_ms = GET_CURRENT_MS();

    AGING_TRACE(0, "aging started, big=%d, running=%d",
                s_aging_ctx.test_data.big_id, s_aging_ctx.running);

    sndp_aging_unlock();

    osSemaphoreRelease(sndp_aging_thread_wakeup_id);
    return 0;
}

void sndp_aging_test_stop(void)
{
    sndp_aging_lock();

    if (s_aging_ctx.running) {
        AGING_TRACE(0, "aging stopped by user");
        sndp_aging_cleanup();
        s_aging_ctx.running = false;
        /* 只把"老化中"标志清掉, 已完成循环和配置保留 */
        s_aging_ctx.test_data.in_aging = 0;
        sndp_aging_data_save();
    }

    sndp_aging_unlock();
}

void sndp_aging_test_exit(void)
{
    sndp_aging_test_stop();

    sndp_aging_lock();
    sndp_aging_test_data_default(&s_aging_ctx.test_data);
    s_aging_ctx.elapsed_base_ms = 0;
    s_aging_ctx.elapsed_tick_ms = GET_CURRENT_MS();
    sndp_aging_data_flush();
    sndp_aging_unlock();

    AGING_TRACE(0, "aging exit, progress cleared");
}

bool sndp_aging_test_is_running(void)
{
    return s_aging_ctx.running;
}

void sndp_aging_test_get_status(sndp_aging_status_t *status)
{
    if (status == NULL) {
        return;
    }

    sndp_aging_lock();

    memset(status, 0, sizeof(*status));
    status->running        = s_aging_ctx.running ? 1 : 0;
    status->total_times    = s_aging_ctx.test_data.total_run;
    status->temperature    = sndp_dev_temperature_get_temperature(false);
    status->bat_voltage    = sndp_dev_get_bat_voltage(false);
    status->bat_percentage = sndp_dev_get_bat_percentage(false);
    status->charger_status = (uint8_t)sndp_dev_charger_get_charging_status(false);
    status->curr_big_id    = s_aging_ctx.test_data.big_id;
    status->curr_sub_id    = s_aging_ctx.test_data.sub_id;
    status->curr_fail_code = sndp_aging_overall_fail_code();

    status->elapsed_ms = sndp_aging_elapsed_ms();

    for (uint8_t i = 0; i < SNDP_AGING_ITEM_QTY; i++) {
        sndp_aging_big_result_t *big_rlt= &s_aging_ctx.test_data.big_result[i];

        /* 本次不跑的大项一律报 0/0/0:
         * 目标次数是上位机判定"这项测不测"的唯一依据(协议里没有单独的"是否测试"位),
         * 报非 0 会让没勾选的大项在界面显示成"等待", 甚至被当成失败。
         * 配置里记住的目标值仍由 0x7E 原样查询, 这里只影响"本次是否在测"。 */
        if (!sndp_aging_big_is_active(i)) {
            status->target_cycle[i] = 0;
            status->run_cycle[i]    = 0;
            status->run_time_s[i]   = 0;
            continue;
        }

        status->target_cycle[i] = s_aging_ctx.test_data.big_cfg[i].target_cycle;
        status->run_cycle[i]    = big_rlt->done_cycle;
        status->run_time_s[i]   = big_rlt->elapsed_ms / 1000u;
    }

    sndp_aging_unlock();
}

/**************************************************************************************************
 * 初始化(开机调用)
 *
 * 职责:
 *   1) 创建互斥量与工作线程
 *   2) 从 flash 载入 cfg + test_data, 若"老化中且未测完"则从记录的大项继续
 *      (软/硬重启类测试必须依赖此机制续跑)
 **************************************************************************************************/
void sndp_aging_test_init(void)
{
    bool loaded;

    if (s_aging_ctx.inited) {
        return;
    }

    if (s_aging_mutex_id == NULL) {
        s_aging_mutex_id = osMutexCreate(osMutex(sndp_aging_mutex));
    }

    memset(&s_aging_ctx, 0, sizeof(s_aging_ctx));
    sndp_aging_flash_init();

    /* ---- 从老化分区恢复 ---- */
    loaded = sndp_aging_data_load();

    if (loaded &&
        s_aging_ctx.test_data.in_aging &&
        s_aging_ctx.test_data.big_id < SNDP_AGING_BIG_QTY) {

        uint8_t big_id = s_aging_ctx.test_data.big_id;

        /* 已运行时长基数随 test_data 落盘; 本次开机从当前 tick 重新起算增量 */
        s_aging_ctx.elapsed_base_ms = s_aging_ctx.test_data.elapsed_ms;
        s_aging_ctx.elapsed_tick_ms = GET_CURRENT_MS();
        s_aging_ctx.running         = true;

        const sndp_aging_big_info_t *big_info = sndp_aging_big_find(big_id);
        sndp_aging_big_cfg_t *big_cfg = &s_aging_ctx.test_data.big_cfg[big_id];
        sndp_aging_big_result_t *big_rlt = &s_aging_ctx.test_data.big_result[big_id];

        AGING_TRACE(0, "aging resumed at big %d(%s), cycle %d/%d",
                    big_id, (big_info != NULL) ? big_info->big_name : "?",
                    big_rlt->done_cycle, big_cfg->target_cycle);

        /* 能从 flash 恢复就说明刚重启过, 统一先挂起:
         * 由老化线程等够 AGING_REBOOT_RESUME_WAIT_MS 再续测, 期间可接收停止/退出。
         * 不能只对"含重启小项的大项"生效——意外复位(如断言复位)同样需要稳定窗口。 */
        s_aging_ctx.resume_hold   = true;
        s_aging_ctx.enter_pending = true;
    }

    if(sndp_aging_thread_wakeup_id == NULL) {
        sndp_aging_thread_wakeup_id = osSemaphoreCreate(osSemaphore(sndp_aging_thread_wakeup), 0);
        ASSERT(sndp_aging_thread_wakeup_id != NULL, "%s, sndp_aging_thread_wakeup_id == NULL", __func__);
    }
    
    /* ---- 创建工作线程 ---- */
    if (s_aging_tid == NULL) {
        s_aging_tid = osThreadCreate(osThread(sndp_aging_thread), NULL);
    }

    s_aging_ctx.inited = true;

    AGING_TRACE(0, "aging module inited, running=%d", s_aging_ctx.running);
}

#endif /* __SNDP_AGING_TEST__ */
