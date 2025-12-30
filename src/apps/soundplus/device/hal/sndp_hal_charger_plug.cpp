#if defined(__SNDP_CHARGER_PLUG_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_charger_plug.h"

#if defined(__SNDP_CHARGER_PLUG_PMU_INT__)
#include "sndp_charger_plug_pmu_int.h"
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
static sndp_hal_charger_plug_s *p_hal_charger_plug = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

uint32_t sndp_hal_charger_plug_init(void)
{
#if defined(__SNDP_CHARGER_PLUG_PMU_INT__)	
	if((sndp_hal_charger_plug_pmu_int.init != NULL) && (sndp_hal_charger_plug_pmu_int.init() == SNDP_HAL_RET_OK)) {
		p_hal_charger_plug = (sndp_hal_charger_plug_s *)&sndp_hal_charger_plug_pmu_int;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_hal_charger_plug = NULL;
	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_charger_plug_set_status_changed_callback(sndp_hal_charger_plug_status_changed_callback callback)
{
	if((p_hal_charger_plug != NULL) && (p_hal_charger_plug->set_status_changed_callback != NULL)) {
		return p_hal_charger_plug->set_status_changed_callback(callback);
	}

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_charger_plug_get_curr_status(sndp_hal_charger_plug_status_e *status)
{
	if((p_hal_charger_plug != NULL) && (p_hal_charger_plug->get_curr_status != NULL)) {
		return p_hal_charger_plug->get_curr_status(status);
	}

	return SNDP_HAL_RET_FAIL;
}


uint32_t sndp_hal_charger_plug_check_curr_status(void)
{
	if((p_hal_charger_plug != NULL) && (p_hal_charger_plug->check_curr_status != NULL)) {
		return p_hal_charger_plug->check_curr_status();
	}

	return SNDP_HAL_RET_FAIL;
}


#endif	/* __SNDP_IOBOX_MGR__ */

