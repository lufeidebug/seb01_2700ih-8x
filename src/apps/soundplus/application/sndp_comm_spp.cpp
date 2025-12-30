#if defined(__SNDP_COMM_SPP__)
#include "stdio.h"
#include "string.h"
#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "spp_service.h"
#include "cqueue.h"
#include "app_tws_ibrt_conn_api.h"
#include "app_bt.h"
#include "btapp.h"
#include "app_tws_ibrt_conn.h"


#include "sndp_if_common.h"
#include "sndp_comm_main.h"
#include "sndp_comm_spp.h"
#include "app_ibrt_middleware.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define COMM_SPP_RFCOMM_CHANNEL_NUM			    RFCOMM_CHANNEL_CUSTOM_1   


#if defined(__3M_PACK__)
#define COMM_SPP_L2CAP_MTU                      (980)
#else
#define COMM_SPP_L2CAP_MTU                      (672)
#endif
#define COMM_SPP_MAX_PACKET_SIZE                (COMM_SPP_L2CAP_MTU)
#define COMM_SPP_MAX_PACKET_NUM                 (2)


#define COMM_SPP_SEND_PACK_SIZE                 (256)
#define COMM_SPP_SEND_QUEUE_BUF_SIZE            (COMM_SPP_SEND_PACK_SIZE * 2)

/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool inited;
    
    bt_spp_channel_t *pSppDevice;
    bool is_connected;
	bool is_sending;
} sndp_comm_spp_context_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void sndp_comm_spp_send_data_exec(void);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_comm_spp_context_s comm_spp_ctx;

static CQueue comm_spp_send_queue;
static osMutexId comm_spp_send_queue_mutex_id = NULL;
osMutexDef(comm_spp_send_queue_mutex);
static uint8_t comm_spp_send_queue_buf[COMM_SPP_SEND_QUEUE_BUF_SIZE];

static uint8_t comm_spp_send_buf[COMM_SPP_SEND_PACK_SIZE];


/** SPP SDP Entries */
static const uint8_t sndp_comm_spp_uuid_128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x53, 0x4E, 0x44, 0x50
};

/** ServiceClassIDList */
static const U8 sndp_comm_spp_class_id[] = {
    SDP_ATTRIB_HEADER_8BIT(17),        /* Data Element Sequence, 17 bytes */
    SDP_UUID_128BIT(sndp_comm_spp_uuid_128), /* 128 bit UUID in Big Endian */
};

static const U8 sndp_comm_spp_protocol_desc_list[] = {
    SDP_ATTRIB_HEADER_8BIT(12), /* Data element sequence, 12 bytes */

    /* Each element of the list is a Protocol descriptor which is a 
     * data element sequence. The first element is L2CAP which only 
     * has a UUID element.  
     */
    SDP_ATTRIB_HEADER_8BIT(3), /* Data element sequence for L2CAP, 3 
                                  * bytes 
                                  */

    SDP_UUID_16BIT(PROT_L2CAP), /* Uuid16 L2CAP */

    /* Next protocol descriptor in the list is RFCOMM. It contains two 
     * elements which are the UUID and the channel. Ultimately this 
     * channel will need to filled in with value returned by RFCOMM.  
     */

    /* Data element sequence for RFCOMM, 5 bytes */
    SDP_ATTRIB_HEADER_8BIT(5),

    SDP_UUID_16BIT(PROT_RFCOMM), /* Uuid16 RFCOMM */

    /* Uint8 RFCOMM channel number - value can vary */
    SDP_UINT_8BIT(COMM_SPP_RFCOMM_CHANNEL_NUM)};

/*
 * BluetoothProfileDescriptorList
 */
static const U8 sndp_comm_spp_profile_desc_list[] = {
#if 1
     SDP_ATTRIB_HEADER_8BIT(22), /* Data element sequence, 22 bytes */

    /* Data element sequence for ProfileDescriptor, 20 bytes */
    SDP_ATTRIB_HEADER_8BIT(20),

    SDP_UUID_128BIT(sndp_comm_spp_uuid_128), /* Uuid128 SPP */
    
#else
    SDP_ATTRIB_HEADER_8BIT(8), /* Data element sequence, 8 bytes */

    /* Data element sequence for ProfileDescriptor, 6 bytes */
    SDP_ATTRIB_HEADER_8BIT(6),

    SDP_UUID_16BIT(SC_SERIAL_PORT), /* Uuid16 SPP */
#endif

    SDP_UINT_16BIT(0x0102)          /* As per errata 2239 */
};

/*
 * * OPTIONAL *  ServiceName
 */
static const U8 sndp_comm_spp_service_name[] = {
    SDP_TEXT_8BIT(5), /* Null terminated text string */
    'S', 'N', 'D', 'P', '\0'};

/* SPP attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * SPP SDP record.
 */
static bt_sdp_record_attr_t sndp_comm_spp_sdp_attributes[] = {

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, sndp_comm_spp_class_id),

    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, sndp_comm_spp_protocol_desc_list),

    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, sndp_comm_spp_profile_desc_list),

    /* SPP service name*/
    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), sndp_comm_spp_service_name),
};

/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_comm_spp_send_queue_get_len(void)
{
    int32_t len = 0;

    osMutexWait(comm_spp_send_queue_mutex_id, osWaitForever);
    len = LengthOfCQueue(&comm_spp_send_queue);
    osMutexRelease(comm_spp_send_queue_mutex_id);
    return len;
}

int32_t sndp_comm_spp_send_queue_push_data(const uint8_t *data, uint32_t data_len)
{
    int32_t ret = 0;

    osMutexWait(comm_spp_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&comm_spp_send_queue, (CQItemType *)data, data_len);
    osMutexRelease(comm_spp_send_queue_mutex_id);
    return ret;
}

static int32_t sndp_comm_spp_send_queue_pop_data(uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    
    osMutexWait(comm_spp_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&comm_spp_send_queue, (CQItemType *)buf, len);
    osMutexRelease(comm_spp_send_queue_mutex_id); 
    return ret;
}

static void sndp_comm_spp_send_data_timeout(void)
{
    comm_spp_ctx.is_sending = false;
    sndp_comm_spp_send_data_exec();
}

static void sndp_comm_spp_send_data_exec(void)
{
    bt_status_t ret = BT_STS_SUCCESS;
    int32_t queue_len;
    int32_t send_len;
    
	if(!comm_spp_ctx.is_connected) {
		COMM_SPP_TRACE(0, "SPP not connected, return");
		return;
	}

    if(comm_spp_ctx.is_sending) {
		COMM_SPP_TRACE(0, "is_sending, delay exec");
        sndp_delay_exec_start((uint32_t)100, (uint32_t)sndp_comm_spp_send_data_exec, 0, 0, 0);
		return;
	}

    queue_len = sndp_comm_spp_send_queue_get_len();
    if(queue_len == 0) {
        COMM_SPP_TRACE(0, "queue_len==0, return");
        return;
    }

    if(queue_len >= COMM_SPP_SEND_PACK_SIZE) {
        send_len = COMM_SPP_SEND_PACK_SIZE;
    } else {
        send_len = queue_len;
    }

    sndp_comm_spp_send_queue_pop_data(comm_spp_send_buf, send_len);
    
    COMM_SPP_TRACE(1, "send_len:%d", send_len);
    ret = bt_spp_write(comm_spp_ctx.pSppDevice->rfcomm_handle, comm_spp_send_buf, send_len);

    if (BT_STS_SUCCESS != ret) {
        COMM_SPP_TRACE(0, "fail");
        sndp_delay_exec_start((uint32_t)100, (uint32_t)sndp_comm_spp_send_data_exec, 0, 0, 0);
    } else {
        COMM_SPP_TRACE(0, "sending...");
        comm_spp_ctx.is_sending = true;
        sndp_delay_exec_start((uint32_t)500, (uint32_t)sndp_comm_spp_send_data_timeout, 0, 0, 0);
    }
}

int32_t sndp_comm_spp_send_data(uint8_t *data, uint16_t data_len)
{
    if(!comm_spp_ctx.inited) {
        return -1;
    }
    
	if(!comm_spp_ctx.is_connected) {
		COMM_SPP_TRACE(0, "SPP not connected, return");
		return -2;
	}
	
    sndp_comm_spp_send_queue_push_data(data, data_len);
	sndp_comm_spp_send_data_exec();
	return 0;
}

static int32_t sndp_comm_spp_recv_data(const bt_bdaddr_t *remote, bt_spp_callback_param_t *param)
{
    if(param == NULL) {
        return -1;
    }
        
    COMM_SPP_TRACE(2, "recv data, pData:%p length=%d", param->rx_data_ptr, param->rx_data_len);
    DUMP8("0x%02x ", param->rx_data_ptr, (param->rx_data_len > 16) ? 16 : param->rx_data_len);
    sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_SPP, (uint8_t *)param->rx_data_ptr, param->rx_data_len);
    return 0;
}


bool sndp_comm_spp_is_connected(void)
{
    return comm_spp_ctx.is_connected;
}

static int sndp_comm_spp_server_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    struct BT_DEVICE_T *curr_device = NULL;
    ibrt_mobile_info_t *p_mobile_info = NULL;
    bt_bdaddr_t *mobile_addr = NULL;

    switch (event)
    {
    case BT_SPP_EVENT_OPENED:
        COMM_SPP_TRACE(0, "::BT_SPP_EVENT_OPENED");
        for (uint8_t i = 0; i < BT_DEVICE_NUM; ++i){
            curr_device = app_bt_get_device(i);
            mobile_addr = &curr_device->remote;
            p_mobile_info = (ibrt_mobile_info_t *)app_ibrt_conn_get_mobile_sm_by_addr(mobile_addr);

            if ((NULL != p_mobile_info) && (p_mobile_info->mobile_mode == IBRT_SNIFF_MODE)) {
                app_tws_ibrt_exit_sniff_with_mobile(mobile_addr);
                break;
            }
            app_ibrt_middleware_prevent_sniff_set((uint8_t*)mobile_addr, OTA_ONGOING);
        }

        comm_spp_ctx.is_connected = true;
        comm_spp_ctx.is_sending = false;
        comm_spp_ctx.pSppDevice = param->spp_chan;
        break;
        
    case BT_SPP_EVENT_CLOSED:
        COMM_SPP_TRACE(0, "::BT_SPP_EVENT_CLOSED");
        for (uint8_t i = 0; i < BT_DEVICE_NUM; ++i) {
            curr_device = app_bt_get_device(i);
            mobile_addr = &curr_device->remote;
            p_mobile_info = (ibrt_mobile_info_t *)app_ibrt_conn_get_mobile_sm_by_addr(mobile_addr);

            if ((NULL != p_mobile_info) && (p_mobile_info->mobile_mode == IBRT_SNIFF_MODE)) {
                app_tws_ibrt_exit_sniff_with_mobile(mobile_addr);
                break;
            }
            app_ibrt_middleware_prevent_sniff_clear((uint8_t*)mobile_addr, OTA_ONGOING);
        }

        comm_spp_ctx.is_connected = false;
        comm_spp_ctx.is_sending = false;
        break;
        
    case BT_SPP_EVENT_TX_DONE:
        COMM_SPP_TRACE(0, "::BT_SPP_EVENT_TX_DONE");
        sndp_delay_exec_start((uint32_t)100, (uint32_t)sndp_comm_spp_send_data_exec, 0, 0, 0);
        break;
    
    case BT_SPP_EVENT_RX_DATA:
        //COMM_SPP_TRACE(0, "::BT_SPP_EVENT_RX_DATA");
        sndp_comm_spp_recv_data(remote, param);
        break;
    default:
        break;
    }
    return 0;
}


int32_t sndp_comm_spp_init(void)
{
    comm_spp_send_queue_mutex_id = osMutexCreate(osMutex(comm_spp_send_queue_mutex));
    ASSERT(comm_spp_send_queue_mutex_id != NULL, "%s, comm_spp_send_queue_mutex_id == NULL", __func__);

    osMutexWait(comm_spp_send_queue_mutex_id, osWaitForever);
    InitCQueue(&comm_spp_send_queue, sizeof(comm_spp_send_queue_buf), comm_spp_send_queue_buf);
    osMutexRelease(comm_spp_send_queue_mutex_id);

    memset(&comm_spp_ctx, 0, sizeof(sndp_comm_spp_context_s));
    comm_spp_ctx.is_sending = false;
    comm_spp_ctx.is_sending = false;
    
    bt_spp_create_port(COMM_SPP_RFCOMM_CHANNEL_NUM, sndp_comm_spp_sdp_attributes, ARRAY_SIZE(sndp_comm_spp_sdp_attributes));
    bt_spp_set_callback(COMM_SPP_RFCOMM_CHANNEL_NUM, COMM_SPP_MAX_PACKET_SIZE*COMM_SPP_MAX_PACKET_NUM, sndp_comm_spp_server_callback, NULL);
    bt_spp_listen(COMM_SPP_RFCOMM_CHANNEL_NUM, false, NULL);
    comm_spp_ctx.pSppDevice = bt_spp_create_channel(BT_DEVICE_ID_1, COMM_SPP_RFCOMM_CHANNEL_NUM);
    comm_spp_ctx.inited = true;
	
	COMM_SPP_TRACE(0, "done.");
	return 0;
}

#endif	/* __SNDP_COMM_SPP__ */

