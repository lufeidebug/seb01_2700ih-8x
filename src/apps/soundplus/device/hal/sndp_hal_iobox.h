#ifndef __SNDP_HAL_INOUT_BOX_H__
#define __SNDP_HAL_INOUT_BOX_H__

#if defined(__SNDP_IOBOX_MGR__)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SNDP_HAL_IOBOX_OUT,
	SNDP_HAL_IOBOX_IN,
	SNDP_HAL_IOBOX_UNKNOWN,
} sndp_hal_iobox_status_e;


typedef void (*sndp_hal_iobox_status_changed_callback)(sndp_hal_iobox_status_e status);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* set_status_changed_callback)(sndp_hal_iobox_status_changed_callback callback);


	/** 
     * status: cover status, sndp_hal_cover_status_e
     * return: 0 no error.
     */
    int32_t (* get_curr_status)(sndp_hal_iobox_status_e *status);

	/** 
     * status: cover status, sndp_hal_cover_status_e
     * return: 0 no error.
     */
    int32_t (* check_curr_status)(void);
	
} sndp_hal_iobox_s;

uint32_t sndp_hal_iobox_init(void);
uint32_t sndp_hal_iobox_set_status_changed_callback(sndp_hal_iobox_status_changed_callback callback);
uint32_t sndp_hal_iobox_get_curr_status(sndp_hal_iobox_status_e *status);
uint32_t sndp_hal_iobox_check_curr_status(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_INOUT_BOX_MGR__ */

#endif /* __SNDP_HAL_INOUT_BOX_H__ */

