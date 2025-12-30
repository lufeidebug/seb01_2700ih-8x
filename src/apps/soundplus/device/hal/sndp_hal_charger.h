#ifndef __SNDP_HAL_CHARGER_H__
#define __SNDP_HAL_CHARGER_H__

#if defined(__SNDP_CHARGER_MGR__)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SNDP_HAL_CHARGER_MODE_STANDBY           = 0x01,
	SNDP_HAL_CHARGER_MODE_SHIPMODE          = 0x02,
	SNDP_HAL_CHARGER_MODE_COMMUNICATION     = 0x03,
	SNDP_HAL_CHARGER_MODE_NOT_CHARGING      = 0x04,
	SNDP_HAL_CHARGER_MODE_TRICKLE_CHARGING  = 0x05,
	SNDP_HAL_CHARGER_MODE_CC_CHARGING       = 0x06,
	SNDP_HAL_CHARGER_MODE_CV_CHARGING       = 0x07,
	SNDP_HAL_CHARGER_MODE_FULL_CHARGING     = 0x08,
	
} sndp_hal_charger_mode_e;


typedef enum {
	SNDP_HAL_CHARGING_CURRENT_ZERO,
	SNDP_HAL_CHARGING_CURRENT_SLOW,
	SNDP_HAL_CHARGING_CURRENT_HIGH,
	SNDP_HAL_CHARGING_CURRENT_HALF,
	SNDP_HAL_CHARGING_CURRENT_1C,
	SNDP_HAL_CHARGING_CURRENT_2C,
	SNDP_HAL_CHARGING_CURRENT_3C,
	
} sndp_hal_charging_current_e;


typedef void (*sndp_hal_charging_mode_changed_callback)(sndp_hal_charger_mode_e mode);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /**
     * charging_current: sndp_hal_charging_current_e
     * return: 0 no error. 
     */
    int32_t (* set_charging_current)(sndp_hal_charging_current_e charging_current);

    /** 
     * callback: This pointer will be called when charging status changed.
     * return: 0 no error.
     */
    int32_t (* set_charging_mode_changed_callback)(sndp_hal_charging_mode_changed_callback callback);

    /** 
     * return: 0 no error.
     */
    int32_t (* check_curr_status)(void);

	/**
     * charging_current: sndp_hal_charging_current_e
     * return: 0 no error. 
     */
    bool (* is_charging_enabled)(void);

} sndp_hal_charger_s;

int32_t sndp_hal_charger_init(void);
int32_t sndp_hal_charger_set_charging_current(sndp_hal_charging_current_e charging_current);
int32_t sndp_hal_charger_set_charging_mode_changed_callback(sndp_hal_charging_mode_changed_callback callback);
int32_t sndp_hal_charger_check_curr_status(void);
bool sndp_hal_charger_is_charging_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_CHARGER_MGR__ */
#endif /* __SNDP_HAL_CHARGER_H__ */

