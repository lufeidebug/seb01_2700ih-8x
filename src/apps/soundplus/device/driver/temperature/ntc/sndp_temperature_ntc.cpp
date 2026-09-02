
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
#define SNDP_NTC_REF_RESISTENCE			    (47000)	//ohm

#define SNDP_NTC_ADC_CHAN					(HAL_GPADC_CHAN_2)


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
	
	{-20, 1135000},
	{-19, 1068000},
	{-18, 1004000},
	{-17, 945000},
	{-16, 889600},
	{-15, 837800},
	{-10, 624100},
    { -5, 469100},
    { -4, 443500},
    { -3, 419500},
    { -2, 396900},
    { -1, 375600},
	{  0, 355600},
    {  1, 336800},
    {  2, 319100},
    {  3, 302400},
    {  4, 286700},
	{  5, 271800},
    {  6, 257800},
    {  7, 244700},
    {  8, 232200},
    {  9, 220500},
	{ 10, 209400},
    { 11, 198900},
    { 12, 189000},
    { 13, 179700},
    { 14, 170900},
	{ 15, 162500},
    { 16, 154600},
    { 17, 147200},
    { 18, 140100},
    { 19, 133400},
	{ 20, 127000},
    { 21, 121000},
    { 22, 115400},
    { 23, 110000},
    { 24, 104800},
	{ 25, 100000},
    { 26,  95400},
    { 27,  91040},
    { 28,  86900},
    { 29,  82970},
	{ 30,  79230},
    { 31,  75690},
    { 32,  72320},
    { 33,  69120},
    { 34,  66070},
	{ 35,  63180},
    { 36,  60420},
    { 37,  57810},
    { 38,  55310},
    { 39,  52940},
	{ 40,  50680},
    { 41,  48530},
    { 42,  46940},
    { 43,  44530},
    { 44,  42670},
	{ 45,  40900},
    { 46,  39210},
    { 47,  37600},
    { 48,  36060},
    { 49,  34600},
	{ 50,  33190},
	{ 55,  27090},
	{ 56,  26030},
	{ 57,  25010},
	{ 58,  24040},
	{ 59,  23110},
	{ 60,  22220},
	{ 99,	0},
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
	//if(sndp_ntc_ctx.curr_temp != sndp_ntc_ctx.last_temp) {
		sndp_ntc_ctx.last_temp = sndp_ntc_ctx.curr_temp;
		
		if(p_sndp_hal_ntc_measure_cb) {
			p_sndp_hal_ntc_measure_cb(sndp_ntc_ctx.curr_temp, sndp_ntc_ctx.voltage);
		}

	//}
	
	return sndp_ntc_ctx.curr_temp;
	
}

static void sndp_temp_ntc_adc_irq_handler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{

	if(volt == HAL_GPADC_BAD_VALUE){
		TEMP_NTC_TRACE(0, "volt BAD, return");
		return;
	}

    TEMP_NTC_TRACE(0, "volt=%d", volt);
    
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

