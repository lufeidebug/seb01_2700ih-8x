#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "cmsis.h"
#include "cmsis_os.h"
#include "string.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "hal_cmu.h"
#include "nvrecord_extension.h"
#include "nvrecord_env.h"
#include "factory_section.h"
#include "pmu.h"
#include "apps.h"
#include "app_tws_ibrt.h"

#include "besbt.h"
#include "btapp.h"
#include "app_bt.h"
#include "app_hfp.h"
#include "app_bt_func.h"
#include "app_ibrt_conn_evt.h"
#include "app_bt_media_manager.h"
#include "app_media_player.h"
#include "app_anc.h"
#include "a2dp_decoder.h"
#include "earbud_ux_api.h"
#include "audio_policy.h"
#include "bts_tws_api.h"
#include "bts_ibrt_conn.h"
#include "bts_core_if.h"
#include "bts_hfp_api.h"
#include "bts_bt_if.h"
#include "bts_bt_conn.h"
#include "bts_core_conn.h"
#include "bts_tws_channel.h"
#include "fir_process.h"
#include "iir_process.h"
#include "audio_process.h"
#include "crc_c.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"
#include "sndp_if_data_access.h"
#include "hal_timer_user_irq.h"

#if defined(__SNDP_ALG_APPLICATION__)
#include "sndp_algo_app.h"
#endif
#if defined(__SNDP_CHARGER_MGR__)
#include "sndp_hal_charger.h"
#endif

#if defined(__SNDP_CHARGER_MGR__)
#include "sndp_hal_battery.h"
#endif

#if defined(__SNDP_COMM_MGR__)     
#include "sndp_comm_cmd.h"
#endif

#if defined(__SNDP_PRODUCT_TEST__)
#include "sndp_product_test.h"
#endif

/**************************************************************************************************
* Constant
**************************************************************************************************/
//#define __SHUTDOWN_HW_CONFIG__



/**************************************************************************************************
* Prototype
**************************************************************************************************/
extern "C" uint8_t is_a2dp_mode(void);
extern "C" uint8_t is_sco_mode(void);
#if defined(__SNDP_SLEEP_APP__)
extern const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_dac_iir_cfg_list[EQ_HW_DAC_IIR_LIST_NUM];
extern const IIR_CFG_T audio_eq_hw_dac_iir_custom_mode;
#endif
/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_bt_conn_status_changed_callback sndp_bt_conn_status_changed_cb_ptr = NULL;

static uint8_t sndp_call_in_out = 0; // 0:none, 1:incoming, 2:outgoing

static sndp_pairing_type_e sndp_pairing_type = SNDP_PAIRING_NONE; // 0:未配对，1：对耳配对，2：单耳配对。

#if defined(__SNDP_PRODUCT_TEST__)
static uint8_t pt_pair_timeout_cnt = 0;
#endif
static sndp_pairing_state_e sndp_pairing_status = SNDP_PAIR_STA_NONE; //0:未配对，1：配对中，2：配对成功，3：配对超时。
uint8_t sndp_is_shutting_down = 0; // 0:正常，1：正在关机流程中
uint8_t sndp_shutdown_reason = SNDP_SHUTDOWN_REASON_NONE;
uint8_t sndp_shutdown_full_receive_08cmd_enter_reset=0;
uint8_t sndp_shutdown_reason_is_charging_full = 0;

uint8_t sndp_user_eq_record = 0xff;
uint8_t sndp_user_anc_record = 0xff;
/**************************************************************************************************
* Function
**************************************************************************************************/

#if defined(__SHUTDOWN_HW_CONFIG__)
void sndp_shutdown_hw_config(void)
{
	const struct HAL_IOMUX_PIN_FUNCTION_MAP app_hall_int_pin_cfg = {
    	HAL_IOMUX_PIN_P2_7, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENABLE
	};
	
	if (app_hall_int_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&app_hall_int_pin_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin, HAL_GPIO_DIR_OUT, 1);
    }
}
#endif


void sndp_save_data_before_shutdown(void)
{
	SNDP_IF_TRACE_ENTER();
#if 1
#if  0
	/* save bat info */
	sndp_da_field_bat_info_s bat_info;
	bat_info.bat_per = sndp_get_bat_percentage(false);
	bat_info.bat_volt = sndp_get_bat_voltage(false);
	sndp_da_write_field(SNDP_DA_FIELD_BAT_INFO, &bat_info, sizeof(sndp_da_field_bat_info_s), true);
#endif

#if defined(__SNDP_SLEEP_APP__)
	sndp_save_app_flag_to_flash();
#if defined(__SNDP_EQ_PARAM_SETTING__)
	sndp_save_eq_param_to_flash();
#endif
#endif

#if defined(__SNDP_ALG_APPLICATION__)
	sndp_alg_save_param();
#endif

	nv_record_flash_flush();
#endif
}

void sndp_pmu_reboot(uint32_t boot_mode)
{
	SNDP_TRACE_IMM(1, "%s, mode=%d", __func__, boot_mode);
    sndp_save_data_before_shutdown();
    osDelay(100);
	hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
	hal_sw_bootmode_set(boot_mode);
    pmu_reboot();
}

void sndp_app_reboot(uint32_t boot_mode)
{
	SNDP_TRACE_IMM(1, "%s, mode=%d", __func__, boot_mode);
    sndp_save_data_before_shutdown();
    osDelay(100);
	hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
	hal_sw_bootmode_set(boot_mode);
    app_reset();
}

void sndp_pmu_shutdown(void)
{
	SNDP_TRACE_IMM(1, "%s", __func__);
    sndp_save_data_before_shutdown();
	
#if defined(__SHUTDOWN_HW_CONFIG__)
	sndp_shutdown_hw_config();
#endif

    osDelay(100);
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    pmu_shutdown();
}

uint8_t sndp_get_is_shutting_down(void)
{
		return sndp_is_shutting_down;
}

uint8_t sndp_get_shutdown_reason(void)
{
		return sndp_shutdown_reason;
}	

uint8_t sndp_get_shutdown_reason_is_charging_full(void)
{
    return sndp_shutdown_reason_is_charging_full;
}

uint8_t sndp_get_shutdown_reset_flag(void)
{
    return sndp_shutdown_full_receive_08cmd_enter_reset;
}

void sndp_set_shutdown_reset_flag(uint8_t flag)
{
    sndp_shutdown_full_receive_08cmd_enter_reset = flag;
}

void sndp_app_shutdown(SNDP_shutdown_reason_e reason)
{
    SNDP_TRACE_IMM(1, "%s, %d", __func__, reason);
	if(sndp_is_shutting_down && reason == SNDP_SHUTDOWN_REASON_CHARGING_FULL)
		return;
    sndp_shutdown_reason = reason;
    sndp_is_shutting_down = 1;
	if (reason == SNDP_SHUTDOWN_REASON_CHARGING_FULL || reason == SNDP_SHUTDOWN_REASON_CHARGING_TIMEOUT) // 充电完成或超时
	{
		sndp_shutdown_reason_is_charging_full = 1;
	}
    sndp_save_data_before_shutdown();
    bta_tws_shutdown();
    osDelay(100);
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_shutdown();
}


void sndp_enter_dut_mode(void)
{
	SNDP_TRACE_IMM(1, "%s", __func__);
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_TEST_MODE|HAL_SW_BOOTMODE_TEST_SIGNALINGMODE);
    osDelay(100);
    pmu_reboot();
}

void sndp_enter_single_dld_mode(void)
{
    SNDP_IF_TRACE_ENTER();

    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_SINGLE_LINE_DOWNLOAD);
	osDelay(100);
    pmu_reboot();
}

void sndp_enter_shipmode(void)
{
#if 1    
	SNDP_TRACE_IMM(1, "%s", __func__);
	osDelay(100);
	hal_gpio_pin_set((enum HAL_GPIO_PIN_T)app_shipmode_cnt_pin_cfg.pin);
#endif  
}

void sndp_enter_restore_factory_setting(void)
{
    sndp_anc_mode_set(SNDP_ANC_MODE_OFF);
    osDelay(100);
    
    sndp_clear_mobile_pairing_list();
    nv_record_flash_flush();

    SNDP_TRACE_IMM(1, "%s, reboot!", __func__);
	osDelay(100);

    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_CUSTOM_OP2_AFTER_REBOOT);
    //pmu_reboot();
    app_reset();
}

void sndp_bt_set_access_mode(SNDP_bt_access_mode_t access_mode)
{
	app_bt_set_access_mode(access_mode);
}

uint8_t sndp_get_pairing_type(void)
{
    return sndp_pairing_type;
}

uint8_t sndp_get_pairing_status(void)
{
    return sndp_pairing_status;
}


void sndp_tws_enable_pairing_mode(void)
{
    /* 先退出再进入，确保这个api内部配对超时定时器重新开始计时 */
    bta_tws_enable_pairing_mode(false);
    bta_tws_enable_pairing_mode(true);
}

void sndp_enter_freeman_pairing(void)
{
    SNDP_IF_TRACE(0, "...");

	sndp_disconnect_all_mobile_link();

    sndp_pairing_type = SNDP_PAIRING_FREEMAN;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
    bta_tws_box_event_entry(BTA_TWS_OPEN);
	bta_tws_enable_freeman_mode(true); 
    sndp_tws_enable_pairing_mode();
    
#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_start_10_second_timer(APP_PAIR_TIMER_ID);   //5minute pairing
#endif    
}

void sndp_start_freeman_pairing(void)
{
    SNDP_IF_TRACE(0, "...");
    
    sndp_pairing_type = SNDP_PAIRING_FREEMAN;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
    
    //sndp_clear_mobile_pairing_list();
	sndp_disconnect_all_mobile_link();
    
#if 0//defined(__SNDP_REBOOT_FORCE_PAIRING__)
	osDelay(100);
	sndp_pmu_reboot(HAL_SW_BOOTMODE_CUSTOM_OP1_AFTER_REBOOT);
#else
    sndp_enter_freeman_pairing();
#endif    

}

void sndp_start_tws_pairing(void)
{
    SNDP_IF_TRACE(0, "...");
    
    sndp_pairing_type = SNDP_PAIRING_TWS;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
    
    sndp_disconnect_all_mobile_link();
    sndp_clear_mobile_pairing_list();
    
#if defined(__SNDP_REBOOT_FORCE_PAIRING__)
	osDelay(100);
	sndp_pmu_reboot(HAL_SW_BOOTMODE_CUSTOM_OP2_AFTER_REBOOT);
#else
    bta_tws_box_event_entry(BTA_TWS_CLOSE);
    sndp_delay_exec_start(1000, (uint32_t) sndp_enter_mobile_pairing_after_tws_connected, 0, 0, 0);
#endif

}

bool sndp_is_left_right_bound(void)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    SNDP_IF_TRACE(1, "nvrecord_env mode=%d", nvrecord_env->ibrt_mode.mode);
    if(nvrecord_env->ibrt_mode.mode != IBRT_UNKNOW) {
        return true;
    } else {
        return false;
    }
}

void sndp_mobile_reconnect_timeout(void)
{
    SNDP_IF_TRACE(0, ".");
	//sndp_enter_mobile_pairing_after_tws_connected();
	sndp_app_shutdown(SNDP_SHUTDOWN_REASON_RECONNECT_TIMEOUT);
}

void sndp_mobile_reconnect_sccessful(void)
{
    SNDP_IF_TRACE(0, ".");
#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_stop_10_second_timer(APP_BT_RECONNECT_TIMER_ID);
#endif
}


void sndp_enter_mobile_reconnect(void)
{
    bta_tws_box_event_entry(BTA_TWS_OPEN);
#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_start_10_second_timer(APP_BT_RECONNECT_TIMER_ID);
#endif
}

#if defined(__SNDP_PRODUCT_TEST__)
static void sndp_pt_reenter_pairing(void)
{
    SNDP_IF_TRACE(0, ".");
    if (sndp_is_tws_link_connected()) {
        /* 双耳模式：只有主耳才重新打开配对 */
        if (sndp_is_tws_master_mode()) {
            sndp_tws_enable_pairing_mode();
        }
    } else {
        /* 单耳模式：直接重新打开配对 */
        sndp_tws_enable_pairing_mode();
    }
}
#endif

void sndp_mobile_pairing_timeout(void)
{
    SNDP_IF_TRACE(0, ".");
    
    // 兜底检查：如果对端（主耳）已经连上手机，不关机
    if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode()
        && sndp_is_master_mobile_link_connected()) {
        SNDP_IF_TRACE(0, "peer already connected, skip shutdown");
        sndp_pairing_status = SNDP_PAIR_STA_SUCCESS;
        app_stop_10_second_timer(APP_PAIR_TIMER_ID);
        app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
        return;
    }

#if defined(__SNDP_PRODUCT_TEST__)
    /* 产测模式下，配对超时计数10次（10 * 3分钟 = 30分钟）才关机 */
    if(sndp_pt_get_test_mode()) {
        /* 双耳从耳：不参与计数，只重启定时器，等待主耳同步关机 */
        if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode()) {
            app_start_10_second_timer(APP_PAIR_TIMER_ID);
            return;
        }

        pt_pair_timeout_cnt++;
        SNDP_IF_TRACE(1, "pt pairing timeout cnt=%d", pt_pair_timeout_cnt);
        if(pt_pair_timeout_cnt < 10) {
            /* 先重启APP-3min定时器，再重新打开配对，避免被其他定时器覆盖 */
            app_start_10_second_timer(APP_PAIR_TIMER_ID);
			sndp_pt_reenter_pairing();
            return;
        }
        pt_pair_timeout_cnt = 0;
    }
#endif

    sndp_pairing_status = SNDP_PAIR_STA_TIMEOUT;
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    	// 主耳连接设备断开后，超时关机，从耳同步也关机
	if (sndp_is_tws_link_connected() && sndp_is_tws_master_mode()) {
		sndp_comm_cmd_send_lr_sync_both_shutdown();
		sndp_delay_exec_start(200, (uint32_t)sndp_app_shutdown, SNDP_SHUTDOWN_REASON_PAIR_TIMEOUT, 0, 0);
	}else
	{
		sndp_app_shutdown(SNDP_SHUTDOWN_REASON_PAIR_TIMEOUT);
	}
    

}

void sndp_mobile_pairing_sccessful(void)
{
    SNDP_IF_TRACE(0, ".");
    sndp_pairing_status = SNDP_PAIR_STA_SUCCESS;

#if defined(__SNDP_PRODUCT_TEST__)
    pt_pair_timeout_cnt = 0;
#endif
        
#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_PAIR_TIMER_ID);
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
}

void sndp_enter_mobile_pairing_after_tws_connected(void)
{
    SNDP_IF_TRACE(0, "enter");

    sndp_pairing_type = SNDP_PAIRING_TWS;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
        
    bta_tws_box_event_entry(BTA_TWS_OPEN);
    sndp_tws_enable_pairing_mode();

#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_start_10_second_timer(APP_PAIR_TIMER_ID);   //5minute pairing
#endif

}


void sndp_tws_enter_mobile_pairing_after_mobile_disconnect(void)
{
    SNDP_IF_TRACE(0, "enter");
	if (sndp_is_tws_slave_mode()) {
		sndp_comm_cmd_send_lr_sync_disconnect_and_tws_pair();	
	}

	sndp_disconnect_all_mobile_link();
	
    sndp_pairing_type = SNDP_PAIRING_TWS;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
        
    bta_tws_box_event_entry(BTA_TWS_OPEN);
    sndp_tws_enable_pairing_mode();

#if defined(__BTIF_AUTOPOWEROFF__)
    app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
    app_start_10_second_timer(APP_PAIR_TIMER_ID);   //5minute pairing
#endif

}

void sndp_tws_pairing_config(uint8_t *addr, uint8_t len)
{
    ibrt_config_t ibrt_config;
	uint8_t local_addr[6] = {0};

	SNDP_IF_TRACE_ENTER();

    sndp_pairing_type = SNDP_PAIRING_TWS;
    sndp_pairing_status = SNDP_PAIR_STA_PAIRING;
	
    if(addr == NULL || len != 6) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return;
    } 

    factory_section_original_btaddr_get(local_addr);
	
    if(sndp_dev_is_right_earphone()) {
		SNDP_IF_TRACE(0, "Right master");
		
        ibrt_config.nv_role = IBRT_MASTER;
		ibrt_config.audio_chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_RCHNL;
		
        memcpy((void *)ibrt_config.local_addr.address, local_addr, 6);
        memcpy((void *)ibrt_config.peer_addr.address, local_addr, 6);

    } else {
		SNDP_IF_TRACE(0, "Left slave");
		
		ibrt_config.nv_role = IBRT_SLAVE;                         
		ibrt_config.audio_chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_LCHNL;
		
        memcpy((void *)ibrt_config.local_addr.address, local_addr, 6);
        memcpy((void *)ibrt_config.peer_addr.address, addr, 6);
    }

	SNDP_IF_TRACE(0, "local_addr: %02X %02X %02X %02X %02X %02X", 
	        ibrt_config.local_addr.address[0],
	        ibrt_config.local_addr.address[1],
	        ibrt_config.local_addr.address[2],
	        ibrt_config.local_addr.address[3],
	        ibrt_config.local_addr.address[4],
	        ibrt_config.local_addr.address[5]);
    SNDP_IF_TRACE(0, "peer_addr: %02X %02X %02X %02X %02X %02X", 
	        ibrt_config.peer_addr.address[0],
	        ibrt_config.peer_addr.address[1],
	        ibrt_config.peer_addr.address[2],
	        ibrt_config.peer_addr.address[3],
	        ibrt_config.peer_addr.address[4],
	        ibrt_config.peer_addr.address[5]);
	
	sndp_ibrt_reconfig_save_to_nvrecord(&ibrt_config);
	osDelay(20);

}

uint8_t *sndp_get_nvrecord_bt_peer_address(void)
{
    struct nvrecord_env_t *nvrecord_env;
    
    nv_record_env_get(&nvrecord_env);
    return nvrecord_env->ibrt_mode.record.bdAddr.address;
}

void sndp_ibrt_reconfig_save_to_nvrecord(void *config)
{ 
    struct nvrecord_env_t *nvrecord_env = NULL;
    ibrt_config_t *ibrt_config = (ibrt_config_t *)config;
    
    nv_record_env_get(&nvrecord_env);
    memset((uint8_t *)&(nvrecord_env->ibrt_mode), 0xff, sizeof(nvrecord_env->ibrt_mode));
    nv_record_env_set(nvrecord_env);

    bta_tws_reconfig_nv_role(ibrt_config->nv_role, (bt_bdaddr_t *)&ibrt_config->peer_addr);
    nv_record_flash_flush();
}

static uint8_t s_sndp_pair_addr[6] = {0};

void sndp_ibrt_nvrecord_config_load(void *config)
{
    struct nvrecord_env_t *nvrecord_env;
    ibrt_config_t *ibrt_config = (ibrt_config_t *)config;
    uint8_t local_addr[6] = {0};
    
	if(sndp_dev_is_right_earphone()) {
        bts_tws_if_set_local_side(BT_LOCATION_RIGHT);
    } else {
        bts_tws_if_set_local_side(BT_LOCATION_LEFT);
    }

    factory_section_original_btaddr_get(local_addr);
    
    nv_record_env_get(&nvrecord_env);
    SNDP_IF_TRACE(1, "nvrecord_env mode=%d", nvrecord_env->ibrt_mode.mode);
    SNDP_IF_TRACE(0, "pair_addr: %02X %02X %02X %02X %02X %02X", 
	        nvrecord_env->ibrt_mode.record.bdAddr.address[0],
	        nvrecord_env->ibrt_mode.record.bdAddr.address[1],
	        nvrecord_env->ibrt_mode.record.bdAddr.address[2],
	        nvrecord_env->ibrt_mode.record.bdAddr.address[3],
	        nvrecord_env->ibrt_mode.record.bdAddr.address[4],
	        nvrecord_env->ibrt_mode.record.bdAddr.address[5]);
    // 开机保存配对地址，供 platform 后续获取
    memcpy(s_sndp_pair_addr, nvrecord_env->ibrt_mode.record.bdAddr.address, 6);

    if(nvrecord_env->ibrt_mode.mode != IBRT_UNKNOW) {
        ibrt_config->nv_role = nvrecord_env->ibrt_mode.mode;
        memcpy((void *)ibrt_config->local_addr.address, local_addr, 6);
        memcpy((void *)ibrt_config->peer_addr.address, nvrecord_env->ibrt_mode.record.bdAddr.address, 6);
        ibrt_config->audio_chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_STEREO; //等左右耳连上后再设置左右通道。
    } else {
        ibrt_config->nv_role = IBRT_UNKNOW;
        ibrt_config->audio_chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_STEREO;
    }

    bts_core_set_ui_role(ibrt_config->nv_role);
}

uint8_t* sndp_get_pair_addr(void)
{
    return s_sndp_pair_addr;
}

int32_t sndp_ibrt_get_tws_pair_addr(uint8_t *addr)
{
    struct nvrecord_env_t *nvrecord_env;

    if(addr == NULL){
        return -1;
    }
    
    factory_section_original_btaddr_get(addr);
    if(nv_record_env_get(&nvrecord_env) == 0) {
        if(nvrecord_env->ibrt_mode.mode != IBRT_UNKNOW) {
            memcpy(addr, nvrecord_env->ibrt_mode.record.bdAddr.address, 6);
        }
    }

    return 0;
}


/******************************************* BT Ctrl Interface ****************************************/

bool sndp_is_freeman_mode(void)
{
	return  bts_core_is_freeman_mode();
}

bool sndp_is_tws_master_mode(void)
{
	bool is_master = false;
	
	if(bts_tws_if_is_tws_link_connected() && app_ibrt_if_is_ui_master()) {
		is_master = true;
	} 

	SNDP_IF_TRACE(1, "%d", is_master);
	return is_master;
}

bool sndp_is_tws_slave_mode(void)
{
	bool is_slave = false;

	if(bts_tws_if_is_tws_link_connected() && app_ibrt_if_is_ui_slave()) {
		is_slave = true;
	} 
	
	SNDP_IF_TRACE(1, "%d", is_slave);
	return is_slave;
}

bool sndp_is_tws_link_connected(void)
{
	return bts_tws_if_is_tws_link_connected();
}

bool sndp_is_slave_ibrt_link_connected(void)
{
    return bts_ibrt_conn_is_ibrt_connected(NULL);
}

bool sndp_is_master_mobile_link_connected(void)
{
    return app_ibrt_if_is_any_mobile_connected();
}

uint8_t sndp_is_besaud_connected(void)
{
    return bts_tws_channel_is_connected();
}


void sndp_connect_status_print(void)
{
	SNDP_TRACE_IMM(0, "\n");
	SNDP_IF_TRACE(1, "---------------enter---------------");
	SNDP_IF_TRACE(2, "mobile=%d, ibrt=%d, tws=%d, besaud=%d", 
    		sndp_is_master_mobile_link_connected(),
    		sndp_is_slave_ibrt_link_connected(),
    		sndp_is_tws_link_connected(),
    		sndp_is_besaud_connected());

	SNDP_IF_TRACE(3, "role master=%d, salve=%d, freeman=%d", 
    		sndp_is_tws_master_mode(),
            sndp_is_tws_slave_mode(),
            sndp_is_freeman_mode());
	
	SNDP_IF_TRACE(1, "---------------end---------------");
	SNDP_TRACE_IMM(0, "\n");
}


bool sndp_is_profile_a2dp_connected(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
		
	    SNDP_IF_TRACE(3, "id=%d, acl_conn=%d, hf_conn=%d", 
	    		id,
	    		device->acl_is_connected,
	            device->hf_conn_flag);

	    if(device->acl_is_connected && device->a2dp_conn_flag) {
        	return true;
	    }

	}
	
	return false;
}

bool sndp_is_profile_avrcp_connected(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
		
	    SNDP_IF_TRACE(3, "id=%d, acl_conn=%d, hf_conn=%d", 
	    		id,
	    		device->acl_is_connected,
	            device->hf_conn_flag);

	    if(device->acl_is_connected && device->avrcp_conn_flag) {
        	return true;
	    }
	}
	
	return false;
}

bool sndp_is_profile_hfp_connected(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
	    SNDP_IF_TRACE(3, "id=%d, acl_conn=%d, hf_conn=%d", 
	    		id,
	    		device->acl_is_connected,
	            device->hf_conn_flag);

	    if(device->acl_is_connected && device->hf_conn_flag) {
        	return true;
	    }
	}
	
	return false;
}

int32_t sndp_disconnect_hfp(void)
{
    struct BT_DEVICE_T* device = NULL;
	
    for(int i = 0; i < BT_DEVICE_NUM; i += 1)
    {
        device = app_bt_get_device(i);
		if(device == NULL)
			continue;
		
        if (device->acl_is_connected) {
            app_bt_HF_DisconnectServiceLink(device->hf_channel);
        }
    }

    return 0;
}

int32_t sndp_disconnect_a2dp(void)
{
    SNDP_IF_TRACE_ENTER();
    return 0;
}

int32_t sndp_connect_a2dp(void)
{
    SNDP_IF_TRACE_ENTER();
    return 0;
}


void sndp_disconnect_mobile_link(uint8_t *mobile_addr)
{
	bt_bdaddr_t bt_addr;

	memcpy(bt_addr.address, mobile_addr, 6);
    app_tws_ibrt_disconnect_mobile(&bt_addr);
}

void sndp_disconnect_all_mobile_link(void)
{
    struct BT_DEVICE_T* device = NULL;
	
    for (int i = 0; i < BT_DEVICE_NUM; i += 1) {
        device = app_bt_get_device(i);
		if(device == NULL)
			continue;
		
        if(device->acl_is_connected) {
            sndp_disconnect_mobile_link(device->remote.address);
        }
    }
}

void sndp_clear_mobile_pairing_list(void)
{
#if 0    
    app_ibrt_if_nvrecord_delete_all_mobile_record();
    sndp_get_mobile_pairing_count();
#else
    nvrec_btdevicerecord* pNvRecord;
    ibrt_link_type_e link_type;
    int record_num = nv_record_get_paired_dev_list(&pNvRecord);

    if (record_num > 0){
        for(uint8_t i = 0; i < record_num; i++){
            link_type = app_tws_ibrt_get_link_type_by_addr(&pNvRecord[i].record.bdAddr);
            if (link_type == MOBILE_LINK) {
                nv_record_ddbrec_delete(&pNvRecord[i].record.bdAddr);
            }
        }
    }

   sndp_get_mobile_pairing_count();
#endif
}


uint8_t sndp_get_mobile_pairing_count(void)
{
    nvrec_btdevicerecord* pNvRecord;
    ibrt_link_type_e link_type;
    int record_num = nv_record_get_paired_dev_list(&pNvRecord);
    uint8_t paired_mobile_num = 0;

    if (record_num > 0){
        for(uint8_t i = 0; i < record_num; i++){
            link_type = app_tws_ibrt_get_link_type_by_addr(&pNvRecord[i].record.bdAddr);
            if ((link_type == MOBILE_LINK) && (paired_mobile_num < app_tws_ibrt_support_max_remote_link())) {
                paired_mobile_num++;
            }
        }
    }

    SNDP_IF_TRACE(2, "record=%d, paired=%d", record_num, paired_mobile_num);
    return paired_mobile_num;

}


uint8_t sndp_get_connected_mobile_count(void)
{
    return app_ibrt_if_get_connected_mobile_count();
}

bool sndp_is_a2dp_mode(void)
{
	return is_a2dp_mode();
}

bool sndp_is_sco_mode(void)
{
	return is_sco_mode();
}

void sndp_ibrt_tws_switch(void)
{
	if(sndp_is_tws_link_connected()) {
		app_ibrt_if_tws_role_switch_request();
	}
}


void sndp_set_bt_conn_status_changed_callback(sndp_bt_conn_status_changed_callback callback)
{
	sndp_bt_conn_status_changed_cb_ptr = callback;
}

void sndp_bt_conn_status_changed(sndp_bt_conn_status_e conn_status, uint8_t reason)
{
	SNDP_IF_TRACE(2, "conn_sta:%d, reason:0x%02x", conn_status, reason);
	
	if(sndp_bt_conn_status_changed_cb_ptr != NULL) {
		sndp_call_func_in_app_thread((uint32_t)sndp_bt_conn_status_changed_cb_ptr, (uint32_t)conn_status, (uint32_t)reason, 0);
	}
}

#if 0
void sndp_global_handler_ind(uint8_t link_type, uint8_t evt_type, uint8_t status)
{
	SNDP_IF_TRACE(3, "link_type:%d, evt_type:%d, status:%d", link_type, evt_type, status);

    switch (evt_type) {
        case BTIF_BTEVENT_LINK_CONNECT_CNF:// An outgoing ACL connection is up
        case BTIF_BTEVENT_LINK_CONNECT_IND://An incoming ACL connection is up
            SNDP_IF_TRACE(0, "LINK_CONNECTED");
		
            if(MOBILE_LINK == link_type) {
                if (BTIF_BEC_NO_ERROR == status) {
					SNDP_IF_TRACE(0, "mobile link connected!");
                    sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_MOBILE_CONNECTED, status);
                }
            }else if (TWS_LINK == link_type) {
                if (BTIF_BEC_NO_ERROR == status) {
					SNDP_IF_TRACE(0, "tws link connected!");
                    sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_TWS_CONNECTED, status);
                }
            }
            break;

        case BTIF_BTEVENT_LINK_DISCONNECT:
			SNDP_IF_TRACE(0, "LINK_DISCONNECTED");
			
            if(MOBILE_LINK == link_type) {
                SNDP_IF_TRACE(1, "mobile link disconnected, status=%d", status);
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_MOBILE_DISCONNECTED, status);
            } else if(TWS_LINK == link_type) {
                SNDP_IF_TRACE(1, "tws link disconnected, status=%d", status);
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_TWS_DISCONNECTED, status);
            }
            break;

        case BTIF_STACK_LINK_DISCONNECT_COMPLETE:
            SNDP_IF_TRACE(0, "LINK_DISCONNECT_COMPLETE");
            if (TWS_LINK == link_type) {
            } else if (MOBILE_LINK == link_type) {
            }
            break;

        case BTIF_BTEVENT_ROLE_CHANGE:
			SNDP_IF_TRACE(0, "ROLE_CHANGE");
            break;

        case BTIF_BTEVENT_BES_AUD_CONNECTED:
			SNDP_IF_TRACE(0, "BES_AUD_CONNECTED");
			
            //tws link callback when besaud connection complete
            if (BTIF_BEC_NO_ERROR == status) {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_BES_AUD_CONNECTED, status);
            }
            break;

        case BTIF_BTEVENT_BES_AUD_DISCONNECTED:
            SNDP_IF_TRACE(0, "BES_AUD_DISCONNECTED");
			sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_BES_AUD_DISCONNECTED, status);
            break;

        case BTIF_BTEVENT_ENCRYPTION_CHANGE:
            break;

        case BTIF_BTEVENT_MODE_CHANGE:
            break;

		case BTIF_BTEVENT_AUTHENTICATED:
			if (MOBILE_LINK == link_type) {
                if (BTIF_BEC_LOCAL_TERMINATED == status) {
                }
			}
			break;

        default:
            break;
    }
}

void sndp_profile_state_change_ind(uint32_t profile, uint8_t connected)
{
    SNDP_IF_TRACE(2, "profile=%x, connected=%x", profile, connected);

    switch (profile) {
        case BTIF_APP_A2DP_PROFILE_ID:
            if (connected) {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_A2DP_CONNECTED, 0);
            } else {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_A2DP_DISCONNECTED, 0);
            }
            break;
        case BTIF_APP_AVRCP_PROFILE_ID:
            if (connected) {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_AVRCP_CONNECTED, 0);
            } else {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_AVRCP_DISCONNECTED, 0);
            }
            break;

        case BTIF_APP_HFP_PROFILE_ID:
            if (connected) {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_HFP_CONNECTED, 0);
            } else {
                sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_HFP_DISCONNECTED, 0);
            }
            break;
        default:
            SNDP_IF_TRACE(1, "unhandle profile(%x)'s state changed.", profile);
            break;
    }
}
#endif


/******************************************* Musci Contrl Interface ****************************************/
uint8_t sndp_music_get_avrcp_palyback_status(void)
{
	uint8_t ret = BTIF_AVRCP_MEDIA_ERROR;
	uint8_t a2dp_device = app_bt_audio_get_curr_a2dp_device();
    struct BT_DEVICE_T* device = app_bt_get_device(a2dp_device);

	if(device == NULL) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return BTIF_AVRCP_MEDIA_ERROR;
	}

    if (device->avrcp_conn_flag) {
		if(app_bt_is_a2dp_streaming(a2dp_device)) {
			if (BTIF_AVRCP_MEDIA_PLAYING == device->avrcp_playback_status) {
	            ret = BTIF_AVRCP_MEDIA_PLAYING;
	        } else {
	            ret = BTIF_AVRCP_MEDIA_PAUSED;
	        }
				
		} else  {
			ret = BTIF_AVRCP_MEDIA_STOPPED;
		}
    } else {
        ret = BTIF_AVRCP_MEDIA_ERROR;
    }

	SNDP_IF_TRACE(4, "id=%d, av_conn=%d, sta=%d, ret=%d",
        a2dp_device, 
        device->avrcp_conn_flag,
        device->avrcp_playback_status,
        ret);

	return ret;
}



bool sndp_music_is_playing(void)
{
	bool ret = false;
#if 0
	if(bt_media_cur_is_bt_stream_music() 
        && (sndp_music_get_avrcp_palyback_status() == BTIF_AVRCP_MEDIA_PLAYING)) {
		ret = true;
    }
#else
    uint8_t device_id = app_bt_audio_get_curr_a2dp_device();
    struct BT_DEVICE_T* device = app_bt_get_device(device_id);

    if (device == NULL) {
        SNDP_IF_TRACE(0, "%d device NULL, ret=0", __LINE__);
        return false;
    }

    ret = (device->a2dp_play_pause_flag != 0);
#endif

	SNDP_IF_TRACE(1, "ret=%d", ret);
	return ret;
}

void sndp_music_ctrl(sndp_music_ctrl_event_e event)
{
	enum BT_DEVICE_ID_T device_id = (enum BT_DEVICE_ID_T)app_bt_audio_get_curr_a2dp_device();

	SNDP_IF_TRACE(1, "event=%d", event);
	
	switch(event) {
		case SNDP_MUSIC_CTRL_STOP:
			app_ibrt_if_a2dp_send_pause(device_id);
			break;
		case SNDP_MUSIC_CTRL_PLAY:
			app_ibrt_if_a2dp_send_play(device_id);
			break;
		case SNDP_MUSIC_CTRL_PAUSE:
			app_ibrt_if_a2dp_send_pause(device_id);
			break;
		case SNDP_MUSIC_CTRL_FORWARD:
			app_ibrt_if_a2dp_send_forward(device_id);
			break;
		case SNDP_MUSIC_CTRL_BACKWARD:
			app_ibrt_if_a2dp_send_backward(device_id);
			break;
		case SNDP_MUSIC_CTRL_VOLUME_INCREASE:
			app_ibrt_if_set_local_volume_up();
			break;
		case SNDP_MUSIC_CTRL_VOLUME_DECREASE:
			app_ibrt_if_set_local_volume_down();
			break;
		case SNDP_MUSIC_CTRL_VOLUME_SET:
			break;
        case SNDP_MUSIC_CTRL_REWIND:
			//app_ibrt_if_a2dp_send_rewind(device_id);
			break;
		case SNDP_MUSIC_CTRL_FAST_FORWARD:
            //app_ibrt_if_a2dp_send_fast_forward(device_id);
			break;
	}
}

uint8_t sndp_get_a2dp_volume(void)
{
    return a2dp_volume_local_get((enum BT_DEVICE_ID_T)app_bt_audio_get_curr_a2dp_device());
}


/******************************************* Call Contrl Interface ****************************************/

void sndp_call_set_in_out_flag(uint8_t flag)
{
	SNDP_IF_TRACE(1, "%d", flag);
	sndp_call_in_out = flag;
}

uint8_t sndp_call_get_in_out_flag(void)
{
	SNDP_IF_TRACE(1, "%d", sndp_call_in_out);
	return sndp_call_in_out;
}


/**
 * include incoming and calling
 */
bool sndp_call_is_active(void)
{
    struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
#if 1		
	    SNDP_IF_TRACE(5, "id=%d, audio_state=%d, callSetup=%d, call=%d, callheld=%d", 
	    		id,
	    		device->hf_audio_state,
	            device->hfchan_callSetup,
	            device->hfchan_call,
	            device->hf_callheld);
#endif

	    if((BT_HFP_CALL_SETUP_NONE != device->hfchan_callSetup)
	        || (BT_HFP_CALL_NONE != device->hfchan_call)
	        || (BT_HFP_CALL_HELD_NONE != device->hf_callheld)) {

			SNDP_IF_TRACE(0, "TRUE");
	        return true;
	    }
	}

	return false;
}


bool sndp_call_is_incoming(void)
{
	struct BT_DEVICE_T* device;

#if 1	
	if(sndp_call_get_in_out_flag() != 1)
		return false;
#endif

	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;

	    if(((BT_HFP_CALL_SETUP_IN == device->hfchan_callSetup) || (BT_HFP_CALL_SETUP_ALERT == device->hfchan_callSetup))
	        && (BT_HFP_CALL_NONE == device->hfchan_call) 
	        && (BT_HFP_CALL_HELD_NONE == device->hf_callheld)) {

			SNDP_IF_TRACE(0, "TRUE");
	        return true;
	    }
	}

	return false;
}

bool sndp_call_is_outgoing(void)
{
	struct BT_DEVICE_T* device;

#if 1	
	if(sndp_call_get_in_out_flag() != 2)
		return false;
#endif

	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;

	    if(((BT_HFP_CALL_SETUP_OUT == device->hfchan_callSetup) || (BT_HFP_CALL_SETUP_ALERT == device->hfchan_callSetup))
	        && (BT_HFP_CALL_NONE == device->hfchan_call) 
	        && (BT_HFP_CALL_HELD_NONE == device->hf_callheld)) {
	        SNDP_IF_TRACE(0, "TRUE");
	        return true;
	    }
	}

	return false;
}


bool sndp_call_is_calling(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
		
	    if((BT_HFP_CALL_NONE == device->hfchan_callSetup) 
            && (BT_HFP_CALL_ACTIVE == device->hfchan_call)) {

			SNDP_IF_TRACE(0, "TRUE");
            return true;
        }
	}
	return false;
}

bool sndp_call_is_threeway_incoming(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
		
	    if((BT_HFP_CALL_SETUP_IN == device->hfchan_callSetup) 
            && (BT_HFP_CALL_ACTIVE == device->hfchan_call) 
            && (BT_HFP_CALL_HELD_NONE == device->hf_callheld)) {

			SNDP_IF_TRACE(0, "TRUE");
            return true;
        }
	}
	
	return false;
}

bool sndp_call_is_threeway_calling(void)
{
	struct BT_DEVICE_T* device;
	
	for (uint8_t id = 0; id < BT_DEVICE_NUM; id++) {
		device = app_bt_get_device(id);
		if(device == NULL)
			continue;
		
	    if((BT_HFP_CALL_SETUP_NONE == device->hfchan_callSetup) 
            && (BT_HFP_CALL_ACTIVE == device->hfchan_call) 
            && (BT_HFP_CALL_HELD_ACTIVE == device->hf_callheld)) {

			SNDP_IF_TRACE(0, "TRUE");
            return true;
        }
	}
	
	return false;
}

bool spfi_call_is_hfp_audio_on(void)
{
    return app_bt_is_hfp_audio_on();
}

void sndp_call_ctrl(sndp_call_ctrl_event_e event)
{
    SNDP_IF_TRACE(0, "event=%d", event);

    if(sndp_is_tws_link_connected() && sndp_is_tws_slave_mode()) {
        SNDP_IF_TRACE(0, "send to master.");
        sndp_comm_cmd_send_lr_sync_call_ctrl(event);
        return;
    }
    
	switch(event) {
		case SNDP_CALL_CTRL_REJECT:
			hfp_handle_key(HFP_KEY_HANGUP_CALL);
			break;
		case SNDP_CALL_CTRL_ANSWER:
			hfp_handle_key(HFP_KEY_ANSWER_CALL);
			break;
		case SNDP_CALL_CTRL_HANGUP:
			hfp_handle_key(HFP_KEY_HANGUP_CALL);
			break;
		case SNDP_CALL_CTRL_THREEWAY_REJECT:
			hfp_handle_key(HFP_KEY_THREEWAY_HOLD_REL_INCOMING);
			break;
		case SNDP_CALL_CTRL_THREEWAY_HOLD_ANSWER:
			hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
			break;
		case SNDP_CALL_CTRL_THREEWAY_HANGUP_ANSWER:
			hfp_handle_key(HFP_KEY_THREEWAY_HANGUP_AND_ANSWER);
			break;
		case SNDP_CALL_CTRL_THREEWAY_SWITCH:
            hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
			break;
		case SNDP_CALL_CTRL_VOLUME_INCREASE:
			app_ibrt_if_set_local_volume_up();
			break;
		case SNDP_CALL_CTRL_VOLUME_DECREASE:
			app_ibrt_if_set_local_volume_down();
			break;
		case SNDP_CALL_CTRL_VOLUME_SET:
			break;
		case SNDP_CALL_CTRL_MUTE:
			hfp_handle_key(HFP_KEY_MUTE);
			break;
		case SNDP_CALL_CTRL_UNMUTE:
			hfp_handle_key(HFP_KEY_CLEAR_MUTE);
			break;
		case SNDP_CALL_CTRL_TONE_SWITCH_TO_PHONE:
			hfp_handle_key(HFP_KEY_CHANGE_TO_PHONE);
			break;
		case SNDP_CALL_CTRL_TONE_SWITCH_TO_EARPHONE:
			hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);
			break;
	}
}


uint8_t sndp_get_hfp_volume(void)
{
    return hfp_volume_local_get((enum BT_DEVICE_ID_T)app_bt_audio_get_curr_hfp_device());
}

/******************************************* ANC Contrl Interface ****************************************/

bool sndp_anc_is_off(void)
{
    app_anc_mode_t anc_mode = app_anc_get_curr_mode();

    return (APP_ANC_MODE_OFF == anc_mode);
}

bool sndp_anc_is_on(void)
{
    app_anc_mode_t anc_mode = app_anc_get_curr_mode();

    if(anc_mode > APP_ANC_MODE_OFF && anc_mode < APP_ANC_MODE_QTY) {
        return true;   
    }

    return false;
}

bool sndp_anc_is_transparent(void)
{
    app_anc_mode_t anc_mode = app_anc_get_curr_mode();

    return (APP_ANC_MODE5 == anc_mode);
}

sndp_anc_mode_e sndp_anc_get_mode_index(void)
{
    return SNDP_ANC_MODE_1;
}

void sndp_anc_mode_set(sndp_anc_mode_e anc_mode)
{
    app_anc_switch((app_anc_mode_t)anc_mode);
}

void sndp_anc_mode_set_locally(sndp_anc_mode_e anc_mode)
{
    app_anc_switch_locally((app_anc_mode_t)anc_mode);
}

sndp_anc_mode_e sndp_anc_get_curr_mode(void)
{
    app_anc_mode_t anc_mode = app_anc_get_curr_mode();
    
	SNDP_IF_TRACE(1, "mode=%d", anc_mode);
    
    return (sndp_anc_mode_e)anc_mode;
}

/******************************************* EQ Contrl Interface ****************************************/
#if defined(__SNDP_SLEEP_APP__)
static uint32_t sndp_calc_crc(uint8_t *data_ptr, uint32_t data_len)
{
	uint32_t calc_crc = 0;
	if(data_ptr == NULL)
	{
		SNDP_IF_TRACE(0, "data_ptr or crc_ptr is NULL");
		return -1;
	}
		calc_crc = crc32_c(0, data_ptr, data_len);
	SNDP_IF_TRACE(2, "calc_crc=0x%08x", calc_crc);	

	return calc_crc;
}

void sndp_set_crc(uint32_t *crc, uint8_t *data_ptr, uint32_t data_len)
{
	
	if(data_ptr == NULL || crc == NULL)
	{
		SNDP_IF_TRACE(0, "data_ptr or crc_ptr is NULL");
		return;
	}

	*crc = sndp_calc_crc(data_ptr, data_len);
	SNDP_IF_TRACE(2, "crc=0x%08x", *crc);
}

bool sndp_check_crc(uint8_t *data_ptr, uint32_t flash_crc, uint32_t data_len)
{
	uint32_t check_crc = 0;

	check_crc = sndp_calc_crc(data_ptr, data_len);
SNDP_IF_TRACE(2, "check_crc=0x%08x flash_crc=0x%08x", check_crc, flash_crc);	
	if(check_crc == flash_crc)
	{
		return true;
	}
	else
	{
		return false;
	}
}

#if defined(__SNDP_EQ_PARAM_SETTING__)
static IIR_CFG_T custom_eq_global_flash_cfg;
static IIR_CFG_T custom_eq_global_run_cfg;
static sndp_da_field_eq_data_s custom_global_eq_data;
void sndp_save_eq_param_to_flash(void)
{
	// Save the custom EQ parameters to flash, so that it can be loaded and used after power on.
	sndp_da_field_eq_data_s *eq_data_ptr = &custom_global_eq_data;

	sndp_da_read_field(SNDP_DA_FIELD_EQ_DATA, (uint8_t *)eq_data_ptr, sizeof(sndp_da_field_eq_data_s),true);
	if(memcmp(eq_data_ptr->data, &custom_eq_global_flash_cfg, sizeof(IIR_CFG_T)) == 0)
	{
		SNDP_IF_TRACE(0, "EQ param not changed, no need to write to flash");
	}
	else
	{
		SNDP_IF_TRACE(0, "EQ param changed, write to flash");
		memcpy(eq_data_ptr->data, &custom_eq_global_flash_cfg, sizeof(IIR_CFG_T));
		sndp_set_crc(&eq_data_ptr->data_crc, eq_data_ptr->data, sizeof(IIR_CFG_T));
		sndp_da_write_field(SNDP_DA_FIELD_EQ_DATA, (uint8_t *)eq_data_ptr, sizeof(sndp_da_field_eq_data_s),true);
	}

}

void sndp_set_default_eq_param(void)
{
	// Set default EQ parameters to the running param, so that the UI can read and display them.
	sndp_da_field_eq_data_s *eq_data_ptr = &custom_global_eq_data;
	memset(&custom_eq_global_flash_cfg, 0, sizeof(IIR_CFG_T));
	memset(&custom_eq_global_run_cfg, 0, sizeof(IIR_CFG_T));

	memcpy(&custom_eq_global_flash_cfg, &audio_eq_hw_dac_iir_custom_mode, sizeof(IIR_CFG_T));
	memcpy(&custom_eq_global_run_cfg, &audio_eq_hw_dac_iir_custom_mode, sizeof(IIR_CFG_T));
	eq_data_ptr->key = SNDP_DA_PARAM_FIELD_VALID;
	memcpy(eq_data_ptr->data, &custom_eq_global_flash_cfg, sizeof(IIR_CFG_T));
	sndp_da_write_field(SNDP_DA_FIELD_EQ_DATA, (uint8_t *)eq_data_ptr, sizeof(sndp_da_field_eq_data_s),true);
}

void sndp_load_eq_param(void)
{
	// Load default EQ parameters to the running param, so that the UI can read and display them.
	sndp_da_field_eq_data_s *eq_data_ptr = &custom_global_eq_data;
	IIR_CFG_T* user_cfg_ptr = (IIR_CFG_T*)eq_data_ptr->data;
	sndp_da_read_field(SNDP_DA_FIELD_EQ_DATA, (uint8_t *)eq_data_ptr, sizeof(sndp_da_field_eq_data_s),true);
	if(eq_data_ptr->key == SNDP_DA_PARAM_FIELD_VALID)
	{
		if(sndp_check_crc(eq_data_ptr->data, eq_data_ptr->data_crc,sizeof(IIR_CFG_T)))
		{
			SNDP_IF_TRACE(0, "Load EQ param from flash");
		}
		else
		{
			SNDP_IF_TRACE(0, "Load EQ param from flash fail, use default param");
			sndp_set_default_eq_param();
		}
	}
	else
	{
		sndp_set_default_eq_param();
	}
		memcpy(&custom_eq_global_run_cfg, user_cfg_ptr, sizeof(IIR_CFG_T));
		memcpy(&custom_eq_global_flash_cfg, user_cfg_ptr, sizeof(IIR_CFG_T));
}

void sndp_set_custom_eq_param(int8_t *param)
{
		sndp_da_field_eq_data_s *eq_data_ptr = &custom_global_eq_data;
		IIR_CFG_T* custom_cfg_tab = &custom_eq_global_run_cfg;

		if(param == NULL)
		{
			SNDP_IF_TRACE(0, "param is NULL");
			return;
		}

		if(eq_data_ptr->key == SNDP_DA_PARAM_FIELD_VALID)
		{
			// Use the custom EQ parameters
			for(int i=0; i < 8; i++)
			{
				custom_cfg_tab->param[i].gain = (float)(param[i] - 0x7f)/10.0; // -12db ~ +12db
			}
		}
		sndp_save_eq_param_to_flash();
}

void sndp_get_custom_eq_param(uint8_t *param)
{
		IIR_CFG_T* custom_cfg_tab = &custom_eq_global_run_cfg;

		if(param == NULL)
		{
			SNDP_IF_TRACE(0, "param is NULL");
			return;
		}

		memcpy(param, custom_cfg_tab, sizeof(IIR_CFG_T));
}
#endif
uint32_t sndp_bt_audio_set_eq(uint8_t index)
{
    const FIR_CFG_T *fir_cfg=NULL;
    const FIR_CFG_T *fir_cfg_2=NULL;
    const IIR_CFG_T *iir_cfg=NULL;
    const IIR_CFG_T *iir_cfg_2=NULL;

    SNDP_IF_TRACE(0,"[EQ] index=%d",  index);

		if(index >= EQ_HW_DAC_IIR_LIST_NUM)
		{
				SNDP_IF_TRACE(0,"[EQ] SET index %u > EQ_HW_DAC_IIR_LIST_NUM", index);
				return 1;
		}

#if defined(__SNDP_EQ_PARAM_SETTING__)
		if(index != SNDP_EQ_MODE_CUSTOM_MODE)
		{
			iir_cfg = audio_eq_hw_dac_iir_cfg_list[index];
		}
		else
		{
			iir_cfg = &custom_eq_global_run_cfg;
		}
#else
			iir_cfg = audio_eq_hw_dac_iir_cfg_list[index];
#endif	
    return audio_eq_set_cfg_full(fir_cfg,fir_cfg_2,iir_cfg,iir_cfg_2,AUDIO_EQ_TYPE_HW_DAC_IIR);
}

void sndp_clean_user_record(void)
{
	sndp_user_eq_record = 0xff;
	sndp_user_anc_record = 0xff;
}

uint8_t sndp_bt_audio_updata_eq_for_anc(void)
{
	bool sndp_anc_status = app_anc_work_status();
	uint8_t sndp_eq_index = sndp_dev_sleep_app_get_eq_index(false);
	
	if(sndp_user_eq_record != sndp_eq_index || sndp_user_anc_record != sndp_anc_status) {
		SNDP_TRACE(4, "anc_status=%d,eq_index=%d,rec_anc=%d,rec_eq=%d", sndp_anc_status,sndp_eq_index,sndp_user_anc_record,sndp_user_eq_record);
		if(sndp_anc_status != 0)
		{
			sndp_user_anc_record = sndp_anc_status;
			sndp_user_eq_record = sndp_eq_index;
			hal_sysfreq_req(HAL_SYSFREQ_USER_ANC, HAL_CMU_FREQ_104M);
			sndp_bt_audio_set_eq(sndp_eq_index + EQ_HW_DAC_IIR_LIST_NUM/2);
			hal_sysfreq_req(HAL_SYSFREQ_USER_ANC, HAL_CMU_FREQ_32K);
		}
		else
		{
			sndp_user_eq_record = sndp_eq_index;
			sndp_user_anc_record = sndp_anc_status;
			sndp_bt_audio_set_eq(sndp_eq_index);
		}	
	}
	return 0;

}

/*****************************************tools function *******************************************/
void sndp_play_findme(void)
{
	media_PlayAudio(AUD_ID_BT_FINDME, 0);
}

#endif
/******************************************* Voice Assistant Interface ****************************************/
extern int app_hfp_siri_voice(bool en);

void sndp_wakeup_voice_assistant(bool onoff)
{
    if(sndp_call_is_active()) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return;
    } 
    
	app_hfp_siri_voice(onoff);

}

void sndp_update_audio_channel(bool tws_conn) 
{
	A2DP_AUDIO_CHANNEL_SELECT_E chnl_sel;
    
	SNDP_IF_TRACE(0,"tws_conn=%d", tws_conn);
	
	if(tws_conn) {
		if(sndp_dev_is_right_earphone()) {
			SNDP_IF_TRACE(0,"RCHNL");
			chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_RCHNL;
		}
		else if(sndp_dev_is_left_earphone())
		{
			SNDP_IF_TRACE(0,"LCHNL");
			chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_LCHNL;

		} else {
			SNDP_IF_TRACE(0,"LRMERGE");
			chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_LRMERGE;
		}
	} else {
		SNDP_IF_TRACE(0,"LRMERGE single");
		chnl_sel = A2DP_AUDIO_CHANNEL_SELECT_LRMERGE;
	}

    bts_bt_if_update_audio_chnl_sel(chnl_sel);
	//a2dp_audio_channel_update(chnl_sel);
    
}

int sndp_language_switch_handler(int new_lan)
{
	SNDP_IF_TRACE(0, "new_lan:%d", new_lan);
	
#ifdef MEDIA_PLAYER_SUPPORT
    
    app_play_audio_set_lang(new_lan - 1);
    media_PlayAudio(AUD_ID_LANGUAGE_SWITCH, 0);
    
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
    nvrecord_env->media_language.language = new_lan;
    nv_record_env_set(nvrecord_env);
#endif

    return 0;
}

void sndp_bt_switch(bool onoff, bool sync)
{
    POSSIBLY_UNUSED bt_bdaddr_t mobile_addr1;
    POSSIBLY_UNUSED bt_bdaddr_t mobile_addr2;
    
    if(sync) {
        sndp_comm_cmd_send_lr_sync_bt_onoff(onoff);
    }
    
    if(onoff) {
        SNDP_IF_TRACE(0, "on..");
#if 1        
        bta_tws_box_event_entry(BTA_TWS_OPEN);
#else
        bta_tws_enable_access_mode(true);
        bta_tws_connect_all_bt_devices();
#endif        
    } else {
        SNDP_IF_TRACE(0, "off..");
        bta_tws_remove_all_bt_devices();
        bta_tws_enable_access_mode(false);
    }
}

unsigned int sndp_get_current_time(void)
{
	return TICKS_TO_MS(hal_sys_timer_get());
}

int sndp_hal_user_timer0_is_enabled(void)
{
    return hal_user_timer0_is_enabled();
}

void sndp_hal_user_timer0_start(uint32_t load)
{
    hal_user_timer0_start(load);
}

void sndp_hal_user_timer0_stop(void)
{
    hal_user_timer0_stop();
}

void sndp_hal_user_timer0_setup(int type, void(*handler)(uint32_t elapsed))
{
    hal_user_timer0_setup((HAL_TIMER_TYPE_T)type, (HAL_TIMER_IRQ_HANDLER_T)handler);
}

int sndp_hal_user_timer1_is_enabled(void)
{
    return hal_user_timer1_is_enabled();
}

void sndp_hal_user_timer1_start(uint32_t load)
{
    hal_user_timer1_start(load);
}

void sndp_hal_user_timer1_stop(void)
{
    hal_user_timer1_stop();
}

void sndp_hal_user_timer1_setup(int type, void(*handler)(uint32_t elapsed))
{
    hal_user_timer1_setup((HAL_TIMER_TYPE_T)type, (HAL_TIMER_IRQ_HANDLER_T)handler);
}
#endif	/* __SNDP_PROJ__ */



