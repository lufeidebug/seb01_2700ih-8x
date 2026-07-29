#if defined(__SNDP_GSENSOR_SUPPORT__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_acc.h"

#if defined(__SNDP_GSENSOR_DA217E__)	
#include "sndp_da217e_adapter.h"
#endif


/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/



/**************************************************************************************************
* Extern
**************************************************************************************************/
#if defined(__SNDP_GSENSOR_DA217E__)	
extern "C" const sndp_hal_acc_s sndp_acc_da217e;
#endif


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_acc_s *p_hal_acc = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_hal_acc_init(void)
{
#if defined(__SNDP_GSENSOR_DA217E__)	
	if((sndp_acc_da217e.init != NULL) && (sndp_acc_da217e.init() == SNDP_HAL_RET_OK)) {
		p_hal_acc = (sndp_hal_acc_s *)&sndp_acc_da217e;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_hal_acc = NULL;
	return SNDP_HAL_RET_FAIL;

}

int32_t sndp_hal_acc_enter_standby_mode(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->enter_standby_mode != NULL))
		return p_hal_acc->enter_standby_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_enter_detection_mode(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->enter_detection_mode != NULL))
		return p_hal_acc->enter_detection_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_set_reading_raw_data_callback(sndp_hal_acc_read_raw_data_callback callback)
{
	if((p_hal_acc != NULL) && (p_hal_acc->set_reading_raw_data_callback != NULL))
		return p_hal_acc->set_reading_raw_data_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_start_reading_raw_data(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->start_reading_raw_data != NULL)) {
		// TRACE(0, "start_reading_raw_data");
		return p_hal_acc->start_reading_raw_data();
	}
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_stop_reading_raw_data(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->stop_reading_raw_data != NULL))
		return p_hal_acc->stop_reading_raw_data();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_start_single_tap_interrupt(void)
{
		if((p_hal_acc != NULL) && (p_hal_acc->start_single_tap_interrupt != NULL))
				return p_hal_acc->start_single_tap_interrupt();
		return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_stop_single_tap_interrupt(void)
{
		if((p_hal_acc != NULL) && (p_hal_acc->stop_single_tap_interrupt != NULL))
				return p_hal_acc->stop_single_tap_interrupt();
		return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_set_calibration_rsp_func(sndp_hal_acc_calibration_rsp_func rsp_func)
{
	if((p_hal_acc != NULL) && (p_hal_acc->set_calibration_rsp_func != NULL))
		return p_hal_acc->set_calibration_rsp_func(rsp_func);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_recv_calibration_data(uint8_t *data, uint16_t data_len)
{
	if((p_hal_acc != NULL) && (p_hal_acc->recv_calibration_data != NULL))
		return p_hal_acc->recv_calibration_data(data, data_len);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_exec_calibration_self_calib(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->exec_calibration_self_calib != NULL))
		return p_hal_acc->exec_calibration_self_calib();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
	if((p_hal_acc != NULL) && (p_hal_acc->write_reg != NULL))
		return p_hal_acc->write_reg(reg_addr, reg_val);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len)
{
	if((p_hal_acc != NULL) && (p_hal_acc->read_reg != NULL))
		return p_hal_acc->read_reg(reg_addr, read_buf, read_len);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_read_chip_id(uint8_t *chipid)
{
	if((p_hal_acc != NULL) && (p_hal_acc->read_chip_id != NULL))
		return p_hal_acc->read_chip_id(chipid);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_read_raw_data(sndp_hal_acc_data_s *acc_data)
{
	if((p_hal_acc != NULL) && (p_hal_acc->read_raw_data != NULL))
		return p_hal_acc->read_raw_data(acc_data);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_samples_measurement_start(int duration_s)
{
	if((p_hal_acc != NULL) && (p_hal_acc->samples_measurement_start != NULL))
		return p_hal_acc->samples_measurement_start(duration_s);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_read_samples_rate(sndp_hal_acc_samples_callback callback)
{
	if((p_hal_acc != NULL) && (p_hal_acc->read_samples_rate != NULL))
		return p_hal_acc->read_samples_rate(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_acc_fifo_task(void)
{
	if((p_hal_acc != NULL) && (p_hal_acc->acc_fifo_task != NULL))
		return p_hal_acc->acc_fifo_task();
	return SNDP_HAL_RET_FAIL;
}

#endif	/* __SNDP_GSENSOR_SUPPORT__ */

