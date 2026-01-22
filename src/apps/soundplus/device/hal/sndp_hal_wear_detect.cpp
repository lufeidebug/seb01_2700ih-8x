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
#if defined(__SNDP_HRSENSOR_SSH401A__)
extern "C" const sndp_hal_wear_detection_s sndp_wear_detection_ssh401a;
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
#if defined(__SNDP_HRSENSOR_SSH401A__)	
	if((sndp_wear_detection_ssh401a.init != NULL) && (sndp_wear_detection_ssh401a.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_wear_detection = (sndp_hal_wear_detection_s *)&sndp_wear_detection_ssh401a;
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

#endif	/* __SNDP_WEAR_DETECT_MGR__ */

