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
    COMM_CMDID_LR_SYNC_PROMPT_ONOFF                 = 0x2A, /* desc: 左右耳同步提示音开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */
    COMM_CMDID_LR_SYNC_UPDATE_MAPPING               = 0x2B, /* desc: 左右耳同步更新按键映射指令。
                                                             * recv: 2 bytes, key behavior(1) + key function(1)
                                                             * rsp : 0 bytes */       
    COMM_CMDID_LR_SYNC_GESTURE_ONOFF                = 0x2C, /* desc: 左右耳同步手势使能开关指令。
                                                             * recv: 1 bytes, onoff(1)
                                                             * rsp : 0 bytes */           
    COMM_CMDID_LR_SYNC_ALL_DEV_STATUS               = 0x2D, /* desc: 左右耳同步所有设备状态指令。
                                                             * recv: n bytes, 
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
    COMM_CMDID_PT_QUERY_DEV_SN                      = 0x4C, /* recv: 0 bytes. 
                                                            rsp: 21 bytes, error code(1) + sn(20). */
    COMM_CMDID_PT_WRITE_DEV_SN                      = 0x4D, /* recv: <=20 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_QUERY_BT_ADDR                     = 0x4E, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
	COMM_CMDID_PT_SET_BT_ADDR                       = 0x4F, /* recv: 6 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_QUERY_BLE_ADDR                    = 0x50, /* recv: 0 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_SET_BLE_ADDR                      = 0x51, /* recv: 0 bytes. 
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_QUERY_RF_FREQUENCY_OFFSET         = 0x52, /* recv: 0 bytes. 
                                                            rsp: 7 bytes, error code(1) + bt mac(6). */
	COMM_CMDID_PT_WRITE_RF_FREQUENCY_OFFSET         = 0x53, /* recv: 0 bytes. 
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
    COMM_CMDID_PT_SWITCH_WEAR_STATUS_REPORT         = 0x5F,	/* recv: 1 bytes. onoff(1)
                                                            rsp: 1 bytes, error code(1). */
    COMM_CMDID_PT_REPORT_WEAR_STATUS                = 0x60,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + status(1). */                                                         
    COMM_CMDID_PT_CHECK_EARSIDE                     = 0x61,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + earsid(1). */
    COMM_CMDID_PT_READ_HALL_STATUS                  = 0x62,	/* recv: 0 bytes. 
                                                            rsp: 2 bytes, error code(1) + earsid(1). */


    /***** 与APP交互指令 *****/
    COMM_CMDID_APP_QUERY_DEV_INFO                   = 0x81,
	COMM_CMDID_APP_QUERY_DEV_STATUS                 = 0x82,
    

} sndp_comm_cmd_id_e;


typedef uint32_t (*sndp_comm_cmd_exec_func)(sndp_comm_cmd_info_s *cmd_info);
typedef uint32_t (*sleep_app_comm_cmd_exec_func)(sleep_app_comm_cmd_info_s *cmd_info);

typedef struct {
	uint8_t cmd_id;
	const char *name;
	sndp_comm_cmd_exec_func cmd_exec_hdlr;
} sndp_comm_cmd_handle_s;

typedef struct {
	uint8_t cmd_id;
	const char *name;
	sleep_app_comm_cmd_exec_func cmd_exec_hdlr;
} sndp_sleep_comm_cmd_handle_s;

uint32_t sndp_comm_cmd_send_lr_sync_dev_info(void);
uint32_t sndp_comm_cmd_send_lr_sync_bat_info(void);
uint32_t sndp_comm_cmd_send_lr_sync_iobox_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_cover_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_wear_status(uint8_t status);
uint32_t sndp_comm_cmd_send_lr_sync_gesture(uint8_t gesture);
uint32_t sndp_comm_cmd_send_lr_sync_language_switch(uint8_t language);
uint32_t sndp_comm_cmd_send_lr_sync_both_shutdown(void);
uint32_t sndp_comm_cmd_send_lr_sync_music_ctrl(uint8_t event);
uint32_t sndp_comm_cmd_send_lr_sync_call_ctrl(uint8_t event);
uint32_t sndp_comm_cmd_send_lr_sync_all_dev_status(uint8_t *data, uint16_t data_len);


uint32_t sndp_comm_cmd_send_pt_test_touch(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_pt_test_ir(uint8_t *data, uint16_t data_len);
uint32_t sndp_comm_cmd_send_pt_report_wear_status(uint8_t status);

int32_t sndp_comm_execute_cmd_hdlr(sndp_comm_cmd_info_s *cmd);
int32_t sleep_comm_execute_cmd_hdlr(sleep_app_comm_cmd_info_s *cmd);


typedef enum {
    SLEEP_APP_CMDID_SET_EQ_MODE = 0x01, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_MODE = 0x02, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + dev status(1). */
    SLEEP_APP_CMDID_SET_EQ_PARAM = 0x03, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_EQ_PARAM = 0x04, /* recv: 1 bytes, ctrl cmd(1). 
                                                    rsp: 2 bytes, error code(1) + param value(1). */ 
    SLEEP_APP_CMDID_FIND_MY_EARPHONE = 0x05, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */   
    SLEEP_APP_CMDID_SET_ANC_MODE = 0x06, /* recv: 1 bytes, anc mode(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_ANC_MODE = 0x07, /* recv: 0 bytes. 
                                                    rsp: 2 bytes, error code(1) + anc mode(1). */
    SLEEP_APP_CMDID_PPG_SETING = 0x08, /* recv: 1 bytes, ppg setting(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_PROXIMITY_NOTIFICATION = 0x09, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_BATTERY_STATUS = 0x0A, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_DEVICE_INFO = 0x0B, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_ENABLE = 0x0C, /* recv: 1 bytes, touch enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_VOICE_PROMPT_ENABLE = 0x0D, /* recv: 1 bytes, voice prompt enable(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_TOUCH_KEY_MAPPING = 0x0E, /* recv: 1 bytes, touch key mapping(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_TOUCH_KEY_MAPPING = 0x0F, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SMART_PLAY_PAUSE = 0x10, /* recv: 1 bytes, smart play/pause(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SET_SETTINGS = 0x11, /* recv: 1 bytes, settings(1). 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_GET_SETTINGS = 0x12, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SENSOR_CONTROL = 0x13, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_EARBUDS_STATUS_LED = 0x14, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_PPG_AUTO_LED_ENABLE_DISABLE = 0x15, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_HEARTRATE = 0x16, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING = 0x17, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_HEARTRATE_MEASURING_WITH_DUMP = 0x18, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_HEARTRATE = 0x19, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_START_SLEEP = 0x1A, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_SLEEP_TRACKING = 0x1B, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
    SLEEP_APP_CMDID_STOP_SLEEP = 0x1C, /* recv: 0 bytes. 
                                                    rsp: 1 bytes, error code(1). */
} sleep_app_cmd_id_e;


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_MGR__ */
#endif /* __SNDP_COMM_CMD_H__ */

