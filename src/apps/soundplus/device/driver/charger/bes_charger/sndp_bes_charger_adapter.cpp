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
        // BESCHG_TRACE(0, "aaDONE: v=%dmV, state=%d",
        //     sndp_dev_get_bat_voltage(false),
        //     charger_charge_status_get());

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

    charge_cfg.prechg_volt = CHARGER_CHARGE_PRECHARGE_VOLTAGE_3000MV;
    charge_cfg.prechg_current = CHARGER_CHARGE_PRECHARGE_CURRENT_8MA;
    charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_60MA;
    charge_cfg.stop_current = CHARGER_CHARGE_STOP_CURRENT_6MA;
    charge_cfg.cv_volt = CHARGER_CHARGE_CONSTANT_VOLTAGE_4400MV;
    charge_cfg.rechg_volt = CHARGER_CHARGE_RECHARGE_VOLTAGE_150MV;
    charge_cfg.rechg_en = false;
    charge_cfg.chg_en = true;
    charger_charge_module_cfg_set(&charge_cfg);

// 充电状态机：OFF → IDLE → PRECHARGE → FAST(CC) → CV → DONE
// 充电器在 OFF/DONE 状态下写入新 CV 后不会自动重启充电状态机，
// 需要强制 disable + enable 触发状态机重启，
// 使其以新写入的 4400MV 作为 CV 判定阈值重新进入充电流程

    charger_charge_disable();  
    charger_charge_enable();

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
    struct CHARGER_CHARGE_MODULE_CFG_T charge_cfg;

    BESCHG_TRACE(0, "chg_c=%d", charging_current);

    charger_charge_module_cfg_get(&charge_cfg);

	switch(charging_current){
		case SNDP_HAL_CHARGING_CURRENT_ZERO:
			break;

        case SNDP_HAL_CHARGING_CURRENT_0P2C: //20mA
            charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_20MA;
            break;
        
        case SNDP_HAL_CHARGING_CURRENT_0P5C: //20mA
            charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_20MA;
            break;
         
        case SNDP_HAL_CHARGING_CURRENT_1C: //40mA
            charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_40MA;
            break;

        case SNDP_HAL_CHARGING_CURRENT_2C: //60mA
            charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_60MA;
            break;
            
       case SNDP_HAL_CHARGING_CURRENT_3C: //60mA
            charge_cfg.cc_current = CHARGER_CHARGE_CONSTANT_CURRENT_60MA;
            break;
    
		default:
			break;
	}

    BESCHG_TRACE(0, "set cc_idx=%d (0:20mA, 1:40mA, 2:60mA)",
        charge_cfg.cc_current);

    if(charging_current == SNDP_HAL_CHARGING_CURRENT_ZERO) {
        charger_charge_disable();
    } else {
        charger_charge_module_cfg_set(&charge_cfg);
        charger_charge_disable();
        charger_charge_enable();
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
    // BESCHG_TRACE(0, "check: v=%dmV, state=%d, mode=%d",
    //     sndp_dev_get_bat_voltage(false),
    //     status, mode);

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


