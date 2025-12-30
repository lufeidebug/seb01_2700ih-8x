#if defined(__SNDP_WEAR_DETECT_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_wear_detect.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/
#if defined(__SNDP_PSENSOR_STK_W2101__)
extern const sndp_hal_wear_detection_s sndp_hal_wear_detection_w2101;
#endif

#if defined(__SNDP_TOUCH_GH6210__)	
extern const sndp_hal_wear_detection_s sndp_hal_wear_detection_gh621x;
#endif

#if defined(__SNDP_TOUCH_HX9031AS__)	
extern const sndp_hal_wear_detection_s sndp_hal_wear_detection_hx9031as;
#endif

#if defined(__SNDP_PSENSOR_JSA1227__)	
extern const sndp_hal_wear_detection_s sndp_hal_wear_detection_jsa1227;
#endif


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_wear_detection_s *p_sndp_hal_wear_detection = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/


int32_t sndp_hal_wear_detection_init(void)
{
#if defined(__SNDP_PSENSOR_STK_W2101__)	
	if((sndp_hal_wear_detection_w2101.init != NULL) && (sndp_hal_wear_detection_w2101.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_wear_detection = (sndp_hal_wear_detection_s *)&sndp_hal_wear_detection_w2101;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_TOUCH_GH6210__)	
	if((sndp_hal_wear_detection_gh621x.init != NULL) && (sndp_hal_wear_detection_gh621x.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_wear_detection = (sndp_hal_wear_detection_s *)&sndp_hal_wear_detection_gh621x;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_TOUCH_HX9031AS__)	
	if((sndp_hal_wear_detection_hx9031as.init != NULL) && (sndp_hal_wear_detection_hx9031as.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_wear_detection = (sndp_hal_wear_detection_s *)&sndp_hal_wear_detection_hx9031as;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_PSENSOR_JSA1227__)	
    if((sndp_hal_wear_detection_jsa1227.init != NULL) && (sndp_hal_wear_detection_jsa1227.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_wear_detection = (sndp_hal_wear_detection_s *)&sndp_hal_wear_detection_jsa1227;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_sndp_hal_wear_detection = NULL;
	return SNDP_HAL_RET_FAIL;

}

int32_t sndp_hal_wear_detection_set_wear_status_changed_callback(sndp_hal_wear_status_changed_callback callback)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->set_wear_status_changed_callback != NULL))
		return p_sndp_hal_wear_detection->set_wear_status_changed_callback(callback);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_get_curr_status(sndp_hal_wear_status_e *status)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->get_curr_status != NULL))
		return p_sndp_hal_wear_detection->get_curr_status(status);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_check_curr_status(void)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->check_curr_status != NULL))
		return p_sndp_hal_wear_detection->check_curr_status();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_enter_standby_mode(void)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->enter_standby_mode != NULL))
		return p_sndp_hal_wear_detection->enter_standby_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_enter_detection_mode(void)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->enter_detection_mode != NULL))
		return p_sndp_hal_wear_detection->enter_detection_mode();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_set_calibration_send_data_func(sndp_hal_wear_calibration_send_data_func func)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->set_calibration_send_data_func != NULL))
		return p_sndp_hal_wear_detection->set_calibration_send_data_func(func);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_recv_calibration_data(uint8_t *data, uint16_t data_len)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->recv_calibration_data != NULL))
		return p_sndp_hal_wear_detection->recv_calibration_data(data, data_len);
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_exec_calibration_self_calib(void)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->exec_calibration_self_calib != NULL))
		return p_sndp_hal_wear_detection->exec_calibration_self_calib();
	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_wear_detection_read_chip_id(uint16_t *chip_id)
{
	if((p_sndp_hal_wear_detection != NULL) && (p_sndp_hal_wear_detection->read_chip_id != NULL))
		return p_sndp_hal_wear_detection->read_chip_id(chip_id);
	return SNDP_HAL_RET_FAIL;
}

#endif	/* __SNDP_WEAR_DETECT_MGR__ */

