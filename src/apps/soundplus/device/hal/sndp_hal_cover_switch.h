#ifndef __SNDP_HAL_COVER_SWITCH_H__
#define __SNDP_HAL_COVER_SWITCH_H__

#if defined(__SNDP_COVER_SWITCH_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SNDP_HAL_COVER_COLSED,
	SNDP_HAL_COVER_OPENED,
	SNDP_HAL_COVER_UNKNOWN,
} sndp_hal_cover_status_e;


typedef void (*sndp_hal_cs_status_changed_callback)(sndp_hal_cover_status_e status);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* set_status_changed_callback)(sndp_hal_cs_status_changed_callback callback);

	/** 
     * status: cover status, sndp_hal_cover_status_e
     * return: 0 no error.
     */
    int32_t (* get_curr_status)(sndp_hal_cover_status_e *status);

	/** 
     * return: 0 no error.
     */
    int32_t (* check_curr_status)(void);

} sndp_hal_cover_switch_s;

uint32_t sndp_hal_cover_switch_init(void);
uint32_t sndp_hal_cover_switch_set_status_changed_callback(sndp_hal_cs_status_changed_callback callback);
uint32_t sndp_hal_cover_switch_get_curr_status(sndp_hal_cs_status_changed_callback callback);
uint32_t sndp_hal_cover_switch_check_curr_status(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COVER_SWITCH_MGR__ */

#endif /* __SNDP_HAL_COVER_SWITCH_H__ */

