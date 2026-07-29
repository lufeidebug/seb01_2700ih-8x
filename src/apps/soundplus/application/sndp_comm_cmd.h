#ifndef __SNDP_COMM_CMD_H__
#define __SNDP_COMM_CMD_H__

#if defined(__SNDP_COMM_MGR__)
#include "sndp_comm_protocol.h"
#include "sleepwave_app_protocol.h"


#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_CMD_TRACE__
#if defined(__SNDP_COMM_CMD_TRACE__)
#define COMM_CMD_TRACE(num, str, ...)            SNDP_TRACE(1 + num, "[COMM_CMD] %s, " str, __func__, ##__VA_ARGS__)
#else
#define COMM_CMD_TRACE(num, str, ...)
#endif



typedef enum {

    /****** 与充电仓交互指令 ******/
	COMM_CMDID_EB_HANDSHAKE                         = 0x00, /* desc: 握手指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 1 bytes, error code(1) */ 
	COMM_CMDID_EB_FREEMAN_PAIRING                   = 0x01, /* desc: 单耳配对指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 1 bytes, error code(1) */ 
	COMM_CMDID_EB_TWS_PAIRING                       = 0x02, /* desc: 对耳耳配对指令。
                                                             * recv: 6 bytes, bt addr(6)。发送到右耳全部为0x00，发送到左耳填充右耳地址。
                                                             * rsp : 7 bytes, error code(1)  + bt addr(6)。响应当前耳机蓝牙地址。*/ 
	COMM_CMDID_EB_QUERY_PARING_STATUS               = 0x03, /* desc: 查询配对指令。
                                                             * recv: 0 bytes, no command data
                                                             * rsp : 3 bytes, error code(1) + tws conn status(1) + mobile conn status(1) */
	COMM_CMDID_EB_QUERY_BAT_INFO                    = 0x04, /* desc: 查询耳机电量信息指令。
                                                             * recv: 0 bytes, no command data
                                                             * rsp : 2 bytes, error code(1) + bat percentage(1) */ 
	COMM_CMDID_EB_EARBUDS_SHUTDOWN                  = 0x05, /* desc: 耳机关机指令。
                                                             * recv: 0 bytes, no command data
                                                             * rsp : 1 bytes, error code(1) */ 
	COMM_CMDID_EB_RESTORE_FACTORY_SETTING           = 0x06, /* desc: 耳机恢复出厂设置指令。
                                                             * recv: 0 bytes, no command data
                                                             * rsp : 1 bytes, error code(1) */ 
	COMM_CMDID_EB_ENTER_FW_UPGRADE                  = 0x07, /* desc: 进入固件升级模式指令。
                                                             * recv: 0 bytes, no command data
                                                             * rsp : 1 bytes, error code(1) */ 									
	COMM_CMDID_EB_REPORT_BOX_INFO                   = 0x08, /* desc: 充电仓上报设备状态指令。
                                                             * recv: 6 bytes, fw ver(3) + bat voltage(2) + bat per(1)
                                                             * rsp : 1 bytes, error code(1) */
	COMM_CMDID_EB_REPORT_COVER_STATUS               = 0x09, /* desc: 充电仓上报开关盖状态指令。
                                                             * recv: 1 bytes, box cover status(1)
                                                             * rsp : 1 bytes, error code(1) */		
	COMM_CMDID_EB_QUERY_MOBILE_PAIRED_COUNT         = 0x0A, /* desc: 查询耳机与手机个数指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 1 bytes, error code(1) */		
    COMM_CMDID_EB_SWITCH_LANGUAGE                   = 0x0B, /* recv: 1 bytes, language(1)
                                                                rsp: 1 bytes, error code(1) */  


    /****** 左右耳同步指令. ******/
    COMM_CMDID_LR_SYNC_WORKING_MODE                 = 0x20, /* desc: 左右耳同步工作模式指令。
                                                             * recv: 1 bytes, working mode(1),
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_BAT_INFO                     = 0x21, /* desc: 左右耳同步电量信息指令。
                                                             * recv: 1 bytes, working mode(1),
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_VOLUME                       = 0x22, /* desc: 左右耳同步音量等级指令。
                                                             * recv: 2 bytes, type(1) + volume level(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_INBOX_STATUS                 = 0x23, /* desc: 左右耳同步出入仓状态指令。
                                                             * recv: 1 bytes, inout status(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_COVER_STATUS                 = 0x24, /* desc: 左右耳同步开关盖状态指令。
                                                             * recv: 1 bytes, cover status(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_WEAR_STATUS                  = 0x25, /* desc: 左右耳同步佩戴状态指令。
                                                             * recv: 1 bytes, wear status(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_GESTURE                      = 0x26, /* desc: 左右耳同步手势事件指令。
                                                             * recv: 1 bytes, gesture event(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_BOTH_SHUTDOWN                = 0x27, /* desc: 左右耳同步关机指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_MUSIC_CTRL                   = 0x28, /* desc: 左右耳同步音乐控制指令。
                                                             * recv: 1 bytes, event(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_CALL_CTRL                    = 0x29, /* desc: 左右耳同步通话控制指令。
                                                             * recv: 1 bytes, event(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_ALL_DEV_STATUS               = 0x2A, /* desc: 左右耳同步所有设备状态指令。
                                                             * recv: n bytes, 
                                                             * rsp : 0 bytes */ 
    COMM_CMDID_LR_SYNC_BT_ONOFF                     = 0x2B, /* desc: 左右耳同步BT开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */ 
    COMM_CMDID_LR_SYNC_MOBILE_CONNECTED             = 0x2C, /* desc: 左右耳同步手机已连接状态指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 0 bytes */                                                          
    
                                                             
#if defined(__SNDP_SLEEP_APP__)                                                  
    COMM_CMDID_LR_SYNC_EQ_INDEX                     = 0x2D, /* desc: 左右耳同步EQ模式指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_ANC_MODE                     = 0x2E, /* desc: 左右耳同步ANC模式指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_SLEEP_MODE                   = 0x2F, /* desc: 左右耳同步睡眠模式指令。
                                                             * recv: 1 bytes, sleep mode(1)
                                                             * rsp : 0 bytes */ 
    COMM_CMDID_LR_SYNC_PROMPT_ONOFF                 = 0x30, /* desc: 左右耳同步提示音开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_GESTRUE_ONOFF                = 0x31, /* desc: 左右耳同步按键手势开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_SPLAYPAUSE_ONOFF             = 0x32, /* desc: 左右耳同步开关smart play/pause。
                                                             * recv: 2 bytes, key behavior(1) + key function(1)
                                                             * rsp : 0 bytes */       
    COMM_CMDID_LR_SYNC_UPDATE_MAPPING               = 0x33, /* desc: 左右耳同步手势使能开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */    
    COMM_CMDID_LR_SYNC_Proximity_Notification_ONOFF = 0x34, /* desc: 左右耳同步 proximity 上报开关。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */ 
    COMM_CMDID_LR_SYNC_Proximity_Notification_DATA  = 0x35, /* desc: 左右耳同步 proximity 数据。
                                                             * recv: 2 bytes, proximity value(2)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_HEARTRATE_ONOFF              = 0x36, /* desc: 左右耳同步心率开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_STAGE_ONOFF                  = 0x37, /* desc: 左右耳同步阶段开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
#endif
    
#if defined(__SNDP_SLEEP_APP__)
    COMM_CMDID_LR_SYNC_SLEEP_SNAPSHOT               = 0x38, /* desc: 左右耳同步睡眠算法快照。
                                                             * recv: n bytes, snapshot data(2 bytes size + n bytes data)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_SLEEP_ROLE_STATUS            = 0x39, /* desc: 左右耳同步睡眠角色状态。
                                                             * recv: 1 bytes, role status(1)
                                                             * rsp : 0 bytes */                                                    
#endif

    COMM_CMDID_LR_SYNC_DISCONNECT_AND_TWS_PAIR       = 0x3A, /* desc: 左右耳同步断开手机连接并进入配对模式指令。
                                                             * recv: 0 bytes,
                                                             * rsp : 0 bytes */
    
    /****** 生产测试指令. ******/
    COMM_CMDID_PT_SWITCH_TEST_MODE                  = 0x40, /* recv: 1 bytes, mode(1).
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_QUERY_TEST_MODE                   = 0x41, /* recv: 0 bytes.
                                                            rsp: 2 bytes, error code(1) + mode(1). */
    COMM_CMDID_PT_SHUTDOWN                          = 0x42, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_REBOOT                            = 0x43, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_ENTER_SHIPMODE                    = 0x44, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_RESTORE_FACTORY_SETTING           = 0x45, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_SINGLE_PAIRING                    = 0x46, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_TWS_PAIRING                       = 0x47, /* recv: 6 bytes. bt mac(6)
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_ENTER_DUT                         = 0x48, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_EXIT_DUT                          = 0x49, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_QUERY_FW_VER                      = 0x4A, /* recv: 0 bytes. 
                                                            rsp: 5 bytes, error code(1) + fw ver(4). */
    COMM_CMDID_PT_QUERY_HW_VER                      = 0x4B, /* recv: 0 bytes. 
                                                            rsp: 3 bytes, error code(1) + hw ver(2). */
    COMM_CMDID_PT_READ_DEV_SN                       = 0x4C, /* recv: 0 bytes. 
                                                            rsp: 21 bytes, error code(1) + sn(20). */
    COMM_CMDID_PT_WRITE_DEV_SN                      = 0x4D, /* recv: <=20 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_READ_BT_ADDR                      = 0x4E, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_WRITE_BT_ADDR                     = 0x4F, /* recv: 6 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_READ_BLE_ADDR                     = 0x50, /* recv: 0 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_WRITE_BLE_ADDR                    = 0x51, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_READ_RF_FREQ_OFF                  = 0x52, /* recv: 0 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_WRITE_RF_FREQ_OFF                 = 0x53, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_TEST_MIC                          = 0x54,	/* recv: 1 bytes. mic index(1)
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_TEST_SPK                          = 0x55,	/* recv: 1 bytes. mic index(1)
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_TEST_GSENSOR                      = 0x56,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_TEST_GSENSOR_REPORT               = 0x57,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_TEST_HRSENSOR                     = 0x58,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_TEST_HRSENSOR_REPORT              = 0x59,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_QUERY_DEV_STATUS                  = 0x5A,	/* recv: 1 bytes. mode(1)
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_READ_ANC_CALIB_STATUS             = 0x5B,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + status(1). */
	COMM_CMDID_PT_READ_ALGO_AUTH_RESULT             = 0x5C,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 bytes, error code(1) + result(1). */
    COMM_CMDID_PT_SWITCH_LOG_OUTPUT                 = 0x5D,	/* recv: 1 bytes, en(1). 
                                                            rsp: 2 bytes, error code(1) + result(1). */
    COMM_CMDID_PT_SWITCH_WEAR_STATUS_REPORT         = 0x5E,	/* recv: 1 bytes. onoff(1)
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_REPORT_WEAR_STATUS                = 0x5F,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + status(1). */                                                         
    COMM_CMDID_PT_CHECK_EARSIDE                     = 0x60,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + earsid(1). */
    COMM_CMDID_PT_READ_HALL_STATUS                  = 0x61,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + earsid(1). */
    COMM_CMDID_PT_TEST_IR                           = 0x62,	/* recv: 0 bytes. 
                                                            rsp: 3 bytes, error code(1) + value(2). */
    COMM_CMDID_PT_TEST_IR_REPORT                    = 0x63,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_QUERY_BT_NAME                     = 0x64,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_READ_TWS_PAIRING_ADDR             = 0x65,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_DEL_TWS_PAIRING_ADDR              = 0x66,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_READ_TEST_FLAG                    = 0x67,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_WRITE_TEST_FLAG                   = 0x68,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */                                                        
    COMM_CMDID_PT_READ_SLEEP_ALGO_AUTH              = 0x69,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */  
    COMM_CMDID_PT_WRITE_DEV_COLOR                   = 0x6A,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */  
    COMM_CMDID_PT_READ_DEV_COLOR                    = 0x6B,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */ 
    COMM_CMDID_PT_SWITCH_ANC_MODE                   = 0x6C,	/* recv: 4 bytes, high(2) +  low(2). 
                                                            rsp: 1 bytes, error code(1). */ 
    COMM_CMDID_PT_QUERY_NTC_INFO                    = 0x6D,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_SWICH_CLICK_TEST                  = 0x6E,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_CLICK_TEST_REPORT                 = 0x6F,	/* recv: 0 bytes, opcode(1). 
                                                            rsp: 2 + n bytes, error code(1) + opcode(1) + opdata(n). */
    COMM_CMDID_PT_QUERY_INBOX_STATUS                = 0x70,	/* recv: 0 bytes.
                                                            rsp: 2 bytes, error code(1) + inbox(1). */
    COMM_CMDID_PT_START_LOOPBACK                    = 0x71,	/* recv: 0 bytes.
                                                            rsp: 1 byte, error code(1). */
    COMM_CMDID_PT_STOP_LOOPBACK                     = 0x72,	/* recv: 0 bytes.
                                                            rsp: 1 byte, error code(1). */
                                                            
    /***** 与APP交互指令 *****/
    COMM_CMDID_APP_QUERY_DEV_INFO                   = 0x81,
	COMM_CMDID_APP_QUERY_DEV_STATUS                 = 0x82,
    

} sndp_comm_cmd_id_e;


typedef uint32_t (*sndp_comm_cmd_exec_func)(sndp_comm_cmd_info_s *cmd_info);
#if defined(__SNDP_SLEEP_APP__)
typedef uint32_t (*sleep_app_comm_cmd_exec_func)(sleep_app_comm_cmd_info_s *cmd_info);
#endif
typedef struct {
	uint8_t cmd_id;
	const char *name;
	sndp_comm_cmd_exec_func cmd_exec_hdlr;
} sndp_comm_cmd_handle_s;

#if defined(__SNDP_SLEEP_APP__)
typedef struct {
	uint8_t cmd_id;
	const char *name;
	sleep_app_comm_cmd_exec_func cmd_exec_hdlr;
} sndp_sleep_comm_cmd_handle_s;
#endif

uint32_t sndp_comm_cmd_send_lr_sync_dev_info(void);
uint32_t sndp_comm_cmd_send_lr_sync_bat_info(void);
uint32_t sndp_comm_cmd_send_lr_sync_iobox_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_cover_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_wear_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_gesture(uint8_t gesture);
uint32_t sndp_comm_cmd_send_lr_sync_language_switch(uint8_t language);
uint32_t sndp_comm_cmd_send_lr_sync_both_shutdown(void);
uint32_t sndp_comm_cmd_send_lr_sync_mobile_connected(void);
uint32_t sndp_comm_cmd_send_lr_sync_disconnect_and_tws_pair(void);
uint32_t sndp_comm_cmd_send_lr_sync_music_ctrl(uint8_t event);
uint32_t sndp_comm_cmd_send_lr_sync_call_ctrl(uint8_t event);
uint32_t sndp_comm_cmd_send_lr_sync_all_dev_status(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_lr_sync_bt_onoff(uint8_t onoff);
#if defined(__SNDP_SLEEP_APP__)
uint32_t sndp_comm_cmd_send_lr_sync_sleep_snapshot(uint8_t *data, uint16_t data_len);
#endif
uint32_t sndp_comm_cmd_send_lr_sync_sleep_mode(uint8_t mode);


uint32_t sndp_comm_cmd_send_pt_test_touch(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_pt_test_ir(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_pt_report_wear_status(uint8_t status);
uint32_t sndp_comm_cmd_send_pt_test_gsensor_report(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_pt_click_test_report(uint8_t tap_event);

bool sndp_comm_cmd_is_log_output_enabled(void);

int32_t sndp_comm_execute_cmd_hdlr(sndp_comm_cmd_info_s *cmd);

#if defined(__SNDP_SLEEP_APP__)
int32_t sleep_comm_execute_cmd_hdlr(sleep_app_comm_cmd_info_s *cmd);
typedef enum {
    SLEEP_APP_CMDID_SET_EQ_MODE = 0x02, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_MODE = 0x03, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + dev status(1). */
    SLEEP_APP_CMDID_SET_EQ_PARAM = 0x04, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_PARAM = 0x05, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 2 bytes, error code(1) + param value(1). */ 
    SLEEP_APP_CMDID_FIND_MY_EARPHONE = 0x06, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */   
    SLEEP_APP_CMDID_SET_ANC_MODE = 0x07, /* recv: 1 bytes, anc mode(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ANC_MODE = 0x08, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + anc mode(1). */
    SLEEP_APP_CMDID_PPG_SETING = 0x0A, /* recv: 1 bytes, ppg setting(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_NOTIFICATION = 0x0B, /* recv: 1 bytes, ppg notification(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION = 0x0C, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION = 0x0D, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_BATTERY_STATUS = 0x0E, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_DEVICE_INFO = 0x0F, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_ENABLE = 0x1A, /* recv: 1 bytes, touch enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_VOICE_PROMPT_ENABLE = 0x1B, /* recv: 1 bytes, voice prompt enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_KEY_MAPPING = 0x17, /* recv: 1 bytes, touch key mapping(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_TOUCH_KEY_MAPPING = 0x18, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SMART_PLAY_PAUSE = 0x29, /* recv: 1 bytes, smart play/pause(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_SMART_PLAY_PAUSE = 0x2A, /* recv: 1 bytes, smart play/pause(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SETTINGS = 0x1E, /* recv: 1 bytes, settings(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_SETTINGS = 0x19, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_CONTROL = 0xFF, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_EARBUDS_STATUS_LED = 0xFE, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_AUTO_LED_ENABLE_DISABLE = 0x13, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_HEARTRATE = 0x30, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING = 0x31, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING_WITH_DUMP = 0x32, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_HEARTRATE = 0x33, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_SLEEP = 0x34, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SLEEP_TRACKING = 0x35, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_SLEEP = 0x36, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_WEAR_STATE_UPDATE = 0x62, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GESTURE_EVENT_UPDATE = 0x38, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_TAP_UPDATE = 0x39, /* recv: 0 bytes. 
                                                rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_NOTIFICATION_DEBUG = 0x50, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ACCELEROMETER_NOTIFICATION_DEBUG = 0x51, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_TEST = 0x60, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_SAMPLE_RATE_REPORT = 0x61, /* recv: 1 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_RESUME_HEART_RATE = 0x37, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
} sleep_app_cmd_id_e;

typedef enum {
    DEVICE_INFO_TAG_BT_NAME = 0x01,
    DEVICE_INFO_TAG_SN      = 0x02,
    DEVICE_INFO_TAG_FW_VER  = 0x03,
    DEVICE_INFO_TAG_HW_VER  = 0x04,
    DEVICE_INFO_TAG_ALGO_VER = 0x05,
} device_info_tag_e;

typedef struct
{
   // Byte0
    uint8_t proximity_off : 1;    // BIT0
    uint8_t proximity_on  : 1;     // BIT1
    uint8_t ppg_256hz    : 1;     // BIT2
    uint8_t ppg_128hz    : 1;     // BIT3
    uint8_t ppg_64hz     : 1;     // BIT4
    uint8_t ppg_off      : 1;     // BIT5
    uint8_t accel_off    : 1;     // BIT6
    uint8_t accel_on     : 1;     // BIT7
    
    // Byte1
    uint8_t anc_transparent  : 1;  // BIT0
    uint8_t anc_adaptive     : 1;  // BIT1
    uint8_t anc_strong       : 1;  // BIT2
    uint8_t anc_off          : 1;  // BIT3
    uint8_t voice_disable    : 1;  // BIT4
    uint8_t voice_enable     : 1;  // BIT5
    uint8_t touch_disable    : 1;  // BIT6
    uint8_t touch_enable     : 1;  // BIT7
    
    // Byte2
    uint8_t reserved0    : 1;      // BIT0
    uint8_t reserved1    : 1;      // BIT1
    uint8_t music_pause  : 1;      // BIT2
    uint8_t music_play   : 1;      // BIT3
    uint8_t smart_playpause_disable : 1;  // BIT4
    uint8_t smart_playpause_enable  : 1;  // BIT5
    uint8_t ppgledpwr_auto  : 1;      // BIT6
    uint8_t ppgledpwr_manual : 1;    // BIT7
}SndpSettingsBitMap_t;

typedef struct{
    uint8_t reserve;
    uint8_t sensor_select;
    uint8_t write_read;
    uint8_t read_lenth;
    uint8_t reg_addr;
    uint8_t write_value;
}SndpSensorCtrlMap_t;

typedef struct{
    uint8_t receive_status;
    uint8_t reg_addr;
    uint8_t select_sensor;
    uint8_t write_read; //read only
    uint8_t read_lenth;
    uint8_t read_value[3];
}SndpSensorCtrlReplyMap_t;

typedef union
{
    struct 
    {
        //byte0
        uint8_t left_battery_level : 7;
        //uint8_t left_charging_statu : 1;
        uint8_t left_iobox_statu : 1;

        //byte1
        uint8_t right_battery_level : 7;
        //uint8_t right_charging_statu : 1;
        uint8_t right_iobox_statu : 1;

        //byte2
        uint8_t cradle_battery_level : 7;
        uint8_t cradle_charging_status : 1;
    } bits;

    uint8_t charging_byte[3];
}SndpGetBattryMap_t;


uint32_t sndp_comm_cmd_sleepapp_report_hr(uint8_t* sendhr, uint8_t* dbbeats_data);
uint32_t sndp_comm_cmd_send_lr_sync_anc_mode(uint8_t ancmode,uint8_t is_save);
uint32_t sndp_comm_cmd_send_lr_sync_sleep_role_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_heart_rate_onoff(uint8_t onoff);
uint32_t sndp_comm_cmd_send_lr_sync_stage_onoff(uint8_t onoff);
uint32_t sndp_comm_cmd_sleepapp_report_sleep_stage(int8_t *sleep_stage,
                                                    uint16_t position_and_control,
                                                    int16_t result_code);
uint32_t sndp_comm_cmd_sleepapp_report_ppg_ntf(int32_t *ppg_raw_data, uint16_t ppg_raw_len);
uint32_t sndp_comm_cmd_sleepapp_report_ppg_raw_data(uint8_t *ppg_raw_data, uint16_t ppg_raw_len);
uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf(int16_t *acc_raw_data, uint16_t acc_raw_len);
uint32_t sndp_comm_cmd_sleepapp_report_acc_ntf_debug(int16_t *acc_raw_data, uint16_t acc_raw_len);
uint32_t sndp_comm_cmd_sleepapp_wear_state_update(uint8_t lR_flag, uint8_t wear_state);
uint8_t sndp_get_findme_vol(void);
void sndp_sleep_app_report_battery(void);
void sndp_comm_cmd_sleepapp_report_acc_samples(uint16_t sensor_samples);
void sndp_comm_cmd_sleepapp_report_ppg_samples(uint16_t sensor_samples);
void sndp_sleep_comm_cmd_analysis_stop(void);
void sndp_sleep_comm_cmd_heartrate_stop(void);
void sndp_sleep_comm_disconnect_timer_handler(void);
uint32_t sndp_comm_cmd_sleepapp_proximity_task(void);
#ifdef __SNDP_SEND_GESTURE__
void sndp_sleep_app_report_gesture(uint32_t gesture);
void sndp_sleep_app_report_tap(void);
#endif

uint32_t sndp_sleep_app_report_anc_mode(void);



#if defined(__SNDP_GESTURE_MAP__)
typedef struct{
    uint8_t ear_side;
    uint8_t key_behavior;
    uint8_t key_function;
}gesture_map_t;

#endif
#endif
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_MGR__ */
#endif /* __SNDP_COMM_CMD_H__ */

