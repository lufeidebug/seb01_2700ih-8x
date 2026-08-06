#ifndef __SNDP_HEART_RATE_H__
#define __SNDP_HEART_RATE_H__

#if defined(__SNDP_HEART_RATE_MGR__)



#ifdef __cplusplus
extern "C" {
#endif



#define __SNDP_HR_DEBUG__

#if defined(__SNDP_HR_DEBUG__)
#define HR_LOG_TAG                  "[HR]"
#define HR_TRACE(num, str, ...)     SNDP_TRACE(num, HR_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#define HR_DUMP					    DUMP8
#else
#define HR_TRACE(num, str, ...)
#define HR_DUMP
#endif

typedef struct {
    int16_t result_code;
    int8_t count;
    uint8_t is_contact;
    int8_t led_state;    
    int32_t pck_interval;
} sndp_hr_dbbeats_data;

typedef enum {
    SENSOR_OP_USER_PPG = 1 << 0,
    SENSOR_OP_USER_HR_PPG  = 1 << 1,
    SENSOR_OP_USER_SUSPEND_PPG = 1 << 2,
    SENSOR_OP_USER_PPG_MAX,
} sensor_ppg_op_user_e;

typedef enum {
    SENSOR_OP_USER_ACC = 1 << 0,    
    SENSOR_OP_USER_HR_ACC = 1 << 1,
    SENSOR_OP_USER_SUSPEND_ACC = 1 << 2,
    SENSOR_OP_USER_ACC_MAX,
} sensor_acc_op_user_e;

typedef enum {
    ACC_DUMP_STATE = 0,
    PPG_DUMP_STATE = 1,
    HR_DUMP_STATE = 2,
    DUMP_STATE_MAX,
} dump_state_e;
/**
 * @brief       Start heartrate mearsuring
 * @param[in]   ppg_sampling_rate  1:64Hz, 2:128Hz, 3:256Hz
 * @param[in]   dump_state  Dump Data enable, 0:OFF, 1:ON
 * @return      void
 */
void sndp_hr_mearsuring_start(int8_t ppg_sampling_rate, uint8_t dump_state);

/**
 * @brief       Stop heartrate mearsuring
 * @return      void
 */
void sndp_hr_mearsuring_stop(void);

/**
 * @brief       Get heartrate mearsuring sampling rate
 * @return      uint8_t
 */
uint8_t sndp_hr_mearsuring_get_sampling_rate(void);

/**
 * @brief       Set heartrate mearsuring sampling rate
 * @param[in]   sampling_rate  1:64Hz, 2:128Hz, 3:256Hz
 * @return      void
 */
void sndp_hr_mearsuring_set_sampling_rate(uint8_t sampling_rate);

/**
 * @brief       Get heartrate mearsuring dump state
 * @return      uint8_t
 */
uint8_t sndp_mearsuring_get_dump_state(dump_state_e dump_state);

/**
 * @brief       Set heartrate mearsuring dump state
 * @param[in]   onoff  1:on, 0:off
 * @return      void
 */
void sndp_mearsuring_set_dump_state(dump_state_e dump_state, uint8_t onoff);

/**
 * @brief       Start sleep analysis
 * @param[in]   sleep_control default:0
 * @return      void
 */
void sndp_sleep_analysis_start(int32_t sleep_control);

/**
 * @brief       Stop sleep analysis.
 * @return      void
 */
void sndp_sleep_analysis_stop(void);

/**
 * @brief       Start PPG notifiycation.
 * @return      void
 */
void sndp_ppg_notification_start(uint8_t dump_state);

/**
 * @brief       Stop PPG notifiycation.
 * @return      void
 */
void sndp_ppg_notification_stop(void);

/**
 * @brief       Start ACC notifiycation.
 * @return      void
 */
void sndp_acc_notification_start(uint8_t dump_state);

/**
 * @brief       Stop ACC notifiycation.
 * @return      void
 */
void sndp_acc_notification_stop(void);


/**
 * @brief       Heartrate app init.
 * @return      void
 */
void sndp_hr_app_init(void);

uint8_t sndp_hr_running_state(void);

void sndp_set_sleep_control(int32_t sleep_control);

int32_t sndp_get_sleep_control(void);

void sndp_dbbeats_put_sleep_sensor_data(void);

void sndp_dbbeats_put_sleep_app_data(int16_t accel_data_m[],
                                uint8_t screen_status[],
                                int8_t sound_state);

int32_t sndp_get_acc_notification(void);

bool sndp_hr_is_reading_ppg_enabled(void);
bool sndp_hr_is_reading_acc_enabled(void);

void sndp_hr_switch_reading_ppg_raw_data(uint32_t user, bool onoff);
void sndp_hr_switch_reading_acc_raw_data(uint32_t user, bool onoff);
void sndp_hr_ble_disconnected_delay10s_start(void);
void sndp_hr_ble_connected_delay10s_stop(void);

/* 统一任务轮询线程接口: IRQ/控制路径调用 */
void sndp_hr_notify_ppg_fifo_ready(void);
void sndp_hr_notify_acc_fifo_ready(void);
void sndp_hr_proximity_tick_enable(bool en);

/* 挂起/恢复HR与睡眠算法(取下/重新佩戴场景):
 * suspend关闭ACC传感器并暂停算法(状态保留), PPG无需管理(取下后底层自动断流);
 * resume按user需求恢复ACC并清空残留队列 */
void sndp_hr_suspend(void);
void sndp_hr_resume(void);
void sndp_sleep_analysis_resume(void);
void sndp_sleep_analysis_suspend(void);
#ifdef __cplusplus
	}
#endif

#endif	/* __SNDP_HEART_RATE_MGR__*/
#endif	/* __SNDP_HEART_RATE_H__*/

