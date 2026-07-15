#if defined(__SNDP_COMM_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_comm_protocol.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_COMM_CRC_INIT_VAL			(0xA1B2)


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/
extern "C" int32_t bt_sco_chain_bypass_tx_algo(uint32_t sel_ch);



/**************************************************************************************************
* Variable
**************************************************************************************************/


/**************************************************************************************************
* Function
**************************************************************************************************/

static uint16_t sndp_comm_protocol_calc_crc16(uint16_t crc, const uint8_t *buff, uint32_t start, uint32_t end)
{
    ASSERT(buff != NULL, "%s, buff == NULL", __func__);
	ASSERT(end >= start, "%s, end(%d) < start(%d)", __func__, end, start);

	for(uint32_t i = start; i < end; i++) {
        crc = (crc >> 8) | (crc << 8);
        crc ^= buff[i];
        crc ^= ((uint8_t) crc) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xFF) << 5;
    }

    return crc;    
}


uint32_t sndp_comm_protocol_parse_recv_data(uint8_t *recv_data, uint16_t recv_len, sndp_comm_cmd_info_s *cmd_info)											
{
    ASSERT(recv_data != NULL, "%s, data == NULL", __func__);
    ASSERT(cmd_info != NULL, "%s, cmd == NULL", __func__);
    
#if 1
	COMM_PROTOCOL_TRACE(0, "recv_len=%d, recv_data:", recv_len);
	DUMP8("%02X ", recv_data, (recv_len > 32) ? (32) : (recv_len));
#endif

	if(recv_len < SNDP_COMM_FRAME_HEAD_LEN + SNDP_COMM_FRAME_CRC_LEN) {
		COMM_PROTOCOL_TRACE(3, "recv_len(%d)<HEAD+CRC(%d), wait more, return", 
				recv_len, SNDP_COMM_FRAME_HEAD_LEN + SNDP_COMM_FRAME_CRC_LEN);
		return SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

	uint8_t flag = recv_data[SNDP_COMM_FRAME_FLAG_IDX];
    uint8_t fromto = recv_data[SNDP_COMM_FRAME_FROM_IDX];
	uint8_t from = (fromto>>4)&0x0f;
    uint8_t to = fromto&0x0f;
	uint8_t path = recv_data[SNDP_COMM_FRAME_PATH_IDX];
	uint8_t cmd_id = recv_data[SNDP_COMM_FRAME_CMDID_IDX];
	uint16_t cmd_data_len = (uint16_t)(((recv_data[SNDP_COMM_FRAME_DLEN_IDX]&0xff)<<8)|(recv_data[SNDP_COMM_FRAME_DLEN_IDX+1]&0xff));


	if(flag != SNDP_COMM_FRAME_FLAG) {
		COMM_PROTOCOL_TRACE(1, "Invalid flag(%02X), return", flag);
		return SNDP_COMM_ERROR_INVALID_HEAD_FLAG;
	}

    
	if((from != SNDP_COMM_DEVICE_LEFT)
		&& (from != SNDP_COMM_DEVICE_RIGHT)
		&& (from != SNDP_COMM_DEVICE_BOX)
		&& (from != SNDP_COMM_DEVICE_ATE)
		&& (from != SNDP_COMM_DEVICE_APP)) {
		COMM_PROTOCOL_TRACE(1, "Invalid from(%02X), return", from);
		return SNDP_COMM_ERROR_INVALID_FROMTO;
	} 
        
    if((to != SNDP_COMM_DEVICE_LEFT)
		&& (to != SNDP_COMM_DEVICE_RIGHT)
		&& (to != SNDP_COMM_DEVICE_LR)
		&& (to != SNDP_COMM_DEVICE_BOX)
		&& (to != SNDP_COMM_DEVICE_ATE)
		&& (to != SNDP_COMM_DEVICE_APP)) {
		COMM_PROTOCOL_TRACE(1, "Invalid to(%02X), return", to);
		return SNDP_COMM_ERROR_INVALID_FROMTO;
	}

	if((path != SNDP_COMM_PATH_TRACE_UART)
		&& (path != SNDP_COMM_PATH_POGOPIN)
		&& (path != SNDP_COMM_PATH_SPP)
		&& (path != SNDP_COMM_PATH_BLE)
		&& (path != SNDP_COMM_PATH_MS)) {
		COMM_PROTOCOL_TRACE(1, "Invalid path(%02X), return", path);
		return SNDP_COMM_ERROR_INVALID_PATH;
	}
	
	if(cmd_data_len > SNDP_COMM_FRAME_CMD_DLEN_MAX) {
		COMM_PROTOCOL_TRACE(1, "Invalid cmd_data_len(%d) > MAX(%d), return", cmd_data_len, SNDP_COMM_FRAME_CMD_DLEN_MAX);
		return SNDP_COMM_ERROR_INVALID_DATA_LEN;
	}

    if(recv_len < SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len + SNDP_COMM_FRAME_CRC_LEN ) {     
		COMM_PROTOCOL_TRACE(2, "recv_len(%d) < frame_len(%d), wait more, return", 
                recv_len, 
                SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len + SNDP_COMM_FRAME_CRC_LEN);
		return SNDP_COMM_ERROR_WAITTING_MORE;
	}


	uint16_t crc_idx = SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len;
	uint16_t recv_crc = (uint16_t)(((recv_data[crc_idx]&0xff)<<8)|(recv_data[crc_idx + 1]&0xff));
	uint16_t calc_crc = sndp_comm_protocol_calc_crc16(SNDP_COMM_CRC_INIT_VAL, recv_data, 0, SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len);
	if(recv_crc != calc_crc) {
		COMM_PROTOCOL_TRACE(2, "Invalid CRC, recv(%04X) != calc(%04X), return", recv_crc, calc_crc);
		return SNDP_COMM_ERROR_INVALID_CRC;
	}

	cmd_info->flag = flag;
	cmd_info->fromto = fromto;
	cmd_info->path = path;
	cmd_info->cmd_id = cmd_id;
	cmd_info->data_len = cmd_data_len;
	memcpy(&cmd_info->data[0], &recv_data[SNDP_COMM_FRAME_DATA_IDX], cmd_data_len);
	
	COMM_PROTOCOL_TRACE(2, "Received command(%02X), cmd_data_len=%d", cmd_id, cmd_data_len);
	return SNDP_COMM_ERROR_NONE;
}


uint16_t sndp_comm_protocol_pack_send_data(sndp_comm_cmd_info_s *cmd_info, uint8_t *send_buf, uint16_t send_buf_size)
{
	if(cmd_info == NULL) {
		COMM_PROTOCOL_TRACE(0, "cmd_info == NULL, return");
		return 0;
	}

	if(send_buf == NULL) {
		COMM_PROTOCOL_TRACE(0, "send_buf == NULL, return");
		return 0;
	}

	if(cmd_info->data_len + SNDP_COMM_FRAME_HEAD_LEN + SNDP_COMM_FRAME_CRC_LEN > send_buf_size) {
		COMM_PROTOCOL_TRACE(2, "HEAD(%d)+cmd_data_len(%d)+CRC(%d) > send_buf_size(%d), return", 
            SNDP_COMM_FRAME_HEAD_LEN,
			cmd_info->data_len, 
			SNDP_COMM_FRAME_CRC_LEN,
			send_buf_size);
        
		return 0;
	}

	uint16_t send_len = 0;
	send_buf[send_len++] = cmd_info->flag;
	send_buf[send_len++] = cmd_info->fromto;
	send_buf[send_len++] = cmd_info->path;
	send_buf[send_len++] = cmd_info->cmd_id;
	send_buf[send_len++] = (uint8_t)((cmd_info->data_len>>8) & 0xff);
	send_buf[send_len++] = (uint8_t)(cmd_info->data_len & 0xff);
	memcpy(&send_buf[send_len], cmd_info->data, cmd_info->data_len);
	send_len += cmd_info->data_len;
	
	uint16_t crc16 = sndp_comm_protocol_calc_crc16(SNDP_COMM_CRC_INIT_VAL, send_buf, 0, send_len);
	send_buf[send_len++] = (uint8_t)((crc16>>8)&0xff);
	send_buf[send_len++] = (uint8_t)(crc16&0xff);

	return send_len;
}

uint16_t sndp_comm_protocol_find_next_frame_idx(uint8_t *recv_data, uint16_t data_len)
{
    ASSERT(recv_data != NULL, "%s, recv_data == NULL", __func__);

    uint16_t idx = 0xffff;
    bool found = false;
    uint32_t sleepFlag = 0;
		uint8_t *flagaddr = (uint8_t*)&sleepFlag;
    
    for(uint16_t i = 1; i < data_len; i++) {
        if(recv_data[i] == SNDP_COMM_FRAME_FLAG) {
            idx = i;
            found = true;
            break;
        }
				((uint8_t*)flagaddr)[2] = recv_data[i+0];
				((uint8_t*)flagaddr)[1] = recv_data[i+1];
				((uint8_t*)flagaddr)[0] = recv_data[i+2];
				if(sleepFlag == 0x574D43) {
					COMM_PROTOCOL_TRACE(2, "find next frame idx(%d)", i);
					idx = i;
					found = true;
					break;
				}
    }

    if(found) {
        return idx; 
    } else {
        return data_len;
    }
    
}

bool sndp_comm_protocol_data_is_valid(uint8_t *recv_data, uint16_t recv_len)											
{
    if(recv_data == NULL) {
        return false;
    }
    
	uint8_t flag = recv_data[SNDP_COMM_FRAME_FLAG_IDX];
	uint16_t cmd_data_len = (uint16_t)((recv_data[SNDP_COMM_FRAME_DLEN_IDX]<<8)|(recv_data[SNDP_COMM_FRAME_DLEN_IDX+1]));

	if(flag != SNDP_COMM_FRAME_FLAG) {
        COMM_PROTOCOL_TRACE(1, "Invalid flag(%02X), return", flag);
		return false;
	}
	
	if(cmd_data_len > SNDP_COMM_FRAME_CMD_DLEN_MAX) {
        COMM_PROTOCOL_TRACE(1, "Invalid cmd_data_len(%d) > MAX(%d), return", cmd_data_len, SNDP_COMM_FRAME_CMD_DLEN_MAX);
		return false;
	}

    if(recv_len < SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len + SNDP_COMM_FRAME_CRC_LEN ) {    
        COMM_PROTOCOL_TRACE(2, "recv_len(%d) < frame_len(%d), wait more, return", 
                recv_len, 
                SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len + SNDP_COMM_FRAME_CRC_LEN);
		return false;
	}

	uint16_t crc_idx = SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len;
	uint16_t recv_crc = (uint16_t)((recv_data[crc_idx]<<8)|(recv_data[crc_idx + 1]));
	uint16_t calc_crc = sndp_comm_protocol_calc_crc16(SNDP_COMM_CRC_INIT_VAL, recv_data, 0, SNDP_COMM_FRAME_HEAD_LEN + cmd_data_len);
    
	if(recv_crc != calc_crc) {
        COMM_PROTOCOL_TRACE(2, "Invalid CRC, recv(%04X) != calc(%04X), return", recv_crc, calc_crc);
		return false;
	}

	return true;
}


#endif	/* __SNDP_COMM_MGR__ */


