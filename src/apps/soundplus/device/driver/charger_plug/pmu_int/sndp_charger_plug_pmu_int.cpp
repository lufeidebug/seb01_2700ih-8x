#if defined(__SNDP_CHARGER_PLUG_PMU_INT__)
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
#include "sndp_hal_pmu.h"
#include "sndp_hal_charger_plug.h"
#include "sndp_charger_plug_pmu_int.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_CHARGER_PLUG_PMU_TRACE__
#ifdef __SNDP_CHARGER_PLUG_PMU_TRACE__
#define CHARGER_PLUG_PMU_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[CHG_PLUG_PMU] %s, " str, __func__, ##__VA_ARGS__)
#else
#define CHARGER_PLUG_PMU_TRACE(num, str, ...)
#endif


#define CHARGER_PLUGIN_DELAY_TIME_MS				(100)
#define CHARGER_PLUGOUT_DELAY_TIME_MS				(100)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void charger_plug_delay_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool charger_plug_inited = false;
static sndp_hal_charger_plug_status_changed_callback charger_plug_status_changed_cb_ptr = NULL;
static sndp_hal_charger_plug_status_e charger_plug_status = SNDP_HAL_CHARGER_UNKNOWN;

osTimerDef(CHARGER_PLUG_DELAY_TIMER, charger_plug_delay_handler);
static osTimerId charger_plug_delay_timer = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

static void charger_plug_delay_handler(void const *param)
{
    //CHARGER_PLUG_PMU_TRACE(0, "00");
	if(charger_plug_status_changed_cb_ptr) {
        //CHARGER_PLUG_PMU_TRACE(0, "11");
		sndp_call_func_in_app_thread((uint32_t)charger_plug_status_changed_cb_ptr, (uint32_t)charger_plug_status, 0, 0);
	}
}

static void charger_plug_charger_plug_status_changed(sndp_hal_pmu_charger_plug_status_e pmu_plug_status)
{
	sndp_hal_charger_plug_status_e new_status = SNDP_HAL_CHARGER_UNKNOWN;
    uint32_t delay_ms = 50;
	
	if(SNDP_HAL_PMU_CHARGER_PLUGIN == pmu_plug_status) {
		new_status = SNDP_HAL_CHARGER_PLUGIN;
        delay_ms = CHARGER_PLUGIN_DELAY_TIME_MS;
	} else if(SNDP_HAL_PMU_CHARGER_PLUGOUT == pmu_plug_status) {
		new_status = SNDP_HAL_CHARGER_PLUGOUT;
        delay_ms = CHARGER_PLUGOUT_DELAY_TIME_MS;
	}

	CHARGER_PLUG_PMU_TRACE(2, "hal_sta=%d, chg_sta=%d", pmu_plug_status, new_status);

	if(new_status != SNDP_HAL_CHARGER_UNKNOWN) {
		charger_plug_status = new_status;
		
		//CHARGER_PLUG_PMU_TRACE(2, "plug_status=%d, delay_ms=%d", charger_plug_status, delay_ms);
		
		osTimerStop(charger_plug_delay_timer);
		osTimerStart(charger_plug_delay_timer, delay_ms);
	}
}

static int32_t charger_plug_pmu_init(void)
{
	if(charger_plug_inited) {
		CHARGER_PLUG_PMU_TRACE(0, "inited");
		return SNDP_HAL_RET_OK;
	}
		
	if(charger_plug_delay_timer == NULL) {
        charger_plug_delay_timer = osTimerCreate (osTimer(CHARGER_PLUG_DELAY_TIMER), osTimerOnce, NULL);
        ASSERT(charger_plug_delay_timer != NULL, "%s, %d", __func__, __LINE__);
	}

	sndp_hal_pmu_init();
	sndp_hal_pmu_add_charger_plug_status_changed_callback(charger_plug_charger_plug_status_changed);

	charger_plug_inited = true;
	CHARGER_PLUG_PMU_TRACE(0, "done.");
	return SNDP_HAL_RET_OK;
}


static int32_t charger_plug_pmu_set_status_changed_callback(sndp_hal_charger_plug_status_changed_callback callback)
{
	charger_plug_status_changed_cb_ptr = callback;
	return 0;
}

static int32_t charger_plug_pmu_get_curr_status(sndp_hal_charger_plug_status_e *status)
{
	sndp_hal_pmu_charger_plug_status_e plug_status = sndp_hal_pmu_get_curr_plug_status();
	*status = (plug_status == SNDP_HAL_PMU_CHARGER_PLUGIN) ? (SNDP_HAL_CHARGER_PLUGIN) : (SNDP_HAL_CHARGER_PLUGOUT);
	return SNDP_HAL_RET_OK;
}

static int32_t charger_plug_pmu_check_curr_status(void)
{
    charger_plug_charger_plug_status_changed(sndp_hal_pmu_get_curr_plug_status());
	return SNDP_HAL_RET_OK;
}



const sndp_hal_charger_plug_s sndp_hal_charger_plug_pmu_int = {
	.init							= charger_plug_pmu_init,
    .set_status_changed_callback	= charger_plug_pmu_set_status_changed_callback,
    .get_curr_status				= charger_plug_pmu_get_curr_status,
    .check_curr_status				= charger_plug_pmu_check_curr_status,
};


#endif	//__SNDP_CHARGER_PLUG_PMU_INT__