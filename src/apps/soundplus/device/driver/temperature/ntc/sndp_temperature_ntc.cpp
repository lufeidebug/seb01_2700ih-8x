
#if defined(__SNDP_TEMPERATURE_NTC__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_gpio.h"
#include "hal_gpadc.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_temperature.h"
#include "sndp_temperature_ntc.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_NTC_REF_VOLTAGE				(1700)	//mV
#define SNDP_NTC_REF_RESISTENCE			    (10000)	//ohm

#define SNDP_NTC_ADC_CHAN					(HAL_GPADC_CHAN_0)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static int32_t sndp_temp_ntc_measure(void);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_temperature_measure_callback p_sndp_hal_ntc_measure_cb;
static sndp_ntc_ctx_s sndp_ntc_ctx;

static const sndp_ntc_temp_table_s sndp_ntc_temp_table[] = {
	{-15, 54166},
	{-10, 42889},
	{  0, 34196},
	{  5, 27445},
	{ 10, 18010},
	{ 15, 14720},
	{ 20, 12099},
	{ 25, 10000},
	{ 30,  8309},
	{ 35,  6939},
	{ 40,  5824},
	{ 45,  4911},
	{ 50,  4160},
	{ 55,  3539},
	{ 60,  3024},
	{ 99,	  0},
};


/**************************************************************************************************
* Function
**************************************************************************************************/

static uint16_t sndp_temp_ntc_conver_volt_to_temperature(void)
{
	uint32_t volt_sum = 0;
	uint32_t resistence;

	/* calculate average voltage */
	uint32_t i;
	for(i = 0; i < sndp_ntc_ctx.measure_index && i < TEMP_NTC_STABLE_COUNT; i++)
		volt_sum += sndp_ntc_ctx.measure_volt[i];
	sndp_ntc_ctx.voltage = volt_sum / i;

	
	if(sndp_ntc_ctx.voltage > SNDP_NTC_REF_VOLTAGE || sndp_ntc_ctx.voltage == 0) {
		/* invalid data */
		sndp_ntc_ctx.curr_temp = 99;
	}  else {

		/* convert voltage to resistence */
		resistence = (sndp_ntc_ctx.voltage * SNDP_NTC_REF_RESISTENCE / (SNDP_NTC_REF_VOLTAGE - sndp_ntc_ctx.voltage));
		
		/* convert voltage to temperature by looking up the table */
		for(uint16_t i = 0; i < sizeof(sndp_ntc_temp_table)/sizeof(sndp_ntc_temp_table[0]); i++) {
			if(resistence >= sndp_ntc_temp_table[i].resistence) {
				sndp_ntc_ctx.curr_temp = sndp_ntc_temp_table[i].temperature;
				break;
			}
		}
	}

	TEMP_NTC_TRACE(3, "volt=%d, lT=%d, cT=%d,", sndp_ntc_ctx.voltage, sndp_ntc_ctx.last_temp, sndp_ntc_ctx.curr_temp);
	
	/* Report temperature after measurement */
	if(sndp_ntc_ctx.curr_temp != sndp_ntc_ctx.last_temp) {
		sndp_ntc_ctx.last_temp = sndp_ntc_ctx.curr_temp;
		
		if(p_sndp_hal_ntc_measure_cb) {
			p_sndp_hal_ntc_measure_cb(sndp_ntc_ctx.curr_temp);
		}

	}
	
	return sndp_ntc_ctx.curr_temp;
	
}

static void sndp_temp_ntc_adc_irq_handler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{

	if(volt == HAL_GPADC_BAD_VALUE){
		TEMP_NTC_TRACE(0, "volt BAD, return");
		return;
	}

	sndp_ntc_ctx.measure_volt[sndp_ntc_ctx.measure_index%TEMP_NTC_STABLE_COUNT] = volt;
	sndp_ntc_ctx.measure_index++;

	sndp_call_func_in_dev_thread((uint32_t)sndp_temp_ntc_conver_volt_to_temperature, 0, 0, 0);	
	
}


static int32_t sndp_temp_ntc_init(void)
{
	memset(&sndp_ntc_ctx, 0, sizeof(sndp_ntc_ctx));
	sndp_temp_ntc_measure();	
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_temp_ntc_set_measure_callback(sndp_hal_temperature_measure_callback callback)
{
	p_sndp_hal_ntc_measure_cb = callback;
	return 0;
}

static int32_t sndp_temp_ntc_measure(void)
{
	hal_gpadc_open(SNDP_NTC_ADC_CHAN, HAL_GPADC_ATP_ONESHOT, sndp_temp_ntc_adc_irq_handler);
	return 0;
}

const sndp_hal_temperature_s sndp_hal_temperature_ntc = {
	.init						= sndp_temp_ntc_init,
	.set_measure_callback 		= sndp_temp_ntc_set_measure_callback,
	.measure					= sndp_temp_ntc_measure,
};

#endif	/* __SP_TEMPERATURE_USE_NTC__ */

