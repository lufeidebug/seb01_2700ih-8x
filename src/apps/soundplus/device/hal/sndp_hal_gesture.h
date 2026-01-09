#ifndef __SNDP_HAL_GESTURE_H__
#define __SNDP_HAL_GESTURE_H__

#if defined(__SNDP_GESTURE_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SNDP_HAL_GESTURE_EVENT_NONE,
	SNDP_HAL_GESTURE_EVENT_PRESS_DOWN,
	SNDP_HAL_GESTURE_EVENT_PRESS_UP,
	SNDP_HAL_GESTURE_EVENT_1_CLICK,
	SNDP_HAL_GESTURE_EVENT_2_CLICK,
	SNDP_HAL_GESTURE_EVENT_3_CLICK,
	SNDP_HAL_GESTURE_EVENT_4_CLICK,
	SNDP_HAL_GESTURE_EVENT_5_CLICK,
	SNDP_HAL_GESTURE_EVENT_LONG_PRESS,
	SNDP_HAL_GESTURE_EVENT_LLONG_PRESS,
	SNDP_HAL_GESTURE_EVENT_REPEAT,
	SNDP_HAL_GESTURE_EVENT_SLIDE_UP,
	SNDP_HAL_GESTURE_EVENT_SLIDE_DOWN,

} sndp_hal_gesture_event_e;


typedef void (*sndp_hal_gesture_event_callback)(sndp_hal_gesture_event_e event);
typedef void (*sndp_hal_gesture_calibration_send_data_func)(uint8_t *data, uint16_t data_len);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* set_event_callback)(sndp_hal_gesture_event_callback callback);


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
    int32_t (* set_calibration_send_data_func)(sndp_hal_gesture_calibration_send_data_func func);


	/** 
     * return: 0 no error.
     */
    int32_t (* recv_calibration_data)(uint8_t *data, uint16_t data_len);

	/** 
     * return: 0 no error.
     */
    int32_t (* exec_calibration_self_calib)(void);


} sndp_hal_gesture_s;

int32_t sndp_hal_gesture_init(void);
int32_t sndp_hal_gesture_set_event_callback(sndp_hal_gesture_event_callback callback);
int32_t sndp_hal_gesture_enter_standby_mode(void);
int32_t sndp_hal_gesture_enter_detection_mode(void);
int32_t sndp_hal_gesture_set_event_callback(sndp_hal_gesture_event_callback callback);
int32_t sndp_hal_gesture_enter_standby_mode(void);
int32_t sndp_hal_gesture_set_calibration_send_data_func(sndp_hal_gesture_calibration_send_data_func func);
int32_t sndp_hal_gesture_recv_calibration_data(uint8_t *data, uint16_t data_len);
int32_t sndp_hal_gesture_exec_calibration_self_calib(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_GESTURE_MGR__ */

#endif /* __SNDP_HAL_GESTURE_H__ */
