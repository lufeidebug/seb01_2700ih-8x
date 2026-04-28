#if defined(__SNDP_COMM_MGR__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "tgt_hardware.h"
#include "app_utils.h"
#include "cqueue.h"


#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"


#include "sndp_comm_main.h"
#if defined(__SNDP_COMM_TRACE_UART__)
#include "sndp_comm_trace_uart.h"
#endif
#if defined(__SNDP_COMM_POGOPIN__)
#include "sndp_comm_pogopin.h"
#endif
#if defined(__SNDP_COMM_BLE__)
#include "sndp_comm_ble.h"
#endif
#if defined(__SNDP_COMM_SPP__)
#include "sndp_comm_spp.h"
#endif
#if defined(__SNDP_COMM_MS__)
#include "sndp_comm_ms.h"
#endif

#include "sndp_comm_cmd.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#if defined(__SNDP_SLEEP_APP__)
static sleep_app_comm_cmd_info_s sndp_sleep_comm_recv_cmd;
static sleep_app_comm_cmd_info_s sndp_sleep_comm_send_cmd;
#endif
/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    sndp_comm_path_id_e path_id;

    //recv hdlr
    CQueue *recv_queue;
    osMutexId recv_mutex_id;
    bool changed;
    uint16_t recv_wait_more_cnt;

    //send hdlr
    int32_t (* send_data)(uint8_t *data, uint16_t data_len);
} sndp_comm_path_hdlr_s;
   


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void sndp_comm_recv_thread(void const *argument);


/**************************************************************************************************
* Variable
**************************************************************************************************/

osSemaphoreDef(sndp_comm_recv_semaphore);
osSemaphoreId sndp_comm_recv_semaphore_id;

#define SNDP_COMM_RECV_THREAD_STACK_SIZE 				(1024*2)
osThreadDef(sndp_comm_recv_thread, osPriorityNormal, 1, SNDP_COMM_RECV_THREAD_STACK_SIZE, "sndp_comm_recv_thread");
osThreadId sndp_comm_recv_thread_tid;

static sndp_comm_path_hdlr_s sndp_comm_path_hdlr_list[SNDP_COMM_PATH_CNT];
static uint8_t sndp_comm_recv_deal_buf[256];

static sndp_comm_cmd_info_s sndp_comm_recv_cmd;
static sndp_comm_cmd_info_s sndp_comm_send_cmd;
static uint8_t sndp_comm_send_frame[SNDP_COMM_FRAME_LEN_MAX];



#if defined(__SNDP_COMM_TRACE_UART__)
#define TRACE_UART_RECV_QUEUE_BUF_SIZE			    (128)

static CQueue trace_uart_recv_queue;
static osMutexId trace_uart_recv_queue_mutex_id = NULL;
osMutexDef(trace_uart_recv_queue_mutex);
static uint8_t trace_uart_recv_queue_buf[TRACE_UART_RECV_QUEUE_BUF_SIZE];

static bool trace_uart_inited = false;
#endif

#if defined(__SNDP_COMM_POGOPIN__)

#define POGOPIN_RECV_QUEUE_BUF_SIZE			    (128)

static CQueue pogopin_recv_queue;
static osMutexId pogopin_recv_queue_mutex_id = NULL;
osMutexDef(pogopin_recv_queue_mutex);
static uint8_t pogopin_recv_queue_buf[POGOPIN_RECV_QUEUE_BUF_SIZE];

static bool pogopin_inited = false;

#endif

#if defined(__SNDP_COMM_BLE__)

#define BLE_RECV_QUEUE_BUF_SIZE			    (256)

static CQueue ble_recv_queue;
static osMutexId ble_recv_queue_mutex_id = NULL;
osMutexDef(ble_recv_queue_mutex);
static uint8_t ble_recv_queue_buf[BLE_RECV_QUEUE_BUF_SIZE];

static bool ble_inited = false;

#endif

#if defined(__SNDP_COMM_SPP__)
#define SPP_RECV_QUEUE_BUF_SIZE			    (128)

static CQueue spp_recv_queue;
static osMutexId spp_recv_queue_mutex_id = NULL;
osMutexDef(spp_recv_queue_mutex);
static uint8_t spp_recv_queue_buf[SPP_RECV_QUEUE_BUF_SIZE];

static bool spp_inited = false;

#endif

#if defined(__SNDP_COMM_MS__)
#define MS_RECV_QUEUE_BUF_SIZE			    (256)

static CQueue ms_recv_queue;
static osMutexId ms_recv_queue_mutex_id = NULL;
osMutexDef(ms_recv_queue_mutex);
static uint8_t ms_recv_queue_buf[MS_RECV_QUEUE_BUF_SIZE];

static bool ms_inited = false;

#endif



/**************************************************************************************************
* Function
**************************************************************************************************/
    
uint8_t sndp_comm_get_local_device(void)
{
    if(sndp_dev_is_left_earphone()) {
        return SNDP_COMM_DEVICE_LEFT;
    } else {
        return SNDP_COMM_DEVICE_RIGHT;
    }
}

uint8_t sndp_comm_get_peer_device(void)
{
    if(sndp_dev_is_left_earphone()) {
        return SNDP_COMM_DEVICE_RIGHT;
    } else {
        return SNDP_COMM_DEVICE_LEFT;
    }
}
    

int32_t sndp_comm_queue_get_len(CQueue *queue, osMutexId mutex_id)
{
    int32_t len = 0;

    if(queue == NULL || mutex_id == NULL) {
        return 0;
    }

    osMutexWait(mutex_id, osWaitForever);
    len = LengthOfCQueue(queue);
    osMutexRelease(mutex_id);
    return len;
}

int32_t sndp_comm_queue_push_data(CQueue *queue, osMutexId mutex_id, const uint8_t *data, uint32_t data_len)
{
    int32_t ret = 0;

    if(queue == NULL || mutex_id == NULL) {
        return -1;
    }
    
    osMutexWait(mutex_id, osWaitForever);
    ret = EnCQueue(queue, (CQItemType *)data, data_len);
    osMutexRelease(mutex_id);
    return ret;
}

static int32_t sndp_comm_queue_peek_data(CQueue *queue, osMutexId mutex_id, uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if(queue == NULL || mutex_id == NULL) {
        return -1;
    }
    
    osMutexWait(mutex_id, osWaitForever);
    ret = PeekCQueueToBuf(queue, (CQItemType *)buf, len);
    osMutexRelease(mutex_id); 
    return ret;
}

static int32_t sndp_comm_queue_pop_data(CQueue *queue, osMutexId mutex_id, uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if(queue == NULL || mutex_id == NULL) {
        return -1;
    }
    
    osMutexWait(mutex_id, osWaitForever);
    ret = DeCQueue(queue, (CQItemType *)buf, len);
    osMutexRelease(mutex_id); 
    return ret;
}

static void sndp_comm_queue_set_change_status(sndp_comm_path_hdlr_s *hdlr, bool status)
{
    if(hdlr ==  NULL) {
        return;
    }
    
    osMutexWait(hdlr->recv_mutex_id, osWaitForever);
    hdlr->changed = status;
    osMutexRelease(hdlr->recv_mutex_id); 
}


static int32_t sndp_comm_main_execute_cmd(sndp_comm_cmd_info_s *cmd)
{
	ASSERT(cmd != NULL, "%s, cmd == NULL", __func__);
    COMM_MIAN_TRACE(4, "from=%02X, to=%02X, path=%02X, cmd_id=%02X, data_len=%d", 
            COMM_GET_FROM(cmd->fromto), COMM_GET_TO(cmd->fromto), cmd->path, cmd->cmd_id, cmd->data_len);

    uint8_t from = COMM_GET_FROM(cmd->fromto);
    uint8_t to = COMM_GET_TO(cmd->fromto);
    uint8_t path = cmd->path;
    
    if(from == to) {
        COMM_MIAN_TRACE(0, "from(%02X) == to(%02X), return", from, to);
        return -1;
    }
    
    if(sndp_dev_is_right_earphone()) {
        if((to == SNDP_COMM_DEVICE_RIGHT || to == SNDP_COMM_DEVICE_LR)) {
            sndp_comm_execute_cmd_hdlr(cmd);
        }

        //转发
        if((to == SNDP_COMM_DEVICE_RIGHT)
            || (from == SNDP_COMM_DEVICE_BOX && to == SNDP_COMM_DEVICE_LEFT)
            || (from == SNDP_COMM_DEVICE_LEFT && to == SNDP_COMM_DEVICE_BOX)
            || (path == SNDP_COMM_PATH_TRACE_UART)
            || (path == SNDP_COMM_PATH_POGOPIN)
            || (path == SNDP_COMM_PATH_MS)
            ) {
            // do not forward
        } else {
            if(to == SNDP_COMM_DEVICE_LR) {
                //去掉右耳标识，避免左右耳之间来回发，死循环。
                cmd->fromto = (from<<4)|SNDP_COMM_DEVICE_LEFT; 
            }

            COMM_MIAN_TRACE(0, "Right forward data, from=%02X, to=%02X", COMM_GET_FROM(cmd->fromto), COMM_GET_TO(cmd->fromto));
            sndp_comm_main_send_cmd(cmd);
        
        }
    } else if(sndp_dev_is_left_earphone()) {
        if((to == SNDP_COMM_DEVICE_LEFT || to == SNDP_COMM_DEVICE_LR)) {
            sndp_comm_execute_cmd_hdlr(cmd);
        }

        //转发
        if((to == SNDP_COMM_DEVICE_LEFT)
            || (from == SNDP_COMM_DEVICE_BOX && to == SNDP_COMM_DEVICE_RIGHT)
            || (from == SNDP_COMM_DEVICE_RIGHT && to == SNDP_COMM_DEVICE_BOX)
            || (path == SNDP_COMM_PATH_TRACE_UART)
            || (path == SNDP_COMM_PATH_POGOPIN)
            || (path == SNDP_COMM_PATH_MS)
            ) {
            // do not forward
        } else {
            
            if(to == SNDP_COMM_DEVICE_LR) {
                //去掉左耳标识，避免左右耳之间来回发，死循环。
                cmd->fromto = (from<<4)|SNDP_COMM_DEVICE_RIGHT; 
            }

            COMM_MIAN_TRACE(0, "Left forward data, from=%02X, to=%02X", COMM_GET_FROM(cmd->fromto), COMM_GET_TO(cmd->fromto));
            sndp_comm_main_send_cmd(cmd);
        
        }
    }

	return 0;
}

#if defined(__SNDP_SLEEP_APP__)
inline void SleepAppFlagGet(uint32_t *flagaddr, uint8_t *dataaddr)
{
    ((uint8_t*)flagaddr)[2] = dataaddr[0];
    ((uint8_t*)flagaddr)[1] = dataaddr[1];
    ((uint8_t*)flagaddr)[0] = dataaddr[2];
}

static int32_t sleep_app_execute_cmd_hdlr(sleep_app_comm_cmd_info_s *cmd)
{
    sleep_comm_execute_cmd_hdlr(cmd);
    return 0;
}
#endif

static void sndp_comm_recv_thread(void const *argument)
{
    uint16_t queue_len;
    uint16_t peek_len;
    uint16_t pop_len;
    uint16_t deal_buf_size = sizeof(sndp_comm_recv_deal_buf);
    uint8_t *deal_buf = sndp_comm_recv_deal_buf;
    sndp_comm_cmd_info_s *recv_cmd = &sndp_comm_recv_cmd;
    uint32_t error_code;
    sndp_comm_path_hdlr_s *path_hdlr;
    bool need_sleep = false;

    COMM_MIAN_TRACE(0, "running...\n");
    
    while(1) {

        need_sleep = true;
        for(int32_t i = 0;  i < SNDP_COMM_PATH_CNT; i++) {
            path_hdlr = &sndp_comm_path_hdlr_list[i];
            if(path_hdlr->path_id == SNDP_COMM_PATH_NONE || path_hdlr->path_id >= SNDP_COMM_PATH_CNT) {
                continue;
            }

            queue_len = sndp_comm_queue_get_len(path_hdlr->recv_queue, path_hdlr->recv_mutex_id);
            
            // COMM_MIAN_TRACE(0, "queue_len=%d, curr_cn=%d, old_cnt=%d\n", queue_len, path_hdlr->recv_curr_cnt, path_hdlr->recv_old_cnt);
            // COMM_MIAN_TRACE(0, "111path=%d, queue_len=%d\n", i, queue_len);
            /* 接收队列中数据长度大于0，并且接收到新数据，就不休眠 */
            if((queue_len > 0) && path_hdlr->changed) {
                need_sleep = false;
                break;
            }
        }

        if(need_sleep) {
            // COMM_MIAN_TRACE(0, "sleep...\n");
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_COMM, APP_SYSFREQ_32K);
            osSemaphoreWait(sndp_comm_recv_semaphore_id, osWaitForever);
            // COMM_MIAN_TRACE(0, "wakeup...\n");
        }
        app_sysfreq_req(APP_SYSFREQ_USER_SNDP_COMM, APP_SYSFREQ_52M);
        
        for(int32_t i = 0;  i < SNDP_COMM_PATH_CNT; i++) {
            path_hdlr = &sndp_comm_path_hdlr_list[i];
            if(path_hdlr->path_id == SNDP_COMM_PATH_NONE || path_hdlr->path_id >= SNDP_COMM_PATH_CNT) {
                continue;
            }
            sndp_comm_queue_set_change_status(path_hdlr, false);
            
            queue_len = sndp_comm_queue_get_len(path_hdlr->recv_queue, path_hdlr->recv_mutex_id);
            // COMM_MIAN_TRACE(0, "222path=%d, queue_len=%d\n", i, queue_len);
            if(queue_len == 0) {
                continue;
            }

            if(queue_len > deal_buf_size) {
                peek_len = deal_buf_size;
            } else {
                peek_len = queue_len;
            }

            COMM_MIAN_TRACE(0, "queue_len=%d, peek_len=%d", queue_len, peek_len);
            sndp_comm_queue_peek_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, peek_len);

            DUMP8("%02X ", deal_buf, peek_len > 32 ? 32 : peek_len);

            if(deal_buf[0] == SNDP_COMM_FRAME_FLAG) {
                error_code = sndp_comm_protocol_parse_recv_data(deal_buf, peek_len, recv_cmd);
                // COMM_MIAN_TRACE(0, "error_code=%02x", error_code);
            
                if(error_code == SNDP_COMM_ERROR_WAITTING_MORE) {
                    #if 0
                    path_hdlr->recv_wait_more_cnt += 1;
                    if(path_hdlr->recv_wait_more_cnt >= 20) {
                        path_hdlr->recv_wait_more_cnt = 0;
                        COMM_MIAN_TRACE(0, "wait more clear");
                        pop_len = sndp_comm_protocol_find_next_frame_idx(deal_buf, peek_len);
                        sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, pop_len);
                    }
                    #endif
                } else {
                    path_hdlr->recv_wait_more_cnt = 0;
                    
                    if(error_code == SNDP_COMM_ERROR_NONE) {
                        pop_len = SNDP_COMM_FRAME_HEAD_LEN + recv_cmd->data_len + SNDP_COMM_FRAME_CRC_LEN;
                        sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, pop_len);
                        sndp_comm_queue_set_change_status(path_hdlr, true);
                        sndp_comm_main_execute_cmd(recv_cmd);
                        
                    } else {
                        pop_len = sndp_comm_protocol_find_next_frame_idx(deal_buf, peek_len);
                        //COMM_MIAN_TRACE(0, "000 pop_len=%02x", pop_len);
                        sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, pop_len);
                        sndp_comm_queue_set_change_status(path_hdlr, true);
                    }
                }
            } else {
#if defined(__SNDP_SLEEP_APP__)
                uint32_t appflag = 0;
                uint16_t app_pop_len = 0;
                uint32_t sleep_app_error_code;
                sleep_app_comm_cmd_info_s app_recv_cmd;
                SleepAppFlagGet(&appflag, deal_buf);
                if(appflag == AppFlag) {
                    path_hdlr->recv_wait_more_cnt = 0;
                    // COMM_MIAN_TRACE(0, "Sleep data, appflag=0x%08X", appflag);
                    sleep_app_error_code = sleep_protocol_parse_recv_data(deal_buf, peek_len, &app_recv_cmd);

                    if(sleep_app_error_code == SLEEP_APP_ERROR_NONE) {
                        app_pop_len = SLEEP_APP_COMM_HEAD_LEN + app_recv_cmd.data_len - SLEEP_APP_CMD_LEN; //SLEEP_APP_COMM_HEAD_LEN + data_len
                        // COMM_MIAN_TRACE(0, "Sleep cmd(%02X), data_len=%d app_pop_len=%d", app_recv_cmd.cmd, app_recv_cmd.data_len, app_pop_len);
                        sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, app_pop_len);
                        sndp_comm_queue_set_change_status(path_hdlr, true);
                        sleep_app_execute_cmd_hdlr(&app_recv_cmd);
                    } else {
                        COMM_MIAN_TRACE(0, "recv invalid Sleep data, error_code=%d", sleep_app_error_code);
                        pop_len = sndp_comm_protocol_find_next_frame_idx(deal_buf, peek_len);
                        //COMM_MIAN_TRACE(0, "111 pop_len=%02x", pop_len);
                        sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, pop_len);
                        sndp_comm_queue_set_change_status(path_hdlr, true);
                    }
                } else {
                    COMM_MIAN_TRACE(0, "recv unknown data, appflag=0x%08X", appflag);
#endif
                    path_hdlr->recv_wait_more_cnt = 0;
                    pop_len = sndp_comm_protocol_find_next_frame_idx(deal_buf, peek_len);
                    //COMM_MIAN_TRACE(0, "111 pop_len=%02x", pop_len);
                    sndp_comm_queue_pop_data(path_hdlr->recv_queue, path_hdlr->recv_mutex_id, deal_buf, pop_len);
                    sndp_comm_queue_set_change_status(path_hdlr, true);
#if defined(__SNDP_SLEEP_APP__)
                }
#endif
            }
            
        }
        
    }
}


static int32_t sndp_comm_main_add_path_hdlr(sndp_comm_path_hdlr_s *hdlr) 
{
    ASSERT(hdlr != NULL, "%s, hdlr == NULL", __func__);
    ASSERT(hdlr->path_id < SNDP_COMM_PATH_CNT, "%s, hdlr->path_id(%d) >= SNDP_COMM_PATH_CNT(%d)", __func__, hdlr->path_id, SNDP_COMM_PATH_CNT);
    ASSERT(hdlr->recv_queue != NULL, "%s, hdlr->recv_queue == NULL", __func__);
    ASSERT(hdlr->recv_mutex_id != 0, "%s, hdlr->recv_mutex_id == 0", __func__);
    ASSERT(hdlr->send_data != NULL, "%s, hdlr->send_data == NULL", __func__);

    memcpy(&sndp_comm_path_hdlr_list[hdlr->path_id], hdlr, sizeof(sndp_comm_path_hdlr_s));
    return 0;
}


int32_t sndp_comm_main_recv_queue_push_data(sndp_comm_path_id_e path_id, uint8_t *data, uint16_t data_len) 
{
    sndp_comm_path_hdlr_s *hdlr;
    
    ASSERT(data != NULL, "%s, data == NULL", __func__);
    ASSERT(path_id < SNDP_COMM_PATH_CNT, "%s, path_id(%d) >= SNDP_COMM_PATH_CNT(%d)", __func__, path_id, SNDP_COMM_PATH_CNT);

    //COMM_MIAN_TRACE(0, "000 path_id=%d, data_len=%d", path_id, data_len);
    
    hdlr = &sndp_comm_path_hdlr_list[path_id];
    sndp_comm_queue_push_data(hdlr->recv_queue, hdlr->recv_mutex_id, data, data_len);
    sndp_comm_queue_set_change_status(hdlr, true);
    osSemaphoreRelease(sndp_comm_recv_semaphore_id);
    //COMM_MIAN_TRACE(0, "done");
    return 0;
}


int32_t sndp_comm_main_send_data(sndp_comm_path_id_e path_id, uint8_t *data, uint16_t data_len)
{
	sndp_comm_path_hdlr_s *hdlr;
    
    ASSERT(data != NULL, "%s, data == NULL", __func__);
    ASSERT(path_id < SNDP_COMM_PATH_CNT, "%s, path_id(%d) >= SNDP_COMM_PATH_CNT(%d)", __func__, path_id, SNDP_COMM_PATH_CNT);
    
    hdlr = &sndp_comm_path_hdlr_list[path_id];
    if(hdlr->send_data) {
        hdlr->send_data(data, data_len);
    } else {
        COMM_MIAN_TRACE(0, "hdlr[%d]->send_data == NULL", path_id);
    }
	return 0;
}

int32_t sndp_comm_main_send_cmd(sndp_comm_cmd_info_s *cmd)
{
    uint32_t send_frame_len;
    
    if(cmd == NULL) {
        return -1;
    }

    SNDP_TRACE_IMM(0, "\n");
    COMM_MIAN_TRACE(4, "from=%02X, to=%02X, path=%02X, cmd_id=%02X, data_len=%d", 
            COMM_GET_FROM(cmd->fromto), COMM_GET_TO(cmd->fromto), cmd->path, cmd->cmd_id, cmd->data_len);

    send_frame_len = sndp_comm_protocol_pack_send_data(cmd, sndp_comm_send_frame, sizeof(sndp_comm_send_frame));
    if(send_frame_len == 0) {
        return -2;
    }

    DUMP8("%02X ", sndp_comm_send_frame, (send_frame_len > 32) ? (32) : (send_frame_len));
    SNDP_TRACE(0, "\n");
    

    uint8_t from = COMM_GET_FROM(cmd->fromto);
    uint8_t to = COMM_GET_TO(cmd->fromto);
    uint8_t path = cmd->path;
    
    sndp_comm_path_id_e send_path = SNDP_COMM_PATH_NONE;
    
    
    if(to == SNDP_COMM_DEVICE_RIGHT || to == SNDP_COMM_DEVICE_LEFT) {
        send_path = SNDP_COMM_PATH_MS;
        
    } else if(to == SNDP_COMM_DEVICE_BOX) {
        send_path = SNDP_COMM_PATH_POGOPIN;
        
    } else if(to == SNDP_COMM_DEVICE_ATE) {
        if(SNDP_COMM_PATH_BLE == path) {
            if(false) {
                
#if defined(__SNDP_COMM_BLE__)                
            } else if(sndp_comm_ble_is_connected()) {
                send_path = SNDP_COMM_PATH_BLE;
#endif                
            } else if(from == SNDP_COMM_DEVICE_RIGHT && sndp_dev_is_right_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
                
            } else if(from == SNDP_COMM_DEVICE_LEFT && sndp_dev_is_left_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
            }
            
        } else if(SNDP_COMM_PATH_SPP == path) {
            if(false) {
                
#if defined(__SNDP_COMM_SPP__)                
            } else if(sndp_comm_spp_is_connected()) {
                send_path = SNDP_COMM_PATH_SPP;
#endif                
            } else if(from == SNDP_COMM_DEVICE_RIGHT && sndp_dev_is_right_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
                
            } else if(from == SNDP_COMM_DEVICE_LEFT && sndp_dev_is_left_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
            }
        
        } else if(SNDP_COMM_PATH_POGOPIN == path) {
            send_path = SNDP_COMM_PATH_POGOPIN;
            
        } else if(SNDP_COMM_PATH_TRACE_UART == path) {
            send_path = SNDP_COMM_PATH_TRACE_UART;
            
        }

    } else if(to == SNDP_COMM_DEVICE_APP) {
        if(SNDP_COMM_PATH_BLE == path) {
            if(false) {
                
#if defined(__SNDP_COMM_BLE__)                
            } else if(sndp_comm_ble_is_connected()) {
                send_path = SNDP_COMM_PATH_BLE;
#endif
            } else if(from == SNDP_COMM_DEVICE_RIGHT && sndp_dev_is_right_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
                
            } else if(from == SNDP_COMM_DEVICE_LEFT && sndp_dev_is_left_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
            }
            
        } else if(SNDP_COMM_PATH_SPP == path) {
            if(false) {
                            
#if defined(__SNDP_COMM_SPP__)                
            } else if(sndp_comm_spp_is_connected()) {
                send_path = SNDP_COMM_PATH_SPP;
#endif                
            } else if(from == SNDP_COMM_DEVICE_RIGHT && sndp_dev_is_right_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
                
            } else if(from == SNDP_COMM_DEVICE_LEFT && sndp_dev_is_left_earphone()) {
                send_path = SNDP_COMM_PATH_MS;
            }
        }   
    }

    sndp_comm_main_send_data(send_path, sndp_comm_send_frame, send_frame_len);
    return 0;
}

#if defined(__SNDP_SLEEP_APP__)
int32_t sleep_app_comm_main_send_cmd(sleep_app_comm_cmd_info_s *cmd)
{
    if(cmd == NULL) {
        return -1;
    }
    sndp_comm_path_id_e send_path = SNDP_COMM_PATH_NONE;
    uint32_t send_frame_len =sleep_protocol_pack_send_data(cmd, sndp_comm_send_frame, sizeof(sndp_comm_send_frame));
    if(send_frame_len == 0) {
        return -2;
    }

    DUMP8("%02X ", sndp_comm_send_frame, (send_frame_len > 32) ? (32) : (send_frame_len));
    SNDP_TRACE(0, "\n");

#if defined(__SNDP_COMM_BLE__)                
        if(sndp_comm_ble_is_connected()) {
                send_path = SNDP_COMM_PATH_BLE;
        }
#endif
#if defined(__SNDP_COMM_SPP__)                
        if(sndp_comm_spp_is_connected()) {
                send_path = SNDP_COMM_PATH_SPP;
        }
#endif    
    sndp_comm_main_send_data(send_path, sndp_comm_send_frame, SLEEP_APP_COMM_HEAD_LEN + cmd->data_len);
    return 0;
}
int32_t sleep_app_comm_main_send_cmd_by_id(sleep_app_cmd_id_e cmd_id, uint8_t datalen, uint8_t *cmd_data)
{
    sleep_app_comm_cmd_info_s *cmd = &sndp_sleep_comm_send_cmd;
    
    cmd->flag = AppFlag;
    cmd->data_len = datalen + SLEEP_APP_CMD_LEN;
    cmd->cmd = cmd_id;
    if(datalen > 0 && datalen < SLEEP_APP_COMM_DATA_LEN_MAX && cmd_data != NULL) {
        memcpy(cmd->value, cmd_data, datalen);
    }

    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}
#endif

int32_t sndp_comm_main_send_cmd_by_id(sndp_comm_cmd_id_e cmd_id, uint8_t from, uint8_t to, uint8_t path, uint8_t *cmd_data, uint16_t cmd_data_len)
{
    sndp_comm_cmd_info_s *cmd = &sndp_comm_send_cmd;
    
    cmd->flag = SNDP_COMM_FRAME_FLAG;
    cmd->fromto = ((from&0x0F)<<4)|(to&0x0F);
    cmd->path = path;
    cmd->cmd_id = cmd_id;
    cmd->data_len = cmd_data_len;
    if(cmd_data_len > 0 && cmd_data_len < SNDP_COMM_FRAME_CMD_DLEN_MAX && cmd_data != NULL) {
        memcpy(cmd->data, cmd_data, cmd_data_len);
    }
    
    sndp_comm_main_send_cmd(cmd);
    return 0;
}

int32_t sndp_comm_main_rsp_cmd(sndp_comm_cmd_info_s *rsp_cmd)
{
    sndp_comm_cmd_info_s *cmd = &sndp_comm_send_cmd;
    uint8_t from, to;

    if(rsp_cmd == NULL) {
        return -1;
    }

    cmd->flag = SNDP_COMM_FRAME_FLAG;
    to = COMM_GET_FROM(rsp_cmd->fromto);
    from = sndp_dev_is_left_earphone() ? SNDP_COMM_DEVICE_LEFT : SNDP_COMM_DEVICE_RIGHT;
    cmd->fromto = ((from&0x0F)<<4)|(to&0x0F);
    cmd->path = rsp_cmd->path;
    cmd->cmd_id = rsp_cmd->cmd_id;
    cmd->data_len = rsp_cmd->data_len;
    if(cmd->data_len > 0 && cmd->data_len  < SNDP_COMM_FRAME_CMD_DLEN_MAX) {
        memcpy(cmd->data, rsp_cmd->data, rsp_cmd->data_len);
    }

    COMM_MIAN_TRACE(4, "from=%02X, to=%02X, path=%02X, cmd_id=%02X, data_len=%d", 
            COMM_GET_FROM(cmd->fromto), COMM_GET_TO(cmd->fromto), cmd->path, cmd->cmd_id, cmd->data_len);
    
    sndp_comm_main_send_cmd(cmd);
    return 0;
}

#if defined(__SNDP_SLEEP_APP__)
int32_t sndp_sleep_comm_main_rsp_cmd(sleep_app_comm_cmd_info_s *rsp_cmd)
{
    sleep_app_comm_cmd_info_s *cmd = &sndp_sleep_comm_recv_cmd;
    if(rsp_cmd == NULL) {
        return -1;
    }
    cmd->flag = AppFlag;
    cmd->cmd = rsp_cmd->cmd;
    cmd->data_len = rsp_cmd->data_len;
    if(cmd->data_len > 0 && cmd->data_len < SLEEP_APP_COMM_DATA_LEN_MAX)
        memcpy(cmd->value, rsp_cmd->value, cmd->data_len);

    COMM_MIAN_TRACE(0, "Sleep cmd(%02X), data_len=%d", cmd->cmd, cmd->data_len);

    sleep_app_comm_main_send_cmd(cmd);
    return 0;
}
#endif

#if defined(__SNDP_COMM_TRACE_UART__)
void sndp_comm_main_trace_uart_init(void)
{
    if(!trace_uart_inited) {
        if(trace_uart_recv_queue_mutex_id == NULL) {
            trace_uart_recv_queue_mutex_id = osMutexCreate(osMutex(trace_uart_recv_queue_mutex));
            ASSERT(trace_uart_recv_queue_mutex_id != NULL, "%s, trace_uart_recv_queue_mutex_id == NULL", __func__);
        }
        osMutexWait(trace_uart_recv_queue_mutex_id, osWaitForever);
        InitCQueue(&trace_uart_recv_queue, sizeof(trace_uart_recv_queue_buf), trace_uart_recv_queue_buf);
        osMutexRelease(trace_uart_recv_queue_mutex_id);

        sndp_comm_path_hdlr_s path_hdlr;
    	path_hdlr.path_id = SNDP_COMM_PATH_TRACE_UART;
        path_hdlr.recv_queue = &trace_uart_recv_queue;
        path_hdlr.recv_mutex_id = trace_uart_recv_queue_mutex_id;
        path_hdlr.recv_wait_more_cnt = 0;
        path_hdlr.send_data = sndp_comm_trace_uart_send_data;
        sndp_comm_main_add_path_hdlr(&path_hdlr);

    	sndp_comm_trace_uart_init();

        trace_uart_inited = true;
    }
}

#endif


#if defined(__SNDP_COMM_POGOPIN__)
void sndp_comm_main_pogopin_init(void)
{
    if(!pogopin_inited) {
        if(pogopin_recv_queue_mutex_id == NULL) {
        	pogopin_recv_queue_mutex_id = osMutexCreate(osMutex(pogopin_recv_queue_mutex));
            ASSERT(pogopin_recv_queue_mutex_id != NULL, "%s, pogopin_recv_queue_mutex_id == NULL", __func__);
        }
        osMutexWait(pogopin_recv_queue_mutex_id, osWaitForever);
        InitCQueue(&pogopin_recv_queue, sizeof(pogopin_recv_queue_buf), pogopin_recv_queue_buf);
        osMutexRelease(pogopin_recv_queue_mutex_id);

        sndp_comm_path_hdlr_s path_hdlr;
    	path_hdlr.path_id = SNDP_COMM_PATH_POGOPIN;
        path_hdlr.recv_queue = &pogopin_recv_queue;
        path_hdlr.recv_mutex_id = pogopin_recv_queue_mutex_id;
        path_hdlr.recv_wait_more_cnt = 0;
        path_hdlr.send_data = sndp_comm_pogopin_send_data;
        sndp_comm_main_add_path_hdlr(&path_hdlr);
    	
    	sndp_comm_pogopin_init();
        
        pogopin_inited = true;
    }
}
#endif


#if defined(__SNDP_COMM_BLE__)
void sndp_comm_main_ble_init(void)
{
    if(!ble_inited) {
        if(ble_recv_queue_mutex_id == NULL) {
        	ble_recv_queue_mutex_id = osMutexCreate(osMutex(ble_recv_queue_mutex));
            ASSERT(ble_recv_queue_mutex_id != NULL, "%s, ble_recv_queue_mutex_id == NULL", __func__);
        }
        osMutexWait(ble_recv_queue_mutex_id, osWaitForever);
        InitCQueue(&ble_recv_queue, sizeof(ble_recv_queue_buf), ble_recv_queue_buf);
        osMutexRelease(ble_recv_queue_mutex_id);

        sndp_comm_path_hdlr_s path_hdlr;
    	path_hdlr.path_id = SNDP_COMM_PATH_BLE;
        path_hdlr.recv_queue = &ble_recv_queue;
        path_hdlr.recv_mutex_id = ble_recv_queue_mutex_id;
        path_hdlr.recv_wait_more_cnt = 0;
        path_hdlr.send_data = sndp_comm_ble_send_data;
        sndp_comm_main_add_path_hdlr(&path_hdlr);

    	sndp_comm_ble_init();
        
        ble_inited = true;
    }
}

#endif


#if defined(__SNDP_COMM_SPP__)
void sndp_comm_main_spp_init(void)
{
    if(!spp_inited) {
        if(spp_recv_queue_mutex_id == NULL) {
        	spp_recv_queue_mutex_id = osMutexCreate(osMutex(spp_recv_queue_mutex));
            ASSERT(spp_recv_queue_mutex_id != NULL, "%s, spp_recv_queue_mutex_id == NULL", __func__);
        }
        osMutexWait(spp_recv_queue_mutex_id, osWaitForever);
        InitCQueue(&spp_recv_queue, sizeof(spp_recv_queue_buf), spp_recv_queue_buf);
        osMutexRelease(spp_recv_queue_mutex_id);

        sndp_comm_path_hdlr_s path_hdlr;
    	path_hdlr.path_id = SNDP_COMM_PATH_SPP;
        path_hdlr.recv_queue = &spp_recv_queue;
        path_hdlr.recv_mutex_id = spp_recv_queue_mutex_id;
        path_hdlr.recv_wait_more_cnt = 0;
        path_hdlr.send_data = sndp_comm_spp_send_data;
        sndp_comm_main_add_path_hdlr(&path_hdlr);

    	sndp_comm_spp_init();
        
        spp_inited = true;
    }
}
#endif


#if defined(__SNDP_COMM_MS__)
void sndp_comm_main_ms_init(void)
{
    if(!ms_inited) {
        if(ms_recv_queue_mutex_id == NULL) {
        	ms_recv_queue_mutex_id = osMutexCreate(osMutex(ms_recv_queue_mutex));
            ASSERT(ms_recv_queue_mutex_id != NULL, "%s, ms_recv_queue_mutex_id == NULL", __func__);
        }
        
        osMutexWait(ms_recv_queue_mutex_id, osWaitForever);
        InitCQueue(&ms_recv_queue, sizeof(ms_recv_queue_buf), ms_recv_queue_buf);
        osMutexRelease(ms_recv_queue_mutex_id);

        sndp_comm_path_hdlr_s path_hdlr;
    	path_hdlr.path_id = SNDP_COMM_PATH_MS;
        path_hdlr.recv_queue = &ms_recv_queue;
        path_hdlr.recv_mutex_id = ms_recv_queue_mutex_id;
        path_hdlr.recv_wait_more_cnt = 0;
        path_hdlr.send_data = sndp_comm_ms_send_data;
        sndp_comm_main_add_path_hdlr(&path_hdlr);

    	sndp_comm_ms_init();
        
        ms_inited = true;
    }
}
#endif


  
int32_t sndp_comm_main_init(sndp_comm_init_mode_e mode)
{
    sndp_comm_recv_semaphore_id = osSemaphoreCreate(osSemaphore(sndp_comm_recv_semaphore), 0);
    ASSERT(sndp_comm_recv_semaphore_id != NULL, "%s, sndp_comm_recv_semaphore_id == NULL", __func__);

    sndp_comm_recv_thread_tid = osThreadCreate(osThread(sndp_comm_recv_thread), NULL);
    ASSERT(sndp_comm_recv_thread_tid != NULL, "%s, sndp_comm_recv_thread_tid == NULL", __func__);


    if(mode == SNDP_COMM_INIT_BASIC) {
        
#if defined(__SNDP_COMM_TRACE_UART__)
        sndp_comm_main_trace_uart_init();
#endif
#if defined(__SNDP_COMM_POGOPIN__)
        sndp_comm_main_pogopin_init();
#endif

    } else if(mode == SNDP_COMM_INIT_ALL) {
	
#if defined(__SNDP_COMM_TRACE_UART__)
    	sndp_comm_main_trace_uart_init();
#endif
#if defined(__SNDP_COMM_POGOPIN__)
    	sndp_comm_main_pogopin_init();
#endif
#if defined(__SNDP_COMM_BLE__)
    	sndp_comm_main_ble_init();
#endif
#if defined(__SNDP_COMM_SPP__)
    	sndp_comm_main_spp_init();
#endif
#if defined(__SNDP_COMM_MS__)
        sndp_comm_main_ms_init();
#endif

	} else if(mode == SNDP_COMM_INIT_RF_TEST) {
        
#if defined(__SNDP_COMM_TRACE_UART__)
        sndp_comm_main_trace_uart_init();
#endif
#if defined(__SNDP_COMM_POGOPIN__)
        sndp_comm_main_pogopin_init();
#endif

	}

	COMM_MIAN_TRACE(0, "done.");
    return 0;
}

#endif	/* __SNDP_COMM_MGR__ */


