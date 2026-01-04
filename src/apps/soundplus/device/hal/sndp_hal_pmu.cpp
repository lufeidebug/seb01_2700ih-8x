#if defined(__SNDP_PMU_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "pmu.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_pmu.h"




/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __HAL_PMU_TRACE__
#ifdef __HAL_PMU_TRACE__
#define HAL_PMU_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[SNDP_HAL_PMU] %s, " str, __func__, ##__VA_ARGS__)
#else
#define HAL_PMU_TRACE(num, str, ...)
#endif


#define SNDP_HAL_PMU_CALLBACK_CNT                   (3)
	
#define CHARGER_PLUG_INT_DEBOUNCE_MS                (30)
#define CHARGER_PLUG_TIMER_DEBOUNCE_MS              (50)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_hal_pmu_charger_plug_debounce_timeout_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool pmu_init = false;
static sndp_hal_pmu_charger_plug_status_changed_callback charger_plug_cb[SNDP_HAL_PMU_CALLBACK_CNT];

osTimerDef(CHARGER_PLUG_DEBOUNCE_TIMER, sndp_hal_pmu_charger_plug_debounce_timeout_handler);
static osTimerId charger_plug_debounce_timer = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

static void sndp_hal_pmu_charger_plug_debounce_timeout_handler(void const *param)
{
	enum PMU_CHARGER_STATUS_T charger;
	sndp_hal_pmu_charger_plug_status_e plug_status;

	charger = pmu_charger_get_status();
    plug_status =  (charger == PMU_CHARGER_PLUGIN) ? (SNDP_HAL_PMU_CHARGER_PLUGIN) : (SNDP_HAL_PMU_CHARGER_PLUGOUT);

	HAL_PMU_TRACE(1, "plug_sta=%d", plug_status);
	
	for(uint8_t i = 0; i < SNDP_HAL_PMU_CALLBACK_CNT; i++) {
		if(charger_plug_cb[i] != NULL) {
			sndp_call_func_in_dev_thread((uint32_t)charger_plug_cb[i], (uint32_t)plug_status, 0, 0);
		}
	}
}

static void sndp_hal_pmu_charger_plug_debounce(void)
{	
	osTimerStop(charger_plug_debounce_timer);
	osTimerStart(charger_plug_debounce_timer, CHARGER_PLUG_TIMER_DEBOUNCE_MS);
}


static void sndp_hal_pmu_plug_irq_handler(enum PMU_CHARGER_STATUS_T status)
{
	static uint32_t last_ticks = 0;
	uint32_t curr_ticks = hal_sys_timer_get();
	uint32_t past_ticks = hal_timer_get_passed_ticks(curr_ticks, last_ticks);

	HAL_PMU_TRACE(0, "sta=%d", status);	
	if(past_ticks > MS_TO_TICKS(CHARGER_PLUG_INT_DEBOUNCE_MS)) {
		last_ticks = curr_ticks;
		sndp_call_func_in_dev_thread((uint32_t)sndp_hal_pmu_charger_plug_debounce, 0, 0, 0);
	}
}


uint32_t sndp_hal_pmu_init(void)
{
	if(pmu_init) {
		HAL_PMU_TRACE(0, "inited");
		return SNDP_HAL_RET_OK;
	}
		
	for(uint8_t i = 0; i < SNDP_HAL_PMU_CALLBACK_CNT; i++) {
		charger_plug_cb[i] = NULL;
	}

	if(charger_plug_debounce_timer == NULL) {
        charger_plug_debounce_timer = osTimerCreate (osTimer(CHARGER_PLUG_DEBOUNCE_TIMER), osTimerOnce, NULL);
	}
    
	pmu_charger_init();
	pmu_charger_set_irq_handler(sndp_hal_pmu_plug_irq_handler);

	pmu_init = true;
	HAL_PMU_TRACE(0, "done.");	
	return SNDP_HAL_RET_OK;
}

uint32_t sndp_hal_pmu_add_charger_plug_status_changed_callback(sndp_hal_pmu_charger_plug_status_changed_callback callback)
{
	uint8_t exsit_idx = SNDP_HAL_PMU_CALLBACK_CNT;
	uint8_t space_idx = SNDP_HAL_PMU_CALLBACK_CNT;
	uint32_t ret = SNDP_HAL_RET_FAIL;

	HAL_PMU_TRACE(1, "cb=%8x", (uint32_t)callback);
	for(uint8_t i = 0; i < SNDP_HAL_PMU_CALLBACK_CNT; i++) {
		if(charger_plug_cb[i] == callback) {
			exsit_idx = i;
			break;
		}

		if(space_idx == SNDP_HAL_PMU_CALLBACK_CNT && charger_plug_cb[i] == NULL) {
			space_idx = i;
		}
	}

	if(exsit_idx < SNDP_HAL_PMU_CALLBACK_CNT) {
		charger_plug_cb[exsit_idx] = callback;
		ret = SNDP_HAL_RET_OK;
	} else if(space_idx < SNDP_HAL_PMU_CALLBACK_CNT) {
		charger_plug_cb[space_idx] = callback;
		ret = SNDP_HAL_RET_OK;
	}

	ASSERT(ret != SNDP_HAL_RET_FAIL, "%s, faild", __func__);
	
	return ret;
}

uint32_t sndp_hal_pmu_clear_charger_plug_status_changed_callback(sndp_hal_pmu_charger_plug_status_changed_callback callback)
{
	for(uint8_t i = 0; i < SNDP_HAL_PMU_CALLBACK_CNT; i++) {
		if(charger_plug_cb[i] == callback) {
			charger_plug_cb[i] = NULL;
		}
	}
	
	return SNDP_HAL_RET_OK;
}

sndp_hal_pmu_charger_plug_status_e sndp_hal_pmu_get_curr_plug_status(void)
{
    enum PMU_CHARGER_STATUS_T charger = pmu_charger_get_status();

    if (charger == PMU_CHARGER_PLUGIN)
        return SNDP_HAL_PMU_CHARGER_PLUGIN;
    else
    	return SNDP_HAL_PMU_CHARGER_PLUGOUT;
}

void sndp_hal_pmu_force_check(void)
{
    sndp_hal_pmu_charger_plug_debounce();
}


#endif	/* __SP_MGR_GESTURE__ */

