#if defined(__SNDP_HEART_RATE_MGR__)
#include "stdio.h"
#include "cmsis_os.h"
#include "string.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "apps.h"
#include "nvrecord_env.h"
#include "app_media_player.h"
#include "app_audio.h"
#include "cqueue.h"
#include "app_utils.h"
#include "audio_dump.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_heart_rate.h"

#if defined(__SNDP_HR_ALGO__)
#include "sleepsense.h"
#endif

#if defined(__SNDP_GSENSOR_SUPPORT__)
#include "sndp_hal_acc.h"
#endif

#if defined(__SNDP_HRSENSOR_SUPPORT__)
#include "sndp_hal_hr.h"
#endif
#include "sndp_comm_cmd.h"
#include "sndp_interact_app.h"
#include "sndp_sleep_role_switch.h"

/**************************************************************************************************
* 统一任务轮询线程(MCU super-loop风格):
* 1、创建统一任务轮询线程 sndp_hr_sleep_app_process_thread。
* 2、PPG/ACC传感器GPIO IRQ、proximity周期定时器通过osSignalSet发送任务信号唤醒线程。
* 3、线程每次被唤醒后按序轮询5个task: ppg_task/acc_task/heartrate_task/sleep_analysis_task/proximity_task。
* 4、心率/睡眠RAW Data队列: 传感器回调(线程内同步执行)Push数据, task3检测数据量足够后送入算法计算。
* 5、各task是否执行由hr_ctx运行状态flag与任务信号位共同决定。
**************************************************************************************************/



/**************************************************************************************************
* Constant
**************************************************************************************************/

#define HR_PPG_CHNNEL_NUM                   (1)
#define HR_PPG_SECOND_1CH_SAMPLES           (64)
#define HR_PPG_SECOND_ALLCH_SAMPLES         (HR_PPG_SECOND_1CH_SAMPLES * HR_PPG_CHNNEL_NUM)

#define HR_ACC_CHNNEL_NUM                   (3)
#define HR_ACC_SECOND_1CH_SAMPLES           (125)
#define HR_ACC_SECOND_ALLCH_SAMPLES         (HR_ACC_SECOND_1CH_SAMPLES * HR_ACC_CHNNEL_NUM)

#define HR_DEV_CHNNEL_NUM                   (1)
#define HR_DEV_SECOND_1CH_SAMPLES           (5)
#define HR_DEV_SECOND_ALLCH_SAMPLES         (HR_DEV_SECOND_1CH_SAMPLES * HR_ACC_CHNNEL_NUM)

#define SLEEP_CALC_SECONDS                  (30)
#define SLEEP_ACC_DATA_SAMPLES              (HR_ACC_SECOND_ALLCH_SAMPLES * SLEEP_CALC_SECONDS)
#define SLEEP_DEV_DATA_SAMPLES              (HR_DEV_SECOND_ALLCH_SAMPLES * SLEEP_CALC_SECONDS)


//#define __SNDP_HR_PRINT_ALGO_EXEC_TIME__

/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool hr_running;
    bool sleep_running;
    bool sleep_tracking;
    uint8_t sampling_rate;
    uint8_t dump_state[DUMP_STATE_MAX]; /* 0:dump close, 1:dump open */
    int32_t sleep_control;

    uint32_t ppg_user_flag;
    uint32_t acc_user_flag;
    bool delay_10S_start;
    bool hr_acc_suspended;
    bool hr_ppg_suspended;
} sndp_hr_ctx_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/




/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hr_ctx_s hr_ctx;

static CQueue ppg_raw_data_queue;
static int32_t ppg_raw_data_queue_buf[HR_PPG_SECOND_ALLCH_SAMPLES*2];

static osMutexId ppg_raw_data_queue_mutex_id = NULL;
osMutexDef(ppg_raw_data_queue_mutex);

static CQueue acc_raw_data_queue;
static int16_t acc_raw_data_queue_buf[HR_ACC_SECOND_ALLCH_SAMPLES*2];

static osMutexId acc_raw_data_queue_mutex_id = NULL;
osMutexDef(acc_raw_data_queue_mutex);


#define HR_PROCESS_THREAD_STACK_SIZE 				(1024*10)
static void sndp_hr_sleep_app_process_thread(void const *argument);
osThreadDef(sndp_hr_sleep_app_process_thread, osPriorityAboveNormal, 1, HR_PROCESS_THREAD_STACK_SIZE, "hr_process_thread");
osThreadId hr_process_thread_tid = NULL;

/*************** 统一任务轮询线程的任务信号位(osSignal: 置位即唤醒, 线程wait返回时自动清除) ***************/
#define SENSOR_TASK_SIGNAL_PPG_FIFO             (1 << 0)  /* ssh401a GPIO IRQ: PPG FIFO就绪 */
#define SENSOR_TASK_SIGNAL_ACC_FIFO             (1 << 1)  /* da217e GPIO IRQ: ACC FIFO就绪 */
#define SENSOR_TASK_SIGNAL_PROXIMITY_TICK       (1 << 2)  /* proximity周期定时器tick */
#define SENSOR_TASK_SIGNAL_SLEEP_TRACK          (1 << 3)  /* 手机端睡眠数据到达 */
#define SENSOR_TASK_SIGNAL_HEARTRATE            (1 << 4)  /* PPG队列凑满1秒数据: 触发心率算法 */
#define SENSOR_TASK_SIGNAL_WAKEUP               (1 << 5)  /* 纯唤醒: 使线程重估看门狗超时, 不触发任何task */

#define SENSOR_TASK_ACC_WATCHDOG_MS             (250)     /* ACC FIFO看门狗周期(沿用原da217e线程行为) */
#define SNDP_HR_PROXIMITY_TICK_PERIOD_MS        (1000)    /* proximity周期上报tick */

static void sndp_hr_proximity_tick_timer_handler(void const *param);
osTimerDef(SNDP_HR_PROXIMITY_TICK_TIMER, sndp_hr_proximity_tick_timer_handler);
static osTimerId sndp_hr_proximity_tick_timer_id = NULL;

static void sndp_heartrate_algo_task(void);

#if defined(__SNDP_HR_ALGO__)
POSSIBLY_UNUSED static struct Dump debug_dump;
#endif

POSSIBLY_UNUSED static int32_t hr_ppg_raw_data[HR_PPG_SECOND_ALLCH_SAMPLES];
POSSIBLY_UNUSED static int16_t hr_acc_raw_data[HR_ACC_SECOND_ALLCH_SAMPLES];
POSSIBLY_UNUSED static int8_t hr_dev_state[HR_DEV_SECOND_ALLCH_SAMPLES];
POSSIBLY_UNUSED static uint16_t hr_measure_time = 0;

POSSIBLY_UNUSED static int16_t sleep_app_accel[90];
POSSIBLY_UNUSED static uint8_t sleep_screen_status[30];
POSSIBLY_UNUSED static uint8_t sleep_sound_state;
POSSIBLY_UNUSED static int8_t sleep_stage[40];
POSSIBLY_UNUSED static int8_t sleep_position;
POSSIBLY_UNUSED static int8_t sound_control;
POSSIBLY_UNUSED static int16_t result_code;

static void sndp_sleep_analysis(void);
static bool sndp_is_notifi_hr_enabled(void);
/**************************************************************************************************
* Function
**************************************************************************************************/
uint8_t sndp_hr_mearsuring_get_sampling_rate(void)
{
    return hr_ctx.sampling_rate;
}

void sndp_hr_mearsuring_set_sampling_rate(uint8_t sampling_rate)
{
    hr_ctx.sampling_rate = sampling_rate;
}

uint8_t sndp_hr_mearsuring_get_dump_state(dump_state_e state)
{
    return hr_ctx.dump_state[state];
}

void sndp_hr_mearsuring_set_dump_state(dump_state_e dump_state, uint8_t onoff)
{
    hr_ctx.dump_state[dump_state] = onoff;
}

void sndp_hr_set_sleep_control(int32_t sleep_control)
{
    hr_ctx.sleep_control = sleep_control;
}

int32_t sndp_hr_get_sleep_control(void)
{
    return hr_ctx.sleep_control;
}

uint8_t sndp_hr_running_state(void)
{
    return hr_ctx.hr_running;
}

bool sndp_hr_is_reading_ppg_enabled(void)
{
    // SNDP_TRACE(0, "ppg_user_flag=%d", hr_ctx.ppg_user_flag);
    return hr_ctx.ppg_user_flag ? true : false;   
}

bool sndp_hr_is_reading_acc_enabled(void)
{
    // SNDP_TRACE(0, "acc_user_flag=%d", hr_ctx.acc_user_flag);
    return hr_ctx.acc_user_flag ? true : false;   
}

static bool sndp_hr_acc_is_suspend(void)
{
    return hr_ctx.hr_acc_suspended;
}

static bool sndp_hr_ppg_is_suspend(void)
{
    return hr_ctx.hr_ppg_suspended;
}

void sndp_hr_user_ppg_flag_set(uint32_t *user_flag, sensor_ppg_op_user_e user)
{
	*user_flag |= user; 
}

void sndp_hr_user_ppg_flag_clear(uint32_t *user_flag, sensor_ppg_op_user_e user)
{
	*user_flag &= ~user; 
}

void sndp_hr_user_acc_flag_set(uint32_t *user_flag, sensor_acc_op_user_e user)
{
	*user_flag |= user; 
}

void sndp_hr_user_acc_flag_clear(uint32_t *user_flag, sensor_acc_op_user_e user)
{
	*user_flag &= ~user; 
}

static int sndp_hr_ppg_raw_data_queue_push_data(int32_t *item, int cnt)
{
    int ret;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&ppg_raw_data_queue, (CQItemType *)item, sizeof(int32_t) * cnt);
    osMutexRelease(ppg_raw_data_queue_mutex_id);
    return ret;
}

POSSIBLY_UNUSED static int sndp_hr_ppg_raw_data_queue_pop_data(int32_t *item, int cnt)
{
    int ret;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&ppg_raw_data_queue, (CQItemType *)item, sizeof(int32_t) * cnt);
    osMutexRelease(ppg_raw_data_queue_mutex_id);

    return ret;
}

static int sndp_hr_ppg_raw_data_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&ppg_raw_data_queue);
    osMutexRelease(ppg_raw_data_queue_mutex_id);

    queue_len = queue_len / sizeof(int32_t);
    return queue_len;
}

static void sndp_hr_ppg_raw_data_queue_reset(void)
{
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ResetCQueue(&ppg_raw_data_queue);
    osMutexRelease(ppg_raw_data_queue_mutex_id);
}


static int sndp_hr_acc_raw_data_queue_push_data(int16_t *item, int cnt)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&acc_raw_data_queue, (CQItemType *)item, sizeof(int16_t) * cnt);
    osMutexRelease(acc_raw_data_queue_mutex_id);
    return ret;
}

POSSIBLY_UNUSED static int sndp_hr_acc_raw_data_queue_pop_data(int16_t *item, int cnt)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&acc_raw_data_queue, (CQItemType *)item, sizeof(int16_t) * cnt);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    return ret;
}

POSSIBLY_UNUSED static int sndp_hr_acc_raw_data_queue_get_len(void)
{
    uint16_t queue_len;

    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&acc_raw_data_queue);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    queue_len = queue_len / sizeof(int16_t);
    return queue_len;
}

static void sndp_hr_acc_raw_data_queue_reset(void)
{
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ResetCQueue(&acc_raw_data_queue);
    osMutexRelease(acc_raw_data_queue_mutex_id);
}
/**
 * @brief   PPG传感器GPIO IRQ上下文调用: 发送PPG FIFO就绪信号唤醒统一任务线程
 */
void sndp_hr_notify_ppg_fifo_ready(void)
{
    // SNDP_TRACE(0, "PPG FIFO ready");
    if(hr_process_thread_tid) {
        osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_PPG_FIFO);
    }
}

/**
 * @brief   ACC传感器GPIO IRQ上下文调用: 发送ACC FIFO就绪信号唤醒统一任务线程
 */
void sndp_hr_notify_acc_fifo_ready(void)
{
    // SNDP_TRACE(0, "ACC FIFO ready");
    if(hr_process_thread_tid) {
        osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_ACC_FIFO);
    }
}

static void sndp_hr_proximity_tick_timer_handler(void const *param)
{
    if(hr_process_thread_tid) {
        osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_PROXIMITY_TICK);
    }
}

/**
 * @brief   使能/停止proximity周期tick定时器(仅发信号, proximity_task由统一线程执行)
 */
void sndp_hr_proximity_tick_enable(bool en)
{
    if(sndp_hr_proximity_tick_timer_id == NULL) {
        return;
    }

    if(en) {
        osTimerStart(sndp_hr_proximity_tick_timer_id, SNDP_HR_PROXIMITY_TICK_PERIOD_MS);
    } else {
        osTimerStop(sndp_hr_proximity_tick_timer_id);
    }
}

/**
 * @brief   心率/睡眠算法处理任务: 从队列取1秒PPG+ACC数据送入dbbeats算法, 并上报心率结果
 *          由统一任务线程在PPG队列数据量足够时调用(约1秒周期)
 */
static void sndp_heartrate_algo_task(void)
{
#if defined(__SNDP_HR_ALGO__)
    POSSIBLY_UNUSED struct HrvIndices hrv;
    POSSIBLY_UNUSED uint8_t* hrv_ptr = (uint8_t*)&hrv;
#else
    POSSIBLY_UNUSED uint8_t* hrv_ptr = NULL;
#endif
    POSSIBLY_UNUSED sndp_hr_dbbeats_data dbbeats_data;
    POSSIBLY_UNUSED int8_t led;
    POSSIBLY_UNUSED int32_t acc_queue_len;
    POSSIBLY_UNUSED int32_t acc_data_len;
#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)
    uint32_t start_time;
    uint32_t end_time;
#endif

    // hr_setp_5: Read ppg data
    // sleep_step_5: Read ppg data
    memset(hr_ppg_raw_data, 0, sizeof(hr_ppg_raw_data));
    sndp_hr_ppg_raw_data_queue_pop_data(hr_ppg_raw_data, HR_PPG_SECOND_ALLCH_SAMPLES);

    // hr_setp_6: Read acc data
    // sleep_step_6: Read acc data
    acc_queue_len = sndp_hr_acc_raw_data_queue_get_len();
    if(acc_queue_len >= HR_ACC_SECOND_ALLCH_SAMPLES) {
        acc_data_len = HR_ACC_SECOND_ALLCH_SAMPLES;
    } else {
        acc_data_len = acc_queue_len;
    }
    memset(hr_acc_raw_data, 0, sizeof(hr_acc_raw_data));
    sndp_hr_acc_raw_data_queue_pop_data(hr_acc_raw_data, acc_data_len);

#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)
    start_time = hal_sys_timer_get();
#endif

    // hr_setp_7: Input data
    // sleep_step_7: Input data
    dbbeats_data.is_contact = 1;
    dbbeats_data.led_state = 50;
    dbbeats_data.pck_interval = 1000;
#if defined(__SNDP_HR_ALGO__)
    dbbeats_put_heartrate_data(
            hr_acc_raw_data,
            hr_ppg_raw_data,
            hr_dev_state,
            dbbeats_data.is_contact,
            dbbeats_data.led_state,
            HR_ACC_SECOND_ALLCH_SAMPLES,
            HR_PPG_SECOND_ALLCH_SAMPLES,
            HR_DEV_SECOND_ALLCH_SAMPLES,
            dbbeats_data.pck_interval);
    // hr_setp_8: Return results
    // sleep_step_8: Return results
    memset(&hrv, 0, sizeof(struct HrvIndices));
    dbbeats_get_heartrate_data(&hrv, &dbbeats_data.result_code, &dbbeats_data.count, &led, &debug_dump);
#endif

#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)
    end_time = hal_sys_timer_get();
    SNDP_TRACE(0, "HR algo cost: %d us", TICKS_TO_US(end_time - start_time));
#endif

    // hr_setp_9: Display hr results
    // sleep_step_9: Display hr results
#if defined(__SNDP_HR_ALGO__)
    if(dbbeats_data.result_code == 1 && hrv.HR > 1) {
        SNDP_TRACE(0, "HR: %d BPM, SDNN: %d ms libv: %s", hrv.HR, hrv.SDNN, lib_engine_version());
    } else if (dbbeats_data.result_code == 101) {
        SNDP_TRACE(0, "HR: Sensor detached");
    }
#else
    SNDP_TRACE(0, "HR_ALGO disabled: No Hr data");
#endif
    // hr_setp_10: Report results
    if(hr_ctx.hr_running) {
#if defined(__SNDP_SLEEP_APP__)
        sndp_comm_cmd_sleepapp_report_hr(hrv_ptr, (uint8_t*)&dbbeats_data);
#endif
    }

    hr_measure_time++;
}

/**
 * @brief   统一任务轮询线程(MCU super-loop风格):
 *          while(1)轮询5个task, 由osSignal信号位唤醒, 运行状态由hr_ctx各flag控制
 *          task1: ppg_task            - PPG FIFO读取(SIGNAL_PPG_FIFO: GPIO IRQ)
 *          task2: acc_task            - ACC FIFO读取(SIGNAL_ACC_FIFO: GPIO IRQ + 250ms看门狗)
 *          task3: heartrate_task      - 心率算法(SIGNAL_HEARTRATE: PPG队列凑满1秒时回调发出)
 *          task4: sleep_analysis_task - 睡眠分期(SIGNAL_SLEEP_TRACK: 手机端数据到达)
 *          task5: proximity_task      - proximity上报(SIGNAL_PROXIMITY_TICK: 1秒定时器)
 */
static void sndp_hr_sleep_app_process_thread(void const *argument)
{
    uint32_t acc_last_poll_ms = 0;
    uint32_t curr_ms;
    uint32_t wait_timeout;
    int32_t fired_signals;
    osEvent evt;

    HR_TRACE(0, "running...");

    while(1) {
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

        // ACC读取期间保留250ms看门狗轮询(沿用原da217e线程行为), 其余情况无限等待任务信号
        wait_timeout = (sndp_hr_is_reading_acc_enabled() && !sndp_hr_acc_is_suspend()) ? SENSOR_TASK_ACC_WATCHDOG_MS : osWaitForever;
        evt = osSignalWait(0, wait_timeout);

        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
        curr_ms = TICKS_TO_MS(hal_sys_timer_get());

        // 超时不携带信号位, 仅用于ACC看门狗
        fired_signals = (evt.status == osEventSignal) ? evt.value.signals : 0;
        // SNDP_TRACE(0, "fired_signals: 0x%08x ppg_queue_len: %d", fired_signals, sndp_hr_ppg_raw_data_queue_get_len());

        /********** task1: ppg_task **********/
#if defined(__SNDP_HRSENSOR_SUPPORT__)
        /* 注意: 绝不能用sndp_hr_is_reading_ppg_enabled()门控! ss_ppg_interrupt_handler除读FIFO外,
           还负责处理佩戴事件(g_proximity_sta, FIFO中断使能的前提)并清除传感器INT引脚;
           若被门控跳过, 边沿触发的GPIO将因INT未清除而永久收不到后续中断 */
        // SNDP_TRACE(0, "ppg_enabled: %d, ppgpend:%d accpend: %d", sndp_hr_is_reading_ppg_enabled(), 
        //                                                 sndp_hr_ppg_is_suspend(), sndp_hr_acc_is_suspend());
        if((fired_signals & SENSOR_TASK_SIGNAL_PPG_FIFO)) {
            sndp_hal_hr_ppg_fifo_task();
        }
#endif

        /********** task2: acc_task **********/
#if defined(__SNDP_GSENSOR_SUPPORT__)
        if(sndp_hr_is_reading_acc_enabled() && !sndp_hr_acc_is_suspend()) {
            if(fired_signals & SENSOR_TASK_SIGNAL_ACC_FIFO) {
                acc_last_poll_ms = curr_ms;
                sndp_hal_acc_fifo_task();
            } else if((curr_ms - acc_last_poll_ms) >= SENSOR_TASK_ACC_WATCHDOG_MS) {
                // 看门狗: 中断丢失时兜底轮询一次
                acc_last_poll_ms = curr_ms;
                // SNDP_TRACE(0, "250ms acc_fifo_task");
                sndp_hal_acc_fifo_task();
            }
        }
#endif

        /********** task3: heartrate_task **********/
        if((fired_signals & SENSOR_TASK_SIGNAL_HEARTRATE) && !sndp_hr_acc_is_suspend() &&
           (hr_ctx.hr_running) && !sndp_hr_ppg_is_suspend() &&
           (sndp_hr_ppg_raw_data_queue_get_len() >= HR_PPG_SECOND_ALLCH_SAMPLES)) {
            sndp_heartrate_algo_task();
        }

        /********** task4: sleep_analysis_task **********/
        if(hr_ctx.sleep_running && hr_ctx.sleep_tracking) {
            hr_ctx.sleep_tracking = false;
            sndp_sleep_analysis();
        }

        /********** task5: proximity_task **********/
        if(fired_signals & SENSOR_TASK_SIGNAL_PROXIMITY_TICK) {
#if defined(__SNDP_SLEEP_APP__)
            sndp_comm_cmd_sleepapp_proximity_task();
#endif
        }
    }
}

static void sndp_hr_read_ppg_callback(int32_t *data, uint16_t cnt)
{
    // HR_TRACE(0, "cnt=%d", cnt);
    if(hr_ctx.hr_running) {
        // SNDP_DUMP32("%08X ", data,  cnt > 16?16:cnt);
        sndp_hr_ppg_raw_data_queue_push_data(data, cnt);

        // 队列凑满1秒数据, 显式发送信号触发heartrate_task(task3)
        if(sndp_hr_ppg_raw_data_queue_get_len() >= HR_PPG_SECOND_ALLCH_SAMPLES) {
            if(hr_process_thread_tid) {
                osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_HEARTRATE);
            }
        }
    }
}

static void sndp_report_ppg_raw_data_callback(uint8_t *data, uint16_t cnt)
{
    if(sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE) == 0x01 || 
       sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE) == 0x01) {
        if(cnt > 0) {
#if defined(__SNDP_SLEEP_APP__)
            sndp_comm_cmd_sleepapp_report_ppg_raw_data(data, cnt);
#endif
        }
    }
}




#if defined(__SNDP_GSENSOR_SUPPORT__)
static void sndp_hr_acc_read_raw_data_callback(sndp_hal_acc_data_s *data, uint16_t cnt)
{
    //HR_TRACE(0, "cnt=%d", cnt);
    //SNDP_DUMP32("%04X ", data,  cnt > 16?16:cnt);
    
    if(hr_ctx.hr_running) {
        sndp_hr_acc_raw_data_queue_push_data((int16_t *)data, cnt * 3);
    }

    if(sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE) == 0x01 || 
        sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE) == 0x01) {
        if(cnt > 0) {
            //report ACC data
                // HR_TRACE(0, "acc notification, cnt=%d", cnt);
#if defined(__SNDP_SLEEP_APP__)
            sndp_comm_cmd_sleepapp_report_acc_ntf((int16_t *)data, cnt * 3);
#endif
            // sndp_comm_cmd_sleepapp_report_acc_ntf_debug((int16_t *)data, cnt * 3);
        }
    }
}
#endif 

void sndp_hr_switch_reading_ppg_raw_data(uint32_t user, bool onoff)
{
    HR_TRACE(0, "user=%d, onoff=%d, flag=%d", user, onoff, hr_ctx.ppg_user_flag);

#if defined(__SNDP_HRSENSOR_SUPPORT__)
    uint32_t suspend_mask = SENSOR_OP_USER_WEAR_SUSPEND_PPG | SENSOR_OP_USER_BT_CALL_SUSPEND_PPG;

    /* 计算修改flag前的期望传感器状态 */
    bool before_suspended = (hr_ctx.ppg_user_flag & suspend_mask) != 0;
    bool before_normal    = (hr_ctx.ppg_user_flag & ~suspend_mask) != 0;
    bool before_on        = !before_suspended && before_normal;

    /* 更新user flag */
    if (onoff) {
        sndp_hr_user_ppg_flag_set(&hr_ctx.ppg_user_flag, (sensor_ppg_op_user_e)user);
    } else {
        sndp_hr_user_ppg_flag_clear(&hr_ctx.ppg_user_flag, (sensor_ppg_op_user_e)user);
    }

    /* 计算修改flag后的期望传感器状态 */
    bool after_suspended = (hr_ctx.ppg_user_flag & suspend_mask) != 0;
    bool after_normal    = (hr_ctx.ppg_user_flag & ~suspend_mask) != 0;
    bool after_on        = !after_suspended && after_normal;

    hr_ctx.hr_ppg_suspended = after_suspended;

    /* 传感器状态无变化, 跳过重复开/关 */
    if (before_on == after_on) {
        return;
    }

    if (after_on) {
        sndp_hal_hr_set_reading_ppg_callback(sndp_hr_read_ppg_callback);
        sndp_hal_hr_set_report_ppg_raw_data_callback(sndp_report_ppg_raw_data_callback);
        // sndp_hal_hr_set_fifo_ready_callback(sndp_hr_notify_ppg_fifo_ready);
        sndp_hal_hr_start_reading_ppg();

        /* 立即唤醒线程服务一次传感器INT:
           1)清除启动前可能挂起的INT(边沿触发,不服务则永久阻塞后续中断);
           2)线程可能正阻塞在osWaitForever,需踢醒后重估看门狗超时 */
        if(hr_process_thread_tid) {
            osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_PPG_FIFO);
        }
    } else {
        sndp_hal_hr_stop_reading_ppg();
    }
#endif
}

void sndp_hr_switch_reading_acc_raw_data(uint32_t user, bool onoff)
{
    HR_TRACE(0, "user=%d, onoff=%d, flag=%d", user, onoff, hr_ctx.acc_user_flag);

#if defined(__SNDP_GSENSOR_SUPPORT__)
    uint32_t suspend_mask = SENSOR_OP_USER_WEAR_SUSPEND_ACC | SENSOR_OP_USER_BT_CALL_SUSPEND_ACC;

    /* 计算修改flag前的期望传感器状态 */
    bool before_suspended = (hr_ctx.acc_user_flag & suspend_mask) != 0;
    bool before_normal    = (hr_ctx.acc_user_flag & ~suspend_mask) != 0;
    bool before_on        = !before_suspended && before_normal;

    /* 更新user flag */
    if (onoff) {
        sndp_hr_user_acc_flag_set(&hr_ctx.acc_user_flag, (sensor_acc_op_user_e)user);
    } else {
        sndp_hr_user_acc_flag_clear(&hr_ctx.acc_user_flag, (sensor_acc_op_user_e)user);
    }

    /* 计算修改flag后的期望传感器状态 */
    bool after_suspended = (hr_ctx.acc_user_flag & suspend_mask) != 0;
    bool after_normal    = (hr_ctx.acc_user_flag & ~suspend_mask) != 0;
    bool after_on        = !after_suspended && after_normal;

    hr_ctx.hr_acc_suspended = after_suspended;

    /* 传感器状态无变化, 跳过重复开/关 */
    if (before_on == after_on) {
        return;
    }

    if (after_on) {
        sndp_hal_acc_stop_single_tap_interrupt();
        sndp_hal_acc_set_reading_raw_data_callback(sndp_hr_acc_read_raw_data_callback);
        sndp_hal_set_acc_fifo_ready_callback(sndp_hr_notify_acc_fifo_ready);
        sndp_hal_acc_start_reading_raw_data();
    } else {
        sndp_hal_acc_stop_reading_raw_data();
        sndp_hal_acc_start_single_tap_interrupt();
    }
#endif
}

void sndp_hr_mearsuring_start(int8_t ppg_sampling_rate, uint8_t dump_state)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);

    SNDP_TRACE(0, "sndp_hr_mearsuring_start...");

    sndp_hr_ppg_raw_data_queue_reset();

#if defined(__SNDP_HR_ALGO__)
    dbbeats_initialize_heartrate_data(ppg_sampling_rate, dump_state);
#endif

    hr_measure_time = 0;
    hr_ctx.hr_running = true;
    sndp_hr_mearsuring_set_dump_state(HR_DUMP_STATE, dump_state);
    sndp_hr_mearsuring_set_sampling_rate(ppg_sampling_rate);
    if(sndp_dev_wear_is_worn(false) || sndp_call_get_in_out_flag() == 0 ) {
        sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_HR_ACC, true);
        sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_HR_PPG, true);
    }else{
        sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_HR_ACC|SENSOR_OP_USER_WEAR_SUSPEND_ACC, true);
        sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_HR_PPG|SENSOR_OP_USER_WEAR_SUSPEND_PPG, true);
    }
}

void sndp_hr_mearsuring_stop(void)
{
    SNDP_TRACE(0, "sndp_hr_mearsuring_stop...");
    hr_ctx.hr_running = false;
    sndp_hr_mearsuring_set_dump_state(HR_DUMP_STATE, 0x00);
    if(sndp_dev_wear_is_worn(false) || sndp_call_get_in_out_flag() == 0) {
        sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_HR_ACC, false);
        sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_HR_PPG, false);
    }else{
        sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_HR_ACC|SENSOR_OP_USER_WEAR_SUSPEND_ACC, false);
        sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_HR_PPG|SENSOR_OP_USER_WEAR_SUSPEND_PPG, false);
    }

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

/**
 * @brief   挂起HR/睡眠算法运行(取下佩戴场景):
 *          1)关闭ACC传感器; 2)task3/task4算法被hr_suspended门控暂停, 算法状态保留;
 *          PPG无需处理: 取下佩戴后传感器底层自动停止数据上传和中断触发。
 *          不改动hr_running/sleep_running等user标志, resume时按原样恢复。
 *          usersuspend: 0 is wear suspend 1 is bt call suspend
 */
void sndp_hr_suspend(uint32_t usersuspend)
{
    sensor_acc_op_user_e acc_op_user;
    sensor_ppg_op_user_e ppg_op_user;

    if(!sndp_is_notifi_hr_enabled()){
        SNDP_TRACE(0, "hr suspend, notifi hr is not enabled");
        return;
    }
    if(usersuspend == 0){
        acc_op_user = SENSOR_OP_USER_WEAR_SUSPEND_ACC;
        ppg_op_user = SENSOR_OP_USER_WEAR_SUSPEND_PPG;
    }else{
        acc_op_user = SENSOR_OP_USER_BT_CALL_SUSPEND_ACC;
        ppg_op_user = SENSOR_OP_USER_BT_CALL_SUSPEND_PPG;        
    }
    
    SNDP_TRACE(0, "hr suspend");
    if(sndp_hr_is_reading_ppg_enabled()){
        /* onoff=true: 置位suspend flag, 优先级高于普通user, 强制关闭PPG传感器 */
        sndp_hr_switch_reading_ppg_raw_data(ppg_op_user, true);
    }   
    if(sndp_hr_is_reading_acc_enabled()){
        /* onoff=true: 置位suspend flag, 优先级高于普通user, 强制关闭ACC传感器 */
        sndp_hr_switch_reading_acc_raw_data(acc_op_user, true);
    }
    if(sndp_hr_running_state()){
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);
    }
}

void sndp_sleep_analysis_suspend(void)
{
    if(!hr_ctx.sleep_running) {
        return;
    }
    SNDP_TRACE(0, "sleep analysis suspend");
    hr_ctx.sleep_running = false;
}

/**
 * @brief   恢复HR/睡眠算法运行(重新佩戴场景):
 *          清除挂起态, ACC按user需求恢复开启, 清空挂起前残留的队列数据从新鲜数据起算。
 */
void sndp_hr_resume(uint32_t usersuspend)
{
    sensor_acc_op_user_e acc_op_user;
    sensor_ppg_op_user_e ppg_op_user;

    if(!sndp_is_notifi_hr_enabled()){
        SNDP_TRACE(0, "hr resume, notifi hr is not enabled");
        return;
    }
    if(usersuspend == 0){
        acc_op_user = SENSOR_OP_USER_WEAR_SUSPEND_ACC;
        ppg_op_user = SENSOR_OP_USER_WEAR_SUSPEND_PPG;
    }else{
        acc_op_user = SENSOR_OP_USER_BT_CALL_SUSPEND_ACC;
        ppg_op_user = SENSOR_OP_USER_BT_CALL_SUSPEND_PPG;        
    }
    
    SNDP_TRACE(0, "hr resume");
    if(sndp_hr_is_reading_ppg_enabled()){
        sndp_hr_ppg_raw_data_queue_reset();
        /* onoff=false: 清除suspend flag, 恢复后由普通user flag决定PPG传感器开关 */
        sndp_hr_switch_reading_ppg_raw_data(ppg_op_user, false);
    }
    if(sndp_hr_is_reading_acc_enabled()){
        sndp_hr_acc_raw_data_queue_reset();
        /* onoff=false: 清除suspend flag, 恢复后由普通user flag决定ACC传感器开关 */
        sndp_hr_switch_reading_acc_raw_data(acc_op_user, false);
    }
    if(sndp_hr_running_state()){
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    }
}

void sndp_sleep_analysis_resume(void)
{
    if(!hr_ctx.sleep_running) {
        return;
    }
    SNDP_TRACE(0, "sleep analysis resume");
    hr_ctx.sleep_running = true;
}

void sndp_dbbeats_put_sleep_app_data(int16_t accel_data_m[],
                                uint8_t screen_status[],
                                int8_t sound_state)
{
    memset(sleep_app_accel, 0, sizeof(sleep_app_accel));
    memset(sleep_screen_status, 0, sizeof(sleep_screen_status));
    sleep_sound_state = 0;

    memcpy(sleep_app_accel, accel_data_m, sizeof(sleep_app_accel));
    memcpy(sleep_screen_status, screen_status, sizeof(sleep_screen_status));
    sleep_sound_state = sound_state;

    hr_ctx.sleep_tracking = true;

    // 通知统一任务线程立即处理(否则要等下一次传感器IRQ唤醒)
    if(hr_process_thread_tid) {
        osSignalSet(hr_process_thread_tid, SENSOR_TASK_SIGNAL_SLEEP_TRACK);
    }
}
// sleep analysis function
static void sndp_sleep_analysis(void) 
{
#if defined(__SNDP_HR_ALGO__)
    SNDP_TRACE(0, "sleep analyse...");
    dbbeats_sleep_data(sleep_app_accel, sleep_screen_status, sleep_sound_state,\
        sleep_stage, &sleep_position, &sound_control, &result_code);
#else
    SNDP_TRACE(0, "HR_ALGO disabled: No sleep data");
#endif
    // sleep_step_14: analysis result
    if (result_code == 1) {
        // SNDP_TRACE(0, "Sleep position: %d\n", sleep_position);
        // sleep_step_15: Use sleep_stage[0~39]

        // sleep_step_16: report data to app via ble.
        // 统一任务线程内直接上报(与HR/PPG/ACC/proximity上报同线程, 共享发送缓冲区无需再切换上下文)
        if(hr_ctx.sleep_running) {
#if defined(__SNDP_SLEEP_APP__)
            uint16_t position_and_control = (sleep_position & 0xFF) | ((sound_control & 0xFF)<<8);
            sndp_comm_cmd_sleepapp_report_sleep_stage(sleep_stage, position_and_control, result_code);
#endif
        }
    } else {
        SNDP_TRACE(0, "Error: %d\n", result_code);
    }
}

void sndp_sleep_analysis_start(int32_t sleep_control)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "sndp_sleep_analysis_start...");
    
    sndp_hr_ppg_raw_data_queue_reset();
    
    // sleep_step_1:算法初始化
#if defined(__SNDP_HR_ALGO__)
    dbbeats_initialize_sleep_data(sleep_control);
#endif

    // sleep_analysis不持有传感器, 仅记录sleep标志, 不触碰hr_running及其他flag
    hr_ctx.sleep_running = true;
    hr_ctx.sleep_tracking = false;
}

void sndp_sleep_analysis_stop(void)
{
    SNDP_TRACE(0, "sndp_sleep_analysis_stop...");

    // hr_setp_17: 停止处理(仅清除sleep标志, 不触碰hr_running及传感器)
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

void sndp_ppg_notification_start(uint8_t dump_state)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "sndp_ppg_notification_start...");

    sndp_hr_ppg_raw_data_queue_reset();
    sndp_hr_mearsuring_set_dump_state(PPG_DUMP_STATE, dump_state);
    sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_PPG, true);
    memset(hr_ppg_raw_data, 0, sizeof(hr_ppg_raw_data));
}

void sndp_ppg_notification_stop(void)
{
    SNDP_TRACE(0, "sndp_ppg_notification_stop...");
    sndp_hr_mearsuring_set_dump_state(PPG_DUMP_STATE, 0x00);
    sndp_hr_switch_reading_ppg_raw_data(SENSOR_OP_USER_PPG, false);
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

void sndp_acc_notification_start(uint8_t dump_state)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "sndp_acc_notification_start...");

    sndp_hr_acc_raw_data_queue_reset();
    sndp_hr_mearsuring_set_dump_state(ACC_DUMP_STATE, dump_state);
    sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_ACC, true);
    memset(hr_acc_raw_data, 0, sizeof(hr_acc_raw_data));
}

void sndp_acc_notification_stop(void)
{
    SNDP_TRACE(0, "sndp_acc_notification_stop...");

    sndp_hr_mearsuring_set_dump_state(ACC_DUMP_STATE, 0x00);
    sndp_hr_switch_reading_acc_raw_data(SENSOR_OP_USER_ACC, false);
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

static bool sndp_is_notifi_hr_enabled(void)
{
    SNDP_TRACE(0, "hr: %d acc: %d ppg: %d", hr_ctx.hr_running, hr_ctx.acc_user_flag, hr_ctx.ppg_user_flag);
   if(hr_ctx.hr_running || hr_ctx.acc_user_flag || hr_ctx.ppg_user_flag) 
   {
      return true;
   }
   else
   {
      return false;
   }
}

void sndp_hr_ble_disconnected_delay10s_start(void)
{   
    SNDP_TRACE(0, "running: %d cover_closed: %d", hr_ctx.sleep_running, sndp_dev_cover_is_closed(false));
 #if defined(__SNDP_SLEEP_APP__)
    if(sndp_dev_cover_is_closed(false)) {
        sndp_sleep_comm_disconnect_timer_handler();
    }else {
        hr_ctx.delay_10S_start = true;
        sndp_delay_exec_start(10000, (uint32_t)sndp_sleep_comm_disconnect_timer_handler, 0, 0, 0);
    }
#endif
}

void sndp_hr_ble_connected_delay10s_stop(void)
{
    SNDP_TRACE(0, "Srunning: %d HRrunning: %d", hr_ctx.sleep_running, hr_ctx.hr_running);
    if(hr_ctx.sleep_running){
        hr_ctx.delay_10S_start = false;
#if defined(__SNDP_SLEEP_APP__)
        sndp_delay_exec_stop((uint32_t)sndp_sleep_comm_cmd_analysis_stop);        
#endif
    }
}

#if defined(__SNDP_HR_ALGO__)
void sndp_hr_print_log(const char *msg)
{
    if(msg != NULL) {
        SNDP_TRACE(0, "%s", msg);
    }
}

#endif

void sndp_hr_app_init(void)
{
    // 1. 创建队列并初始化。
    if(ppg_raw_data_queue_mutex_id == NULL) {
        ppg_raw_data_queue_mutex_id = osMutexCreate((osMutex(ppg_raw_data_queue_mutex)));
        ASSERT(ppg_raw_data_queue_mutex_id != NULL, "%s, %d", __func__, __LINE__);
    }
    InitCQueue(&ppg_raw_data_queue, sizeof(ppg_raw_data_queue_buf), (CQItemType *)ppg_raw_data_queue_buf);
    
    if(acc_raw_data_queue_mutex_id == NULL) {
        acc_raw_data_queue_mutex_id = osMutexCreate(osMutex(acc_raw_data_queue_mutex));
        ASSERT(acc_raw_data_queue_mutex_id != NULL, "%s, %d", __func__, __LINE__);
    }
    InitCQueue(&acc_raw_data_queue, sizeof(acc_raw_data_queue_buf), (CQItemType *)acc_raw_data_queue_buf);

    // 2. 创建proximity周期tick定时器(仅发任务信号, proximity_task由统一任务线程执行)。
    if (sndp_hr_proximity_tick_timer_id == NULL) {
        sndp_hr_proximity_tick_timer_id = osTimerCreate(osTimer(SNDP_HR_PROXIMITY_TICK_TIMER), osTimerPeriodic, NULL);
        ASSERT(sndp_hr_proximity_tick_timer_id != NULL, "%s, line=%d", __func__, __LINE__);
    }

    // 3. 创建统一任务轮询线程(任务信号由IRQ/定时器通过osSignalSet发送, 无需信号量)。
    if (hr_process_thread_tid == NULL)  {
        hr_process_thread_tid = osThreadCreate(osThread(sndp_hr_sleep_app_process_thread), NULL);
        ASSERT(hr_process_thread_tid != NULL, "%s, line=%d", __func__, __LINE__);
    }

    memset(&hr_ctx, 0, sizeof(sndp_hr_ctx_s));

#if defined(__SNDP_HR_ALGO__)
    SNDP_TRACE(0, "lib_ver:%s", lib_engine_version());
    dbbeats_print_log_cfg(sndp_hr_print_log);
#endif

	HR_TRACE(0, "done");

}

#endif	/* __SNDP_HEART_RATE_MGR__ */

