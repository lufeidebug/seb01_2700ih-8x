
#if defined(__SNDP_TEMPLATE__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_gpio.h"
#include "hal_gpadc.h"

#include "spif_common.h"
#include "sphal_common.h"
#include "sphal_temperature.h"
#include "spdrv_temperature_ntc.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sphal_temperature_measure_callback sphal_temp_measure_cb_ptr = NULL;
static int16_t spdrv_temperature = 25;


/**************************************************************************************************
* Function
**************************************************************************************************/

static int32_t spdrv_template_init(void)
{	
	return SPHAL_RET_OK;
}

static int32_t spdrv_template_set_measure_callback(sphal_temperature_measure_callback callback)
{
	sphal_temp_measure_cb_ptr = callback;
	return 0;
}

static int32_t spdrv_template_measure(void)
{
	return 0;
}

const sphal_temperature_s sphal_temperature_template = {
	.init						= spdrv_template_init,
	.set_measure_callback 		= spdrv_template_set_measure_callback,
	.measure					= spdrv_template_measure,
};

#endif	/* __SNDP_TEMPLATE__ */


