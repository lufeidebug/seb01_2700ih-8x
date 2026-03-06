#ifndef __SNDP_COMM_MAIN_H__
#define __SNDP_COMM_MAIN_H__

#if defined(__SNDP_COMM_MGR__)
#include "sndp_comm_protocol.h"
#include "sndp_comm_cmd.h"



#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_MAIN_TRACE__
#if defined(__SNDP_COMM_MAIN_TRACE__)
#define COMM_MIAN_TRACE(num, str, ...)            SNDP_TRACE(1 + num, "[COMM_MAIN] %s, " str, __func__, ##__VA_ARGS__)
#else
#define COMM_MIAN_TRACE(num, str, ...)
#endif

typedef enum {
    SNDP_COMM_INIT_ALL,
    SNDP_COMM_INIT_FOR_RF_TEST,
    
} sndp_comm_init_mode_e;

uint8_t sndp_comm_get_local_device(void);
uint8_t sndp_comm_get_peer_device(void);

int32_t sndp_comm_main_recv_queue_push_data(sndp_comm_path_id_e path_id, uint8_t *data, uint16_t data_len);

int32_t sndp_comm_main_send_data(sndp_comm_path_id_e path_id, uint8_t *data, uint16_t data_len);
int32_t sndp_comm_main_send_cmd(sndp_comm_cmd_info_s *cmd);
int32_t sndp_comm_main_send_cmd_by_id(sndp_comm_cmd_id_e cmd_id, uint8_t from, uint8_t to, uint8_t path, uint8_t *cmd_data, uint16_t cmd_data_len);
int32_t sndp_comm_main_rsp_cmd(sndp_comm_cmd_info_s *rsp_cmd);
int32_t sndp_comm_main_init(sndp_comm_init_mode_e mode);
int32_t sndp_sleep_comm_main_rsp_cmd(sleep_app_comm_cmd_info_s *rsp_cmd);

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_MGR__ */
#endif /* __SNDP_COMM_MAIN_H__ */

