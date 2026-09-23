/***************************************************************************
 *
 * Copyright 2015-2021 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#ifdef BT_SPP_SUPPORT
#include <stdio.h>
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "app_audio.h"
#include "app_status_ind.h"
#include "bluetooth_bt_api.h"
#include "app_bt_stream.h"
#include "nvrecord_bt.h"
#include "nvrecord_env.h"
#include "cqueue.h"
#include "app_spp_tota.h"
#include "app_tota_cmd_code.h"
#include "app_tota.h"
#include "app_tota_cmd_handler.h"
#include "plat_types.h"
#include "spp_api.h"
#include "sdp_api.h"
#include "tota_stream_data_transfer.h"
#include "app_tota_common.h"
#include "heap_api.h"
#if defined(OTA_OVER_TOTA_ENABLED)
#include "ota_control.h"
#include "ota_basic.h"
#endif
#include "bta_bt_api.h"


#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
#include "sndp_comm_spp.h"
#endif


typedef struct{
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
    bool isConnected[BT_DEVICE_NUM];
    bt_spp_channel_t *pSppDevice[BT_DEVICE_NUM];
#else
    bool isConnected;
    bt_spp_channel_t *pSppDevice;
#endif
    const tota_callback_func_t *callBack;
}tota_spp_ctl_t;

static tota_spp_ctl_t tota_spp_ctl = {
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
    .isConnected    = {false},
    .pSppDevice     = {NULL},
#else
    .isConnected    = false,
    .pSppDevice     = NULL,
#endif
    .callBack       = NULL,
};

// static inline void _update_tx_buf(void);
// static inline uint8_t * _get_tx_buf_ptr(void);

#ifdef SPP_DEBUG_TOOL
extern bool app_spp_debug_cmd_check(uint8_t *cmd, uint16_t len);
extern uint8_t *app_spp_debug_cmd_process(uint8_t *cmd, uint16_t len, uint16_t *out_len);
#endif

/* is tota busy, use to handle sniff */
bool spp_tota_in_progress(void)
{
    return is_stream_data_running();
}

/****************************************************************************
 * TOTA SPP SDP Entries
 ****************************************************************************/

/*---------------------------------------------------------------------------
 *
 * ServiceClassIDList
 */
static const U8 TotaSppClassId[] = {
#ifdef IS_TOTA_RFCOMM_UUID_CUSTOMIZED
    SDP_ATTRIB_HEADER_8BIT(17),
    SDP_UUID_128BIT(tota_rfcomm_custom_uuid),
#else
    SDP_ATTRIB_HEADER_8BIT(3),        /* Data Element Sequence, 6 bytes */
    SDP_UUID_16BIT(SC_SERIAL_PORT),     /* Hands-Free UUID in Big Endian */
#endif
};

static const U8 TotaSppProtoDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(12),  /* Data element sequence, 12 bytes */

    /* Each element of the list is a Protocol descriptor which is a
     * data element sequence. The first element is L2CAP which only
     * has a UUID element.
     */
    SDP_ATTRIB_HEADER_8BIT(3),   /* Data element sequence for L2CAP, 3
                                  * bytes
                                  */

    SDP_UUID_16BIT(PROT_L2CAP),  /* Uuid16 L2CAP */

    /* Next protocol descriptor in the list is RFCOMM. It contains two
     * elements which are the UUID and the channel. Ultimately this
     * channel will need to filled in with value returned by RFCOMM.
     */

    /* Data element sequence for RFCOMM, 5 bytes */
    SDP_ATTRIB_HEADER_8BIT(5),

    SDP_UUID_16BIT(PROT_RFCOMM), /* Uuid16 RFCOMM */

    /* Uint8 RFCOMM channel number - value can vary */
    SDP_UINT_8BIT(RFCOMM_CHANNEL_TOTA)
};

/*
 * BluetoothProfileDescriptorList
 */
static const U8 TotaSppProfileDescList[] = {
#ifdef IS_TOTA_RFCOMM_UUID_CUSTOMIZED
    SDP_ATTRIB_HEADER_8BIT(22), /* Data element sequence, 22 bytes */

    /* Data element sequence for ProfileDescriptor, 20 bytes */
    SDP_ATTRIB_HEADER_8BIT(20),

    SDP_UUID_128BIT(tota_rfcomm_custom_uuid), /* Uuid16 SPP */
    SDP_UINT_16BIT(0x0102)          /* As per errata 2239 */
#else
    SDP_ATTRIB_HEADER_8BIT(8),        /* Data element sequence, 8 bytes */

    /* Data element sequence for ProfileDescriptor, 6 bytes */
    SDP_ATTRIB_HEADER_8BIT(6),

    SDP_UUID_16BIT(SC_SERIAL_PORT),   /* Uuid16 SPP */
    SDP_UINT_16BIT(0x0102)            /* As per errata 2239 */
#endif
};

/*
 * * OPTIONAL *  ServiceName
 */
static const U8 TotaSppServiceName1[] = {
    SDP_TEXT_8BIT(5),          /* Null terminated text string */
    'T', 'O', 'T', 'A', '\0'
};

// static const U8 TotaSppServiceName2[] = {
//     SDP_TEXT_8BIT(5),          /* Null terminated text string */
//     'S', 'p', 'p', '2', '\0'
// };

/* SPP attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * SPP SDP record.
 */
//static const SdpAttribute TotaSppSdpAttributes1[] = {
static bt_sdp_record_attr_t TotaSppSdpAttributes1[] = { // list attr id in ascending order

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, TotaSppClassId),

    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, TotaSppProtoDescList),

    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, TotaSppProfileDescList),

    /* SPP service name*/
    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), TotaSppServiceName1),
};

/*
static bt_sdp_record_attr_t TotaSppSdpAttributes2[] = { // list attr id in ascending order

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, TotaSppClassId),

    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, TotaSppProtoDescList),

    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, TotaSppProfileDescList),


    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), TotaSppServiceName2),
};
*/

static int tota_spp_handle_data_event_func(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    uint8_t *pData = (uint8_t *)param->rx_data_ptr;
    uint16_t dataLen = param->rx_data_len;

    TOTA_V2_TRACE(1,"spp tota v2 rx:%d", dataLen);
    //TOTA_V2_DUMP8("[0x%x]", pData, dataLen);

#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
    if (sndp_comm_spp_cmd_check(pData, dataLen)) {
        sndp_comm_spp_recv_data(pData, dataLen);
        return 0;
    }
#endif

#ifdef SPP_DEBUG_TOOL
    if (app_spp_debug_cmd_check(pData, dataLen)) {
        uint8_t *ret_buf = app_spp_debug_cmd_process(pData, dataLen, &dataLen);
        if (ret_buf != NULL) {
            bta_spp_send_data(param->spp_chan->rfcomm_handle, ret_buf, dataLen);
        }
        TOTA_V2_TRACE(0, "[%s] Bypass TOTA.", __func__);
        return 0;
    }
#endif
    tota_spp_ctl.callBack->rx_cb(pData, dataLen);
    return 0;
}


static int spp_tota_callback(const bt_bdaddr_t *remote, bt_spp_event_t event, bt_spp_callback_param_t *param)
{
    switch (event)
    {
    case BT_SPP_EVENT_OPENED: {
        TOTA_V2_TRACE(0, "spp_tota_callback v2 ::BTIF_SPP_EVENT_REMDEV_CONNECTED");
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
        if (param == NULL || param->device_id >= BT_DEVICE_NUM) {
            break;
        }
        bool wasConnected = false;
        for (uint8_t i = 0; i < BT_DEVICE_NUM; ++i) {
            wasConnected |= tota_spp_ctl.isConnected[i];
        }
        tota_spp_ctl.isConnected[param->device_id] = true;
        tota_spp_ctl.pSppDevice[param->device_id] = param->spp_chan;
#else
        tota_spp_ctl.isConnected = true;
        tota_spp_ctl.pSppDevice = param->spp_chan;
#endif
#if defined(OTA_OVER_TOTA_ENABLED)
        bes_ota_event_param_t otaParam;
        otaParam.pathType = DATA_PATH_SPP;
        memcpy(otaParam.param.address, (uint8_t*)&param->spp_chan->remote, sizeof(otaParam.param.address));
        otaParam.event = BES_OTA_CONN;
        app_ota_push_rx_data(SPP_RX_DATA_SELF_OTA_OVER_TOTA, &otaParam);
#endif
#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
        sndp_comm_spp_set_connect_sta(true);
#endif
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
        if (!wasConnected && tota_spp_ctl.callBack->connected_cb) {
#else
        if (tota_spp_ctl.callBack->connected_cb) {
#endif
            tota_spp_ctl.callBack->connected_cb();
        }
        break;
    }
    case BT_SPP_EVENT_CLOSED: {
        TOTA_V2_TRACE(0, "spp_tota_callback v2 ::BTIF_SPP_EVENT_REMDEV_DISCONNECTED");
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
        if (param == NULL || param->device_id >= BT_DEVICE_NUM) {
            break;
        }
#if defined(OTA_OVER_TOTA_ENABLED) && defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
        bt_spp_channel_t *closedChannel = tota_spp_ctl.pSppDevice[param->device_id];
#endif
        tota_spp_ctl.isConnected[param->device_id] = false;
        tota_spp_ctl.pSppDevice[param->device_id] = NULL;
        bool anyConnected = false;
        for (uint8_t i = 0; i < BT_DEVICE_NUM; ++i) {
            anyConnected |= tota_spp_ctl.isConnected[i];
        }
#else
        tota_spp_ctl.isConnected = false;
#endif
#if defined(OTA_OVER_TOTA_ENABLED)
        bes_ota_event_param_t otaParam;
        otaParam.pathType = DATA_PATH_SPP;
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
        if (closedChannel != NULL) {
            memcpy(otaParam.param.address, (uint8_t*)&closedChannel->remote, sizeof(otaParam.param.address));
        }
#endif
        otaParam.event = BES_OTA_DISCONN;
        app_ota_push_rx_data(SPP_RX_DATA_SELF_OTA_OVER_TOTA, &otaParam);
#endif
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
        sndp_comm_spp_set_connect_sta(anyConnected);
#endif
        if (!anyConnected && tota_spp_ctl.callBack->disconnected_cb) {
            tota_spp_ctl.callBack->disconnected_cb();
        }
#else
#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
        sndp_comm_spp_set_connect_sta(false);
#endif
        if (tota_spp_ctl.callBack->disconnected_cb) {
            tota_spp_ctl.callBack->disconnected_cb();
        }
#endif
        break;
    }
    case BT_SPP_EVENT_TX_DONE:
        TOTA_V2_TRACE(0, "spp_tota_callback v2 ::BTIF_SPP_EVENT_DATA_SENT");
        //osSemaphoreRelease(tota_spp_ctl.txSem);
        if (tota_spp_ctl.callBack->tx_done_cb)
        {
            tota_spp_ctl.callBack->tx_done_cb();
        }
        break;
    case BT_SPP_EVENT_RX_DATA:
        tota_spp_handle_data_event_func(remote, event, param);
        break;
    default:
        break;
    }
    return 0;
}

void app_spp_tota_init(const tota_callback_func_t *tota_callback_func)
{
    //tota_spp_ctl.txSem = osSemaphoreCreate(osSemaphore(tota_spp_tx_sem), 1);
    //tota_spp_ctl.txMutex = osMutexCreate(osMutex(tota_spp_tx_mutex));
    tota_spp_ctl.callBack = tota_callback_func;

    bta_spp_create_port(RFCOMM_CHANNEL_TOTA, TotaSppSdpAttributes1, ARRAY_SIZE(TotaSppSdpAttributes1));

    bta_spp_set_callback(RFCOMM_CHANNEL_TOTA, TOTA_RX_BUF_SIZE, spp_tota_callback, NULL);

#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
    bt_adapter_spp_listen(RFCOMM_CHANNEL_TOTA, true, NULL);

    for (uint8_t device_id = 0; device_id < BT_DEVICE_NUM; ++device_id) {
        tota_spp_ctl.pSppDevice[device_id] = bta_spp_create_channel(device_id, RFCOMM_CHANNEL_TOTA);
    }
#else
    bt_adapter_spp_listen(RFCOMM_CHANNEL_TOTA, false, NULL);
    tota_spp_ctl.pSppDevice = bta_spp_create_channel(BT_DEVICE_ID_1, RFCOMM_CHANNEL_TOTA);
#endif
}

/* this func is safe in thread */
bool app_spp_tota_send_data(uint8_t* ptrData, uint16_t length)
{
    bool sent = false;

    /**
     * app_spp_tota_send_data may be called in bt thread, as same the thread who
     * send TX_DONE. in this case, app_spp_tota_send_data will lock the bt thread
     * forever due to TX_DONE is no change to reported, becuase the bt thread is
     * already locked.
     *
     */

    TOTA_V2_TRACE(1, "spp tx:%d", length);
#if defined(__SNDP_TOTA_SPP_MULTI_DEVICE__)
    for (uint8_t i = 0; i < BT_DEVICE_NUM; ++i) {
        bt_spp_channel_t *channel = tota_spp_ctl.pSppDevice[i];
        if (!tota_spp_ctl.isConnected[i] || channel == NULL || channel->rfcomm_handle == 0) {
            continue;
        }
        sent |= bta_spp_send_data(channel->rfcomm_handle, ptrData, length);
    }
#else
    if (tota_spp_ctl.isConnected) {
        sent = bta_spp_send_data(tota_spp_ctl.pSppDevice->rfcomm_handle, ptrData, length);
    }
#endif
    return sent;
}

#if defined(__SNDP_COMM_SPP_USE_OTA_CHANNEL__)
extern "C" bool sndp_spp_tota_send_data(uint8_t* ptrData, uint16_t length)
{
    return app_spp_tota_send_data(ptrData, length);
}
#endif


// static inline void _update_tx_buf(void)
// {
//     tota_spp_ctl.txIndex = (tota_spp_ctl.txIndex + 1) % MAX_SPP_PACKET_NUM;
// }

// static inline uint8_t * _get_tx_buf_ptr(void)
// {
//     return (tota_spp_ctl.txBuff + tota_spp_ctl.txIndex*MAX_SPP_PACKET_SIZE);
// }

#endif /* BT_SPP_SUPPORT */
