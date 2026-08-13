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
#include "nvrecord_extension.h"
#include "nvrecord_env.h"
#include "factory_section.h"
#include "app_media_player.h"
#include "app_bt_stream.h"
#include "app_audio.h"
#include "app_anc.h"
#include "iir_process.h"
#include "app_tws_ibrt.h"
#include "apps.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"
#include "sndp_if_data_access.h"

#include "sndp_ui.h"

#include "sndp_comm_protocol.h"
#include "sndp_comm_main.h"
#include "sndp_comm_cmd.h"
#include "sndp_comm_ble.h"

#if defined(__SNDP_PRODUCT_TEST__)
#include "sndp_product_test.h"
#define COMM_CMDID_IS_PT_CMD(id)  ((id) >= COMM_CMDID_PT_SWITCH_TEST_MODE && (id) <= COMM_CMDID_PT_CLICK_TEST_REPORT)
#endif

#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)
#include "sndp_hal_cover_switch.h"
#include "sndp_cover_switch_box_notify.h"
#endif

#if defined(__SNDP_HEART_RATE_MGR__)
#include "sndp_heart_rate.h"
#endif

#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)
#include "sndp_sleep_role_switch.h"
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

        if(!sndp_is_tws_link_connected()) {
            sndp_call_func_in_app_thread((uint32_t)sndp_enter_freeman_pairing, 0, 0, 0);
        } else {
            sndp_call_func_in_app_thread((uint32_t)sndp_tws_enter_mobile_pairing_after_mobile_disconnect, 0, 0, 0);
        }

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

	if(cmd_info->data_len == 6) {

        memcpy(fw_ver, &cmd_info->data[0], 3);
        bat_info.bat_volt = (uint16_t)((cmd_info->data[3]<<8)|cmd_info->data[4]);
        if(bat_info.bat_per > 0x64){
            err_code = SNDP_COMM_ERROR_BOX_BATPER_ERR;
            bat_info.bat_per = 0x64;
        }else{
            bat_info.bat_per = cmd_info->data[5];
        }
        sndp_dev_set_box_fw_ver(fw_ver);
        sndp_dev_set_box_bat_info(bat_info);
	} else {
		err_code = SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

    COMM_CMD_TRACE(1, "data_len=%d percent=%d", cmd_info->data_len,bat_info.bat_per);

    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);

    // set shutdown reset flag            
    if(sndp_get_is_shutting_down() && sndp_get_shutdown_reason_is_charging_full()) {
        sndp_set_shutdown_reset_flag(1);
        COMM_CMD_TRACE(0, "set shutdown reset flag 1"); 
    }   
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

uint32_t sndp_comm_cmd_send_lr_sync_mobile_connected(void)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_MOBILE_CONNECTED, NULL, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_both_shutdown(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_app_shutdown(SNDP_SHUTDOWN_REASON_BOTH_SHUTDOWN);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_mobile_connected(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_call_func_in_app_thread((uint32_t)sndp_mobile_pairing_sccessful, 0, 0, 0);
	sndp_call_func_in_app_thread((uint32_t)sndp_mobile_reconnect_sccessful, 0, 0, 0);
	return 0;
}



uint32_t sndp_comm_cmd_send_lr_sync_disconnect_and_tws_pair(void)
{
	sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_DISCONNECT_AND_TWS_PAIR, NULL, 0);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_disconnect_and_tws_pair(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_call_func_in_app_thread((uint32_t)sndp_tws_enter_mobile_pairing_after_mobile_disconnect, 0, 0, 0);
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
uint32_t sndp_comm_cmd_send_lr_sync_sleep_app_flag(SNDP_SLEEP_APP_FLAG_NAME flag_name, uint8_t value, bool is_save)
{
    uint8_t data[3] = {(uint8_t)flag_name, value, (uint8_t)is_save};
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_SLEEP_APP_FLAG, data, sizeof(data));
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_sleep_app_flag(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 3) {
        SNDP_SLEEP_APP_FLAG_NAME flag_name = (SNDP_SLEEP_APP_FLAG_NAME)cmd_info->data[0];
        uint8_t value = cmd_info->data[1];
        bool is_save = (bool)cmd_info->data[2];

        switch(flag_name) {
        case SNDP_EQ_INDEX_FLAG:
            sndp_dev_sleep_app_set_eq_index(false, value, is_save);
            break;
        case SNDP_ANC_MODE_FLAG:
            sndp_dev_sleep_app_anc_mode_set(false, value, is_save);
            sndp_sleep_app_report_anc_mode();
            break;
        case SNDP_PROMPT_ONOFF_FLAG:
            sndp_dev_sleep_app_set_prompt_onoff(false, value, is_save);
            break;
        case SNDP_GESTURE_ONOFF_FLAG:
            sndp_dev_sleep_app_set_gesture_onoff(false, value, is_save);
            break;
        case SNDP_SPLAYPAUSE_ONOFF_FLAG:
            sndp_dev_sleep_app_set_splaypause_onoff(false, value, is_save);
            break;
        case SNDP_SLEEP_MODE_FLAG:
            sndp_dev_set_working_mode((sndp_dev_working_mode_e)value);
            break;
        default:
            break;
        }
    }
    return 0;
}

#if defined(__SNDP_HEART_RATE_MGR__)
uint32_t sndp_comm_cmd_send_lr_sync_heart_rate_onoff(uint8_t onoff)
{
    uint8_t data[3] = {0};
    data[0] = onoff;
    data[1] = sndp_hr_mearsuring_get_sampling_rate();
    data[2] = sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE);
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_HEARTRATE_ONOFF, data, 3);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_heart_rate_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 3) {
        sndp_dev_sleep_app_set_heartrate_onoff(false, cmd_info->data[0]);
        sndp_hr_mearsuring_set_sampling_rate(cmd_info->data[1]);
        sndp_hr_mearsuring_set_dump_state(HR_DUMP_STATE, cmd_info->data[2]);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_stage_onoff(uint8_t onoff)
{
    uint8_t data[5] = {0};
    uint32_t sleep_control = sndp_hr_get_sleep_control();
    data[0] = onoff;
    data[1] = (sleep_control>>24) & 0xFF;
    data[2] = (sleep_control>>16) & 0xFF;
    data[3] = (sleep_control>>8) & 0xFF;
    data[4] = sleep_control & 0xFF;

    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_STAGE_ONOFF, data, 5);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_stage_onoff(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 5) {
        sndp_dev_sleep_app_set_stage_onoff(false, cmd_info->data[0]);
        sndp_hr_set_sleep_control((cmd_info->data[1]<<24) | (cmd_info->data[2]<<16) | (cmd_info->data[3]<<8) | cmd_info->data[4]);
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

        sndp_comm_cmd_sleepapp_report_proximity_to_app();
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
        sndp_hr_proximity_tick_enable(true);
        sndp_comm_cmd_sleepapp_proximity_task();    // 立即执行一次, 与主耳流程一致
    } else {
        sndp_hr_proximity_tick_enable(false);
    }

    return 0;
}
uint32_t sndp_comm_cmd_send_lr_sync_start_sleep(uint32_t sleep_control)
{
    uint8_t data[4];
    data[0] = (sleep_control >> 24) & 0xFF;
    data[1] = (sleep_control >> 16) & 0xFF;
    data[2] = (sleep_control >> 8) & 0xFF;
    data[3] = sleep_control & 0xFF;
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_START_SLEEP, data, sizeof(data));
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_start_sleep(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 4) {
        uint32_t sleep_control = cmd_info->data[0]<<24 | cmd_info->data[1]<<16 | cmd_info->data[2]<<8 | cmd_info->data[3];
        COMM_CMD_TRACE(1, "LR sync start sleep, control=%d", sleep_control);
        /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
        sndp_dev_sleep_app_set_stage_onoff(false, 0x01);
        sndp_dev_set_working_mode(SNDP_DEV_WORKING_MODE_SLEEP);
        sndp_hr_set_sleep_control((int32_t)sleep_control);
        sndp_ui_sleep_anc_mode_on();
        sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_start, 0, 0, 0);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_sleep_tracking(uint8_t *data, uint16_t data_len)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_SLEEP_TRACKING, data, data_len);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_sleep_tracking(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len >= 211) {
        int16_t *accel_data_m = (int16_t*)&cmd_info->data[0];
        uint8_t *screen_status = &cmd_info->data[180];
        uint8_t sound_state = cmd_info->data[210];
        sndp_dbbeats_put_sleep_app_data(accel_data_m, screen_status, sound_state);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_stop_sleep(void)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_STOP_SLEEP, NULL, 0);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_stop_sleep(sndp_comm_cmd_info_s *cmd_info)
{
    COMM_CMD_TRACE(0, "LR sync stop sleep");
    /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
    sndp_dev_sleep_app_set_stage_onoff(false, 0x00);
    sndp_dev_set_working_mode(SNDP_DEV_WORKING_MODE_BT);
    sndp_ui_sleep_anc_mode_off();
    sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_stop, 0, 0, 0);
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_start_heartrate(uint8_t sampling_rate, uint8_t dump_data)
{
    uint8_t data[2] = {sampling_rate, dump_data};
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_START_HEARTRATE, data, sizeof(data));
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_start_heartrate(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 2) {
        uint8_t sampling_rate = cmd_info->data[0];
        uint8_t dump_data = cmd_info->data[1];
        COMM_CMD_TRACE(1, "LR sync start heartrate, sampling=%d dump=%d", sampling_rate, dump_data);
        /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
        sndp_dev_sleep_app_set_heartrate_onoff(false, 0x01);
        sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_start, sampling_rate, dump_data, 0);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_stop_heartrate(void)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_STOP_HEARTRATE, NULL, 0);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_stop_heartrate(sndp_comm_cmd_info_s *cmd_info)
{
    COMM_CMD_TRACE(0, "LR sync stop heartrate");
    /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
    sndp_dev_sleep_app_set_heartrate_onoff(false, 0x00);
    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_ppg_notification(uint8_t onoff)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_PPG_NOTIFICATION, &onoff, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_ppg_notification(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
        uint8_t onoff = cmd_info->data[0];
        COMM_CMD_TRACE(1, "LR sync ppg notification, onoff=%d", onoff);
        /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
        if(onoff) {
            sndp_ppg_notification_start(0x01);
        } else {
            sndp_ppg_notification_stop();
        }
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_acc_notification(uint8_t onoff)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_ACC_NOTIFICATION, &onoff, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_acc_notification(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
        uint8_t onoff = cmd_info->data[0];
        COMM_CMD_TRACE(1, "LR sync acc notification, onoff=%d", onoff);
        /* 直接调用底层逻辑，不通过sync包装避免回环同步 */
        if(onoff) {
            sndp_acc_notification_start(0x01);
        } else {
            sndp_acc_notification_stop();
        }
    }
    return 0;
}
#endif

#if defined(__SNDP_GESTURE_MAP__)
uint32_t sndp_comm_cmd_send_lr_sync_update_mapping(gesture_map_t* mapping, uint16_t data_len)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_UPDATE_MAPPING, (uint8_t*)mapping, data_len);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_update_mapping(sndp_comm_cmd_info_s *cmd_info)
{
    for(int i = 0; i<SNDP_DEV_GESTURE_MAX*2; i++) {
        if(cmd_info->data[3*i] == 0) {
            if(sndp_dev_is_left_earphone()){
                sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)cmd_info->data[3*i+1], (sndp_dev_function_type_t)cmd_info->data[3*i+2]);
            }else{
                sndp_dev_gesture_mapper_update_mapping(true, (sndp_dev_gesture_type_t)cmd_info->data[3*i+1], (sndp_dev_function_type_t)cmd_info->data[3*i+2]);
            }
        }
        if(cmd_info->data[3*i] == 1) {
            if(sndp_dev_is_left_earphone()){
                sndp_dev_gesture_mapper_update_mapping(true, (sndp_dev_gesture_type_t)cmd_info->data[3*i+1], (sndp_dev_function_type_t)cmd_info->data[3*i+2]);
            }else{
                sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)cmd_info->data[3*i+1], (sndp_dev_function_type_t)cmd_info->data[3*i+2]);
            }
        }
    }
    return 0;
}
#endif

#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)
uint32_t sndp_comm_cmd_send_lr_sync_sleep_snapshot(uint8_t *data, uint16_t data_len)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_SLEEP_SNAPSHOT, data, data_len);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_sleep_snapshot(sndp_comm_cmd_info_s *cmd_info)
{
    COMM_CMD_TRACE(0, "recv sleep snapshot, len=%d", cmd_info->data_len);
    if (cmd_info->data_len > 0) {
        sndp_sleep_role_switch_recv_snapshot(cmd_info->data, cmd_info->data_len);
    }
    return 0;
}

uint32_t sndp_comm_cmd_send_lr_sync_sleep_role_status(uint8_t status)
{
    sndp_comm_cmd_send_cmd_to_peer(COMM_CMDID_LR_SYNC_SLEEP_ROLE_STATUS, &status, 1);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_lr_sync_sleep_role_status(sndp_comm_cmd_info_s *cmd_info)
{
    if(cmd_info->data_len == 1) {
        sndp_sleep_role_set_peer((Device_Role_t)cmd_info->data[0]);
    }
    return 0;
}
#endif
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
        if(cmd_info->data[0] == 0) {
            sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
            sndp_call_func_in_app_thread((uint32_t)sndp_app_reboot, 0, 0, 0);
            return 0;
        }
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
    uint8_t path = cmd_info->path;
    COMM_CMD_TRACE(0, " path=%d", path);
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
	if(sndp_is_tws_link_connected()) {
        if(path == SNDP_COMM_PATH_SPP) {
            if(sndp_is_tws_master_mode()) {
                sndp_call_func_in_app_thread((uint32_t)sndp_tws_enter_mobile_pairing_after_mobile_disconnect, 0, 0, 0);
            }
        } else {
            sndp_call_func_in_app_thread((uint32_t)sndp_tws_enter_mobile_pairing_after_mobile_disconnect, 0, 0, 0);
        }
    } else {
        sndp_call_func_in_app_thread((uint32_t)sndp_enter_freeman_pairing, 0, 0, 0);
    }

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
	cmd_info->data[cmd_info->data_len++] = fw_ver[1];
	cmd_info->data[cmd_info->data_len++] = fw_ver[2];
	cmd_info->data[cmd_info->data_len++] = fw_ver[3];
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

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_read_dev_sn(sndp_comm_cmd_info_s *cmd_info)
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

static uint32_t sndp_comm_cmd_recv_pt_read_bt_addr(sndp_comm_cmd_info_s *cmd_info)
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
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    
    if(cmd_info->data_len == 6) {
        if(factory_section_set_bt_address(&cmd_info->data[0]) != 0) {
            err_code = SNDP_COMM_ERROR_SAVE_FAIL;
        }
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
	return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_ble_addr(sndp_comm_cmd_info_s *cmd_info)
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

static uint32_t sndp_comm_cmd_recv_pt_read_frequency_offset(sndp_comm_cmd_info_s *cmd_info)
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

#if defined(__SNDP_GSENSOR_SUPPORT__)
POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_test_gsensor(sndp_comm_cmd_info_s *cmd_info)
{  
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint8_t op_code;
    uint8_t chip_id;
    sndp_hal_acc_data_s acc_data = {0};
    uint8_t send_data[10];
    uint8_t send_len = 0;

    if(cmd_info->data_len > 0) {
        op_code = cmd_info->data[0];
        if(op_code == 0x01) {   //read chip id
            if(sndp_hal_acc_read_chip_id(&chip_id) == 0) {
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = chip_id;
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
            
        } else if(op_code == 0x02) {   //read raw data
            if(sndp_hal_acc_read_raw_data(&acc_data) == 0) {            
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = (uint8_t)((acc_data.x>>8)&0xff);
                send_data[send_len++] = (uint8_t)(acc_data.x&0xff);
                send_data[send_len++] = (uint8_t)((acc_data.y>>8)&0xff);
                send_data[send_len++] = (uint8_t)(acc_data.y&0xff);
                send_data[send_len++] = (uint8_t)((acc_data.z>>8)&0xff);
                send_data[send_len++] = (uint8_t)(acc_data.z&0xff);
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
            
        } else if(op_code == 0x03) { // calib 
            if(sndp_hal_acc_exec_calibration_self_calib() != 0) {
                op_code = SNDP_COMM_ERROR_EXEC_FAIL;
            }
           
        } else {
            err_code = SNDP_COMM_ERROR_NOT_SUPPORT;
        }
        
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = err_code;
    for(uint16_t i = 0; i < send_len; i++) {
        cmd_info->data[cmd_info->data_len++] = send_data[i];
    }
   
    sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}


void sndp_comm_cmd_read_ppg_callback(int32_t *ppg_data, uint16_t cnt)
{
    uint8_t data[40];
    uint32_t data_len;

    COMM_CMD_TRACE(2, "cnt=%d", cnt);

    sndp_hal_hr_set_reading_ppg_callback(NULL);
    sndp_hal_hr_stop_reading_ppg();
    sndp_hal_hr_switch_operation_mode(SNDP_HAL_HR_PROX);

    if(cnt > 10) {
        cnt = 10;
    }

    data_len = 0;
    data[data_len++] = SNDP_COMM_ERROR_NONE;
    data[data_len++] = 0x02;
    data[data_len++] = cnt;
    for(uint16_t i = 0; i < cnt; i++) {
        data[data_len++] = (uint8_t)((ppg_data[i]>>16)&0xff);
        data[data_len++] = (uint8_t)((ppg_data[i]>>8)&0xff);
        data[data_len++] = (uint8_t)(ppg_data[i]&0xff);
    }

    sndp_comm_main_send_cmd_by_id(COMM_CMDID_PT_TEST_HRSENSOR_REPORT, 
            sndp_comm_get_local_device(), 
            dev_test_from, 
            dev_test_path, 
            data, 
            data_len);
}


POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_recv_pt_test_hrsensor(sndp_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_HRSENSOR_SUPPORT__)    
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint8_t op_code;
    uint8_t chip_id;
    uint8_t send_data[64];
    uint8_t send_len = 0;

    if(cmd_info->data_len > 0) {
        dev_test_from = COMM_GET_FROM(cmd_info->fromto);
        dev_test_path = cmd_info->path;
    
        op_code = cmd_info->data[0];
        if(op_code == 0x01) {   //read chip id
            if(sndp_hal_hr_read_chip_id(&chip_id) == 0) {
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = chip_id;
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
        } else if(op_code == 0x02) {   //read ppg data
            sndp_hal_hr_switch_operation_mode(SNDP_HAL_HR_PROX_PPG_0);
            sndp_hal_hr_set_reading_ppg_callback(sndp_comm_cmd_read_ppg_callback);
            sndp_hal_hr_start_reading_ppg();

        } else {
            err_code = SNDP_COMM_ERROR_NOT_SUPPORT;
        }
        
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = err_code;
    for(uint16_t i = 0; i < send_len; i++) {
        cmd_info->data[cmd_info->data_len++] = send_data[i];
    }
   
    sndp_comm_main_rsp_cmd(cmd_info);
    
#else
    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
#endif

    return 0;
}
#endif

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

#if defined(__SNDP_LOG_OUTPUT_SWITCH__)
static void hal_trace_log_output_switch(uint32_t en, uint32_t param1, uint32_t param2)
{
    hal_trace_output_enable(en ? true : false);
}
#endif

static bool sndp_comm_cmd_log_output_enabled = true;

bool sndp_comm_cmd_is_log_output_enabled(void)
{
    return sndp_comm_cmd_log_output_enabled;
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
        sndp_comm_cmd_log_output_enabled = log_en;
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

static uint32_t sndp_comm_cmd_recv_pt_query_inbox_status(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = sndp_dev_iobox_is_in_box(false) ? 1 : 0;
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

/* ====== F-MIC -> SPK production loopback (Analog MIC, stereo out) ====== */
#if defined(__SNDP_FT_MIC_LOOPBACK__)
/* FT_LB: 产测 loopback（复用 BES 自带 app_factorymode_audioloop，SNDP 参数在 app_factory_audio.cpp 里适配） */
static bool s_ft_lb_is_running = false;

/* 启动产测 loopback（通过 app_audio_sendrequest 走音频线程） */
static uint32_t sndp_ft_lb_start(void)
{
    if (s_ft_lb_is_running)
        return 0;

    COMM_CMD_TRACE(0, "[FT_LB] start");
    app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP,
                          (uint8_t)APP_BT_SETTING_OPEN, 0);
    s_ft_lb_is_running = true;
    COMM_CMD_TRACE(0, "[FT_LB] request sent");
    return 0;
}

/* 停止产测 loopback */
static uint32_t sndp_ft_lb_stop(void)
{
    if (!s_ft_lb_is_running)
        return 0;

    COMM_CMD_TRACE(0, "[FT_LB] stop");
    app_audio_sendrequest(APP_FACTORYMODE_AUDIO_LOOP,
                          (uint8_t)APP_BT_SETTING_CLOSE, 0);
    s_ft_lb_is_running = false;
    COMM_CMD_TRACE(0, "[FT_LB] stop request sent");
    return 0;
}

/* UART 0x71 handler：启动产测 loopback */
static uint32_t sndp_comm_cmd_recv_pt_start_loopback(sndp_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    sndp_ft_lb_start();
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}

/* UART 0x72 handler：停止产测 loopback */
static uint32_t sndp_comm_cmd_recv_pt_stop_loopback(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_ft_lb_stop();
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    sndp_comm_main_rsp_cmd(cmd_info);
    return 0;
}
#endif /* __SNDP_FT_MIC_LOOPBACK__ */
#if defined(__SNDP_HRSENSOR_SUPPORT__)
static uint32_t sndp_comm_cmd_recv_pt_test_ir(sndp_comm_cmd_info_s *cmd_info)
{  
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint8_t op_code;
    uint8_t chip_id;
    uint16_t ps_value = 0;
    uint16_t high_threshold;
    uint16_t low_threshold;
    uint8_t send_data[10];
    uint8_t send_len = 0;

    if(cmd_info->data_len > 0) {
        op_code = cmd_info->data[0];
        if(op_code == 0x01) {   //read chip id
            if(sndp_hal_hr_read_chip_id(&chip_id) == 0) {
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = chip_id;
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
            
        } else if(op_code == 0x02) {   //read proximity value
            if(sndp_hal_hr_read_proximity_value(&ps_value) == 0) {            
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = (uint8_t)((ps_value>>8)&0xff);
                send_data[send_len++] = (uint8_t)(ps_value&0xff);
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
            
        } else if(op_code == 0x03) { // read proximity threshold
            high_threshold = 0;
            low_threshold = 0;
            if(sndp_hal_hr_read_proximity_threshold(&high_threshold, &low_threshold) == 0) {
                send_len = 0;
                send_data[send_len++] = op_code;
                send_data[send_len++] = (uint8_t)((high_threshold>>8)&0xff);
                send_data[send_len++] = (uint8_t)(high_threshold&0xff);
                send_data[send_len++] = (uint8_t)((low_threshold>>8)&0xff);
                send_data[send_len++] = (uint8_t)(low_threshold&0xff);
            } else {
                op_code = SNDP_COMM_ERROR_READ_FAIL;
            }
            
        } else if(op_code == 0x04) { // write proximity threshold
            if(cmd_info->data_len == 5) {
                high_threshold = (cmd_info->data[1]<<8) | cmd_info->data[2];
                low_threshold = (cmd_info->data[3]<<8) | cmd_info->data[4];
                COMM_CMD_TRACE(1, "h=%d, l=%d", high_threshold, low_threshold);

                if(sndp_hal_hr_write_proximity_threshold(high_threshold, low_threshold) == 0) {
                    high_threshold = 0;
                    low_threshold = 0;
                    sndp_hal_hr_read_proximity_threshold(&high_threshold, &low_threshold);
                    send_len = 0;
                    send_data[send_len++] = op_code;
                    send_data[send_len++] = (uint8_t)((high_threshold>>8)&0xff);
                    send_data[send_len++] = (uint8_t)(high_threshold&0xff);
                    send_data[send_len++] = (uint8_t)((low_threshold>>8)&0xff);
                    send_data[send_len++] = (uint8_t)(low_threshold&0xff);
                } else {
                    op_code = SNDP_COMM_ERROR_SAVE_FAIL;
                }
            } else {
                err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
            }
        }else {
            err_code = SNDP_COMM_ERROR_NOT_SUPPORT;
        }
        
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = err_code;
    for(uint16_t i = 0; i < send_len; i++) {
        cmd_info->data[cmd_info->data_len++] = send_data[i];
    }
   
	sndp_comm_main_rsp_cmd(cmd_info);

    return 0;
}
#endif

static uint32_t sndp_comm_cmd_recv_pt_query_bt_name(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t *bt_name = NULL;
    uint16_t name_len;

    bt_name = factory_section_get_bt_name();
    
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    if(bt_name != NULL) {
        name_len = strlen((char *)bt_name);
        if(name_len > SNDP_COMM_FRAME_CMD_DLEN_MAX - 1) {
            name_len = SNDP_COMM_FRAME_CMD_DLEN_MAX - 1;
        }
        
        memcpy(&cmd_info->data[cmd_info->data_len], bt_name, name_len);
        cmd_info->data_len += name_len;
        cmd_info->data[cmd_info->data_len] = 0;
    }
    
	sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_tws_pairing_addr(sndp_comm_cmd_info_s *cmd_info)
{
    struct nvrecord_env_t *nvrecord_env;
        
    nv_record_env_get(&nvrecord_env);
    
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    
    if(nvrecord_env->ibrt_mode.mode != 0xFF) {
        memcpy(&cmd_info->data[cmd_info->data_len], nvrecord_env->ibrt_mode.record.bdAddr.address, 6);
    } else {
        memset(&cmd_info->data[cmd_info->data_len], 0x00, 6);
    }

    cmd_info->data_len += 6;

    sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_del_tws_pairing_addr(sndp_comm_cmd_info_s *cmd_info)
{
    ibrt_config_t ibrt_config;
    uint8_t local_addr[6] = {0};

    factory_section_original_btaddr_get(local_addr);


    ibrt_config.nv_role = 0xFF;
    memcpy((void *)ibrt_config.local_addr.address, local_addr, 6);
    memset(ibrt_config.peer_addr.address, 0, 6);
    sndp_ibrt_reconfig_save_to_nvrecord(&ibrt_config);

    sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NONE);
   
    return 0;
}


static uint32_t sndp_comm_cmd_recv_pt_read_test_flag(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_da_field_test_flag_s field_test_flag;

    memset(&field_test_flag, 0, sizeof(field_test_flag));
    if(sndp_da_read_field(SNDP_DA_FIELD_TEST_FLAG, &field_test_flag, sizeof(sndp_da_field_test_flag_s), true) == 0) {
        if(field_test_flag.key != SNDP_DA_PARAM_FIELD_VALID ) {
            field_test_flag.test_flag = 0;
        }
    }

    COMM_CMD_TRACE(1, "test_flag=%08X", field_test_flag.test_flag);

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
#if 0
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((field_test_flag.test_flag>>24)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((field_test_flag.test_flag>>16)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((field_test_flag.test_flag>>8)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(field_test_flag.test_flag&0xff);
#else
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(field_test_flag.test_flag & 0xff);
#endif
	sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_write_test_flag(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    uint8_t test_item_idx;

    sndp_da_field_test_flag_s field_test_flag;

    if(cmd_info->data_len == 1) {
        test_item_idx = cmd_info->data[0];
        COMM_CMD_TRACE(1, "test_item_idx=%d", test_item_idx);
        
        if(test_item_idx >= 1 && test_item_idx <= 32) {
            memset(&field_test_flag, 0, sizeof(field_test_flag));
            sndp_da_read_field(SNDP_DA_FIELD_TEST_FLAG, &field_test_flag, sizeof(sndp_da_field_test_flag_s), true);

            if(field_test_flag.key != SNDP_DA_PARAM_FIELD_VALID) {
                field_test_flag.test_flag = 0;
                COMM_CMD_TRACE(1, "test_flag not initialized, set to 0");
            }

            COMM_CMD_TRACE(1, "0 test_flag=%08X", field_test_flag.test_flag);
#if 0
            field_test_flag.test_flag |= (1<<(test_item_idx-1));
#else
            field_test_flag.test_flag = test_item_idx;
#endif
            COMM_CMD_TRACE(1, "1 test_flag=%08X", field_test_flag.test_flag);
            
            if(sndp_da_write_field(SNDP_DA_FIELD_TEST_FLAG, &field_test_flag, sizeof(sndp_da_field_test_flag_s), true) != 0) {
                err_code = SNDP_COMM_ERROR_SAVE_FAIL;
            }
            
        } else {
            err_code = SNDP_COMM_ERROR_PARAM_OUT_RANG;
        }
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_sleep_algo_auth(sndp_comm_cmd_info_s *cmd_info)
{
	sndp_comm_cmd_rsp_with_errcode(cmd_info, SNDP_COMM_ERROR_NOT_SUPPORT);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_write_dev_color(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;
    sndp_da_field_dev_color_s field_dev_color;

    if(cmd_info->data_len == 1) {
        memset(&field_dev_color, 0, sizeof(field_dev_color));
        field_dev_color.dev_color = cmd_info->data[0];
        COMM_CMD_TRACE(1, "key=%08X, color=%08X", field_dev_color.key, field_dev_color.dev_color);
        if(sndp_da_write_field(SNDP_DA_FIELD_DEV_COLOR, &field_dev_color, sizeof(sndp_da_field_dev_color_s), true) != 0) {
            err_code = SNDP_COMM_ERROR_SAVE_FAIL;
        }
            
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_read_dev_color(sndp_comm_cmd_info_s *cmd_info)
{
    sndp_da_field_dev_color_s field_dev_color;

    memset(&field_dev_color, 0, sizeof(field_dev_color));
    if(sndp_da_read_field(SNDP_DA_FIELD_DEV_COLOR, &field_dev_color, sizeof(sndp_da_field_dev_color_s), true) == 0) {
        COMM_CMD_TRACE(1, "key=%08X, color=%08X", field_dev_color.key, field_dev_color.dev_color);
        
        if(field_dev_color.key != SNDP_DA_PARAM_FIELD_VALID ) {
            field_dev_color.dev_color = 0;
        }
    }

    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(field_dev_color.dev_color&0xff);
	sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_switch_anc_mode(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

    if(cmd_info->data_len == 1) {
        if((sndp_anc_mode_e)cmd_info->data[0] < SNDP_ANC_MODE_QTY) {
            sndp_anc_mode_set_locally((sndp_anc_mode_e) cmd_info->data[0]);
        } else {
            err_code = SNDP_COMM_ERROR_PARAM_OUT_RANG;
        }
            
    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
    return 0;
}

static uint32_t sndp_comm_cmd_recv_pt_query_ntc_info(sndp_comm_cmd_info_s *cmd_info)
{
    int16_t temperature;
    uint16_t ntc_voltage;
    
    temperature = sndp_dev_temperature_get_temperature(false);
    ntc_voltage = sndp_dev_temperature_get_ntc_voltage(false);
    
    cmd_info->data_len = 0;
    cmd_info->data[cmd_info->data_len++] = SNDP_COMM_ERROR_NONE;
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((temperature>>8)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(temperature&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)((ntc_voltage>>8)&0xff);
    cmd_info->data[cmd_info->data_len++] = (uint8_t)(ntc_voltage&0xff);
    
	sndp_comm_main_rsp_cmd(cmd_info);
   
    return 0;
}

static uint8_t click_test_from = SNDP_COMM_DEVICE_ATE;
static uint8_t click_test_path = SNDP_COMM_PATH_SPP;

static uint32_t sndp_comm_cmd_recv_pt_switch_click_test(sndp_comm_cmd_info_s *cmd_info)
{
    uint8_t err_code = SNDP_COMM_ERROR_NONE;

    if(cmd_info->data_len == 1) {
        click_test_from = COMM_GET_FROM(cmd_info->fromto);
        click_test_path =  cmd_info->path;
        sndp_pt_switch_click_test(cmd_info->data[0]);

    } else {
        err_code = SNDP_COMM_ERROR_PARAM_LEN_INVALID;
    }
    
	sndp_comm_cmd_rsp_with_errcode(cmd_info, err_code);
    return 0;
}

uint32_t sndp_comm_cmd_send_pt_click_test_report(uint8_t tap_event)
{   
    uint8_t data[4];
    uint32_t data_len;

    data_len = 0;
    data[data_len++] = SNDP_COMM_ERROR_NONE;
    data[data_len++] = tap_event;

    sndp_comm_main_send_cmd_by_id(COMM_CMDID_PT_CLICK_TEST_REPORT, 
            sndp_comm_get_local_device(), 
            click_test_from, 
            click_test_path, 
            data, 
            data_len);
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
    { COMM_CMDID_LR_SYNC_MOBILE_CONNECTED       , "LR_SYNC_MOBILE_CONNECTED", sndp_comm_cmd_recv_lr_sync_mobile_connected       },
    { COMM_CMDID_LR_SYNC_DISCONNECT_AND_TWS_PAIR, "LR_SYNC_DISCONNECT_TWSPR", sndp_comm_cmd_recv_lr_sync_disconnect_and_tws_pair },
    { COMM_CMDID_LR_SYNC_MUSIC_CTRL             , "LR_SYNC_MUSIC_CTRL"      , sndp_comm_cmd_recv_lr_sync_music_ctrl             },
    { COMM_CMDID_LR_SYNC_CALL_CTRL              , "LR_SYNC_CALL_CTRL"       , sndp_comm_cmd_recv_lr_sync_call_ctrl              },
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
    { COMM_CMDID_PT_READ_DEV_SN                 , "PT_R_DEV_SN "            , sndp_comm_cmd_recv_pt_read_dev_sn                 },
	{ COMM_CMDID_PT_WRITE_DEV_SN                , "PT_W_DEV_SN"	            , sndp_comm_cmd_recv_pt_write_dev_sn                },
    { COMM_CMDID_PT_READ_BT_ADDR                , "PT_R_BT_ADDR"	        , sndp_comm_cmd_recv_pt_read_bt_addr                },
	{ COMM_CMDID_PT_WRITE_BT_ADDR               , "PT_W_BT_ADDR "	        , sndp_comm_cmd_recv_pt_set_bt_addr                 },
	{ COMM_CMDID_PT_READ_BLE_ADDR               , "PT_R_BLE_ADDR"	        , sndp_comm_cmd_recv_pt_read_ble_addr		        },
	{ COMM_CMDID_PT_WRITE_BLE_ADDR              , "PT_W_BLE_ADDR"	        , sndp_comm_cmd_recv_pt_set_ble_addr		        },
    { COMM_CMDID_PT_READ_RF_FREQ_OFF            , "PT_R_RF_FQ_OFF"          , sndp_comm_cmd_recv_pt_read_frequency_offset       }, 
	{ COMM_CMDID_PT_WRITE_RF_FREQ_OFF           , "PT_W_RF_FQ_OFF"          , sndp_comm_cmd_recv_pt_write_frequency_offset      },	
	{ COMM_CMDID_PT_TEST_MIC                    , "PT_TEST_MIC"	            , sndp_comm_cmd_recv_pt_test_mic                    },
    { COMM_CMDID_PT_TEST_SPK                    , "PT_TEST_SPK"	            , sndp_comm_cmd_recv_pt_test_speaker                },
	{ COMM_CMDID_PT_QUERY_DEV_STATUS            , "PT_Q_DEV_STA"	        , sndp_comm_cmd_recv_pt_query_dev_status            },
	{ COMM_CMDID_PT_READ_ANC_CALIB_STATUS       , "PT_R_ANC_CALIB_STA"      , sndp_comm_cmd_recv_pt_read_anc_calib_status       },
    { COMM_CMDID_PT_READ_ALGO_AUTH_RESULT       , "PT_R_ALGO_AUTH_RST"      , sndp_comm_cmd_recv_pt_read_algo_auth_result       },
    { COMM_CMDID_PT_SWITCH_LOG_OUTPUT           , "PT_S_LOG_OUTPUT"         , sndp_comm_cmd_recv_pt_log_output_switch           },
    { COMM_CMDID_PT_SWITCH_WEAR_STATUS_REPORT   , "PT_S_WEAR_STA_RPT"       , sndp_comm_cmd_recv_pt_switch_wear_status_report   },
    { COMM_CMDID_PT_CHECK_EARSIDE               , "PT_C_EARSIDE"            , sndp_comm_cmd_recv_pt_check_earside               },
    { COMM_CMDID_PT_READ_HALL_STATUS            , "PT_R_HALL_STA"           , sndp_comm_cmd_recv_pt_read_hall_status            },
#if defined(__SNDP_HRSENSOR_SUPPORT__)
    { COMM_CMDID_PT_TEST_IR                     , "PT_TEST_IR"              , sndp_comm_cmd_recv_pt_test_ir                     },
    { COMM_CMDID_PT_TEST_HRSENSOR               , "PT_TEST_HRSENSOR"	    , sndp_comm_cmd_recv_pt_test_hrsensor               },
#endif
#if defined(__SNDP_GSENSOR_SUPPORT__)
    { COMM_CMDID_PT_TEST_GSENSOR                , "PT_TEST_GSENSOR"	        , sndp_comm_cmd_recv_pt_test_gsensor                },
#endif
    { COMM_CMDID_PT_QUERY_BT_NAME               , "PT_Q_BT_NAME"            , sndp_comm_cmd_recv_pt_query_bt_name               },
    { COMM_CMDID_PT_READ_TWS_PAIRING_ADDR       , "PT_R_TWS_PAIRING_ADDR"   , sndp_comm_cmd_recv_pt_query_tws_pairing_addr      },
    { COMM_CMDID_PT_DEL_TWS_PAIRING_ADDR        , "PT_D_TWS_PAIRING_ADDR"   , sndp_comm_cmd_recv_pt_del_tws_pairing_addr        },
    { COMM_CMDID_PT_READ_TEST_FLAG              , "PT_R_TEST_FLAG"          , sndp_comm_cmd_recv_pt_read_test_flag              },
    { COMM_CMDID_PT_WRITE_TEST_FLAG             , "PT_W_TEST_FLAG"          , sndp_comm_cmd_recv_pt_write_test_flag             },
    { COMM_CMDID_PT_READ_SLEEP_ALGO_AUTH        , "PT_R_SLEEP_ALGO_AUTH"    , sndp_comm_cmd_recv_pt_read_sleep_algo_auth        },
    { COMM_CMDID_PT_WRITE_DEV_COLOR             , "PT_W_DEV_COLOR"          , sndp_comm_cmd_recv_pt_write_dev_color             },
    { COMM_CMDID_PT_READ_DEV_COLOR              , "PT_R_DEV_COLOR"          , sndp_comm_cmd_recv_pt_read_dev_color              },
    { COMM_CMDID_PT_SWITCH_ANC_MODE             , "PT_S_ANC_MODE"           , sndp_comm_cmd_recv_pt_switch_anc_mode             },
    { COMM_CMDID_PT_QUERY_NTC_INFO              , "PT_Q_NTC_INFO"           , sndp_comm_cmd_recv_pt_query_ntc_info              },
    { COMM_CMDID_PT_SWICH_CLICK_TEST            , "PT_S_CLICK_TEST"         , sndp_comm_cmd_recv_pt_switch_click_test           },
    { COMM_CMDID_PT_QUERY_INBOX_STATUS          , "PT_Q_INBOX_STA"          , sndp_comm_cmd_recv_pt_query_inbox_status          },
#if defined(__SNDP_FT_MIC_LOOPBACK__)
    { COMM_CMDID_PT_START_LOOPBACK              , "PT_S_LOOPBACK"           , sndp_comm_cmd_recv_pt_start_loopback              },
    { COMM_CMDID_PT_STOP_LOOPBACK               , "PT_E_LOOPBACK"           , sndp_comm_cmd_recv_pt_stop_loopback               },
#endif
#endif
    

    /***** 与APP交互指令 *****/
    { COMM_CMDID_APP_QUERY_DEV_INFO             , "APP_Q_DEV_INFO"      , sndp_comm_cmd_recv_app_query_dev_info                 },
	{ COMM_CMDID_APP_QUERY_DEV_STATUS           , "APP_Q_DEV_STA"       , sndp_comm_cmd_recv_app_query_dev_status               },
        
#if defined(__SNDP_SLEEP_APP__)
    { COMM_CMDID_LR_SYNC_SLEEP_APP_FLAG          , "LR_SYNC_SLEEP_APP_FLAG"  , sndp_comm_cmd_recv_lr_sync_sleep_app_flag          },
#if defined(__SNDP_GESTURE_MAP__)
    { COMM_CMDID_LR_SYNC_UPDATE_MAPPING         , "LR_SYNC_UPDATE_MAPPING"  , sndp_comm_cmd_recv_lr_sync_update_mapping         },
#endif
#if defined(__SNDP_HEART_RATE_MGR__)
    { COMM_CMDID_LR_SYNC_Proximity_Notification_ONOFF   , "LR_SYNC_Prox_Notifi_ONOFF"   , sndp_comm_cmd_recv_lr_sync_Proximity_Notification_ONOFF   },
    { COMM_CMDID_LR_SYNC_Proximity_Notification_DATA    , "LR_SYNC_Prox_Notifi_DATA"    , sndp_comm_cmd_recv_lr_sync_Proximity_Notification_DATA    },
    { COMM_CMDID_LR_SYNC_HEARTRATE_ONOFF        , "LR_SYNC_HEARTRATE_ONOFF"     , sndp_comm_cmd_recv_lr_sync_heart_rate_onoff       },
    { COMM_CMDID_LR_SYNC_STAGE_ONOFF            , "LR_SYNC_STAGE_ONOFF"         , sndp_comm_cmd_recv_lr_sync_stage_onoff            },
    { COMM_CMDID_LR_SYNC_START_SLEEP             , "LR_SYNC_START_SLEEP"    , sndp_comm_cmd_recv_lr_sync_start_sleep              },
    { COMM_CMDID_LR_SYNC_SLEEP_TRACKING          , "LR_SYNC_SLEEP_TRACKING" , sndp_comm_cmd_recv_lr_sync_sleep_tracking           },
    { COMM_CMDID_LR_SYNC_STOP_SLEEP              , "LR_SYNC_STOP_SLEEP"     , sndp_comm_cmd_recv_lr_sync_stop_sleep               },
    { COMM_CMDID_LR_SYNC_START_HEARTRATE         , "LR_SYNC_START_HR"      , sndp_comm_cmd_recv_lr_sync_start_heartrate          },
    { COMM_CMDID_LR_SYNC_STOP_HEARTRATE          , "LR_SYNC_STOP_HR"       , sndp_comm_cmd_recv_lr_sync_stop_heartrate           },
    { COMM_CMDID_LR_SYNC_PPG_NOTIFICATION        , "LR_SYNC_PPG_NTF"       , sndp_comm_cmd_recv_lr_sync_ppg_notification         },
    { COMM_CMDID_LR_SYNC_ACC_NOTIFICATION        , "LR_SYNC_ACC_NTF"       , sndp_comm_cmd_recv_lr_sync_acc_notification         },
#endif
#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)
    { COMM_CMDID_LR_SYNC_SLEEP_SNAPSHOT         , "LR_SYNC_SLEEP_SNAPSHOT"      , sndp_comm_cmd_recv_lr_sync_sleep_snapshot         },
    { COMM_CMDID_LR_SYNC_SLEEP_ROLE_STATUS      , "LR_SYNC_SLEEP_ROLE_STATUS"   , sndp_comm_cmd_recv_lr_sync_sleep_role_status      },
#endif
#endif
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

#if defined(__SNDP_PRODUCT_TEST__)
    if(COMM_CMDID_IS_PT_CMD(sndp_comm_exec_cmd.cmd_id)
        && sndp_comm_exec_cmd.cmd_id != COMM_CMDID_PT_SWITCH_TEST_MODE
        && sndp_comm_exec_cmd.cmd_id != COMM_CMDID_PT_QUERY_TEST_MODE
        && sndp_comm_exec_cmd.cmd_id != COMM_CMDID_PT_QUERY_FW_VER
        && sndp_comm_exec_cmd.cmd_id != COMM_CMDID_PT_ENTER_DUT
        && sndp_comm_exec_cmd.path != SNDP_COMM_PATH_TRACE_UART) {
        if(!sndp_pt_is_in_test_mode()) {
            COMM_CMD_TRACE(1, "cmd(0x%02X) rejected, not in test mode", sndp_comm_exec_cmd.cmd_id);
            sndp_comm_cmd_rsp_with_errcode(&sndp_comm_exec_cmd, SNDP_COMM_ERROR_NOT_IN_TEST_MODE);
            return 0;
        }
    }
#endif
    
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
#endif	/* __SNDP_COMM_CMD_DEFAULT__ */


