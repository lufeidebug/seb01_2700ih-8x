#ifndef __SLEEPWAVE_APP_PROTOCOL_H__
#define __SLEEPWAVE_APP_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif
#define __SLEEP_COMM_PROTOCOL_TRACE__
#if defined(__SLEEP_COMM_PROTOCOL_TRACE__)
#define SLEEP_PROTOCOL_TRACE(num, str, ...)            SNDP_TRACE(1 + num, "[COMM_PROTOCOL] %s, " str, __func__, ##__VA_ARGS__)
#else
#define SLEEP_PROTOCOL_TRACE(num, str, ...)
#endif

#define AppFlag 0x574D43

typedef enum {
	SLEEP_APP_ERROR_NONE                    = 0x00,
    SLEEP_APP_ERROR_INVALID_FRAME_LEN       = 0x01,
	SLEEP_APP_ERROR_INVALID_HEAD_FLAG       = 0x02,
	SLEEP_APP_ERROR_INVALID_FROMTO          = 0x03,
	SLEEP_APP_ERROR_INVALID_PATH            = 0x04,
	SLEEP_APP_ERROR_INVALID_CMDID           = 0x05,
	SLEEP_APP_ERROR_INVALID_DATA_LEN        = 0x06,
	SLEEP_APP_ERROR_INVALID_CRC             = 0x07,
	SLEEP_APP_ERROR_WAITTING_MORE           = 0x08,
	SLEEP_APP_ERROR_INVALID_BUF_SIZE        = 0x0A,
    
	SLEEP_APP_ERROR_SYS_BUSY                = 0x10,
	SLEEP_APP_ERROR_CALLING                 = 0x11,
	SLEEP_APP_ERROR_NOT_WEARED              = 0x12,
	SLEEP_APP_ERROR_BAT_LOW                 = 0x13,
	SLEEP_APP_ERROR_TWS_DISCONNECTED        = 0x14,
	SLEEP_APP_ERROR_NOT_SUPPORT             = 0x15,
	SLEEP_APP_ERROR_SAVE_FAIL               = 0x16,
	SLEEP_APP_ERROR_READ_FAIL               = 0x17,
	SLEEP_APP_ERROR_NULL_POINTER            = 0x18,

	SLEEP_APP_ERROR_PARAM_LEN_INVALID       = 0x20,
	SLEEP_APP_ERROR_PARAM_OUT_RANG          = 0x21,
	
	
} sleep_app_error_code_e;

#define SLEEP_APP_FLAG_LEN (3)
#define SLEEP_APP_DATA_LEN (1)
#define SLEEP_APP_CMD_LEN (1)
#define SLEEP_APP_COMM_HEAD_LEN (SLEEP_APP_FLAG_LEN + SLEEP_APP_DATA_LEN + SLEEP_APP_CMD_LEN)
#define SLEEP_APP_COMM_DATA_LEN_MAX      (210)

typedef struct {
    uint32_t    flag;
    uint8_t     data_len;
    uint8_t     cmd;          
    uint8_t     value[SLEEP_APP_COMM_DATA_LEN_MAX];
} sleep_app_comm_cmd_info_s;

uint32_t sleep_protocol_parse_recv_data(uint8_t *recv_data, uint16_t recv_len, sleep_app_comm_cmd_info_s *cmd_info);
uint16_t sleep_protocol_pack_send_data(sleep_app_comm_cmd_info_s *cmd_info, uint8_t *send_buf, uint16_t send_buf_size);

#ifdef __cplusplus
}
#endif
#endif