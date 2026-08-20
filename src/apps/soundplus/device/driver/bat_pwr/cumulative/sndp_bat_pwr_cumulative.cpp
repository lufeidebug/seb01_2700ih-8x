
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
    { 100,	4359 },
    {  99,	4330 },
    {  98,	4317 },
    {  97,	4308 },
    {  96,	4297 },
    {  95,	4289 },
    {  94,	4276 },
    {  93,	4265 },
    {  92,	4257 },
    {  91,	4243 },
    {  90,	4235 },
    {  89,	4223 },
    {  88,	4212 },
    {  87,	4200 },
    {  86,	4192 },
    {  85,	4178 },
    {  84,	4166 },
    {  83,	4155 },
    {  82,	4144 },
    {  81,	4135 },
    {  80,	4121 },
    {  79,	4114 },
    {  78,	4101 },
    {  77,	4088 },
    {  76,	4081 },
    {  75,	4068 },
    {  74,	4057 },
    {  73,	4048 },
    {  72,	4035 },
    {  71,	4027 },
    {  70,	4012 },
    {  69,	4004 },
    {  68,	3991 },
    {  67,	3982 },
    {  66,	3973 },
    {  65,	3962 },
    {  64,	3954 },
    {  63,	3944 },
    {  62,	3936 },
    {  61,	3926 },
    {  60,	3920 },
    {  59,	3910 },
    {  58,	3903 },
    {  57,	3898 },
    {  56,	3890 },
    {  55,	3882 },
    {  54,	3876 },
    {  53,	3873 },
    {  52,	3864 },
    {  51,	3856 },
    {  50,	3852 },
    {  49,	3843 },
    {  48,	3835 },
    {  47,	3832 },
    {  46,	3823 },
    {  45,	3816 },
    {  44,	3810 },
    {  43,	3804 },
    {  42,	3798 },
    {  41,	3792 },
    {  40,	3784 },
    {  39,	3775 },
    {  38,	3769 },
    {  37,	3763 },
    {  36,	3753 },
    {  35,	3747 },
    {  34,	3740 },
    {  33,	3730 },
    {  32,	3723 },
    {  31,	3714 },
    {  30,	3708 },
    {  29,	3697 },
    {  28,	3690 },
    {  27,	3686 },
    {  26,	3681 },
    {  25,	3675 },
    {  24,	3672 },
    {  23,	3664 },
    {  22,	3654 },
    {  21,	3637 },
    {  20,	3622 },
    {  19,	3607 },
    {  18,	3592 },
    {  17,	3575 },
    {  16,	3560 },
    {  15,	3543 },
    {  14,	3521 },
    {  13,	3502 },
    {  12,	3480 },
    {  11,	3458 },
    {  10,	3435 },
    {   9,	3414 },
    {   8,	3393 },
    {   7,	3372 },
    {   6,	3349 },
    {   5,	3325 },
    {   4,	3300 },
    {   3,	3275 },
    {   2,	3250 },
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

