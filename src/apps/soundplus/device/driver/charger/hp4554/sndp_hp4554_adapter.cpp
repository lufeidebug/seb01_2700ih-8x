#if defined(__SNDP_CHARGER_HP4554__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_iomux.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_hal_common.h"
#include "sndp_hal_charger.h"
#include "sndp_hal_charger_plug.h"
#include "sndp_hp4554_adapter.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_CHARGER_INT_DEBOUNCE_REPEAT_MS             (20) //ms
#define SNDP_CHARGER_INT_DEBOUNCE_DELAY_MS              (100) //ms


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_hp4554_status_debounce_timeout_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_charging_mode_changed_callback charging_mode_changed_cb_ptr;

osTimerDef(HP4554_STATUS_DEBOUNCE_TIMER, sndp_hp4554_status_debounce_timeout_handler);
static osTimerId hp4554_status_debounce_timer = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

static void sndp_hp4554_status_debounce_timeout_handler(void const *param)
{
    uint8_t val;
    sndp_hal_charger_mode_e status;
    sndp_hal_charger_plug_status_e charger_plug_sta = SNDP_HAL_CHARGER_PLUGOUT;
        
    
    if (app_charging_status_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
        return;
    }
    
	val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_charging_status_pin_cfg.pin);

    sndp_hal_charger_plug_get_curr_status(&charger_plug_sta);
    if(charger_plug_sta == SNDP_HAL_CHARGER_PLUGIN) {
        if(val) {
		    status = SNDP_HAL_CHARGER_MODE_FULL_CHARGING;
        } else {
            status = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
        }
    } else {
        status = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
    }
	
    HP4554_TRACE(0, "val=%d, sta=%d.", val, status);
    
	if(charging_mode_changed_cb_ptr) {
		sndp_call_func_in_dev_thread((uint32_t)charging_mode_changed_cb_ptr, (uint32_t)status, 0, 0);
	}
}


void sndp_hp4554_status_debounce(void)
{
	osTimerStop(hp4554_status_debounce_timer);
	osTimerStart(hp4554_status_debounce_timer, SNDP_CHARGER_INT_DEBOUNCE_DELAY_MS);
}

static void sndp_hp4554_status_irq_handler(enum HAL_GPIO_PIN_T pin)
{
	static uint32_t last_time = 0;
	uint32_t curr_time = hal_sys_timer_get();
	uint32_t passed_ticks = hal_timer_get_passed_ticks(curr_time, last_time);

	//HP4554_TRACE(1, "passed_ms=%d, repeat_ms=%d", TICKS_TO_MS(passed_ticks), SNDP_CHARGER_INT_DEBOUNCE_REPEAT_MS);
	
	if(TICKS_TO_MS(passed_ticks) >= SNDP_CHARGER_INT_DEBOUNCE_REPEAT_MS) {
		last_time = hal_sys_timer_get();
		sndp_call_func_in_dev_thread((uint32_t)sndp_hp4554_status_debounce, 0, 0, 0);
	}
}

POSSIBLY_UNUSED static void sndp_hp4554_status_irq_config(void)
{
    if (app_charging_status_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
        HAL_GPIO_IRQ_CFG_T cfg;
    	cfg.irq_debounce = true;
    	cfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
    	cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;
    	cfg.irq_handler = sndp_hp4554_status_irq_handler;
    	cfg.irq_enable = true;

    	hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_charging_status_pin_cfg.pin, &cfg);
        //HP4554_TRACE(0, "done.");
    }
    
}

int32_t sndp_hp4554_init(void)
{
	if(hp4554_status_debounce_timer == NULL) {
        hp4554_status_debounce_timer = osTimerCreate (osTimer(HP4554_STATUS_DEBOUNCE_TIMER), osTimerOnce, NULL);
		ASSERT(hp4554_status_debounce_timer != NULL, "%s, %d", __func__, __LINE__);
	}

	sndp_hp4554_status_irq_config();
    HP4554_TRACE(0, "done.");
	return SNDP_HAL_RET_OK;
}


int32_t sndp_hp4554_set_charging_current(sndp_hal_charging_current_e charging_current)
{
    HP4554_TRACE(0, "chg_c=%d", charging_current);
    return 0;
    
	switch(charging_current){
		case SNDP_HAL_CHARGING_CURRENT_ZERO:
            if (app_charging_status_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_set((enum HAL_GPIO_PIN_T)app_charging_enable_pin_cfg.pin);
            }

            if (app_charging_quick_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)app_charging_quick_pin_cfg.pin);
            }
			break;
		case SNDP_HAL_CHARGING_CURRENT_SLOW:
            if (app_charging_status_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)app_charging_enable_pin_cfg.pin);
            }

            if (app_charging_quick_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)app_charging_quick_pin_cfg.pin);
            }
           
			break;
		case SNDP_HAL_CHARGING_CURRENT_HIGH:
            if (app_charging_status_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)app_charging_enable_pin_cfg.pin);
            }

            if (app_charging_quick_pin_cfg.pin == HAL_IOMUX_PIN_NUM){
                hal_gpio_pin_set((enum HAL_GPIO_PIN_T)app_charging_quick_pin_cfg.pin);
            }
			break;

		case SNDP_HAL_CHARGING_CURRENT_HALF:
			break;
			
		case SNDP_HAL_CHARGING_CURRENT_1C:
			break;
			
		case SNDP_HAL_CHARGING_CURRENT_2C:
			break;
			
		case SNDP_HAL_CHARGING_CURRENT_3C:
			break;
			
	}
	return 0;
}

int32_t sndp_hp4554_set_charging_mode_changed_callback(sndp_hal_charging_mode_changed_callback callback)
{
	charging_mode_changed_cb_ptr = callback;
	return 0;
}

int32_t sndp_hp4554_check_curr_status(void)
{
	sndp_call_func_in_dev_thread((uint32_t)sndp_hp4554_status_debounce, 0, 0, 0);
	return 0;
}


bool sndp_hp4554_is_charging_enabled(void)
{
	if(hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_charging_enable_pin_cfg.pin))
		return true;

	return false;
}

const sndp_hal_charger_s sndp_hal_charger_hp4554 = {
	.init									= sndp_hp4554_init,
	.set_charging_current					= sndp_hp4554_set_charging_current,
	.set_charging_mode_changed_callback     = sndp_hp4554_set_charging_mode_changed_callback,
	.check_curr_status                      = sndp_hp4554_check_curr_status,
	.is_charging_enabled					= sndp_hp4554_is_charging_enabled,
};


#endif //__SNDP_CHARGER_HP4554__


