#if defined(__SNDP_GESTURE_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_gesture.h"




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

#if defined(__SNDP_GSENSOR_DA217E__)	
extern "C" const sndp_hal_gesture_s sndp_gesture_da217e;
#endif


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


#endif	/* __SP_MGR_GESTURE__ */

