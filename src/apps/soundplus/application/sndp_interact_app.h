#ifndef __SNDP_INTERACT_APP_H__
#define __SNDP_INTERACT_APP_H__

#if defined(__SNDP_PROJ__)



#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_INTERACT_APP_TRACE__

#if defined(__SNDP_INTERACT_APP_TRACE__)
#define INTERACT_APP_TRACE(num, str, ...)       SNDP_TRACE(1 + num,	"[INTERACT_APP] %s, " str, __func__, ##__VA_ARGS__)
#else
#define INTERACT_APP_TRACE(num, str, ...)
#endif

#if defined(__SNDP_SLEEP_APP__)
int32_t sleep_comm_execute_cmd_hdlr(sleep_app_comm_cmd_info_s *cmd);
typedef uint32_t (*sleep_app_comm_cmd_exec_func)(sleep_app_comm_cmd_info_s *cmd_info);

typedef struct {
	uint8_t cmd_id;
	const char *name;
	sleep_app_comm_cmd_exec_func cmd_exec_hdlr;
} sndp_sleep_comm_cmd_handle_s;

typedef enum {
    SLEEP_APP_CMDID_SET_EQ_MODE = 0x02, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_MODE = 0x03, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + dev status(1). */
    SLEEP_APP_CMDID_SET_EQ_PARAM = 0x04, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_PARAM = 0x05, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 2 bytes, error code(1) + param value(1). */ 
    SLEEP_APP_CMDID_FIND_MY_EARPHONE = 0x06, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */   
    SLEEP_APP_CMDID_SET_ANC_MODE = 0x07, /* recv: 1 bytes, anc mode(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ANC_MODE = 0x08, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + anc mode(1). */
    SLEEP_APP_CMDID_PPG_SETING = 0x0A, /* recv: 1 bytes, ppg setting(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_NOTIFICATION = 0x0B, /* recv: 1 bytes, ppg notification(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION = 0x0C, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION = 0x0D, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_BATTERY_STATUS = 0x0E, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_DEVICE_INFO = 0x0F, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_ENABLE = 0x1A, /* recv: 1 bytes, touch enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_VOICE_PROMPT_ENABLE = 0x1B, /* recv: 1 bytes, voice prompt enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_KEY_MAPPING = 0x17, /* recv: 1 bytes, touch key mapping(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_TOUCH_KEY_MAPPING = 0x18, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SMART_PLAY_PAUSE = 0x29, /* recv: 1 bytes, smart play/pause(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_SMART_PLAY_PAUSE = 0x2A, /* recv: 1 bytes, smart play/pause(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SETTINGS = 0x1E, /* recv: 1 bytes, settings(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_SETTINGS = 0x19, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_CONTROL = 0xFF, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_EARBUDS_STATUS_LED = 0xFE, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_AUTO_LED_ENABLE_DISABLE = 0x13, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_HEARTRATE = 0x30, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING = 0x31, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING_WITH_DUMP = 0x32, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_HEARTRATE = 0x33, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_SLEEP = 0x34, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SLEEP_TRACKING = 0x35, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_SLEEP = 0x36, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_WEAR_STATE_UPDATE = 0x62, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GESTURE_EVENT_UPDATE = 0x38, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_TAP_UPDATE = 0x39, /* recv: 0 bytes. 
                                                rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_NOTIFICATION_DEBUG = 0x50, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION_DEBUG = 0x51, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_TEST = 0x60, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_SAMPLE_RATE_REPORT = 0x61, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_RESUME_HEART_RATE = 0x37, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
} sleep_app_cmd_id_e;

typedef enum {
    DEVICE_INFO_TAG_BT_NAME = 0x01,
    DEVICE_INFO_TAG_SN      = 0x02,
    DEVICE_INFO_TAG_FW_VER  = 0x03,
    DEVICE_INFO_TAG_HW_VER  = 0x04,
    DEVICE_INFO_TAG_ALGO_VER = 0x05,
} device_info_tag_e;

typedef struct
{
   // Byte0
    uint8_t proximity_off : 1;    // BIT0
    uint8_t proximity_on  : 1;     // BIT1
    uint8_t ppg_256hz    : 1;     // BIT2
    uint8_t ppg_128hz    : 1;     // BIT3
    uint8_t ppg_64hz     : 1;     // BIT4
    uint8_t ppg_off      : 1;     // BIT5
    uint8_t accel_off    : 1;     // BIT6
    uint8_t accel_on     : 1;     // BIT7
    
    // Byte1
    uint8_t anc_transparent  : 1;  // BIT0
    uint8_t anc_adaptive     : 1;  // BIT1
    uint8_t anc_strong       : 1;  // BIT2
    uint8_t anc_off          : 1;  // BIT3
    uint8_t voice_disable    : 1;  // BIT4
    uint8_t voice_enable     : 1;  // BIT5
    uint8_t touch_disable    : 1;  // BIT6
    uint8_t touch_enable     : 1;  // BIT7
    
    // Byte2
    uint8_t reserved0    : 1;      // BIT0
    uint8_t reserved1    : 1;      // BIT1
    uint8_t music_pause  : 1;      // BIT2
    uint8_t music_play   : 1;      // BIT3
    uint8_t smart_playpause_disable : 1;  // BIT4
    uint8_t smart_playpause_enable  : 1;  // BIT5
    uint8_t ppgledpwr_auto  : 1;      // BIT6
    uint8_t ppgledpwr_manual : 1;    // BIT7
}SndpSettingsBitMap_t;

typedef struct{
    uint8_t reserve;
    uint8_t sensor_select;
    uint8_t write_read;
    uint8_t read_lenth;
    uint8_t reg_addr;
    uint8_t write_value;
}SndpSensorCtrlMap_t;

typedef struct{
    uint8_t receive_status;
    uint8_t reg_addr;
    uint8_t select_sensor;
    uint8_t write_read; //read only
    uint8_t read_lenth;
    uint8_t read_value[3];
}SndpSensorCtrlReplyMap_t;

typedef union
{
    struct 
    {
        //byte0
        uint8_t left_battery_level : 7;
        //uint8_t left_charging_statu : 1;
        uint8_t left_iobox_statu : 1;

        //byte1
        uint8_t right_battery_level : 7;
        //uint8_t right_charging_statu : 1;
        uint8_t right_iobox_statu : 1;

        //byte2
        uint8_t cradle_battery_level : 7;
        uint8_t cradle_charging_status : 1;
    } bits;

    uint8_t charging_byte[3];
}SndpGetBattryMap_t;


uint32_t sndp_comm_cmd_sleepapp_report_hr(uint8_t* sendhr, uint8_t* dbbeats_data);
uint32_t sndp_comm_cmd_send_lr_sync_sleep_role_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_heart_rate_onoff(uint8_t onoff);
uint32_t sndp_comm_cmd_send_lr_sync_stage_onoff(uint8_t onoff);
uint32_t sndp_comm_cmd_sleepapp_report_sleep_stage(int8_t *sleep_stage,
                                                    uint16_t position_and_control,
                                                    int16_t result_code);
uint32_t sndp_comm_cmd_sleepapp_report_ppg_raw_data(uint8_t *ppg_raw_data, uint16_t ppg_raw_len);
uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf(int16_t *acc_raw_data, uint16_t acc_raw_len);
uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf_debug(int16_t *acc_raw_data, uint16_t acc_raw_len);
uint32_t sndp_comm_cmd_sleepapp_wear_state_update(uint8_t lR_flag, uint8_t wear_state);
uint8_t sndp_get_findme_vol(void);
void sndp_sleep_app_report_battery(void);
void sndp_comm_cmd_sleepapp_report_acc_samples(uint16_t sensor_samples);
void sndp_comm_cmd_sleepapp_report_ppg_samples(uint16_t sensor_samples);
void sndp_sleep_comm_cmd_analysis_stop(void);
void sndp_sleep_comm_cmd_heartrate_stop(void);
void sndp_sleep_comm_disconnect_timer_handler(void);
uint32_t sndp_comm_cmd_sleepapp_proximity_task(void);
#ifdef __SNDP_SEND_GESTURE__
void sndp_sleep_app_report_gesture(uint32_t gesture);
void sndp_sleep_app_report_tap(void);
#endif

uint32_t sndp_sleep_app_report_anc_mode(void);
uint32_t sndp_comm_cmd_sleepapp_report_proximity_to_app(void);


#if defined(__SNDP_GESTURE_MAP__)
typedef struct{
    uint8_t ear_side;
    uint8_t key_behavior;
    uint8_t key_function;
}gesture_map_t;

#endif
#endif

int32_t sndp_interact_app_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */
#endif /* __SNDP_INTERACT_APP_H__ */

