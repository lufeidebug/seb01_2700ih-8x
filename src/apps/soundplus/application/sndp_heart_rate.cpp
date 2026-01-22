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


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool hr_running;
    bool sleep_running;
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


#define HR_PROCESS_THREAD_STACK_SIZE 				(1024*2)
static void sndp_hr_process_thread(void const *argument);
osThreadDef(sndp_hr_process_thread, osPriorityNormal, 1, HR_PROCESS_THREAD_STACK_SIZE, "hr_process_thread");
osThreadId hr_process_thread_tid = NULL;

osSemaphoreId hr_process_wait_semaphore_id = NULL;
osSemaphoreDef(hr_process_wait_semaphore);

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
POSSIBLY_UNUSED static struct Dump debug_dump;
#endif

POSSIBLY_UNUSED static int32_t hr_ppg_raw_data[HR_PPG_SECOND_ALLCH_SAMPLES];
POSSIBLY_UNUSED static int16_t hr_acc_raw_data[HR_ACC_SECOND_ALLCH_SAMPLES];
POSSIBLY_UNUSED static int8_t hr_dev_state[HR_DEV_SECOND_ALLCH_SAMPLES];

POSSIBLY_UNUSED static int16_t sleep_acc_raw_data[SLEEP_ACC_DATA_SAMPLES]; //11250 = 125*3*30(s)
POSSIBLY_UNUSED static uint32_t sleep_acc_raw_data_len = 0;
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
POSSIBLY_UNUSED static struct SleepHrvIndices sleep_hrv_data;
#endif
POSSIBLY_UNUSED static uint16_t sleep_hrv_data_len = 0;

POSSIBLY_UNUSED static int8_t sleep_dev_state[SLEEP_DEV_DATA_SAMPLES]; //150 = 5 * 30(s)
POSSIBLY_UNUSED static uint8_t sleep_is_contact[SLEEP_CALC_SECONDS];

POSSIBLY_UNUSED static int16_t sleep_app_accel[90];
POSSIBLY_UNUSED static uint8_t sleep_screen_status[30];


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

static int ppg_raw_data_queue_pop_data(int32_t *item, int cnt)
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

static int acc_raw_data_queue_pop_data(int16_t *item, int cnt)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&acc_raw_data_queue, (CQItemType *)item, sizeof(int16_t) * cnt);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    return ret;
}

static int acc_raw_data_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(acc_raw_data_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&acc_raw_data_queue);
    osMutexRelease(acc_raw_data_queue_mutex_id);

    queue_len = queue_len / sizeof(int16_t);
    return queue_len;
}


static void sndp_hr_process_thread(void const *argument)
{
    // Return results
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)    
    POSSIBLY_UNUSED struct HrvIndices hrv;
#endif
    POSSIBLY_UNUSED int16_t result_code;
    POSSIBLY_UNUSED int8_t count;
    POSSIBLY_UNUSED int8_t led;
    POSSIBLY_UNUSED int acc_queue_len;
    POSSIBLY_UNUSED int acc_data_len;
    

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

        // hr_setp_7: Input data
        // sleep_step_7: Input data
        dbbeats_put_heartrate_data(
                hr_acc_raw_data, 
                hr_ppg_raw_data, 
                hr_dev_state, 
                1, 50, 
                HR_ACC_SECOND_ALLCH_SAMPLES, 
                HR_PPG_SECOND_ALLCH_SAMPLES, 
                HR_DEV_SECOND_ALLCH_SAMPLES, 
                1000);

        // hr_setp_8: Return results
        // sleep_step_8: Return results
        dbbeats_get_heartrate_data(&hrv, &result_code, &count, &led, &debug_dump);

        // hr_setp_9: Display hr results
        // sleep_step_9: Display hr results
        if(result_code == 1 && hrv.HR > 1) {
            HR_TRACE(0, "HR: %d BPM, SDNN: %d ms", hrv.HR, hrv.SDNN);
        } else if (result_code == 101) {
            HR_TRACE(0, "Sensor detached");
        }


        // hr_setp_10: Report results
        if(hr_ctx.hr_running) {

        }
        
        
        if(hr_ctx.sleep_running) {
            HR_TRACE(0, "sleep analyse...");
            
            // sleep_step_10: 把每秒的加速度传感器数据填充到缓存中。
            for(int32_t i = 0; i < HR_ACC_SECOND_ALLCH_SAMPLES && sleep_acc_raw_data_len < SLEEP_ACC_DATA_SAMPLES; i++) {
                sleep_acc_raw_data[sleep_acc_raw_data_len++] = hr_acc_raw_data[i];
            }

            // sleep_step_11: 把每秒经过心率算法计算后的hrv数据保存到 sleep_hrv_data中。
            if(sleep_hrv_data_len < SLEEP_CALC_SECONDS) {
                sleep_hrv_data.HR[sleep_hrv_data_len] = hrv.HR;
                sleep_hrv_data.SDNN[sleep_hrv_data_len] = hrv.SDNN;
                sleep_hrv_data_len++;
            }

            //缓存满30秒数据，处理睡眠数据。
            if(sleep_hrv_data_len >= SLEEP_CALC_SECONDS) {
                
                // sleep_step_13: Input sensor data
                dbbeats_put_sleep_sensor_data(
                        sleep_acc_raw_data, 
                        sleep_hrv_data, 
                        sleep_dev_state, 
                        sleep_is_contact, 
                        11250, 30, 150, 30);
                
                sleep_acc_raw_data_len = 0;

                // sleep_step_14: input app data
                dbbeats_put_sleep_app_data(
                    sleep_app_accel, 
                    sleep_screen_status, 
                    0);
                
            }
        }
#else
        HR_TRACE(0, "hr is processing ...");
#endif

    }

}

void sndp_hr_mearsuring_start(void)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_104M);
    
    // hr_setp_1: 算法初始化
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)    
    dbbeats_initialize_heartrate_data(1, 0);
#endif

    // hr_setp_2: 打开读取加速度数据
#if defined(__SNDP_GSENSOR_SUPPORT__)
    sndp_hal_acc_start_reading_raw_data();
#endif

    // hr_setp_3: 打开读取PPG数据
#if 0//defined(__SNDP_HRSENSOR_SUPPORT__)
    sndp_hal_hr_start_reading_ppg();
#endif 

    hr_ctx.hr_running = true;
}

void sndp_hr_mearsuring_stop(void)
{
    // hr_setp_10: 停止处理
    hr_ctx.hr_running = false;
    
    // hr_setp_11: 停止读取ppg数据
#if 0//defined(__SNDP_HRSENSOR_SUPPORT__)
    sndp_hal_hr_stop_reading_ppg();
#endif

    // hr_setp_12: 停止读取加速度数据
#if defined(__SNDP_GSENSOR_SUPPORT__)
    sndp_hal_acc_stop_reading_raw_data();
#endif

    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_HR_PROCESS, APP_SYSFREQ_32K);

}


// Define callback function
void sndp_sleep_analysis_callback(int8_t *sleep_stage,
                   int8_t sleep_position,
                   int8_t sound_control,
                   int16_t result_code) 
{
    // sleep_step_14: analysis result
    if (result_code == 1) {
        HR_TRACE(0, "Sleep position: %d\n", sleep_position);
        // sleep_step_15: Use sleep_stage[0~39]

        // sleep_step_16: report data to app via ble.
        
    } else {
        HR_TRACE(0, "Error: %d\n", result_code);
    }
}

/*
 * 睡眠分析处理函数。
 * sensor data: 从加速度传感器IC获取。
 * app data: app通过ble没30秒发送1次。
 * 处理后会调用 sndp_sleep_analysis_callback 函数。
 */
void sndp_sleep_analysis_process(void)
{
    
    
}

void sndp_sleep_analysis_start(void)
{
    sleep_acc_raw_data_len = 0;
    sleep_hrv_data_len = 0;

    // sleep_step_1:算法初始化
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    dbbeats_initialize_sleep_data(0, sndp_sleep_analysis_callback);
#endif

    // sleep_step_2:打开读取加速度数据。
#if defined(__SNDP_GSENSOR_SUPPORT__)
    sndp_hal_acc_stop_reading_raw_data();
#endif    

    // sleep_step_3:打开读取PPG数据。
#if defined(__SNDP_HRSENSOR_SUPPORT__)
    sndp_hal_hr_start_reading_ppg();
#endif

    hr_ctx.sleep_running = true;
    
}

void sndp_sleep_analysis_stop(void)
{
    // hr_setp_17: 停止处理
    hr_ctx.sleep_running = false;
    
    // hr_setp_18: 停止读取ppg数据
#if defined(__SNDP_HRSENSOR_SUPPORT__)
    sndp_hal_hr_stop_reading_ppg();
#endif

    // hr_setp_19: 停止读取加速度数据
#if defined(__SNDP_GSENSOR_SUPPORT__)
    sndp_hal_acc_stop_reading_raw_data();
#endif

}

#if defined(__SNDP_HEART_RATE_MGR__)
static void sndp_hr_read_ppg_callback(int32_t *data, uint16_t cnt)
{
    //HR_TRACE(0, "cnt=%d", cnt);
    if(hr_ctx.hr_running || hr_ctx.sleep_running) {
        ppg_raw_data_queue_push_data(data, cnt);
        if(ppg_raw_data_queue_get_len() >= HR_PPG_SECOND_ALLCH_SAMPLES) {
            //HR_TRACE(0, "wakeup thread");
            osSemaphoreRelease(hr_process_wait_semaphore_id);
        }
    }
}
#endif 


#if defined(__SNDP_GSENSOR_SUPPORT__)
static void sndp_hr_acc_read_raw_data_callback(sndp_hal_acc_data_s *data, uint16_t cnt)
{
    //HR_TRACE(0, "cnt=%d", cnt);
    if(hr_ctx.hr_running || hr_ctx.sleep_running) {
        acc_raw_data_queue_push_data((int16_t *)data, cnt * 3);
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

    // 4. 设置读取心率IC数据回调。
#if defined(__SNDP_HEART_RATE_MGR__)
    sndp_hal_hr_set_reading_ppg_callback(sndp_hr_read_ppg_callback);
#endif   

    // 5. 设置读取加速度IC数据回调。
#if defined(__SNDP_GSENSOR_SUPPORT__)
    sndp_hal_acc_set_reading_raw_data_callback(sndp_hr_acc_read_raw_data_callback);
#endif 

	HR_TRACE(0, ".");

}

#endif	/* __SNDP_HEART_RATE_MGR__ */

