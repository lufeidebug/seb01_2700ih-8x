#ifndef __SNDP_HAL_HR_H__
#define __SNDP_HAL_HR_H__


#if defined(__SNDP_HRSENSOR_SUPPORT__)

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*sndp_hal_hr_read_ppg_callback)(int32_t *data, uint16_t cnt);
typedef void (*sndp_hal_hr_calib_callback)(uint8_t *data, uint16_t data_len);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

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
    int32_t (* set_reading_ppg_callback)(sndp_hal_hr_read_ppg_callback callback);
    
	/** 
     * return: 0 no error.
     */
    int32_t (* start_reading_ppg)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* stop_reading_ppg)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* set_calib_callback)(sndp_hal_hr_calib_callback callback);

    /** 
     * return: 0 no error.
     */
    int32_t (* exec_calib)(uint8_t *data, uint16_t data_len);

	/** 
     * return: 0 no error.
     */
    int32_t (* exec_self_calib)(void);

    /** 
     * return: 0 no error.
     */
    int32_t (* read_proximity_value)(unsigned short* value);

} sndp_hal_hr_s;


int32_t sndp_hal_hr_init(void);
int32_t sndp_hal_hr_enter_standby_mode(void);
int32_t sndp_hal_hr_enter_detection_mode(void);

int32_t sndp_hal_hr_set_reading_ppg_callback(sndp_hal_hr_read_ppg_callback callback);
int32_t sndp_hal_hr_start_reading_ppg(void);
int32_t sndp_hal_hr_stop_reading_ppg(void);

int32_t sndp_hal_hr_set_calib_callback(sndp_hal_hr_calib_callback callback);
int32_t sndp_hal_hr_exec_calib(uint8_t *data, uint16_t data_len);
int32_t sndp_hal_hr_exec_self_calib(void);
int32_t sndp_hal_hr_read_proximity_value(unsigned short* value);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_HRSENSOR_SUPPORT__ */

#endif /* __SNDP_HAL_HR_H__ */
