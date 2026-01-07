#if defined(__SNDP_COVER_SWITCH_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_cover_switch.h"

#if defined(__SNDP_COVER_SWITCH_HALL__)
#include "sndp_cover_switch_hall.h"
#endif

#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)
#include "sndp_cover_switch_box_notify.h"
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
static sndp_hal_cover_switch_s *p_sndp_hal_cover_switch = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

uint32_t sndp_hal_cover_switch_init(void)
{

#if defined(__SNDP_COVER_SWITCH_HALL__)	
	if((sndp_hal_cover_switch_hall.init != NULL) && (sndp_hal_cover_switch_hall.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_cover_switch = (sndp_hal_cover_switch_s *)&sndp_hal_cover_switch_hall;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)	
		if((sndp_hal_cover_switch_box_notify.init != NULL) && (sndp_hal_cover_switch_box_notify.init() == SNDP_HAL_RET_OK)) {
			p_sndp_hal_cover_switch = (sndp_hal_cover_switch_s *)&sndp_hal_cover_switch_box_notify;
			return SNDP_HAL_RET_OK;
		}
#endif

	p_sndp_hal_cover_switch = NULL;
	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_cover_switch_set_status_changed_callback(sndp_hal_cs_status_changed_callback callback)
{
	if((p_sndp_hal_cover_switch != NULL) && (p_sndp_hal_cover_switch->set_status_changed_callback != NULL)) {
		return p_sndp_hal_cover_switch->set_status_changed_callback(callback);
	}

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_cover_switch_get_curr_status(sndp_hal_cover_status_e *status)
{
	if((p_sndp_hal_cover_switch != NULL) && (p_sndp_hal_cover_switch->get_curr_status != NULL)) {
		return p_sndp_hal_cover_switch->get_curr_status(status);
	}

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_cover_switch_check_curr_status(void)
{
	if((p_sndp_hal_cover_switch != NULL) && (p_sndp_hal_cover_switch->check_curr_status != NULL)) {
		return p_sndp_hal_cover_switch->check_curr_status();
	}
	return SNDP_HAL_RET_FAIL;
}


#endif	/* __SP_MGR_COVER_SWITCH__ */

