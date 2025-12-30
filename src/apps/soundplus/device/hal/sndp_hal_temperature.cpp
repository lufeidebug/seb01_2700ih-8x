#if defined(__SNDP_TEMPERATURE_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_temperature.h"

#if defined(__SNDP_TEMPERATURE_NTC__)
#include "sndp_temperature_ntc.h"
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
static sndp_hal_temperature_s *p_sndp_hal_temperature = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_hal_temperature_init(void)
{
#if defined(__SNDP_TEMPERATURE_NTC__)	
	if(sndp_hal_temperature_ntc.init != NULL && sndp_hal_temperature_ntc.init() == SNDP_HAL_RET_OK) {
		p_sndp_hal_temperature = (sndp_hal_temperature_s *)&sndp_hal_temperature_ntc;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_sndp_hal_temperature = NULL;
	return SNDP_HAL_RET_FAIL;
}


int32_t sndp_hal_temperature_set_measure_callback(sndp_hal_temperature_measure_callback callback)
{
	if(p_sndp_hal_temperature != NULL && p_sndp_hal_temperature->set_measure_callback != NULL)
		return p_sndp_hal_temperature->set_measure_callback(callback);

	return SNDP_HAL_RET_FAIL;
}

int32_t sndp_hal_temperature_measure(void)
{
	if(p_sndp_hal_temperature != NULL && p_sndp_hal_temperature->measure != NULL)
		return p_sndp_hal_temperature->measure();

	return SNDP_HAL_RET_FAIL;
}

#endif	/* __SP_MGR_TEMPERATURE__ */

