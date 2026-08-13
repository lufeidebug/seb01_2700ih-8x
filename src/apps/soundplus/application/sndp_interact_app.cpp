#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "cqueue.h"

#include "sndp_if_platform.h"
#include "sndp_if_common.h"
#include "sndp_comm_main.h"
#include "sndp_comm_ble.h"
#include "sndp_comm_pogopin.h"

#include "sndp_interact_app.h"
#include "sndp_interact_box.h"
#include "sndp_heart_rate.h"
#include "sndp_sensor_service.h"
#include "hal_aud.h"
#include "iir_process.h"
#include "sndp_ui.h"
#if defined(__SNDP_HR_ALGO__)
#include "sleepsense.h"
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



/**************************************************************************************************
* Variable
**************************************************************************************************/

uint8_t wear_state_update_onoff = 0;
uint8_t sndp_sleepapp_report_battery_onoff = 0;
uint8_t sndp_findme_fadein_vol = TGT_VOLUME_LEVEL_8;

/**************************************************************************************************
* Function
**************************************************************************************************/
#if defined(__SNDP_SLEEP_APP__)
static void sndp_findme_loop_handler(uint8_t onoff);
static void sndp_sleep_app_set_flag_onoff(SNDP_SLEEP_APP_FLAG_NAME flag_name, bool peer, uint8_t onoff, bool sava);
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
    sndp_sleep_app_set_flag_onoff(SNDP_EQ_INDEX_FLAG, false, cmd_info->value[0], true);
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
#if defined(__SNDP_EQ_PARAM_SETTING__)
    int8_t freq_gain[8];
    memset(freq_gain, 0, sizeof(freq_gain));
    memcpy(freq_gain, cmd_info->value, cmd_info->data_len-1);
    sndp_set_custom_eq_param(freq_gain);
    memset(cmd_info->value, 0, cmd_info->data_len-1);
    
    cmd_info->data_len = 2;
    cmd_info->value[0] = 0; //success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
#else
    sndp_sleep_comm_cmd_rsp_with_errcode(cmd_info);
#endif
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_eq_param(sleep_app_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_EQ_PARAM_SETTING__)
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
#else
    sndp_sleep_comm_cmd_rsp_with_errcode(cmd_info);
#endif
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
    sndp_delay_exec_start(100, (uint32_t)sndp_findme_loop_handler,cmd_info->value[0],0,0);

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
    
    COMM_CMD_TRACE(1, "anc mode=%d", anc_mode);
    if(anc_mode <= 0x05){
        if(sndp_dev_wear_is_worn(true))
        {
            if(!sndp_dev_iobox_is_in_box(false))
            {
                sndp_anc_mode_set((sndp_anc_mode_e)anc_mode);
            }            
        }
        else
        {
            sndp_anc_mode_set_locally((sndp_anc_mode_e)anc_mode);
        }
        sndp_dev_sleep_app_anc_mode_set(false, (sndp_anc_mode_e)anc_mode, true);
        sndp_comm_cmd_send_lr_sync_sleep_app_flag(SNDP_ANC_MODE_FLAG, anc_mode, true);
        cmd_info->value[0] = 0; // success
    }else{
        cmd_info->value[0] = 0x01;
    }

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

uint32_t sndp_sleep_app_report_anc_mode(void)
{	
    //COMM_CMD_TRACE(1, "ble_is_connected %d", sndp_comm_ble_is_connected());
    if(!sndp_comm_ble_is_connected())
    {
        return 1;
    }
    
    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();

    cmd->value[data_len++] = sndp_dev_sleep_app_anc_mode_get(false);
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_GET_ANC_MODE;
    sleep_app_comm_main_send_cmd(cmd);
	return 0;
}

#if defined(__SNDP_HEART_RATE_MGR__)
POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_ppg_setting(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
     value[0] : 
     0x00: PPG OFF
     0x01: PPG 64Hz  //only support 64HZ
     0x02: PPG 128Hz
     0x03: PPG 256Hz
     */
     
    cmd_info->data_len = 0x02;
    if(cmd_info->value[0] == 0x01 || cmd_info->value[0] == 0x00){
        cmd_info->value[0] = 0x00; //success
    }else{
        cmd_info->value[0] = 0x01;
    }
    
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

static uint16_t acc_ntf_debug_count = 0;
static uint16_t ppg_ntf_debug_count = 0;
POSSIBLY_UNUSED static uint32_t  sleep_comm_cmd_recv_ppg_notification(sleep_app_comm_cmd_info_s *cmd_info)
{
    //if StartHeartrate 0x30 Dump on
    uint8_t onoff = cmd_info->value[0];
    ppg_ntf_debug_count = 0;
    COMM_CMD_TRACE(1, "ppg notification=%d", onoff);
    if(onoff){
        sndp_ppg_notification_start(0x01);
    }else{
        // 始终注销ppg_notification user; 若HR仍在运行, apply会保持PPG传感器开启
        sndp_ppg_notification_stop();
    }
    /* 同步给对耳 */
    sndp_comm_cmd_send_lr_sync_ppg_notification(onoff);
    COMM_CMD_TRACE(1, "dump=%d", sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE));
    return 0;
}

POSSIBLY_UNUSED static uint32_t sndp_comm_cmd_sleepapp_set_local_proximity(void)
{
    unsigned short proximity_value = 0;
    
    sndp_dev_hr_read_proximity_value(&proximity_value);
    sndp_sleep_app_set_flag_onoff(SNDP_PROXIMITY_DATA, false, proximity_value, false);
    return 0;
}

void sndp_comm_cmd_sleepapp_start_proximity(void)
{
    sndp_sensor_service_start_proximity_local();
    sndp_comm_cmd_sleepapp_proximity_task();
}

void sndp_comm_cmd_sleepapp_stop_proximity(void)
{
    sndp_sensor_service_stop_proximity_local();
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_proximity_notification(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t get_proximity_onoff = cmd_info->value[0];
    
    TR_INFO(0, (get_proximity_onoff == 0x01) ? "enable proximity" : "disable proximity");  

    if (get_proximity_onoff == 0x01) {
        sndp_comm_cmd_sleepapp_start_proximity();
    } else {
        sndp_comm_cmd_sleepapp_stop_proximity();
    }

    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_accelerometer_notification(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t onoff = cmd_info->value[0];
    acc_ntf_debug_count = 0;
    COMM_CMD_TRACE(1, "acc notification=%d", onoff);

    sndp_sensor_service_set_acc_notification_local(onoff);
    /* 同步给对耳 */
    sndp_comm_cmd_send_lr_sync_acc_notification(onoff);
    
    COMM_CMD_TRACE(1, "dump=%d", sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE));
    return 0;
}
#endif

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_battery_status(sleep_app_comm_cmd_info_s *cmd_info)
{
    sndp_sleepapp_report_battery_onoff = cmd_info->value[0];
    sndp_sleep_app_report_battery();
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_device_info(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t *p = cmd_info->value;
    char *bt_name = (char *)sndp_dev_get_bt_name();
    char *sn = (char *)sndp_dev_get_dev_sn();
    char *hw_ver = (char *)sndp_dev_get_hw_ver(false);
    char *fw_ver = (char *)sndp_dev_get_fw_ver(false);
#if defined(__SNDP_HR_ALGO__)
    char *algo_ver = (char *)lib_engine_version();
#else
    char algo_ver[] = "0.0.0.0";
#endif
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

    // lib_engine_version ver
    {
        len = strlen(algo_ver);
        *p++ = len + 1;
        *p++ = DEVICE_INFO_TAG_ALGO_VER;
        memcpy(p, algo_ver, len);
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
    sndp_sleep_app_set_flag_onoff(SNDP_GESTURE_ONOFF_FLAG, false, cmd_info->value[0], true);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_voice_prompt_enable(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t prompt_onoff = cmd_info->value[0];
    sndp_sleep_app_set_flag_onoff(SNDP_PROMPT_ONOFF_FLAG, false, prompt_onoff, true);
    cmd_info->value[0] = 0; // success

    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_touch_key_mapping(sleep_app_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_GESTURE_MAP__)
    gesture_map_t gesture_map[2*SNDP_DEV_GESTURE_MAX];

    for(int i=0; i<SNDP_DEV_GESTURE_MAX*2; i++){
        if(cmd_info->value[3*i+1] >= SNDP_DEV_GESTURE_MAX || cmd_info->value[3*i+2] >= SNDP_FUNC_MAX)
        {
            cmd_info->value[0] = 0x01; // fail
            break;
        }
        if(cmd_info->value[3*i] == 0) //left buds key set
        {
            if(sndp_dev_is_left_earphone()){
                sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)cmd_info->value[3*i+1], (sndp_dev_function_type_t)cmd_info->value[3*i+2]);
            }else{
                sndp_dev_gesture_mapper_update_mapping(true, (sndp_dev_gesture_type_t)cmd_info->value[3*i+1], (sndp_dev_function_type_t)cmd_info->value[3*i+2]);
            }
            gesture_map[i].ear_side = 0;
            gesture_map[i].key_behavior = (sndp_dev_gesture_type_t)cmd_info->value[3*i+1];
            gesture_map[i].key_function = (sndp_dev_function_type_t)cmd_info->value[3*i+2];
        }
        else if(cmd_info->value[3*i] == 1) //right buds key set
        {
            if(sndp_dev_is_right_earphone()){
                sndp_dev_gesture_mapper_update_mapping(false, (sndp_dev_gesture_type_t)cmd_info->value[3*i+1], (sndp_dev_function_type_t)cmd_info->value[3*i+2]);
            }else{
                sndp_dev_gesture_mapper_update_mapping(true, (sndp_dev_gesture_type_t)cmd_info->value[3*i+1], (sndp_dev_function_type_t)cmd_info->value[3*i+2]);
            }               
            gesture_map[i].ear_side = 1;
            gesture_map[i].key_behavior = (sndp_dev_gesture_type_t)cmd_info->value[3*i+1];
            gesture_map[i].key_function = (sndp_dev_function_type_t)cmd_info->value[3*i+2];
        }
        else //invalid
        {
            cmd_info->value[0] = 0x01; // fail
            break;
        }
    }

    sndp_comm_cmd_send_lr_sync_update_mapping(gesture_map, sizeof(gesture_map));
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0x00; // success
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
#endif
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_get_touch_key_mapping(sleep_app_comm_cmd_info_s *cmd_info)
{
#if defined(__SNDP_GESTURE_MAP__)
    gesture_map_t gesture_map_get[2*SNDP_DEV_GESTURE_MAX];
    for(int i=0; i<SNDP_DEV_GESTURE_MAX*2; i++){
        if(i<SNDP_DEV_GESTURE_MAX){
            gesture_map_get[i].ear_side = 0;
        }else{
            gesture_map_get[i].ear_side = 1;
        }
        gesture_map_get[i].key_behavior = i%SNDP_DEV_GESTURE_MAX;
        gesture_map_get[i].key_function = sndp_dev_gesture_mapper_get_function(gesture_map_get[i].ear_side, (sndp_dev_gesture_type_t)gesture_map_get[i].key_behavior);
        cmd_info->value[3*i] = gesture_map_get[i].ear_side;
        cmd_info->value[3*i+1] = gesture_map_get[i].key_behavior;
        cmd_info->value[3*i+2] = gesture_map_get[i].key_function;
    }
    cmd_info->data_len = 0x19;
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
#endif
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_set_smart_play_pause(sleep_app_comm_cmd_info_s *cmd_info)
{   
    uint8_t smart_playpause = cmd_info->value[0];
    sndp_dev_sleep_app_set_splaypause_onoff(false, smart_playpause, true);
    sndp_comm_cmd_send_lr_sync_sleep_app_flag(SNDP_SPLAYPAUSE_ONOFF_FLAG, smart_playpause, true);
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
#if defined(__SNDP_HEART_RATE_MGR__)   
    {
        //byte0 bit0~bit7 of struct
        /******************proximity map*******************/
        if(BitMap.proximity_off)
        {
            sndp_comm_cmd_sleepapp_stop_proximity();
        }
        if(BitMap.proximity_on)
        {
            sndp_comm_cmd_sleepapp_start_proximity();
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
            sndp_hr_mearsuring_set_sampling_rate(0x01);
        }else if(BitMap.ppg_off)
        {
            sndp_hr_mearsuring_set_sampling_rate(0x00);  
        }

        if(BitMap.accel_off)
        {
            sndp_acc_notification_stop();
        }

        if(BitMap.accel_on)
        {
            sndp_acc_notification_start(0x01);
        }
    }
#endif   
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
        if(sndp_dev_wear_is_worn(true))
        {
            sndp_anc_mode_set((sndp_anc_mode_e)anc_mode);
        }
        else
        {
            sndp_anc_mode_set_locally((sndp_anc_mode_e)anc_mode);
        }
        if(anc_mode < SNDP_ANC_MODE_QTY)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_ANC_MODE_FLAG, false, anc_mode, false);
        }   
        /******************anc map*******************/

        /******************voice prompt map*******************/
        if(BitMap.voice_disable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_PROMPT_ONOFF_FLAG, false, 0x00, false);
        }
        else if(BitMap.voice_enable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_PROMPT_ONOFF_FLAG, false, 0x01, false);
        }
        
        /******************touch onoff map*******************/
        if(BitMap.touch_disable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_GESTURE_ONOFF_FLAG, false, 0x00, false);
        }
        else if(BitMap.touch_enable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_GESTURE_ONOFF_FLAG, false, 0x01, false);
        }
        /******************touch onoff map*******************/
    }

    {
        //byte3 bit0~bit7 of struct
        if(BitMap.music_pause)
        {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PAUSE);
        }
        else if(BitMap.music_play)
        {
            sndp_music_ctrl(SNDP_MUSIC_CTRL_PLAY);
        }

        if(BitMap.smart_playpause_enable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_SPLAYPAUSE_ONOFF_FLAG, false, 0x01, false);
        }
        else if(BitMap.smart_playpause_disable)
        {
            sndp_sleep_app_set_flag_onoff(SNDP_SPLAYPAUSE_ONOFF_FLAG, false, 0x00, false);
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
#if defined(__SNDP_HEART_RATE_MGR__)
    cmd_info->value[1] = sndp_hr_mearsuring_get_sampling_rate();
#endif
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

#if defined(__SNDP_HEART_RATE_MGR__)
POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_start_heartrate(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint8_t sampling_rate = cmd_info->value[0];
    uint8_t dump_data = cmd_info->value[1];
    if(dump_data == 1){
        acc_ntf_debug_count = 0;
        ppg_ntf_debug_count = 0;
    }
    sndp_sensor_service_start_heartrate_local(sampling_rate, dump_data);
    /* 同步给对耳 */
    sndp_comm_cmd_send_lr_sync_start_heartrate(sampling_rate, dump_data);
    return 0;
}

void sndp_sleep_comm_cmd_heartrate_stop(void)
{
    COMM_CMD_TRACE(0, "heartrate stop");
    sndp_sensor_service_stop_heartrate_local();
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_stop_heartrate(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_sleep_comm_cmd_heartrate_stop();
    /* 同步给对耳 */
    sndp_comm_cmd_send_lr_sync_stop_heartrate();
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

void sndp_sleep_comm_cmd_analysis_start(int32_t sleep_control)
{
    COMM_CMD_TRACE(0, "sleep analysis start");
    sndp_sensor_service_enter_sleep_mode_local(sleep_control);
#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)
    sndp_sleep_role_start();
#endif
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_start_sleep(sleep_app_comm_cmd_info_s *cmd_info)
{
    uint32_t sleep_control = cmd_info->value[0]<<24 | cmd_info->value[1]<<16 | cmd_info->value[2]<<8 | cmd_info->value[3];
    DUMP8("0x%02x ", cmd_info->value, 4);
    COMM_CMD_TRACE(1, "sleep control=%d wear state=%d", sleep_control, sndp_dev_wear_is_worn(false));
    if(sndp_dev_wear_is_worn(false)){
        sndp_sleep_comm_cmd_analysis_start(sleep_control);
        /* 同步给对耳，两耳同时运行睡眠分析 */
        sndp_comm_cmd_send_lr_sync_start_sleep(sleep_control);
        cmd_info->value[0] = 0; // success
    } else {
        cmd_info->value[0] = 0x01; // fail
    }
    cmd_info->data_len = 0x02;
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_sleep_tracking(sleep_app_comm_cmd_info_s *cmd_info)
{
    int16_t *accel_data_m = (int16_t*)&cmd_info->value[0];
    uint8_t *screen_status = &cmd_info->value[180];
    uint8_t sound_state = cmd_info->value[210];
    sndp_dbbeats_put_sleep_app_data(accel_data_m, screen_status, sound_state);
    /* 同步追踪数据给对耳，运行相同的逻辑 */
    sndp_comm_cmd_send_lr_sync_sleep_tracking(cmd_info->value, 211);
    return 0;
}

void sndp_sleep_comm_disconnect_timer_handler(void)
{
    if(sndp_dev_sleep_app_get_stage_onoff(false)){
        sndp_sleep_comm_cmd_analysis_stop();
    }
    if(sndp_dev_sleep_app_get_proximity_onoff(false)){
        sndp_sensor_service_stop_proximity_local();
    }
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)){
        sndp_sensor_service_stop_heartrate_local();
    }
    if(sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE)){
        sndp_acc_notification_stop();
    }
    if(sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE)){
        sndp_ppg_notification_stop();
    }
}

void sndp_sleep_comm_cmd_analysis_stop(void)
{
    COMM_CMD_TRACE(0, "sleep analysis stop");
    sndp_sensor_service_exit_sleep_mode_local();
#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)
    sndp_sleep_role_stop();
#endif
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_stop_sleep(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_sleep_comm_cmd_analysis_stop();
    /* 同步给对耳，两耳同时停止睡眠分析 */
    sndp_comm_cmd_send_lr_sync_stop_sleep();
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}
#endif

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

#if defined(__SNDP_HEART_RATE_MGR__)
POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_sensor_test(sleep_app_comm_cmd_info_s *cmd_info)
{
    /*
        Byte0	Sensor Type
        0x00: PPG
        0x01: Accelerometer
        Byte1	Measurement Duration
        0x01: 1sec
        0x02: 2sec
        0x03: 3sec    
    */
    uint8_t sensor_type = cmd_info->value[0];
    POSSIBLY_UNUSED uint8_t measurement_duration = cmd_info->value[1];
    if(sensor_type == 0x00){
        if(sndp_hr_is_reading_ppg_enabled() || sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE)){
#if defined(__SNDP_GSENSOR_SUPPORT__)
            sndp_hal_hr_read_samples_rate(sndp_comm_cmd_sleepapp_report_ppg_samples);
            sndp_hal_hr_samples_measurement_start(measurement_duration);
#endif
        }else{
            cmd_info->value[0] = 0xff; 
            cmd_info->data_len = 0x02;
            sndp_sleep_comm_main_rsp_cmd(cmd_info);
        }
    }
    else if(sensor_type == 0x01)
    {
        if(sndp_hr_is_reading_acc_enabled() || sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE)){
#if defined(__SNDP_GSENSOR_SUPPORT__)
            sndp_hal_acc_read_samples_rate(sndp_comm_cmd_sleepapp_report_acc_samples);
            sndp_hal_acc_samples_measurement_start(measurement_duration);
#endif
        }else{
            cmd_info->value[0] = 0xff; 
            cmd_info->data_len = 0x02;
            sndp_sleep_comm_main_rsp_cmd(cmd_info);
        }
    }

    return 0;
}

POSSIBLY_UNUSED static uint32_t sleep_comm_cmd_recv_app_resume_heart_rate(sleep_app_comm_cmd_info_s *cmd_info)
{
    cmd_info->data_len = 0x02;
    cmd_info->value[0] = 0; // success
    sndp_hr_ble_connected_delay10s_stop();
    sndp_sleep_comm_main_rsp_cmd(cmd_info);
    return 0;
}

void sndp_comm_cmd_sleepapp_report_ppg_samples(uint16_t sensor_samples)
{
    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0, "BLE is not connected, stop ppg samples reporting");
        return;
    }
    uint8_t data_len = 0;
    uint16_t ppg_sample_rate = 64;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();
    memset(cmd->value, 0, sizeof(cmd->value));
    cmd->value[data_len++] = 0x00;
    cmd->value[data_len++] = (uint8_t)(ppg_sample_rate >> 8);
    cmd->value[data_len++] = (uint8_t)(ppg_sample_rate & 0xFF);
    cmd->value[data_len++] = (uint8_t)((sensor_samples>>8) & 0xFF);
    cmd->value[data_len++] = (uint8_t)(sensor_samples & 0xFF);
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_SENSOR_SAMPLE_RATE_REPORT;
    sleep_app_comm_main_send_cmd(cmd);  
}

void sndp_comm_cmd_sleepapp_report_acc_samples(uint16_t sensor_samples)
{
    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0, "BLE is not connected, stop acc samples reporting");
        return;
    }
    uint8_t data_len = 0;
    uint16_t acc_sample_rate = 125;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();
    memset(cmd->value, 0, sizeof(cmd->value));
    cmd->value[data_len++] = 0x01;
    cmd->value[data_len++] = (uint8_t)((acc_sample_rate >> 8) & 0xFF);
    cmd->value[data_len++] = (uint8_t)(acc_sample_rate & 0xFF);
    cmd->value[data_len++] = (uint8_t)((sensor_samples>>8) & 0xFF);
    cmd->value[data_len++] = (uint8_t)(sensor_samples & 0xFF);
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_SENSOR_SAMPLE_RATE_REPORT;
    sleep_app_comm_main_send_cmd(cmd);  
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
        // sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_stop, 0, 0, 0);
        return 1;
    }

    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();
    memset(cmd->value, 0, sizeof(cmd->value));
    memcpy(cmd->value, sleep_stage, 40);
    data_len += 40;
    cmd->value[data_len++] = (uint8_t)((position_and_control >> 8) & 0xFF);
    cmd->value[data_len++] = position_and_control & 0xFF;
    cmd->value[data_len++] = (uint8_t)((result_code >> 8) & 0xFF);
    cmd->value[data_len++] = (uint8_t)(result_code & 0xFF);
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_SLEEP_TRACKING;
    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_hr(uint8_t* sendhr, uint8_t* dbbeats_data)
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
        // sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
        return 1;
    }
    
    sndp_hr_dbbeats_data *dbbeats_data_ptr = (sndp_hr_dbbeats_data *)dbbeats_data;
#if defined(__SNDP_HR_ALGO__)
    HrvIndices *sendhr_ptr = (HrvIndices *)sendhr;
#endif
    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();
#if defined(__SNDP_HR_ALGO__)
    cmd->value[data_len++] = sendhr_ptr->HR & 0xFF;
    cmd->value[data_len++] = (sendhr_ptr->HR>>8) & 0xFF;
    cmd->value[data_len++] = sendhr_ptr->SDNN & 0xFF;
    cmd->value[data_len++] = (sendhr_ptr->SDNN>>8) & 0xFF;
    cmd->value[data_len++] = sendhr_ptr->coherence & 0xFF;
    cmd->value[data_len++] = (sendhr_ptr->coherence>>8) & 0xFF;
#else
    data_len += 6;
#endif
    cmd->value[data_len++] = 0;
    cmd->value[data_len++] = dbbeats_data_ptr->count;
    cmd->value[data_len++] = dbbeats_data_ptr->result_code & 0xFF;
    cmd->value[data_len++] = (dbbeats_data_ptr->result_code>>8) & 0xFF;
    if(sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE))
    {
        if(sndp_dev_is_left_earphone()){
            cmd->value[data_len++] = 0x01;
        }else if(sndp_dev_is_right_earphone()){
            cmd->value[data_len++] = 0x02;
        }
        cmd->value[data_len++] = dbbeats_data_ptr->is_contact;
        cmd->value[data_len++] = dbbeats_data_ptr->led_state;
        cmd->value[data_len++] = dbbeats_data_ptr->pck_interval & 0xFF;
        cmd->value[data_len++] = (dbbeats_data_ptr->pck_interval>>8) & 0xFF;
        cmd->value[data_len++] = (dbbeats_data_ptr->pck_interval>>16) & 0xFF;
        cmd->value[data_len++] = (dbbeats_data_ptr->pck_interval>>24) & 0xFF;
        cmd->flag = AppFlag;
        cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
        cmd->cmd = SLEEP_APP_CMDID_HEARTRATE_MEASURING_WITH_DUMP;
        sleep_app_comm_main_send_cmd(cmd);
    }else{
        cmd->flag = AppFlag;
        cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
        cmd->cmd = SLEEP_APP_CMDID_HEARTRATE_MEASURING;
        sleep_app_comm_main_send_cmd(cmd);
    }
	return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_ppg_raw_data(uint8_t *ppg_raw_data, uint16_t ppg_raw_len)
{
    if(!sndp_comm_ble_is_connected()){
        COMM_CMD_TRACE(0, "ble is not connected, not report ppg");
        return 0;
    }

    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();

    // 1. 打包LR_flag 1字节
    if(sndp_dev_is_left_earphone())
        cmd->value[data_len++] = 0x01;
    else
        cmd->value[data_len++] = 0x02;

    // 2. 打包ppg_raw_data_len（1字节）
    // ppg_raw_len = ppg_raw_len > 192 ? 192 : ppg_raw_len;
    cmd->value[data_len++] = (uint8_t)ppg_raw_len;

    // 3. 填充FIFO原始数据。
    for (int i = 0; i < ppg_raw_len; i++) {
        cmd->value[data_len++] = ppg_raw_data[i]; 
    }
    
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_PPG_NOTIFICATION;
    sleep_app_comm_main_send_cmd(cmd);

    return 0;
}


uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf_debug(int16_t *acc_raw_data, uint16_t acc_raw_len)
{
    int16_t *acc_data_ptr = acc_raw_data;
    acc_ntf_debug_count++;
    if(!sndp_comm_ble_is_connected()){
        COMM_CMD_TRACE(0, "ble is not connected, not report accelerometer");
        return 0;
    }

    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd(); // 1字节LR_flag + 2字节count + 最多25个数据点，每个数据点包含X/Y/Z三个轴，每轴2字节
    // COMM_CMD_TRACE(1, "report accelerometer to app, len=%d", acc_raw_len);
        // 1. 打包LR_flag 1字节
    if(sndp_dev_is_left_earphone()){
        cmd->value[data_len++] = 0x01;
    }
    else{
        cmd->value[data_len++] = 0x02;
    }
        
        cmd->value[data_len++] = (uint8_t)((acc_ntf_debug_count >> 8) & 0xFF);  // count高字节
        cmd->value[data_len++] = (uint8_t)(acc_ntf_debug_count);
    // 2. 打包acc_raw_len个数据点，每个数据点包含X/Y/Z三个轴，每轴2字节，最多150字节
        for (int i = 0; i < acc_raw_len/3; i++) {
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i] & 0xFF);         // X轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i] >> 8) & 0xFF);  // X轴最高字节
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i + 1] & 0xFF);     // Y轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i + 1] >> 8) & 0xFF); // Y轴最高字节
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i + 2] & 0xFF);     // Z轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i + 2] >> 8) & 0xFF); // Z轴最高字节
        }
    

    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION_DEBUG;
    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf(int16_t *acc_raw_data, uint16_t acc_raw_len)
{
    int16_t *acc_data_ptr = acc_raw_data;
    if(!sndp_comm_ble_is_connected()){
        COMM_CMD_TRACE(0, "ble is not connected, not report accelerometer");
        return 0;
    }

    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();
    // COMM_CMD_TRACE(1, "report accelerometer to app, len=%d", acc_raw_len);
        // 1. 打包LR_flag 1字节
    if(sndp_dev_is_left_earphone())
        cmd->value[data_len++] = 0x01;
    else
        cmd->value[data_len++] = 0x02;

    // 2. 打包acc_raw_len个数据点，每个数据点包含X/Y/Z三个轴，每轴2字节，最多150字节
        for (int i = 0; i < acc_raw_len/3; i++) {
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i] & 0xFF);         // X轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i] >> 8) & 0xFF);  // X轴最高字节
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i + 1] & 0xFF);     // Y轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i + 1] >> 8) & 0xFF); // Y轴最高字节
            cmd->value[data_len++] = (uint8_t)(acc_data_ptr[3*i + 2] & 0xFF);     // Z轴最低字节
            cmd->value[data_len++] = (uint8_t)((acc_data_ptr[3*i + 2] >> 8) & 0xFF); // Z轴最高字节
        }
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION;
    sleep_app_comm_main_send_cmd(cmd);

    return 0;
}

uint32_t sndp_comm_cmd_sleepapp_report_proximity_to_app(void)
{
    unsigned short proximity_value_local = 0;
    unsigned short proximity_value_peer = 0;
    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();

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
        cmd->value[data_len++] = 0x01;
        cmd->value[data_len++] = (uint8_t)(proximity_value_local & 0xFF);
        cmd->value[data_len++] = (uint8_t)((proximity_value_local >> 8) & 0xFF);
        cmd->value[data_len++] = (uint8_t)(proximity_value_peer & 0xFF);
        cmd->value[data_len++] = (uint8_t)((proximity_value_peer >> 8) & 0xFF);
    } else {
        cmd->value[data_len++] = 0x02;
        cmd->value[data_len++] = (uint8_t)(proximity_value_peer & 0xFF);
        cmd->value[data_len++] = (uint8_t)((proximity_value_peer >> 8) & 0xFF);
        cmd->value[data_len++] = (uint8_t)(proximity_value_local & 0xFF);
        cmd->value[data_len++] = (uint8_t)((proximity_value_local >> 8) & 0xFF);
    }

    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION;
    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}

/**
 * @brief   proximity周期任务: 由统一任务线程每1秒tick调用一次, 左右耳运行完全相同的流程
 *          1. 读取本地proximity并缓存
 *          2. TWS连接时: 转发本地值给对耳(对耳收到后缓存并触发上报)
 *             单耳时: 直接上报手机
 *          注: 上报动作两侧都会执行, 仅BLE连接手机的一侧实际发出数据
 */
uint32_t sndp_comm_cmd_sleepapp_proximity_task(void)
{
    if (!sndp_dev_sleep_app_get_proximity_onoff(false)) {
        return 0;
    }

    /* 1. 读取本地proximity并缓存 */
    unsigned short proximity_value_local = 0;

    sndp_dev_hr_read_proximity_value(&proximity_value_local);
    sndp_dev_sleep_app_set_proximity_data(false, proximity_value_local);

    if (sndp_is_tws_link_connected()) {
        /* 2. TWS连接: 转发本地值给对耳, 使对耳也持有双耳数据 */
        sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(proximity_value_local);
    } else if (sndp_comm_ble_is_connected()) {
        /* 3. 单耳直连手机: 直接上报 */
        sndp_dev_sleep_app_set_proximity_data(true, 0);
        sndp_comm_cmd_sleepapp_report_proximity_to_app();
    }
    return 0;
}
#endif 

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
    uint8_t data_len = 0;
    sleep_app_comm_cmd_info_s *cmd = sleep_app_comm_main_get_send_cmd();    
    sndp_dev_sleep_app_wear_cnt(lR_flag, wear_state);
    memcpy(wear_onoff_cnt, sndp_dev_sleep_app_get_wear_cnt(), sizeof(wear_onoff_cnt));
    COMM_CMD_TRACE(0,"wear cnt:%d, %d, %d, %d", wear_onoff_cnt[0], wear_onoff_cnt[1],wear_onoff_cnt[2],wear_onoff_cnt[3]);
    cmd->value[data_len++] = lR_flag;
    cmd->value[data_len++] = wear_state;
    if(lR_flag == SNDP_DEV_EARSIDE_LEFT){
        cmd->value[data_len++] = wear_onoff_cnt[SNDP_DEV_LEFT_WEAR_CNT] & 0xff;
        cmd->value[data_len++] = (wear_onoff_cnt[SNDP_DEV_LEFT_WEAR_CNT] >> 8) & 0xff;
        cmd->value[data_len++] = wear_onoff_cnt[SNDP_DEV_LEFT_UNWEAR_CNT] & 0xff;
        cmd->value[data_len++] = (wear_onoff_cnt[SNDP_DEV_LEFT_UNWEAR_CNT] >> 8) & 0xff;
    }else{
        cmd->value[data_len++] = wear_onoff_cnt[SNDP_DEV_RIGHT_WEAR_CNT] & 0xff;
        cmd->value[data_len++] = (wear_onoff_cnt[SNDP_DEV_RIGHT_WEAR_CNT] >> 8) & 0xff;
        cmd->value[data_len++] = wear_onoff_cnt[SNDP_DEV_RIGHT_UNWEAR_CNT] & 0xff;
        cmd->value[data_len++] = (wear_onoff_cnt[SNDP_DEV_RIGHT_UNWEAR_CNT] >> 8) & 0xff;
    }
    cmd->flag = AppFlag;
    cmd->data_len = data_len + SLEEP_APP_CMD_LEN;
    cmd->cmd = SLEEP_APP_CMDID_WEAR_STATE_UPDATE;
    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}

static void sndp_findme_loop_handler(uint8_t onoff)
{
    if(onoff)
    {
        COMM_CMD_TRACE(0,"stop findeme....");
        sndp_findme_fadein_vol = TGT_VOLUME_LEVEL_8;
        return;
    }
   
    sndp_play_findme();
    if(sndp_findme_fadein_vol < TGT_VOLUME_LEVEL_16){
        sndp_findme_fadein_vol += 2;
        if(sndp_findme_fadein_vol >= TGT_VOLUME_LEVEL_16){
            sndp_findme_fadein_vol = TGT_VOLUME_LEVEL_16;
        }
    }
    sndp_delay_exec_start(2800, (uint32_t)sndp_findme_loop_handler,0,0,0);
}

uint8_t sndp_get_findme_vol(void)
{
    return sndp_findme_fadein_vol;
}

void sndp_sleep_app_report_battery(void)
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
    if(!sndp_sleepapp_report_battery_onoff)
    {
        COMM_CMD_TRACE(0,"rtn battery report off.....");
        return;
    }
    
    SndpGetBattryMap_t reply_battery;
    sndp_dev_bat_info_s cradle_bt_info;
    memset(&reply_battery, 0, sizeof(SndpGetBattryMap_t));
    memset(&cradle_bt_info, 0, sizeof(sndp_dev_bat_info_s));
    
    if(sndp_dev_is_left_earphone())
    {
        //reply_battery.bits.left_charging_statu = (uint8_t)sndp_dev_charger_is_charging(false);
        reply_battery.bits.left_iobox_statu = (uint8_t)sndp_dev_iobox_is_in_box(false);
        reply_battery.bits.left_battery_level = sndp_dev_get_bat_percentage(false);

        if(sndp_is_tws_link_connected()){
            //reply_battery.bits.right_charging_statu = (uint8_t)sndp_dev_charger_is_charging(true);
            reply_battery.bits.right_iobox_statu = (uint8_t)sndp_dev_iobox_is_in_box(true);
            reply_battery.bits.right_battery_level = sndp_dev_get_bat_percentage(true);
        }else{
            //reply_battery.bits.right_charging_statu = 1;
            reply_battery.bits.right_iobox_statu = 1;
            reply_battery.bits.right_battery_level = 0x7f;
        }
    }

    if(sndp_dev_is_right_earphone())
    {
        //reply_battery.bits.right_charging_statu = (uint8_t)sndp_dev_charger_is_charging(false);
        reply_battery.bits.right_iobox_statu = (uint8_t)sndp_dev_iobox_is_in_box(false);
        reply_battery.bits.right_battery_level = sndp_dev_get_bat_percentage(false);

        if(sndp_is_tws_link_connected()){
            //reply_battery.bits.left_charging_statu = (uint8_t)sndp_dev_charger_is_charging(true);
            reply_battery.bits.left_iobox_statu = (uint8_t)sndp_dev_iobox_is_in_box(true);
            reply_battery.bits.left_battery_level = sndp_dev_get_bat_percentage(true);
        }else{
            //reply_battery.bits.left_charging_statu = 1;
            reply_battery.bits.left_iobox_statu = 1;
            reply_battery.bits.left_battery_level = 0x7f;
        }
       
    }
    sndp_dev_get_box_bat_info(&cradle_bt_info);
    reply_battery.bits.cradle_charging_status = 0;
    reply_battery.bits.cradle_battery_level = cradle_bt_info.bat_per;
    // COMM_CMD_TRACE(0,"left:%02x,right:%02x,cradle:%02x", 
    //                    reply_battery.bits.left_battery_level,
    //                    reply_battery.bits.right_battery_level, 
    //                    reply_battery.bits.cradle_battery_level);
    // DUMP8("%02x",&reply_battery,sizeof(reply_battery));                                           
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_GET_BATTERY_STATUS, sizeof(reply_battery), (uint8_t*)&reply_battery);
}

#ifdef __SNDP_SEND_GESTURE__
void sndp_sleep_app_report_gesture(uint32_t gesture)
{
    /*
        SNDP_DEV_GESTURE_EVENT_NONE, == 0
        SNDP_DEV_GESTURE_EVENT_PRESS_DOWN, == 1
        SNDP_DEV_GESTURE_EVENT_PRESS_UP, == 2
        SNDP_DEV_GESTURE_EVENT_1_CLICK, == 3
        SNDP_DEV_GESTURE_EVENT_2_CLICK, == 4
        SNDP_DEV_GESTURE_EVENT_3_CLICK, == 5
        SNDP_DEV_GESTURE_EVENT_4_CLICK, == 6
        SNDP_DEV_GESTURE_EVENT_5_CLICK, == 7
        SNDP_DEV_GESTURE_EVENT_LONG_PRESS, == 8
        SNDP_DEV_GESTURE_EVENT_LLONG_PRESS, == 9
        SNDP_DEV_GESTURE_EVENT_REPEAT,  == 10
        SNDP_DEV_GESTURE_EVENT_SLIDE_UP, == 11
        SNDP_DEV_GESTURE_EVENT_SLIDE_DOWN, == 12
    */
    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0,"ble not connected,gesture no send......");
        return;
    }
    uint8_t reply_gesture[2] = {0};
    reply_gesture[0] = gesture;
    reply_gesture[1] = sndp_dev_get_earside(false);
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_GESTURE_EVENT_UPDATE, sizeof(reply_gesture), reply_gesture);
}

void sndp_sleep_app_report_tap(void)
{
//避免在stack not ready时调用ble connected判断接口，导致死机
    if(!app_is_stack_ready())
    {
        COMM_CMD_TRACE(0,"stack not ready,ble api no use......");
        return;
    }     
    if(!sndp_comm_ble_is_connected())
    {
        COMM_CMD_TRACE(0,"ble not connected,tap no send......");
        return;
    }
    uint8_t reply_tap[2] = {0};
    reply_tap[0] = 0x00;
    reply_tap[1] = sndp_dev_get_earside(false);
    sleep_app_comm_main_send_cmd_by_id(SLEEP_APP_CMDID_TAP_UPDATE, sizeof(reply_tap), reply_tap);
}
#endif
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
#if defined(__SNDP_HEART_RATE_MGR__)
    { SLEEP_APP_CMDID_PPG_SETING,                     "APP_SET_PPG_SETTING",                sleep_comm_cmd_recv_app_set_ppg_setting },
    { SLEEP_APP_CMDID_PPG_NOTIFICATION,               "APP_PPG_NOTIFICATION",               sleep_comm_cmd_recv_ppg_notification },
    { SLEEP_APP_CMDID_PPG_NOTIFICATION_DEBUG,         "APP_PPG_NOTIFICATION_DEBUG",         sleep_comm_cmd_recv_ppg_notification },
    { SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION,     "APP_GET_PROXIMITY_NOTIFICATION",     sleep_comm_cmd_recv_app_get_proximity_notification },
    { SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION, "APP_GET_ACCELEROMETER_NOTIFICATION", sleep_comm_cmd_recv_app_get_accelerometer_notification },
    { SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION_DEBUG, "APP_GET_ACCELEROMETER_NOTIFICATION_DEBUG", sleep_comm_cmd_recv_app_get_accelerometer_notification },
#endif
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
#if defined(__SNDP_HEART_RATE_MGR__)
    { SLEEP_APP_CMDID_START_HEARTRATE,                "APP_START_HEARTRATE",                sleep_comm_cmd_recv_app_start_heartrate },
    { SLEEP_APP_CMDID_STOP_HEARTRATE,                 "APP_STOP_HEARTRATE",                 sleep_comm_cmd_recv_app_stop_heartrate },
    { SLEEP_APP_CMDID_START_SLEEP,                    "APP_START_SLEEP",                    sleep_comm_cmd_recv_app_start_sleep },
    { SLEEP_APP_CMDID_SLEEP_TRACKING,                 "APP_SLEEP_TRACKING",                 sleep_comm_cmd_recv_app_sleep_tracking },
    { SLEEP_APP_CMDID_STOP_SLEEP,                     "APP_STOP_SLEEP",                     sleep_comm_cmd_recv_app_stop_sleep },
#endif
    { SLEEP_APP_CMDID_WEAR_STATE_UPDATE,              "APP_WEAR_STATE",                     sleep_comm_cmd_recv_app_wear_state },
#if defined(__SNDP_HEART_RATE_MGR__)
    { SLEEP_APP_CMDID_SENSOR_TEST,                    "APP_SENSOR_TEST",                    sleep_comm_cmd_recv_app_sensor_test  },
    { SLEEP_APP_CMDID_RESUME_HEART_RATE,              "APP_RESUME_HEART_RATE",              sleep_comm_cmd_recv_app_resume_heart_rate }
#endif
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

    COMM_CMD_TRACE(2,"BLE CON: %d, slave mode:%d", sndp_comm_ble_is_connected(), sndp_is_tws_slave_mode());
    // if(!sndp_comm_ble_is_connected()) {
        // if(sndp_is_tws_slave_mode()) {
        //     return -1;
        // }
    // }

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

static void sndp_sleep_app_set_flag_onoff(SNDP_SLEEP_APP_FLAG_NAME flag_name, bool peer, uint8_t onoff, bool sava)
{
    COMM_CMD_TRACE(1, "flag_name = %d, peer = %d, onoff = %d, sava = %d", flag_name, peer, onoff, sava);
    switch (flag_name)
    {
    case SNDP_PROMPT_ONOFF_FLAG:
        sndp_dev_sleep_app_set_prompt_onoff(peer, onoff, sava);
        break;
    case SNDP_EQ_INDEX_FLAG:
        sndp_dev_sleep_app_set_eq_index(peer, onoff, sava);
        break;
    case SNDP_ANC_MODE_FLAG:
        sndp_dev_sleep_app_anc_mode_set(peer, onoff, sava);
        break;
    case SNDP_GESTURE_ONOFF_FLAG:
        sndp_dev_sleep_app_set_gesture_onoff(peer, onoff, sava);
        break;
    case SNDP_SPLAYPAUSE_ONOFF_FLAG:
        sndp_dev_sleep_app_set_splaypause_onoff(peer, onoff, sava);
        break;
#if defined(__SNDP_HEART_RATE_MGR__)
    case SNDP_PROXIMITY_ONOFF_FLAG:
        sndp_dev_sleep_app_set_proximity_onoff(peer, onoff);
        sndp_comm_cmd_send_lr_sync_Proximity_Notification_ONOFF(onoff);
        break;
    case SNDP_PROXIMITY_DATA:
        sndp_dev_sleep_app_set_proximity_data(peer, onoff);
        sndp_comm_cmd_send_lr_sync_Proximity_Notification_DATA(onoff);
        break;
    case SNDP_STAGE_ONOFF_FLAG:
        sndp_dev_sleep_app_set_stage_onoff(peer, onoff);
        sndp_comm_cmd_send_lr_sync_stage_onoff(onoff);
        break;
    case SNDP_HEARTRATE_ONOFF_FLAG:
        sndp_dev_sleep_app_set_heartrate_onoff(peer, onoff);
        sndp_comm_cmd_send_lr_sync_heart_rate_onoff(onoff);
        break;
#endif
    default:
        break;
    }

    /* 统一同步基础flag到对耳 */
    if (flag_name <= SNDP_SLEEP_MODE_FLAG) {
        sndp_comm_cmd_send_lr_sync_sleep_app_flag(flag_name, onoff, sava);
    }
}
#endif

int32_t sndp_interact_app_init(void)
{

    
	INTERACT_APP_TRACE(0, "done.");
	return 0;
}
#endif	/* __SNDP_PROJ__ */


