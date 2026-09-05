#ifndef __SNDP_COMM_PROTOCOL_H__
#define __SNDP_COMM_PROTOCOL_H__

#if defined(__SNDP_COMM_MGR__)

#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_PROTOCOL_TRACE__
#if defined(__SNDP_COMM_PROTOCOL_TRACE__)
#define COMM_PROTOCOL_TRACE(num, str, ...)            SNDP_TRACE(1 + num, "[COMM_PROTOCOL] %s, " str, __func__, ##__VA_ARGS__)
#else
#define COMM_PROTOCOL_TRACE(num, str, ...)
#endif


/*********************************************************************************************************
* FLAG(1) + FROM_TO(1)) + PATH(1) + CMDID(1) + DLEN(2) + DATA(n=DLen) + CRC(2)
**********************************************************************************************************/

#define SNDP_COMM_FRAME_FLAG_LEN            (1)
#define SNDP_COMM_FRAME_FROM_LEN            (1)
#define SNDP_COMM_FRAME_PATH_LEN            (1)
#define SNDP_COMM_FRAME_CMDID_LEN           (1)
#define SNDP_COMM_FRAME_DLEN_LEN            (2)
#define SNDP_COMM_FRAME_CRC_LEN             (2)

#define SNDP_COMM_FRAME_FLAG_IDX            (0)
#define SNDP_COMM_FRAME_FROM_IDX            (SNDP_COMM_FRAME_FLAG_IDX + SNDP_COMM_FRAME_FLAG_LEN)
#define SNDP_COMM_FRAME_PATH_IDX            (SNDP_COMM_FRAME_FROM_IDX + SNDP_COMM_FRAME_FROM_IDX)
#define SNDP_COMM_FRAME_CMDID_IDX           (SNDP_COMM_FRAME_PATH_IDX + SNDP_COMM_FRAME_PATH_LEN)
#define SNDP_COMM_FRAME_DLEN_IDX            (SNDP_COMM_FRAME_CMDID_IDX + SNDP_COMM_FRAME_CMDID_LEN)
#define SNDP_COMM_FRAME_DATA_IDX            (SNDP_COMM_FRAME_DLEN_IDX + SNDP_COMM_FRAME_DLEN_LEN)


#define SNDP_COMM_FRAME_HEAD_LEN            (SNDP_COMM_FRAME_FLAG_LEN + \
                                                SNDP_COMM_FRAME_FROM_LEN + \
                                                SNDP_COMM_FRAME_PATH_LEN + \
                                                SNDP_COMM_FRAME_CMDID_LEN + \
                                                SNDP_COMM_FRAME_DLEN_LEN)

#define SNDP_COMM_FRAME_CMD_DLEN_MAX        (246)
#define SNDP_COMM_FRAME_LEN_MAX             (SNDP_COMM_FRAME_HEAD_LEN + SNDP_COMM_FRAME_CMD_DLEN_MAX + SNDP_COMM_FRAME_CRC_LEN)

#define SNDP_COMM_FRAME_FLAG                (0xFE)


#define SNDP_COMM_DEVICE_LEFT               (0x01)
#define SNDP_COMM_DEVICE_RIGHT              (0x02)
#define SNDP_COMM_DEVICE_LR                 (0x03)
#define SNDP_COMM_DEVICE_BOX                (0x04)
#define SNDP_COMM_DEVICE_ATE                (0x05)
#define SNDP_COMM_DEVICE_APP                (0x08)


typedef enum {
	SNDP_COMM_PATH_NONE                     = 0x00,
	SNDP_COMM_PATH_TRACE_UART	 			= 0x01,
	SNDP_COMM_PATH_POGOPIN	 			    = 0x02,
	SNDP_COMM_PATH_SPP	 					= 0x03,
	SNDP_COMM_PATH_BLE	 					= 0x04,
	SNDP_COMM_PATH_MS	 			        = 0x05,

    SNDP_COMM_PATH_CNT
} sndp_comm_path_id_e;



typedef enum {
	SNDP_COMM_ERROR_NONE                    = 0x00,
    SNDP_COMM_ERROR_INVALID_FRAME_LEN       = 0x01,
	SNDP_COMM_ERROR_INVALID_HEAD_FLAG       = 0x02,
	SNDP_COMM_ERROR_INVALID_FROMTO          = 0x03,
	SNDP_COMM_ERROR_INVALID_PATH            = 0x04,
	SNDP_COMM_ERROR_INVALID_CMDID           = 0x05,
	SNDP_COMM_ERROR_INVALID_DATA_LEN        = 0x06,
	SNDP_COMM_ERROR_INVALID_CRC             = 0x07,
	SNDP_COMM_ERROR_WAITTING_MORE           = 0x08,
	SNDP_COMM_ERROR_INVALID_BUF_SIZE        = 0x0A,
    
	SNDP_COMM_ERROR_SYS_BUSY                = 0x10,
	SNDP_COMM_ERROR_CALLING                 = 0x11,
	SNDP_COMM_ERROR_NOT_WEARED              = 0x12,
	SNDP_COMM_ERROR_BAT_LOW                 = 0x13,
	SNDP_COMM_ERROR_TWS_DISCONNECTED        = 0x14,
	SNDP_COMM_ERROR_NOT_SUPPORT             = 0x15,
	SNDP_COMM_ERROR_SAVE_FAIL               = 0x16,
	SNDP_COMM_ERROR_READ_FAIL               = 0x17,
	SNDP_COMM_ERROR_NULL_POINTER            = 0x18,
	SNDP_COMM_ERROR_EXEC_FAIL               = 0x19,

	SNDP_COMM_ERROR_PARAM_LEN_INVALID       = 0x20,
	SNDP_COMM_ERROR_PARAM_OUT_RANG          = 0x21,
	SNDP_COMM_ERROR_NOT_IN_TEST_MODE        = 0x22,
	SNDP_COMM_ERROR_BOX_BATPER_ERR          = 0x23,
	SNDP_COMM_ERROR_BT_NOT_INIT_DONE        = 0x24,
	
} sndp_comm_error_code_e;



typedef struct {
    uint8_t     flag;	/* Fixed value = SPCOMM_FRAME_FLAG */
    uint8_t     fromto;
    uint8_t     path;          
    uint8_t     cmd_id;
    uint8_t     data[SNDP_COMM_FRAME_CMD_DLEN_MAX];
    uint16_t    data_len;
} sndp_comm_cmd_info_s;


#define COMM_GET_FROM(fromto)           (((fromto)>>4)&0x0F)
#define COMM_GET_TO(fromto)             ((fromto)&0x0F)

#define COMM_IS_FROM_DEV(fromto, dev)   ((((fromto)>>4)&0x0F) == (dev))
#define COMM_IS_TO_DEV(fromto, dev)     (((fromto)&0x0F) == (dev))

uint32_t sndp_comm_protocol_parse_recv_data(uint8_t *recv_data, uint16_t recv_len, sndp_comm_cmd_info_s *cmd_info);
uint16_t sndp_comm_protocol_pack_send_data(sndp_comm_cmd_info_s *cmd_info, uint8_t *send_buf, uint16_t send_buf_size);
uint16_t sndp_comm_protocol_find_next_frame_flag_idx(uint8_t *recv_data, uint16_t data_len);
uint16_t sndp_comm_protocol_find_next_sleep_flag_idx(uint8_t *recv_data, uint16_t data_len);
bool sndp_comm_protocol_data_is_valid(uint8_t *recv_data, uint16_t recv_len);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_MGR__ */
#endif /* __SNDP_COMM_PROTOCOL_H__ */

