
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
    { 100,	4368 },
    {  99,	4353 },
    {  98,	4338 },
    {  97,	4324 },
    {  96,	4311 },
    {  95,	4298 },
    {  94,	4287 },
    {  93,	4276 },
    {  92,	4265 },
    {  91,	4254 },
    {  90,	4243 },
    {  89,	4232 },
    {  88,	4221 },
    {  87,	4209 },
    {  86,	4198 },
    {  85,	4187 },
    {  84,	4176 },
    {  83,	4165 },
    {  82,	4154 },
    {  81,	4143 },
    {  80,	4132 },
    {  79,	4121 },
    {  78,	4110 },
    {  77,	4099 },
    {  76,	4088 },
    {  75,	4077 },
    {  74,	4066 },
    {  73,	4056 },
    {  72,	4045 },
    {  71,	4034 },
    {  70,	4024 },
    {  69,	4013 },
    {  68,	4002 },
    {  67,	3991 },
    {  66,	3980 },
    {  65,	3970 },
    {  64,	3960 },
    {  63,	3951 },
    {  62,	3942 },
    {  61,	3933 },
    {  60,	3925 },
    {  59,	3917 },
    {  58,	3909 },
    {  57,	3902 },
    {  56,	3894 },
    {  55,	3887 },
    {  54,	3880 },
    {  53,	3873 },
    {  52,	3866 },
    {  51,	3859 },
    {  50,	3853 },
    {  49,	3846 },
    {  48,	3840 },
    {  47,	3833 },
    {  46,	3827 },
    {  45,	3821 },
    {  44,	3815 },
    {  43,	3809 },
    {  42,	3803 },
    {  41,	3797 },
    {  40,	3791 },
    {  39,	3784 },
    {  38,	3778 },
    {  37,	3772 },
    {  36,	3765 },
    {  35,	3758 },
    {  34,	3751 },
    {  33,	3745 },
    {  32,	3737 },
    {  31,	3729 },
    {  30,	3721 },
    {  29,	3713 },
    {  28,	3705 },
    {  27,	3696 },
    {  26,	3688 },
    {  25,	3683 },
    {  24,	3678 },
    {  23,	3674 },
    {  22,	3670 },
    {  21,	3665 },
    {  20,	3657 },
    {  19,	3643 },
    {  18,	3624 },
    {  17,	3606 },
    {  16,	3590 },
    {  15,	3573 },
    {  14,	3556 },
    {  13,	3538 },
    {  12,	3519 },
    {  11,	3498 },
    {  10,	3475 },
    {   9,	3452 },
    {   8,	3428 },
    {   7,	3405 },
    {   6,	3382 },
    {   5,	3358 },
    {   4,	3331 },
    {   3,	3302 },
    {   2,	3270 },
    {   1,	BAT_MIN_VOLTAGE },
    {   0,	0 },


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

