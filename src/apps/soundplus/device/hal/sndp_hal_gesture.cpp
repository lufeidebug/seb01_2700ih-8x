#if defined(__SNDP_GESTURE_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_gesture.h"

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
* Variable
**************************************************************************************************/
static sndp_hal_gesture_s *p_sndp_hal_gesture = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_hal_gesture_init(void)
{
#if defined(__SNDP_GSENSOR_DA217E__)	
	if((sndp_gesture_da217e.init != NULL) && (sndp_gesture_da217e.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_gesture = (sndp_hal_gesture_s *)&sndp_gesture_da217e;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_sndp_hal_gesture = NULL;
	return SNDP_HAL_RET_FAIL;

}

int32_t sndp_hal_gesture_set_event_callback(sndp_hal_gesture_event_callback callback)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->set_event_callback != NULL))
		return p_sndp_hal_gesture->set_event_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_gesture_enter_standby_mode(void)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->enter_standby_mode != NULL))
		return p_sndp_hal_gesture->enter_standby_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_gesture_enter_detection_mode(void)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->enter_detection_mode != NULL))
		return p_sndp_hal_gesture->enter_detection_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_gesture_set_calibration_send_data_func(sndp_hal_gesture_calibration_send_data_func func)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->set_calibration_send_data_func != NULL))
		return p_sndp_hal_gesture->set_calibration_send_data_func(func);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_gesture_recv_calibration_data(uint8_t *data, uint16_t data_len)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->recv_calibration_data != NULL))
		return p_sndp_hal_gesture->recv_calibration_data(data, data_len);
	return SNDP_HAL_RET_FAIL;
}


int32_t sndp_hal_gesture_exec_calibration_self_calib(void)
{
	if((p_sndp_hal_gesture != NULL) && (p_sndp_hal_gesture->exec_calibration_self_calib != NULL))
		return p_sndp_hal_gesture->exec_calibration_self_calib();
	return SNDP_HAL_RET_FAIL;
}


#endif	/* __SP_MGR_GESTURE__ */

