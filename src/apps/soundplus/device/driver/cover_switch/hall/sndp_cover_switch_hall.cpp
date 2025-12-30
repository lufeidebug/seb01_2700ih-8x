#if defined(__SNDP_COVER_SWITCH_HALL__)
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
#include "sndp_hal_common.h"
#include "sndp_hal_cover_switch.h"
#include "sndp_cover_switch_hall.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define COVER_SWITCH_INT_DEBOUNCE_REPEAT_MS				(30)
#define COVER_SWITCH_INT_DEBOUNCE_OPEN_DELAY_MS			(200)
#define COVER_SWITCH_INT_DEBOUNCE_CLOSE_DELAY_MS		(500)

#define COVER_SWITCH_HALL_OPEN_VAL						(0)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_cs_hall_debounce_timeout_handler(void const *param);
static int32_t sndp_cs_hall_get_curr_status(sndp_hal_cover_status_e *status);

/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool cs_hall_init = false;
static sndp_hal_cs_status_changed_callback cs_hall_status_changed_cb_ptr = NULL;

osTimerDef(CS_HALL_DEBOUNCE_TIMER, sndp_cs_hall_debounce_timeout_handler);
static osTimerId cs_hall_debounce_timer = NULL;
static struct HAL_GPIO_IRQ_CFG_T cs_hall_gpiocfg;
	
static sndp_hal_cover_status_e cs_cover_status = SNDP_HAL_COVER_UNKNOWN;


/**************************************************************************************************
* Function
**************************************************************************************************/
static void sndp_cs_hall_debounce_timeout_handler(void const *param)
{
	sndp_hal_cover_status_e curr_status;
	
	if(sndp_cs_hall_get_curr_status(&curr_status) == SNDP_HAL_RET_OK) {
		
		CS_HALL_TRACE(2, "pre_status=%d, curr_status=%d", cs_cover_status, curr_status);

		if(curr_status != cs_cover_status) {
			cs_cover_status = curr_status;
			
			if(cs_hall_status_changed_cb_ptr) {
				cs_hall_status_changed_cb_ptr(cs_cover_status);
			}
		}
	}
}

static void sndp_cs_hall_debounce_start(void)
{
	sndp_hal_cover_status_e curr_status;
	uint32_t delay_time = COVER_SWITCH_INT_DEBOUNCE_OPEN_DELAY_MS;
	
	if(sndp_cs_hall_get_curr_status(&curr_status) == SNDP_HAL_RET_OK) {
		if(curr_status == SNDP_HAL_COVER_COLSED) {
			delay_time = COVER_SWITCH_INT_DEBOUNCE_CLOSE_DELAY_MS;
		}
	}
	
	osTimerStop(cs_hall_debounce_timer);
	osTimerStart(cs_hall_debounce_timer, delay_time);
}

static void sndp_cs_hall_int_disable(void)
{
    cs_hall_gpiocfg.irq_enable = false;
    hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin, &cs_hall_gpiocfg);
}

static void sndp_cs_hall_int_enable(void)
{
	uint8_t val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin);
	
	if(val)
    	cs_hall_gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
	else
		cs_hall_gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;

	cs_hall_gpiocfg.irq_enable = true;
    hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin, &cs_hall_gpiocfg);
}

static void sndp_cs_hall_irq_irqhandler(enum HAL_GPIO_PIN_T pin)
{
	static uint32_t last_ticks = 0;
	uint32_t curr_ticks;
	uint32_t past_ticks;

	sndp_cs_hall_int_disable();
	curr_ticks = hal_sys_timer_get();
	past_ticks = hal_timer_get_passed_ticks(curr_ticks, last_ticks);
	
	CS_HALL_TRACE(2, "past_ms=%d, repeat_ms=%d", TICKS_TO_MS(past_ticks), COVER_SWITCH_INT_DEBOUNCE_REPEAT_MS);

	if(TICKS_TO_MS(past_ticks) >= COVER_SWITCH_INT_DEBOUNCE_REPEAT_MS) {
		last_ticks = curr_ticks;
		
		sndp_call_func_in_dev_thread((uint32_t)sndp_cs_hall_debounce_start, 0, 0, 0);
	}
	sndp_cs_hall_int_enable();
}

static void sndp_cs_hall_int_init(void)
{
	if(app_hall_int_pin_cfg.pin !=  HAL_IOMUX_PIN_NUM) {
	    cs_hall_gpiocfg.irq_enable = true;
	    cs_hall_gpiocfg.irq_debounce = true;
		cs_hall_gpiocfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
	    //cs_hall_gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
	    cs_hall_gpiocfg.irq_handler = sndp_cs_hall_irq_irqhandler;
	    
	    sndp_cs_hall_int_enable();
	}
}

static int32_t sndp_cs_hall_init(void)
{
	if(cs_hall_init) {
		CS_HALL_TRACE(0, "already initialized");
		return SNDP_HAL_RET_OK;
	}
		
	if(cs_hall_debounce_timer == NULL)
        cs_hall_debounce_timer = osTimerCreate (osTimer(CS_HALL_DEBOUNCE_TIMER), osTimerOnce, NULL);

	ASSERT(cs_hall_debounce_timer != NULL, "%s, sndp_if_delay_exec_timer == NULL", __func__);

	sndp_cs_hall_int_init();

	cs_hall_init = true;
	CS_HALL_TRACE(0, "init done.");
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_cs_hall_set_status_changed_callback(sndp_hal_cs_status_changed_callback callback)
{
	cs_hall_status_changed_cb_ptr = callback;
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_cs_hall_get_curr_status(sndp_hal_cover_status_e *status)
{
	ASSERT(status != NULL, "%s, status == NULL", __func__);
	
	if(app_hall_int_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
		uint8_t val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin);
		*status = (val == COVER_SWITCH_HALL_OPEN_VAL) ? (SNDP_HAL_COVER_COLSED) : (SNDP_HAL_COVER_OPENED);
		CS_HALL_TRACE(1, "status=%d", *status);
		return SNDP_HAL_RET_OK;
	} else {
		return SNDP_HAL_RET_FAIL;
	}
}

static int32_t sndp_cs_hall_check_curr_status(void)
{
	sndp_cs_hall_debounce_timeout_handler(NULL);
	return SNDP_HAL_RET_OK;
}


const sndp_hal_cover_switch_s sndp_hal_cover_switch_hall = {
	.init							= sndp_cs_hall_init,
    .set_status_changed_callback	= sndp_cs_hall_set_status_changed_callback,
    .get_curr_status				= sndp_cs_hall_get_curr_status,
    .check_curr_status				= sndp_cs_hall_check_curr_status,
};


#endif	//__SNDP_COVER_SWITCH_HALL__