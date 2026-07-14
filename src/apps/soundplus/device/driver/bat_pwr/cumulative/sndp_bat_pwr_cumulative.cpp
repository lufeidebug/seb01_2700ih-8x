
#if defined(__SNDP_BAT_CUMULATIVE__)

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
#include "sndp_bat_pwr_cumulative.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define BAT_STABLE_COUNT                    (5)


#define BAT_MAX_VOLTAGE                         (4400)
#define BAT_MIN_VOLTAGE                         (3200)

#define BAT_TOTAL_CAPACITY                      (37*1000)  //uAh

#define BAT_CHARGING_PHASE_1_SETP_CAP           (10 * 10)  //uAh
#define BAT_CHARGING_PHASE_2_SETP_CAP           (20 * 10)  //uAh
#define BAT_CHARGING_PHASE_3_SETP_CAP           (15 * 10)  //uAh
#define BAT_CHARGING_PHASE_4_SETP_CAP           (10 * 10)  //uAh
#define BAT_CHARGING_PHASE_5_SETP_CAP           (5 * 10)  //uAh

#define BAT_CHARGING_PHASE_1_CAP_UPPER_LIMIT    (2*1000)  //uAh
#define BAT_CHARGING_PHASE_2_CAP_UPPER_LIMIT    (29*1000)  //uAh
#define BAT_CHARGING_PHASE_3_CAP_UPPER_LIMIT    (32*1000)  //uAh
#define BAT_CHARGING_PHASE_4_CAP_UPPER_LIMIT    (36*1000)  //uAh
#define BAT_CHARGING_PHASE_5_CAP_UPPER_LIMIT    (BAT_TOTAL_CAPACITY)  //uAh


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    sndp_hal_bat_charging_status_e charge_status;

    sndp_hal_bat_info_s last;
    sndp_hal_bat_info_s curr;

    uint16_t measure_volt[BAT_STABLE_COUNT];
    uint16_t measure_index;

    uint16_t charging_cnt;
    uint32_t charging_cap;
} bat_cumulative_ctx_s;
    

typedef struct {
    uint8_t percent;
    uint16_t voltage;
} bat_percent_volt_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bat_cumulative_ctx_s bat_cumulative_ctx;
static sndp_hal_battery_measure_callback bat_measure_cb_ptr;

static const bat_percent_volt_s bat_percent_volt_table[] = {
    { 100,	4280 },
    {  99,	4250 },
    {  98,	4230 },
    {  97,	4215 },
    {  96,	4200 },
    {  95,	4185 },
    {  94,	4175 },
    {  93,	4160 },
    {  92,	4150 },
    {  91,	4135 },
    {  90,	4125 },
    {  89,	4110 },
    {  88,	4100 },
    {  87,	4090 },
    {  86,	4080 },
    {  85,	4070 },
    {  84,	4055 },
    {  83,	4045 },
    {  82,	4035 },
    {  81,	4025 },
    {  80,	4010 },
    {  79,	4000 },
    {  78,	3990 },
    {  77,	3980 },
    {  76,	3970 },
    {  75,	3960 },
    {  74,	3950 },
    {  73,	3940 },
    {  72,	3930 },
    {  71,	3920 },
    {  70,	3910 },
    {  69,	3900 },
    {  68,	3890 },
    {  67,	3880 },
    {  66,	3870 },
    {  65,	3863 },
    {  64,	3854 },
    {  63,	3845 },
    {  62,	3837 },
    {  61,	3829 },
    {  60,	3821 },
    {  59,	3813 },
    {  58,	3805 },
    {  57,	3797 },
    {  56,	3789 },
    {  55,	3781 },
    {  54,	3773 },
    {  53,	3766 },
    {  52,	3758 },
    {  51,	3750 },
    {  50,	3743 },
    {  49,	3736 },
    {  48,	3729 },
    {  47,	3722 },
    {  46,	3715 },
    {  45,	3709 },
    {  44,	3703 },
    {  43,	3696 },
    {  42,	3690 },
    {  41,	3684 },
    {  40,	3678 },
    {  39,	3672 },
    {  38,	3666 },
    {  37,	3661 },
    {  36,	3655 },
    {  35,	3650 },
    {  34,	3645 },
    {  33,	3640 },
    {  32,	3634 },
    {  31,	3628 },
    {  30,	3622 },
    {  29,	3617 },
    {  28,	3611 },
    {  27,	3605 },
    {  26,	3598 },
    {  25,	3591 },
    {  24,	3583 },
    {  23,	3575 },
    {  22,	3566 },
    {  21,	3556 },
    {  20,	3545 },
    {  19,	3533 },
    {  18,	3519 },
    {  17,	3504 },
    {  16,	3487 },
    {  15,	3469 },
    {  14,	3449 },
    {  13,	3426 },
    {  12,	3403 },
    {  11,	3377 },
    {  10,	3350 },
    {   9,	3335 },
    {   8,	3320 },
    {   7,	3305 },
    {   6,	3290 },
    {   5,	3275 },
    {   4,	3260 },
    {   3,	3245 },
    {   2,	3230 },
    {   1,	3215 },
    {   0,  BAT_MIN_VOLTAGE},
};


/**************************************************************************************************
* Function
**************************************************************************************************/
static int32_t bat_cumulative_info_changed(void)
{
	//BAT_CUMU_TRACE(0, "...");

	if(bat_measure_cb_ptr != NULL) {
		bat_measure_cb_ptr(bat_cumulative_ctx.last);
	}

	return 0;
}

static uint8_t bat_cumulative_get_percent_by_voltage(uint32_t voltage)
{
    int i;
    int length;
    uint8_t bat_percent;
    
    length = sizeof(bat_percent_volt_table)/sizeof(bat_percent_volt_s);
    for(i = 0; i < length; i++) {
        if(voltage >= bat_percent_volt_table[i].voltage) {
            break;
        }
    }

    if(i >= length) {
        bat_percent = 0;
    } else {
        bat_percent = bat_percent_volt_table[i].percent;
    }

    return bat_percent;
}

/*
* 10s执行一次。
*/
static int32_t bat_cumulative_conver_volt_to_percentage(void)
{
	uint16_t per = 0;
	uint16_t volt = bat_cumulative_ctx.curr.bat_volt;

	if(bat_cumulative_ctx.charge_status == SNDP_HAL_BAT_CHARGING) {
        bat_cumulative_ctx.charging_cnt++;

        if(bat_cumulative_ctx.charging_cnt == 1) {
            if(volt > 4130) {
                volt -= 10;
            } else if(volt > 4100) {
                volt -= 20;
            } else if(volt > 4080) {
                volt -= 30;
            } else if(volt > BAT_MIN_VOLTAGE) {
                volt -= 50;
            }
            
            per = bat_cumulative_get_percent_by_voltage(volt);
            bat_cumulative_ctx.charging_cap = BAT_TOTAL_CAPACITY * per / 100;
            
        } else {
            if(bat_cumulative_ctx.charging_cap < BAT_CHARGING_PHASE_1_CAP_UPPER_LIMIT) {
                bat_cumulative_ctx.charging_cap += BAT_CHARGING_PHASE_1_SETP_CAP;
    		} else if(bat_cumulative_ctx.charging_cap < BAT_CHARGING_PHASE_2_CAP_UPPER_LIMIT) {
                bat_cumulative_ctx.charging_cap += BAT_CHARGING_PHASE_2_SETP_CAP;
    		} else if(bat_cumulative_ctx.charging_cap < BAT_CHARGING_PHASE_3_CAP_UPPER_LIMIT) {
    	        bat_cumulative_ctx.charging_cap += BAT_CHARGING_PHASE_3_SETP_CAP;
    		} else if(bat_cumulative_ctx.charging_cap < BAT_CHARGING_PHASE_4_CAP_UPPER_LIMIT) {
                bat_cumulative_ctx.charging_cap += BAT_CHARGING_PHASE_4_SETP_CAP;
    		} else if(bat_cumulative_ctx.charging_cap < BAT_TOTAL_CAPACITY) {
                bat_cumulative_ctx.charging_cap += BAT_CHARGING_PHASE_5_SETP_CAP;
    		}

            if(bat_cumulative_ctx.charging_cap >= BAT_TOTAL_CAPACITY) {
                bat_cumulative_ctx.charging_cap = BAT_TOTAL_CAPACITY - 1;    //99%
    		}

    		per = bat_cumulative_ctx.charging_cap * 100 / BAT_TOTAL_CAPACITY;
        }

        SNDP_TRACE(1, "volt2per: cnt=%d, v=%d, p=%d, cap=%d", 
            bat_cumulative_ctx.charging_cnt,
            volt,
            per,
            bat_cumulative_ctx.charging_cap);

	} else if(bat_cumulative_ctx.charge_status == SNDP_HAL_BAT_CHARGED_FULL) {
        bat_cumulative_ctx.charging_cnt = 0;
        
		per = 100;
		bat_cumulative_ctx.charging_cap = BAT_TOTAL_CAPACITY;

	} else if(bat_cumulative_ctx.charge_status == SNDP_HAL_BAT_NOT_CHARGING){
        bat_cumulative_ctx.charging_cnt = 0;
        
		per = bat_cumulative_get_percent_by_voltage(volt);
        
		/* In the discharging state, the current percentage can not higher than the last percentage.*/
		if(per > bat_cumulative_ctx.last.bat_per) {
			if(per == 100 && bat_cumulative_ctx.last.bat_per == 99) {
			} else {
				per = bat_cumulative_ctx.last.bat_per;
			}
		}
        
		bat_cumulative_ctx.charging_cap = BAT_TOTAL_CAPACITY*per/100;
	}

	bat_cumulative_ctx.curr.bat_per = per;
	bat_cumulative_ctx.curr.bat_level = bat_cumulative_ctx.curr.bat_per/10;
	if(bat_cumulative_ctx.curr.bat_level > 9) {
		bat_cumulative_ctx.curr.bat_level = 9;
	}

	SNDP_TRACE(1, "volt2per: sta=%d, lv=%4d, lp=%3d, cv=%4d, cp=%3d", 
            bat_cumulative_ctx.charge_status,
            bat_cumulative_ctx.last.bat_volt, bat_cumulative_ctx.last.bat_per,
            bat_cumulative_ctx.curr.bat_volt, bat_cumulative_ctx.curr.bat_per);

#if 0 //test
	
	bat_cumulative_ctx.last.bat_volt = 3300;
	bat_cumulative_ctx.last.bat_per = 0;
	bat_cumulative_ctx.last.bat_level = 0;
	sndp_call_func_in_app_thread((uint32_t) bat_cumulative_info_changed, 0, 0, 0);
#else
	bat_cumulative_ctx.last.bat_volt = bat_cumulative_ctx.curr.bat_volt;
	bat_cumulative_ctx.last.bat_per = bat_cumulative_ctx.curr.bat_per;
	bat_cumulative_ctx.last.bat_level = bat_cumulative_ctx.curr.bat_level;
	sndp_call_func_in_app_thread((uint32_t) bat_cumulative_info_changed, 0, 0, 0);
#endif
	
	return 0;
}

static void bat_cumulative_adc_irq_handler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{
	uint32_t volt_sum = 0;

	if(volt == HAL_GPADC_BAD_VALUE){
		BAT_CUMU_TRACE(0, "volt == HAL_GPADC_BAD_VALUE");
		return;
	}

	bat_cumulative_ctx.measure_volt[bat_cumulative_ctx.measure_index%BAT_STABLE_COUNT] = volt<<2;
	bat_cumulative_ctx.measure_index++;

	uint32_t i;
	for(i = 0; i < bat_cumulative_ctx.measure_index && i < BAT_STABLE_COUNT; i++) {
		volt_sum += bat_cumulative_ctx.measure_volt[i];
	}
	
	bat_cumulative_ctx.curr.bat_volt = volt_sum / i;
	BAT_CUMU_TRACE(1, "curr.bat_volt=%d", bat_cumulative_ctx.curr.bat_volt);
	sndp_call_func_in_dev_thread((uint32_t)bat_cumulative_conver_volt_to_percentage, 0, 0, 0);	
	
}

static int32_t bat_cumulative_init(sndp_hal_bat_charging_status_e charging_status, uint8_t init_bat_per)
{
    memset(&bat_cumulative_ctx, 0, sizeof(bat_cumulative_ctx));
    bat_cumulative_ctx.charge_status = charging_status;
    bat_cumulative_ctx.last.bat_per = init_bat_per;
	
    return SNDP_HAL_RET_OK;
}

static int32_t bat_cumulative_set_measure_callback(sndp_hal_battery_measure_callback callback)
{
	bat_measure_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

static int32_t bat_cumulative_measure(sndp_hal_bat_charging_status_e charging_status)
{
	BAT_CUMU_TRACE(1, "charging_status=%d", charging_status);
	
	bat_cumulative_ctx.charge_status  = charging_status;
	hal_gpadc_open(HAL_GPADC_CHAN_BATTERY, HAL_GPADC_ATP_ONESHOT, bat_cumulative_adc_irq_handler);
    return SNDP_HAL_RET_OK;
}

const sndp_hal_battery_s sndp_hal_bat_cumulative = {
    .init							= bat_cumulative_init,
	.set_measure_callback			= bat_cumulative_set_measure_callback,
    .measure						= bat_cumulative_measure,
};


#endif /* __SNDP_BAT_CUMULATIVE__ */

