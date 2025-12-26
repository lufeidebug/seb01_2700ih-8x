#include <stdio.h>
#include <string.h>
#include "cmsis_os.h"
#include "hal_trace.h"
#include "cqueue.h"
#include "cmsis_os.h"
#include "cqueue.h"
#include "bluetooth_bt_api.h"
#include "umm_malloc.h"
#include "app_ai_tws.h"

#include "ai_spp.h"
#include "ai_transport.h"
#include "ai_thread.h"
#include "ai_control.h"
#include "ai_manager.h"
#include "app_bt.h"
#include "bts_bt_if.h"
#ifdef IBRT
#include "app_tws_ibrt.h"
#endif
#ifdef BIXBY_USE_BESTOOL
#include "app_bixbycontrol_handle.h"
#endif

#define SPP_RECV_BUFFER_SIZE    (L2CAP_CFG_MTU*4)
#define SPP_RCV_BUFFER_SIZE_EXT (L2CAP_CFG_MTU)

typedef struct
{
    uint8_t ai_index;
    bt_spp_channel_t *spp_dev;
} AI_SPP_DEVICE_T;

static AI_SPP_DEVICE_T ai_spp_dev[MUTLI_AI_NUM][AI_CONNECT_COUNT];

#ifdef BIXBY_USE_BESTOOL
static uint8_t service_id_ext = 0;
bt_spp_channel_t *ai_spp_dev_ext[AI_CONNECT_NUM_MAX] = {NULL};
extern void BIXBY_CONTROL_SPP_Register128UuidSdpServices(uint8_t *service_id, bt_sdp_record_param_t *param);
#endif

void app_ai_spp_deinit_tx_buf(void)
{

}

uint8_t ai_app_find_spec_from_service_id(uint16_t service_id)
{
    AI_VOICE_TRACE(2, "%s, service_id %d", __func__, service_id);
    uint8_t ai_spec = 0;

    switch(service_id)
    {
        case RFCOMM_CHANNEL_AMA:
           ai_spec = AI_SPEC_AMA;
           break;
        case RFCOMM_CHANNEL_BES:
           ai_spec = AI_SPEC_BES;
           break;
        case RFCOMM_CHANNEL_BAIDU:
           ai_spec = AI_SPEC_BAIDU;
           break;
        case RFCOMM_CHANNEL_TENCENT:
           ai_spec = AI_SPEC_TENCENT;
           break;
        case RFCOMM_CHANNEL_ALI:
           ai_spec = AI_SPEC_ALI;
           break;
        case RFCOMM_CHANNEL_COMMON:
           ai_spec = AI_SPEC_COMMON;
           break;
        case RFCOMM_CHANNEL_COMMON_RECORD:
            ai_spec = AI_SPEC_RECORDING;
            break;
        case RFCOMM_CHANNEL_BIXBY_CONTROL:
            ai_spec = AI_SPEC_BIXBY;
            break;
        case RFCOMM_CHANNEL_BIXBY_VOICE:
            ai_spec = AI_SPEC_BIXBY;
            break;
        case RFCOMM_CHANNEL_MMA:
           ai_spec = AI_SPEC_MMA;
           break;
        default:
//            ASSERT(false, "Invalid service id %d.", service_id);
            AI_VOICE_TRACE(0,"Invalid service id");
            break;
    }

    return ai_spec;
}

#ifdef BIXBY_USE_BESTOOL
static int ai_spp_ext_data_event_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    bt_spp_channel_t *spp_chan = param->spp_chan;
    uint8_t device_id = spp_chan->device_id;
    uint8_t *pData = (uint8_t *)param->rx_data_ptr;
    uint16_t dataLen = param->rx_data_len;
    AI_VOICE_TRACE(1, "[ai_spp_ext_data_event_callback] d(%d) recv data:", device_id);
    AI_VOICE_DUMP8("%02x ", pData, (dataLen>16)?16:dataLen);
    app_bixby_control_receive_data_handle(pData, dataLen, device_id);
    return 0;
}

static int ai_spp_ext_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    AI_VOICE_TRACE(0, "[::%s] bixby event:%02x", __func__, event);
    bt_spp_channel_t *spp_chan = param->spp_chan;
    if (BT_SPP_EVENT_OPENED == event)
    {
        AI_VOICE_DUMP8("%02x ", spp_chan->remote.address, BT_ADDR_OUTPUT_PRINT_NUM);
        app_bixby_control_connected((void *)&spp_chan->remote, AI_TRANSPORT_SPP, spp_chan->device_id);
    }
    else if (BT_SPP_EVENT_CLOSED == event)
    {
        AI_VOICE_DUMP8("%02x ", spp_chan->remote.address, BT_ADDR_OUTPUT_PRINT_NUM);
        app_bixby_control_disconnected((void *)&spp_chan->remote, AI_TRANSPORT_SPP, spp_chan->device_id);
    }
    else if (BT_SPP_EVENT_TX_DONE == event)
    {
        app_bixby_control_send_done_handle(spp_chan->device_id);
    }
    else if (BT_SPP_EVENT_RX_DATA == event)
    {
        ai_spp_ext_data_event_callback(remote, event, param);
    }
    else
    {
        AI_VOICE_TRACE(1,"::unknown event %d\n", event);
    }
    return 0;
}

bt_spp_channel_t *bixby_control_spp_find_device_from_id(uint8_t device_id)
{
    bt_spp_channel_t *_ai_spp_dev_ext = NULL;
    for (uint8_t i = 0; i < AI_CONNECT_NUM_MAX; i += 1)
    {
        _ai_spp_dev_ext = ai_spp_dev_ext[i];
        if (_ai_spp_dev_ext && bta_spp_is_connected(_ai_spp_dev_ext->rfcomm_handle))
        {
            return _ai_spp_dev_ext;
        }
    }
    return NULL;
}
#endif

int ai_spp_data_event_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    bt_spp_channel_t *spp_chan = param->spp_chan;
    uint8_t device_id = param->device_id;
    uint8_t *pData = (uint8_t *)param->rx_data_ptr;
    uint16_t dataLen = param->rx_data_len;

    uint8_t service_id = spp_chan->port->local_server_channel;
    uint8_t ai_index = ai_app_find_spec_from_service_id(service_id);
    //AI_VOICE_TRACE(2,"%s ai_index %d", __func__, ai_index);

    ai_function_handle(CALLBACK_CMD_RECEIVE, pData, dataLen, ai_index, device_id);

    return 0;
}

void app_ai_spp_set_spp_info(uint8_t ai_index, bt_spp_channel_t *spp_chan)
{
    for (uint8_t i = 0; i < AI_CONNECT_NUM_MAX; i++)
    {
        if(ai_spp_dev[ai_index][i].spp_dev == NULL)
        {
            ai_spp_dev[ai_index][i].ai_index = ai_index;
            ai_spp_dev[ai_index][i].spp_dev = spp_chan;
            break;
        }
    }
}

void app_ai_spp_clear_spp_info(uint8_t ai_index, bt_spp_channel_t *spp_chan)
{
    for (uint8_t i = 0; i < AI_CONNECT_NUM_MAX; i++)
    {
        if(ai_spp_dev[ai_index][i].spp_dev != NULL &&
           ai_spp_dev[ai_index][i].spp_dev == spp_chan)
        {
            ai_spp_dev[ai_index][i].ai_index = ai_index;
            ai_spp_dev[ai_index][i].spp_dev = NULL;
            break;
        }
    }
}

bt_spp_channel_t *ai_spp_find_device_from_id(uint8_t ai_index, uint8_t device_id)
{
    ASSERT(ai_index<MUTLI_AI_NUM, "%s ai_index %d", __func__, ai_index);

    for (uint8_t i = 0; i < AI_CONNECT_NUM_MAX; i++)
    {
        if (app_ai_spp_is_connected(ai_index, i))
        {
            return ai_spp_dev[ai_index][i].spp_dev;
        }
    }

    return NULL;
}

uint8_t app_ai_spp_get_device_connected_index(uint8_t ai_index, bt_bdaddr_t* bdaddr)
{
    for (uint8_t i = 0 ; i < AI_CONNECT_NUM_MAX; i += 1)
    {
        if(ai_spp_dev[ai_index][i].spp_dev != NULL)
        {
            if(memcmp(&ai_spp_dev[ai_index][i].spp_dev->remote, bdaddr, sizeof(bt_bdaddr_t)) == 0)
            {
                return i;
            }
        }
    }
    return AI_CONNECT_NUM_MAX;
}

static int app_spp_ai_accept_channel_request(const bt_bdaddr_t *remote, uint16_t local_server_channel)
{
    AI_TRANS_TYPE_E type = AI_TRANSPORT_IDLE;
    bool complete = false;
    uint8_t ai_index = ai_app_find_spec_from_service_id(local_server_channel);

    AI_VOICE_TRACE(3,"%s chnnl = %d index = %d",__func__, local_server_channel, ai_index);

    if(ai_index == 0){
        return true;
    }

    complete = app_ai_is_setup_complete(ai_index);
    if(complete == true){
        return false;
    }
    AI_VOICE_TRACE(1,"setup_complete ?= %d",complete);
    for (uint8_t ai_connect_index = 0; ai_connect_index < AI_CONNECT_NUM_MAX; ai_connect_index++){
        type = app_ai_get_transport_type(ai_index, ai_connect_index);
        AI_VOICE_TRACE(2,"index = %d type = %d",ai_connect_index,type);
        if(type == AI_TRANSPORT_BLE){
            return false;
        }
    }
    return true;
}

void ai_spp_enumerate_disconnect_service(uint8_t *address,uint8_t deviceID)
{
    for(uint8_t ai_index = AI_SPEC_AMA; ai_index < AI_SPEC_COUNT; ai_index++){
        uint8_t connect_index = app_ai_get_connect_index_from_addr(address, ai_index);
        if(connect_index != AI_CONNECT_NUM_MAX){
            if (app_ai_get_transport_type(ai_index, connect_index) == AI_TRANSPORT_SPP){
                connect_index = app_ai_spp_get_device_connected_index(ai_index, (bt_bdaddr_t*)address);
                if(connect_index != AI_CONNECT_NUM_MAX)
                    app_ai_spp_disconnlink(ai_index, connect_index);
            }
        }
    }
}

int ai_spp_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    bt_spp_channel_t *spp_chan = param->spp_chan;
    bt_spp_port_t *spp_port = spp_chan->port;
    uint8_t device_id = param->device_id;
    uint8_t ai_index = ai_app_find_spec_from_service_id(spp_port->local_server_channel);
    if (BT_SPP_EVENT_TX_DONE != event && BT_SPP_EVENT_RX_DATA != event)
    {
        AI_VOICE_TRACE(3, "[::%s] event:%02x ai_spec:%d mobileAddr:", __func__, event, ai_index);
        AI_VOICE_DUMP8("%02x ", spp_chan->remote.address, BT_ADDR_OUTPUT_PRINT_NUM);
    }
    if (BT_SPP_EVENT_OPENED == event)
    {
        if(app_ai_get_connected_num() >= AI_CONNECT_NUM_MAX && \
           (false == app_ai_check_device_is_connected(spp_chan->remote.address)))
        {
            AI_VOICE_TRACE(1,"%s, Already connected max num ai !!!", __func__);
        }
        else
        {
            app_ai_spp_set_spp_info(ai_index, spp_chan);
            ai_function_handle(CALLBACK_AI_CONNECT, (void *)spp_chan->remote.address, AI_TRANSPORT_SPP, ai_index, device_id);
        }
#ifdef IBRT
        if ((app_bt_is_a2dp_connected(device_id) == false) && (app_bt_is_hfp_connected(device_id) == false))
        {
            if(bts_bt_if_is_dev_link_connected((bt_bdaddr_t *)&spp_chan->remote))
            {
                AI_VOICE_TRACE(1, "manual reconnect mobile profiles");
                app_bt_ibrt_reconnect_mobile_profile((bt_bdaddr_t *)&spp_chan->remote);
            }
        }
#endif
    }
    else if (BT_SPP_EVENT_CLOSED == event)
    {
        uint8_t connect_index = app_ai_get_connect_index_from_addr(spp_chan->remote.address, ai_index);
        app_ai_spp_clear_spp_info(ai_index, spp_chan);

        if(AI_CONNECT_NUM_MAX == connect_index)
        {
            AI_VOICE_TRACE(2, "%s, disconnect by self, because connected num is %d!!!", __func__, connect_index);
        }
#ifndef SS_BIXBY_INTEGRATED
        if (app_ai_get_transport_type(ai_index, connect_index) == AI_TRANSPORT_SPP)
#endif
        {
            app_ai_update_connect_state(AI_IS_DISCONNECTING, connect_index);
            ai_function_handle(CALLBACK_AI_DISCONNECT, (void *)spp_chan->remote.address, AI_TRANSPORT_SPP, ai_index, device_id);
        }
        // 20210324:clear ai information have moved to app_ai_disconnect_handle(), push CALLBACK_AI_DISCONNECT to dedicated AI 
        // module to clear connect information.
        //app_ai_clear_connect_info(connect_index);
    }
    else if (BT_SPP_EVENT_TX_DONE == event)
    {
        ai_function_handle(CALLBACK_DATA_SEND_DONE, NULL, 0, ai_index, device_id);
    }
    else if (BT_SPP_EVENT_RX_DATA == event)
    {
        ai_spp_data_event_callback(remote, event, param);
    }
    else
    {
        AI_VOICE_TRACE(1,"::unknown event %d\n", event);
    }
    return 0;
}

bool app_ai_spp_send(uint8_t* ptrData, uint32_t length, uint8_t ai_index, uint8_t device_id)
{
    bool ret = FALSE;
    POSSIBLY_UNUSED uint16_t valid_len = (length&0xFFFF);
    //AI_VOICE_TRACE(2,"%s length %d", __func__, length);

    if (app_ai_is_in_tws_mode(0))
    {
        if (app_ai_tws_link_connected() && (app_ai_tws_get_local_role() != APP_AI_TWS_MASTER))
        {
            AI_VOICE_TRACE(2,"%s role %d isn't MASTER", __func__, app_ai_tws_get_local_role());
            return FALSE;
        }
    }

    bt_spp_channel_t *_ai_spp_dev = ai_spp_find_device_from_id(ai_index, device_id);
    if(_ai_spp_dev)
    {
        ret = (bta_spp_write(_ai_spp_dev->rfcomm_handle, ptrData, valid_len)==BT_STS_FAILED)?FALSE:TRUE;
    }

    return ret;
}

AI_SPP__REGISTER_INSTANCE_T* ai_spp_register_get_entry_pointer_from_ai_code(uint32_t ai_code)
{
    for (uint32_t index = 0;
         index < ((uint32_t)__ai_spp_register_table_end-(uint32_t)__ai_spp_register_table_start)/sizeof(AI_SPP__REGISTER_INSTANCE_T); index++)
    {
        if (AI_SPP_PTR_FROM_ENTRY_INDEX(index)->ai_code == ai_code)
        {
            //AI_VOICE_TRACE(3,"%s ai_code %d index %d", __func__, ai_code, index);
            return AI_SPP_PTR_FROM_ENTRY_INDEX(index);
        }
    }

    ASSERT(0, "%s fail ai_code %d", __func__, ai_code);
    return NULL;
}

uint32_t ai_spp_register_get_entry_index_from_ai_code(uint32_t ai_code)
{
    for (uint32_t index = 0;
         index < ((uint32_t)__ai_spp_register_table_end-(uint32_t)__ai_spp_register_table_start)/sizeof(AI_SPP__REGISTER_INSTANCE_T); index++)
    {
        if (AI_SPP_PTR_FROM_ENTRY_INDEX(index)->ai_code == ai_code)
        {
            AI_VOICE_TRACE(3,"%s ai_code %d index %d", __func__, ai_code, index);
            return index;
        }
    }

    ASSERT(0, "%s fail ai_code %d", __func__, ai_code);
    return INVALID_AI_SPP_ENTRY_INDEX;
}

static void ai_spp_register_uuid_sdp_services(uint8_t *service_id, bt_sdp_record_param_t *param, uint8_t ai_index)
{
    AI_SPP__REGISTER_INSTANCE_T *pInstance = NULL;

    AI_VOICE_TRACE(1,"%s", __func__);
    pInstance = ai_spp_register_get_entry_pointer_from_ai_code(ai_index);
    if(pInstance)
        pInstance->ai_handler(service_id, param);
}

void app_ai_spp_init(int portType, uint8_t ai_index)
{
    AI_VOICE_TRACE(1,"%s",__func__);
    bt_sdp_record_param_t param;
    uint8_t service_id = 0;

#ifdef BIXBY_USE_BESTOOL
    if (AI_SPEC_BIXBY == ai_index)
    {
        BIXBY_CONTROL_SPP_Register128UuidSdpServices(&service_id, &param_ext);

        AI_VOICE_TRACE(2,"%s %d ",__func__, service_id);

        bta_spp_create_port(service_id, param.attrs, param.attr_count);

        bta_spp_set_callback(service_id, SPP_RCV_BUFFER_SIZE_EXT, ai_spp_ext_callback, NULL);

        if (portType == BT_SPP_SERVER_PORT)
        {
            bt_adapter_spp_listen(service_id, true, app_spp_ai_accept_channel_request);
        }

        for (uint8_t j = 0 ; j < AI_CONNECT_NUM_MAX; j += 1)
        {
            ai_spp_dev_ext[j] = bta_spp_create_channel(j, service_id);
        }
    }
    else
#endif
    {
        ai_spp_register_uuid_sdp_services(&service_id, &param, ai_index);

        AI_VOICE_TRACE(2,"%s %d ",__func__, service_id);

        bta_spp_create_port(service_id, param.attrs, param.attr_count);

        bta_spp_set_callback(service_id, SPP_RECV_BUFFER_SIZE, ai_spp_callback, NULL);

        if (portType == BT_SPP_SERVER_PORT)
        {
            bt_adapter_spp_listen(service_id, true, app_spp_ai_accept_channel_request);
        }

        for (uint8_t i = 0 ; i < AI_CONNECT_NUM_MAX; i += 1)
        {
            ai_spp_dev[ai_index][i].spp_dev = NULL;
            ai_spp_dev[ai_index][i].ai_index = ai_index;
        }
    }
}


void app_ai_spp_client_init(uint8_t ai_index)
{
    AI_VOICE_TRACE(2,"%s AI index:%d",__func__, ai_index);

#if BTIF_SPP_CLIENT == BTIF_ENABLED
    app_ai_spp_init(BT_SPP_CLIENT_PORT, ai_index);
#else
    AI_VOICE_TRACE(0,"!!!SPP_CLIENT is no support ");
#endif
}


void app_ai_spp_server_init(uint8_t ai_index)
{
    AI_VOICE_TRACE(2,"%s AI index:%d", __func__, ai_index);

#if BTIF_SPP_SERVER == BTIF_ENABLED
    app_ai_spp_init(BT_SPP_SERVER_PORT, ai_index);
#else
    AI_VOICE_TRACE(0,"!!!SPP_SERVER is no support ");
#endif
}

bool app_ai_spp_is_connected(uint8_t ai_index, uint8_t connected_index)
{
    bool connected = false;
    bt_spp_channel_t *sppDev = ai_spp_dev[ai_index][connected_index].spp_dev;
    if (sppDev)
    {
        connected = bta_spp_is_connected(sppDev->rfcomm_handle);
    }
    return connected;
}

void app_ai_spp_disconnlink(uint8_t ai_index, uint8_t connected_index)//only used for device force disconnect
{
    AI_VOICE_TRACE(2,"%s ai_index:%d, connection_index:%d", __func__, ai_index, connected_index);
    bt_spp_channel_t *_ai_spp_dev = ai_spp_dev[ai_index][connected_index].spp_dev;
    if(_ai_spp_dev)
    {
        bta_spp_disconnect(_ai_spp_dev->rfcomm_handle, BTIF_BEC_LOCAL_TERMINATED);
    }
}

void app_ai_link_free_after_spp_dis(uint8_t ai_index, uint8_t connected_index)
{
    AI_VOICE_TRACE(1,"%s", __func__);
}

void app_ai_all_spp_profile_disconnect(uint8_t device_id)
{
    uint8_t ai_connect_index = AI_CONNECT_1;
    for (uint8_t ai_index = 0; ai_index < MUTLI_AI_NUM; ai_index++)
    {
        ai_connect_index = app_ai_get_connect_index_from_device_id(device_id, ai_index);
        app_ai_spp_disconnlink(ai_index, ai_connect_index);
    }
#ifdef BIXBY_USE_BESTOOL
    bt_spp_channel_t *_ai_spp_dev_ext = bixby_control_spp_find_device_from_id(device_id);
    if (_ai_spp_dev_ext)
    {
        bta_spp_disconnect(_ai_spp_dev_ext->rfcomm_handle, BTIF_BEC_LOCAL_TERMINATED);
    }
#endif
}

