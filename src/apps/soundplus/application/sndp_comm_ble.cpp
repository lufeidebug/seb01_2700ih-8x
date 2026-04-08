#if defined(__SNDP_COMM_BLE__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "cqueue.h"
#include "bta_ble_api.h"
#include "bts_ble_api.h"

#include "sndp_if_common.h"
#include "sndp_comm_main.h"
#include "sndp_comm_ble.h"

#ifndef CFG_APP_DATAPATH_SERVER
#define CFG_APP_DATAPATH_SERVER
#endif
#include "app_datapaths.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_COMM_BLE_SEND_BUF_SIZE			(128)

/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool inited;

    uint8_t conidx;
    sndp_comm_ble_conn_status_e conn_status;
    bool sending;
    uint16_t mtu;

} sndp_comm_ble_context_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void sndp_comm_ble_send_timeout_timer_handler(void const *param);
static void sndp_comm_ble_send_data_handle(void);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_comm_ble_context_s sndp_comm_ble_ctx;

static CQueue sndp_comm_ble_send_queue;	
static osMutexId sndp_comm_ble_send_queue_mutex_id = NULL;
osMutexDef(sndp_comm_ble_send_queue_mutex);

static uint8_t sndp_comm_ble_send_queue_buf[SNDP_COMM_BLE_SEND_BUF_SIZE];
static uint8_t sndp_comm_ble_send_pop_buf[SNDP_COMM_BLE_SEND_BUF_SIZE];

osTimerDef(BLE_SEND_TIMEOUT_TIMER, sndp_comm_ble_send_timeout_timer_handler);
static osTimerId ble_send_timeout_timer = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_comm_ble_send_queue_push_data(const uint8_t *data, uint32_t data_len)
{
    int32_t ret = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)data, data_len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
    return ret;
}

static int32_t sndp_comm_ble_send_queue_pop_data(uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)buf, len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id); 
    return ret;
}

int32_t sndp_comm_ble_send_queue_get_len(void)
{
    int32_t len = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    len = LengthOfCQueue(&sndp_comm_ble_send_queue);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
    return len;
}


static void sndp_comm_ble_send_timeout_timer_handler(void const *param)
{
	sndp_comm_ble_ctx.sending = false;
	sndp_comm_ble_send_data_handle();
}

static void sndp_comm_ble_send_data_handle(void)
{
    uint16_t send_len;
    uint16_t queue_len;

    queue_len = sndp_comm_ble_send_queue_get_len();
	if(queue_len == 0) {
		return;
	}
	
	send_len = sndp_comm_ble_ctx.mtu;
	if(queue_len < sndp_comm_ble_ctx.mtu) {
		send_len = queue_len;
	}

    sndp_comm_ble_send_queue_pop_data(sndp_comm_ble_send_pop_buf, send_len);

#if 1
    COMM_BLE_TRACE(1, "queue_len=%d, send_len=%d", queue_len, send_len);
	DUMP8("%02X ", sndp_comm_ble_send_pop_buf, send_len > 32 ? 32 : send_len);
#endif

	sndp_comm_ble_ctx.sending = true;
	osTimerStart(ble_send_timeout_timer, 100);
#ifdef CFG_APP_DATAPATH_SERVER   
	app_datapath_server_send_data_via_notification(
	        bta_ble_get_conhdl_by_conidx(sndp_comm_ble_ctx.conidx), 
	        sndp_comm_ble_send_pop_buf, 
	        send_len);
#endif	
}

int32_t sndp_comm_ble_send_data(uint8_t *data, uint16_t data_len)
{
    if(data == NULL) {
        COMM_BLE_TRACE(0, "data == null, return");
        return -1;
    }
    
    if(!sndp_comm_ble_ctx.inited) {
        COMM_BLE_TRACE(0, "not inited, return");
        return -2;
    }
    
    if(!sndp_comm_ble_is_connected()) {
        COMM_BLE_TRACE(0, "not connected, return");
        return -3;
    }
    
    
    sndp_comm_ble_send_queue_push_data(data, data_len);
    if(!sndp_comm_ble_ctx.sending) {
	    sndp_call_func_in_app_thread((uint32_t)sndp_comm_ble_send_data_handle, 0, 0, 0);
    }
	return 0;
}

POSSIBLY_UNUSED static void sndp_comm_ble_tx_done(void)
{
	COMM_BLE_ENTER();
	sndp_comm_ble_ctx.sending = false;
	osTimerStop(ble_send_timeout_timer);
	sndp_comm_ble_send_data_handle();
}

POSSIBLY_UNUSED static void sndp_comm_ble_recv_data_callback(uint8_t *p_buff, uint16_t bufLength)
{
	COMM_BLE_TRACE(1, "bufLength=%d", bufLength);
    
    sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_BLE, p_buff, bufLength);
}

POSSIBLY_UNUSED static void sndp_comm_ble_disconnected_done(uint8_t conidx)
{
	COMM_BLE_ENTER();
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_DISCONNECTED;
}

POSSIBLY_UNUSED static void sndp_comm_ble_connected_done(uint8_t conidx)
{
	COMM_BLE_ENTER();
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_CONNECTED;
    sndp_comm_ble_ctx.conidx = conidx;
#ifdef CFG_APP_DATAPATH_SERVER    
    app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
#endif
}

POSSIBLY_UNUSED static void sndp_comm_ble_mtuexchanged_done(uint8_t conidx, uint16_t mtu)
{
	COMM_BLE_TRACE(1, "mute=%d", mtu);
    if(mtu > SNDP_COMM_BLE_SEND_BUF_SIZE){
        sndp_comm_ble_ctx.mtu = SNDP_COMM_BLE_SEND_BUF_SIZE;
    } else {
        sndp_comm_ble_ctx.mtu = mtu;
    }
	
    sndp_comm_ble_ctx.conidx = conidx;
    sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_CONNECTED;
#ifdef CFG_APP_DATAPATH_SERVER    
    app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
#endif    
}

sndp_comm_ble_conn_status_e sndp_comm_ble_get_conn_status(void)
{
	return sndp_comm_ble_ctx.conn_status;
}

bool sndp_comm_ble_is_connected(void)
{
    if(bts_ble_gap_is_connection_on(bta_ble_get_conhdl_by_conidx(sndp_comm_ble_ctx.conidx))) {
        return true;
    }
	return (sndp_comm_ble_ctx.conn_status == SNDP_COMM_BLE_CONNECTED) ? (true) : (false);
}


int32_t sndp_comm_ble_init(void)
{
	if(ble_send_timeout_timer == NULL)
		ble_send_timeout_timer = osTimerCreate (osTimer(BLE_SEND_TIMEOUT_TIMER), osTimerOnce, NULL);

    sndp_comm_ble_send_queue_mutex_id = osMutexCreate(osMutex(sndp_comm_ble_send_queue_mutex));
    ASSERT(sndp_comm_ble_send_queue_mutex_id != NULL, "%s, sndp_comm_ble_send_queue_mutex_id == NULL", __func__);

    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    InitCQueue(&sndp_comm_ble_send_queue, sizeof(sndp_comm_ble_send_queue_buf), sndp_comm_ble_send_queue_buf);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
	
	memset(&sndp_comm_ble_ctx, 0, sizeof(sndp_comm_ble_ctx));
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_DISCONNECTED;
	sndp_comm_ble_ctx.sending = false;
	sndp_comm_ble_ctx.mtu = 20;
    
#ifdef CFG_APP_DATAPATH_SERVER
	app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
	app_datapath_server_register_rx_done(sndp_comm_ble_recv_data_callback);
	app_datapath_server_register_disconnected_done(sndp_comm_ble_disconnected_done);
	app_datapath_server_register_connected_done(sndp_comm_ble_connected_done);
	app_datapath_server_register_mtu_exchanged_done(sndp_comm_ble_mtuexchanged_done);
#endif

    sndp_comm_ble_ctx.inited = true;
    sndp_comm_ble_ctx.conidx = 0;
	COMM_BLE_TRACE(0, "done.");
	return 0;
}


#endif	/* __SNDP_COMM_BLE__ */


