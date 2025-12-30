
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


#define BAT_MAX_VOLTAGE                         (4200)
#define BAT_MIN_VOLTAGE                         (3300)

#define BAT_TOTAL_CAPACITY                      (70*1000)  //uAh

#define BAT_CHARGING_PHASE_1_SETP_CAP           (10 * 10)  //uAh
#define BAT_CHARGING_PHASE_2_SETP_CAP           (19 * 10)  //uAh
#define BAT_CHARGING_PHASE_3_SETP_CAP           (13 * 10)  //uAh
#define BAT_CHARGING_PHASE_4_SETP_CAP           (9 * 10)  //uAh
#define BAT_CHARGING_PHASE_5_SETP_CAP           (6 * 10)  //uAh

#define BAT_CHARGING_PHASE_1_CAP_UPPER_LIMIT    (2*1000)  //uAh
#define BAT_CHARGING_PHASE_2_CAP_UPPER_LIMIT    (60*1000)  //uAh
#define BAT_CHARGING_PHASE_3_CAP_UPPER_LIMIT    (65*1000)  //uAh
#define BAT_CHARGING_PHASE_4_CAP_UPPER_LIMIT    (68*1000)  //uAh
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
    { 100,	4151 },
    {  99,	4127 },
    {  98,	4108 },
    {  97,	4094 },
    {  96,	4085 },
    {  95,	4078 },
    {  94,	4072 },
    {  93,	4067 },
    {  92,	4064 },
    {  91,	4061 },
    {  90,	4057 },
    {  89,	4053 },
    {  88,	4049 },
    {  87,	4044 },
    {  86,	4037 },
    {  85,	4030 },
    {  84,	4023 },
    {  83,	4014 },
    {  82,	4005 },
    {  81,	3995 },
    {  80,	3985 },
    {  79,	3974 },
    {  78,	3965 },
    {  77,	3955 },
    {  76,	3946 },
    {  75,	3937 },
    {  74,	3928 },
    {  73,	3920 },
    {  72,	3912 },
    {  71,	3904 },
    {  70,	3897 },
    {  69,	3890 },
    {  68,	3882 },
    {  67,	3875 },
    {  66,	3867 },
    {  65,	3859 },
    {  64,	3850 },
    {  63,	3841 },
    {  62,	3833 },
    {  61,	3824 },
    {  60,	3814 },
    {  59,	3804 },
    {  58,	3794 },
    {  57,	3785 },
    {  56,	3775 },
    {  55,	3766 },
    {  54,	3756 },
    {  53,	3747 },
    {  52,	3737 },
    {  51,	3728 },
    {  50,	3718 },
    {  49,	3709 },
    {  48,	3700 },
    {  47,	3692 },
    {  46,	3684 },
    {  45,	3677 },
    {  44,	3669 },
    {  43,	3663 },
    {  42,	3656 },
    {  41,	3649 },
    {  40,	3643 },
    {  39,	3637 },
    {  38,	3631 },
    {  37,	3626 },
    {  36,	3620 },
    {  35,	3615 },
    {  34,	3609 },
    {  33,	3604 },
    {  32,	3599 },
    {  31,	3594 },
    {  30,	3589 },
    {  29,	3584 },
    {  28,	3579 },
    {  27,	3573 },
    {  26,	3568 },
    {  25,	3562 },
    {  24,	3556 },
    {  23,	3550 },
    {  22,	3543 },
    {  21,	3535 },
    {  20,	3528 },
    {  19,	3521 },
    {  18,	3513 },
    {  17,	3505 },
    {  16,	3496 },
    {  15,	3487 },
    {  14,	3478 },
    {  13,	3468 },
    {  12,	3458 },
    {  11,	3447 },
    {  10,	3436 },
    {   9,	3425 },
    {   8,	3415 },
    {   7,	3404 },
    {   6,	3395 },
    {   5,	3384 },
    {   4,	3373 },
    {   3,	3359 },
    {   2,	3342 },
    {   1,	3318 },
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

        TRACE(1, "volt2per: cnt=%d, v=%d, p=%d, cap=%d", 
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
			per = bat_cumulative_ctx.last.bat_per;	
		}
        
		bat_cumulative_ctx.charging_cap = BAT_TOTAL_CAPACITY*per/100;
	}

	bat_cumulative_ctx.curr.bat_per = per;
	bat_cumulative_ctx.curr.bat_level = bat_cumulative_ctx.curr.bat_per/10;
	if(bat_cumulative_ctx.curr.bat_level > 9) {
		bat_cumulative_ctx.curr.bat_level = 9;
	}

	TRACE(1, "volt2per: sta=%d, lv=%4d, lp=%3d, cv=%4d, cp=%3d", 
            bat_cumulative_ctx.charge_status,
            bat_cumulative_ctx.last.bat_volt, bat_cumulative_ctx.last.bat_per,
            bat_cumulative_ctx.curr.bat_volt, bat_cumulative_ctx.curr.bat_per);

#if 0
	if(bat_cumulative_ctx.last.bat_per != bat_cumulative_ctx.curr.bat_per) {
		bat_cumulative_ctx.last.bat_volt = bat_cumulative_ctx.curr.bat_volt;
		bat_cumulative_ctx.last.bat_per = bat_cumulative_ctx.curr.bat_per;
		bat_cumulative_ctx.last.bat_level = bat_cumulative_ctx.curr.bat_level;
		spif_call_func_in_app_thread((uint32_t) bat_cumulative_info_changed, 0, 0, 0);
	}
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

