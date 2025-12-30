#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_iomux.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_cover_switch.h"
#include "sndp_cover_switch_box_notify.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool cs_box_notify_init = false;
static sndp_hal_cs_status_changed_callback cs_box_notify_status_changed_cb_ptr = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

POSSIBLY_UNUSED static void sndp_box_notify_cs_status_changed(sndp_hal_cover_status_e *cover_status)
{
	if(cs_box_notify_status_changed_cb_ptr) {
		sndp_call_func_in_app_thread((uint32_t)cs_box_notify_status_changed_cb_ptr, 
					(uint32_t)cover_status, 0, 0);
	}
}

static int32_t sndp_box_notify_cs_init(void)
{
	if(cs_box_notify_init) {
		CS_NOTIFY_TRACE(0, "already initialized");
		return SNDP_HAL_RET_OK;
	}
		
	//TODO:

	cs_box_notify_init = true;
	CS_NOTIFY_TRACE(0, "init done.");
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_box_notify_cs_set_status_changed_callback(sndp_hal_cs_status_changed_callback callback)
{
	cs_box_notify_status_changed_cb_ptr = callback;
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_box_notify_cs_get_curr_status(sndp_hal_cover_status_e *status)
{
	return SNDP_HAL_RET_FAIL;
}

static int32_t sndp_box_notify_cs_check_curr_status(void)
{
	return SNDP_HAL_RET_FAIL;
}

const sndp_hal_cover_switch_s sndp_hal_cover_switch_box_notify = {
	.init							= sndp_box_notify_cs_init,
    .set_status_changed_callback	= sndp_box_notify_cs_set_status_changed_callback,
    .get_curr_status				= sndp_box_notify_cs_get_curr_status,
    .check_curr_status				= sndp_box_notify_cs_check_curr_status,
};


#endif	//__SNDP_COVER_SWITCH_BOX_NOTIFY__