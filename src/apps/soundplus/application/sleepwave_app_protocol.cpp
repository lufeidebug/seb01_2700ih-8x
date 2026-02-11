/*
*
*
*Mery Sleep Project customer app comunication protocol
*
*
*
*/
#if defined(__SNDP_COMM_MGR__)

#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "sleepwave_app_protocol.h"

uint32_t sleep_protocol_parse_recv_data(uint8_t *recv_data, uint16_t recv_len, sleep_app_comm_cmd_info_s *cmd_info)
{
    ASSERT(recv_data != NULL, "%s, data == NULL", __func__);
    ASSERT(cmd_info != NULL, "%s, cmd == NULL", __func__);  

    uint32_t flag = (recv_data[0]<<16)|(recv_data[1]<<8)|(recv_data[2]);
    uint8_t data_len = recv_data[3];
    uint8_t cmd_id = recv_data[4];
    uint8_t *cmd_data = &recv_data[5];
    
    if(recv_len < SLEEP_APP_COMM_HEAD_LEN) {
        SLEEP_PROTOCOL_TRACE(2, "recv_len(%d) < SLEEP_APP_COMM_HEAD_LEN(%d), return", recv_len, SLEEP_APP_COMM_HEAD_LEN);
        return SLEEP_APP_ERROR_INVALID_DATA_LEN;
    }

    if(data_len > SLEEP_APP_COMM_DATA_LEN_MAX) {
        SLEEP_PROTOCOL_TRACE(2, "data_len(%d) > SLEEP_APP_COMM_DATA_LEN_MAX(%d), return", data_len, SLEEP_APP_COMM_DATA_LEN_MAX);
        return SLEEP_APP_ERROR_INVALID_DATA_LEN;
    }

    if(recv_len < SLEEP_APP_COMM_HEAD_LEN + data_len) {
        SLEEP_PROTOCOL_TRACE(2, "recv_len(%d) < SLEEP_APP_COMM_HEAD_LEN(%d) + data_len(%d), return", recv_len, SLEEP_APP_COMM_HEAD_LEN, data_len);
        return SLEEP_APP_ERROR_INVALID_DATA_LEN;
    }

    if(flag != AppFlag) {
        SLEEP_PROTOCOL_TRACE(2, "Invalid flag(0x%06X), return", flag);
        return SLEEP_APP_ERROR_INVALID_HEAD_FLAG;
    }
    
    if(data_len > 0 && cmd_data == NULL) {
        SLEEP_PROTOCOL_TRACE(2, "data_len(%d) > 0 but cmd_data == NULL, return", data_len);
        return SLEEP_APP_ERROR_INVALID_DATA_LEN;
    }
    
    cmd_info->flag = flag;
    cmd_info->data_len = data_len;
    cmd_info->cmd = cmd_id;
    memcpy(cmd_info->value, cmd_data, data_len);
    
    return SLEEP_APP_ERROR_NONE;
}

uint16_t sleep_protocol_pack_send_data(sleep_app_comm_cmd_info_s *cmd_info, uint8_t *send_buf, uint16_t send_buf_size)
{
    if(cmd_info == NULL) {
        SLEEP_PROTOCOL_TRACE(2, "cmd_info == NULL, return");
        return 0;
    }

    if(send_buf == NULL) {
        SLEEP_PROTOCOL_TRACE(2, "send_buf == NULL, return");
        return 0;
    }

    if(SLEEP_APP_COMM_HEAD_LEN + cmd_info->data_len > send_buf_size) {
        SLEEP_PROTOCOL_TRACE(2, "SLEEP_APP_COMM_HEAD_LEN(%d) + data_len(%d) > send_buf_size(%d), return", 
            SLEEP_APP_COMM_HEAD_LEN, cmd_info->data_len, send_buf_size);
        return 0;
    }

    uint16_t send_len = 0;
    send_buf[send_len++] = (uint8_t)((cmd_info->flag>>16)&0xFF);
    send_buf[send_len++] = (uint8_t)((cmd_info->flag>>8)&0xFF);
    send_buf[send_len++] = (uint8_t)(cmd_info->flag&0xFF);
    send_buf[send_len++] = cmd_info->data_len;
    send_buf[send_len++] = cmd_info->cmd;
    memcpy(&send_buf[send_len], cmd_info->value, cmd_info->data_len);
    send_len += cmd_info->data_len;

    return send_len;
}
#endif