#if defined(__SNDP_BAT_LOOKUP_TABLE__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_gpadc.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_battery.h"
#include "sndp_bat_pwr_lkt.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SP_HAL_GPADC_BAD_VALUE			(0xFFFF)


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_bat_lkt_ctx_s bat_lkt_ctx;
static sndp_hal_battery_measure_callback bat_measure_cb_ptr;

static const uint32_t bat_volt_per_table[][2] = {
	{3200, 0},
	{3350, 5},
	{3450, 10},
	{3520, 15},
	{3580, 20}, 
	{3620, 25},
	{3650, 30},
	{3680, 35},
	{3710, 40},
	{3740, 45},
	{3770, 50},
	{3800, 55},
	{3830, 60},
	{3860, 65},
	{3900, 70},
	{3940, 75},
	{3980, 80},
	{4020, 85}, 
	{4060, 90},
	{4100, 95},
	{4200, 100},
};

static const uint32_t bat_volt_per_charging_table[][2] = {
	{3200, 0},
	{3350, 5},
	{3450, 10},
	{3520, 15},
	{3580, 20}, 
	{3620, 25},
	{3650, 30},
	{3680, 35},
	{3710, 40},
	{3740, 45},
	{3770, 50},
	{3800, 55},
	{3830, 60},
	{3860, 65},
	{3900, 70},
	{3940, 75},
	{3980, 80},
	{4020, 85}, 
	{4060, 90},
	{4100, 95},
	{4200, 100},
};

#if 0
static uint16_t sndp_bat_volt_per_table_normal[101] = {
	4200,	/* 100 */
	4185, 4170, 4155, 4140, 4125, 4110, 4095, 4080, 4065, 4050,	/* 99 ~ 90*/
	4040, 4030, 4020, 4010, 4000, 3990, 3980, 3970, 3960, 3950,	/* 89 ~ 80*/
	3940, 3930, 3920, 3910, 3900, 3890, 3880, 3870, 3860, 3850,	/* 79 ~ 70*/
	3840, 3830, 3820, 3810, 3800, 3790, 3780, 3770, 3760, 3750,	/* 69 ~ 60*/
	3740, 3730, 3720, 3710, 3700, 3690, 3680, 3670, 3660, 3650,	/* 59 ~ 50*/
	3640, 3630, 3620, 3610, 3600, 3590, 3580, 3570, 3560, 3550,	/* 49 ~ 40*/
	3540, 3530, 3520, 3510, 3500, 3490, 3480, 3470, 3460, 3450,	/* 39 ~ 30*/
	3440, 3430, 3420, 3410, 3400, 3390, 3380, 3370, 3360, 3350, /* 29 ~ 20*/
	3340, 3330, 3320, 3310, 3300, 3290, 3280, 3270, 3260, 3250,	/* 19 ~ 10*/
	3240, 3225, 3210, 3195, 3180, 3165, 3150, 3135, 3120, 3100,	/* 09 ~ 00*/
};

static uint16_t sndp_bat_volt_per_table_charging[101] = {
	3100, 3115, 3130, 3145, 3160, 3175, 3190, 3205, 3220, 3235,	/* 00 ~ 09*/
	3250, 3262, 3274, 3286, 3298, 3310, 3322, 3334, 3346, 3358,	/* 10 ~ 19*/
	3370, 3382, 3394, 3406, 3418, 3430, 3442, 3454, 3466, 3478,	/* 20 ~ 19*/
	3490, 3502, 3514, 3526, 3538, 3550, 3562, 3574, 3586, 3598,	/* 30 ~ 29*/
	3610, 3620, 3630, 3640, 3650, 3660, 3670, 3680, 3690, 3700,	/* 40 ~ 39*/
	3710, 3720, 3730, 3740, 3750, 3760, 3770, 3780, 3790, 3800,	/* 50 ~ 49*/
	3810, 3820, 3830, 3840, 3850, 3860, 3870, 3880, 3890, 3900,	/* 60 ~ 59*/
	3910, 3920, 3930, 3940, 3950, 3960, 3970, 3980, 3990, 4000,	/* 70 ~ 69*/
	4010, 4020, 4030, 4040, 4050, 4060, 4070, 4080, 4090, 4100,	/* 80 ~ 89*/
	4110, 4119, 4128, 4137, 4146, 4155, 4164, 4173, 4182, 4191,	/* 90 ~ 99*/
	4200, 	/* 100 */
};
#endif


/**************************************************************************************************
* Function
**************************************************************************************************/

#if 0
static uint8_t sndp_bat_volt_to_per(bool is_charging, uint16_t volt)
{
	uint8_t per = 0;
	
	if(is_charging) {
     	for(uint8_t i = 0;  i < sizeof(sndp_bat_volt_per_table_charging)/sizeof(sndp_bat_volt_per_table_charging[0]); i++) {
			if(volt <= sndp_bat_volt_per_table_charging[i]) {
				per = i;
				break;
			}
     	}
	} else {
		for(uint8_t i = 0;  i < sizeof(sndp_bat_volt_per_table_normal)/sizeof(sndp_bat_volt_per_table_normal[0]); i++) {
			if(volt >= sndp_bat_volt_per_table_normal[i]) {
				per = 100 -i;
				break;
			}
     	}
	}

	TRACE(5, "%s, charging=%d, volt=%d, per=%d, level=%d", __func__, is_charging, volt, per, per/10);
	return per;
}
#endif

static int32_t sndp_bat_conver_volt_to_percentage(void)
{
	uint16_t per = 100;
	uint16_t volt = bat_lkt_ctx.curr.bat_volt;

	if(bat_lkt_ctx.charge_status == SNDP_HAL_BAT_CHARGING) {
		for(uint32_t i = 0; i < sizeof(bat_volt_per_charging_table)/sizeof(bat_volt_per_charging_table[0]); i++){
			if(volt < bat_volt_per_charging_table[i][0]){
				per =  bat_volt_per_charging_table[i][1];
				break;
			}
		}
	} else {
		for(uint32_t i = 0; i < sizeof(bat_volt_per_table)/sizeof(bat_volt_per_table[0]); i++){
			if(volt < bat_volt_per_table[i][0]){
				per =  bat_volt_per_table[i][1];
				break;
			}
		}
	}

	bat_lkt_ctx.curr.bat_per = per;

	if(bat_lkt_ctx.charge_status == SNDP_HAL_BAT_CHARGED_FULL) {
		bat_lkt_ctx.curr.bat_per = 100;
	}

	bat_lkt_ctx.curr.bat_level = bat_lkt_ctx.curr.bat_per/10;
	if(bat_lkt_ctx.curr.bat_level > 9)
		bat_lkt_ctx.curr.bat_level = 9;

	BAT_LKT_TRACE(1, "charge_status=%d", bat_lkt_ctx.charge_status);
	BAT_LKT_TRACE(2, "last, bat_volt=%4d, bat_per=%3d", bat_lkt_ctx.last.bat_volt, bat_lkt_ctx.last.bat_per);
	BAT_LKT_TRACE(2, "curr, bat_volt=%4d, bat_per=%3d", bat_lkt_ctx.curr.bat_volt, bat_lkt_ctx.curr.bat_per);

	if(bat_lkt_ctx.last.bat_per != bat_lkt_ctx.curr.bat_per) {
		bat_lkt_ctx.last.bat_volt = bat_lkt_ctx.curr.bat_volt;
		bat_lkt_ctx.last.bat_per = bat_lkt_ctx.curr.bat_per;
		bat_lkt_ctx.last.bat_level = bat_lkt_ctx.curr.bat_level;
		if(bat_measure_cb_ptr != NULL) {
    		bat_measure_cb_ptr(bat_lkt_ctx.last);
    	}
	}
	return 0;
}


static void sndp_bat_lkt_adc_irq_handler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{

	uint32_t volt_sum = 0;

	if(volt == HAL_GPADC_BAD_VALUE){
		BAT_LKT_TRACE(0, "volt == HAL_GPADC_BAD_VALUE");
		return;
	}

	bat_lkt_ctx.measure_volt[bat_lkt_ctx.measure_index%SP_BAT_STABLE_COUNT] = volt<<2;
	bat_lkt_ctx.measure_index++;

	uint32_t i;
	for(i = 0; i < bat_lkt_ctx.measure_index && i < SP_BAT_STABLE_COUNT; i++)
		volt_sum += bat_lkt_ctx.measure_volt[i];
	
	bat_lkt_ctx.curr.bat_volt = volt_sum / i;
	BAT_LKT_TRACE(1, "curr.bat_volt=%d", bat_lkt_ctx.curr.bat_volt);
	sndp_call_func_in_dev_thread((uint32_t)sndp_bat_conver_volt_to_percentage, 0, 0, 0);	
	
}

static int32_t sndp_bat_lkt_init(sndp_hal_bat_charging_status_e charging_status, uint8_t init_bat_per)
{
    BAT_LKT_TRACE(1, "charging_status=%d, init_bat_per=%d", charging_status, init_bat_per);
    
    memset(&bat_lkt_ctx, 0, sizeof(bat_lkt_ctx));
    bat_lkt_ctx.charge_status = charging_status;
    bat_lkt_ctx.last.bat_per = init_bat_per;
	
    return SNDP_HAL_RET_OK;
}

static int32_t sndp_bat_lkt_set_measure_callback(sndp_hal_battery_measure_callback callback)
{
    BAT_LKT_TRACE(1, "...");
	bat_measure_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

static int32_t sndp_bat_lkt_measure(sndp_hal_bat_charging_status_e charging_status)
{
	BAT_LKT_TRACE(1, "charging_status=%d", charging_status);
	
	bat_lkt_ctx.charge_status  = charging_status;
	hal_gpadc_open(HAL_GPADC_CHAN_BATTERY, HAL_GPADC_ATP_ONESHOT, sndp_bat_lkt_adc_irq_handler);
    return SNDP_HAL_RET_OK;
}

const sndp_hal_battery_s sndp_hal_bat_lkt = {
    .init							= sndp_bat_lkt_init,
	.set_measure_callback			= sndp_bat_lkt_set_measure_callback,
    .measure						= sndp_bat_lkt_measure,
};


#endif	//__SNDP_BAT_LOOKUP_TABLE__
