#ifndef __SNDP_HAL_WEAR_DETECT_H__
#define __SNDP_HAL_WEAR_DETECT_H__

#if defined(__SNDP_WEAR_DETECT_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SNDP_HAL_WEAR_OFF,
	SNDP_HAL_WEAR_ON,
	

} sndp_hal_wear_status_e;


typedef void (*sndp_hal_wear_status_changed_callback)(sndp_hal_wear_status_e status);
typedef int32_t (*sndp_hal_wear_calibration_send_data_func)(uint8_t *data, uint16_t data_len);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* set_wear_status_changed_callback)(sndp_hal_wear_status_changed_callback callback);

	/** 
     * status: wear status, sndp_hal_wear_status_e
     * return: 0 no error.
     */
    int32_t (* get_curr_status)(sndp_hal_wear_status_e *status);

	/** 
	 * the status will be returned by calling the sndp_hal_wear_status_changed_callback
     * return: 0 no error.
     */
    int32_t (* check_curr_status)(void);

	/** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* enter_standby_mode)(void);

	/** 
     * return: 0 no error.
     */
    int32_t (* enter_detection_mode)(void);

	/** 
     * return: 0 no error.
     */
    int32_t (* set_calibration_send_data_func)(sndp_hal_wear_calibration_send_data_func func);


	/** 
     * return: 0 no error.
     */
    int32_t (* recv_calibration_data)(uint8_t *data, uint16_t data_len);

	/** 
     * return: 0 no error.
     */
    int32_t (* exec_calibration_self_calib)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* read_chip_id)(uint16_t *chip_id);

} sndp_hal_wear_detection_s;

int32_t sndp_hal_wear_detection_init(void);
int32_t sndp_hal_wear_detection_set_wear_status_changed_callback(sndp_hal_wear_status_changed_callback callback);
int32_t sndp_hal_wear_detection_get_curr_status(sndp_hal_wear_status_e *status);
int32_t sndp_hal_wear_detection_check_curr_status(void);
int32_t sndp_hal_wear_detection_enter_standby_mode(void);
int32_t sndp_hal_wear_detection_enter_detection_mode(void);
int32_t sndp_hal_wear_detection_set_calibration_send_data_func(sndp_hal_wear_calibration_send_data_func func);
int32_t sndp_hal_wear_detection_recv_calibration_data(uint8_t *data, uint16_t data_len);
int32_t sndp_hal_wear_detection_exec_calibration_self_calib(void);
int32_t sndp_hal_wear_detection_read_chip_id(uint16_t *chip_id);

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_WEAR_DETECT_MGR__ */

#endif /* __SNDP_HAL_WEAR_DETECT_H__ */
