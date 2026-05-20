#if defined(__SNDP_HRSENSOR_SUPPORT__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_hr.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/
#if defined(__SNDP_HRSENSOR_SSH401A__)
extern "C" const sndp_hal_hr_s sndp_hr_ssh401a;
#endif


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_hr_s *p_hal_hr = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_hal_hr_init(void)
{
#if defined(__SNDP_HRSENSOR_SSH401A__)	
	if((sndp_hr_ssh401a.init != NULL) && (sndp_hr_ssh401a.init() == SNDP_HAL_RET_OK)) {
		p_hal_hr = (sndp_hal_hr_s *)&sndp_hr_ssh401a;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_hal_hr = NULL;
	return SNDP_HAL_RET_FAIL;

}


int32_t sndp_hal_hr_enter_standby_mode(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->enter_standby_mode != NULL))
		return p_hal_hr->enter_standby_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_enter_detection_mode(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->enter_detection_mode != NULL))
		return p_hal_hr->enter_detection_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_set_reading_ppg_callback(sndp_hal_hr_read_ppg_callback callback)
{
	if((p_hal_hr != NULL) && (p_hal_hr->set_reading_ppg_callback != NULL))
		return p_hal_hr->set_reading_ppg_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_start_reading_ppg(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->start_reading_ppg != NULL))
		return p_hal_hr->start_reading_ppg();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_stop_reading_ppg(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->stop_reading_ppg != NULL))
		return p_hal_hr->stop_reading_ppg();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_set_calib_callback(sndp_hal_hr_calib_callback callback)
{
	if((p_hal_hr != NULL) && (p_hal_hr->set_calib_callback != NULL))
		return p_hal_hr->set_calib_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_exec_calib(uint8_t *data, uint16_t data_len)
{
	if((p_hal_hr != NULL) && (p_hal_hr->exec_calib != NULL))
		return p_hal_hr->exec_calib(data, data_len);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_exec_self_calib(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->exec_self_calib != NULL))
		return p_hal_hr->exec_self_calib();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_read_proximity_value(unsigned short* value)
{
	if((p_hal_hr != NULL) && (p_hal_hr->read_proximity_value != NULL))
		return p_hal_hr->read_proximity_value(value);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_read_proximity_threshold(uint16_t *high_threshold, uint16_t *low_threshold)
{
    if((p_hal_hr != NULL) && (p_hal_hr->read_proximity_threshold != NULL))
        return p_hal_hr->read_proximity_threshold(high_threshold, low_threshold);
    return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_write_proximity_threshold(uint16_t high_threshold, uint16_t low_threshold)
{
	if((p_hal_hr != NULL) && (p_hal_hr->write_proximity_threshold != NULL))
		return p_hal_hr->write_proximity_threshold(high_threshold, low_threshold);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
	if((p_hal_hr != NULL) && (p_hal_hr->write_reg != NULL))
		return p_hal_hr->write_reg(reg_addr, reg_val);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len)
{
	if((p_hal_hr != NULL) && (p_hal_hr->read_reg != NULL))
		return p_hal_hr->read_reg(reg_addr, read_buf, read_len);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_set_ppg_test_mode_callback(sndp_hal_hr_ppg_test_mode_callback callback)
{
    if((p_hal_hr != NULL) && (p_hal_hr->set_ppg_test_mode_callback != NULL))
        return p_hal_hr->set_ppg_test_mode_callback(callback);
    return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_switch_ppg_test_mode(uint8_t en)
{
	if((p_hal_hr != NULL) && (p_hal_hr->switch_ppg_test_mode != NULL))
		return p_hal_hr->switch_ppg_test_mode(en);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_switch_operation_mode(sndp_hal_hr_operation_mode_e op_mode)
{
	if((p_hal_hr != NULL) && (p_hal_hr->switch_operation_mode != NULL))
		return p_hal_hr->switch_operation_mode(op_mode);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_read_chip_id(uint8_t *chipid)
{
	if((p_hal_hr != NULL) && (p_hal_hr->read_chip_id != NULL))
		return p_hal_hr->read_chip_id(chipid);
	return SNDP_HAL_RET_FAIL;
}



#endif	/* __SNDP_HRSENSOR_SUPPORT__ */

