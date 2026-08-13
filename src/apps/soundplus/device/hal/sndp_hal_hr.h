#ifndef __SNDP_HAL_HR_H__
#define __SNDP_HAL_HR_H__


#if defined(__SNDP_HRSENSOR_SUPPORT__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    SNDP_HAL_HR_PPG_0      = 0, /**< SEQ0=PPG0                        */
    SNDP_HAL_HR_PPG_1      = 1, /**< SEQ0=PPG0, SEQ1=PPG1             */
    SNDP_HAL_HR_PPG_2      = 2, /**< SEQ0=PPG0, SEQ1=PPG1,  SEQ2=PPG2 */
    SNDP_HAL_HR_PPG_3      = 3, /**<            SEQ1=PPG1             */
    SNDP_HAL_HR_PPG_4      = 4, /**<            SEQ1=PPG1,  SEQ2=PPG2 */
    SNDP_HAL_HR_PROX       = 5, /**< SEQ0=Prox */
    SNDP_HAL_HR_PROX_PPG_0 = 6, /**< SEQ0=Prox, SEQ1=PPG1             */
    SNDP_HAL_HR_PROX_PPG_1 = 7, /**< SEQ0=Prox, SEQ1=PPG1,  SEQ2=PPG2 */
} sndp_hal_hr_operation_mode_e;


typedef void (*sndp_hal_hr_read_ppg_callback)(int32_t *data, uint16_t cnt);
typedef void (*sndp_hal_hr_calib_callback)(uint8_t *data, uint16_t data_len);
typedef void (*sndp_hal_hr_report_ppg_raw_data_callback)(uint8_t *data, uint16_t data_len);
typedef void (*sndp_hal_hr_ppg_samples_callback)(uint16_t data);
typedef void (*sndp_hal_hr_fifo_ready_callback)(void);

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

    
    /** 
     * return: 0 no error.
     */
    int32_t (* read_proximity_threshold)(uint16_t *high_threshold, uint16_t *low_threshold);

    /** 
     * return: 0 no error.
     */
    int32_t (* write_proximity_threshold)(uint16_t high_threshold, uint16_t low_threshold);

    /** 
     * return: 0 no error.
     */
    int32_t (* write_reg)(uint8_t reg_addr, uint8_t reg_val);

    /** 
     * return: 0 no error.
     */
    int32_t (* read_reg)(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len);

    
    /** 
     * return: 0 no error.
     */
    int32_t (* set_report_ppg_raw_data_callback)(sndp_hal_hr_report_ppg_raw_data_callback callback);

    /** 
     * op_mode: sndp_hal_hr_operation_mode_e
     * return: 0 no error.
     */
    int32_t (* switch_operation_mode)(sndp_hal_hr_operation_mode_e op_mode);

    /** 
     * chipid: 
     * return: 0 no error.
     */
    int32_t (* read_chip_id)(uint8_t *chipid);
    /** 
    int32_t (* samples_measurement_start)(void);
     * return: 0 no error.
     */
    int32_t (* samples_measurement_start)(int duration_s);

    int32_t (* read_samples_rate)(sndp_hal_hr_ppg_samples_callback callback);

    /**
     * desc: Handle ppg fifo interruption (read fifo and trigger callbacks).
     *       Called by the unified sleep app process thread.
     * return: 0 no error.
     */
    int32_t (* ppg_fifo_task)(void);

    int32_t (* set_fifo_ready_callback)(sndp_hal_hr_fifo_ready_callback callback);

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
int32_t sndp_hal_hr_read_proximity_threshold(uint16_t *high_threshold, uint16_t *low_threshold);
int32_t sndp_hal_hr_write_proximity_threshold(uint16_t high_threshold, uint16_t low_threshold);

int32_t sndp_hal_hr_write_reg(uint8_t reg_addr, uint8_t reg_val);
int32_t sndp_hal_hr_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len);

int32_t sndp_hal_hr_set_report_ppg_raw_data_callback(sndp_hal_hr_report_ppg_raw_data_callback callback);
int32_t sndp_hal_hr_switch_operation_mode(sndp_hal_hr_operation_mode_e op_mode);

int32_t sndp_hal_hr_read_chip_id(uint8_t *chipid);
int32_t sndp_hal_hr_samples_measurement_start(int duration_s);
int32_t sndp_hal_hr_read_samples_rate(sndp_hal_hr_ppg_samples_callback callback);
int32_t sndp_hal_hr_ppg_fifo_task(void);
int32_t sndp_hal_hr_set_fifo_ready_callback(sndp_hal_hr_fifo_ready_callback callback);
void sndp_hal_hr_ppg_fifo_task_ready_callback(void);
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_HRSENSOR_SUPPORT__ */

#endif /* __SNDP_HAL_HR_H__ */
