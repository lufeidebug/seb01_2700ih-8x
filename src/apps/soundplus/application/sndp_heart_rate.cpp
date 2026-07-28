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

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
#include "sleepsense.h"
#endif

#if defined(__SNDP_GSENSOR_SUPPORT__)
#include "sndp_hal_acc.h"
#endif

#if defined(__SNDP_HRSENSOR_SUPPORT__)
#include "sndp_hal_hr.h"
#endif
#include "sndp_comm_cmd.h"
#include "sndp_sleep_role_switch.h"

/**************************************************************************************************
* 1、创建心率处理线程。
* 2、创建心率处理线程唤醒信号量。
* 3、创建心率RAW Data 队列。接收到数据时Push到队列中，并唤醒心率处理线程。
* 4、创建加速度RAW Data 队列。接收到数据时Push到队列中，并唤醒心率处理线程。
* 5、处理线程中每次被唤醒，检测心率RAW Data和加速度RAW Data数据是否有足够的数据。有就送入算法中计算。
* 6、
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
    bool ppg_notification;
    bool acc_notification;
    uint8_t sampling_rate;
    uint8_t dump_state;
    int32_t sleep_control;

    bool ppg_reading_en;
    bool acc_reading_en;
    bool delay_10S_start;
    
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
static void sndp_hr_process_thread(void const *argument);
osThreadDef(sndp_hr_process_thread, osPriorityAboveNormal, 1, HR_PROCESS_THREAD_STACK_SIZE, "hr_process_thread");
osThreadId hr_process_thread_tid = NULL;

osSemaphoreId hr_process_wait_semaphore_id = NULL;
osSemaphoreDef(hr_process_wait_semaphore);

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
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
/**************************************************************************************************
* Function
**************************************************************************************************/

static int ppg_raw_data_queue_push_data(int32_t *item, int cnt)
{
    int ret;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&ppg_raw_data_queue, (CQItemType *)item, sizeof(int32_t) * cnt);
    osMutexRelease(ppg_raw_data_queue_mutex_id);
    return ret;
}

POSSIBLY_UNUSED static int ppg_raw_data_queue_pop_data(int32_t *item, int cnt)
{
    int ret;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&ppg_raw_data_queue, (CQItemType *)item, sizeof(int32_t) * cnt);
    osMutexRelease(ppg_raw_data_queue_mutex_id);

    return ret;
}

static int ppg_raw_data_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&ppg_raw_data_queue);
    osMutexRelease(ppg_raw_data_queue_mutex_id);

    queue_len = queue_len / sizeof(int32_t);
    return queue_len;
}

static void ppg_raw_data_queue_reset(void)
{
    osMutexWait(ppg_raw_data_queue_mutex_id, osWaitForever);
    ResetCQueue(&ppg_raw_data_queue);
    osMutexRelease(ppg_raw_data_queue_mutex_id);
}


static int acc_raw_data_queue_push_data(int16_t *item, int cnt)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&acc_raw_data_queue, (CQItemType *)item, sizeof(int16_t) * cnt);
    osMutexRelease(acc_raw_data_queue_mutex_id);
    return ret;
}

POSSIBLY_UNUSED static int acc_raw_data_queue_pop_data(int16_t *item, int cnt)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&acc_raw_data_queue, (CQItemType *)item, sizeof(int16_t) * cnt);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    return ret;
}

POSSIBLY_UNUSED static int acc_raw_data_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&acc_raw_data_queue);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    queue_len = queue_len / sizeof(int16_t);
    return queue_len;
}

// #define __SNDP_RUN_ALGO_ONLY__
static void sndp_hr_process_thread(void const *argument)
{
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)    
    POSSIBLY_UNUSED struct HrvIndices hrv;
    POSSIBLY_UNUSED uint8_t* hrv_ptr = (uint8_t*)&hrv;
#endif
    POSSIBLY_UNUSED sndp_hr_dbbeats_data dbbeats_data;
    POSSIBLY_UNUSED int8_t led;
    POSSIBLY_UNUSED int32_t acc_queue_len;
    POSSIBLY_UNUSED int32_t acc_data_len;
#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)    
    uint32_t start_time;
    uint32_t end_time;
#endif    

    HR_TRACE(0, "running...");

    while(1) {
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);
        osSemaphoreWait(hr_process_wait_semaphore_id, osWaitForever);
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
        //HR_TRACE(0, "wakeup");

        if(!hr_ctx.hr_running && !hr_ctx.sleep_running) {
            HR_TRACE(0, "app not running, ret");
            continue;
        }

        // hr_setp_4: Check whether the PPG data is enough.
        // sleep_step_4: Check whether the PPG data is enough.
        if(ppg_raw_data_queue_get_len() < HR_PPG_SECOND_ALLCH_SAMPLES) {
            HR_TRACE(0, "ppg is not enough, ret");
            continue;
        }


#if 0
        // Check whether the acc data is enough.
        if(acc_raw_data_queue_get_len() < HR_ACC_SECOND_ALLCH_SAMPLES) {
            continue;
        }
#endif
        // hr_setp_5: Read ppg data
        // sleep_step_5: Read ppg data
        memset(hr_ppg_raw_data, 0, sizeof(hr_ppg_raw_data));
        ppg_raw_data_queue_pop_data(hr_ppg_raw_data, HR_PPG_SECOND_ALLCH_SAMPLES);


        // hr_setp_6: Read acc data
        // sleep_step_6: Read acc data
        acc_queue_len = acc_raw_data_queue_get_len();
        if(acc_queue_len >= HR_ACC_SECOND_ALLCH_SAMPLES) {
            acc_data_len = HR_ACC_SECOND_ALLCH_SAMPLES;
        } else {
            acc_data_len = acc_queue_len;
        }
        memset(hr_acc_raw_data, 0, sizeof(hr_acc_raw_data));
        acc_raw_data_queue_pop_data(hr_acc_raw_data, acc_data_len);
        
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)

#if 0
        SNDP_TRACE(0, "engine_ver: %s", lib_engine_version());
        SNDP_TRACE(0, "ppg data, idx:(%d):", hr_measure_time);
        SNDP_DUMP32("%6d, ", &hr_ppg_raw_data[0],  16);
        SNDP_DUMP32("%6d, ", &hr_ppg_raw_data[16],  16);
        SNDP_DUMP32("%6d, ", &hr_ppg_raw_data[32],  16);
        SNDP_DUMP32("%6d, ", &hr_ppg_raw_data[48],  16);
#endif        

#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)    
        start_time = hal_sys_timer_get();
#endif         

        // hr_setp_7: Input data
        // sleep_step_7: Input data
        dbbeats_data.is_contact = 1;
        dbbeats_data.led_state = 50;
        dbbeats_data.pck_interval = 1000;
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

#if defined(__SNDP_HR_PRINT_ALGO_EXEC_TIME__)    
        end_time = hal_sys_timer_get();
        SNDP_TRACE(0, "HR algo cost: %d us", TICKS_TO_US(end_time - start_time));
#endif

        // hr_setp_9: Display hr results
        // sleep_step_9: Display hr results
        if(dbbeats_data.result_code == 1 && hrv.HR > 1) {
            SNDP_TRACE(0, "HR: %d BPM, SDNN: %d ms libv: %s", hrv.HR, hrv.SDNN, lib_engine_version());
        } else if (dbbeats_data.result_code == 101) {
            SNDP_TRACE(0, "HR: Sensor detached");
        }

        // hr_setp_10: Report results
        if(hr_ctx.hr_running) {

                // sndp_call_func_in_app_thread((uint32_t)sndp_comm_cmd_sleepapp_report_hr, (uint32_t)hrv_ptr, (uint32_t)&dbbeats_data, 0);
                sndp_comm_cmd_sleepapp_report_hr(hrv_ptr, (uint8_t*)&dbbeats_data);
        }
        
        if(hr_ctx.sleep_running && hr_ctx.sleep_tracking) {
            hr_ctx.sleep_tracking = false;
            sndp_sleep_analysis();
        }
#endif

        hr_measure_time++;
    }
}

uint8_t sndp_hr_mearsuring_get_sampling_rate(void)
{
    return hr_ctx.sampling_rate;
}

void sndp_hr_mearsuring_set_sampling_rate(uint8_t sampling_rate)
{
    hr_ctx.sampling_rate = sampling_rate;
}

uint8_t sndp_hr_mearsuring_get_dump_state(void)
{
    return hr_ctx.dump_state;
}

void sndp_hr_mearsuring_set_dump_state(uint8_t dump_state)
{
    hr_ctx.dump_state = dump_state;
}

void sndp_set_sleep_control(int32_t sleep_control)
{
    hr_ctx.sleep_control = sleep_control;
}

int32_t sndp_get_sleep_control(void)
{
    return hr_ctx.sleep_control;
}

uint8_t sndp_hr_running_state(void)
{
    return hr_ctx.hr_running;
}

int32_t sndp_get_acc_notification(void)
{
    return hr_ctx.acc_notification;
}



static void sndp_hr_read_ppg_callback(int32_t *data, uint16_t cnt)
{
    // HR_TRACE(0, "cnt=%d", cnt);
    if(hr_ctx.hr_running || hr_ctx.sleep_running) {
        // SNDP_DUMP32("%08X ", data,  cnt > 16?16:cnt);
        ppg_raw_data_queue_push_data(data, cnt);

        //HR_TRACE(0, "queue_len=%d, %d", ppg_raw_data_queue_get_len(), HR_PPG_SECOND_ALLCH_SAMPLES);
        if(ppg_raw_data_queue_get_len() >= HR_PPG_SECOND_ALLCH_SAMPLES) {
            //HR_TRACE(0, "wakeup thread");
            osSemaphoreRelease(hr_process_wait_semaphore_id);
        }
    }
}

static void sndp_report_ppg_raw_data_callback(uint8_t *data, uint16_t cnt)
{
    if(hr_ctx.dump_state) {
        if(cnt > 0) {
            sndp_comm_cmd_sleepapp_report_ppg_raw_data(data, cnt);
            //report PPG data
            // sndp_comm_cmd_sleepapp_report_ppg_ntf(data, cnt);
        }
    }
}




#if defined(__SNDP_GSENSOR_SUPPORT__)
static void sndp_hr_acc_read_raw_data_callback(sndp_hal_acc_data_s *data, uint16_t cnt)
{
    //HR_TRACE(0, "cnt=%d", cnt);
    //SNDP_DUMP32("%04X ", data,  cnt > 16?16:cnt);
    
    if(hr_ctx.hr_running || hr_ctx.sleep_running) {
        acc_raw_data_queue_push_data((int16_t *)data, cnt * 3);
    }

    if(hr_ctx.dump_state) {
        if(cnt > 0) {
            //report ACC data
                // HR_TRACE(0, "acc notification, cnt=%d", cnt);
            sndp_comm_cmd_sleepapp_report_acc_ntf((int16_t *)data, cnt * 3);
            // sndp_comm_cmd_sleepapp_report_acc_ntf_debug((int16_t *)data, cnt * 3);
        }
    }
}
#endif 

bool sndp_hr_is_reading_ppg_enabled(void)
{
    return hr_ctx.ppg_reading_en;   
}

void sndp_hr_switch_reading_ppg(bool onoff)
{
    hr_ctx.ppg_reading_en = onoff;
    
#if defined(__SNDP_HRSENSOR_SUPPORT__)
    if(onoff) {
        sndp_hal_hr_set_reading_ppg_callback(sndp_hr_read_ppg_callback);
        sndp_hal_hr_set_report_ppg_raw_data_callback(sndp_report_ppg_raw_data_callback);
        sndp_hal_hr_start_reading_ppg();
    } else {
        sndp_hal_hr_stop_reading_ppg();
    }
#endif     
}

bool sndp_hr_is_reading_acc_enabled(void)
{
    return hr_ctx.acc_reading_en;   
}

void sndp_hr_switch_reading_acc_raw_data(bool onoff)
{
    hr_ctx.acc_reading_en = onoff;
    
#if defined(__SNDP_GSENSOR_SUPPORT__)
    if(onoff) {
        sndp_hal_acc_stop_single_tap_interrupt();
        sndp_hal_acc_set_reading_raw_data_callback(sndp_hr_acc_read_raw_data_callback);
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

    SNDP_TRACE(0, "...");

    ppg_raw_data_queue_reset();

    // hr_setp_1: 算法初始化
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)   
    dbbeats_initialize_heartrate_data(ppg_sampling_rate, dump_state);
#endif

    // hr_setp_2: 打开读取加速度数据
    sndp_hr_switch_reading_acc_raw_data(true);

    // hr_setp_3: 打开读取PPG数据
    sndp_hr_switch_reading_ppg(true);

    hr_measure_time = 0;
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;
    hr_ctx.hr_running = true;
    sndp_hr_mearsuring_set_sampling_rate(ppg_sampling_rate);
    sndp_hr_mearsuring_set_dump_state(dump_state);
}

void sndp_hr_mearsuring_stop(void)
{
    SNDP_TRACE(0, "...");
    
    // hr_setp_10: 停止处理
    hr_ctx.hr_running = false;
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;

    // hr_setp_11: 停止读取ppg数据
    sndp_hr_switch_reading_ppg(false);

    // hr_setp_12: 停止读取加速度数据
    sndp_hr_switch_reading_acc_raw_data(false);

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

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
}
// sleep analysis function
static void sndp_sleep_analysis(void) 
{
    SNDP_TRACE(0, "sleep analyse...");
    dbbeats_sleep_data(sleep_app_accel, sleep_screen_status, sleep_sound_state,\
        sleep_stage, &sleep_position, &sound_control, &result_code);
    // sleep_step_14: analysis result
    if (result_code == 1) {
        // SNDP_TRACE(0, "Sleep position: %d\n", sleep_position);
        // sleep_step_15: Use sleep_stage[0~39]

        // sleep_step_16: report data to app via ble.
        if(hr_ctx.sleep_running) {
            uint16_t position_and_control = (sleep_position & 0xFF) | ((sound_control & 0xFF)<<8);
            sndp_call_func_in_app_thread((uint32_t)sndp_comm_cmd_sleepapp_report_sleep_stage, (uint32_t)sleep_stage, position_and_control, result_code);
        }
    } else {
        SNDP_TRACE(0, "Error: %d\n", result_code);
    }
}

void sndp_sleep_analysis_start(int32_t sleep_control)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "...");
    
    ppg_raw_data_queue_reset();
    
    // sleep_step_1:算法初始化
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    dbbeats_initialize_sleep_data(sleep_control);
#endif

    // sleep_step_2:打开读取加速度数据。
    sndp_hr_switch_reading_acc_raw_data(true);   

    // sleep_step_3:打开读取PPG数据。
    sndp_hr_switch_reading_ppg(true);

    hr_ctx.hr_running = true;
    hr_ctx.sleep_running = true;
    hr_ctx.sleep_tracking = false;
}

void sndp_sleep_analysis_stop(void)
{
    SNDP_TRACE(0, "...");
    
    // hr_setp_17: 停止处理
    hr_ctx.hr_running = false;
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;

    // hr_setp_18: 停止读取ppg数据
    sndp_hr_switch_reading_ppg(false);

    // hr_setp_19: 停止读取加速度数据
    sndp_hr_switch_reading_acc_raw_data(false);

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

bool sndp_hr_is_ppg_notification_enabled(void)
{
    return hr_ctx.ppg_notification;   
}

void sndp_ppg_notification_start(uint8_t dump_state)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "...");
    
    ppg_raw_data_queue_reset();

    // sleep_step_1:停止读取ACC数据。
    sndp_hr_switch_reading_acc_raw_data(false);

    // sleep_step_2:打开读取PPG数据。
    sndp_hr_switch_reading_ppg(true);

    hr_ctx.hr_running = false;
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;
    hr_ctx.ppg_notification = true;
    hr_ctx.acc_notification = false;
    sndp_hr_mearsuring_set_dump_state(dump_state);
    memset(hr_ppg_raw_data, 0, sizeof(hr_ppg_raw_data));

}

void sndp_ppg_notification_stop(void)
{
    SNDP_TRACE(0, "...");
    
    // hr_setp_3: 停止处理
    hr_ctx.ppg_notification = false;
    
    // hr_setp_4: 停止读取ppg数据
    sndp_hr_switch_reading_ppg(false);
    sndp_hr_mearsuring_set_dump_state(0x00);
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

void sndp_acc_notification_start(uint8_t dump_state)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    SNDP_TRACE(0, "...");
    
    ppg_raw_data_queue_reset();

    // sleep_step_1:停止读取PPG数据。
    sndp_hr_switch_reading_ppg(false);

    // sleep_step_2:开始读取ACC数据。
    sndp_hr_switch_reading_acc_raw_data(true);

    hr_ctx.hr_running = false;
    hr_ctx.sleep_running = false;
    hr_ctx.sleep_tracking = false;
    hr_ctx.ppg_notification = false;
    hr_ctx.acc_notification = true;
    sndp_hr_mearsuring_set_dump_state(dump_state);
    memset(hr_acc_raw_data, 0, sizeof(hr_acc_raw_data));

}

void sndp_acc_notification_stop(void)
{
    SNDP_TRACE(0, "...");
    
    // hr_setp_3: 停止处理
    hr_ctx.acc_notification = false;
    sndp_hr_mearsuring_set_dump_state(0x00);
    // hr_setp_4: 停止读取ACC数据
    sndp_hr_switch_reading_acc_raw_data(false);

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}

void sndp_hr_ble_disconnected_delay10s_start(void)
{   
    SNDP_TRACE(0, "running: %d cover_closed: %d", hr_ctx.sleep_running, sndp_dev_cover_is_closed(false));
    if(hr_ctx.sleep_running) {
        if(sndp_dev_cover_is_closed(false)) { //关盖后断连直接关闭analysis
            sndp_sleep_comm_disconnect_timer_handler();
        }else {
            hr_ctx.delay_10S_start = true;
            sndp_delay_exec_start(10000, (uint32_t)sndp_sleep_comm_disconnect_timer_handler, 0, 0, 0);
        }
    } 
}

void sndp_hr_ble_connected_delay10s_stop(void)
{
    SNDP_TRACE(0, "Srunning: %d HRrunning: %d", hr_ctx.sleep_running, hr_ctx.hr_running);
    if(hr_ctx.sleep_running){
        hr_ctx.delay_10S_start = false;
        sndp_delay_exec_stop((uint32_t)sndp_sleep_comm_cmd_analysis_stop);        
    }
}

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
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

    // 2. 创建处理线程唤醒信号量。
    if (hr_process_wait_semaphore_id == NULL) {
        hr_process_wait_semaphore_id = osSemaphoreCreate(osSemaphore(hr_process_wait_semaphore), 0);
    }

    // 3. 创建处理线程。
    if (hr_process_thread_tid == NULL)  {
        hr_process_thread_tid = osThreadCreate(osThread(sndp_hr_process_thread), NULL);
        ASSERT(hr_process_thread_tid != NULL, "%s, line=%d", __func__, __LINE__);
    }

    memset(&hr_ctx, 0, sizeof(sndp_hr_ctx_s));

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    SNDP_TRACE(0, "lib_ver:%s", lib_engine_version());
    dbbeats_print_log_cfg(sndp_hr_print_log);
#endif

	HR_TRACE(0, "done");

}

#endif	/* __SNDP_HEART_RATE_MGR__ */

