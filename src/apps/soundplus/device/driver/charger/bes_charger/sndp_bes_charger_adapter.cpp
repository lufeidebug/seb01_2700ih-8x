#if defined(__SNDP_CHARGER_BES__)
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
#include "sndp_bes_charger_adapter.h"
#include "charger_best1503.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
typedef enum {
    CHG_IRQ_MOD_CHARGE_INTR     = 0,
    CHG_IRQ_MOD_AC_ON_DET_IN    = 1,
    CHG_IRQ_MOD_AC_ON_DET_OUT   = 2,
    CHG_IRQ_MOD_TRIPRE_TIMEOUT  = 3,
    CHG_IRQ_MOD_FAST_TIMEOUT    = 4,
    CHG_IRQ_MOD_ACIN_OV         = 5,
    CHG_IRQ_MOD_OTP             = 6,
    CHG_IRQ_MOD_CHRG_TRICKLE    = 7,
    CHG_IRQ_MOD_CHRG_PRE        = 8,
    CHG_IRQ_MOD_CHRG_FAST       = 9,
    CHG_IRQ_MOD_CHRG_OFF        = 10,
    CHG_IRQ_MOD_CHRG_DONE       = 11,

    CHG_IRQ_MOD_QTY,
} bes_chg_irq_module_e;



/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_charging_mode_changed_callback charging_mode_changed_cb_ptr;


/**************************************************************************************************
* Function
**************************************************************************************************/

static sndp_hal_charger_mode_e sndp_bes_charger_convert_status(enum CHARGER_CHARGE_STATUE_E status)
{
    sndp_hal_charger_mode_e mode;
    
    switch(status) {
        case CHARGER_CHARGE_STATUS_IDLE:
            mode = SNDP_HAL_CHARGER_MODE_STANDBY;
            break;
        case CHARGER_CHARGE_STATUS_TRICKLE:
            mode = SNDP_HAL_CHARGER_MODE_TRICKLE_CHARGING;
            break;
        case CHARGER_CHARGE_STATUS_FAST:
            mode = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
            break;
        case CHARGER_CHARGE_STATUS_PRE:
            mode = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
            break;
        case CHARGER_CHARGE_STATUS_FAULT:
            mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
            break;
        case CHARGER_CHARGE_STATUS_OFF:
            mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
            break;
        case CHARGER_CHARGE_STATUS_DONE:
            mode = SNDP_HAL_CHARGER_MODE_FULL_CHARGING;
            break;
        default:
            mode = SNDP_HAL_CHARGER_MODE_STANDBY;
            break;
    
    }

    return mode;
}

static void sndp_bes_charger_report_mode(sndp_hal_charger_mode_e mode)
{
    BESCHG_TRACE(0, "%d", mode);

    if(charging_mode_changed_cb_ptr) {
        sndp_call_func_in_dev_thread((uint32_t)charging_mode_changed_cb_ptr, mode, 0, 0);
    }
}

static void sndp_bes_charger_irq_handler(uint32_t status)
{
    sndp_hal_charger_mode_e mode;
    
    BESCHG_TRACE(0, "%d", status);
    
    if(status&CHARGER_IRQ_CAUSE_AC_ON_DET_IN) {
        mode = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_AC_ON_DET_OUT) {
        mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_TRIPRE_TIMEOUT) {
        mode = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_FAST_TIMEOUT) {
        mode = SNDP_HAL_CHARGER_MODE_CV_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_ACIN_OV) {
        mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_OTP) {
        mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_CHARGE_TRICKLE) {
        mode = SNDP_HAL_CHARGER_MODE_TRICKLE_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_CHARGE_PRE) {
        mode = SNDP_HAL_CHARGER_MODE_TRICKLE_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_CHARGE_FAST) {
        mode = SNDP_HAL_CHARGER_MODE_CC_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_CHARGE_OFF) {
        mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
        
    } else if(status&CHARGER_IRQ_CAUSE_CHARGE_DONE) {
        mode = SNDP_HAL_CHARGER_MODE_FULL_CHARGING;
        
    } else {
        mode = SNDP_HAL_CHARGER_MODE_NOT_CHARGING;
        
    }

    
    sndp_bes_charger_report_mode(mode);
}

int32_t sndp_bes_charger_init(void)
{
    struct CHARGER_CHARGE_MODULE_CFG_T charge_cfg;

    charger_charge_irq_handler_set(sndp_bes_charger_irq_handler);
        
    charger_charge_module_cfg_get(&charge_cfg);

    BESCHG_TRACE(0, "dft: %d %d %d %d %d %d %d",
        charge_cfg.prechg_current, 
        charge_cfg.cc_current, 
        charge_cfg.stop_current,
        charge_cfg.cv_volt, 
        charge_cfg.rechg_volt,
        charge_cfg.rechg_en, 
        charge_cfg.chg_en);

    charge_cfg.prechg_volt = CHARGER_CHARGE_PRECHARGE_VOLTAGE_2800MV;
    charge_cfg.prechg_current = CHARGER_CHARGE_PRECHARGE_CURRENT_10MA;
    charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_60MA;
    charge_cfg.stop_current = CHARGER_CHARGE_STOP_CURRENT_4MA;
    charge_cfg.cv_volt = CHARGER_CHARGE_CONSTANT_VOLTAGE_4400MV;
    charge_cfg.rechg_volt = CHARGER_CHARGE_RECHARGE_VOLTAGE_150MV;
    charge_cfg.rechg_en = false;
    charge_cfg.chg_en = true;
    charger_charge_module_cfg_set(&charge_cfg);

    BESCHG_TRACE(8,"set: %d %d %d %d %d %d %d",
        charge_cfg.prechg_current,
        charge_cfg.cc_current,
        charge_cfg.stop_current,
        charge_cfg.cv_volt,
        charge_cfg.rechg_volt,
        charge_cfg.rechg_en,
        charge_cfg.chg_en);

    charger_charge_module_cfg_get(&charge_cfg);

    BESCHG_TRACE(8,"get: %d %d %d %d %d %d %d",
        charge_cfg.prechg_current,
        charge_cfg.cc_current,
        charge_cfg.stop_current,
        charge_cfg.cv_volt,
        charge_cfg.rechg_volt,
        charge_cfg.rechg_en,
        charge_cfg.chg_en);
    

    //charger_charge_enable();
    BESCHG_TRACE(0, "done.");
	return SNDP_HAL_RET_OK;
}


int32_t sndp_bes_charger_set_charging_current(sndp_hal_charging_current_e charging_current)
{
    BESCHG_TRACE(0, "chg_c=%d", charging_current);

	switch(charging_current){
		case SNDP_HAL_CHARGING_CURRENT_ZERO:
            charger_charge_disable();
			break;
    
		default:
            charger_charge_enable();
			break;
	}
    
	return SNDP_HAL_RET_OK;
}

int32_t sndp_bes_charger_set_charging_mode_changed_callback(sndp_hal_charging_mode_changed_callback callback)
{
	charging_mode_changed_cb_ptr = callback;
	return SNDP_HAL_RET_OK;
}

int32_t sndp_bes_charger_check_curr_status(void)
{
    enum CHARGER_CHARGE_STATUE_E status;
    sndp_hal_charger_mode_e mode;
    
    status = charger_charge_status_get();
    mode = sndp_bes_charger_convert_status(status);
    sndp_bes_charger_report_mode(mode);
	return SNDP_HAL_RET_OK;
}

int32_t sndp_bes_charger_get_charging_status(sndp_hal_charger_mode_e *mode)
{
    enum CHARGER_CHARGE_STATUE_E status;
    
    if(mode == NULL) {
        return SNDP_HAL_RET_FAIL;
    }
    
    status = charger_charge_status_get();
    *mode = sndp_bes_charger_convert_status(status);
    return SNDP_HAL_RET_OK;
}

bool sndp_bes_charger_is_charging_enabled(void)
{
	return true;
}

extern "C" const sndp_hal_charger_s sndp_hal_charger_bes = {
	.init									= sndp_bes_charger_init,
	.set_charging_current					= sndp_bes_charger_set_charging_current,
	.set_charging_mode_changed_callback     = sndp_bes_charger_set_charging_mode_changed_callback,
	.check_curr_status                      = sndp_bes_charger_check_curr_status,
	.get_charging_status                    = sndp_bes_charger_get_charging_status,
	.is_charging_enabled					= sndp_bes_charger_is_charging_enabled,
};


#endif //__SNDP_CHARGER_HP4554__


