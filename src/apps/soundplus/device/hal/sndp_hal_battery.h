#ifndef __SNDP_HAL_BATTERY_H__
#define __SNDP_HAL_BATTERY_H__

#if defined(__SNDP_BATTERY_MGR__)


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SNDP_HAL_BAT_NOT_CHARGING,
	SNDP_HAL_BAT_CHARGING,
	SNDP_HAL_BAT_CHARGED_FULL,

} sndp_hal_bat_charging_status_e;

#if defined(__SNDP_COULOMETER_CW2015__)
typedef enum {
	SNDP_HAL_CW2015_CHIPMODE_NORMAL,
	SNDP_HAL_CW2015_CHIPMODE_SLEEP,
	SNDP_HAL_CW2015_CHIPMODE_QUICK_START,
	SNDP_HAL_CW2015_CHIPMODE_RESTART,

} sndp_hal_cw2015_chipmode_e;
#endif


typedef struct {
	uint16_t bat_volt;
	uint8_t bat_per;
	uint8_t bat_level;
	
} sndp_hal_bat_info_s;


typedef void (*sndp_hal_battery_measure_callback)(sndp_hal_bat_info_s bat_info);

typedef struct {
    /**
     * first: first call after power on.
     * charging: is it charging.
     * init_bat_per: The initialization value comes from the value saved in flash, 0xff is invalid
     * return: 0 initialization successed 
     */
    int32_t (* init)(sndp_hal_bat_charging_status_e charging_status, uint8_t init_bat_per);

    /** 
     * callback: This pointer will be called when the measurement is complete.
     * return: 0 no error.
     */
    int32_t (* set_measure_callback)(sndp_hal_battery_measure_callback callback);

	/**
     * charge_status: 0: not charge, 1:charging, 2:charged full
     * return: 0 no error. 
     */
    int32_t (* measure)(sndp_hal_bat_charging_status_e charging_status);

	
	/**
	 * charge_status: 0: not charge, 1:charging, 2:charged full
	 * return: 0 no error. 
	 */
	int32_t (* update_charging_status)(sndp_hal_bat_charging_status_e charging_status);
	

#if defined(__SNDP_COULOMETER_CW2015__)
	/**
	 * charge_status: 0: not charge, 1:charging, 2:charged full
	 * return: 0 no error. 
	 */
	int32_t (* cw2015_set_chipmode)(sndp_hal_cw2015_chipmode_e chipmode);
#endif

} sndp_hal_battery_s;


uint32_t sndp_hal_battery_init(sndp_hal_bat_charging_status_e charging_status, uint8_t init_bat_per);
uint32_t sndp_hal_battery_set_measure_callback(sndp_hal_battery_measure_callback callback);
uint32_t sndp_hal_battery_measure(sndp_hal_bat_charging_status_e charging_status);
uint32_t sndp_hal_battery_update_charging_status(sndp_hal_bat_charging_status_e charging_status);
#if defined(__SNDP_COULOMETER_CW2015__)
uint32_t sndp_hal_battery_cw2015_set_chipmode(sndp_hal_cw2015_chipmode_e chipmode);
#endif



#ifdef __cplusplus
}
#endif


#endif  /* __SNDP_BATTERY_MGR__ */
#endif /* __SNDP_HAL_BATTERY_H__ */


