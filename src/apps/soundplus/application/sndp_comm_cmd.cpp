#if defined(__SNDP_COMM_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "tgt_hardware.h"
#include "app_utils.h"
#include "factory_section.h"
#include "app_media_player.h"
#include "app_anc.h"
#include "iir_process.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"
#include "sndp_ui.h"

#include "sndp_comm_protocol.h"
#include "sndp_comm_main.h"
#include "sndp_comm_cmd.h"
#include "sndp_comm_ble.h"
#if defined(__SNDP_PSENSOR_JSA1227__)
#include "sndp_jsa1227.h"
#endif

#if defined(__SNDP_ALG_APPLICATION__)
#include "sndp_algo_app.h"
#endif

#if defined(SNDP_TX_AI_ENABLE)
#include "Soundplus_adapter.h"
#endif

#if defined(__SNDP_PRODUCT_TEST__)
#include "sndp_product_test.h"
#endif

#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)
#include "sndp_hal_cover_switch.h"
#include "sndp_cover_switch_box_notify.h"
#endif

#if defined(__SNDP_HEART_RATE_MGR__)
#include "sndp_heart_rate.h"
#if defined(__SNDP_HR_ALGO__)
#include "sleepsense.h"
#endif
#endif

#if defined(__SNDP_HEART_RATE_MGR__)
#include "sndp_hal_hr.h"
#endif

/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/
extern "C" uint8_t sndp_anc_get_calib_result(void);



/**************************************************************************************************
* Variable
**************************************************************************************************/
POSSIBLY_UNUSED static uint8_t dev_test_from = SNDP_COMM_DEVICE_ATE;
POSSIBLY_UNUSED static uint8_t dev_test_path = SNDP_COMM_PATH_POGOPIN;

#if defined(__SNDP_SLEEP_APP__)
POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_sleepapp_start_report_proximity(void);
static bool sndp_comm_proximity_slave_timer_running = false;
static uint32_t sndp_comm_proximity_report_interval_ms = 1000;
static uint32_t sndp_comm_cmd_sleepapp_send_local_proximity_to_peer(void);
static uint32_t sndp_comm_cmd_sleepapp_report_proximity_to_app(void);
static uint32_t sndp_comm_cmd_sleepapp_stop_report_proximity(void);
static bool sndp_comm_sleepapp_report_sleep_tracking = false;
static uint8_t wear_state_update_onoff = 0;
static uint8_t ppg_notify_data[98] = {0};
#endif


/**************************************************************************************************
* Function
**************************************************************************************************/
POSSIBLY_UNUSED static uint8_t sndp_comm_get_cmd_param_earside(void)
{
    uint8_t earside;
    
    if(!sndp_is_besaud_connected()){
        if(sndp_dev_is_left_earphone()) {
		    earside = 0;
    	} else {
    		earside = 1;
    	}
    } else {
        earside = 2;
    }

    return earside;
}

POSSIBLY_UNUSED static bool sndp_comm_is_curr_device_exec(uint8_t earside)
{
    if((sndp_dev_is_left_earphone() && (earside == SNDP_COMM_DEVICE_LEFT || earside == SNDP_COMM_DEVICE_LR))
        || (sndp_dev_is_right_earphone() && (earside == SNDP_COMM_DEVICE_RIGHT || earside == SNDP_COMM_DEVICE_LR))) {

        return true;
    }

    return false;
}

int32_t sndp_comm_cmd_send_cmd_to_peer(sndp_comm_cmd_id_e cmd_id, uint8_t *cmd_data, uint16_t cmd_data_len)
{
    if(!sndp_is_besaud_connected()) {
        COMM_CMD_TRACE(1, "not connected, cmd_id=0x%02X", cmd_id);
        return -1;
    }
    
    return sndp_comm_main_send_cmd_by_id(cmd_id, 
                sndp_comm_get_local_device(), 
                sndp_comm_get_peer_device(), 
                SNDP_COMM_PATH_MS, 
                cmd_data, 
                cmd_data_len);
}

int32_t sndp_comm_cmd_send_cmd_to_app(sndp_comm_cmd_id_e cmd_id, uint8_t *cmd_data, uint16_t cmd_data_len)
{
    return sndp_comm_main_send_cmd_by_id(cmd_id, 
                sndp_comm_get_local_device(), 
                SNDP_COMM_DEVICE_APP, 
                SNDP_COMM_PATH_BLE, 
                cmd_data, 
                cmd_data_len);
}

int32_t sndp_comm_cmd_send_cmd_to_box(sndp_comm_cmd_id_e cmd_id, uint8_t *cmd_data, uint16_t cmd_data_len)
{
    return sndp_comm_main_send_cmd_by_id(cmd_id, 
                sndp_comm_get_local_device(), 
                SNDP_COMM_DEVICE_BOX, 
                SNDP_COMM_PATH_POGOPIN, 
                cmd_data, 
                cmd_data_len);
}

static void sndp_comm_cmd_rsp_with_errcode(sndp_comm_cmd_info_s *cmd_info, uint8_t errcode)
{
	cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = errcode;
	sndp_comm_main_rsp_cmd(cmd_info);
}


static uint32_t sndp_comm_cmd_recv_eb_handshake(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_freeman_pairing(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

#if 0
	if(sndp_is_tws_link_connected()) {
		err_code = SNDP_COMM_ERROR_TWS_DISCONNECTED;
	}
#endif

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
	    sndp_call_func_in_app_thread((uint32_t)sndp_start_freeman_pairing, 0, 0, 0);
    }
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_tws_pairing(sndp_comm_cmd_info_s *cmd_info)
{
	uint32_t data_len;
	uint8_t *data = cmd_info->data;
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
	uint8_t local_bt_addr[6] = {0};
	uint8_t recv_bt_addr[6] = {0};

	if(cmd_info->data_len == 6) {
		for(uint8_t i = 0; i < sizeof(recv_bt_addr); i++) {
			recv_bt_addr[i] = data[i];
		}

		factory_section_original_btaddr_get(local_bt_addr);
	} else {
		err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

    data_len = 0;
	data[data_len++] = err_code;
    for(uint8_t i = 0; i < sizeof(local_bt_addr); i++) {
		data[data_len++] = local_bt_addr[i];
	}
	cmd_info->data_len = data_len;
	sndp_comm_main_rsp_cmd(cmd_info);

    if(err_code == SNDP_COMM_ERROR_NONE) {
    	sndp_tws_pairing_config(recv_bt_addr, sizeof(recv_bt_addr));
    	sndp_call_func_in_app_thread((uint32_t)sndp_start_tws_pairing, 0, 0, 0);
    }
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_query_pairing_status(sndp_comm_cmd_info_s *cmd_info)
{
	uint32_t data_len;
	uint8_t *data = cmd_info->data;
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

#if 0
    data_len = 0;
	data[data_len++] = err_code;
	data[data_len++] = sndp_is_tws_link_connected() ? 0x01 : 0x00;
    if(sndp_is_tws_master_mode()) {
	    data[data_len++] = sndp_is_master_mobile_link_connected() ? 0x01 : 0x00;
    } else if(sndp_is_tws_slave_mode()) {
	    data[data_len++] = sndp_is_slave_ibrt_link_connected() ? 0x01 : 0x00;
    } else {
        data[data_len++] = sndp_is_master_mobile_link_connected() ? 0x01 : 0x00;
    }
#else
    data_len = 0;
	data[data_len++] = err_code;
	data[data_len++] = sndp_get_pairing_type();
    data[data_len++] = sndp_get_pairing_status();
#endif

	cmd_info->data_len = data_len;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_query_bat_info(sndp_comm_cmd_info_s *cmd_info)
{
	uint32_t data_len;
	uint8_t *data = cmd_info->data;
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint16_t bat_voltage;
    uint8_t bat_percent;

    bat_voltage = sndp_dev_get_bat_voltage(false);
    bat_percent = sndp_dev_get_bat_percentage(false);

	data_len = 0;
	data[data_len++] = err_code;
    data[data_len++] = (bat_voltage>>8)&0xFF;
    data[data_len++] = bat_voltage&0xFF;
	data[data_len++] = bat_percent;
	cmd_info->data_len = data_len;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_shutdown(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	sndp_call_func_in_app_thread((uint32_t)sndp_app_shutdown, SNDP_SHUTDOWN_REASON_SHUTDOWN_CMD, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_restore_factory_setting(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	sndp_call_func_in_app_thread((uint32_t)sndp_enter_restore_factory_setting, 0, 0, 0);
	return 0;
}

#define FW_UPGRADE_MIN_BAT_PER (10)
static uint32_t sndp_comm_cmd_recv_eb_enter_fw_upgrade(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

	if(sndp_dev_get_bat_percentage(false) < FW_UPGRADE_MIN_BAT_PER) {
        COMM_CMD_TRACE(1, "bat_percent(%d) < %d", sndp_dev_get_bat_percentage(false), FW_UPGRADE_MIN_BAT_PER);
		err_code = SNDP_COMM_ERROR_BAT_LOW;
	}

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

	if(err_code == SNDP_COMM_ERROR_NONE) {
		sndp_call_func_in_app_thread((uint32_t)sndp_enter_single_dld_mode, 0, 0, 0);
	}
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_report_box_info(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
	uint8_t fw_ver[3];
	sndp_dev_bat_info_s bat_info;

	COMM_CMD_TRACE(1, "data_len=%d", cmd_info->data_len);

	if(cmd_info->data_len == 6) {
		memcpy(fw_ver, &cmd_info->data[0], 3);
		bat_info.bat_volt = (uint16_t)((cmd_info->data[3]<<8)|cmd_info->data[4]);
		bat_info.bat_per = cmd_info->data[5];
		
		sndp_dev_set_box_fw_ver(fw_ver);
		sndp_dev_set_box_bat_info(bat_info);
	} else {
		err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_report_cover_status(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

	if(cmd_info->data_len == 1) {
		COMM_CMD_TRACE(1, "box cover=%d", cmd_info->data[0]);
		//TODO:
 
#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)    
        sndp_dev_cover_status_e cover_status;
        if(cmd_info->data[0] == 0)
            cover_status = SNDP_DEV_COVER_COLSED;
        else
            cover_status = SNDP_DEV_COVER_OPENED;  
		sndp_call_func_in_app_thread((uint32_t)sndp_dev_cover_status_changed_handler, cover_status, 0, 0);
#endif
	} else {
		err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_mobile_pair_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = sndp_get_mobile_pairing_count();
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_eb_switch_language(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint32_t lang_idx = 0;
    
    if(cmd_info->data_len == 1) {
		lang_idx = cmd_info->data[0];
		
		if((lang_idx != LANGUAGE_ID_EN)
            && (lang_idx != LANGUAGE_ID_CN)) {
            
			err_code = SNDP_COMM_ERROR_PARAM_OUT_RANG;
		}
	} else {
		err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
	}

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_call_func_in_app_thread((uint32_t) sndp_language_switch_handler, lang_idx, 0, 0);
	}
	return 0;
}


uint32_t sndp_comm_cmd_send_lr_sync_working_mode(uint8_t mode)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_WORKING_MODE, (uint8_t *)&mode, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_working_mode(sndp_comm_cmd_info_s *cmd_info)
{
	POSSIBLY_UNUSED uint8_t mode;

    if(cmd_info->data_len == 1) {
        mode = cmd_info->data[0];

        //TODO:
    }
   
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_bat_info(void)
{
	sndp_dev_bat_info_s bat_info;
    
    sndp_dev_get_bat_info(false, &bat_info);
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_BAT_INFO, (uint8_t *)&bat_info, sizeof(sndp_dev_bat_info_s));
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_bat_info(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_dev_bat_info_s bat_info;
    
	memcpy((void *)&bat_info, cmd_info->data, cmd_info->data_len);
	sndp_dev_set_bat_info(true, bat_info);
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_volume(uint8_t level)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_VOLUME, &level, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_volume(sndp_comm_cmd_info_s *cmd_info)
{
    POSSIBLY_UNUSED uint8_t volume;

    if(cmd_info->data_len == 1) {
        volume = cmd_info->data[0];

        //TODO:
    }
    
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_iobox_status(uint8_t status)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_INBOX_STATUS, &status, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_iobox_status(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
	    sndp_dev_iobox_set_status(true, (sndp_dev_iobox_status_e)cmd_info->data[0]);
    }
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_cover_status(uint8_t status)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_COVER_STATUS, &status, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_cover_status(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
	    sndp_dev_cover_set_status(true, (sndp_dev_cover_status_e)cmd_info->data[0]);
    }
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_wear_status(uint8_t status)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_WEAR_STATUS, &status, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_wear_status(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
    	sndp_dev_wear_set_status(true, (sndp_dev_wear_status_e)cmd_info->data[0]);
        sndp_call_func_in_app_thread((uint32_t) sndp_ui_wear_action, cmd_info->data[0], true, 0);
#if defined(__SNDP_SLEEP_APP__)
        //统计对耳传来的佩戴和脱戴次数，才会发送数据给app。
        if(sndp_dev_is_left_earphone()){
           sndp_call_func_in_app_thread((uint32_t)sndp_comm_cmd_sleepapp_wear_state_update,SNDP_DEV_EARSIDE_RIGHT,cmd_info->data[0],0);
        }
        else if(sndp_dev_is_right_earphone()){
            sndp_call_func_in_app_thread((uint32_t)sndp_comm_cmd_sleepapp_wear_state_update,SNDP_DEV_EARSIDE_LEFT,cmd_info->data[0],0);
        }
#endif
    }
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_gesture(uint8_t gesture)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_GESTURE, &gesture, 1);
	return 0;
}


static uint32_t sndp_comm_cmd_recv_lr_sync_gesture(sndp_comm_cmd_info_s *cmd_info)
{
	POSSIBLY_UNUSED uint8_t event;

    if(cmd_info->data_len == 1) {
        event = cmd_info->data[0];

        //TODO:
    }
    
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_both_shutdown(void)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_BOTH_SHUTDOWN, NULL, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_both_shutdown(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_app_shutdown(SNDP_SHUTDOWN_REASON_BOTH_SHUTDOWN);
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_music_ctrl(uint8_t event)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_MUSIC_CTRL, &event, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_music_ctrl(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
	    sndp_music_ctrl((sndp_music_ctrl_event_e)cmd_info->data[0]);
    }
	return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_call_ctrl(uint8_t event)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_CALL_CTRL, &event, 1);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_call_ctrl(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
	    sndp_call_ctrl((sndp_call_ctrl_event_e)cmd_info->data[0]);
    }
	return 0;
}

#if defined(__SNDP_SLEEP_APP__)
uint32_t sndp_comm_cmd_send_lr_sync_eq_set(uint8_t eqmode)
{
    uint8_t data = eqmode;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_EQ_INDEX, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_eq_set(sndp_comm_cmd_info_s *cmd_info)
{
     if(cmd_info->data_len == 1) {
        sndp_dev_sleep_app_set_eq_index(false, cmd_info->data[0], true);
    }   
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_anc_mode(uint8_t ancmode,uint8_t is_save)
{
    uint8_t data[2] = {ancmode, is_save};
    // COMM_CMD_TRACE(1, "ancmode=%d, is_save=%d", ancmode, is_save);
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_ANC_MODE, data, 2);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_anc_mode(sndp_comm_cmd_info_s *cmd_info)
{
     if(cmd_info->data_len == 2) {
        // COMM_CMD_TRACE(1, "ancmode=%d, is_save=%d", cmd_info->data[0], cmd_info->data[1]);
        sndp_dev_sleep_app_anc_mode_set(false, cmd_info->data[0], cmd_info->data[1]);
        if(sndp_dev_wear_is_worn(false)){
            sndp_anc_mode_set_locally((sndp_anc_mode_e)cmd_info->data[0]);
        }
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_prompt_onoff(uint8_t onoff)
{
    uint8_t data = onoff;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_PROMPT_ONOFF, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_prompt_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
        sndp_dev_sleep_app_set_prompt_onoff(false, cmd_info->data[0], true);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_gesture_onoff(uint8_t onoff)
{
    uint8_t data = onoff;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_GESTRUE_ONOFF, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_gesture_onoff(sndp_comm_cmd_info_s *cmd_info)
{
     if(cmd_info->data_len == 1) {
        COMM_CMD_TRACE(0,"data_len:%d data:%d", cmd_info->data_len,cmd_info->data[0]);
        sndp_dev_sleep_app_set_gesture_onoff(false, cmd_info->data[0], true);
    }
    return 0;
}


uint32_t sndp_comm_cmd_send_lr_sync_splaypause_onoff(uint8_t onoff)
{
    uint8_t data = onoff;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_SPLAYPAUSE_ONOFF, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_splaypause_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
     sndp_dev_sleep_app_set_splaypause_onoff(false, cmd_info->data[0], true);
    }
   
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_update_mapping(uint8_t key_behavior,uint8_t key_function)
{
    uint8_t data[] = {key_behavior, key_function};
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_UPDATE_MAPPING, data, sizeof(data));
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_update_mapping(sndp_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_GESTURE_MAP__)
    uint8_t key_behavior = cmd_info->data[0];
    uint8_t key_function = cmd_info->data[1];
    if(cmd_info->data_len == 2) {

        sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)key_behavior, (sndp_dev_function_type_t)key_function);

    }
#endif
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_findme_onoff(uint8_t onoff)
{
    uint8_t data = onoff;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_FINDME_ONOFF, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_findme_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
      sndp_call_func_in_app_thread((uint32_t)sndp_play_findme,0,0,0);
    }
   
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(unsigned short proximity_value)
{
    uint8_t data[2];
    data[0] = (uint8_t)((proximity_value >> 8) & 0xFF);
    data[1] = (uint8_t)(proximity_value & 0xFF);
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_Proximity_Notification_DATA, data, sizeof(data));
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_Proximity_Notification_DATA(sndp_comm_cmd_info_s *cmd_info)
{
    if (cmd_info->data_len == 2) {
        unsigned short peer_value = ((unsigned short)cmd_info->data[0] << 8) | (unsigned short)cmd_info->data[1];
        sndp_dev_sleep_app_set_proximity_data(true, peer_value);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(uint8_t onoff)
{
    uint8_t data = onoff;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_Proximity_Notification_ONOFF, &data, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_Proximity_Notification_ONOFF(sndp_comm_cmd_info_s *cmd_info)
{
    if (cmd_info->data_len != 1) {
        return 0;
    }

    uint8_t onoff = cmd_info->data[0];
    sndp_dev_sleep_app_set_proximity_onoff(false, onoff);

    if (onoff == 0x01) {
        if (!sndp_comm_proximity_slave_timer_running) {
            sndp_comm_proximity_slave_timer_running = true;
            sndp_comm_cmd_sleepapp_send_local_proximity_to_peer();
        }
    } else {
        if (sndp_comm_proximity_slave_timer_running) {
            sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer);
            sndp_comm_proximity_slave_timer_running = false;
        }
    }

    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_start_heartrate_measure(uint8_t sampling_rate, uint8_t dump_data)
{
    uint8_t data[2] = {sampling_rate, dump_data};
    int32_t result = sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_START_HEARTRATE_MEASUREMENT, data, sizeof(data));
    COMM_CMD_TRACE(1, "send cmd failed, result=%d", result);
    return 0;
}

uint32_t sndp_comm_cmd_recv_lr_sync_start_heartrate_measure(sndp_comm_cmd_info_s *cmd_info)
{
    if (cmd_info->data_len == 2) {
        uint8_t sampling_rate = cmd_info->data[0];
        uint8_t dump_data = cmd_info->data[1];
        sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_start, sampling_rate, dump_data, 0);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_stop_heartrate_measure(void)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_STOP_HEARTRATE_MEASUREMENT, NULL, 0);
    return 0;
}

uint32_t sndp_comm_cmd_recv_lr_sync_stop_heartrate_measure(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
    return 0;
}
#endif

uint32_t sndp_comm_cmd_send_lr_sync_all_dev_status(uint8_t *data, uint16_t data_len)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_ALL_DEV_STATUS, data, data_len);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_all_dev_status(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len > 0) {
        sndp_ui_all_status_sync_recv(cmd_info->data, cmd_info->data_len);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_bt_onoff(uint8_t onoff)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_BT_ONOFF, &onoff, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_bt_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
        sndp_bt_switch(cmd_info->data[0] ? true : false, false);
    }
    return 0;
}



#if defined(__SNDP_PRODUCT_TEST__)
static uint32_t sndp_comm_cmd_recv_pt_switch_test_mode(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

    if(cmd_info->data_len == 1) {
        sndp_pt_set_test_mode(cmd_info->data[0]);
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_test_mode(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = sndp_pt_get_test_mode();
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_shutdown(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	sndp_call_func_in_app_thread((uint32_t) sndp_app_shutdown, SNDP_SHUTDOWN_REASON_PT_SHUTDOWN_CMD, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_reboot(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

	sndp_call_func_in_app_thread((uint32_t) sndp_app_reboot, 0, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_enter_shipmode(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

	sndp_call_func_in_app_thread((uint32_t) sndp_enter_shipmode, 0, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_restore_factory_mode(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

	sndp_call_func_in_app_thread((uint32_t) sndp_enter_restore_factory_setting, 0, 0, 0);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_single_pairing(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

	sndp_call_func_in_app_thread((uint32_t)sndp_start_freeman_pairing, 0, 0, 0);
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_tws_pairing(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint8_t local_bt_addr[6] = {0};
    uint8_t recv_bt_addr[6] = {0};

    if(cmd_info->data_len == 6) {
        for(uint8_t i = 0; i < sizeof(recv_bt_addr); i++) {
            recv_bt_addr[i] = cmd_info->data[i];
        }

        factory_section_original_btaddr_get(local_bt_addr);
    } else {
        err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
    }

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = err_code;
    for(uint8_t i = 0; i < sizeof(local_bt_addr); i++) {
        cmd_info->data[cmd_info->data_len++] = local_bt_addr[i];
    }
    sndp_comm_main_rsp_cmd(cmd_info);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_tws_pairing_config(recv_bt_addr, sizeof(recv_bt_addr));
        sndp_call_func_in_app_thread((uint32_t)sndp_start_tws_pairing, 0, 0, 0);
    }
    return 0;
}


static uint32_t sndp_comm_cmd_recv_pt_enter_dut_mode(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

	sndp_call_func_in_app_thread((uint32_t) sndp_enter_dut_mode, 0, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_exit_dut_mode(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);

    sndp_delay_exec_start(100, (uint32_t)sndp_app_reboot, 0, 0, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_fw_ver(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t *fw_ver = sndp_dev_get_fw_ver(false);
    
    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = fw_ver[0];
	cmd_info->data[cmd_info->data_len++] = fw_ver[1];;
	cmd_info->data[cmd_info->data_len++] = fw_ver[2];;
	cmd_info->data[cmd_info->data_len++] = fw_ver[3];;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_hw_ver(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t *hw_ver = sndp_dev_get_hw_ver(false);
    
	/* RSP Data Setting */
	cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = hw_ver[0];
	cmd_info->data[cmd_info->data_len++] = hw_ver[1];
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_bat_info(sndp_comm_cmd_info_s *cmd_info)
{
    uint16_t bat_voltage;
    uint8_t bat_percent;

    bat_voltage = sndp_dev_get_bat_voltage(false);
    bat_percent = sndp_dev_get_bat_percentage(false);

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = (bat_voltage>>8)&0xFF;
    cmd_info->data[cmd_info->data_len++] = bat_voltage&0xFF;
    cmd_info->data[cmd_info->data_len++] = bat_percent;
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_inout_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_dev_iobox_get_status(false);
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_cover_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_dev_cover_get_status(false);
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_wear_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_dev_wear_get_status(false);
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_dev_sn(sndp_comm_cmd_info_s *cmd_info)
{
	uint8_t *sn = NULL;
	uint16_t sn_len;
	
    sn = sndp_dev_get_dev_sn();
	sn_len = strlen((char *)sn);

    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    if(sn != NULL) {
	    memcpy(&cmd_info->data[cmd_info->data_len], sn, sn_len);
    }
	cmd_info->data_len += sn_len;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_write_dev_sn(sndp_comm_cmd_info_s *cmd_info)
{
	bool ret = sndp_dev_save_dev_sn(cmd_info->data, cmd_info->data_len);

	cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = ret ? 0x01 : 0x00;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_bt_addr(sndp_comm_cmd_info_s *cmd_info)
{
	uint8_t *bt_addr = sndp_dev_get_bt_addr(false);
	
	cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	memcpy(&cmd_info->data[cmd_info->data_len], bt_addr, 6);
	cmd_info->data_len += 6;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_set_bt_addr(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_ble_addr(sndp_comm_cmd_info_s *cmd_info)
{
	uint8_t *ble_addr = sndp_dev_get_ble_addr(false);

    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	memcpy(&cmd_info->data[cmd_info->data_len], ble_addr, 6);
	cmd_info->data_len += 6;
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_set_ble_addr(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_frequency_offset(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint32_t xtal_fcap = 0;
	
	if(factory_section_xtal_fcap_get(&xtal_fcap) != 0) {
        err_code = SNDP_COMM_ERROR_READ_FAIL;
        xtal_fcap = 0;
	} 

	cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = err_code;
    cmd_info->data[cmd_info->data_len++] = (xtal_fcap>>24)&0xff;
    cmd_info->data[cmd_info->data_len++] = (xtal_fcap>>16)&0xff;
    cmd_info->data[cmd_info->data_len++] = (xtal_fcap>>8)&0xff;
    cmd_info->data[cmd_info->data_len++] = xtal_fcap&0xff;
	sndp_comm_main_rsp_cmd(cmd_info);

	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_write_frequency_offset(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint32_t index = 0;
    uint32_t xtal_fcap = 0;

    if(cmd_info->data_len == 4) {
        xtal_fcap = 0;
        xtal_fcap = (cmd_info->data[index++]<<24);
        xtal_fcap |= (cmd_info->data[index++]<<16);
        xtal_fcap |= (cmd_info->data[index++]<<8);
        xtal_fcap |= cmd_info->data[index++];
        COMM_CMD_TRACE(1, "xtal_fcap=%d", xtal_fcap);

        if(xtal_fcap < 20 || xtal_fcap > 511) {
             err_code = SNDP_COMM_ERROR_PARAM_OUT_RANG;
        }
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_call_func_in_app_thread((uint32_t) factory_section_xtal_fcap_set, xtal_fcap, 0, 0);
    }
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_test_mic(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
	uint8_t mic_index = 0;
    
	if(cmd_info->data_len == 1) {
        mic_index = cmd_info->data[0];
	} else {
		err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_call_func_in_app_thread((uint32_t) sndp_pt_set_test_mic_index, mic_index, 0, 0);
    }
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_test_speaker(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
    
	sndp_call_func_in_app_thread((uint32_t) sndp_start_freeman_pairing, 0, 0, 0);
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_test_gsensor(sndp_comm_cmd_info_s *cmd_info)
{
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_test_hrsensor(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
	return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_query_dev_status(sndp_comm_cmd_info_s *cmd_info)
{
	/* RSP Data Setting */
	/* bat voltage(2) + bat percentage(1) + charging status(1) + iobox(1) + cover status(1) + wear status(1) */
	uint16_t voltage = sndp_dev_get_bat_voltage(false);

	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
	cmd_info->data[cmd_info->data_len++] = (uint8_t)((voltage>>8)&0xff);
	cmd_info->data[cmd_info->data_len++] = (uint8_t)((voltage>>0)&0xff);
	cmd_info->data[cmd_info->data_len++] = sndp_dev_get_bat_percentage(false);
	cmd_info->data[cmd_info->data_len++] = sndp_dev_iobox_get_status(false);
	cmd_info->data[cmd_info->data_len++] = sndp_dev_cover_get_status(false);
	cmd_info->data[cmd_info->data_len++] = sndp_dev_wear_get_status(false);
	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_anc_calib_status(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t result;
    
    result = 0; //sndp_anc_get_calib_result();
    
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = result;
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_algo_auth_result(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
#if defined(SNDP_TX_AI_ENABLE)
    cmd_info->data[cmd_info->data_len++] = soundplus_auth_status();
#else
    cmd_info->data[cmd_info->data_len++] = 0;
#endif
    sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_log_output_switch(sndp_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_LOG_OUTPUT_SWITCH__)     
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    bool log_en = true;
    
    if(cmd_info->data_len == 1) {
        log_en = cmd_info->data[0] ? (true) : (false);
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_delay_exec_start(50, (uint32_t) hal_trace_log_output_switch, log_en, 0, 0);
    }
    
#else
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
#endif   
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_switch_wear_status_report(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    bool report = false;
    
    if(cmd_info->data_len > 0) {
        dev_test_from = COMM_GET_FROM(cmd_info->fromto);
        dev_test_path = cmd_info->path;
        report = cmd_info->data[0] ? true : false;
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    if(err_code == SNDP_COMM_ERROR_NONE) {
        sndp_pt_switch_wear_status_report(report);  
    }

    return 0;
}

uint32_t sndp_comm_cmd_send_pt_report_wear_status(uint8_t status)
{	
	uint8_t data[32];
	uint32_t data_len;

    COMM_CMD_TRACE(2, "status=%d", status);

    data_len = 0;
	data[data_len++] = SNDP_COMM_ERROR_NONE;
	data[data_len++] = status;

	sndp_comm_main_send_cmd_by_id(COMM_CMDID_PT_REPORT_WEAR_STATUS, 
            sndp_comm_get_local_device(), 
            dev_test_from, 
            dev_test_path, 
            data, 
            data_len);
	return 0;
}
#endif


static uint32_t sndp_comm_cmd_recv_pt_check_earside(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_dev_get_earside(false);
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_hall_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_pt_read_hall_status();
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_proximity_value(sndp_comm_cmd_info_s *cmd_info)
{
    uint16_t value = 0;

    sndp_dev_hr_read_proximity_value(&value);
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((value>>8)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(value&0xff);
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_write_proximity_thresold(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint16_t high_threshold;
    uint16_t low_threshold;

    if(cmd_info->data_len == 4) {
        high_threshold = (cmd_info->data[0]<<8) | cmd_info->data[1];
        low_threshold = (cmd_info->data[2]<<8) | cmd_info->data[3];
        COMM_CMD_TRACE(1, "h=%d, l=%d", high_threshold, low_threshold);

        sndp_hal_hr_write_proximity_threshold(high_threshold, low_threshold);
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
   
    return 0;
}




/*--------------------------------------------- APP Command Start-----------------------------------------------------*/
static uint32_t sndp_comm_cmd_recv_app_query_dev_info(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;

    //hw version
	memcpy(&cmd_info->data[cmd_info->data_len], sndp_dev_get_hw_ver(false), 2);
    cmd_info->data_len += 2;
    
	//sw version
	memcpy(&cmd_info->data[cmd_info->data_len], sndp_dev_get_fw_ver(false), 4);
    cmd_info->data_len += 4;
        
	//bt addr
	memcpy(&cmd_info->data[cmd_info->data_len], sndp_dev_get_bt_addr(false), 6);    //right local
    cmd_info->data_len += 6;

	sndp_comm_main_rsp_cmd(cmd_info);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_app_query_dev_status(sndp_comm_cmd_info_s *cmd_info)
{
	uint8_t local_bat_per = sndp_dev_get_bat_percentage(false)&0x7f;
	
	cmd_info->data_len = 0;
	cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;

    /** bat info */
    if(sndp_dev_charger_is_charging(false))
        cmd_info->data[cmd_info->data_len++] = local_bat_per|0x80;
    else
        cmd_info->data[cmd_info->data_len++] = local_bat_per;

    /** bt connection status */
    cmd_info->data[cmd_info->data_len] = 0;
    if(sndp_is_tws_link_connected())
        cmd_info->data[cmd_info->data_len] |= 0x01;  
    if(sndp_is_master_mobile_link_connected())
        cmd_info->data[cmd_info->data_len] |= 0x10;  
    cmd_info->data_len++;

    /** call status */
	cmd_info->data[cmd_info->data_len++] = sndp_call_is_calling();

    /** wear status */
    if(sndp_dev_wear_is_worn(false))
        cmd_info->data[cmd_info->data_len] = 1;
    else
        cmd_info->data[cmd_info->data_len] = 0;
    
    cmd_info->data_len++;

	sndp_comm_main_rsp_cmd(cmd_info);

	return 0;
}


static const sndp_comm_cmd_handle_s sndp_comm_cmd_hdlr_list[] = {
    /****** 与充电仓交互指令 ******/
	{ COMM_CMDID_EB_HANDSHAKE                   , "EB_HANDSHAKE"            , sndp_comm_cmd_recv_eb_handshake                   },
	{ COMM_CMDID_EB_FREEMAN_PAIRING             , "EB_FREEMAN_PAIRING"      , sndp_comm_cmd_recv_eb_freeman_pairing             },
	{ COMM_CMDID_EB_TWS_PAIRING                 , "EB_TWS_PAIRING"          , sndp_comm_cmd_recv_eb_tws_pairing                 },
	{ COMM_CMDID_EB_QUERY_PARING_STATUS         , "EB_Q_PARING_STA"         , sndp_comm_cmd_recv_eb_query_pairing_status        },
	{ COMM_CMDID_EB_QUERY_BAT_INFO              , "EB_Q_BAT_INFO"           , sndp_comm_cmd_recv_eb_query_bat_info              },
	{ COMM_CMDID_EB_EARBUDS_SHUTDOWN            , "EB_EARBUDS_SHUTDOWN"     , sndp_comm_cmd_recv_eb_shutdown                    },
	{ COMM_CMDID_EB_RESTORE_FACTORY_SETTING     , "EB_RESTORE_FS"           , sndp_comm_cmd_recv_eb_restore_factory_setting     },
	{ COMM_CMDID_EB_ENTER_FW_UPGRADE            , "EB_ENTER_FW_UPGRADE"     , sndp_comm_cmd_recv_eb_enter_fw_upgrade            },
	{ COMM_CMDID_EB_REPORT_BOX_INFO             , "EB_R_BOX_INFO"           , sndp_comm_cmd_recv_eb_report_box_info             },
	{ COMM_CMDID_EB_REPORT_COVER_STATUS         , "EB_R_COVER_STA"          , sndp_comm_cmd_recv_eb_report_cover_status         },
	{ COMM_CMDID_EB_QUERY_MOBILE_PAIRED_COUNT   , "EB_Q_MB_PAIRED_CNT"      , sndp_comm_cmd_recv_eb_mobile_pair_status          },
    { COMM_CMDID_EB_SWITCH_LANGUAGE             , "EB_S_LANGUAGE"	        , sndp_comm_cmd_recv_eb_switch_language 		    },

    /****** 左右耳同步指令. ******/
    { COMM_CMDID_LR_SYNC_WORKING_MODE           , "LR_SYNC_WORKING_MODE"    , sndp_comm_cmd_recv_lr_sync_working_mode           },
	{ COMM_CMDID_LR_SYNC_BAT_INFO               , "LR_SYNC_BAT_INFO"        , sndp_comm_cmd_recv_lr_sync_bat_info               },
	{ COMM_CMDID_LR_SYNC_VOLUME                 , "LR_SYNC_VOLUME"          , sndp_comm_cmd_recv_lr_sync_volume                 },
	{ COMM_CMDID_LR_SYNC_INBOX_STATUS           , "LR_SYNC_IOBOX_STA"       , sndp_comm_cmd_recv_lr_sync_iobox_status           },
	{ COMM_CMDID_LR_SYNC_COVER_STATUS           , "LR_SYNC_COVER_STA"	    , sndp_comm_cmd_recv_lr_sync_cover_status           },
	{ COMM_CMDID_LR_SYNC_WEAR_STATUS            , "LR_SYNC_WEAR_STA"	    , sndp_comm_cmd_recv_lr_sync_wear_status            },
	{ COMM_CMDID_LR_SYNC_GESTURE                , "LR_SYNC_GESTURE"         , sndp_comm_cmd_recv_lr_sync_gesture                },
    { COMM_CMDID_LR_SYNC_BOTH_SHUTDOWN          , "LR_SYNC_BOTH_SHUTDOWN"   , sndp_comm_cmd_recv_lr_sync_both_shutdown          },
    { COMM_CMDID_LR_SYNC_MUSIC_CTRL             , "LR_SYNC_MUSIC_CTRL"      , sndp_comm_cmd_recv_lr_sync_music_ctrl             },
    { COMM_CMDID_LR_SYNC_CALL_CTRL              , "LR_SYNC_CALL_CTRL"       , sndp_comm_cmd_recv_lr_sync_call_ctrl              },
#if defined(__SNDP_SLEEP_APP__)
    { COMM_CMDID_LR_SYNC_EQ_INDEX               , "LR_SYNC_EQ_INDEX"    , sndp_comm_cmd_recv_lr_sync_eq_set           },
    { COMM_CMDID_LR_SYNC_ANC_MODE               , "LR_SYNC_ANC_MODE"    , sndp_comm_cmd_recv_lr_sync_anc_mode           },
    { COMM_CMDID_LR_SYNC_PROMPT_ONOFF           , "LR_SYNC_PROMPT_ONOFF"    , sndp_comm_cmd_recv_lr_sync_prompt_onoff           },
    { COMM_CMDID_LR_SYNC_GESTRUE_ONOFF           , "LR_SYNC_GESTRUE_ONOFF"    , sndp_comm_cmd_recv_lr_sync_gesture_onoff           },
    { COMM_CMDID_LR_SYNC_SPLAYPAUSE_ONOFF          , "LR_SYNC_SPLAYPAUSE_ONOFF"   , sndp_comm_cmd_recv_lr_sync_splaypause_onoff          },
    { COMM_CMDID_LR_SYNC_UPDATE_MAPPING         , "LR_SYNC_UPDATE_MAPPING"  , sndp_comm_cmd_recv_lr_sync_update_mapping         },
    { COMM_CMDID_LR_SYNC_FINDME_ONOFF           , "LR_SYNC_FINEDME_ONOFF"  , sndp_comm_cmd_recv_lr_sync_findme_onoff         },
    { COMM_CMDID_LR_SYNC_Proximity_Notification_ONOFF          , "LR_SYNC_Proximity_Notification_ONOFF"  , sndp_comm_cmd_recv_lr_sync_Proximity_Notification_ONOFF         },
    { COMM_CMDID_LR_SYNC_Proximity_Notification_DATA           , "LR_SYNC_Proximity_Notification_DATA"  , sndp_comm_cmd_recv_lr_sync_Proximity_Notification_DATA         },
    { COMM_CMDID_LR_SYNC_START_HEARTRATE_MEASUREMENT           , "LR_SYNC_START_HR_MEASURE"  , sndp_comm_cmd_recv_lr_sync_start_heartrate_measure         },
    { COMM_CMDID_LR_SYNC_STOP_HEARTRATE_MEASUREMENT            , "LR_SYNC_STOP_HR_MEASURE"  , sndp_comm_cmd_recv_lr_sync_stop_heartrate_measure         },
#endif
    { COMM_CMDID_LR_SYNC_ALL_DEV_STATUS         , "LR_SYNC_ALL_DEV_STATUS"  , sndp_comm_cmd_recv_lr_sync_all_dev_status         },
    { COMM_CMDID_LR_SYNC_BT_ONOFF               , "LR_SYNC_BT_ONOFF"        , sndp_comm_cmd_recv_lr_sync_bt_onoff               },
    
#if defined(__SNDP_PRODUCT_TEST__)
    /****** 生产测试指令. ******/
	{ COMM_CMDID_PT_SWITCH_TEST_MODE            , "PT_S_TEST_MODE"          , sndp_comm_cmd_recv_pt_switch_test_mode            },
	{ COMM_CMDID_PT_QUERY_TEST_MODE             , "PT_Q_TEST_MODE"          , sndp_comm_cmd_recv_pt_query_test_mode             },
    { COMM_CMDID_PT_SHUTDOWN                    , "PT_SHUTDOWN "	        , sndp_comm_cmd_recv_pt_shutdown                    },
	{ COMM_CMDID_PT_REBOOT                      , "PT_REBOOT"               , sndp_comm_cmd_recv_pt_reboot                      },
	{ COMM_CMDID_PT_ENTER_SHIPMODE              , "PT_ENTER_SHIPMODE"       , sndp_comm_cmd_recv_pt_enter_shipmode              },
	{ COMM_CMDID_PT_RESTORE_FACTORY_SETTING     , "PT_RESTORE_FS"           , sndp_comm_cmd_recv_pt_restore_factory_mode        },
	{ COMM_CMDID_PT_SINGLE_PAIRING              , "PT_SINGLE_PAIRING"       , sndp_comm_cmd_recv_pt_single_pairing              },
	{ COMM_CMDID_PT_TWS_PAIRING                 , "PT_TWS_PAIRING"          , sndp_comm_cmd_recv_pt_tws_pairing                 },
	{ COMM_CMDID_PT_ENTER_DUT                   , "PT_ENTER_DUT"            , sndp_comm_cmd_recv_pt_enter_dut_mode              },
	{ COMM_CMDID_PT_EXIT_DUT                    , "PT_EXIT_DUT"	            , sndp_comm_cmd_recv_pt_exit_dut_mode		        },
    { COMM_CMDID_PT_QUERY_FW_VER                , "PT_Q_FW_VER"	            , sndp_comm_cmd_recv_pt_query_fw_ver                },
	{ COMM_CMDID_PT_QUERY_HW_VER                , "PT_Q_HW_VER"	            , sndp_comm_cmd_recv_pt_query_hw_ver                },
    { COMM_CMDID_PT_QUERY_DEV_SN                , "PT_Q_DEV_SN "            , sndp_comm_cmd_recv_pt_query_dev_sn                },
	{ COMM_CMDID_PT_WRITE_DEV_SN                , "PT_W_DEV_SN"	            , sndp_comm_cmd_recv_pt_write_dev_sn                },
    { COMM_CMDID_PT_QUERY_BT_ADDR               , "PT_Q_BT_ADDR"	        , sndp_comm_cmd_recv_pt_query_bt_addr               },
	{ COMM_CMDID_PT_SET_BT_ADDR                 , "PT_S_BT_ADDR "	        , sndp_comm_cmd_recv_pt_set_bt_addr                 },
	{ COMM_CMDID_PT_QUERY_BLE_ADDR              , "PT_Q_BLE_ADDR"	        , sndp_comm_cmd_recv_pt_query_ble_addr		        },
	{ COMM_CMDID_PT_SET_BLE_ADDR                , "PT_SET_BLE_ADDR"	        , sndp_comm_cmd_recv_pt_set_ble_addr		        },
    { COMM_CMDID_PT_QUERY_RF_FREQUENCY_OFFSET   , "PT_Q_RF_FQ_OFF"          , sndp_comm_cmd_recv_pt_query_frequency_offset      }, 
	{ COMM_CMDID_PT_WRITE_RF_FREQUENCY_OFFSET   , "PT_W_RF_FQ_OFF"          , sndp_comm_cmd_recv_pt_write_frequency_offset      },	
	{ COMM_CMDID_PT_TEST_MIC                    , "PT_TEST_MIC"	            , sndp_comm_cmd_recv_pt_test_mic                    },
    { COMM_CMDID_PT_TEST_SPK                    , "PT_TEST_SPK"	            , sndp_comm_cmd_recv_pt_test_speaker                },
    { COMM_CMDID_PT_TEST_GSENSOR                , "PT_TEST_GSENSOR"	        , sndp_comm_cmd_recv_pt_test_gsensor                     },
    { COMM_CMDID_PT_TEST_HRSENSOR               , "PT_TEST_HRSENSOR"	    , sndp_comm_cmd_recv_pt_test_hrsensor                    },
	{ COMM_CMDID_PT_QUERY_DEV_STATUS            , "PT_Q_DEV_STA"	        , sndp_comm_cmd_recv_pt_query_dev_status            },
	{ COMM_CMDID_PT_READ_ANC_CALIB_STATUS       , "PT_R_ANC_CALIB_STA"      , sndp_comm_cmd_recv_pt_read_anc_calib_status       },
    { COMM_CMDID_PT_READ_ALGO_AUTH_RESULT       , "PT_R_ALGO_AUTH_RST"      , sndp_comm_cmd_recv_pt_read_algo_auth_result       },
    { COMM_CMDID_PT_SWITCH_LOG_OUTPUT           , "PT_S_LOG_OUTPUT"         , sndp_comm_cmd_recv_pt_log_output_switch           },
    { COMM_CMDID_PT_SWITCH_WEAR_STATUS_REPORT   , "PT_S_WEAR_STA_RPT"       , sndp_comm_cmd_recv_pt_switch_wear_status_report   },
    { COMM_CMDID_PT_CHECK_EARSIDE               , "PT_C_EARSIDE"            , sndp_comm_cmd_recv_pt_check_earside               },
    { COMM_CMDID_PT_READ_HALL_STATUS            , "PT_R_HALL_STA"           , sndp_comm_cmd_recv_pt_read_hall_status            },
    { COMM_CMDID_PT_READ_PROXIMITY_VALUE        , "PT_R_PROX_VALUE"         , sndp_comm_cmd_recv_pt_read_proximity_value        },
    { COMM_CMDID_PT_WRITE_PROXIMITY_THRESHOLD   , "PT_W_PROX_THRESHOLD"     , sndp_comm_cmd_recv_pt_write_proximity_thresold    },
#endif
    

    /***** 与APP交互指令 *****/
    { COMM_CMDID_APP_QUERY_DEV_INFO             , "APP_Q_DEV_INFO"      , sndp_comm_cmd_recv_app_query_dev_info                 },
	{ COMM_CMDID_APP_QUERY_DEV_STATUS           , "APP_Q_DEV_STA"       , sndp_comm_cmd_recv_app_query_dev_status               },

};

static const int32_t sndp_comm_cmd_hdlr_cnt = sizeof(sndp_comm_cmd_hdlr_list) / sizeof(sndp_comm_cmd_hdlr_list[0]);
static sndp_comm_cmd_info_s sndp_comm_exec_cmd;
int32_t sndp_comm_execute_cmd_hdlr(sndp_comm_cmd_info_s *cmd)
{
    sndp_comm_cmd_handle_s *cmd_hdlr = NULL;
    uint32_t ret = 0;

    if(cmd == NULL) {
        return -1;
    }

    memcpy(&sndp_comm_exec_cmd, cmd, sizeof(sndp_comm_cmd_info_s));
    
    for(uint32_t i = 0; i < sndp_comm_cmd_hdlr_cnt; i++) {
        cmd_hdlr = (sndp_comm_cmd_handle_s *)&sndp_comm_cmd_hdlr_list[i];
        if(sndp_comm_exec_cmd.cmd_id == cmd_hdlr->cmd_id) {
            if(cmd_hdlr->cmd_exec_hdlr) {
                COMM_CMD_TRACE(1, "cmd name = %s", cmd_hdlr->name ? cmd_hdlr->name : "null");
                ret = cmd_hdlr->cmd_exec_hdlr(&sndp_comm_exec_cmd);
            }
            break;
        }
    }

    return ret;
}

#if defined(__SNDP_SLEEP_APP__)
POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_eq_mode(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        EQ mode set 
        value[0]:
        0x00： normal(default)
        0x01：jazz
        0x02：rock
        0x03：classics
        0x04：relaxed
        0x09：custom mode
    */
    COMM_CMD_TRACE(1, "eq mode=%d", cmd_info->value[0]);
    sndp_dev_sleep_app_set_eq_index(false, cmd_info->value[0], true);
    sndp_comm_cmd_send_lr_sync_eq_set(cmd_info->value[0]);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_eq_mode(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t eq_index = sndp_dev_sleep_app_get_eq_index(false);

    cmd_info->data_len = 0x02;

    cmd_info->value[0] = eq_index;
    
    COMM_CMD_TRACE(1, "eq mode=%d", cmd_info->value[0]);

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_eq_param(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        value[0]~value[7]:
        Q:fix 0.75  0db=0x7F(step 0.1db)
        31Hz Gain     default 0x7F
        62Hz Gain     default 0x7F
        125Hz Gain    default 0x7F
        250Hz Gain    default 0x7F
        1000Hz Gain   default 0x7F
        2000Hz Gain   default 0x7F
        4000Hz Gain   default 0x7F
        8000Hz Gain   default 0x7F

        For example:
        31Hz 5.0db,   0xB1(0.1*50+0x7f)
        62Hz 2.0db,   0x93(0.1*20+0x7f)
        125Hz 1.0ddb, 0x89(0.1*10+0x7f)
        250Hz -4.0db,0x57(0.1*-40+0x7f)
        1000Hz -8.5db, 0x2A
        2000Hz 6.2db, 0xBD
        4000Hz -2.5db, 0x66
        8000Hz -10.0db,0x1B
        (MIX GAIN  -12DB)
        (MAX GAIN  12DB)   
    */

    int8_t freq_gain[8];
    memset(freq_gain, 0, sizeof(freq_gain));
    memcpy(freq_gain, cmd_info->value, cmd_info->data_len-1);
    sndp_set_custom_eq_param(freq_gain);
    memset(cmd_info->value, 0, cmd_info->data_len-1);
    
    cmd_info->data_len = 2;
    cmd_info->value[0] = 0; //success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_eq_param(sleep_app_comm_cmd_info_s *cmd_info)
{
    IIR_CFG_T sleep_iir_cfg;
    int8_t eq_gain = 0;
    sndp_get_custom_eq_param((uint8_t*)&sleep_iir_cfg);
    cmd_info->data_len = 9;
    for(int i=0; i<8; i++)
    {
        eq_gain = (int8_t)sleep_iir_cfg.param[i].gain;
        if(eq_gain > -12 && eq_gain < 12)
        {
            cmd_info->value[i] = (uint8_t)(eq_gain + 0x7F);
        }
        else
        {
            cmd_info->value[i] = 0;
        }
        
    }
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_find_my_earphone(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
    value[0]:
        0x00：start
        0x01：stop
    */
    COMM_CMD_TRACE(0, "findme earphone = %d",cmd_info->value[0]);
    if(cmd_info->value[0]) 
    {
        //stop findme
    }
    else
    {
        sndp_call_func_in_app_thread((uint32_t)sndp_play_findme,0,0,0);
    }

    cmd_info->value[0] = 0;
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_anc_mode(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        value[0]:
            0x00: ANC OFF
            0x01: ANC ON (Strong)
            0x02: ANC ON (Moderate)
            0x03: ANC ON (Mild)
            0x04: ANC ON (Adaptive)
            0x05: Transparent
    */
    uint8_t anc_mode = cmd_info->value[0];
    if(anc_mode == 2 || anc_mode == 3 || anc_mode == 4)
    {
        anc_mode = SNDP_ANC_MODE_1; //only support strong and off
    }
    COMM_CMD_TRACE(1, "anc mode=%d", anc_mode);
    if(sndp_dev_wear_is_worn(false))
    {
        sndp_anc_mode_set_locally((sndp_anc_mode_e)anc_mode);
    }
    sndp_dev_sleep_app_anc_mode_set(false, (sndp_anc_mode_e)anc_mode, true);
    sndp_comm_cmd_send_lr_sync_anc_mode(anc_mode, 1);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_anc_mode(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = sndp_dev_sleep_app_anc_mode_get(false);
    COMM_CMD_TRACE(1, "anc mode=%d", cmd_info->value[0]);

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_ppg_setting(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    /*
     value[0] : 
     0x00: PPG OFF
     0x01: PPG 64Hz  //only support 64HZ
     0x02: PPG 128Hz
     0x03: PPG 256Hz
     */
    if(cmd_info->value[0] == 0x01){
        cmd_info->value[0] = 0x00; //success
    }else{
        cmd_info->value[0] = 0x01;
    }
    
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t  sleep_comm_cmd_recv_ppg_notification(sleep_app_comm_cmd_info_s *cmd_info)
{   
    //if StartHeartrate 0x30 Dump on
    sndp_hr_mearsuring_set_dump_state(0x01);
    sndp_ppg_notification_start();
    COMM_CMD_TRACE(1, "dump=%d", sndp_hr_mearsuring_get_dump_state());
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_sleepapp_set_local_proximity(void)
{
    unsigned short proximity_value = 0;
    sndp_dev_hr_read_proximity_value(&proximity_value);
    sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(proximity_value);
    sndp_dev_sleep_app_set_proximity_data(false, proximity_value);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_proximity_notification(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t get_proximity_onoff = cmd_info->value[0];
    TR_INFO(0, (get_proximity_onoff == 0x01) ? "enable proximity" : "disable proximity");

    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_start_report_proximity);
    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer);
    sndp_dev_sleep_app_set_proximity_onoff(false, get_proximity_onoff);

    if (get_proximity_onoff == 0x01) {
        if (sndp_is_tws_link_connected()) {
            sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(0x01);
        }
        sndp_comm_cmd_sleepapp_start_report_proximity();
    } else {
        if (sndp_is_tws_link_connected()) {
            sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(0x00);
        }
        sndp_comm_cmd_sleepapp_stop_report_proximity();
    }

    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_accelerometer_notification(sleep_app_comm_cmd_info_s *cmd_info)
{
    sndp_hr_mearsuring_set_dump_state(0x01);
    sndp_acc_notification_start();
    COMM_CMD_TRACE(1, "dump=%d", sndp_hr_mearsuring_get_dump_state());
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_battery_status(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        Byte0	Left battery
                0xFF:  not connected
                BIT7:  1 (in the charging case)
                       0 (out of charging case)
                BIT6~0: battery level
        Byte1	Right battery
                0xFF:  not connected
                BIT7:  1 (in the charging case)
                       0 (out of charging case)
                BIT6~0: battery level
        Byte2	Cradle battery
                0xFF:   not connected
                BIT7:   1 (in the charging case)
                        0 (out of charging case)
                BIT6~0: battery level   
    */
    SndpGetBattryMap_t reply_battery;
    memset(&reply_battery, 0, sizeof(SndpGetBattryMap_t));
    
    if(sndp_dev_is_left_earphone())
    {
        reply_battery.bits.left_charging_statu = sndp_dev_charger_is_charging(false);
        reply_battery.bits.left_battery_level = sndp_dev_get_bat_percentage(false);

        if(sndp_is_tws_link_connected()){
            reply_battery.bits.right_charging_statu = sndp_dev_charger_is_charging(true);
            reply_battery.bits.right_battery_level = sndp_dev_get_bat_percentage(true);
        }else{
            reply_battery.charging_byte[1] = 0xff;
        }

        reply_battery.charging_byte[2] = 0xff;
    }

    if(sndp_dev_is_right_earphone())
    {
        reply_battery.bits.right_charging_statu = sndp_dev_charger_is_charging(false);
        reply_battery.bits.right_battery_level = sndp_dev_get_bat_percentage(false);

        if(sndp_is_tws_link_connected()){
            reply_battery.bits.left_charging_statu = sndp_dev_charger_is_charging(true);
            reply_battery.bits.left_battery_level = sndp_dev_get_bat_percentage(true);
        }else{
            reply_battery.charging_byte[1] = 0xff;
        }

        reply_battery.charging_byte[2] = 0xff;        
    }
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_device_info(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t *p = cmd_info->value;
    char *bt_name = (char *)sndp_dev_get_bt_name();
    char *sn = (char *)sndp_dev_get_dev_sn();
    char *hw_ver = (char *)sndp_dev_get_hw_ver(false);
    char *fw_ver = (char *)sndp_dev_get_fw_ver(false);
    char temp_str[20];
    uint8_t len;
        
    memset(cmd_info->value, 0, sizeof(cmd_info->value));

    // BT Name
    {
        len = strlen(bt_name);
        *p++ = len + 1; // Length = 1 (tag) + value_len
        *p++ = DEVICE_INFO_TAG_BT_NAME;
        memcpy(p, bt_name, len);
        p += len;
    }

    // SN
    {
        len = strlen(sn);
        *p++ = len + 1;
        *p++ = DEVICE_INFO_TAG_SN;
        memcpy(p, sn, len);
        p += len;
    }

    // FW Ver
    {
        memset(temp_str, 0, sizeof(temp_str));
        sprintf(temp_str, "%d.%d.%d.%d", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
        len = strlen(temp_str);
        *p++ = len + 1;
        *p++ = DEVICE_INFO_TAG_FW_VER;
        memcpy(p, temp_str, len);
        p += len;
    }

    // HW Ver
    {
        memset(temp_str, 0, sizeof(temp_str));
        sprintf(temp_str, "%d.%d", hw_ver[0], hw_ver[1]);
        len = strlen(temp_str);
        *p++ = len + 1;
        *p++ = DEVICE_INFO_TAG_HW_VER;
        memcpy(p, temp_str, len);
        p += len;
    }

    cmd_info->data_len = p - cmd_info->value + 1; // 加1是因为data_len不包含cmd_id本身

    COMM_CMD_TRACE(0,"data_len:%d", cmd_info->data_len);

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_touch_enable(sleep_app_comm_cmd_info_s *cmd_info)
{

    TR_INFO(0, (cmd_info->value[0]==0x01)?"enable touch":"disable touch");
    sndp_dev_sleep_app_set_gesture_onoff(false, cmd_info->value[0], true);
    
    sndp_comm_cmd_send_lr_sync_gesture_onoff(cmd_info->value[0]);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_voice_prompt_enable(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t prompt_onoff = cmd_info->value[0];
    sndp_dev_sleep_app_set_prompt_onoff(false, (cmd_info->value[0] == 0x01) ? true : false, true);
    sndp_comm_cmd_send_lr_sync_prompt_onoff(prompt_onoff);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_touch_key_mapping(sleep_app_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_GESTURE_MAP__)
    uint8_t lrflag = cmd_info->value[0];
    uint8_t key_behavior = cmd_info->value[1];
    uint8_t key_function = cmd_info->value[2];

    if(lrflag > 2 || key_behavior > 2) {
        cmd_info->value[0] = 0x01; // fail
    } else {
        cmd_info->value[0] = 0x00; // success
    }

    if(lrflag == 0 && sndp_dev_is_left_earphone()){
        sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)key_behavior, (sndp_dev_function_type_t)key_function);
    }else{
        sndp_comm_cmd_send_lr_sync_update_mapping(key_behavior, key_function);
    }

    if(lrflag == 1 && sndp_dev_is_right_earphone()){
        sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)key_behavior, (sndp_dev_function_type_t)key_function);
    }else{
        sndp_comm_cmd_send_lr_sync_update_mapping(key_behavior, key_function);
    }

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
#endif
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_touch_key_mapping(sleep_app_comm_cmd_info_s *cmd_info)
{
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_smart_play_pause(sleep_app_comm_cmd_info_s *cmd_info)
{   
    uint8_t smart_playpause = cmd_info->value[0];
    sndp_dev_sleep_app_set_splaypause_onoff(false, smart_playpause, true);
    sndp_comm_cmd_send_lr_sync_splaypause_onoff(smart_playpause);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_smart_play_pause(sleep_app_comm_cmd_info_s *cmd_info)
{   
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = sndp_dev_sleep_app_get_splaypause_onoff(false);
    COMM_CMD_TRACE(1, "smart_play=%d", cmd_info->value[0]);

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_settings(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        Byte0	Bit Value 
        1: Run command   0: do nothing

            BIT7: Accelerometer ON
            BIT6: Accelerometer OFF
            BIT5: PPG OFF
            BIT4: PPG ON (64Hz)
            BIT3: PPG ON (128Hz)
            BIT2: PPG ON (256Hz)
            BIT1: Proximity ON (keep noti)
            BIT0: Proximity OFF (stop noti)
        Byte1	BIT7: Touch Enable
            BIT6: Touch Disable
            BIT5: VoicePrompt Enable
            BIT4: VoicePrompt Disable
            BIT3: ANC OFF
            BIT2: ANC ON Strong
            BIT1: ANC ON Adaptive
            BIT0: ANC ON Transparent
        Byte2	BIT7: PPGLEDPWR Manual (CMD 0x13 0x00, CMD 0xFF 0x00 0x01 0x01 0x00 0x23 0x2E)
            BIT6: PPGLEDPWR Auto (CMD 0x13 0x01)
            BIT5: Smart PlayPause Enable
            BIT4: Smart PlayPause Disable
            BIT3: Music Play
            BIT2: Music Pause
            BIT1: reserved(0)
            BIT0: reserved(0)
    */
    SndpSettingsBitMap_t BitMap;
    memcpy(&BitMap, cmd_info->value, 3);
   
    {
        //byte0 bit0~bit7 of struct
        /******************proximity map*******************/
        if(BitMap.proximity_off)
        {
            sndp_dev_sleep_app_set_proximity_onoff(false, 0x00);
            if (sndp_is_tws_link_connected()) {
                sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(0x00);
            }
        }
        if(BitMap.proximity_on)
        {
            sndp_dev_sleep_app_set_proximity_onoff(false, 0x01);
            if (sndp_is_tws_link_connected()) {
                sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(0x01);
            }
            sndp_comm_cmd_sleepapp_start_report_proximity();
        }
        /******************proximity map*******************/

        if(BitMap.ppg_256hz)
        {
            //not support
        }else if(BitMap.ppg_128hz)
        {
            //not support
        }else if(BitMap.ppg_64hz)
        {
            //not support
        }else if(BitMap.ppg_off)
        {
            //not support   
        }

        if(BitMap.accel_off)
        {
            //not support
        }

        if(BitMap.accel_on)
        {
            //not support
        }
    }
    
    {
        sndp_anc_mode_e anc_mode = SNDP_ANC_MODE_QTY;
        //Byte2 bit0~bit7 of struct
        /******************anc map*******************/
        if(BitMap.anc_transparent)
        {
            anc_mode = SNDP_ANC_MODE_TRANSPARENT;
        }
        else if(BitMap.anc_adaptive)
        {
            anc_mode = SNDP_ANC_MODE_1;
        }
        else if(BitMap.anc_strong)
        {
            anc_mode = SNDP_ANC_MODE_1;
        }
        else if(BitMap.anc_off)
        {
            anc_mode = SNDP_ANC_MODE_OFF;
        }
        if(sndp_dev_wear_is_worn(false))
        {
            sndp_anc_mode_set_locally((sndp_anc_mode_e)anc_mode);
        }
        sndp_dev_sleep_app_anc_mode_set(false, (sndp_anc_mode_e)anc_mode, true);
        sndp_comm_cmd_send_lr_sync_anc_mode(anc_mode, 1);
        /******************anc map*******************/

        /******************voice prompt map*******************/
        if(BitMap.voice_disable)
        {
            sndp_dev_sleep_app_set_prompt_onoff(false, 0x00, false);
            sndp_comm_cmd_send_lr_sync_prompt_onoff(0x00);
        }
        else if(BitMap.voice_enable)
        {
            sndp_dev_sleep_app_set_prompt_onoff(false, 0x01, false);
            sndp_comm_cmd_send_lr_sync_prompt_onoff(0x01);
        }
        
        /******************touch onoff map*******************/
        if(BitMap.touch_disable)
        {
            sndp_dev_sleep_app_set_gesture_onoff(false, 0x00, false);
            sndp_comm_cmd_send_lr_sync_gesture_onoff(0x00);
        }
        else if(BitMap.touch_enable)
        {
            sndp_dev_sleep_app_set_gesture_onoff(false, 0x01, false);
            sndp_comm_cmd_send_lr_sync_gesture_onoff(0x01);
        }
        /******************touch onoff map*******************/
    }

    {
        //byte3 bit0~bit7 of struct
        if(BitMap.music_pause)
        {
            //not support
        }
        else if(BitMap.music_play)
        {
            //not support
        }

        if(BitMap.smart_playpause_enable)
        {
            sndp_dev_sleep_app_set_splaypause_onoff(false, 0x01, false);
            sndp_comm_cmd_send_lr_sync_splaypause_onoff(0x01);
        }
        else if(BitMap.smart_playpause_disable)
        {
            sndp_dev_sleep_app_set_splaypause_onoff(false, 0x00, false);
            sndp_comm_cmd_send_lr_sync_splaypause_onoff(0x00);
        }

        if(BitMap.ppgledpwr_auto)
        {
            //not support
        }
        else if(BitMap.ppgledpwr_manual)
        {
            //not support
        }

    }
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_settings(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x06;
    cmd_info->value[0] = sndp_dev_sleep_app_anc_mode_get(false);
    cmd_info->value[1] = sndp_hr_mearsuring_get_sampling_rate();
    cmd_info->value[2] = sndp_dev_sleep_app_get_gesture_onoff(false);
    cmd_info->value[3] = sndp_dev_sleep_app_get_prompt_onoff(false);
    cmd_info->value[4] = sndp_dev_sleep_app_get_splaypause_onoff(false);
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_sensor_control(sleep_app_comm_cmd_info_s *cmd_info)
{
        /* receving data format from app:
            BYTE0 Reserve：
                    default 0x00
            BYTE1 Sensor Select
                    0x01=PPG, 0x02=Acc
            BYTE2 Write or Read
                    0x01=Write, 0x02=Read
            BYTE3 Read length（max 0x03）
                    (0x00 = Write only)
            BYTE4 Register Address

            BYTE5 Write Values
                Write only
        */
       /* reply data format to app:
        BYTE0   Receive Status
                0x00= Success, 0x01 = Fail

        BYTE1   Register Address

        BYTE2    Selected Sensor
                 0x01 = PPG, 0x02= Acc
        
        BYTE3   Write or Read
                0x02 = Read (read only)

        BYTE4   Read length (max 0x03)

        BYTE[5~7]	 Register Read Values 
       */
    SndpSensorCtrlMap_t *pSensorCtrlMap = (SndpSensorCtrlMap_t *)cmd_info->value;
    SndpSensorCtrlReplyMap_t pSensorCtrlReplyMap;
    uint8_t read_value[3] = {0};
    uint8_t read_len = 0;
    memset(&pSensorCtrlReplyMap, 0, sizeof(pSensorCtrlReplyMap));
    if(cmd_info->data_len != 7){
        COMM_CMD_TRACE(0, "invalid data_len=%d", cmd_info->data_len);
        cmd_info->data_len = 0x09;
        pSensorCtrlReplyMap.receive_status = 0x01; // fail
        memcpy(cmd_info->value, &pSensorCtrlReplyMap, sizeof(pSensorCtrlReplyMap));
        sndp_sleep_comm_main_rsp_cmd(cmd_info);
        return 0;
    }

    if(pSensorCtrlMap->sensor_select == 0x01) //ppg
    {
        COMM_CMD_TRACE(3, "ppg reg=0x%02x, write_read=%d, read_len=%d, write_value=0x%02x", 
                        pSensorCtrlMap->reg_addr, pSensorCtrlMap->write_read, pSensorCtrlMap->read_lenth, pSensorCtrlMap->write_value);
        if(pSensorCtrlMap->write_read == 0x01) //write
        {
            read_len = 0x01;
            sndp_dev_hr_write_reg(pSensorCtrlMap->reg_addr, pSensorCtrlMap->write_value);
            sndp_dev_hr_read_reg(pSensorCtrlMap->reg_addr, read_value, read_len);
        }
        else if(pSensorCtrlMap->write_read == 0x02) //read
        {
            read_len = pSensorCtrlMap->read_lenth > 3 ? 3 : pSensorCtrlMap->read_lenth;
            sndp_dev_hr_read_reg(pSensorCtrlMap->reg_addr, read_value, read_len);
        }
        else
        {
            COMM_CMD_TRACE(0, "invalid write_read value=%d", pSensorCtrlMap->write_read);
            cmd_info->data_len = 0x09;
            pSensorCtrlReplyMap.receive_status = 0x01; // fail
            memcpy(cmd_info->value, &pSensorCtrlReplyMap, sizeof(pSensorCtrlReplyMap));
            sndp_sleep_comm_main_rsp_cmd(cmd_info);
            return 0;
        }
        COMM_CMD_TRACE(3, "ppg read_value=0x%02x%02x%02x len=%d", read_value[0], read_value[1], read_value[2], read_len);
    }
    else if(pSensorCtrlMap->sensor_select == 0x02) //accel
    {
        COMM_CMD_TRACE(3, "ppg reg=0x%02x, write_read=%d, read_len=%d, write_value=0x%02x", 
                pSensorCtrlMap->reg_addr, pSensorCtrlMap->write_read, pSensorCtrlMap->read_lenth, pSensorCtrlMap->write_value);
        if(pSensorCtrlMap->write_read == 0x01) //write
        {
            read_len = 0x01;
            sndp_dev_acc_write_reg(pSensorCtrlMap->reg_addr, pSensorCtrlMap->write_value);
            sndp_dev_acc_read_reg(pSensorCtrlMap->reg_addr, read_value, read_len);
        }
        else if(pSensorCtrlMap->write_read == 0x02) //read
        {
            read_len = pSensorCtrlMap->read_lenth > 3 ? 3 : pSensorCtrlMap->read_lenth;
            sndp_dev_acc_read_reg(pSensorCtrlMap->reg_addr, read_value, read_len);
        }
        else
        {
            COMM_CMD_TRACE(0, "invalid write_read value=%d", pSensorCtrlMap->write_read);
            cmd_info->data_len = 0x09;
            pSensorCtrlReplyMap.receive_status = 0x01; // fail
            memcpy(cmd_info->value, &pSensorCtrlReplyMap, sizeof(pSensorCtrlReplyMap));
            sndp_sleep_comm_main_rsp_cmd(cmd_info);
            return 0;
        }
        COMM_CMD_TRACE(3, "ppg read_value=0x%02x%02x%02x len=%d", read_value[0], read_value[1], read_value[2], read_len);
    }
    else
    {
        COMM_CMD_TRACE(0, "invalid sensor_select value=%d", pSensorCtrlMap->sensor_select);
        cmd_info->data_len = 0x09;
        pSensorCtrlReplyMap.receive_status = 0x01; // fail
        memcpy(cmd_info->value, &pSensorCtrlReplyMap, sizeof(pSensorCtrlReplyMap));
        sndp_sleep_comm_main_rsp_cmd(cmd_info);
        return 0;
    }

    pSensorCtrlReplyMap.receive_status = 0x00; // success
    pSensorCtrlReplyMap.reg_addr = pSensorCtrlMap->reg_addr;
    pSensorCtrlReplyMap.select_sensor = pSensorCtrlMap->sensor_select;
    pSensorCtrlReplyMap.write_read = pSensorCtrlMap->write_read;
    pSensorCtrlReplyMap.read_lenth = read_len;
    memcpy(pSensorCtrlReplyMap.read_value, read_value, read_len);
    cmd_info->data_len = 0x09;
    memcpy(cmd_info->value, &pSensorCtrlReplyMap, sizeof(pSensorCtrlReplyMap));
    sndp_sleep_comm_main_rsp_cmd(cmd_info);

    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_earbuds_status_led_control(sleep_app_comm_cmd_info_s *cmd_info)
{  
     //Discarded command
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_ppg_auto_led_enable_disable(sleep_app_comm_cmd_info_s *cmd_info)
{   
    //Discarded command
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_start_heartrate(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t sampling_rate = cmd_info->value[0];
    uint8_t dump_data = cmd_info->value[1];
    sndp_hr_mearsuring_set_sampling_rate(sampling_rate);
    sndp_hr_mearsuring_set_dump_state(dump_data);
    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_start, sampling_rate, dump_data, 0);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_stop_heartrate(sleep_app_comm_cmd_info_s *cmd_info)
{
    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_start_sleep(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint32_t sleep_mode = cmd_info->value[0]<<24 | cmd_info->value[1]<<16 | cmd_info->value[2]<<8 | cmd_info->value[3];
    DUMP8("0x%02x ", cmd_info->value, 4);
    COMM_CMD_TRACE(1, "sleep mode=%d", sleep_mode);
    sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_start, sleep_mode, 0, 0);
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_sleep_tracking(sleep_app_comm_cmd_info_s *cmd_info)
{
    sndp_comm_sleepapp_report_sleep_tracking = true;
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_stop_sleep(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_stop, 0, 0, 0);
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_wear_state(sleep_app_comm_cmd_info_s *cmd_info)
{
    wear_state_update_onoff = cmd_info->value[0];
    COMM_CMD_TRACE(1,"wear_state_update_onoff:%d", wear_state_update_onoff);
    if(wear_state_update_onoff == 0)
    {
        sndp_dev_sleep_app_clean_wear_cnt();
    }

    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_sleep_stage(int8_t *sleep_stage,
                                                    uint16_t position_and_control,
                                                    int16_t result_code)
{
    /*
        Byte0	Sleep Stage … 40
    -----------------------------------
        Byte40	Sleep Position 
    -----------------------------------
        Byte41	Sound Control
        Byte42
    ------------------------------------
        Byte43	Result Code 
    */
   if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0, "BLE is not connected, stop sleep stage reporting");
        sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_stop, 0, 0, 0);
        return 1;
    }
    if(sndp_comm_sleepapp_report_sleep_tracking){
        uint8_t sendvalue[44];
        uint8_t sendlen = 0;
        memset(sendvalue, 0, sizeof(sendvalue));
        memcpy(sendvalue, sleep_stage, 40);
        sendlen += 40;
        sendvalue[sendlen] = (uint8_t)((position_and_control >> 8) & 0xFF);
        sendlen++;
        sendvalue[sendlen] = position_and_control & 0xFF;
        sendlen++;
        sendvalue[sendlen] = (uint8_t)((result_code >> 8) & 0xFF);
        sendlen++;
        sendvalue[sendlen] = (uint8_t)(result_code & 0xFF);
        sendlen++;
        sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_SLEEP_TRACKING, sendlen+1, sendvalue);
        sndp_comm_sleepapp_report_sleep_tracking = false;
    }
    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_hr(uint8_t* sendhr, uint8_t sendhrcount, uint8_t resulcode)
{	
    /*
        Byte0   HR
        Byte1	
    ------------------------
        Byte2   SDNN
        Byte3	
    ------------------------
        Byte4   coherence
        Byte5
        Byte6	
    ------------------------
        Byte7	HR count
    ------------------------
        Byte8   Result Code
        Byte9	
    */

    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0, "BLE is not connected, stop hr");
        sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
        return 1;
    }

    uint8_t sendvalue[16];
    uint8_t sendlen = 0;
    memset(sendvalue, 0, sizeof(sendvalue));
#if defined(__SNDP_HR_ALGO__)
    memcpy(sendvalue, sendhr, sizeof(HrvIndices));
    sendlen += sizeof(HrvIndices);
#else
    memcpy(sendvalue, "Noalgo", sizeof("Noalgo"));
    sendlen += sizeof("Noalgo");
#endif
    sendvalue[sendlen] = sendhrcount;
    sendlen++;
    sendvalue[sendlen] = resulcode;
    sendlen++;

    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_HEARTRATE_MEASURING, sendlen+1, sendvalue);
	return 0;
}

static uint8_t* pack_ppg_data(uint8_t* notify_data, uint16_t ppg_raw_data_len, int32_t* ppg_raw_data) {
    uint8_t* p = notify_data;
    
    // 1. 打包LR_flag 1字节
    if(sndp_dev_is_left_earphone())
        *p++ = 0x01;
    else
        *p++ = 0x02;

    // 2. 打包ppg_raw_data_len（1字节）
    *p++ = (uint8_t)ppg_raw_data_len;           // 低字节

    // 3. 打包32个int32的低3字节 32*3=96字节
    for (int i = 0; i < ppg_raw_data_len; i++) {
        *p++ = (uint8_t)(ppg_raw_data[i] & 0xFF);         // 最低字节
        *p++ = (uint8_t)((ppg_raw_data[i] >> 8) & 0xFF);  // 中间字节
        *p++ = (uint8_t)((ppg_raw_data[i] >> 16) & 0xFF); // 最高字节（低3字节中的）
    }
    
    return notify_data;  // 返回缓冲区起始位置
}

uint32_t sndp_comm_cmd_sleepapp_report_ppg_ntf(int32_t *ppg_raw_data, uint16_t ppg_raw_len)
{
    if(!sndp_comm_ble_is_connected()){
        COMM_CMD_TRACE(0, "ble is not connected, not report ppg");
        return 0;
    }

    if(sndp_hr_mearsuring_get_dump_state() == 0){
        COMM_CMD_TRACE(0, "dump state is off, not report ppg");
        return 0;
    }
    uint16_t ppg_samples_len = ppg_raw_len > 32 ? 32 : ppg_raw_len; // 最多打包32个数据点
    pack_ppg_data(ppg_notify_data, ppg_samples_len, ppg_raw_data);

    // COMM_CMD_TRACE(1, "report ppg to app, len=%d", ppg_samples_len);
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_PPG_NOTIFICATION, ppg_samples_len*3+1+1+1, (uint8_t *)ppg_notify_data);

    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf(int16_t *acc_raw_data, uint16_t acc_raw_len)
{
    if(!sndp_comm_ble_is_connected()){
        COMM_CMD_TRACE(0, "ble is not connected, not report accelerometer");
        return 0;
    }

    if(sndp_hr_mearsuring_get_dump_state() == 0){
        COMM_CMD_TRACE(0, "dump state is off, not report accelerometer");
        return 0;
    }

    // COMM_CMD_TRACE(1, "report accelerometer to app, len=%d", acc_raw_len);
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION, acc_raw_len*sizeof(int16_t)+1, (uint8_t *)acc_raw_data);

    return 0;
}

static uint32_t sndp_comm_cmd_sleepapp_report_proximity_to_app(void)
{
    unsigned short proximity_value_local = 0;
    unsigned short proximity_value_peer = 0;
    uint8_t sendvalue[5] = {0};

    sndp_dev_hr_read_proximity_value(&proximity_value_local);
    sndp_dev_sleep_app_set_proximity_data(false, proximity_value_local);
    proximity_value_peer = sndp_dev_sleep_app_get_proximity_data(true);
    COMM_CMD_TRACE(2, "local prox=%d, peer prox=%d", proximity_value_local, proximity_value_peer);
    /*
        Byte0	Master Source
                0x01:   Left
                0x02:   Right
    --------------------------------------
        Byte1
        Byte2	Proximity Value (Left)
    --------------------------------------
        Byte3
        Byte4	Proximity Value (Right)
    */
    if (sndp_dev_is_left_earphone()) {
        sendvalue[0] = 0x01;
        sendvalue[1] = (uint8_t)(proximity_value_local & 0xFF);
        sendvalue[2] = (uint8_t)((proximity_value_local >> 8) & 0xFF);
        sendvalue[3] = (uint8_t)(proximity_value_peer & 0xFF);
        sendvalue[4] = (uint8_t)((proximity_value_peer >> 8) & 0xFF);
    } else {
        sendvalue[0] = 0x02;
        sendvalue[1] = (uint8_t)(proximity_value_peer & 0xFF);
        sendvalue[2] = (uint8_t)((proximity_value_peer >> 8) & 0xFF);
        sendvalue[3] = (uint8_t)(proximity_value_local & 0xFF);
        sendvalue[4] = (uint8_t)((proximity_value_local >> 8) & 0xFF);
    }

    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION, sizeof(sendvalue)+1, sendvalue);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_sleepapp_send_local_proximity_to_peer(void)
{
    unsigned short proximity_value_local = 0;
    sndp_dev_hr_read_proximity_value(&proximity_value_local);
    sndp_dev_sleep_app_set_proximity_data(false, proximity_value_local);
    sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(proximity_value_local);

    if (sndp_dev_sleep_app_get_proximity_onoff(false) && sndp_comm_proximity_slave_timer_running) {
        sndp_delay_exec_start(sndp_comm_proximity_report_interval_ms,
                              (uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer,
                              0, 0, 0);
    }
    return 0;
}

static uint32_t sndp_comm_cmd_sleepapp_start_report_proximity(void)
{
    // if (!sndp_is_tws_master_mode() && sndp_is_tws_link_connected()) {
    //     COMM_CMD_TRACE(0, "not in master mode, skip reporting proximity");
    //     return 0;
    // }

    if (!sndp_dev_sleep_app_get_proximity_onoff(false)) {
        return 0;
    }

    if(!sndp_comm_ble_is_connected()){
        return 0;
    }
    sndp_comm_cmd_sleepapp_report_proximity_to_app();
    sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(sndp_dev_sleep_app_get_proximity_data(false));

    sndp_delay_exec_start(sndp_comm_proximity_report_interval_ms,
                          (uint32_t)sndp_comm_cmd_sleepapp_start_report_proximity,
                          0, 0, 0);
    return 0;
}

static uint32_t sndp_comm_cmd_sleepapp_stop_report_proximity(void)
{
    if (!sndp_is_tws_master_mode()) { 
        return 0;
    }

    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_start_report_proximity);
    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer);
    sndp_comm_proximity_slave_timer_running = false;
    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_proximity_role_switch_update(void)
{
    if (!sndp_dev_sleep_app_get_proximity_onoff(false)) {
        return 0;
    }

    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_start_report_proximity);
    sndp_delay_exec_stop((uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer);
    sndp_comm_proximity_slave_timer_running = false;

    if (sndp_is_tws_master_mode()) {
        sndp_comm_cmd_sleepapp_start_report_proximity();
    } else if (sndp_is_tws_slave_mode()) {
        sndp_delay_exec_start(sndp_comm_proximity_report_interval_ms,
                              (uint32_t)sndp_comm_cmd_sleepapp_send_local_proximity_to_peer,
                              0, 0, 0);
        sndp_comm_proximity_slave_timer_running = true;
    }

    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_wear_state_update(uint8_t lR_flag, uint8_t wear_state)
{
    /*
        Byte0  left/right
        Byte1  wear state
        Byte2
        Byte3  wear on cnt
        Byte4  
        Byte5  wear off cnt
    */
    uint16_t wear_onoff_cnt[SNDP_DEV_WEAR_CNT_MAX] = {0};
    uint8_t sendvalue[6] = {0};

    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0,"ble not connected,wear state update no use......");
        return 0;
    }

    if(wear_state_update_onoff == 0)
    {
        COMM_CMD_TRACE(0,"wear_state_update_onoff is 0");
        return 0;        
    }
    
    sndp_dev_sleep_app_wear_cnt(lR_flag, wear_state);
    memcpy(wear_onoff_cnt, sndp_dev_sleep_app_get_wear_cnt(), sizeof(wear_onoff_cnt));
    COMM_CMD_TRACE(0,"wear cnt:%d, %d, %d, %d", wear_onoff_cnt[0], wear_onoff_cnt[1],wear_onoff_cnt[2],wear_onoff_cnt[3]);
    sendvalue[0] = lR_flag;
    sendvalue[1] = wear_state;
    if(lR_flag == SNDP_DEV_EARSIDE_LEFT){
        sendvalue[2] = wear_onoff_cnt[SNDP_DEV_LEFT_WEAR_CNT] & 0xff;
        sendvalue[3] = (wear_onoff_cnt[SNDP_DEV_LEFT_WEAR_CNT] >> 8) & 0xff;
        sendvalue[4] = wear_onoff_cnt[SNDP_DEV_LEFT_UNWEAR_CNT] & 0xff;
        sendvalue[5] = (wear_onoff_cnt[SNDP_DEV_LEFT_UNWEAR_CNT] >> 8) & 0xff;
    }else{
        sendvalue[2] = wear_onoff_cnt[SNDP_DEV_RIGHT_WEAR_CNT] & 0xff;
        sendvalue[3] = (wear_onoff_cnt[SNDP_DEV_RIGHT_WEAR_CNT] >> 8) & 0xff;
        sendvalue[4] = wear_onoff_cnt[SNDP_DEV_RIGHT_UNWEAR_CNT] & 0xff;
        sendvalue[5] = (wear_onoff_cnt[SNDP_DEV_RIGHT_UNWEAR_CNT] >> 8) & 0xff;
    }
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_WEAR_STATE_UPDATE, sizeof(sendvalue)+1, sendvalue);
    return 0;
}
/*
BLE packet format:
Flag  |  Parameter  |  Length	|  Cmd	       |       Data
3BYTE |  1BYTE	    |  1BYTE	|   N          |        BYTE
0x57  |  0x4D 0x43	|  CMD and  |  data length |   Command	NULL allowed，but no more than 240 bytes
*/
static const sndp_sleep_comm_cmd_handle_s sleep_app_comm_cmd_hdlr_list[] = {
    { SLEEP_APP_CMDID_SET_EQ_MODE,                    "APP_SET_EQ_MODE",                    sleep_comm_cmd_recv_app_set_eq_mode },
    { SLEEP_APP_CMDID_GET_EQ_MODE,                    "APP_GET_EQ_MODE",                    sleep_comm_cmd_recv_app_get_eq_mode },
    { SLEEP_APP_CMDID_SET_EQ_PARAM,                   "APP_SET_EQ_PARAM",                   sleep_comm_cmd_recv_app_set_eq_param },
    { SLEEP_APP_CMDID_GET_EQ_PARAM,                   "APP_GET_EQ_PARAM",                   sleep_comm_cmd_recv_app_get_eq_param },
    { SLEEP_APP_CMDID_FIND_MY_EARPHONE,               "APP_FIND_MY_EARPHONE",               sleep_comm_cmd_recv_app_find_my_earphone },
    { SLEEP_APP_CMDID_SET_ANC_MODE,                   "APP_SET_ANC_MODE",                   sleep_comm_cmd_recv_app_set_anc_mode },
    { SLEEP_APP_CMDID_GET_ANC_MODE,                   "APP_GET_ANC_MODE",                   sleep_comm_cmd_recv_app_get_anc_mode },
    { SLEEP_APP_CMDID_PPG_SETING,                     "APP_SET_PPG_SETTING",                sleep_comm_cmd_recv_app_set_ppg_setting },
    { SLEEP_APP_CMDID_PPG_NOTIFICATION,               "APP_PPG_NOTIFICATION",               sleep_comm_cmd_recv_ppg_notification },
    { SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION,     "APP_GET_PROXIMITY_NOTIFICATION",     sleep_comm_cmd_recv_app_get_proximity_notification },
    { SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION, "APP_GET_ACCELEROMETER_NOTIFICATION", sleep_comm_cmd_recv_app_get_accelerometer_notification },
    { SLEEP_APP_CMDID_GET_BATTERY_STATUS,             "APP_GET_BATTERY_STATUS",             sleep_comm_cmd_recv_app_get_battery_status },
    { SLEEP_APP_CMDID_GET_DEVICE_INFO,                "APP_GET_DEVICE_INFO",                sleep_comm_cmd_recv_app_get_device_info },
    { SLEEP_APP_CMDID_SET_TOUCH_ENABLE,               "APP_SET_TOUCH_ENABLE",               sleep_comm_cmd_recv_app_set_touch_enable },
    { SLEEP_APP_CMDID_SET_VOICE_PROMPT_ENABLE,        "APP_SET_VOICE_PROMPT_ENABLE",        sleep_comm_cmd_recv_app_set_voice_prompt_enable },
    { SLEEP_APP_CMDID_SET_TOUCH_KEY_MAPPING,          "APP_SET_TOUCH_KEY_MAPPING",          sleep_comm_cmd_recv_app_set_touch_key_mapping },
    { SLEEP_APP_CMDID_GET_TOUCH_KEY_MAPPING,          "APP_GET_TOUCH_KEY_MAPPING",          sleep_comm_cmd_recv_app_get_touch_key_mapping },
    { SLEEP_APP_CMDID_SET_SMART_PLAY_PAUSE,           "APP_SET_SMART_PLAY_PAUSE",           sleep_comm_cmd_recv_app_set_smart_play_pause },
    { SLEEP_APP_CMDID_GET_SMART_PLAY_PAUSE,           "APP_GET_SMART_PLAY_PAUSE",           sleep_comm_cmd_recv_app_get_smart_play_pause },
    { SLEEP_APP_CMDID_SET_SETTINGS,                   "APP_SET_SETTINGS",                   sleep_comm_cmd_recv_app_set_settings },
    { SLEEP_APP_CMDID_GET_SETTINGS,                   "APP_GET_SETTINGS",                   sleep_comm_cmd_recv_app_get_settings },
    { SLEEP_APP_CMDID_SENSOR_CONTROL,                 "APP_SENSOR_CONTROL",                 sleep_comm_cmd_recv_app_sensor_control },
    { SLEEP_APP_CMDID_EARBUDS_STATUS_LED,             "APP_EARBUDS_STATUS_LED_CONTROL",     sleep_comm_cmd_recv_app_earbuds_status_led_control },
    { SLEEP_APP_CMDID_PPG_AUTO_LED_ENABLE_DISABLE,    "APP_PPG_AUTO_LED_ENABLE_DISABLE",    sleep_comm_cmd_recv_app_ppg_auto_led_enable_disable },
    { SLEEP_APP_CMDID_START_HEARTRATE,                "APP_START_HEARTRATE",                sleep_comm_cmd_recv_app_start_heartrate },
    { SLEEP_APP_CMDID_STOP_HEARTRATE,                 "APP_STOP_HEARTRATE",                 sleep_comm_cmd_recv_app_stop_heartrate },
    { SLEEP_APP_CMDID_START_SLEEP,                    "APP_START_SLEEP",                    sleep_comm_cmd_recv_app_start_sleep },
    { SLEEP_APP_CMDID_SLEEP_TRACKING,                 "APP_SLEEP_TRACKING",                 sleep_comm_cmd_recv_app_sleep_tracking },
    { SLEEP_APP_CMDID_STOP_SLEEP,                     "APP_STOP_SLEEP",                     sleep_comm_cmd_recv_app_stop_sleep },
    { SLEEP_APP_CMDID_WEAR_STATE_UPDATE,              "APP_WEAR_STATE",                     sleep_comm_cmd_recv_app_wear_state },
};
static const int32_t sndp_sleep_app_comm_cmd_hdlr_cnt = sizeof(sleep_app_comm_cmd_hdlr_list) / sizeof(sleep_app_comm_cmd_hdlr_list[0]);
static sleep_app_comm_cmd_info_s sndp_sleep_app_comm_exec_cmd;
int32_t sleep_comm_execute_cmd_hdlr(sleep_app_comm_cmd_info_s *cmd)
{
    sndp_sleep_comm_cmd_handle_s *cmd_hdlr = NULL;
    uint32_t ret = 0;

    if(cmd == NULL) {
        return -1;
    }

    memcpy(&sndp_sleep_app_comm_exec_cmd, cmd, sizeof(sleep_app_comm_cmd_info_s));
    for(uint32_t i = 0; i < sndp_sleep_app_comm_cmd_hdlr_cnt; i++) {
        cmd_hdlr = (sndp_sleep_comm_cmd_handle_s *)&sleep_app_comm_cmd_hdlr_list[i];
        if(sndp_sleep_app_comm_exec_cmd.cmd == cmd_hdlr->cmd_id) {
            if(cmd_hdlr->cmd_exec_hdlr) {
                COMM_CMD_TRACE(1, "cmd name = %s", cmd_hdlr->name ? cmd_hdlr->name : "null");
                ret = cmd_hdlr->cmd_exec_hdlr(&sndp_sleep_app_comm_exec_cmd);
            }
            break;
        }
    }
    return ret;
}
#endif
#endif	/* __SNDP_COMM_CMD_DEFAULT__ */


