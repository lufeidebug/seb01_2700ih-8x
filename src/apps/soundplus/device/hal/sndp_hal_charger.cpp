#if defined(__SNDP_CHARGER_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"


#include "sndp_hal_common.h"
#include "sndp_hal_charger.h"

#if defined(__SNDP_CHARGER_HP4554__)
extern "C" const sndp_hal_charger_s sndp_hal_charger_hp4554;
#endif

#if defined(__SNDP_CHARGER_BES__)
extern "C" const sndp_hal_charger_s sndp_hal_charger_bes;
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
static sndp_hal_charger_s *p_sndp_hal_charger = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_hal_charger_init(void)
{
#if defined(__SNDP_CHARGER_HP4554__)
	if((sndp_hal_charger_hp4554.init != NULL) && (sndp_hal_charger_hp4554.init() == SNDP_HAL_RET_OK)){
		p_sndp_hal_charger = (sndp_hal_charger_s *)&sndp_hal_charger_hp4554;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_CHARGER_BES__)
    if((sndp_hal_charger_bes.init != NULL) && (sndp_hal_charger_bes.init() == SNDP_HAL_RET_OK)){
		p_sndp_hal_charger = (sndp_hal_charger_s *)&sndp_hal_charger_bes;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_sndp_hal_charger = NULL;
    return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_charger_set_charging_current(sndp_hal_charging_current_e charging_current)
{
    if((p_sndp_hal_charger != NULL) && (p_sndp_hal_charger->set_charging_current != NULL)) {
        if(p_sndp_hal_charger->set_charging_current(charging_current) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_charger_set_charging_mode_changed_callback(sndp_hal_charging_mode_changed_callback callback)
{
	if((p_sndp_hal_charger != NULL) && (p_sndp_hal_charger->set_charging_mode_changed_callback != NULL)) {
        if(p_sndp_hal_charger->set_charging_mode_changed_callback(callback) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_charger_check_curr_status(void)
{
	if((p_sndp_hal_charger != NULL) && (p_sndp_hal_charger->check_curr_status != NULL)) {
        if(p_sndp_hal_charger->check_curr_status() == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_charger_get_charging_status(sndp_hal_charger_mode_e *mode)
{
    if((p_sndp_hal_charger != NULL) && (p_sndp_hal_charger->get_charging_status != NULL)) {
        if(p_sndp_hal_charger->get_charging_status(mode) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}


bool sndp_hal_charger_is_charging_enabled(void)
{
	if((p_sndp_hal_charger != NULL) && (p_sndp_hal_charger->is_charging_enabled != NULL)) {
		return p_sndp_hal_charger->is_charging_enabled();
    }
	return false;
}


#endif	/* __SNDP_CHARGER_MGR__ */

