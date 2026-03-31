#if defined(__SNDP_IOBOX_GPIO_INT__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_iomux.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"
#include "app_utils.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_iobox.h"
#include "sndp_iobox_gpio_int.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define IOBOX_INT_DEBOUNCE_REPEAT_MS			(70)
#define IOBOX_INT_INBOX_DEBOUNCE_DELAYE_MS		(100)
#define IOBOX_INT_OUTBOX_DEBOUNCE_DELAYE_MS		(200)

#define IOBOX_INT_IN_BOX_VAL					(1)

/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_iobox_gpio_int_debounce_timer_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool iob_gpio_init = false;
static sndp_hal_iobox_status_changed_callback iob_gpio_status_changed_cb_ptr = NULL;
static sndp_hal_iobox_status_e iobox_status = SNDP_HAL_IOBOX_UNKNOWN;

static struct HAL_GPIO_IRQ_CFG_T iob_gpio_irq_cfg;


osTimerDef(IOBOX_INT_DEBOUNCE_TIMER, sndp_iobox_gpio_int_debounce_timer_handler);
static osTimerId iobox_int_debounce_timer = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/
static sndp_hal_iobox_status_e sndp_iobox_gpio_get_iob_status(void)
{
	uint8_t val;
	
	if(app_iobox_det_pin_cfg.pin == HAL_IOMUX_PIN_NUM) {
		return SNDP_HAL_IOBOX_UNKNOWN;
	}
		
	val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_iobox_det_pin_cfg.pin);
	return (val == IOBOX_INT_IN_BOX_VAL) ? (SNDP_HAL_IOBOX_IN) : (SNDP_HAL_IOBOX_OUT);
}

static void sndp_iobox_gpio_int_debounce_timer_handler(void const *param)
{
	sndp_hal_iobox_status_e iob_status;
	
	iob_status = sndp_iobox_gpio_get_iob_status();
	IOB_GPIO_TRACE(2, "last=%d, curr=%d", iobox_status, iob_status);
	
	if(iob_status != SNDP_HAL_IOBOX_UNKNOWN) {
		iobox_status = iob_status;

		if(iob_gpio_status_changed_cb_ptr) {
			sndp_call_func_in_app_thread((uint32_t)iob_gpio_status_changed_cb_ptr, 
						(uint32_t)iobox_status, 0, 0);
		}
	}	
}

#if defined(__SNDP_COMM_MGR__)
void sndp_comm_set_freq_32k(void)
{
    app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_32K);  
}
#endif

static void sndp_iobox_gpio_int_debounce_start(void)
{
	sndp_hal_iobox_status_e iob_status;
	uint32_t delay_time;
	
	iob_status = sndp_iobox_gpio_get_iob_status();
	if(iob_status == SNDP_HAL_IOBOX_IN)
		delay_time = IOBOX_INT_INBOX_DEBOUNCE_DELAYE_MS;
	else
		delay_time = IOBOX_INT_OUTBOX_DEBOUNCE_DELAYE_MS;
		
	osTimerStop(iobox_int_debounce_timer);
	osTimerStart(iobox_int_debounce_timer, delay_time);
    
#if defined(__SNDP_COMM_MGR__)
    sndp_delay_exec_start(5000, (uint32_t)sndp_comm_set_freq_32k, 0, 0, 0);
#endif
}

static void sndp_iobox_gpio_int_disable(void)
{
	if(app_iobox_det_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
	    iob_gpio_irq_cfg.irq_enable = false;
	    hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_iobox_det_pin_cfg.pin, &iob_gpio_irq_cfg);
	}
}

static void sndp_iobox_gpio_int_enable(void)
{
	if(app_iobox_det_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
		uint8_t val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T) app_iobox_det_pin_cfg.pin);
		
		if(val)
	    	iob_gpio_irq_cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
		else
			iob_gpio_irq_cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;

		iob_gpio_irq_cfg.irq_enable = true;
	    hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_iobox_det_pin_cfg.pin, &iob_gpio_irq_cfg);
	}
}

static void sndp_iobox_gpio_int_irq_handler(enum HAL_GPIO_PIN_T pin)
{
	static uint32_t last_ticks = 0;
	uint32_t curr_ticks;
	uint32_t past_ticks;

	sndp_iobox_gpio_int_disable();
	curr_ticks = hal_sys_timer_get();
	past_ticks = hal_timer_get_passed_ticks(curr_ticks, last_ticks);
	
	IOB_GPIO_TRACE(2, "past_ms=%d, repeat_ms=%d", TICKS_TO_MS(past_ticks), IOBOX_INT_DEBOUNCE_REPEAT_MS);

	if(TICKS_TO_MS(past_ticks) >= IOBOX_INT_DEBOUNCE_REPEAT_MS) {
		last_ticks = curr_ticks;
#if defined(__SNDP_COMM_MGR__)        
		app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_52M);  
#endif
		sndp_call_func_in_dev_thread((uint32_t)sndp_iobox_gpio_int_debounce_start, 0, 0, 0);
	}
	sndp_iobox_gpio_int_enable();
}

static int32_t sndp_iobox_gpio_int_init(void)
{
	if(app_iobox_det_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
	    iob_gpio_irq_cfg.irq_enable = true;
	    iob_gpio_irq_cfg.irq_debounce = true;
		iob_gpio_irq_cfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
	    //iob_gpio_irq_cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
	    iob_gpio_irq_cfg.irq_handler = sndp_iobox_gpio_int_irq_handler;
	    
	    sndp_iobox_gpio_int_enable();

		sndp_iobox_gpio_int_irq_handler((enum HAL_GPIO_PIN_T)app_iobox_det_pin_cfg.pin);
		return 0;
	} 

	return 1;
}

static int32_t sndp_iobox_gpio_init(void)
{
	if(iob_gpio_init) {
		IOB_GPIO_TRACE(0, "already initialized");
		return SNDP_HAL_RET_OK;
	}
	
	if(iobox_int_debounce_timer == NULL) {
        iobox_int_debounce_timer = osTimerCreate (osTimer(IOBOX_INT_DEBOUNCE_TIMER), osTimerOnce, NULL);
        ASSERT(iobox_int_debounce_timer != NULL, "%s, iobox_int_debounce_timer == NULL", __func__);
	}

	if(sndp_iobox_gpio_int_init() == 0) {
		iob_gpio_init = true;
		IOB_GPIO_TRACE(0, "init done.");
		return SNDP_HAL_RET_OK;
	} else {
		IOB_GPIO_TRACE(0, "init fail.");
		return SNDP_HAL_RET_FAIL;
	}
}


static int32_t sndp_iobox_gpio_set_status_changed_callback(sndp_hal_iobox_status_changed_callback callback)
{
	iob_gpio_status_changed_cb_ptr = callback;
	return 0;
}

static int32_t sndp_iobox_gpio_get_curr_status(sndp_hal_iobox_status_e *status)
{
	*status = sndp_iobox_gpio_get_iob_status();
	return SNDP_HAL_RET_OK;
}

static int32_t sndp_iobox_gpio_check_curr_status(void)
{
	sndp_call_func_in_dev_thread((uint32_t)sndp_iobox_gpio_int_debounce_start, 0, 0, 0);
	return SNDP_HAL_RET_OK;
}

const sndp_hal_iobox_s sndp_hal_iobox_gpio_int = {
	.init							= sndp_iobox_gpio_init,
    .set_status_changed_callback	= sndp_iobox_gpio_set_status_changed_callback,
    .get_curr_status				= sndp_iobox_gpio_get_curr_status,
    .check_curr_status              = sndp_iobox_gpio_check_curr_status,
};


#endif	//__SNDP_IOBOX_GPIO_INT__

