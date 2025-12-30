#if defined(__SNDP_BATTERY_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_battery.h"

#if defined(__SNDP_BAT_LOOKUP_TABLE__)
#include "sndp_bat_pwr_lkt.h"
#endif

#if defined(__SNDP_BAT_CUMULATIVE__)
#include "sndp_bat_pwr_cumulative.h"
#endif

#if defined(__SNDP_COULOMETER_CW2015__)
#include "sndp_cw2015_adapter.h"
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
static sndp_hal_battery_s *p_sndp_hal_battery = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

uint32_t sndp_hal_battery_init(sndp_hal_bat_charging_status_e charging_status, uint8_t init_bat_per)
{
#if defined(__SNDP_BAT_LOOKUP_TABLE__)
	if((sndp_hal_bat_lkt.init != NULL) && (sndp_hal_bat_lkt.init(charging_status, init_bat_per) == SNDP_HAL_RET_OK)) {
		p_sndp_hal_battery = (sndp_hal_battery_s *)&sndp_hal_bat_lkt;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_BAT_CUMULATIVE__)
	if((sndp_hal_bat_cumulative.init != NULL) && (sndp_hal_bat_cumulative.init(charging_status, init_bat_per) == SNDP_HAL_RET_OK)) {
		p_sndp_hal_battery = (sndp_hal_battery_s *)&sndp_hal_bat_cumulative;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_COULOMETER_CW2015__)
		if((sndp_hal_bat_cw2015.init != NULL) && (sndp_hal_bat_cw2015.init(charging_status, init_bat_per) == SNDP_HAL_RET_OK)) {
			p_sndp_hal_battery = (sndp_hal_battery_s *)&sndp_hal_bat_cw2015;
			return SNDP_HAL_RET_OK;
		}
#endif

	p_sndp_hal_battery = NULL;
    return SNDP_HAL_RET_FAIL;
}


uint32_t sndp_hal_battery_set_measure_callback(sndp_hal_battery_measure_callback callback)
{
	if(p_sndp_hal_battery != NULL && p_sndp_hal_battery->set_measure_callback != NULL) {
        if(p_sndp_hal_battery->set_measure_callback(callback) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_battery_measure(sndp_hal_bat_charging_status_e charging_status)
{
    if(p_sndp_hal_battery != NULL && p_sndp_hal_battery->measure != NULL) {
        if(p_sndp_hal_battery->measure(charging_status) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_battery_update_charging_status(sndp_hal_bat_charging_status_e status)
{
	if(p_sndp_hal_battery != NULL && p_sndp_hal_battery->update_charging_status != NULL) {
        if(p_sndp_hal_battery->update_charging_status(status) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
    }

	return SNDP_HAL_RET_FAIL;
}


#if defined(__SNDP_COULOMETER_CW2015__)
uint32_t sndp_hal_battery_cw2015_set_chipmode(sndp_hal_cw2015_chipmode_e chipmode)
{
	if(p_sndp_hal_battery != NULL && p_sndp_hal_battery->cw2015_set_chipmode != NULL) {
		if(p_sndp_hal_battery->cw2015_set_chipmode(chipmode) == SNDP_HAL_RET_OK)
			return SNDP_HAL_RET_OK;
	}

	return SNDP_HAL_RET_FAIL;
}

#endif


#endif	/* __SNDP_BATTERY_MGR__ */

