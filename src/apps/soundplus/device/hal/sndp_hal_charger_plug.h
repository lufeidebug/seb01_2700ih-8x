#ifndef __SNDP_HAL_CHARGER_PLUG_H__
#define __SNDP_HAL_CHARGER_PLUG_H__

#if defined(__SNDP_CHARGER_PLUG_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SNDP_HAL_CHARGER_PLUGIN,
	SNDP_HAL_CHARGER_PLUGOUT,

	SNDP_HAL_CHARGER_UNKNOWN,
} sndp_hal_charger_plug_status_e;


typedef void (*sndp_hal_charger_plug_status_changed_callback)(sndp_hal_charger_plug_status_e status);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* set_status_changed_callback)(sndp_hal_charger_plug_status_changed_callback callback);


	/** 
     * return: 0 no error.
     */
    int32_t (* get_curr_status)(sndp_hal_charger_plug_status_e *status);

	/** 
     * status: charger plug status
     * return: 0 no error.
     */
    int32_t (* check_curr_status)(void);
	
} sndp_hal_charger_plug_s;

uint32_t sndp_hal_charger_plug_init(void);
uint32_t sndp_hal_charger_plug_set_status_changed_callback(sndp_hal_charger_plug_status_changed_callback callback);
uint32_t sndp_hal_charger_plug_get_curr_status(sndp_hal_charger_plug_status_e *status);
uint32_t sndp_hal_charger_plug_check_curr_status(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_CHARGER_PLUG_MGR__ */

#endif /* __SNDP_HAL_CHARGER_PLUG_H__ */
