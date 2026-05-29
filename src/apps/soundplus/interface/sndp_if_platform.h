#ifndef __SNDP_IF_PLATFORM_H__
#define __SNDP_IF_PLATFORM_H__


#if defined(__SNDP_PROJ__)



#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SNDP_BT_CONN_STATUS_NONE,
	SNDP_BT_CONN_STATUS_MOBILE_DISCONNECTED 	= 1,
	SNDP_BT_CONN_STATUS_MOBILE_CONNECTED 		= 2,
	
	SNDP_BT_CONN_STATUS_IBRT_DISCONNECTED 		= 3,
	SNDP_BT_CONN_STATUS_IBRT_CONNECTED 			= 4,
	
	SNDP_BT_CONN_STATUS_TWS_DISCONNECTED 		= 5,
	SNDP_BT_CONN_STATUS_TWS_CONNECTED 			= 6,
	
	SNDP_BT_CONN_STATUS_A2DP_DISCONNECTED 		= 7,
	SNDP_BT_CONN_STATUS_A2DP_CONNECTED 			= 8,
	
	SNDP_BT_CONN_STATUS_AVRCP_DISCONNECTED 		= 9,
	SNDP_BT_CONN_STATUS_AVRCP_CONNECTED 		= 10,
	
	SNDP_BT_CONN_STATUS_HFP_DISCONNECTED 		= 11,
	SNDP_BT_CONN_STATUS_HFP_CONNECTED 			= 12,

	SNDP_BT_CONN_STATUS_BLE_DISCONNECTED 		= 13,
	SNDP_BT_CONN_STATUS_BLE_CONNECTED 			= 14,

	SNDP_BT_CONN_STATUS_SPP_DISCONNECTED 		= 15,
	SNDP_BT_CONN_STATUS_SPP_CONNECTED 			= 16,

	SNDP_BT_CONN_STATUS_BES_AUD_DISCONNECTED 	= 17,
	SNDP_BT_CONN_STATUS_BES_AUD_CONNECTED 		= 18,

    SNDP_BT_CONN_STATUS_HFP_CALLSETUP_IND 		= 19,
    SNDP_BT_CONN_STATUS_HFP_RING_IND 			= 20,
	SNDP_BT_CONN_STATUS_HFP_CALL_IND 			= 21,

	SNDP_BT_CONN_AVRCP_PLAYBACK_STATUS_CHANGED  = 22,
	SNDP_BT_CONN_ROLE_ROLE_CHANGED                     = 23,
	
} sndp_bt_conn_status_e;

typedef enum {
	SNDP_MUSIC_CTRL_STOP,
	SNDP_MUSIC_CTRL_PLAY,
	SNDP_MUSIC_CTRL_PAUSE,
	SNDP_MUSIC_CTRL_FORWARD,
	SNDP_MUSIC_CTRL_BACKWARD,
	SNDP_MUSIC_CTRL_VOLUME_INCREASE,
	SNDP_MUSIC_CTRL_VOLUME_DECREASE,
	SNDP_MUSIC_CTRL_VOLUME_SET,
	SNDP_MUSIC_CTRL_REWIND,
	SNDP_MUSIC_CTRL_FAST_FORWARD,
} sndp_music_ctrl_event_e;


typedef enum {
	SNDP_CALL_CTRL_REJECT,
	SNDP_CALL_CTRL_ANSWER,
	SNDP_CALL_CTRL_HANGUP,
	SNDP_CALL_CTRL_THREEWAY_REJECT,
	SNDP_CALL_CTRL_THREEWAY_HOLD_ANSWER,
	SNDP_CALL_CTRL_THREEWAY_HANGUP_ANSWER,
	SNDP_CALL_CTRL_THREEWAY_SWITCH,
	SNDP_CALL_CTRL_VOLUME_INCREASE,
	SNDP_CALL_CTRL_VOLUME_DECREASE,
	SNDP_CALL_CTRL_VOLUME_SET,
	SNDP_CALL_CTRL_MUTE,
	SNDP_CALL_CTRL_UNMUTE,
	SNDP_CALL_CTRL_TONE_SWITCH_TO_PHONE,
	SNDP_CALL_CTRL_TONE_SWITCH_TO_EARPHONE
	
} sndp_call_ctrl_event_e;

typedef enum {
	SNDP_ANC_MODE_OFF,
	SNDP_ANC_MODE_1,
	SNDP_ANC_MODE_2,
	SNDP_ANC_MODE_3,
	SNDP_ANC_MODE_4,
	SNDP_ANC_MODE_TRANSPARENT,
	
	SNDP_ANC_MODE_QTY,
} sndp_anc_mode_e;


#if defined(__SNDP_SLEEP_APP__)
typedef enum {
		SNDP_EQ_MODE_NORMAL = 0,
		SNDP_EQ_MODE_JAZZ,
		SNDP_EQ_MODE_ROCK,
		SNDP_EQ_MODE_CLASSIC,
		SNDP_EQ_MODE_RELAXED,
		SNDP_EQ_MODE_CUSTOM_MODE = 0x09, //用户自定义模式
		SNDP_EQ_MAX,
} sndp_eq_mode_e;
#endif



typedef enum {
    SNDP_PAIRING_NONE,
    SNDP_PAIRING_TWS,
    SNDP_PAIRING_FREEMAN,
    
} sndp_pairing_type_e;


typedef enum {
    SNDP_PAIR_STA_NONE       = 0,
    SNDP_PAIR_STA_PAIRING    = 1,
	SNDP_PAIR_STA_SUCCESS    = 2,
	SNDP_PAIR_STA_TIMEOUT    = 3,
	
} sndp_pairing_state_e;


typedef enum {
	SNDP_SHUTDOWN_REASON_NONE               = 0,
	SNDP_SHUTDOWN_REASON_LOWPWR             = 1,
	SNDP_SHUTDOWN_REASON_CHARGING_FULL      = 2,
	SNDP_SHUTDOWN_REASON_CHARGING_TIMEOUT   = 3,
	SNDP_SHUTDOWN_REASON_LONGPRESS          = 4,
	SNDP_SHUTDOWN_REASON_TEMPERATURE        = 5,
	SNDP_SHUTDOWN_REASON_CLOSE_DISCHARGE    = 6,
	SNDP_SHUTDOWN_REASON_SHUTDOWN_CMD       = 7,
	SNDP_SHUTDOWN_REASON_BOTH_SHUTDOWN      = 8,
	SNDP_SHUTDOWN_REASON_PT_SHUTDOWN_CMD    = 9,
	SNDP_SHUTDOWN_REASON_PAIR_TIMEOUT       = 10,
	SNDP_SHUTDOWN_REASON_RECONNECT_TIMEOUT  = 11,

} SNDP_shutdown_reason_e;


typedef enum{
	  SNDP_BT_NOT_ACCESSIBEL     = 0x00,
		SNDP_BT_DISCOVERABLE_ONLY  = 0x01,
		SNDP_BT_CONNECTABLE_ONLY   = 0x02,
		SNDP_BT_GENERAL_ACCESSIBLE = 0x03,
		SNDP_BT_LIMITED_ACCESSIBLE = 0x13,
}SNDP_bt_access_mode_t;

typedef void (*sndp_bt_conn_status_changed_callback)(sndp_bt_conn_status_e conn_status, uint8_t reason);

void sndp_save_data_before_shutdown(void);
void sndp_pmu_reboot(uint32_t boot_mode);
void sndp_app_reboot(uint32_t boot_mode);
void sndp_pmu_shutdown(void);
void sndp_app_shutdown(SNDP_shutdown_reason_e reason);
void sndp_enter_dut_mode(void);
void sndp_enter_single_dld_mode(void);
void sndp_enter_shipmode(void);
void sndp_enter_restore_factory_setting(void);
void sndp_bt_set_access_mode(SNDP_bt_access_mode_t access_mode);

uint8_t sndp_get_pairing_type(void);
uint8_t sndp_get_pairing_status(void);
void sndp_enter_freeman_pairing(void);
void sndp_start_freeman_pairing(void);
void sndp_start_tws_pairing(void);
bool sndp_is_left_right_bound(void);
void sndp_mobile_reconnect_timeout(void);
void sndp_mobile_reconnect_sccessful(void);
void sndp_enter_mobile_reconnect(void);
void sndp_mobile_pairing_timeout(void);
void sndp_mobile_pairing_sccessful(void);
void sndp_enter_mobile_pairing_after_tws_connected(void);
void sndp_tws_enter_mobile_pairing_after_mobile_disconnect(void);
void sndp_enter_mobile_pairing_directly(void);
void sndp_tws_pairing_config(uint8_t *addr, uint8_t len);
void sndp_ibrt_reconfig_save_to_nvrecord(void *config);
void sndp_ibrt_nvrecord_config_load(void *config);
int32_t sndp_ibrt_get_tws_pair_addr(uint8_t *addr);

bool sndp_is_freeman_mode(void);
bool sndp_is_tws_master_mode(void);
bool sndp_is_tws_slave_mode(void);
bool sndp_is_tws_link_connected(void);
uint8_t sndp_is_besaud_connected(void);
bool sndp_is_slave_ibrt_link_connected(void);
bool sndp_is_master_mobile_link_connected(void);
bool sndp_is_profile_a2dp_connected(void);
bool sndp_is_profile_avrcp_connected(void);
bool sndp_is_profile_hfp_connected(void);
int32_t sndp_disconnect_hfp(void);
int32_t sndp_disconnect_a2dp(void);
int32_t sndp_connect_a2dp(void);
void sndp_disconnect_mobile_link(uint8_t *mobile_addr);
void sndp_disconnect_all_mobile_link(void);
void sndp_clear_mobile_pairing_list(void);
uint8_t sndp_get_mobile_pairing_count(void);
uint8_t sndp_get_connected_mobile_count(void);

bool sndp_is_a2dp_mode(void);
bool sndp_is_sco_mode(void);
void sndp_ibrt_tws_switch(void);

void sndp_set_bt_conn_status_changed_callback(sndp_bt_conn_status_changed_callback callback);

/* link_type = ibrt_link_type_e */
void sndp_bt_conn_status_changed(sndp_bt_conn_status_e conn_status, uint8_t reason);
void sndp_global_handler_ind(uint8_t link_type, uint8_t evt_type, uint8_t status);
void sndp_profile_state_change_ind(uint32_t profile, uint8_t connected);

bool sndp_music_is_playing(void);
void sndp_music_ctrl(sndp_music_ctrl_event_e event);
uint8_t sndp_get_a2dp_volume(void);

void sndp_call_set_in_out_flag(uint8_t flag);
bool sndp_call_is_active(void);
bool sndp_call_is_incoming(void);
bool sndp_call_is_outgoing(void);
bool sndp_call_is_calling(void);
bool sndp_call_is_threeway_incoming(void);
bool sndp_call_is_threeway_calling(void);
bool spfi_call_is_hfp_audio_on(void);
void sndp_call_ctrl(sndp_call_ctrl_event_e event);
uint8_t sndp_get_call_volume(void);


bool sndp_anc_is_off(void);
bool sndp_anc_is_on(void);
bool sndp_anc_is_transparent(void);
sndp_anc_mode_e sndp_anc_get_mode_index(void);
void sndp_anc_mode_set(sndp_anc_mode_e anc_mode);
void sndp_anc_mode_set_locally(sndp_anc_mode_e anc_mode);
sndp_anc_mode_e sndp_anc_get_curr_mode(void);

void sndp_wakeup_voice_assistant(bool onoff);
void sndp_update_audio_channel(bool tws_conn);

void sndp_connect_status_print(void);
uint8_t *sndp_get_nvrecord_bt_peer_address(void);

int sndp_language_switch_handler(int new_lan);
void sndp_bt_switch(bool onoff, bool sync);
uint8_t sndp_get_is_shutting_down(void);
uint8_t sndp_get_shutdown_reason_is_charging_full(void);
uint8_t sndp_get_shutdown_reset_flag(void);
void sndp_set_shutdown_reset_flag(uint8_t flag);
uint8_t sndp_get_shutdown_reason(void);

#if defined(__SNDP_SLEEP_APP__)
void sndp_load_eq_param(void);
void sndp_set_crc(uint32_t *crc, uint8_t *data_ptr, uint32_t data_len);
bool sndp_check_crc(uint8_t *data_ptr, uint32_t flash_crc, uint32_t data_len);
#if defined(__SNDP_EQ_PARAM_SETTING__)
void sndp_get_custom_eq_param(uint8_t *param);
void sndp_set_custom_eq_param(int8_t *param);
void sndp_save_eq_param_to_flash(void);
#endif
uint32_t sndp_bt_audio_set_eq(uint8_t index);
uint8_t sndp_bt_audio_updata_eq_for_anc(void);
void sndp_clean_user_record(void);
#endif
void sndp_play_findme(void);
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */

#endif /* __SNDP_IF_PLATFORM_H__ */

