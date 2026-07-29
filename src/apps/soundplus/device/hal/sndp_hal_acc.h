#ifndef __SNDP_HAL_ACC_H__
#define __SNDP_HAL_ACC_H__

#if defined(__SNDP_GSENSOR_SUPPORT__)

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} sndp_hal_acc_data_s;

typedef void (*sndp_hal_acc_read_raw_data_callback)(sndp_hal_acc_data_s *data, uint16_t cnt);
typedef void (*sndp_hal_acc_calibration_rsp_func)(uint8_t *data, uint16_t data_len);
typedef void (*sndp_hal_acc_samples_callback)(uint16_t data);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
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
    int32_t (* set_reading_raw_data_callback)(sndp_hal_acc_read_raw_data_callback callback);


	/** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* start_reading_raw_data)(void);


	/** 
     * return: 0 no error.
     */
    int32_t (* stop_reading_raw_data)(void);

	/** 
     * callback: This pointer will be called when the lid switch stutas changes.
     * return: 0 no error.
     */
    int32_t (* start_single_tap_interrupt)(void);


	/** 
     * return: 0 no error.
     */
    int32_t (* stop_single_tap_interrupt)(void);
	/** 
     * return: 0 no error.
     */
    int32_t (* set_calibration_rsp_func)(sndp_hal_acc_calibration_rsp_func rsp_func);


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
    int32_t (* write_reg)(uint8_t reg_addr, uint8_t reg_val);

    /** 
     * return: 0 no error.
     */
    int32_t (* read_reg)(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len);

    /** 
     * chipid: 
     * return: 0 no error.
     */
    int32_t (* read_chip_id)(uint8_t *chipid);

    int32_t (* read_raw_data)(sndp_hal_acc_data_s *acc_data);

    /** 
        int32_t (* samples_measurement_start)(void);
     * return: 0 no error.
     */
    int32_t (* samples_measurement_start)(int duration_s);

    int32_t (* read_samples_rate)(sndp_hal_acc_samples_callback callback);

    /**
     * desc: Handle acc fifo interruption (read fifo and trigger callbacks).
     *       Called by the unified sleep app process thread.
     * return: 0 no error.
     */
    int32_t (* acc_fifo_task)(void);
} sndp_hal_acc_s;

int32_t sndp_hal_acc_init(void);
int32_t sndp_hal_acc_enter_standby_mode(void);
int32_t sndp_hal_acc_enter_detection_mode(void);
int32_t sndp_hal_acc_start_single_tap_interrupt(void);
int32_t sndp_hal_acc_stop_single_tap_interrupt(void);
int32_t sndp_hal_acc_set_reading_raw_data_callback(sndp_hal_acc_read_raw_data_callback callback);
int32_t sndp_hal_acc_start_reading_raw_data(void);
int32_t sndp_hal_acc_stop_reading_raw_data(void);
int32_t sndp_hal_acc_set_calibration_rsp_func(sndp_hal_acc_calibration_rsp_func rsp_func);
int32_t sndp_hal_acc_recv_calibration_data(uint8_t *data, uint16_t data_len);
int32_t sndp_hal_acc_exec_calibration_self_calib(void);

int32_t sndp_hal_acc_write_reg(uint8_t reg_addr, uint8_t reg_val);
int32_t sndp_hal_acc_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len);
int32_t sndp_hal_acc_read_chip_id(uint8_t *chipid);
int32_t sndp_hal_acc_read_raw_data(sndp_hal_acc_data_s *acc_data);
int32_t sndp_hal_acc_samples_measurement_start(int duration_s);
int32_t sndp_hal_acc_read_samples_rate(sndp_hal_acc_samples_callback callback);
int32_t sndp_hal_acc_fifo_task(void);
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_GSENSOR_SUPPORT__ */

#endif /* __SNDP_HAL_ACC_H__ */
