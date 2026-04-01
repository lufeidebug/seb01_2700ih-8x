#if defined(__SNDP_UI__)
#include "stdio.h"
#include "cmsis_os.h"
#include "string.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "apps.h"
#include "nvrecord_env.h"
#include "app_media_player.h"
#include "app_audio.h"
#include "app_key.h"
#include "bta_tws_ux_api.h"

#include "sndp_ui.h"
#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#if defined(__SNDP_CHARGER_MGR__)
#include "sndp_hal_charger.h"
#endif

#if defined(__SNDP_AUDIO_TEST__)
#include "sndp_audio_test.h"
#endif

#if defined(__SNDP_COMM_MGR__)
#include "sndp_comm_cmd.h"
#endif

#if defined(__SNDP_HEART_RATE_MGR__)
#include "sndp_heart_rate.h"
#endif
#include "sndp_comm_ble.h"

#if defined(__SNDP_AUDIO_TEST__)
#include "sndp_audio_test.h"
#endif


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SPUI_TIME_TODO_INTERVAL					(10)	//seconds

#define SPUI_CHARGING_TIME_MAX					(3600*2)	//seconds
#define SPUI_CHARGING_FULL_TIME_MAX				(60*1)		//seconds

#define SPUI_LOWPWR_WARNING_PERCENTAGE			(20)
#define SPUI_LOWPWR_WARNING_INTERVAL			(60*5)	//seconds
#define SPUI_LOWPWR_WARNING_CNT_MAX				(3)	//times

#define SPUI_LOWPWR_SHUTDOWN_PERCENTAGE			(0)	//percentage
#define SPUI_LOWPWR_SHUTDOWN_VOLTAGE			(3300)	//mv
#define SPUI_LOWPWR_SHUTDOWN_CHECK_CNT			(3)		//times, 10s*times

#define SPUI_WORKING_TEMPERATURE_MAX			(50)	
#define SPUI_WORKING_TEMPERATURE_MIN			(0)
#define SPUI_TEMPERATURE_ABNORMAL_DURATION		(30)	//seconds

#define SPUI_CLOSE_DISCHARGE_MAX				(60*1)		//seconds


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    uint32_t charging_time;
    uint32_t charging_full_time;

    bool lowpwr_check_enable;
    uint16_t lowpwr_warning_cnt;
    uint32_t lowpwr_warning_last_time;
    uint16_t lowpwr_shutdown_cnt;
    
    uint32_t close_discharge_time;

    bool temperature_check_enable;
    uint16_t temperature_exp_shutdown_time; 

    bool wear_play_music_allowed;

    sndp_anc_status_e anc_status;
    sndp_anc_mode_e anc_mode;
    
} sndp_ui_ctx_s;

typedef struct {
    sndp_dev_cover_status_e cover_sta;
    sndp_dev_iobox_status_e iobox_sta;
    sndp_dev_wear_status_e wear_sta;
    sndp_dev_bat_info_s bat_info;

} sndp_ui_all_dev_sta_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void sndp_ui_all_status_sync_send(void);



/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_ui_ctx_s sndp_ui_ctx;

static sndp_ui_pairing_type_e sndp_ui_pairing_type = SNDP_UI_PAIRING_NONE;  //0:none, 1:tws pairing, 2:freeman pairing


/**************************************************************************************************
* Function
**************************************************************************************************/

void sndp_ui_working_mode_switch(void)
{
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {
        SPUI_TRACE(0, "BT_MODE");
        //Close ANC
        sndp_anc_mode_set(SNDP_ANC_MODE_OFF);

        //Close sleep analysis, save data.
#if defined(__SNDP_HEART_RATE_MGR__)        
        sndp_sleep_analysis_stop();
#endif

        //Set the working mode to BT mode.
        sndp_dev_set_working_mode(SNDP_DEV_WORKING_MODE_BT);

        //Play prompt sound.
#ifdef MEDIA_PLAYER_SUPPORT            
        media_PlayAudio(AUD_ID_WORKING_MODE_BT, 0);
#endif

        //Open BT.
        sndp_bt_switch(true, true);
        
    } else {
        SPUI_TRACE(0, "SLEEP_MODE");
        
        //Close BT.
        sndp_bt_switch(false, true);

        //set the working mode to Sleep mode.
        sndp_dev_set_working_mode(SNDP_DEV_WORKING_MODE_SLEEP);

        //Play prompt sound.
#ifdef MEDIA_PLAYER_SUPPORT            
        media_PlayAudio(AUD_ID_WORKING_MODE_SLEEP, 0);
#endif

        //Open ANC.
        sndp_anc_mode_set(sndp_ui_ctx.anc_mode);

        //Open sleep analysis.
#if defined(__SNDP_HEART_RATE_MGR__)        
        sndp_sleep_analysis_start(0);
#endif

    }
}


void sndp_ui_pairing_type_set(sndp_ui_pairing_type_e type)
{
     sndp_ui_pairing_type = type;
}

bool sndp_ui_pairing_type_is(sndp_ui_pairing_type_e type)
{
    return (sndp_ui_pairing_type == type);
}


//---------------------------------------- volume ctrl --------------------------------------------

void sndp_ui_volume_set(uint8_t type, uint8_t level) 
{
    //SPUI_TRACE(2, "type=%d, level=%d", type, level);
}

void sndp_ui_volume_inc(uint8_t type, uint8_t level) 
{
    //SPUI_TRACE(2, "type=%d, level=%d", type, level);
}

void sndp_ui_volume_dec(uint8_t type, uint8_t level) 
{
    //SPUI_TRACE(2, "type=%d, level=%d", type, level);
}


//---------------------------------------- anc ctrl --------------------------------------------

static void sndp_ui_anc_switch(void) 
{
	SPUI_TRACE(1, "status=%d, mode=%d", sndp_ui_ctx.anc_status, sndp_ui_ctx.anc_mode);
	
	if(sndp_ui_ctx.anc_status == SNDP_ANC_STA_OFF) {
        sndp_ui_ctx.anc_status = SNDP_ANC_STA_ON;
        
#ifdef MEDIA_PLAYER_SUPPORT        
		media_PlayAudio(AUD_ID_ANC_ON, 0);
#endif
		sndp_delay_exec_start(1000, (uint32_t)sndp_anc_mode_set, (uint32_t)sndp_ui_ctx.anc_mode, 0, 0);

	} else if(sndp_ui_ctx.anc_status == SNDP_ANC_STA_ON) {
        sndp_ui_ctx.anc_status = SNDP_ANC_STA_TRANSPARENT;
        
		sndp_anc_mode_set(SNDP_ANC_MODE_OFF);
#ifdef MEDIA_PLAYER_SUPPORT        
		media_PlayAudio(AUD_ID_ANC_ON, 0);
#endif           
		sndp_delay_exec_start(1000, (uint32_t)sndp_anc_mode_set, (uint32_t)SNDP_ANC_MODE_TRANSPARENT, 0, 0);

	} else if(sndp_ui_ctx.anc_status == SNDP_ANC_STA_TRANSPARENT) {
        sndp_ui_ctx.anc_status = SNDP_ANC_STA_OFF;
        
	    sndp_anc_mode_set(SNDP_ANC_MODE_OFF);
#ifdef MEDIA_PLAYER_SUPPORT        
		media_PlayAudio(AUD_ID_ANC_OFF, 0);
#endif  

	}
}


//---------------------------------------- wear ctrl --------------------------------------------
POSSIBLY_UNUSED static void sndp_ui_wear_on_play_music(void)
{
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {
        SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
    }
    
    if(!sndp_ui_ctx.wear_play_music_allowed) {
        SPUI_TRACE(0, "%d, rtn", __LINE__);
        return;
    }
    
	if(sndp_is_sco_mode()) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}

	if(sndp_music_is_playing()) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}
#if defined(__SNDP_SLEEP_APP__)
    if(sndp_dev_get_splaypause_onoff(false) == 0)
    {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;        
    }
#endif
	sndp_music_ctrl(SNDP_MUSIC_CTRL_PLAY);
}

static void sndp_ui_wear_off_stop_music(void)
{				
	if(sndp_is_sco_mode()) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}

	if(sndp_is_tws_link_connected()) {
		if(sndp_dev_wear_is_worn(false) || sndp_dev_wear_is_worn(true)) {
			SPUI_TRACE(0, "%d, rtn", __LINE__);
			return;
		}
	}
#if defined(__SNDP_SLEEP_APP__)
    if(sndp_dev_get_splaypause_onoff(false) == 0)
    {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;        
    }
#endif
	sndp_music_ctrl(SNDP_MUSIC_CTRL_PAUSE);
}

static void sndp_ui_wear_on_role_switch(void)
{
    if(sndp_is_tws_link_connected()) {
        if(sndp_is_tws_slave_mode() && !sndp_dev_wear_is_worn(true)) {
            SPUI_TRACE(0, "%d", __LINE__);
            sndp_ibrt_tws_switch();
        }
    }
}

static void sndp_ui_wear_off_role_switch(void)
{
    if(sndp_is_tws_link_connected()) {
        if(sndp_is_tws_master_mode() && sndp_dev_wear_is_worn(true)) {
            SPUI_TRACE(0, "%d", __LINE__);
            sndp_ibrt_tws_switch();
        }
    }
}

static POSSIBLY_UNUSED void sndp_ui_wear_off_tone_switch_to_phone(void)
{
//	return;
	
	if(!sndp_call_is_active()) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}

    if(sndp_is_tws_link_connected()) {
        if(!sndp_is_tws_master_mode()) {
		    SPUI_TRACE(0, "%d, rtn", __LINE__);
            return;
        } else if(sndp_dev_wear_is_worn(true)) {
            SPUI_TRACE(0, "%d, rtn", __LINE__);
            return;
        }
	}
    
    sndp_call_ctrl(SNDP_CALL_CTRL_TONE_SWITCH_TO_PHONE);
}

static POSSIBLY_UNUSED void sndp_ui_wear_on_tone_switch_to_earbuds(void)
{
//	return;

	if(!sndp_call_is_active()){
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}

    sndp_call_ctrl(SNDP_CALL_CTRL_TONE_SWITCH_TO_EARPHONE);
}

static POSSIBLY_UNUSED void sndp_ui_wear_on_open_anc(void)
{
	SPUI_TRACE(0, "starting...");
	if(sndp_is_tws_link_connected()) {
		if(sndp_dev_wear_is_worn(false) && sndp_dev_wear_is_worn(true)) {
			sndp_anc_mode_set(sndp_ui_ctx.anc_mode);
		} else {
			sndp_anc_mode_set_locally(sndp_ui_ctx.anc_mode);
		}
	} else {
		sndp_anc_mode_set_locally(sndp_ui_ctx.anc_mode);
	}
}

static POSSIBLY_UNUSED void sndp_ui_wear_off_close_anc(void)
{
	if(!sndp_anc_is_on()) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	} 

	SPUI_TRACE(0, "stopping...");
#if 0		
	sndp_anc_mode_set(SNDP_ANC_MODE_OFF);
#else
	sndp_anc_mode_set_locally(SNDP_ANC_MODE_OFF);
#endif
	
}

static POSSIBLY_UNUSED void sndp_ui_wear_on_start_hr(void)
{
	SPUI_TRACE(0, "starting");
#if defined(__SNDP_HEART_RATE_MGR__)            
    //sndp_hr_mearsuring_start(1, 0);
    //sndp_sleep_analysis_start(0);
#endif    
}

static POSSIBLY_UNUSED void sndp_ui_wear_off_stop_hr(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)            
    sndp_hr_mearsuring_stop();
    sndp_sleep_analysis_start(0);
#endif

	SPUI_TRACE(0, "stopped");
}

static void sndp_ui_wear_on_play_tone(void) 
{
	SPUI_TRACE_ENTER();
	
#ifdef MEDIA_PLAYER_SUPPORT
    media_PlayAudio_locally(AUD_ID_WEAR_ON, 0);
#endif

}

void sndp_ui_wear_action(sndp_dev_wear_status_e wear_action, bool remote)
{
	SPUI_TRACE(2, "wear_action=%d, remote=%d", wear_action, remote);

	if(remote == false) {
		if(SNDP_DEV_WEAR_ON == wear_action) {	
            sndp_delay_exec_start(200, (uint32_t)sndp_ui_wear_on_tone_switch_to_earbuds, 0, 0, 0);
            sndp_delay_exec_start(300, (uint32_t)sndp_ui_wear_on_play_music, 0, 0, 0);
			sndp_delay_exec_start(500, (uint32_t)sndp_ui_wear_on_role_switch, 0, 0, 0);          
            //sndp_delay_exec_start(1000, (uint32_t)sndp_ui_wear_on_start_hr, 0, 0, 0);           
	    } else if(SNDP_DEV_WEAR_OFF == wear_action) {
            sndp_delay_exec_start(200, (uint32_t)sndp_ui_wear_off_tone_switch_to_phone, 0, 0, 0);
			sndp_delay_exec_start(100, (uint32_t)sndp_ui_wear_off_stop_music, 0, 0, 0);	
            sndp_delay_exec_start(500, (uint32_t)sndp_ui_wear_off_role_switch, 0, 0, 0);           
            sndp_ui_wear_off_stop_hr();
		}

        
	} else if(remote == true) {
		// only the master can execute.
		if(SNDP_DEV_WEAR_ON == wear_action) {		

		} else if(SNDP_DEV_WEAR_OFF == wear_action) {
			sndp_delay_exec_start(100, (uint32_t)sndp_ui_wear_off_stop_music, 0, 0, 0);	
		}
	}
}


/* Local deal wear status is changed */
static void sndp_ui_wear_status_changed(sndp_dev_wear_status_e wear_status)
{
	SPUI_TRACE(1, "WEAR_%s", (SNDP_DEV_WEAR_ON == wear_status) ? "ON" : "OFF");

	sndp_delay_exec_stop((uint32_t)sndp_ui_wear_on_play_tone);
	sndp_delay_exec_stop((uint32_t)sndp_ui_wear_on_play_music);
	sndp_delay_exec_stop((uint32_t)sndp_ui_wear_off_stop_music);
    sndp_delay_exec_stop((uint32_t)sndp_ui_wear_on_tone_switch_to_earbuds);
    sndp_delay_exec_stop((uint32_t)sndp_ui_wear_off_tone_switch_to_phone);
    sndp_delay_exec_stop((uint32_t)sndp_ui_wear_on_role_switch);
    sndp_delay_exec_stop((uint32_t)sndp_ui_wear_off_role_switch);  
    sndp_delay_exec_stop((uint32_t)sndp_ui_wear_on_start_hr);

    if(sndp_ui_pairing_type_is(SNDP_UI_PAIRING_FREEMAN)) {
        SPUI_TRACE(0, "freeman pairing return.");
        return;
    } 
    
	/* update the ibrt status machine */
	if(SNDP_DEV_WEAR_ON == wear_status) {
        sndp_dev_acc_enter_detection_mode();
    
		bta_tws_box_event_entry(BTA_TWS_WEAR_UP);
		//bta_tws_box_event_entry(APP_UI_EV_MOBILE_RECONNECT);	
		/* play wear prompt tone */
		sndp_delay_exec_start(500, (uint32_t)sndp_ui_wear_on_play_tone, 0, 0, 0);

	} else {
        sndp_dev_acc_enter_standby_mode();
        
		/* update the ibrt status machine */
		bta_tws_box_event_entry(BTA_TWS_WEAR_DOWN);

	}

	sndp_ui_wear_action(wear_status, false);
}

//---------------------------------------- cover ctrl --------------------------------------------
static void sndp_ui_cover_status_changed(sndp_dev_cover_status_e cover_status)
{
    static sndp_dev_cover_status_e status = SNDP_DEV_COVER_UNKNOWN;

    if(cover_status == status) {
        return;
    }
    status = cover_status;
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);   

    SPUI_TRACE(1, "BOX_%s", (SNDP_DEV_COVER_COLSED == cover_status) ? "CLOSED" : "OPENED");
    
    if(SNDP_DEV_COVER_COLSED == cover_status) {
        sndp_dev_wear_disable_detection();
        bta_tws_box_event_entry(BTA_TWS_CLOSE);
        
    } else {
        //sndp_dev_wear_enable_detection();
#if defined(__BTIF_EARPHONE__)
        app_stop_10_second_timer(APP_PAIR_TIMER_ID);
#endif

        /* update the ibrt status machine */
        bta_tws_box_event_entry(BTA_TWS_OPEN);
    }
    
}


//---------------------------------------- iobox ctrl --------------------------------------------

static void sndp_ui_inbox_role_switch(void)
{
    if(sndp_is_tws_link_connected()) {
        if(sndp_is_tws_master_mode() && !sndp_dev_iobox_is_in_box(true)) {
            SPUI_TRACE(0, "%d", __LINE__);
            sndp_ibrt_tws_switch();
        }
    }
}

static void sndp_ui_outbox_role_switch(void)
{
    if(sndp_is_tws_link_connected()) {
        if(sndp_is_tws_slave_mode() && sndp_dev_iobox_is_in_box(true)) {
            SPUI_TRACE(0, "%d", __LINE__);
            sndp_ibrt_tws_switch();
        }
    }
}

static void sndp_ui_iobox_status_changed(sndp_dev_iobox_status_e inout_status)
{
    static sndp_dev_iobox_status_e status = SNDP_DEV_IOBOX_UNKNOWN;

    if(inout_status == status) {
        return;
    }   
    
    status = inout_status;
    SPUI_TRACE(1, "%s", (SNDP_DEV_IOBOX_IN == inout_status) ? "IN_BOX" : "OUT_BOX");

    sndp_delay_exec_stop((uint32_t)sndp_ui_inbox_role_switch);
    sndp_delay_exec_stop((uint32_t)sndp_ui_outbox_role_switch);
    
    if(inout_status == SNDP_DEV_IOBOX_IN) {
        sndp_dev_wear_disable_detection();
        
        bta_tws_box_event_entry(BTA_TWS_DOCK);

        if(sndp_anc_is_on()) {
            sndp_anc_mode_set_locally(SNDP_ANC_MODE_OFF);
        } 

        sndp_delay_exec_start(300, (uint32_t)sndp_ui_inbox_role_switch, 0, 0, 0);

        sndp_delay_exec_start(100, (uint32_t)sndp_dev_io_pmu_check_cover, 0, 0, 0);
    
    } else {
        bta_tws_box_event_entry(BTA_TWS_UNDOCK);
        sndp_dev_wear_enable_detection();
        //spif_wear_detection_exec_calibration_self_calib();

        sndp_delay_exec_start(300, (uint32_t)sndp_ui_outbox_role_switch, 0, 0, 0);
    }
}

void sndp_ui_gesture_1click_hdlr(bool remote)
{
    SPUI_TRACE(0, "remote=%d", remote);
    
    
}

void sndp_ui_gesture_2click_hdlr(bool remote)
{
    SPUI_TRACE(0, "remote=%d", remote);
    
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {

    } else if(sndp_call_is_active()) {
        if(sndp_call_is_threeway_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_THREEWAY_HOLD_ANSWER);
        } else if(sndp_call_is_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_ANSWER);
        } else if(sndp_call_is_threeway_calling()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_HANGUP);
        } else {
            sndp_call_ctrl(SNDP_CALL_CTRL_HANGUP);
        }
        
    } else {
        if(sndp_music_is_playing()) {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PAUSE);
        } else {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PLAY);
        }
    }
    
}

void sndp_ui_gesture_3click_hdlr(bool remote)
{
    SPUI_TRACE(0, "remote=%d", remote);
    
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {
        if(sndp_dev_is_left_earphone()) {
            sndp_ui_anc_switch();
        } else {
            sndp_ui_working_mode_switch();
        }
        
    } else if(sndp_call_is_active()) {
        if(sndp_call_is_threeway_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_THREEWAY_REJECT);
        } else if(sndp_call_is_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_REJECT);
        } 
        
    } else {
#if 0
        if(sndp_dev_is_left_earphone()) {
            sndp_ui_anc_switch();
        } else {
            sndp_ui_working_mode_switch();
        }        
#else
        sndp_ui_anc_switch();
#endif
    }
    
}

#if defined(__SNDP_SLEEP_APP__)
#if defined(__SNDP_GESTURE_MAP__)
void sndp_function_play_pause(void) {
    // 实现播放/暂停功能
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {

    } else if(sndp_call_is_active()) {
        if(sndp_call_is_threeway_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_THREEWAY_HOLD_ANSWER);
        } else if(sndp_call_is_threeway_calling()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_HANGUP);
        } else if(sndp_call_is_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_ANSWER);
        } else {
            sndp_call_ctrl(SNDP_CALL_CTRL_HANGUP);
        }
        
    } else {
        if(sndp_music_is_playing()) {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PAUSE);
        } else {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PLAY);
        }
    }
}

void sndp_function_next_track(void) 
{
    // 下一曲功能
    if(sndp_music_is_playing()) {
        sndp_music_ctrl(SNDP_MUSIC_CTRL_FORWARD);
    }
}

void sndp_function_prev_track(void) 
{
    // 上一曲功能
    if(sndp_music_is_playing()) {
        sndp_music_ctrl(SNDP_MUSIC_CTRL_BACKWARD);
    }
}

void sndp_function_voice_assistant(void) 
{
    // 唤醒语音助手
    sndp_wakeup_voice_assistant(true);

}
 
void sndp_function_anc_mode_switch(void) 
{
    // ANC模式切换
    if(sndp_dev_is_working_mode(SNDP_DEV_WORKING_MODE_SLEEP)) {
        if(sndp_dev_is_left_earphone()) {
            sndp_ui_anc_switch();
        } else {
            sndp_ui_working_mode_switch();
        }
        
    } else if(sndp_call_is_active()) {
        if(sndp_call_is_threeway_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_THREEWAY_REJECT);
        } else if(sndp_call_is_incoming()) {
            sndp_call_ctrl(SNDP_CALL_CTRL_REJECT);
        } 
        
    } else {
        if(sndp_dev_is_left_earphone()) {
            sndp_ui_anc_switch();
        } else {
            sndp_ui_working_mode_switch();
        }
    }
}

static function_callback_t sndp_ui_gesture_func_table[SNDP_FUNC_MAX] = {
    sndp_function_play_pause,        
    sndp_function_next_track,       
    sndp_function_prev_track,       
    sndp_function_voice_assistant,
    sndp_function_anc_mode_switch,  
};
#endif 
#endif
/**
* 本地处理手势事件
*/
void sndp_ui_gesture_event_local_hdlr(sndp_dev_gesture_event_e gesture_event)
{
    SPUI_TRACE(0, "event=%d", gesture_event);

#if defined(__SNDP_SLEEP_APP__) && defined(__SNDP_GESTURE_MAP__)
    if(!sndp_dev_get_gesture_onoff(false)){
        SPUI_TRACE(0, "gesture detection is off, rtn");
        return;
    }
    sndp_dev_gesture_type_t dev_gesture = SNDP_DEV_GESTURE_MAX;
    switch (gesture_event)
    {
    case SNDP_DEV_GESTURE_EVENT_1_CLICK:
        dev_gesture = SNDP_DEV_GESTURE_CLICK;
        break;
    case SNDP_DEV_GESTURE_EVENT_2_CLICK:
        dev_gesture = SNDP_DEV_GESTURE_DOUBLE_CLICK;
        break;
    case SNDP_DEV_GESTURE_EVENT_3_CLICK:
        dev_gesture = SNDP_DEV_GESTURE_TRIPLE_CLICK;
        break;
    case SNDP_DEV_GESTURE_EVENT_LONG_PRESS:
        dev_gesture = SNDP_DEV_GESTURE_LONG_PRESS;
        break;
    default:
        break;
    }
    sndp_dev_gesture_mapper_handle_gesture(dev_gesture);
#else
    //user gesture mapper, cancel this switch!!!!!!!
    switch(gesture_event) {
        case SNDP_DEV_GESTURE_EVENT_1_CLICK:
            sndp_ui_gesture_1click_hdlr(false);
            break;
        case SNDP_DEV_GESTURE_EVENT_2_CLICK:
            sndp_ui_gesture_2click_hdlr(false);
            break;
        case SNDP_DEV_GESTURE_EVENT_3_CLICK:
            sndp_ui_gesture_3click_hdlr(false);
            break;
        default:
            break;
    }
#endif    
}

/**
* 手势事件发送到对耳处理
*/
void sndp_ui_gesture_event_peer_hdlr(sndp_dev_gesture_event_e gesture_event)
{
    SPUI_TRACE(0, "event=%d", gesture_event);
    
    switch(gesture_event) {
        case SNDP_DEV_GESTURE_EVENT_1_CLICK:
            sndp_ui_gesture_1click_hdlr(true);
            break;
        case SNDP_DEV_GESTURE_EVENT_2_CLICK:
            sndp_ui_gesture_2click_hdlr(true);
            break;
        case SNDP_DEV_GESTURE_EVENT_3_CLICK:
            sndp_ui_gesture_3click_hdlr(true);
            break;
        default:
            break;
    }
}

void sndp_ui_gesture_event_recv_from_peer(sndp_dev_gesture_event_e gesture_event)
{
    sndp_ui_gesture_event_peer_hdlr(gesture_event);
}

void sndp_ui_gesture_event_sent_to_peer(sndp_dev_gesture_event_e gesture_event)
{
#if defined(__SNDP_COMM_MGR__)    
    sndp_comm_cmd_send_lr_sync_gesture(gesture_event);
#endif
}

static void sndp_ui_gesture_event_generated(sndp_dev_gesture_event_e gesture_event)
{
    bool peerProcess = false;

    SPUI_TRACE(0, "event=%d", gesture_event);

    if(!sndp_dev_wear_is_worn(false)) {
        SPUI_TRACE(0, "not worn, rtn");
        return;
    }
    
    if(peerProcess) {
        sndp_ui_gesture_event_sent_to_peer(gesture_event);
    } else {
        sndp_ui_gesture_event_local_hdlr(gesture_event);
    }
}


//---------------------------------------- charger_plug ctrl --------------------------------------------
void sndp_ui_charger_plug_status_changed(sndp_dev_charger_plug_e plug_status)
{
    SPUI_TRACE(0, "%s", (SNDP_DEV_CHARGER_PLUG_IN == plug_status) ? "PLUG_IN" : "PLUG_OUT");

    if(SNDP_DEV_CHARGER_PLUG_OUT == plug_status) {
        sndp_delay_exec_start(100, (uint32_t)sndp_dev_bat_pwr_measure, 0, 0, 0);
    }

    if(SNDP_DEV_CHARGER_PLUG_UNKNOWN != plug_status) {
        sndp_delay_exec_start(100, (uint32_t)sndp_dev_io_pmu_check_cover, 0, 0, 0);
    }
}

//---------------------------------------- bat ctrl --------------------------------------------
static void sndp_ui_bat_lr_sync(void)
{
    
    static uint8_t last_sync_percent = 0xFF;
    sndp_dev_bat_info_s curr;

    sndp_dev_get_bat_info(false, &curr);

    SPUI_TRACE(0, "last=%d, curr valid=%d, per=%d", 
            last_sync_percent,
            curr.valid,
            curr.bat_per
            );
    
    if(curr.valid && sndp_is_tws_link_connected()) {
#if defined(__SNDP_COMM_MGR__)         
        if((last_sync_percent == 0xFF) || (last_sync_percent != curr.bat_per)) {
            sndp_comm_cmd_send_lr_sync_bat_info();
        }
#endif        
        last_sync_percent = curr.bat_per;
    }
}

void sndp_ui_bat_pwr_measure_callback(sndp_dev_bat_info_s old_bat_info, sndp_dev_bat_info_s new_bat_info)
{
    SPUI_TRACE(0, "new valid=%d, per=%d", new_bat_info.valid,new_bat_info.bat_per);
    sndp_ui_ctx.lowpwr_check_enable = true;
    sndp_ui_bat_lr_sync();
}


//---------------------------------------- key ctrl --------------------------------------------
#if defined(__SNDP_KEY_TEST__)
static void sndp_ui_pwr_key_hdlr(APP_KEY_STATUS *status, void *param)
{
    SPUI_TRACE(0, "event=%d", status->event);
    
    switch(status->event) {
        case APP_KEY_EVENT_CLICK:
           sndp_start_freeman_pairing();
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            sndp_ui_working_mode_switch();
            break;
        case APP_KEY_EVENT_TRIPLECLICK:
            break;
        case APP_KEY_EVENT_LONGPRESS:
            sndp_app_shutdown(SNDP_SHUTDOWN_REASON_LONGPRESS);
            break;
        case APP_KEY_EVENT_LONGLONGPRESS:
            break;
        default:
            break;
    }

}

static void sndp_ui_fn1_key_hdlr(APP_KEY_STATUS *status, void *param)
{
    SPUI_TRACE(0, "event=%d", status->event);
    
    switch(status->event) {
        case APP_KEY_EVENT_CLICK:
#if 0
            sndp_ui_anc_switch();
#endif
#if 0            
            sndp_start_freeman_pairing();
#endif            
#if 0
            {
                uint8_t data[] = {0xa1, 0xb2, 0xc3, 0xd4};
                sndp_comm_ble_send_data(data, 4);
            }
#endif

#if defined(__SNDP_HEART_RATE_MGR__)            
            //sndp_hr_mearsuring_start(1, 0);
            sndp_sleep_analysis_start(0);
#endif
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
#if defined(__SNDP_HEART_RATE_MGR__)            
            //sndp_hr_mearsuring_stop();
            sndp_sleep_analysis_stop();
#endif
            break;
        case APP_KEY_EVENT_LONGPRESS:
            break;
        default:
            break;
    }
}

static void sndp_ui_fn2_key_hdlr(APP_KEY_STATUS *status, void *param)
{
    SPUI_TRACE(0, "event=%d", status->event);
    
    switch(status->event) {
        case APP_KEY_EVENT_CLICK:
            sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_OPENED);
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            sndp_dev_cover_status_changed_handler(SNDP_DEV_COVER_COLSED);
            break;
        case APP_KEY_EVENT_LONGPRESS:
            sndp_dev_wear_status_changed_handler(SNDP_DEV_WEAR_ON);
            break;
        default:
            break;
    }

}

static void sndp_ui_fn3_key_hdlr(APP_KEY_STATUS *status, void *param)
{
    SPUI_TRACE(0, "event=%d", status->event);
    
    switch(status->event) {
        case APP_KEY_EVENT_CLICK:
            sndp_dev_iobox_status_changed_handler(SNDP_DEV_IOBOX_OUT);
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            sndp_dev_iobox_status_changed_handler(SNDP_DEV_IOBOX_IN);
            break;
        case APP_KEY_EVENT_LONGPRESS:
            sndp_dev_wear_status_changed_handler(SNDP_DEV_WEAR_OFF);
            break;
        default:
            break;
    }
}

static void sndp_ui_fn4_key_hdlr(APP_KEY_STATUS *status, void *param)
{
    SPUI_TRACE(0, "event=%d", status->event);
    
    switch(status->event) {
        case APP_KEY_EVENT_CLICK:
            sndp_ui_gesture_event_generated(SNDP_DEV_GESTURE_EVENT_1_CLICK);
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            sndp_ui_gesture_event_generated(SNDP_DEV_GESTURE_EVENT_2_CLICK);
            break;
        case APP_KEY_EVENT_TRIPLECLICK:
            sndp_ui_gesture_event_generated(SNDP_DEV_GESTURE_EVENT_3_CLICK);
            break;
        case APP_KEY_EVENT_LONGPRESS:
            break;
        default:
            break;
    }
}

static const APP_KEY_HANDLE  sndp_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR, APP_KEY_EVENT_CLICK         },  "sndp key",  sndp_ui_pwr_key_hdlr, NULL},
    {{APP_KEY_CODE_PWR, APP_KEY_EVENT_DOUBLECLICK   },  "sndp key",  sndp_ui_pwr_key_hdlr, NULL},
    {{APP_KEY_CODE_PWR, APP_KEY_EVENT_TRIPLECLICK   },  "sndp key",  sndp_ui_pwr_key_hdlr, NULL},
    {{APP_KEY_CODE_PWR, APP_KEY_EVENT_LONGPRESS     },  "sndp key",  sndp_ui_pwr_key_hdlr, NULL},    
    {{APP_KEY_CODE_PWR, APP_KEY_EVENT_LONGLONGPRESS },  "sndp key",  sndp_ui_pwr_key_hdlr, NULL}, 

    {{APP_KEY_CODE_FN1, APP_KEY_EVENT_CLICK         },  "sndp key",  sndp_ui_fn1_key_hdlr, NULL},
    {{APP_KEY_CODE_FN1, APP_KEY_EVENT_DOUBLECLICK   },  "sndp key",  sndp_ui_fn1_key_hdlr, NULL},
    {{APP_KEY_CODE_FN1, APP_KEY_EVENT_LONGPRESS     },  "sndp key",  sndp_ui_fn1_key_hdlr, NULL},

    {{APP_KEY_CODE_FN2, APP_KEY_EVENT_CLICK         },  "sndp key",  sndp_ui_fn2_key_hdlr, NULL},
    {{APP_KEY_CODE_FN2, APP_KEY_EVENT_DOUBLECLICK   },  "sndp key",  sndp_ui_fn2_key_hdlr, NULL},
    {{APP_KEY_CODE_FN2, APP_KEY_EVENT_LONGPRESS     },  "sndp key",  sndp_ui_fn2_key_hdlr, NULL},
        
    {{APP_KEY_CODE_FN3, APP_KEY_EVENT_CLICK         },  "sndp key",  sndp_ui_fn3_key_hdlr, NULL},
    {{APP_KEY_CODE_FN3, APP_KEY_EVENT_DOUBLECLICK   },  "sndp key",  sndp_ui_fn3_key_hdlr, NULL},
    {{APP_KEY_CODE_FN3, APP_KEY_EVENT_LONGPRESS     },  "sndp key",  sndp_ui_fn3_key_hdlr, NULL},

    {{APP_KEY_CODE_FN4, APP_KEY_EVENT_CLICK         },  "sndp key",  sndp_ui_fn4_key_hdlr, NULL},
    {{APP_KEY_CODE_FN4, APP_KEY_EVENT_DOUBLECLICK   },  "sndp key",  sndp_ui_fn4_key_hdlr, NULL},
    {{APP_KEY_CODE_FN4, APP_KEY_EVENT_TRIPLECLICK   },  "sndp key",  sndp_ui_fn4_key_hdlr, NULL},
    {{APP_KEY_CODE_FN4, APP_KEY_EVENT_LONGPRESS     },  "sndp key",  sndp_ui_fn4_key_hdlr, NULL},
};

void sndp_ui_key_init(void)
{
    uint8_t i = 0;
    
    SPUI_TRACE(1, "registe key cnt:%d", ARRAY_SIZE(sndp_key_handle_cfg));
    app_key_handle_clear();
    for (i = 0; i < ARRAY_SIZE(sndp_key_handle_cfg); i++) {
        app_key_handle_registration(&sndp_key_handle_cfg[i]);
    }
}
#endif

//---------------------------------------- prompt ctrl --------------------------------------------
static bool sndp_ui_prompt_sta = false;

bool sndp_ui_is_prompt_playing(void)
{
    return sndp_ui_prompt_sta;
}

int sndp_ui_prompt_finish_cb(int aud_id)
{
	SPUI_TRACE(1, "aud_id=%02x", aud_id);
    sndp_ui_prompt_sta = false;

    if(aud_id == AUD_ID_POWER_ON) {

    } else if(aud_id == AUD_ID_POWER_OFF) {

    } 

	return 0;
}

int sndp_ui_prompt_start_cb(int aud_id)
{
	SPUI_TRACE(1, "aud_id=%02x", aud_id);
    sndp_ui_prompt_sta = true;
    
	return 0;
}

//---------------------------------------- bat and temperature ctrl --------------------------------------------

static void sndp_ui_bat_charging_check(void)
{
	if(sndp_dev_charger_is_plugin(false)) {
		sndp_ui_ctx.charging_time += SPUI_TIME_TODO_INTERVAL;
		sndp_ui_ctx.close_discharge_time = 0;

		if(sndp_ui_ctx.charging_time >= SPUI_CHARGING_TIME_MAX) {
			sndp_call_func_in_app_thread((uint32_t)sndp_app_shutdown, 0, SNDP_SHUTDOWN_REASON_CHARGING_TIMEOUT, 0);
			return;
		}
		
		if(sndp_dev_charger_is_charging_full(false)) {
			sndp_ui_ctx.charging_full_time += SPUI_TIME_TODO_INTERVAL;
			SPUI_TRACE(0, "chg_full_t = %d.", sndp_ui_ctx.charging_full_time);
			if(sndp_ui_ctx.charging_full_time >= SPUI_CHARGING_FULL_TIME_MAX) {
                if(sndp_dev_cover_is_closed(false)) { 
    				sndp_call_func_in_app_thread((uint32_t)sndp_app_shutdown, SNDP_SHUTDOWN_REASON_CHARGING_FULL, 0, 0);
                }
				return;
			}
		} 

		//SPUI_TRACE(2, "charging_time=%d, charging_full_time=%d", sndp_ui_ctx.charging_time, sndp_ui_ctx.charging_full_time);
		
		sndp_dev_charger_set_charging_current();
	}else {
		if(sndp_dev_cover_is_closed(false)) {
			sndp_ui_ctx.close_discharge_time += 10;
			SPUI_TRACE(0, "close_dischg_t = %d.", sndp_ui_ctx.close_discharge_time);
			if(sndp_ui_ctx.close_discharge_time > SPUI_CLOSE_DISCHARGE_MAX) {
				sndp_call_func_in_app_thread((uint32_t)sndp_app_shutdown, SNDP_SHUTDOWN_REASON_CLOSE_DISCHARGE, 0, 0);
				return;
			}
		} else {
            sndp_ui_ctx.close_discharge_time = 0;
		}
	}

}

static void sndp_ui_bat_lowpwr_check(void)
{
	if(sndp_dev_charger_is_plugin(false)) {
		return;
	} 

	if(!sndp_ui_ctx.lowpwr_check_enable) {
		SPUI_TRACE(0, "%d, rtn", __LINE__);
		return;
	}
		
	/* Low powr warning check */
	if(sndp_dev_get_bat_percentage(false) < SPUI_LOWPWR_WARNING_PERCENTAGE) {
		sndp_ui_ctx.lowpwr_warning_last_time += SPUI_TIME_TODO_INTERVAL;
		SPUI_TRACE(0, "cnt=%d, time=%d",
		    sndp_ui_ctx.lowpwr_warning_cnt,
		    sndp_ui_ctx.lowpwr_warning_last_time);

        if(sndp_ui_ctx.lowpwr_warning_cnt == 0) {
            sndp_ui_ctx.lowpwr_warning_last_time = 0;
            sndp_ui_ctx.lowpwr_warning_cnt++;
            media_PlayAudio(AUD_ID_BT_CHARGE_PLEASE, 0);
            
        } else if(sndp_ui_ctx.lowpwr_warning_last_time >= SPUI_LOWPWR_WARNING_INTERVAL) {
			sndp_ui_ctx.lowpwr_warning_last_time = 0;
			sndp_ui_ctx.lowpwr_warning_cnt++;

#if 0
			if(sndp_ui_ctx.lowpwr_warning_cnt < SPUI_LOWPWR_WARNING_CNT_MAX) {
				media_PlayAudio(AUD_ID_BT_CHARGE_PLEASE, 0);
			}
#else
            media_PlayAudio(AUD_ID_BT_CHARGE_PLEASE, 0);
#endif
		}
	} else {
		sndp_ui_ctx.lowpwr_warning_last_time = 0;
		sndp_ui_ctx.lowpwr_warning_cnt = 0;
	}


	/* Low powr shutdown check */
	if(sndp_dev_get_bat_percentage(false) <= SPUI_LOWPWR_SHUTDOWN_PERCENTAGE) {

		sndp_ui_ctx.lowpwr_shutdown_cnt++;
		SPUI_TRACE(1, "lowpwr_shutdown_cnt=%d", sndp_ui_ctx.lowpwr_shutdown_cnt);
		
		if(sndp_ui_ctx.lowpwr_shutdown_cnt >= SPUI_LOWPWR_SHUTDOWN_CHECK_CNT) {
            //media_PlayAudio(AUD_ID_POWER_OFF, 0);
            sndp_app_shutdown(SNDP_SHUTDOWN_REASON_LOWPWR);
		}
	} else {
		sndp_ui_ctx.lowpwr_shutdown_cnt = 0;

	}

}

static void sndp_ui_working_temperature_check(void)
{
	if(!sndp_ui_ctx.temperature_check_enable)
		return;

	int16_t temperature = sndp_dev_temperature_get_value(false);
	SPUI_TRACE(1, "T=%d", temperature);
	
	if(temperature > SPUI_WORKING_TEMPERATURE_MAX
		|| temperature < SPUI_WORKING_TEMPERATURE_MIN ) {

		sndp_ui_ctx.temperature_exp_shutdown_time += SPUI_TIME_TODO_INTERVAL;

		if(sndp_ui_ctx.temperature_exp_shutdown_time >= SPUI_TEMPERATURE_ABNORMAL_DURATION) {
			sndp_call_func_in_app_thread((uint32_t)sndp_app_shutdown, SNDP_SHUTDOWN_REASON_TEMPERATURE, 0, 0);
		}
	} else {
		sndp_ui_ctx.temperature_exp_shutdown_time = 0;
	}
}

static void sndp_ui_temperature_measure_callback(int16_t temperature)
{
	SPUI_TRACE(1, "T=%d", temperature);
	sndp_ui_ctx.temperature_check_enable = true;
    sndp_ui_working_temperature_check();
}


//---------------------------------------- bt ctrl --------------------------------------------
POSSIBLY_UNUSED static void sndp_ui_bt_conn_status_changed(sndp_bt_conn_status_e conn_status, uint8_t reason)
{
    SPUI_TRACE(0, "conn_sta=%d, reason=%02X", conn_status, reason);
    
	switch(conn_status) {
		case SNDP_BT_CONN_STATUS_MOBILE_DISCONNECTED:
			
#ifdef MEDIA_PLAYER_SUPPORT            
            media_PlayAudio(AUD_ID_BT_DIS_CONNECT, 0);
#endif
            SPUI_TRACE(0, "mobile con_cnt=%d, con_sta=%d", 
                    sndp_get_connected_mobile_count(), 
                    sndp_is_master_mobile_link_connected());

			if(reason == 0x13) { 
			    //REMOTE_USER_TERMINATED         
				sndp_call_func_in_app_thread((uint32_t)sndp_enter_mobile_pairing_after_tws_connected, 0, 0, 0);
		
			} else { 
				//other reason, shutdown time is set to 15 minutes
                sndp_enter_mobile_reconnect();
			}
			break;
            
		case SNDP_BT_CONN_STATUS_MOBILE_CONNECTED:
			
#ifdef MEDIA_PLAYER_SUPPORT            
            media_PlayAudio(AUD_ID_BT_CONNECTED, 0);
#endif			
			sndp_mobile_pairing_sccessful();
            sndp_mobile_reconnect_sccessful();
			break;
	
		case SNDP_BT_CONN_STATUS_IBRT_DISCONNECTED:
            if(reason == 0x13) { 
			    //REMOTE_USER_TERMINATED         
				sndp_call_func_in_app_thread((uint32_t)sndp_enter_mobile_pairing_after_tws_connected, 0, 0, 0);
		
			} else { 
				//other reason, shutdown time is set to 15 minutes
                sndp_enter_mobile_reconnect();
			}	
			break;
        
		case SNDP_BT_CONN_STATUS_IBRT_CONNECTED:
            sndp_mobile_pairing_sccessful();
            sndp_mobile_reconnect_sccessful();
			break;

		case SNDP_BT_CONN_STATUS_TWS_DISCONNECTED:
			sndp_update_audio_channel(false);
			sndp_dev_clear_device_info(true);
			break;
            
		case SNDP_BT_CONN_STATUS_TWS_CONNECTED:
			sndp_update_audio_channel(true);
            sndp_ui_all_status_sync_send();
			break;

		case SNDP_BT_CONN_STATUS_A2DP_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_A2DP_CONNECTED:
			break;

		case SNDP_BT_CONN_STATUS_AVRCP_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_AVRCP_CONNECTED:
			break;
            
		case SNDP_BT_CONN_AVRCP_PLAYBACK_STATUS_CHANGED:
            if(reason == 1) {
                sndp_ui_ctx.wear_play_music_allowed = true;
            } else if(reason == 2) {
                sndp_ui_ctx.wear_play_music_allowed = false;
            }         
			break;
		case SNDP_BT_CONN_STATUS_HFP_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_HFP_CONNECTED:
			break;

		case SNDP_BT_CONN_STATUS_BLE_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_BLE_CONNECTED:
			break;

		case SNDP_BT_CONN_STATUS_SPP_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_SPP_CONNECTED:
			break;

		case SNDP_BT_CONN_STATUS_BES_AUD_DISCONNECTED:
			break;
            
		case SNDP_BT_CONN_STATUS_BES_AUD_CONNECTED:
			break;

    case SNDP_BT_CONN_STATUS_HFP_CALLSETUP_IND:
			break;
            
		case SNDP_BT_CONN_STATUS_HFP_RING_IND:
			break;

		case SNDP_BT_CONN_STATUS_HFP_CALL_IND:
			break;
            
		default:
			break;

	}
}

static void sndp_ui_bt_event_exec_after_power_on(void)
{
    if(sndp_dev_cover_is_closed(false)) {
        SPUI_TRACE(0, "%d, rtn", __LINE__);
        return;
    }
    
    if(sndp_ui_pairing_type_is(SNDP_UI_PAIRING_FREEMAN)) {
        SPUI_TRACE(0, "force freeman pairing");

        sndp_ui_pairing_type_set(SNDP_UI_PAIRING_NONE);
        sndp_enter_freeman_pairing();
        sndp_delay_exec_start(100, (uint32_t)media_PlayAudio, AUD_ID_BT_PAIRING, 0, 0);
       
    } else if (sndp_ui_pairing_type_is(SNDP_UI_PAIRING_TWS)) {
        SPUI_TRACE(0, "force tws pairing");
        
        sndp_ui_pairing_type_set(SNDP_UI_PAIRING_NONE);
        sndp_enter_mobile_pairing_after_tws_connected();
        sndp_delay_exec_start(300, (uint32_t)media_PlayAudio, AUD_ID_BT_PAIRING, 0, 0);
        
    } else {
        if(sndp_is_left_right_bound()) {
            if(sndp_get_mobile_pairing_count() > 0) {
                SPUI_TRACE(0, "mobile reconnecting");
                
                sndp_enter_mobile_reconnect();
            
            } else {
                SPUI_TRACE(0, "no paired record, tws pairing");
                
                //sndp_delay_exec_start(300, (uint32_t)media_PlayAudio, AUD_ID_BT_PAIRING, 0, 0);
                sndp_enter_mobile_pairing_after_tws_connected();
            }
        } else {
            SPUI_TRACE(0, "the left and right not bound.");
        }

    }
}


//---------------------------------------- other ctrl --------------------------------------------

static void sndp_ui_dev_status_print(void)
{
	uint8_t *fw_ver = sndp_dev_get_fw_ver(false);   
	const uint8_t *str_wear_on      = (const uint8_t *)"  on";
	const uint8_t *str_wear_off     = (const uint8_t *)" off";
    const uint8_t *str_cover_open   = (const uint8_t *)"open";
	const uint8_t *str_cover_close  = (const uint8_t *)"clos";
    const uint8_t *str_iobox_in     = (const uint8_t *)"  in";
	const uint8_t *str_iobox_out    = (const uint8_t *)" out";
    uint8_t *role;

    if(sndp_is_tws_master_mode())
        role = (uint8_t *)"M";
    else if(sndp_is_tws_slave_mode())
        role = (uint8_t *)"S";
    else
        role = (uint8_t *)"U";
	
	SNDP_TRACE_IMM(0, "\n");
	SNDP_TRACE(0, "-------------------------------------------------------------");
	SNDP_TRACE(3, "[con sta] tws:%4d, mobi:%4d, ibrt:%4d, role:%s", sndp_is_tws_link_connected(), sndp_is_master_mobile_link_connected(), sndp_is_slave_ibrt_link_connected(), role);
	if(sndp_is_besaud_connected()) {
        SNDP_TRACE(2, "[chgplug] loc:%4d, peer:%4d", sndp_dev_charger_is_plugin(false), sndp_dev_charger_is_plugin(true));
		SNDP_TRACE(2, "[chg sta] loc:%4d, peer:%4d", sndp_dev_charger_get_charging_status(false), sndp_dev_charger_get_charging_status(true));
		SNDP_TRACE(2, "[percent] loc:%4d, peer:%4d", sndp_dev_get_bat_percentage(false), sndp_dev_get_bat_percentage(true));
		SNDP_TRACE(2, "[voltage] loc:%4d, peer:%4d", sndp_dev_get_bat_voltage(false), sndp_dev_get_bat_voltage(true));       
		SNDP_TRACE(2, "[temp   ] loc:%4d, peer:%4d", sndp_dev_temperature_get_value(false), sndp_dev_temperature_get_value(true));
		SNDP_TRACE(2, "[wear   ] loc:%4s, peer:%4s", sndp_dev_wear_is_worn(false) ? str_wear_on : str_wear_off, sndp_dev_wear_is_worn(true) ? str_wear_on : str_wear_off);
        SNDP_TRACE(2, "[cover  ] loc:%4s, peer:%4s", sndp_dev_cover_is_opened(false) ? str_cover_open : str_cover_close, sndp_dev_cover_is_opened(true) ? str_cover_open : str_cover_close);
        SNDP_TRACE(2, "[iobox  ] loc:%4s, peer:%4s", sndp_dev_iobox_is_in_box(false) ? str_iobox_in : str_iobox_out, sndp_dev_iobox_is_in_box(true) ? str_iobox_in : str_iobox_out);
	} else {
        SNDP_TRACE(1, "[chgplug] loc:%4d, peer:----", sndp_dev_charger_is_plugin(false));
		SNDP_TRACE(1, "[chg sta] loc:%4d, peer:----", sndp_dev_charger_get_charging_status(false));
		SNDP_TRACE(1, "[percent] loc:%4d, peer:----", sndp_dev_get_bat_percentage(false));
		SNDP_TRACE(1, "[voltage] loc:%4d, peer:----", sndp_dev_get_bat_voltage(false));     
		SNDP_TRACE(1, "[temp   ] loc:%4d, peer:----", sndp_dev_temperature_get_value(false));
		SNDP_TRACE(1, "[wear   ] loc:%4s, peer:----", sndp_dev_wear_is_worn(false) ? str_wear_on : str_wear_off);
        SNDP_TRACE(1, "[cover  ] loc:%4s, peer:----", sndp_dev_cover_is_opened(false) ? str_cover_open : str_cover_close);
        SNDP_TRACE(1, "[iobox  ] loc:%4s, peer:----", sndp_dev_iobox_is_in_box(false) ? str_iobox_in : str_iobox_out);
	}
	SNDP_TRACE(4, "[fw ver ] %d.%d.%d.%d", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
	SNDP_TRACE(0, "-------------------------------------------------------------");
	SNDP_TRACE_IMM(0, "\n");
}

static void sndp_ui_all_status_sync_send(void)
{
    sndp_ui_all_dev_sta_s all_dev_sta;
    
	if(sndp_is_besaud_connected()) {
        SNDP_TRACE_IMM(0, "\n");
        
#if defined(__SNDP_COMM_MGR__)
        all_dev_sta.cover_sta = sndp_dev_cover_get_status(false);
        all_dev_sta.iobox_sta = sndp_dev_iobox_get_status(false);
        all_dev_sta.wear_sta = sndp_dev_wear_get_status(false);
        sndp_dev_get_bat_info(false, &all_dev_sta.bat_info);
        sndp_comm_cmd_send_lr_sync_all_dev_status((uint8_t *)&all_dev_sta, sizeof(sndp_ui_all_dev_sta_s));
#endif        
	}
}

void sndp_ui_all_status_sync_recv(uint8_t *data, uint16_t len)
{
    sndp_ui_all_dev_sta_s all_dev_sta;

    if(len == sizeof(sndp_ui_all_dev_sta_s)) {
        memcpy(&all_dev_sta, data, len);
        sndp_dev_cover_set_status(true, all_dev_sta.cover_sta);
        sndp_dev_iobox_set_status(true, all_dev_sta.iobox_sta);
        sndp_dev_wear_set_status(true, all_dev_sta.wear_sta);
        sndp_dev_set_bat_info(true, all_dev_sta.bat_info);
    }
}

void sndp_ui_timing_to_do(void)
{
	sndp_ui_dev_status_print();

    sndp_delay_exec_start(100, (uint32_t)sndp_dev_bat_pwr_measure, 0, 0, 0);
 	sndp_delay_exec_start(300, (uint32_t)sndp_dev_temperature_measure, 0, 0, 0);
    sndp_ui_bat_charging_check();
    sndp_ui_bat_lowpwr_check();

	//sndp_connect_status_print(); //for test
}

static void sndp_ui_check_dev_initial_status(void)
{
	//SPUI_TRACE_ENTER();

	sndp_dev_charger_plug_set_status_changed_callback(sndp_ui_charger_plug_status_changed);
    sndp_dev_bat_pwr_set_measure_callback(sndp_ui_bat_pwr_measure_callback);
    sndp_dev_temperature_set_measure_callback(sndp_ui_temperature_measure_callback);
    sndp_dev_cover_set_status_changed_callback(sndp_ui_cover_status_changed);
    sndp_dev_iobox_set_status_changed_callback(sndp_ui_iobox_status_changed);
    sndp_dev_wear_set_status_changed_callback(sndp_ui_wear_status_changed);
    sndp_dev_gesture_set_event_callback(sndp_ui_gesture_event_generated);
#if defined(__SNDP_SLEEP_APP__)
#if defined(__SNDP_GESTURE_MAP__)
    sndp_dev_register_gesture_funcs(sndp_ui_gesture_func_table);
    sndp_dev_gesture_mapper_init();
#endif
#endif
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_charger_plug_check_curr_status, 0, 0, 0);
    sndp_call_func_in_dev_thread((uint32_t)sndp_dev_charger_check_curr_status, 0, 0, 0);
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_bat_pwr_measure, 0, 0, 0);
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_temperature_measure, 0, 0, 0);
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_cover_check_curr_status, 0, 0, 0);
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_iobox_check_curr_status, 0, 0, 0);
	sndp_call_func_in_dev_thread((uint32_t)sndp_dev_wear_check_curr_status, 0, 0, 0);
}

void sndp_ui_init_pre(void)
{
	SPUI_TRACE_ENTER();
    
	memset(&sndp_ui_ctx, 0, sizeof(sndp_ui_ctx));
    sndp_ui_ctx.anc_status = SNDP_ANC_STA_OFF;
    sndp_ui_ctx.anc_mode = SNDP_ANC_MODE_1; //need init from flash.
        
    sndp_set_bt_conn_status_changed_callback(sndp_ui_bt_conn_status_changed);
	app_prompt_start_callback_register(sndp_ui_prompt_start_cb);
	app_prompt_finish_callback_register(sndp_ui_prompt_finish_cb);
}

void sndp_ui_init(void)
{
	SPUI_TRACE_ENTER();
    
#if defined(__SNDP_HEART_RATE_MGR__)
    sndp_hr_app_init();
#endif

	sndp_ui_check_dev_initial_status();
    sndp_delay_exec_start(300, (uint32_t)sndp_ui_bt_event_exec_after_power_on, 0, 0, 0);

#if defined(__SNDP_AUDIO_TEST__)
    sndp_delay_exec_start(2000, (uint32_t)sndp_audio_test_switch, 0, 0, 0);
#endif    
}

#endif	/* __SNDP_UI__ */

