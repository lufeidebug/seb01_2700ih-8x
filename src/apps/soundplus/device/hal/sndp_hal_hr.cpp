#if defined(__SNDP_HRSENSOR_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_hr.h"

#if defined(__SNDP_HRSENSOR_SSH401A__)
#include "sndp_ssh401a_adapter.h"
#endif


/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/



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

int32_t sndp_hal_hr_set_hr_measure_callback(sndp_hal_hr_measure_callback callback)
{
	if((p_hal_hr != NULL) && (p_hal_hr->set_hr_measure_callback != NULL))
		return p_hal_hr->set_hr_measure_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_start_hr_measure(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->start_hr_measure != NULL))
		return p_hal_hr->start_hr_measure();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_stop_hr_measure(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->stop_hr_measure != NULL))
		return p_hal_hr->stop_hr_measure();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_set_read_raw_data_callback(sndp_hal_hr_read_raw_data_callback callback)
{
	if((p_hal_hr != NULL) && (p_hal_hr->set_read_raw_data_callback != NULL))
		return p_hal_hr->set_read_raw_data_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_hr_read_raw_data(void)
{
	if((p_hal_hr != NULL) && (p_hal_hr->read_raw_data != NULL))
		return p_hal_hr->read_raw_data();
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


#endif	/* __SNDP_HRSENSOR_MGR__ */

