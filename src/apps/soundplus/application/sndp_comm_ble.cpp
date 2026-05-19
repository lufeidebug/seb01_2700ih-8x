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
#include "factory_section.h"
#include "nvrecord_extension.h"
#include "nvrecord_env.h"
#include "ble_core_common.h"
#include "app_custom.h"
#include "gap_service.h"
#include "bes_gap_api.h"
#include "app_ibrt_customif_cmd.h"
#include "bts_core_if.h"
#include "app_ble_adv.h"

#include "sndp_if_device.h"
#include "sndp_if_common.h"
#include "sndp_if_platform.h"

#include "sndp_comm_main.h"
#include "sndp_comm_ble.h"

#ifndef CFG_APP_DATAPATH_SERVER
#define CFG_APP_DATAPATH_SERVER
#endif
#include "app_datapaths.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_COMM_BLE_SEND_BUF_SIZE			(256)

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

static uint8_t sndp_comm_ble_send_queue_buf[SNDP_COMM_BLE_SEND_BUF_SIZE*3];
static uint8_t sndp_comm_ble_send_pop_buf[SNDP_COMM_BLE_SEND_BUF_SIZE];

osTimerDef(BLE_SEND_TIMEOUT_TIMER, sndp_comm_ble_send_timeout_timer_handler);
static osTimerId ble_send_timeout_timer = NULL;

#if defined(__SNDP_COMM_BLE_ADV_SET__)
typedef struct {
    uint8_t public_addr[6];  // 广播地址
    bool    synced;          // 是否已同步
} sndp_ble_addr_ctx_t;
static sndp_ble_addr_ctx_t g_ble_addr_ctx = {0};
#endif

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



#if defined(__SNDP_COMM_BLE_ADV_SET__)

/* 获取地址 */
sndp_ble_addr_ctx_t* sndp_ble_addr_get_ctx(void)
{
    return &g_ble_addr_ctx;
}

bool sndp_comm_ble_activity_prepare(ble_adv_activity_t *adv);

/* 初始化 */
void sndp_ble_advertising_init(void)
{   
    COMM_BLE_TRACE(0, ".");
    ble_bdaddr_t addr = {{0}};
    addr = bes_ble_gap_get_current_ble_addr(); //

    memcpy(g_ble_addr_ctx.public_addr, addr.addr, 6);
    g_ble_addr_ctx.synced = false;

    ble_adv_activity_t *ble_create_adv;
    ble_create_adv=app_ble_register_advertising(BLE_BASIC_ADV_HANDLE, BLE_ADV_USAGE_DEFAULT, sndp_comm_ble_activity_prepare);
    if (ble_create_adv == NULL) {
        COMM_BLE_TRACE(0, "SNDP ADV Register FAIL!!!");
    }    

}

/* 获取当前广播地址 */
uint8_t* sndp_ble_get_public_addr(void)
{
    return g_ble_addr_ctx.public_addr;
}
/* 主耳：发送public地址 */
void sndp_master_send_ble_public_addr(void)
{
    if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode())
    {
        COMM_BLE_TRACE(0, "[TWS] slave mode, dont send public addr");
        return;
    }
    COMM_BLE_TRACE(0, "done.");

    g_ble_addr_ctx.synced = true;
	tws_ctrl_send_cmd(APP_TWS_CMD_SNDP_BLE_PUBLIC_ADDR_SYNC, g_ble_addr_ctx.public_addr, 6);
}

/* 从耳：接收public地址 */
void sndp_ble_receive_master_public_addr(uint8_t *addr, uint16_t length)
{
    COMM_BLE_TRACE(0, "done.");
    if(addr == NULL || length != 6) {
        COMM_BLE_TRACE(0, "addr == null or length != 6, return");
        return;
    }

    memcpy(g_ble_addr_ctx.public_addr, addr, 6);
    g_ble_addr_ctx.synced = true;
}

void sndp_ble_set_public_addr(void)
{
    COMM_BLE_TRACE(0, ".");
    uint8_t *addr = sndp_ble_get_public_addr();
    COMM_BLE_TRACE(0, "public_addr=%s", addr);
    bes_ble_gap_set_public_address((const bt_bdaddr_t *)addr);//api-set public addr!!!
}


bool sndp_comm_ble_activity_prepare(ble_adv_activity_t *adv)
{

    sndp_ble_set_public_addr();//ble addr set

    if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode())
    {
        COMM_BLE_TRACE(0, "[TWS] slave mode, disable advertising");
        return false;
    }


    gap_adv_param_t *adv_param = &adv->adv_param;

    adv->adv_handle = BLE_BASIC_ADV_HANDLE;
    adv->user = USER_SNDP_BLE;
    adv_param->connectable = true;
    adv_param->scannable = true;
    adv_param->use_legacy_pdu = false;
    adv_param->include_tx_power_data = true;

    app_ble_set_adv_tx_power_level(adv, BLE_ADV_TX_POWER_LEVEL_0);

    app_ble_dt_set_flags(adv_param, false);

    uint8_t* ble_name = factory_section_get_ble_name();
    int ble_name_len = strlen((char *)ble_name);
    //app_ble_dt_set_local_name(adv_param, (char *)ble_name);
#if 0
    BLE_ADV_ACTIVITY_USER_E actv_user = app_ble_param_get_actv_user_from_adv_user(adv->user);
    COMM_BLE_TRACE(1, "adv->user=%d", (adv->user));
    COMM_BLE_TRACE(1, "BLE_ADV_ACTIVITY_USER_E=%d", actv_user);
#endif

    struct nvrecord_env_t *nvrecord_env;
    uint8_t local_ble_addr[6] = {0};
    uint8_t local_bt_addr[6] = {0};
    bt_bdaddr_t peer_bt_addr;
    uint8_t adv_data[64];
    uint8_t adv_data_size = 0;
    uint8_t scan_rsp_data[32];
    uint8_t scan_rsp_data_size = 0;
    uint8_t adv_data_max = sizeof(adv_data);

    factory_section_original_bleaddr_get(local_ble_addr);
    factory_section_original_btaddr_get(local_bt_addr);
    
    nv_record_env_get(&nvrecord_env);
    if(nvrecord_env->ibrt_mode.mode != 0xFF) {
        memcpy(&peer_bt_addr, &nvrecord_env->ibrt_mode.record.bdAddr, sizeof(bt_bdaddr_t));
    } else {
        memset(&peer_bt_addr, 0, sizeof(bt_bdaddr_t));
    }

    adv_data[adv_data_size++] = 0x02;
    adv_data[adv_data_size++] = 0x01;
    adv_data[adv_data_size++] = 0x06;

    //Manufacturer Type Data
    adv_data[adv_data_size++] = 0x0E; //Manufacturer Length
    adv_data[adv_data_size++] = 0xFF; //Manufacturer Type

    //Manufacturer: Commpany ID
    adv_data[adv_data_size++] = 0x00;
    adv_data[adv_data_size++] = 0x00; 

    //Manufacturer: Device Product Key
    adv_data[adv_data_size++] = 0x00;
    adv_data[adv_data_size++] = 0x00; 

    //Manufacturer: BLE MAC addr
    for(int i = 0; i < 6; i++) {
        adv_data[adv_data_size++] = local_ble_addr[5-i];
    }

    //Manufacturer: Mobile connect status
    if(sndp_is_tws_slave_mode())
        adv_data[adv_data_size++] = sndp_is_master_mobile_link_connected(); 
    else
        adv_data[adv_data_size++] = sndp_is_slave_ibrt_link_connected(); 

    //Manufacturer: TWS connect status
    adv_data[adv_data_size++] = sndp_is_tws_link_connected(); 

    //Manufacturer: Reserved
    adv_data[adv_data_size++] = 0x00; 


    //BT name Type Data
    //uint8_t* ble_name = factory_section_get_ble_name();
    //int ble_name_len = strlen((char *)ble_name);
    if(ble_name_len > adv_data_max - adv_data_size) {
        ble_name_len = adv_data_max - adv_data_size;
    } 

    adv_data[adv_data_size++] = 1 + ble_name_len; 
    adv_data[adv_data_size++] = 0x09; 
    memcpy(&adv_data[adv_data_size++], ble_name, ble_name_len);
    adv_data_size += ble_name_len;

    //Classic BT package Type Data
    if(adv_data_max - adv_data_size >= 6) {
        adv_data[adv_data_size++] = 0x07; 
        adv_data[adv_data_size++] = 0x1B; 
        if(nvrecord_env->ibrt_mode.mode != 0xFF) {
            for(int i = 0; i < 6; i++) {
                adv_data[adv_data_size++] = peer_bt_addr.address[i];
            }
        } else {
            for(int i = 0; i < 6; i++) {
                adv_data[adv_data_size++] = local_bt_addr[i];
            }
        }
    } else {
        scan_rsp_data[scan_rsp_data_size++] = 0x07; 
        scan_rsp_data[scan_rsp_data_size++] = 0x1B; 
        if(nvrecord_env->ibrt_mode.mode != 0xFF) {
            for(int i = 0; i < 6; i++) {
                scan_rsp_data[scan_rsp_data_size++] = peer_bt_addr.address[i];
            }
        } else {
            for(int i = 0; i < 6; i++) {
                scan_rsp_data[scan_rsp_data_size++] = local_bt_addr[i];
            }
        }
    }
    gap_dt_add_raw_data(&adv_param->adv_data, adv_data, sizeof(adv_data));

    gap_dt_add_raw_data(&adv_param->scan_rsp_data, scan_rsp_data, sizeof(scan_rsp_data));

    COMM_BLE_TRACE(0, "SUCCESS!!!");
    return true;
 }
#endif

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

    
#if defined(__SNDP_COMM_BLE_ADV_SET__)
    sndp_ble_advertising_init();
    app_ble_refresh_adv_state_generic();
    COMM_BLE_TRACE(0, "SNDP ADV INIT SUCC!!!");
#endif
    
	return 0;
}


#endif	/* __SNDP_COMM_BLE__ */


