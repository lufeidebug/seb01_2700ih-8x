#ifndef __SNDP_IF_DEVICE_H__
#define __SNDP_IF_DEVICE_H__

#if defined(__SNDP_PROJ__)



#ifdef __cplusplus
extern "C" {
#endif

#define SNDP_DEV_DEV_SN_LEN                 (20)

typedef enum {
	SNDP_DEV_WORKING_MODE_BT        = 0x00,
	SNDP_DEV_WORKING_MODE_SLEEP     = 0x01,
} sndp_dev_working_mode_e;
    

typedef enum {
	SNDP_DEV_EARSIDE_LEFT           = 0x00,
	SNDP_DEV_EARSIDE_RIGHT          = 0x01,	
	SNDP_DEV_EARSIDE_UNKNOWN        = 0xFF,	
	
} sndp_dev_earside_e;


typedef enum {
	SNDP_DEV_CHARGER_NOT_CHARGING,
	SNDP_DEV_CHARGER_CHARGING,
	SNDP_DEV_CHARGER_CHARGING_FULL,

} sndp_dev_charging_status_e;


typedef enum {
	SNDP_DEV_COVER_COLSED,
	SNDP_DEV_COVER_OPENED,
	SNDP_DEV_COVER_UNKNOWN,
} sndp_dev_cover_status_e;


typedef enum {
	SNDP_DEV_GESTURE_EVENT_NONE,
	SNDP_DEV_GESTURE_EVENT_PRESS_DOWN,
	SNDP_DEV_GESTURE_EVENT_PRESS_UP,
	SNDP_DEV_GESTURE_EVENT_1_CLICK,
	SNDP_DEV_GESTURE_EVENT_2_CLICK,
	SNDP_DEV_GESTURE_EVENT_3_CLICK,
	SNDP_DEV_GESTURE_EVENT_4_CLICK,
	SNDP_DEV_GESTURE_EVENT_5_CLICK,
	SNDP_DEV_GESTURE_EVENT_LONG_PRESS,
	SNDP_DEV_GESTURE_EVENT_LLONG_PRESS,
	SNDP_DEV_GESTURE_EVENT_REPEAT,
	SNDP_DEV_GESTURE_EVENT_SLIDE_UP,
	SNDP_DEV_GESTURE_EVENT_SLIDE_DOWN,

} sndp_dev_gesture_event_e;


typedef enum {
	SNDP_DEV_WEAR_OFF,
	SNDP_DEV_WEAR_ON,
	SNDP_DEV_WEAR_UNKNOWN,
} sndp_dev_wear_status_e;


typedef enum {
	SNDP_DEV_IOBOX_OUT,
	SNDP_DEV_IOBOX_IN,
	SNDP_DEV_IOBOX_UNKNOWN,
} sndp_dev_iobox_status_e;

typedef enum {
	SNDP_DEV_CHARGER_PLUG_IN,
	SNDP_DEV_CHARGER_PLUG_OUT,
	SNDP_DEV_CHARGER_PLUG_UNKNOWN,
} sndp_dev_charger_plug_e;


typedef enum {
	SNDP_DEV_KEY_CODE_NONE,
	SNDP_DEV_KEY_CODE_POWER_KEY,
	SNDP_DEV_KEY_CODE_FUNC_KEY,
	SNDP_DEV_KEY_CODE_VOLUME_INC_KEY,
	SNDP_DEV_KEY_CODE_VOLUME_DEC_KEY,
} sndp_dev_key_code_e;


typedef enum {
	SNDP_DEV_VOLUME_TYPE_MUSIC,
	SNDP_DEV_VOLUME_TYPE_CALL,	
	SNDP_DEV_VOLUME_TYPE_HEARAID,	
	
} sndp_dev_volume_type_e;


typedef struct {
	uint16_t bat_volt;
	uint8_t bat_per;
	uint8_t bat_level;
} sndp_dev_bat_info_s;


typedef struct {
	uint8_t fw_ver[4]; 
	uint8_t bt_addr[6];
	uint8_t ble_addr[6];
	
} sndp_dev_dev_info_s;


typedef struct {
	sndp_dev_bat_info_s bat_info;
	sndp_dev_iobox_status_e inout_status;
	sndp_dev_cover_status_e cover_status;
	sndp_dev_wear_status_e wear_status;
#if defined(__SNDP_ALG_APPLICATION__)
	uint8_t hearaid_status;		// 0:stop, 1:running
#endif	
} sndp_dev_all_status_s;



typedef struct {
	uint8_t fw_ver[4];
	uint8_t hw_ver[2];
	uint8_t bt_addr[6];
	uint8_t ble_addr[6];

	sndp_dev_earside_e ear_side;
	
	sndp_dev_bat_info_s bat_info;

	int16_t temperature;

    sndp_dev_charger_plug_e charger_status;
        
	sndp_dev_charging_status_e charging_status;

	sndp_dev_cover_status_e cover_status;

	sndp_dev_wear_status_e wear_status;

	sndp_dev_iobox_status_e inout_status;

} sndp_dev_earbuds_param_s;


typedef struct {
    uint8_t fw_ver[4];
    
    sndp_dev_charging_status_e charging_status;

	sndp_dev_bat_info_s bat_info;
	
	int16_t temperature;
	
} sndp_dev_box_param_s;



typedef struct {
    uint32_t working_mode;
    
	sndp_dev_earbuds_param_s local;
	sndp_dev_earbuds_param_s peer;
	sndp_dev_box_param_s box;

#if defined(__SNDP_TOUCH_CALI__)
	bool touch_calib_enable;
#endif	
} sndp_dev_context_s;



typedef void (*sndp_dev_bat_pwr_measure_cb)(sndp_dev_bat_info_s old_bat_info, sndp_dev_bat_info_s new_bat_info);
typedef void (*sndp_dev_temperature_measure_cb)(int16_t temperature);
typedef void (*sndp_dev_cover_status_changed_cb)(sndp_dev_cover_status_e cover_status);
typedef void (*sndp_dev_iobox_status_changed_cb)(sndp_dev_iobox_status_e inout_status);
typedef void (*sndp_dev_wear_status_changed_cb)(sndp_dev_wear_status_e wear_status);
typedef void (*sndp_dev_gesture_event_cb)(sndp_dev_gesture_event_e gesture_event);
typedef void (*sndp_dev_charger_plug_cb)(sndp_dev_charger_plug_e plug_status);


/************************************************** Wear Info Start **************************************************/
bool sndp_dev_wear_is_worn(bool peer);
sndp_dev_wear_status_e sndp_dev_wear_get_status(bool peer);
void sndp_dev_wear_set_status(bool peer, sndp_dev_wear_status_e wear_status);
void sndp_dev_wear_check_curr_status(void);
void sndp_dev_wear_enable_detection(void);
void sndp_dev_wear_disable_detection(void);
void sndp_dev_wear_status_changed_handler(sndp_dev_wear_status_e status);
void sndp_dev_wear_set_status_changed_callback(sndp_dev_wear_status_changed_cb callback);
void sndp_dev_wear_init(sndp_dev_wear_status_changed_cb callback);
/************************************************** Wear Info End **************************************************/


/************************************************** Gesture Info Start **************************************************/
void sndp_dev_gesture_set_event_callback(sndp_dev_gesture_event_cb callback);
void sndp_dev_gesture_init(sndp_dev_gesture_event_cb callback);

/************************************************** Gesture Info End **************************************************/


/************************************************** InOut Box Info Start **************************************************/
bool sndp_dev_iobox_is_in_box(bool peer);
bool sndp_dev_iobox_is_out_box(bool peer);
sndp_dev_iobox_status_e sndp_dev_iobox_get_status(bool peer);
void sndp_dev_iobox_set_status(bool peer, sndp_dev_iobox_status_e inout_status);
void sndp_dev_iobox_check_curr_status(void);
void sndp_dev_iobox_set_status_changed_callback(sndp_dev_iobox_status_changed_cb callback);
void sndp_dev_iobox_init(sndp_dev_iobox_status_changed_cb callback);

/************************************************** InOut Box Info End **************************************************/


/************************************************** Cover Switch Info Start **************************************************/
bool sndp_dev_cover_is_opened(bool peer);
bool sndp_dev_cover_is_closed(bool peer);
sndp_dev_cover_status_e sndp_dev_cover_get_status(bool peer);
void sndp_dev_cover_set_status(bool peer, sndp_dev_cover_status_e cover_status);
void sndp_dev_cover_check_curr_status(void);
void sndp_dev_cover_status_changed_handler(sndp_dev_cover_status_e status);
void sndp_dev_cover_set_status_changed_callback(sndp_dev_cover_status_changed_cb callback);
void sndp_dev_cover_init(sndp_dev_cover_status_changed_cb callback);

/************************************************** Cover Switch Info End **************************************************/


/************************************************** Temperature Info Start **************************************************/
int16_t sndp_dev_temperature_get_value(bool peer);
void sndp_dev_temperature_set_value(bool peer, int16_t temperature);
void sndp_dev_temperature_measure(void);
void sndp_dev_temperature_set_measure_callback(sndp_dev_temperature_measure_cb callback);
void sndp_dev_temperature_init(sndp_dev_temperature_measure_cb callback);

/************************************************** Temperature Info End **************************************************/

/************************************************** Charger Info Start **************************************************/
bool sndp_dev_charger_is_charging(bool peer);
bool sndp_dev_charger_is_charging_full(bool peer);
void sndp_dev_charger_set_charging_status(bool peer, sndp_dev_charging_status_e charging_status);
sndp_dev_charging_status_e sndp_dev_charger_get_charging_status(bool peer);
void sndp_dev_charger_set_charging_current(void);
void sndp_dev_charger_check_curr_status(void);
bool sndp_dev_charger_is_charging_enabled(void);
void sndp_dev_charger_init(void);
/************************************************** Charger Info End **************************************************/


/************************************************** PMU Info Start **************************************************/
bool sndp_dev_charger_is_plugin(bool peer);
void sndp_dev_charger_plug_set_status(bool peer, sndp_dev_charger_plug_e inout);
void sndp_dev_charger_plug_check_curr_status(void);
void sndp_dev_charger_plug_set_status_changed_callback(sndp_dev_charger_plug_cb callback);
void sndp_dev_charger_plug_init(sndp_dev_charger_plug_cb callback);

/************************************************** PMU Info End **************************************************/


/************************************************** Battery Info Start **************************************************/
uint8_t sndp_dev_get_bat_report_level(void);
uint8_t sndp_dev_get_bat_percentage(bool peer);
uint16_t sndp_dev_get_bat_voltage(bool peer);
uint8_t sndp_dev_get_bat_level(bool peer);
bool sndp_dev_get_bat_info(bool peer, sndp_dev_bat_info_s * bat_info);
void sndp_dev_set_bat_info(bool peer, sndp_dev_bat_info_s bat_info);
bool sndp_dev_get_box_bat_info(sndp_dev_bat_info_s *bat_info);
bool sndp_dev_set_box_bat_info(sndp_dev_bat_info_s bat_info);
sndp_dev_charging_status_e sndp_dev_get_box_charging_sta(void);
bool sndp_dev_set_box_charging_sta(sndp_dev_charging_status_e sta);
void sndp_dev_bat_pwr_measure(void);
void sndp_dev_bat_pwr_set_measure_callback(sndp_dev_bat_pwr_measure_cb callback);
void sndp_dev_bat_pwr_init(sndp_dev_bat_pwr_measure_cb callback);

/************************************************** Battery Info End **************************************************/


/************************************************** EarSid Info End **************************************************/
sndp_dev_earside_e sndp_dev_get_local_earside(void);
sndp_dev_earside_e sndp_dev_get_earside(bool peer);
bool sndp_dev_is_right_earphone(void);
bool sndp_dev_is_left_earphone(void);

/************************************************** EarSid Info End **************************************************/

/************************************************** Device Info Start **************************************************/
uint8_t *sndp_dev_get_fw_ver(bool peer);
char *sndp_dev_get_fw_ver_str(bool peer);
void sndp_dev_set_fw_ver(bool peer, uint8_t *fw_ver);
uint8_t *sndp_dev_get_hw_ver(bool peer);
char *sndp_dev_get_hw_ver_str(bool peer);
bool sndp_dev_set_hw_ver(bool peer, uint8_t *hw_ver);
uint8_t *sndp_dev_get_box_fw_ver(void);
bool sndp_dev_set_box_fw_ver(uint8_t *fw_ver);
uint8_t *sndp_dev_get_bt_addr(bool peer);
bool sndp_dev_set_bt_addr(bool peer, uint8_t *bt_addr);
uint8_t *sndp_dev_get_ble_addr(bool peer);
bool sndp_dev_set_ble_addr(bool peer, uint8_t *ble_addr);
char *sndp_dev_get_bt_name(void);
bool sndp_dev_modify_bt_name(uint8_t *name , uint16_t len);
uint8_t *sndp_dev_get_dev_sn(void);
bool sndp_dev_save_dev_sn(uint8_t *sn, uint16_t sn_len);
void sndp_dev_init_device_info(void);
void sndp_dev_clear_device_info(bool peer);
char *sndp_dev_get_dev_model_name(void);

/************************************************** Device Info End **************************************************/

/************************************************** Working Mode Start **************************************************/
void sndp_dev_set_working_mode(sndp_dev_working_mode_e mode);
uint32_t sndp_dev_get_working_mode(void);
bool sndp_dev_is_working_mode(sndp_dev_working_mode_e mode);
/**************************************************  Working Mode End **************************************************/

/************************************************** Heart rate Start **************************************************/
void sndp_dev_hr_enter_standby_mode(void);
void sndp_dev_hr_enter_detection_mode(void);
void sndp_dev_hr_init(void);
/**************************************************  Heart rate Start **************************************************/


/************************************************** acc Start **************************************************/
void sndp_dev_acc_enter_standby_mode(void);
void sndp_dev_acc_enter_detection_mode(void);
void sndp_dev_acc_init(void);
/**************************************************  acc End **************************************************/

void sndp_dev_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */
#endif /* __SNDP_IF_DEVICE_H__ */
