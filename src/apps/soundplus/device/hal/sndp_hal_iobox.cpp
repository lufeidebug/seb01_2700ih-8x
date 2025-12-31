#if defined(__SNDP_IOBOX_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_iobox.h"

#if defined(__SNDP_IOBOX_PMU_INT__)
#include "sndp_iobox_pmu_int.h"
#endif


#if defined(__SNDP_IOBOX_GPIO_INT__)
#include "sndp_iobox_gpio_int.h"
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
static sndp_hal_iobox_s *p_sndp_hal_iobox = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

uint32_t sndp_hal_iobox_init(void)
{
#if defined(__SNDP_IOBOX_PMU_INT__)	
	if((sndp_hal_iobox_pmu_int.init != NULL) && (sndp_hal_iobox_pmu_int.init() == SNDP_HAL_RET_OK)) {
		p_sndp_hal_iobox = (sndp_hal_iobox_s *)&sndp_hal_iobox_pmu_int;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_IOBOX_GPIO_INT__)
    if((sndp_hal_iobox_gpio_int.init != NULL) && (sndp_hal_iobox_gpio_int.init() == SNDP_HAL_RET_OK)) {
        p_sndp_hal_iobox = (sndp_hal_iobox_s *)&sndp_hal_iobox_gpio_int;
        return SNDP_HAL_RET_OK;
    }
#endif

	p_sndp_hal_iobox = NULL;
	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_iobox_set_status_changed_callback(sndp_hal_iobox_status_changed_callback callback)
{
	if((p_sndp_hal_iobox != NULL) && (p_sndp_hal_iobox->set_status_changed_callback != NULL)) {
		return p_sndp_hal_iobox->set_status_changed_callback(callback);
	}

	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_iobox_get_curr_status(sndp_hal_iobox_status_e *status)
{
	if((p_sndp_hal_iobox != NULL) && (p_sndp_hal_iobox->get_curr_status != NULL)) {
		return p_sndp_hal_iobox->get_curr_status(status);
	}

	return SNDP_HAL_RET_FAIL;
}


uint32_t sndp_hal_iobox_check_curr_status(void)
{
	if((p_sndp_hal_iobox != NULL) && (p_sndp_hal_iobox->check_curr_status != NULL)) {
		return p_sndp_hal_iobox->check_curr_status();
	}

	return SNDP_HAL_RET_FAIL;
}


#endif	/* __SNDP_IOBOX_MGR__ */

