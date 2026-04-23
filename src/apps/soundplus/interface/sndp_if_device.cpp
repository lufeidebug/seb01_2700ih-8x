#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "pmu.h"
#include "app_thread.h"
#include "tgt_hardware.h"
#include "factory_section.h"
#include "fir_process.h"
#include "iir_process.h"
#include "app_factory_bt.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"
#include "sndp_if_data_access.h"
#include "sndp_hal_common.h"


#if defined(__SNDP_BATTERY_MGR__)
#include "sndp_hal_battery.h"
#endif

#if defined(__SNDP_CHARGER_PLUG_MGR__)   
#include "sndp_hal_charger_plug.h"
#endif 

#if defined(__SNDP_CHARGER_MGR__)
#include "sndp_hal_charger.h"
#endif

#if defined(__SNDP_TEMPERATURE_MGR__)
#include "sndp_hal_temperature.h"
#endif

#if defined(__SNDP_COVER_SWITCH_MGR__)
#include "sndp_hal_cover_switch.h"
#endif

#if defined(__SNDP_IOBOX_MGR__)
#include "sndp_hal_iobox.h"
#endif

#if defined(__SNDP_WEAR_DETECT_MGR__)
#include "sndp_hal_wear_detect.h"
#endif

#if defined(__SNDP_GESTURE_MGR__)
#include "sndp_hal_gesture.h"
#endif

#if defined(__SNDP_COMM_SPP__)
#include "sndp_comm_spp.h"
#endif

#if defined(__SNDP_COMM_MGR__)
#include "sndp_comm_main.h"
#include "sndp_comm_cmd.h"
#endif

#if defined(__SNDP_COMM_POGOPIN__)	
#include "sndp_hal_pogopin_comm.h"
#endif    

#if defined(__SNDP_HRSENSOR_SUPPORT__)	
#include "sndp_hal_hr.h"
#endif    
#if defined(__SNDP_GSENSOR_SUPPORT__)	
#include "sndp_hal_acc.h"
#endif



/**************************************************************************************************
* Constant
**************************************************************************************************/
//#define __SNDP_EARSIDE_BY_BT_ADDR__

#ifndef SW_VERSION1
#define SW_VERSION1                     (0)
#endif

#ifndef SW_VERSION2
#define SW_VERSION2                     (0)
#endif

#ifndef SW_VERSION3
#define SW_VERSION3                     (0)
#endif

#ifndef SW_VERSION4
#define SW_VERSION4                     (0)
#endif

#ifndef HW_VERSION1
#define HW_VERSION1                     (0)
#endif

#ifndef HW_VERSION2
#define HW_VERSION2                     (0)
#endif




/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/



/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_dev_context_s sndp_dev_ctx;
static sndp_dev_bat_pwr_measure_cb sndp_dev_bat_pwr_measure_cb_ptr = NULL;
static sndp_dev_temperature_measure_cb sndp_dev_temperature_measure_cb_ptr = NULL;
static sndp_dev_cover_status_changed_cb sndp_dev_cover_status_changed_cb_ptr = NULL;
static sndp_dev_iobox_status_changed_cb sndp_dev_iobox_status_changed_cb_ptr = NULL;
static sndp_dev_wear_status_changed_cb sndp_dev_wear_status_changed_cb_prt = NULL;
static sndp_dev_gesture_event_cb sndp_dev_gesture_event_cb_ptr = NULL;
static sndp_dev_charger_plug_cb sndp_dev_charger_plug_cb_ptr = NULL;

static const char *sndp_dev_dev_model_name = "EAGLEPLUS\0";    //SNDP_BT_NAME;
#if defined(__SNDP_SLEEP_APP__)
 static sndp_da_field_sleep_app_data_s sleep_app_data_global;
 static sndp_sleep_app_flag sleep_flag_run;
 static sndp_sleep_app_flag sleep_flag_flash;
#endif
/**************************************************************************************************
* Function
**************************************************************************************************/


/************************************************** Wear Info Start **************************************************/

bool sndp_dev_wear_is_worn(bool peer)
{
	sndp_dev_wear_status_e wear_status;

	wear_status = (peer) ? (sndp_dev_ctx.peer.wear_status) : (sndp_dev_ctx.local.wear_status);
	return (wear_status == SNDP_DEV_WEAR_ON) ? (true) : (false);
}

sndp_dev_wear_status_e sndp_dev_wear_get_status(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.wear_status) : (sndp_dev_ctx.local.wear_status);
}

void sndp_dev_wear_set_status(bool peer, sndp_dev_wear_status_e wear_status)
{
	SNDP_IF_TRACE(2, "peer=%d, wear_status=%d", peer, wear_status);
	if(peer) 
		sndp_dev_ctx.peer.wear_status = wear_status;
	else
		sndp_dev_ctx.local.wear_status = wear_status;
}


void sndp_dev_wear_check_curr_status(void)
{
	SNDP_IF_TRACE_ENTER();

#if 0   //for test
    sndp_dev_set_wear_status(false, SNDP_DEV_WEAR_ON);
    return;
#endif
    
#if defined(__SNDP_WEAR_DETECT_MGR__)
	sndp_hal_wear_detection_check_curr_status();
#endif

}

void sndp_dev_wear_enable_detection(void)
{
	SNDP_IF_TRACE(0, "enter");
	
#if defined(__SNDP_WEAR_DETECT_MGR__)	
	sndp_hal_wear_detection_enter_detection_mode();
#endif

}

void sndp_dev_wear_disable_detection(void)
{
	SNDP_IF_TRACE(0, "enter");
	
#if defined(__SNDP_WEAR_DETECT_MGR__)		
	sndp_hal_wear_detection_enter_standby_mode();
#endif

}

void sndp_dev_wear_status_changed_handler(sndp_dev_wear_status_e status)
{
	sndp_dev_wear_status_e curr_status;

	curr_status = sndp_dev_wear_get_status(false);
	SNDP_IF_TRACE(1, "curr_status=%d, new_status=%d", curr_status, status);

	if(status != SNDP_DEV_WEAR_UNKNOWN) {
		sndp_dev_wear_set_status(false, status);

 #if defined(__SNDP_COMM_MGR__)        
        sndp_comm_cmd_send_lr_sync_wear_status(status);
#endif   

		if(sndp_dev_wear_status_changed_cb_prt) {
            sndp_call_func_in_app_thread((uint32_t)sndp_dev_wear_status_changed_cb_prt, status, 0, 0);
		}
	}
}

#if defined(__SNDP_WEAR_DETECT_MGR__)
static void sndp_dev_wear_status_changed(sndp_hal_wear_status_e status)
{
	sndp_dev_wear_status_e wear_status = SNDP_DEV_WEAR_UNKNOWN;

	if(SNDP_HAL_WEAR_ON == status)
		wear_status = SNDP_DEV_WEAR_ON;
	else if(SNDP_HAL_WEAR_OFF == status)
		wear_status = SNDP_DEV_WEAR_OFF;

	SNDP_IF_TRACE(1, "status=%d, wear_status=%d", status, wear_status);
	
#if 0   //for test
    return;
#endif

	if(wear_status != SNDP_DEV_WEAR_UNKNOWN) {
#if defined(__SNDP_TOUCH_CALI__)  
#if defined(__SNDP_COMM_MGR__)
        sndp_comm_cmd_send_pt_report_wear_status(wear_status);
#endif
#endif        
		sndp_dev_wear_status_changed_handler(wear_status);

	}
}
#endif

void sndp_dev_wear_set_status_changed_callback(sndp_dev_wear_status_changed_cb callback)
{
    sndp_dev_wear_status_changed_cb_prt = callback;	
}

void sndp_dev_wear_init(void)
{
	SNDP_IF_TRACE_ENTER();

	sndp_dev_wear_set_status(false, SNDP_DEV_WEAR_UNKNOWN);
#if defined(__SNDP_WEAR_DETECT_MGR__)
	sndp_hal_wear_detection_init();
	sndp_hal_wear_detection_set_wear_status_changed_callback(sndp_dev_wear_status_changed);
#if 0    
	if(sndp_dev_iobox_is_in_box(false)) {
		sndp_dev_wear_disable_detection();
	} else {
		sndp_dev_wear_enable_detection();
	}
#endif    
#endif
#if defined(__SNDP_PWRON_ENTER_TOUCH_CALI__)
	sndp_dev_enable_touch_calibration(true);
#endif

}

/************************************************** Wear Info End **************************************************/


/************************************************** Gesture Info Start **************************************************/

#if defined(__SNDP_GESTURE_MGR__)
void sndp_dev_gesture_event_callback(sndp_hal_gesture_event_e event)
{
	sndp_dev_gesture_event_e gesture = SNDP_DEV_GESTURE_EVENT_NONE;

	switch(event) {
		case SNDP_HAL_GESTURE_EVENT_PRESS_DOWN:
			gesture = SNDP_DEV_GESTURE_EVENT_PRESS_DOWN;
			break;
		case SNDP_HAL_GESTURE_EVENT_PRESS_UP:
			gesture = SNDP_DEV_GESTURE_EVENT_PRESS_UP;
			break;
		case SNDP_HAL_GESTURE_EVENT_1_CLICK:
			gesture = SNDP_DEV_GESTURE_EVENT_1_CLICK;
			break;
		case SNDP_HAL_GESTURE_EVENT_2_CLICK:
			gesture = SNDP_DEV_GESTURE_EVENT_2_CLICK;
			break;
		case SNDP_HAL_GESTURE_EVENT_3_CLICK:
			gesture = SNDP_DEV_GESTURE_EVENT_3_CLICK;
			break;
		case SNDP_HAL_GESTURE_EVENT_4_CLICK:
			gesture = SNDP_DEV_GESTURE_EVENT_4_CLICK;
			break;
		case SNDP_HAL_GESTURE_EVENT_5_CLICK:
			gesture = SNDP_DEV_GESTURE_EVENT_5_CLICK;
			break;
		case SNDP_HAL_GESTURE_EVENT_LONG_PRESS:
			gesture = SNDP_DEV_GESTURE_EVENT_LONG_PRESS;
			break;
		case SNDP_HAL_GESTURE_EVENT_LLONG_PRESS:
			gesture = SNDP_DEV_GESTURE_EVENT_LLONG_PRESS;
			break;
		case SNDP_HAL_GESTURE_EVENT_REPEAT:
			gesture = SNDP_DEV_GESTURE_EVENT_REPEAT;
			break;
		case SNDP_HAL_GESTURE_EVENT_SLIDE_UP:
			gesture = SNDP_DEV_GESTURE_EVENT_SLIDE_UP;
			break;
		case SNDP_HAL_GESTURE_EVENT_SLIDE_DOWN:
			gesture = SNDP_DEV_GESTURE_EVENT_SLIDE_DOWN;
			break;
		default:
			break;
	}

	if(gesture != SNDP_DEV_GESTURE_EVENT_NONE && sndp_dev_gesture_event_cb_ptr != NULL) {
        sndp_call_func_in_app_thread((uint32_t)sndp_dev_gesture_event_cb_ptr, gesture, 0, 0);
	}
}
#endif


void sndp_dev_gesture_set_event_callback(sndp_dev_gesture_event_cb callback)
{
    sndp_dev_gesture_event_cb_ptr = callback;
}

void sndp_dev_gesture_init(void)
{
	SNDP_IF_TRACE_ENTER();

#if defined(__SNDP_GESTURE_MGR__)
	sndp_hal_gesture_init();
	sndp_hal_gesture_set_event_callback(sndp_dev_gesture_event_callback);
#endif
}

#if defined(__SNDP_SLEEP_APP__)
#if defined(__SNDP_GESTURE_MAP__)
function_callback_t sndp_dev_gesture_func_table[SNDP_FUNC_MAX] = {0};

void sndp_dev_gesture_mapper_set_default(sndp_dev_gesture_mapper_t* mapper) 
{
    if (!mapper) return;
  
    sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[SNDP_DEV_GESTURE_CLICK] = sndp_dev_gesture_func_table[SNDP_FUNC_A];
    sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[SNDP_DEV_GESTURE_DOUBLE_CLICK] = sndp_dev_gesture_func_table[SNDP_FUNC_E];
    sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[SNDP_DEV_GESTURE_TRIPLE_CLICK] = sndp_dev_gesture_func_table[SNDP_FUNC_C];
    sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[SNDP_DEV_GESTURE_LONG_PRESS] = sndp_dev_gesture_func_table[SNDP_FUNC_D];
}

void sndp_dev_gesture_mapper_handle_gesture(sndp_dev_gesture_type_t gesture) 
{
    if (gesture >= SNDP_DEV_GESTURE_MAX) {
        return;
    }
    
    function_callback_t func = NULL;
    
    func = sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[gesture];
    
    if (func) {
        func();  // 执行对应的功能函数
    }
}

bool sndp_dev_gesture_mapper_update_mapping(bool peer, sndp_dev_gesture_type_t gesture, sndp_dev_function_type_t func_type) 
{
    if (gesture >= SNDP_DEV_GESTURE_MAX || func_type >= SNDP_FUNC_MAX) {
        return false;
    }
    
    function_callback_t new_func = NULL;
    
    // 根据功能类型选择对应的函数
    switch (func_type) {
        case SNDP_FUNC_A: new_func = sndp_dev_gesture_func_table[SNDP_FUNC_A]; break;
        case SNDP_FUNC_B: new_func = sndp_dev_gesture_func_table[SNDP_FUNC_B]; break;
        case SNDP_FUNC_C: new_func = sndp_dev_gesture_func_table[SNDP_FUNC_C]; break;
        case SNDP_FUNC_D: new_func = sndp_dev_gesture_func_table[SNDP_FUNC_D]; break;
        case SNDP_FUNC_E: new_func = sndp_dev_gesture_func_table[SNDP_FUNC_E]; break;
        default: return false;
    }
    
	if(peer) {
			sndp_dev_ctx.peer.gesture_mapper.ear_mapping_table.func_table[gesture] = new_func;
	} else {
			sndp_dev_ctx.local.gesture_mapper.ear_mapping_table.func_table[gesture] = new_func;
	}
    
    return true;
}

void sndp_dev_gesture_mapper_init(void) 
{
    memset(&sndp_dev_ctx.local.gesture_mapper, 0, sizeof(sndp_dev_gesture_mapper_t));
    sndp_dev_gesture_mapper_set_default(&sndp_dev_ctx.local.gesture_mapper);
    sndp_dev_ctx.local.gesture_mapper.initialized = true;
}

void sndp_dev_register_gesture_funcs(function_callback_t *funcs)
{
    if(funcs) {
        for(int i = 0; i < SNDP_FUNC_MAX; i++) {
            sndp_dev_gesture_func_table[i] = funcs[i];
        }
    }
}
#endif
#endif
/************************************************** Gesture Info End **************************************************/


/************************************************** InOut Box Info Start **************************************************/
bool sndp_dev_iobox_is_in_box(bool peer)
{
	sndp_dev_iobox_status_e inout_status;
	
	inout_status =  (peer) ? (sndp_dev_ctx.peer.inout_status) : (sndp_dev_ctx.local.inout_status);
	return (inout_status == SNDP_DEV_IOBOX_IN) ? (true) : (false);
}

bool sndp_dev_iobox_is_out_box(bool peer)
{
	sndp_dev_iobox_status_e inout_status;
	
	inout_status =  (peer) ? (sndp_dev_ctx.peer.inout_status) : (sndp_dev_ctx.local.inout_status);
	return (inout_status == SNDP_DEV_IOBOX_OUT) ? (true) : (false);
}


sndp_dev_iobox_status_e sndp_dev_iobox_get_status(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.inout_status) : (sndp_dev_ctx.local.inout_status);
}

void sndp_dev_iobox_set_status(bool peer, sndp_dev_iobox_status_e inout_status)
{
	//SNDP_IF_TRACE(2, "peer=%d, inout_status=%d", peer, inout_status);
	
	if(peer) 
		sndp_dev_ctx.peer.inout_status = inout_status;
	else
		sndp_dev_ctx.local.inout_status = inout_status;
}

void sndp_dev_iobox_check_curr_status(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_IOBOX_MGR__)
	sndp_hal_iobox_check_curr_status();
#endif

}

void sndp_dev_iobox_status_changed_handler(sndp_dev_iobox_status_e status)
{
	SNDP_IF_TRACE(0, "status=%d", status);

	if(status != SNDP_DEV_IOBOX_UNKNOWN) {
		sndp_dev_iobox_set_status(false, status);
        
#if defined(__SNDP_COMM_MGR__)    
        sndp_comm_cmd_send_lr_sync_iobox_status(status);
#endif

#if 0//defined(__SNDP_COMM_POGOPIN__)			
        if(SNDP_DEV_IOBOX_OUT == status) {
    		sndp_hal_pogopin_comm_set_mode(SNDP_HAL_POGOPIN_MODE_CHARGING);
    	} else {
    		sndp_hal_pogopin_comm_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    	}
#endif
		
		if(sndp_dev_iobox_status_changed_cb_ptr) {
            sndp_call_func_in_app_thread((uint32_t)sndp_dev_iobox_status_changed_cb_ptr, status, 0, 0);
		}
	}
}

#if defined(__SNDP_IOBOX_MGR__)
static void sndp_dev_iobox_status_changed(sndp_hal_iobox_status_e status)
{
	sndp_dev_iobox_status_e iobox_status;

	if(SNDP_HAL_IOBOX_IN == status)
		iobox_status = SNDP_DEV_IOBOX_IN;
	else
		iobox_status = SNDP_DEV_IOBOX_OUT;
	
	//SNDP_IF_TRACE(2, "status=%d, iobox_status=%d", status, iobox_status);
	sndp_dev_iobox_status_changed_handler(iobox_status);
}
#endif

void sndp_dev_iobox_set_status_changed_callback(sndp_dev_iobox_status_changed_cb callback)
{
    sndp_dev_iobox_status_changed_cb_ptr = callback;
}

void sndp_dev_iobox_init(void)
{
	SNDP_IF_TRACE_ENTER();
    
	sndp_dev_iobox_set_status(false, SNDP_DEV_IOBOX_OUT);

#if defined(__SNDP_IOBOX_MGR__)
	sndp_hal_iobox_init();
	sndp_hal_iobox_set_status_changed_callback(sndp_dev_iobox_status_changed);
#endif
}


/************************************************** InOut Box Info End **************************************************/


/************************************************** Cover Switch Info Start **************************************************/
void sndp_dev_io_pmu_check_cover(void)
{
	sndp_dev_cover_status_e cover_status = SNDP_DEV_COVER_UNKNOWN;
	
	SNDP_IF_TRACE(0, "io: %d CG_plugin:%d", sndp_dev_iobox_get_status(false), sndp_dev_charger_is_plugin(false));	
	
	if(sndp_dev_iobox_is_in_box(false) && sndp_dev_charger_is_plugin(false)){

        cover_status = SNDP_DEV_COVER_COLSED;

	} else if(sndp_dev_iobox_is_in_box(false)) {

        cover_status = SNDP_DEV_COVER_OPENED;
		
	} else if(sndp_dev_iobox_is_out_box(false) && !sndp_dev_charger_is_plugin(false))  {
        cover_status = SNDP_DEV_COVER_OPENED;
	}
    
	sndp_dev_cover_status_changed_handler(cover_status);
}

bool sndp_dev_cover_is_opened(bool peer)
{
	sndp_dev_cover_status_e cover_status;

	cover_status =  (peer) ? (sndp_dev_ctx.peer.cover_status) : (sndp_dev_ctx.local.cover_status);
	return (cover_status == SNDP_DEV_COVER_OPENED) ? (true) : (false);
}

bool sndp_dev_cover_is_closed(bool peer)
{
	sndp_dev_cover_status_e cover_status;

	cover_status =  (peer) ? (sndp_dev_ctx.peer.cover_status) : (sndp_dev_ctx.local.cover_status);
	return (cover_status == SNDP_DEV_COVER_COLSED) ? (true) : (false);
}

sndp_dev_cover_status_e sndp_dev_cover_get_status(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.cover_status) : (sndp_dev_ctx.local.cover_status);
}

void sndp_dev_cover_set_status(bool peer, sndp_dev_cover_status_e cover_status)
{
	//SNDP_IF_TRACE(2, "peer=%d, cover_status=%d", peer, cover_status);
	if(peer) 
		sndp_dev_ctx.peer.cover_status = cover_status;
	else
		sndp_dev_ctx.local.cover_status = cover_status;
}

void sndp_dev_cover_check_curr_status(void)
{
	SNDP_IF_TRACE_ENTER();
 #if defined(__SNDP_COVER_SWITCH_MGR__)   
	sndp_hal_cover_switch_check_curr_status();
 #endif
}

void sndp_dev_cover_status_changed_handler(sndp_dev_cover_status_e status)
{
	sndp_dev_cover_status_e curr_status;

	curr_status = sndp_dev_cover_get_status(false);
	
	SNDP_IF_TRACE(1, "curr=%d, new=%d", curr_status, status);

    
	if(status != SNDP_DEV_COVER_UNKNOWN) {
		sndp_dev_cover_set_status(false, status);

#if defined(__SNDP_COMM_MGR__)    
        sndp_comm_cmd_send_lr_sync_wear_status(status);
#endif
		if(sndp_dev_cover_status_changed_cb_ptr) {
			sndp_call_func_in_app_thread((uint32_t)sndp_dev_cover_status_changed_cb_ptr, status, 0, 0);
		}
	}
}

#if defined(__SNDP_COVER_SWITCH_MGR__)
static void sndp_dev_cover_status_changed(sndp_hal_cover_status_e status)
{
	sndp_dev_cover_status_e cover_status;
	
	if(SNDP_HAL_COVER_COLSED == status)
		cover_status = SNDP_DEV_COVER_COLSED;
	else
		cover_status = SNDP_DEV_COVER_OPENED;
	
	//SNDP_IF_TRACE(1, "status=%d, cover_status=%d", status, cover_status);
	sndp_dev_cover_status_changed_handler(cover_status);
}
#endif

void sndp_dev_cover_set_status_changed_callback(sndp_dev_cover_status_changed_cb callback)
{
    sndp_dev_cover_status_changed_cb_ptr = callback;
}

void sndp_dev_cover_init(void)
{
	SNDP_IF_TRACE_ENTER();	

    sndp_dev_cover_set_status(false, SNDP_DEV_COVER_OPENED);
    
#if defined(__SNDP_COVER_SWITCH_MGR__)
	sndp_hal_cover_switch_init();
	sndp_hal_cover_switch_set_status_changed_callback(sndp_dev_cover_status_changed);
#endif
}

/************************************************** Cover Switch Info End **************************************************/


/************************************************** Temperature Info Start **************************************************/

int16_t sndp_dev_temperature_get_value(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.temperature) : (sndp_dev_ctx.local.temperature);
}

void sndp_dev_temperature_set_value(bool peer, int16_t temperature)
{
	if(peer) 
		sndp_dev_ctx.peer.temperature = temperature;
	else
		sndp_dev_ctx.local.temperature = temperature;
}

void sndp_dev_temperature_measure(void)
{
	SNDP_IF_TRACE_ENTER();
#if defined(__SNDP_TEMPERATURE_MGR__)	
	sndp_hal_temperature_measure();
#endif
}

#if defined(__SNDP_TEMPERATURE_MGR__)
static void sndp_dev_temperature_measure_callback(int16_t temperature)
{
	SNDP_IF_TRACE(1, "temperature=%d", temperature);

	sndp_dev_temperature_set_value(false, temperature);

	if(sndp_dev_temperature_measure_cb_ptr) {
        sndp_call_func_in_app_thread((uint32_t)sndp_dev_temperature_measure_cb_ptr, temperature, 0, 0);
	}
}
#endif

void sndp_dev_temperature_set_measure_callback(sndp_dev_temperature_measure_cb callback)
{
    sndp_dev_temperature_measure_cb_ptr = callback;
}

void sndp_dev_temperature_init(void)
{
	SNDP_IF_TRACE_ENTER();

	sndp_dev_temperature_set_value(false, 25);
#if defined(__SNDP_TEMPERATURE_MGR__)
	sndp_hal_temperature_init();
	sndp_hal_temperature_set_measure_callback(sndp_dev_temperature_measure_callback);
#endif
}

/************************************************** Temperature Info End **************************************************/


/************************************************** Charger Info Start **************************************************/
bool sndp_dev_charger_is_charging(bool peer)
{
	sndp_dev_charging_status_e charging_status;

	charging_status = (peer) ? (sndp_dev_ctx.peer.charging_status) : (sndp_dev_ctx.local.charging_status);
	return (charging_status == SNDP_DEV_CHARGER_CHARGING) ? (true) : (false);
}

bool sndp_dev_charger_is_charging_full(bool peer)
{
	sndp_dev_charging_status_e charging_status;

	charging_status = (peer) ? (sndp_dev_ctx.peer.charging_status) : (sndp_dev_ctx.local.charging_status);
	if(SNDP_DEV_CHARGER_CHARGING_FULL == charging_status)
		return true;

#if 0	
	uint8_t bat_per = (peer) ? (sndp_dev_ctx.peer.bat_info.bat_per) : (sndp_dev_ctx.local.bat_info.bat_per);
	if(bat_per == 100)
		return true;
#endif

    return false;
}

void sndp_dev_charger_set_charging_status(bool peer, sndp_dev_charging_status_e charging_status)
{
	if(peer) 
		sndp_dev_ctx.peer.charging_status = charging_status;
	else
		sndp_dev_ctx.local.charging_status = charging_status;
}

sndp_dev_charging_status_e sndp_dev_charger_get_charging_status(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.charging_status) : (sndp_dev_ctx.local.charging_status);
}

void sndp_dev_charger_set_charging_current(void)
{
#if defined(__SNDP_CHARGER_MGR__)	
	static sndp_hal_charging_current_e curr_current = SNDP_HAL_CHARGING_CURRENT_ZERO;
	sndp_hal_charging_current_e set_current = SNDP_HAL_CHARGING_CURRENT_ZERO;
	
	if(sndp_dev_charger_is_plugin(false)) {
		uint8_t temp = sndp_dev_temperature_get_value(false);
		if(temp < 0)
			set_current = SNDP_HAL_CHARGING_CURRENT_ZERO;
		else if(temp < 15)
			set_current = SNDP_HAL_CHARGING_CURRENT_HALF;
		else if(temp <= 45)
			set_current = SNDP_HAL_CHARGING_CURRENT_1C;
		else
			set_current = SNDP_HAL_CHARGING_CURRENT_ZERO;
	} else {
		set_current = SNDP_HAL_CHARGING_CURRENT_ZERO;
	}

	/* Avoid dupicate settings */
	if(set_current != curr_current) {
		curr_current = set_current;
		sndp_hal_charger_set_charging_current(set_current);
	}
#endif	
}

void sndp_dev_charger_check_curr_status(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_CHARGER_MGR__)   
    sndp_hal_charger_check_curr_status();
#endif 

}

bool sndp_dev_charger_is_charging_enabled(void) 
{  
#if defined(__SNDP_CHARGER_MGR__)	
	return sndp_hal_charger_is_charging_enabled();
#endif

    return false;
}

#if defined(__SNDP_CHARGER_MGR__)
static void sndp_dev_charger_charging_mode_changed(sndp_hal_charger_mode_e mode)
{	
    SNDP_IF_TRACE(0, "mode=%d", mode);
    
	switch(mode) {
		case SNDP_HAL_CHARGER_MODE_STANDBY:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_NOT_CHARGING);
			break;
		case SNDP_HAL_CHARGER_MODE_SHIPMODE:
			break;
		case SNDP_HAL_CHARGER_MODE_COMMUNICATION:
			break;
		case SNDP_HAL_CHARGER_MODE_NOT_CHARGING:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_NOT_CHARGING);        
			break;
		case SNDP_HAL_CHARGER_MODE_TRICKLE_CHARGING:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_CHARGING);
			break;
		case SNDP_HAL_CHARGER_MODE_CC_CHARGING:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_CHARGING);
			break;
		case SNDP_HAL_CHARGER_MODE_CV_CHARGING:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_CHARGING);
			break;
		case SNDP_HAL_CHARGER_MODE_FULL_CHARGING:
			sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_CHARGING_FULL);
#if 0
            sndp_dev_bat_pwr_measure();
#else
            sndp_app_shutdown(SNDP_SHUTDOWN_REASON_CHARGING_FULL);
#endif
			break;
	}
}
#endif

void sndp_dev_charger_init(void)
{
	SNDP_IF_TRACE_ENTER();
    sndp_dev_charger_set_charging_status(false, SNDP_DEV_CHARGER_NOT_CHARGING);
#if defined(__SNDP_CHARGER_MGR__)	
	sndp_hal_charger_init();
	sndp_hal_charger_set_charging_mode_changed_callback(sndp_dev_charger_charging_mode_changed);
    sndp_hal_charger_check_curr_status();
#endif	
}

/************************************************** Charger Info End **************************************************/


/************************************************** Charger Plug Info Start **************************************************/
bool sndp_dev_charger_is_plugin(bool peer)
{
    if(peer) {
	    return (sndp_dev_ctx.peer.charger_status == SNDP_DEV_CHARGER_PLUG_IN) ? true : false;
    } else {
        return (sndp_dev_ctx.local.charger_status == SNDP_DEV_CHARGER_PLUG_IN) ? true : false;
    }
}

void sndp_dev_charger_plug_set_status(bool peer, sndp_dev_charger_plug_e state)
{
	if(peer) 
		sndp_dev_ctx.peer.charger_status = state;
	else
		sndp_dev_ctx.local.charger_status = state;
}

void sndp_dev_charger_plug_check_curr_status(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_CHARGER_PLUG_MGR__)   
    sndp_hal_charger_plug_check_curr_status();
#endif 

}

#if defined(__SNDP_CHARGER_PLUG_MGR__)   
void sndp_dev_charger_plug_status_changed(sndp_hal_charger_plug_status_e status)
{
    sndp_dev_charger_plug_e charger_plug = SNDP_DEV_CHARGER_PLUG_UNKNOWN;
       
    SNDP_IF_TRACE(1, "status=%d", status);

    if(status == SNDP_HAL_CHARGER_PLUGIN) {
        charger_plug = SNDP_DEV_CHARGER_PLUG_IN;
    } else if(status == SNDP_HAL_CHARGER_PLUGOUT) {
        charger_plug = SNDP_DEV_CHARGER_PLUG_OUT;
    }
    
    if(charger_plug != SNDP_DEV_CHARGER_PLUG_UNKNOWN) {
        sndp_dev_charger_plug_set_status(false, charger_plug);
        if(sndp_dev_charger_plug_cb_ptr) {
    		sndp_dev_charger_plug_cb_ptr(charger_plug);
        }
#if defined(__SNDP_CHARGER_MGR__)
        sndp_hal_charger_check_curr_status();
#endif

        if(charger_plug == SNDP_DEV_CHARGER_PLUG_IN) {
    		if(bt_is_in_dut_test_mode()){
                sndp_pmu_reboot(0);
    		}
    	}


#if defined(__SNDP_COMM_POGOPIN__)			
        if(charger_plug == SNDP_DEV_CHARGER_PLUG_IN) {
    		sndp_hal_pogopin_comm_set_mode(SNDP_HAL_POGOPIN_MODE_CHARGING);
    	} else {
    		sndp_hal_pogopin_comm_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    	}
#endif


#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)
        if(charger_plug == SNDP_DEV_CHARGER_PLUG_IN) {
            sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_COLSED);
        } else if(charger_plug == SNDP_DEV_CHARGER_PLUG_OUT) {
        #if 0
        
            if(sndp_dev_iobox_is_in_box(false)) {
                sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_OPENED);
            } else {
                sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_COLSED);
            }
        #else
            sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_OPENED);dddd
        #endif
        }
#endif        
    }
}
#endif 


void sndp_dev_charger_plug_set_status_changed_callback(sndp_dev_charger_plug_cb callback)
{
    sndp_dev_charger_plug_cb_ptr = callback;
}

void sndp_dev_charger_plug_init(void)
{
	SNDP_IF_TRACE_ENTER();
    sndp_dev_charger_plug_set_status(false, SNDP_DEV_CHARGER_PLUG_UNKNOWN);
    sndp_dev_charger_plug_set_status(true, SNDP_DEV_CHARGER_PLUG_UNKNOWN);
#if defined(__SNDP_CHARGER_PLUG_MGR__)   
    sndp_hal_charger_plug_init();
	sndp_hal_charger_plug_set_status_changed_callback(sndp_dev_charger_plug_status_changed);
#endif    
}

/************************************************** Charger Plug Info End **************************************************/



/************************************************** Battery Info Start **************************************************/
uint8_t sndp_dev_get_bat_report_level(void) 
{
    sndp_dev_bat_info_s local;
    sndp_dev_bat_info_s peer;
    uint8_t report_level = 9;
    uint8_t reprot_earside = 0xFF;

    sndp_dev_get_bat_info(false, &local);
    sndp_dev_get_bat_info(true, &peer);

    if(sndp_is_tws_link_connected()) {
        if(local.valid && peer.valid) {
            if(local.bat_level <= peer.bat_level) {
    			report_level = local.bat_level;
                reprot_earside = sndp_dev_get_earside(false);
    		} else {
    			report_level = peer.bat_level;
                reprot_earside = sndp_dev_get_earside(true);
    		}
        } else if(local.valid && !peer.valid) {
            report_level = local.bat_level;
            reprot_earside = sndp_dev_get_earside(false);
        } else if(!local.valid && peer.valid) {
            report_level = peer.bat_level;
            reprot_earside = sndp_dev_get_earside(true);
        } else {
            report_level = 9;
        }
    } else {
        if(local.valid) {
            report_level = local.bat_level;
            reprot_earside = sndp_dev_get_earside(false);
        } else {
            report_level = 9;
        }
    }

    SNDP_IF_TRACE(3, "earside=%d, level=%d", reprot_earside, report_level);
    return report_level;
}

uint8_t sndp_dev_get_bat_percentage(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.bat_info.bat_per) : (sndp_dev_ctx.local.bat_info.bat_per);
}

uint16_t sndp_dev_get_bat_voltage(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.bat_info.bat_volt) : (sndp_dev_ctx.local.bat_info.bat_volt);
}

uint8_t sndp_dev_get_bat_level(bool peer)
{
	return (peer) ? (sndp_dev_ctx.peer.bat_info.bat_level) : (sndp_dev_ctx.local.bat_info.bat_level);
}

bool sndp_dev_get_bat_info(bool peer, sndp_dev_bat_info_s * bat_info)
{
	if(bat_info == NULL)
		return false;
	
	if(peer)
		memcpy((void *)bat_info, (void *)&sndp_dev_ctx.peer.bat_info, sizeof(sndp_dev_bat_info_s));
	else
		memcpy((void *)bat_info, (void *)&sndp_dev_ctx.local.bat_info, sizeof(sndp_dev_bat_info_s));

	return true;
}

void sndp_dev_set_bat_info(bool peer, sndp_dev_bat_info_s bat_info)
{
	if(peer)
		memcpy((void *)&sndp_dev_ctx.peer.bat_info, (void *)&bat_info, sizeof(sndp_dev_bat_info_s));
	else
		memcpy((void *)&sndp_dev_ctx.local.bat_info, (void *)&bat_info, sizeof(sndp_dev_bat_info_s));
}

bool sndp_dev_get_box_bat_info(sndp_dev_bat_info_s *bat_info)
{
	if(bat_info == NULL)
		return false;
	
	memcpy((void *)bat_info, (void *)&sndp_dev_ctx.box.bat_info, sizeof(sndp_dev_bat_info_s));
	return true;
}

bool sndp_dev_set_box_bat_info(sndp_dev_bat_info_s bat_info)
{
	memcpy((void *)&sndp_dev_ctx.box.bat_info, (void *)&bat_info, sizeof(sndp_dev_bat_info_s));
	return true;
}

sndp_dev_charging_status_e sndp_dev_get_box_charging_sta(void)
{
	return sndp_dev_ctx.box.charging_status;
}

bool sndp_dev_set_box_charging_sta(sndp_dev_charging_status_e sta)
{
	sndp_dev_ctx.box.charging_status = sta;
	return true;
}


#if defined(__SNDP_BATTERY_MGR__)	
static void sndp_dev_bat_pwr_measure_callback(sndp_hal_bat_info_s bat_info)
{
	sndp_dev_bat_info_s old_bat_info;
	sndp_dev_bat_info_s new_bat_info;

	SNDP_IF_TRACE(3, "bat_volt=%d, bat_per=%d, bat_level=%d", bat_info.bat_volt, bat_info.bat_per, bat_info.bat_level);
	
	sndp_dev_get_bat_info(false, &old_bat_info);

    new_bat_info.valid = true;
	new_bat_info.bat_per = bat_info.bat_per;
	new_bat_info.bat_volt = bat_info.bat_volt;
	new_bat_info.bat_level = bat_info.bat_level;
	sndp_dev_set_bat_info(false, new_bat_info);

	if(sndp_dev_bat_pwr_measure_cb_ptr) {
		sndp_dev_bat_pwr_measure_cb_ptr(old_bat_info, new_bat_info);
	}
}
#endif

void sndp_dev_bat_pwr_measure(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_BATTERY_MGR__)	
	sndp_hal_battery_measure((sndp_hal_bat_charging_status_e)sndp_dev_charger_get_charging_status(false));
#endif
}

void sndp_dev_bat_pwr_set_measure_callback(sndp_dev_bat_pwr_measure_cb callback)
{
    sndp_dev_bat_pwr_measure_cb_ptr = callback;
}

void sndp_dev_bat_pwr_init(void)
{
	SNDP_IF_TRACE_ENTER();

#if defined(__SNDP_BATTERY_MGR__)	
	sndp_hal_battery_init((sndp_hal_bat_charging_status_e)sndp_dev_charger_is_charging(false), sndp_dev_get_bat_percentage(false));
	sndp_hal_battery_set_measure_callback(sndp_dev_bat_pwr_measure_callback);
#endif	
}

/************************************************** Battery Info End **************************************************/


/************************************************** EarSide Info Start **************************************************/
sndp_dev_earside_e sndp_dev_get_local_earside(void)
{ 
	static sndp_dev_earside_e earside = SNDP_DEV_EARSIDE_UNKNOWN;
	uint8_t val;
	
	if(earside == SNDP_DEV_EARSIDE_UNKNOWN) {

#if defined(__SNDP_DEV_EARSIDE_BY_BT_ADDR__)
		uint8_t mac_addr[6] = {0};
		factory_section_original_btaddr_get(mac_addr);
		SNDP_IF_TRACE(1, "mac_addr:");
		DUMP8("%02x ", mac_addr, 6);

		if(mac_addr[0] % 2 == 0) 
			val = 1;
		else
			val = 0;

#else
        
        if (app_ear_side_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
            hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&app_ear_side_pin_cfg, 1);
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_ear_side_pin_cfg.pin, HAL_GPIO_DIR_IN, 1);

            val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_ear_side_pin_cfg.pin);

            // 原理图配置：右耳悬空，左耳拉低。读取电平后配置为拉低可以省电。
            struct HAL_IOMUX_PIN_FUNCTION_MAP earside_pin_cfg;
            memcpy(&earside_pin_cfg, &app_ear_side_pin_cfg, sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP));
            earside_pin_cfg.pull_sel = HAL_IOMUX_PIN_PULLDOWN_ENABLE;
            hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&earside_pin_cfg, 1);
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)earside_pin_cfg.pin, HAL_GPIO_DIR_IN, 1);
            
        } else {
            val = 0;
        }
#endif

		if(val) {
			earside = SNDP_DEV_EARSIDE_RIGHT;
		} else {
			earside = SNDP_DEV_EARSIDE_LEFT;
		}
	}
	
	return earside;
}

sndp_dev_earside_e sndp_dev_get_earside(bool peer)
{
	static bool inited = false;
	
	if(inited == false) {
		if(sndp_dev_get_local_earside() == SNDP_DEV_EARSIDE_RIGHT) {
			sndp_dev_ctx.local.ear_side = SNDP_DEV_EARSIDE_RIGHT;
			sndp_dev_ctx.peer.ear_side = SNDP_DEV_EARSIDE_LEFT;
		} else {
			sndp_dev_ctx.local.ear_side = SNDP_DEV_EARSIDE_LEFT;
			sndp_dev_ctx.peer.ear_side = SNDP_DEV_EARSIDE_RIGHT;
		}
		
		inited = true;
	}
	
	return (peer) ? (sndp_dev_ctx.peer.ear_side) : (sndp_dev_ctx.local.ear_side);
}

bool sndp_dev_is_right_earphone(void)
{
	return (SNDP_DEV_EARSIDE_RIGHT == sndp_dev_get_local_earside());
}

bool sndp_dev_is_left_earphone(void)
{
	return (SNDP_DEV_EARSIDE_LEFT == sndp_dev_get_local_earside());
}


/************************************************** EarSide Info End **************************************************/


/************************************************** Device Info Start **************************************************/

uint8_t *sndp_dev_get_fw_ver(bool peer) 
{
	if(peer)
		return &sndp_dev_ctx.peer.fw_ver[0];
	else
		return &sndp_dev_ctx.local.fw_ver[0];
}

char *sndp_dev_get_fw_ver_str(bool peer)
{
    static char temp[20] = {0};
    uint8_t *fw_ver;
    
    if(peer)
		fw_ver = &sndp_dev_ctx.peer.fw_ver[0];
	else
		fw_ver = &sndp_dev_ctx.local.fw_ver[0];
    
    sprintf(temp, "%d.%d.%d.%d", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
    return temp;
}

void sndp_dev_set_fw_ver(bool peer, uint8_t *fw_ver) 
{
	if(fw_ver == NULL)
		return;
		
	if(peer)
		memcpy(&sndp_dev_ctx.peer.fw_ver[0], fw_ver, 4);
	else
		memcpy(&sndp_dev_ctx.local.fw_ver[0], fw_ver, 4);
}

uint8_t *sndp_dev_get_hw_ver(bool peer) 
{
	if(peer)
		return &sndp_dev_ctx.peer.hw_ver[0];
	else
		return &sndp_dev_ctx.local.hw_ver[0];
}

char *sndp_dev_get_hw_ver_str(bool peer)
{
    static char temp[10] = {0};
    uint8_t *hw_ver;
    
    if(peer)
        hw_ver = &sndp_dev_ctx.peer.hw_ver[0];
    else
        hw_ver = &sndp_dev_ctx.local.hw_ver[0];
    
    sprintf(temp, "%d.%d", hw_ver[0], hw_ver[1]);
    return temp;
}


bool sndp_dev_set_hw_ver(bool peer, uint8_t *hw_ver) 
{
	if(hw_ver == NULL)
		return false;
	
	if(peer)
		memcpy(&sndp_dev_ctx.peer.hw_ver[0], hw_ver, 2);
	else
		memcpy(&sndp_dev_ctx.local.hw_ver[0], hw_ver, 2);
	
	return true;
}

uint8_t *sndp_dev_get_box_fw_ver(void) 
{
	return &sndp_dev_ctx.box.fw_ver[0];
}

bool sndp_dev_set_box_fw_ver(uint8_t *fw_ver) 
{
	if(fw_ver == NULL)
		return false;
	
	memcpy(&sndp_dev_ctx.box.fw_ver[0], fw_ver, 4);
	return true;
}

uint8_t *sndp_dev_get_bt_addr(bool peer) 
{
	if(peer)
		return &sndp_dev_ctx.peer.bt_addr[0];
	else
		return &sndp_dev_ctx.local.bt_addr[0];
	
}

bool sndp_dev_set_bt_addr(bool peer, uint8_t *bt_addr) 
{
	if(bt_addr == NULL)
		return false;
	
	if(peer)
		memcpy(&sndp_dev_ctx.peer.bt_addr[0], bt_addr, 6);
	else
		memcpy(&sndp_dev_ctx.local.bt_addr[0], bt_addr, 6);
	
	return true;
}

uint8_t *sndp_dev_get_ble_addr(bool peer) 
{
	if(peer)
		return &sndp_dev_ctx.peer.ble_addr[0];
	else
		return &sndp_dev_ctx.local.ble_addr[0];
}

bool sndp_dev_set_ble_addr(bool peer, uint8_t *ble_addr) 
{
	if(ble_addr == NULL)
		return false;
	
	if(peer)
		memcpy(&sndp_dev_ctx.peer.ble_addr[0], ble_addr, 6);
	else
		memcpy(&sndp_dev_ctx.local.ble_addr[0], ble_addr, 6);
	
	return true;
}

#if defined(__SNDP_APP_MODIFY_BT_NAME__)
static sndp_da_field_bt_name_s field_bt_name;

#if 0   //for test
unsigned char test_bt_name[100] = {
	0xE4, 0xBB, 0x96, 0xE7, 0x9A, 0x84, 0xE8, 0x93, 0x9D, 0xE7, 0x89, 0x99, 0xE8, 0x80, 0xB3, 0xE6, 
	0x9C, 0xBA, 0xE4, 0xB8, 0x80, 0xE4, 0xBA, 0x8C, 0xE4, 0xB8, 0x89, 0xE5, 0x9B, 0x9B, 0xE4, 0xBA, 
	0x94, 0xE5, 0x85, 0xAD, 0xE4, 0xB8, 0x83, 0xE5, 0x85, 0xAB, 0xE4, 0xB9, 0x9D, 0xE5, 0x8D, 0x81, 
	0xE4, 0xB8, 0x80, 0xE4, 0xBA, 0x8C, 0xE4, 0xB8, 0x89, 0xE5, 0x9B, 0x9B, 0x00
};
#endif

char *sndp_dev_get_bt_name(void)
{
    bool name_type = 0;
    char *bt_name;
    uint8_t len;

    memset(&field_bt_name, 0, sizeof(sndp_da_field_bt_name_s));
    if(sndp_da_read_field_data_from_running_param(SNDP_DA_FIELD_BT_NAME, &field_bt_name, sizeof(sndp_da_field_bt_name_s)) == 0) {
        if(field_bt_name.key == SNDP_DA_PARAM_FIELD_VALID) {
            if(field_bt_name.len > SNDP_DA_BT_NAME_LEN)
                len = SNDP_DA_BT_NAME_LEN;
            else
                len = field_bt_name.len;
            
            field_bt_name.name[len] = '\0';
            bt_name = (char *)field_bt_name.name;
            name_type = 1;
        }
    }
    
    if(name_type == 0) {
       bt_name = (char *)factory_section_get_bt_name();
    }

#if 0   //for test
    bt_name = (char *)test_bt_name;
#endif


    SNDP_IF_TRACE(2, "name_type=%d, name=%s", name_type, bt_name);
    return bt_name;
}


bool sndp_dev_modify_bt_name(uint8_t *name , uint16_t len)
{
    SNDP_IF_TRACE(1, "len=%d, name=%s", len, name);

    if(len > SNDP_DA_BT_NAME_LEN) {
        len = SNDP_DA_BT_NAME_LEN;
    }

    memset(&field_bt_name, 0, sizeof(sndp_da_field_bt_name_s));
    field_bt_name.len = len;
    memcpy(field_bt_name.name, name, len);
    if(sndp_da_write_field_data_to_running_param(SNDP_DA_FIELD_BT_NAME, &field_bt_name, sizeof(sndp_da_field_bt_name_s), true) == 0) {
        memset(&field_bt_name, 0, sizeof(sndp_da_field_bt_name_s));
        if(sndp_da_read_field_data_from_backup_param(SNDP_DA_FIELD_BT_NAME, &field_bt_name, sizeof(sndp_da_field_bt_name_s), true) == 0) {
            if((len == field_bt_name.len) && (memcmp(name, field_bt_name.name, len) == 0)) {
                SNDP_IF_TRACE(0, "bt name saved successfully.");
                return true;
            }
        }
    }

    return false;
}
#else
char *sndp_dev_get_bt_name(void)
{
		char *bt_name;

		bt_name = (char *)factory_section_get_bt_name();

		SNDP_IF_TRACE(1, "bt_name=%s", bt_name);
		return bt_name;
}
#endif

static uint8_t sndp_dev_dev_sn[SNDP_DEV_DEV_SN_LEN + 1];
#define SNDP_SN_VALID_FLAG          (0xA1B2C3D4)

void sndp_dev_get_default_sn(uint8_t *buf, uint16_t buf_size)
{
    uint8_t local_bt_addr[6] = {0};

    if(buf_size < SNDP_DEV_DEV_SN_LEN) {
        return;
    }
    
    memset(buf, 0, buf_size);  
	factory_section_original_btaddr_get(local_bt_addr);
    sprintf((char *)buf, "SEB01%02X%02X%02X%02X%02X%02X", 
        local_bt_addr[5],
        local_bt_addr[4],
        local_bt_addr[3],
        local_bt_addr[2],
        local_bt_addr[1],
        local_bt_addr[0]);
   
}

uint8_t *sndp_dev_get_dev_sn(void) 
{
    sndp_da_field_sn_s field_sn;

    memset(sndp_dev_dev_sn, 0, sizeof(sndp_dev_dev_sn));    
    if(sndp_da_read_field(SNDP_DA_FIELD_SN, &field_sn, sizeof(sndp_da_field_sn_s), false) == 0) {
        if(field_sn.valid == 0xA1B2C3D4 ) {
            memcpy(sndp_dev_dev_sn, field_sn.sn, SNDP_DEV_DEV_SN_LEN);
        } else {
            sndp_dev_get_default_sn(sndp_dev_dev_sn, sizeof(sndp_dev_dev_sn));
        }
    }
    return sndp_dev_dev_sn;
}

bool sndp_dev_save_dev_sn(uint8_t *sn, uint16_t sn_len) 
{
    sndp_da_field_sn_s field_sn;
    
	if(sn == NULL)
		return false;
    if(sn_len > SNDP_DEV_DEV_SN_LEN)
        return false;

    SNDP_IF_TRACE(2, "sn_len=%d, sn:%s", sn_len, sn);

    memset(&field_sn.sn, 0, SNDP_DEV_DEV_SN_LEN);
    field_sn.valid = SNDP_SN_VALID_FLAG;
    strncpy((char *)field_sn.sn, (char *)sn, sn_len);
    if(sndp_da_write_field(SNDP_DA_FIELD_SN, &field_sn, sizeof(sndp_da_field_sn_s), true) == 0) {
        memset(&field_sn, 0, sizeof(sndp_da_field_sn_s));
        if(sndp_da_read_field(SNDP_DA_FIELD_SN, &field_sn, sizeof(sndp_da_field_sn_s), true) == 0) {
            if(memcmp(sn, field_sn.sn, SNDP_DEV_DEV_SN_LEN) == 0) {
                SNDP_IF_TRACE(0, "sn saved successfully.");
                return true;
            }
        }
    }
    
	return false;
}


#ifdef FIRMWARE_REV
extern "C" void system_get_info(uint8_t *fw_rev_0, uint8_t *fw_rev_1,
    uint8_t *fw_rev_2, uint8_t *fw_rev_3);

#endif


void sndp_dev_init_device_info(void)
{
	uint8_t fw_ver[4] = {0};
	uint8_t hw_ver[2] = {0};
	uint8_t local_bt_addr[6] = {0};
	uint8_t local_ble_addr[6] = {0};
    uint8_t box_ver[4] = {0};
    sndp_dev_bat_info_s bat_info = {false, 4200, 100, 9};

    sndp_dev_set_box_fw_ver(box_ver);
    
    sndp_dev_set_fw_ver(true, fw_ver);
    sndp_dev_set_hw_ver(true, hw_ver);
    sndp_dev_set_bt_addr(true, local_bt_addr);
    sndp_dev_set_ble_addr(true, local_ble_addr);
    sndp_dev_set_bat_info(true, bat_info);


	fw_ver[0] = SW_VERSION1;
	fw_ver[1] = SW_VERSION2;
	fw_ver[2] = SW_VERSION3;
	fw_ver[3] = SW_VERSION4;

	hw_ver[0] = HW_VERSION1;
	hw_ver[1] = HW_VERSION2;

	factory_section_original_btaddr_get(local_bt_addr);
	factory_section_original_bleaddr_get(local_ble_addr);

    sndp_dev_set_fw_ver(false, fw_ver);
	sndp_dev_set_hw_ver(false, hw_ver);
	sndp_dev_set_bt_addr(false, local_bt_addr);
	sndp_dev_set_ble_addr(false, local_ble_addr);	
	sndp_dev_set_bat_info(false, bat_info);
    

	SNDP_IF_TRACE(0, "fw_ver=%d.%d.%d.%d", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
	SNDP_IF_TRACE(0, "hw_ver=%d.%d", hw_ver[0], hw_ver[1]);
	SNDP_IF_TRACE(0, "bt_addr=%02x,%02x,%02x,%02x,%02x,%02x",
			local_bt_addr[0], local_bt_addr[1], local_bt_addr[2],
			local_bt_addr[3], local_bt_addr[4], local_bt_addr[5]);
	SNDP_IF_TRACE(0, "ble_addr=%02x,%02x,%02x,%02x,%02x,%02x", 
			local_ble_addr[0], local_ble_addr[1], local_ble_addr[2],
			local_ble_addr[3], local_ble_addr[4], local_ble_addr[5]);

    SNDP_IF_TRACE(0, "bat_info, %d, %d, %d", 
			bat_info.bat_volt, bat_info.bat_per, bat_info.bat_level);
}


void sndp_dev_clear_device_info(bool peer)
{
	sndp_dev_earbuds_param_s *param;

	if(peer)
		param = &sndp_dev_ctx.peer;
	else
		param = &sndp_dev_ctx.local;

	memset(&param->bat_info, 0, sizeof(sndp_dev_bat_info_s));
    param->bat_info.valid = false;
	param->temperature = 0;
	param->charging_status = SNDP_DEV_CHARGER_NOT_CHARGING;
	param->cover_status = SNDP_DEV_COVER_UNKNOWN;
	param->wear_status = SNDP_DEV_WEAR_UNKNOWN;
	param->inout_status = SNDP_DEV_IOBOX_UNKNOWN;
	
}

char *sndp_dev_get_dev_model_name(void)
{
    return (char *)sndp_dev_dev_model_name;
}


/************************************************** Device Info End **************************************************/

/************************************************** Working Mode Start **************************************************/

void sndp_dev_set_working_mode(sndp_dev_working_mode_e mode)
{
    sndp_dev_ctx.working_mode = mode;
    SNDP_IF_TRACE(0, "mode=%d, working_mode=%d", mode, sndp_dev_ctx.working_mode);
}

uint32_t sndp_dev_get_working_mode(void)
{
    return sndp_dev_ctx.working_mode;
}

bool sndp_dev_is_working_mode(sndp_dev_working_mode_e mode)
{
    return (sndp_dev_ctx.working_mode == mode);
}
/**************************************************  Working Mode End **************************************************/


/************************************************** hr Start **************************************************/
void sndp_dev_hr_enter_standby_mode(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_HEART_RATE_MGR__)	
	sndp_hal_hr_enter_standby_mode();
#endif
}

void sndp_dev_hr_enter_detection_mode(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_HEART_RATE_MGR__)	
	sndp_hal_hr_enter_detection_mode();
#endif
}

void sndp_dev_hr_read_proximity_value(unsigned short *proximity_value)
{
	SNDP_IF_TRACE_ENTER();
#if defined(__SNDP_HEART_RATE_MGR__)	
	sndp_hal_hr_read_proximity_value(proximity_value);
#endif
}

int32_t sndp_dev_hr_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
	SNDP_IF_TRACE_ENTER();
#if defined(__SNDP_HEART_RATE_MGR__)	
	return sndp_hal_hr_write_reg(reg_addr, reg_val);
#else
		return -1;
#endif
}

int32_t sndp_dev_hr_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len)
{
	SNDP_IF_TRACE_ENTER();	
#if defined(__SNDP_HEART_RATE_MGR__)	
	return sndp_hal_hr_read_reg(reg_addr, read_buf, read_len);
#else
		return -1;
#endif
}

void sndp_dev_hr_init(void)
{
	SNDP_IF_TRACE_ENTER();

#if defined(__SNDP_HRSENSOR_SUPPORT__)	
	sndp_hal_hr_init();
#endif
}

/**************************************************  hr End **************************************************/


/************************************************** acc Start **************************************************/
void sndp_dev_acc_enter_standby_mode(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_GSENSOR_SUPPORT__)	
	sndp_hal_acc_enter_standby_mode();
#endif
}

void sndp_dev_acc_enter_detection_mode(void)
{
	SNDP_IF_TRACE_ENTER();
	
#if defined(__SNDP_GSENSOR_SUPPORT__)	
	sndp_hal_acc_enter_detection_mode();
#endif
}

int32_t sndp_dev_acc_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
	SNDP_IF_TRACE_ENTER();
#if defined(__SNDP_GSENSOR_SUPPORT__)	
	return sndp_hal_acc_write_reg(reg_addr, reg_val);
#else
		return -1;	
#endif
}

int32_t sndp_dev_acc_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len)
{
	SNDP_IF_TRACE_ENTER();	
#if defined(__SNDP_GSENSOR_SUPPORT__)	
	return sndp_hal_acc_read_reg(reg_addr, read_buf, read_len);
#else
		return -1;	
#endif
}

void sndp_dev_acc_init(void)
{
	SNDP_IF_TRACE_ENTER();
    
#if defined(__SNDP_GSENSOR_SUPPORT__)	
	sndp_hal_acc_init();
#endif	
}

/**************************************************  acc End **************************************************/
#if defined(__SNDP_SLEEP_APP__)
/************************************************** prompt start **************************************************/
void sndp_dev_sleep_app_set_prompt_onoff(bool peer, uint8_t onoff, bool sava_data)
{
	if(peer)
	{
		sndp_dev_ctx.peer.sleep_app_flag.sleep_prompt_onoff = onoff;
	}
	else
	{
		sndp_dev_ctx.local.sleep_app_flag.sleep_prompt_onoff = onoff;
	}	
	

		sleep_flag_run.sleep_prompt_onoff = onoff;
		if(sava_data)
		{
			sleep_flag_flash.sleep_prompt_onoff = onoff;
			sndp_save_app_flag_to_flash();
		}
		SNDP_IF_TRACE(0, "local=%d peer=%d", sndp_dev_ctx.local.sleep_app_flag.sleep_prompt_onoff, sndp_dev_ctx.peer.sleep_app_flag.sleep_prompt_onoff);
}

bool sndp_dev_sleep_app_get_prompt_onoff(bool peer)
{
	if(peer)
		return sndp_dev_ctx.peer.sleep_app_flag.sleep_prompt_onoff;
	else
		return sndp_dev_ctx.local.sleep_app_flag.sleep_prompt_onoff;
	
		SNDP_IF_TRACE(0, "local=%d peer=%d", sndp_dev_ctx.local.sleep_app_flag.sleep_prompt_onoff, sndp_dev_ctx.peer.sleep_app_flag.sleep_prompt_onoff);
}

/************************************************** prompt end **************************************************/
/**************************************************set eq mode **************************************************/
void sndp_dev_sleep_app_set_eq_index(bool peer, uint8_t index, bool save_data)
{
	SNDP_IF_TRACE(1, "index=%d", index);
	if(index > SNDP_EQ_MODE_RELAXED && index != SNDP_EQ_MODE_CUSTOM_MODE) 
	{
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return;
	}
	
	if(peer){
		sndp_dev_ctx.peer.sleep_app_flag.sleep_eq_index = index;
	}
	else{
		sndp_dev_ctx.local.sleep_app_flag.sleep_eq_index = index;
	}

	sleep_flag_run.sleep_eq_index = index;
	if(save_data)
	{
		sleep_flag_flash.sleep_eq_index = index;
		sndp_save_app_flag_to_flash();
	}

}

uint8_t sndp_dev_sleep_app_get_eq_index(bool peer)
{
	uint8_t select_eq_num = 0;
	SNDP_IF_TRACE(1, "index=%d", sndp_dev_ctx.peer.sleep_app_flag.sleep_eq_index);
	
	if(sndp_dev_ctx.peer.sleep_app_flag.sleep_eq_index > SNDP_EQ_MODE_RELAXED && sndp_dev_ctx.peer.sleep_app_flag.sleep_eq_index != SNDP_EQ_MODE_CUSTOM_MODE) 
	{
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return 0;
	}

	if(peer)
	{
			select_eq_num = sndp_dev_ctx.peer.sleep_app_flag.sleep_eq_index;
	}
	else
	{
			select_eq_num = sndp_dev_ctx.local.sleep_app_flag.sleep_eq_index;	
	}
	return select_eq_num;
}

void sndp_dev_sleep_app_anc_mode_set(bool peer,uint8_t anc_mode,bool sava_data)
{
	if(peer)
		sndp_dev_ctx.peer.sleep_app_flag.sleep_anc_mode = anc_mode;
	else
		sndp_dev_ctx.local.sleep_app_flag.sleep_anc_mode = anc_mode;

	sleep_flag_run.sleep_anc_mode = anc_mode;
	if(sava_data)
	{
		sleep_flag_flash.sleep_anc_mode = anc_mode;
		sndp_save_app_flag_to_flash();
	}
}

uint8_t sndp_dev_sleep_app_anc_mode_get(bool peer)
{
	if(peer)
		return sndp_dev_ctx.peer.sleep_app_flag.sleep_anc_mode;
	else
		return sndp_dev_ctx.local.sleep_app_flag.sleep_anc_mode;
}

/**************************************************set eq mode end************************************************/
void sndp_dev_sleep_app_set_gesture_onoff(bool peer, uint8_t onoff, bool sava)
{
	SNDP_IF_TRACE(0, "enter");
	if(peer) {
		sndp_dev_ctx.peer.sleep_app_flag.sleep_gesture_onoff = onoff;
	} else {
		sndp_dev_ctx.local.sleep_app_flag.sleep_gesture_onoff = onoff;
	}

	sleep_flag_run.sleep_gesture_onoff = onoff;
	if(sava)
	{
		sleep_flag_flash.sleep_gesture_onoff = onoff;
		sndp_save_app_flag_to_flash();		
	}
}

bool sndp_dev_sleep_app_get_gesture_onoff(bool peer)
{
	if(peer) {
		return sndp_dev_ctx.peer.sleep_app_flag.sleep_gesture_onoff;
	} else {
		return sndp_dev_ctx.local.sleep_app_flag.sleep_gesture_onoff;
	}
}

void sndp_dev_sleep_app_set_splaypause_onoff(bool peer, uint8_t onoff, bool sava)
{
	SNDP_IF_TRACE(0, "onoff:%d,save:%d",onoff,sava);
	if(peer) {
		sndp_dev_ctx.peer.sleep_app_flag.sleep_splaypause_onoff = onoff;
	} else {
		sndp_dev_ctx.local.sleep_app_flag.sleep_splaypause_onoff = onoff;
	}

	sleep_flag_run.sleep_splaypause_onoff = onoff;
	if(sava)
	{
		sleep_flag_flash.sleep_splaypause_onoff = onoff;
		sndp_save_app_flag_to_flash();		
	}
}

uint8_t sndp_dev_sleep_app_get_splaypause_onoff(bool peer)
{
	if(peer) {
		return sndp_dev_ctx.peer.sleep_app_flag.sleep_splaypause_onoff;
	} else {
		return sndp_dev_ctx.local.sleep_app_flag.sleep_splaypause_onoff;
	}
}

uint8_t sndp_dev_sleep_app_set_proximity_onoff(bool peer, uint8_t onoff)
{
	SNDP_IF_TRACE(0, "enter");
	if(peer) {
		sndp_dev_ctx.peer.sleep_proximity_onoff = onoff;
	} else {
		sndp_dev_ctx.local.sleep_proximity_onoff = onoff;
	}

	return 0;
}

uint8_t sndp_dev_sleep_app_get_proximity_onoff(bool peer)
{
	if(peer) {
		return sndp_dev_ctx.peer.sleep_proximity_onoff;
	} else {
		return sndp_dev_ctx.local.sleep_proximity_onoff;
	}
}

unsigned short sndp_dev_sleep_app_get_proximity_data(bool peer)
{
	if(peer) {
		return sndp_dev_ctx.peer.sleep_proximity_data;
	} else {
		return sndp_dev_ctx.local.sleep_proximity_data;
	}
}

uint8_t sndp_dev_sleep_app_set_proximity_data(bool peer, unsigned short data)
{
	if(peer) {
		sndp_dev_ctx.peer.sleep_proximity_data = data;
	} else {
		sndp_dev_ctx.local.sleep_proximity_data = data;
	}

	return 0;
}

void sndp_dev_sleep_app_wear_cnt(uint8_t ear_side, uint8_t wear_status)
{
	if(ear_side == SNDP_DEV_EARSIDE_LEFT)
	{
		if(wear_status == SNDP_DEV_WEAR_ON)
		{
			sndp_dev_ctx.peer.sleep_wear_cnt[SNDP_DEV_LEFT_WEAR_CNT]++;
		}
		else if(wear_status == SNDP_DEV_WEAR_OFF)
		{
			sndp_dev_ctx.peer.sleep_wear_cnt[SNDP_DEV_LEFT_UNWEAR_CNT]++;
		}
		else
		{
			//SNDP_DEV_WEAR_UNKNOWN
		}
	}
	else
	{
		if(wear_status == SNDP_DEV_WEAR_ON)
		{
			sndp_dev_ctx.peer.sleep_wear_cnt[SNDP_DEV_RIGHT_WEAR_CNT]++;
		}
		else if(wear_status == SNDP_DEV_WEAR_OFF)
		{
			sndp_dev_ctx.peer.sleep_wear_cnt[SNDP_DEV_RIGHT_UNWEAR_CNT]++;
		}
		else
		{
			//SNDP_DEV_WEAR_UNKNOWN
		}
	}
}

uint16_t *sndp_dev_sleep_app_get_wear_cnt(void)
{
		return &sndp_dev_ctx.peer.sleep_wear_cnt[0];
}

void sndp_dev_sleep_app_clean_wear_cnt(void)
{
	memset(&sndp_dev_ctx.peer.sleep_wear_cnt[0], 0, sizeof(uint16_t)*SNDP_DEV_WEAR_CNT_MAX);
}

void sndp_save_app_flag_to_flash(void)
{
	// Save the custom EQ parameters to flash, so that it can be loaded and used after power on.
	sndp_da_field_sleep_app_data_s *sleep_flag_ptr = &sleep_app_data_global;
	// sndp_sleep_app_flag *test_flag = (sndp_sleep_app_flag *)sleep_flag_ptr->data;
	sndp_da_read_field(SNDP_DA_FIELD_APP_DATA, (uint8_t *)sleep_flag_ptr, sizeof(sndp_da_field_sleep_app_data_s),true);
	if(memcmp(sleep_flag_ptr->data, &sleep_flag_flash, sizeof(sndp_sleep_app_flag)) == 0)
	{
		SNDP_IF_TRACE(0, "app flag not changed, no need to write to flash");
	}
	else
	{
		memcpy(sleep_flag_ptr->data, &sleep_flag_flash, sizeof(sndp_sleep_app_flag));
		sndp_set_crc(&sleep_flag_ptr->data_crc, sleep_flag_ptr->data, sizeof(sndp_sleep_app_flag));
		SNDP_IF_TRACE(0, "app flag changed, write to flash %d",sleep_flag_ptr->data_crc);
		// SNDP_IF_TRACE(0, "eq:%d anc:%d",test_flag->sleep_eq_index, test_flag->sleep_anc_mode);
		// SNDP_IF_TRACE(0, "gesture:%d prompt:%d",test_flag->sleep_gesture_onoff, test_flag->sleep_prompt_onoff);
		// SNDP_IF_TRACE(0, "splaypause:%d",test_flag->sleep_splaypause_onoff);
		sndp_da_write_field(SNDP_DA_FIELD_APP_DATA, (uint8_t *)sleep_flag_ptr, sizeof(sndp_da_field_sleep_app_data_s),true);
	}
	  // sndp_da_read_field(SNDP_DA_FIELD_APP_DATA, sleep_flag_ptr,sizeof(sndp_da_field_sleep_app_data_s),true);
		// SNDP_IF_TRACE(0, "eq:%d anc:%d",test_flag->sleep_eq_index, test_flag->sleep_anc_mode);
		// SNDP_IF_TRACE(0, "gesture:%d prompt:%d",test_flag->sleep_gesture_onoff, test_flag->sleep_prompt_onoff);
		// SNDP_IF_TRACE(0, "splaypause:%d",test_flag->sleep_splaypause_onoff);
}

void sndp_set_default_flag(void)
{
	// Set default EQ parameters to the running param, so that the UI can read and display them.
	sndp_da_field_sleep_app_data_s *sleep_flag_ptr = &sleep_app_data_global;
	memset(&sleep_flag_flash, 0, sizeof(sndp_sleep_app_flag));
	memset(&sleep_flag_run, 0, sizeof(sndp_sleep_app_flag));

	/*****************from flash back flag*****************************/
	sleep_flag_flash.sleep_eq_index = 0;
	sleep_flag_flash.sleep_anc_mode = 0;
	sleep_flag_flash.sleep_gesture_onoff = 1;
	sleep_flag_flash.sleep_prompt_onoff = 1;
	sleep_flag_flash.sleep_splaypause_onoff = 1;

	/*****************from flash running flag*****************************/
	sleep_flag_run.sleep_eq_index = 0;
	sleep_flag_run.sleep_anc_mode = 0;
	sleep_flag_run.sleep_gesture_onoff = 1;
	sleep_flag_run.sleep_prompt_onoff = 1;	
	sleep_flag_run.sleep_splaypause_onoff = 1;
	sleep_flag_ptr->key = SNDP_DA_PARAM_FIELD_VALID;
	memcpy(sleep_flag_ptr->data, &sleep_flag_flash, sizeof(sndp_sleep_app_flag));
	sndp_da_write_field(SNDP_DA_FIELD_APP_DATA, (uint8_t *)sleep_flag_ptr, sizeof(sndp_da_field_sleep_app_data_s),true);
}

void sndp_load_sleep_app_flag(void)
{
	sndp_da_field_sleep_app_data_s* sleep_flag_ptr = &sleep_app_data_global;
	sndp_sleep_app_flag* user_flag_ptr = (sndp_sleep_app_flag*)sleep_flag_ptr->data;
	sndp_da_read_field(SNDP_DA_FIELD_APP_DATA, sleep_flag_ptr,sizeof(sndp_da_field_sleep_app_data_s),true);
	if(sleep_flag_ptr->key == SNDP_DA_PARAM_FIELD_VALID)
	{
		if(sndp_check_crc(sleep_flag_ptr->data, sleep_flag_ptr->data_crc, sizeof(sndp_sleep_app_flag)))
		{
			SNDP_IF_TRACE(0, "Load flag param from flash");
		}
		else
		{
			SNDP_IF_TRACE(0, "Load flag param from crc fail, use default param");
			sndp_set_default_flag();
		}
	}
	else
	{
			SNDP_IF_TRACE(0, "Load flag param from key fail, use default param");
			sndp_set_default_flag();
	}
	memcpy(&sleep_flag_flash, user_flag_ptr, sizeof(sndp_sleep_app_flag));
	memcpy(&sleep_flag_run, user_flag_ptr, sizeof(sndp_sleep_app_flag));
}

void sndp_load_sleep_app_param(void)
{
	sndp_load_sleep_app_flag();
#if defined(__SNDP_EQ_PARAM_SETTING__)
	sndp_load_eq_param();
#endif
	// SNDP_IF_TRACE(0, "sleep_prompt_onoff=%d, sleep_eq_index=%d", sleep_flag_run.sleep_prompt_onoff, sleep_flag_run.sleep_eq_index);
	// SNDP_IF_TRACE(0, "sleep_anc_mode=%d, sleep_gesture_onoff=%d", sleep_flag_run.sleep_anc_mode, sleep_flag_run.sleep_gesture_onoff);
	// SNDP_IF_TRACE(0, "sleep_splaypause_onoff=%d", sleep_flag_run.sleep_splaypause_onoff);

	sndp_dev_sleep_app_set_prompt_onoff(false, sleep_flag_run.sleep_prompt_onoff, false);
	sndp_dev_sleep_app_set_gesture_onoff(false, sleep_flag_run.sleep_gesture_onoff, false);
	sndp_dev_sleep_app_set_eq_index(false, sleep_flag_run.sleep_eq_index, false);
	sndp_dev_sleep_app_anc_mode_set(false, sleep_flag_run.sleep_anc_mode, false);
	sndp_dev_sleep_app_set_splaypause_onoff(false, sleep_flag_run.sleep_splaypause_onoff, false);
}
#endif

void sndp_dev_init(void)
{
	//SPUI_TRACE_ENTER();
#if defined(__SNDP_SLEEP_APP__)
  sndp_load_sleep_app_param();
#endif
	sndp_dev_init_device_info();
    
	sndp_dev_charger_plug_init();
    sndp_dev_charger_init();
    sndp_dev_bat_pwr_init();
    sndp_dev_temperature_init();
    sndp_dev_cover_init();
    sndp_dev_iobox_init();
    sndp_dev_wear_init();
    sndp_dev_gesture_init();
    sndp_dev_hr_init();
    sndp_dev_acc_init();	
}

#endif	/* __SNDP_UI__ */


