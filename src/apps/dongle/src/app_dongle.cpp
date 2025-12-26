/***************************************************************************
 *
 * Copyright 2015-2024 BES.
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
 * @brief dongle application.
 *
 ****************************************************************************/

/****************************** header include ********************************/
#include <stdint.h>

#include "bt_service.h"
#include "app_dongle.h"
#include "bt_dongle.h"
#include "nvrecord_extension.h"
#include "factory_section.h"

/***************************** external declaration *****************************/
extern void app_dongle_test_init(void);
extern void app_dongle_test_deinit(void);

/***************************** macro defination *******************************/
#define APP_BT_DONGLE_INPUT_TYPE        BT_SVC_AUD_PATH_USB
#define APP_BT_DONGLE_OUTPUT_TYPE       BT_SVC_AUD_PATH_LEA_UC
#define APP_BT_DONGLE_DEV_NAME          "BES_DONGLE"

#define APP_BT_DONGLE_A2DP_SCO_CONN_MAX 1
#define APP_BT_DONGLE_A2DP_AUTO_SWITCH   true

#define APP_BT_DONGLE_BC_SYNC_VOLUM      true
#define APP_BT_DONGLE_BC_COMPANY_ID      0x02B0
#define APP_BT_DONGLE_BC_DEV_ID          0x030201
#define APP_BT_DONGLE_BC_LINK_ENC        false

#define APP_BT_DONGLE_USB_TX_RX_INTE     false

#define APP_BT_DONGLE_UC_CIG_NUM        1
#if defined (BLE_AUDIO_STEREO_CHAN_OVER_CIS_CNT)
#define APP_BT_DONGLE_UC_CONN_MAX       1
#define APP_BT_DONGLE_UC_CIS_NUM        1
#else
#define APP_BT_DONGLE_UC_CONN_MAX       BLE_CONNECTION_MAX
#define APP_BT_DONGLE_UC_CIS_NUM        2
#endif
#ifdef GSBC_SUPPORT
#define APP_BT_DONGLE_UC_CODE_TYPE       0x08
#define APP_BT_DONGLE_UC_SDU_INTERVAL_US 10000
#define APP_BT_DONGLE_UC_SDU_FRAME_OCTET 119
#else
#define APP_BT_DONGLE_UC_CODE_TYPE       0x06
#ifdef AOB_LOW_LATENCY_MODE
#define APP_BT_DONGLE_UC_SDU_INTERVAL_US 5000
#define APP_BT_DONGLE_UC_SDU_FRAME_OCTET 60
#elif BLE_AUDIO_FRAME_DUR_7_5MS
#define APP_BT_DONGLE_UC_SDU_INTERVAL_US 7500
#define APP_BT_DONGLE_UC_SDU_FRAME_OCTET 90
#else
#define APP_BT_DONGLE_UC_SDU_INTERVAL_US 10000
#define APP_BT_DONGLE_UC_SDU_FRAME_OCTET 120
#endif
#endif

#if (APP_BT_DONGLE_UC_SDU_INTERVAL_US == 500) && (APP_BT_DONGLE_UC_CIS_NUM == 2)
#define APP_BT_DONGLE_UC_NSE             2
#else
#define APP_BT_DONGLE_UC_NSE             3
#endif
#define APP_BT_DONGLE_UC_PHY             2
#define APP_BT_DONGLE_UC_BN              1
#define APP_BT_DONGLE_UC_FT              1

/*****************************  type defination ********************************/
typedef struct
{
    bool scan_enable;
} app_dongle_env_t;


/*****************************  variable defination *****************************/
static app_dongle_env_t app_dongle_env = {0};

/*****************************  function declaration ****************************/
static void app_dongle_prepare_path_param(uint8_t type, bt_dongle_path_param_u* param)
{
    switch (type)
    {
        case BT_SVC_AUD_PATH_LINEIN:
        case BT_SVC_AUD_PATH_I2S:
        {
        } break;
        case BT_SVC_AUD_PATH_USB:
        {
            param->usb.tx_rx_integrated = APP_BT_DONGLE_USB_TX_RX_INTE;
        } break;
        case BT_SVC_AUD_PATH_A2DP:
        {
            param->a2dp.auto_switch = APP_BT_DONGLE_A2DP_AUTO_SWITCH;
        } break;
        case BT_SVC_AUD_PATH_SCO:
        {
        } break;
        case BT_SVC_AUD_PATH_LEA_UC:
        {
            param->lea_uc.codec_id[0]                           = APP_BT_DONGLE_UC_CODE_TYPE;
            param->lea_uc.cig_count                             = APP_BT_DONGLE_UC_CIG_NUM;
            param->lea_uc.cig_param[0].sdu_interval_us          = APP_BT_DONGLE_UC_SDU_INTERVAL_US;
            param->lea_uc.cig_param[0].iso_interval_us          = APP_BT_DONGLE_UC_SDU_INTERVAL_US;
            param->lea_uc.cig_param[0].ft                       = APP_BT_DONGLE_UC_FT;
            param->lea_uc.cig_param[0].cis_count                = APP_BT_DONGLE_UC_CIS_NUM;
            param->lea_uc.cig_param[0].cis_param[0].nse         = APP_BT_DONGLE_UC_NSE;
            param->lea_uc.cig_param[0].cis_param[0].phy         = APP_BT_DONGLE_UC_PHY;
            param->lea_uc.cig_param[0].cis_param[0].bn          = APP_BT_DONGLE_UC_BN;
            param->lea_uc.cig_param[0].cis_param[0].pdu_max_tx  = APP_BT_DONGLE_UC_SDU_FRAME_OCTET;
            param->lea_uc.cig_param[0].cis_param[0].pdu_max_rx  = APP_BT_DONGLE_UC_SDU_FRAME_OCTET;
            if (param->lea_uc.cig_param[0].cis_count == 1)
            {
                param->lea_uc.cig_param[0].cis_param[0].pdu_max_tx *= 2;
                param->lea_uc.cig_param[0].cis_param[0].pdu_max_rx *= 2;
            }
            else if (param->lea_uc.cig_param[0].cis_count == 2)
            {
                memcpy(&param->lea_uc.cig_param[0].cis_param[1],
                    &param->lea_uc.cig_param[0].cis_param[0],
                    sizeof(param->lea_uc.cig_param[0].cis_param[1]));
            }

        } break;
        case BT_SVC_AUD_PATH_LEA_BC:
        {
            param->lea_bc.company_id  = APP_BT_DONGLE_BC_COMPANY_ID;
            param->lea_bc.dev_id      = APP_BT_DONGLE_BC_DEV_ID;
            param->lea_bc.sync_volume = APP_BT_DONGLE_BC_SYNC_VOLUM;
            param->lea_bc.enc         = APP_BT_DONGLE_BC_LINK_ENC;
        } break;
        default:
            break;
    }
}

static void app_dongle_pairing_check(void)
{
    uint8_t conn_dev_max = 0;
    const bt_dongle_info_t* bt_dongle_info = NULL;

    TRACE(0, "%s, %d", __func__, app_dongle_env.scan_enable);
    if (!app_dongle_env.scan_enable)
    {
        return;
    }

    bt_dongle_get_info(&bt_dongle_info);
    if (!bt_dongle_info)
    {
        return;
    }

    if ((bt_dongle_info->open_param.output_type == BT_SVC_AUD_PATH_A2DP) ||
        (bt_dongle_info->open_param.output_type == BT_SVC_AUD_PATH_SCO))
    {
        conn_dev_max = APP_BT_DONGLE_A2DP_SCO_CONN_MAX;
    }
    else if (bt_dongle_info->open_param.output_type == BT_SVC_AUD_PATH_LEA_UC)
    {
        conn_dev_max = APP_BT_DONGLE_UC_CONN_MAX;
    }

    if (bt_dongle_info->conn_dev_count < conn_dev_max)
    {
        bt_dongle_scan_start();
    }
}

void app_dongle_event_cb(bt_dongle_event_t* event)
{
    uint8_t* local_name = NULL;
    const bt_dongle_info_t* bt_dongle_info = NULL;

    switch (event->event)
    {
        case BT_DONGLE_EVENT_OPEN:
        {
            TRACE(0, "[%s][%d]:dongle_open: %d", __func__, __LINE__,
                event->param.open.status);
        }
        break;
        case BT_DONGLE_EVENT_CLOSE:
        {
            TRACE(0, "[%s][%d]:dongle_close: %d", __func__, __LINE__,
                event->param.close.status);
        }
        break;
        case BT_DONGLE_EVENT_START_SCAN:
        {
            TRACE(0, "[%s][%d]:dongle_scan_start: %d", __func__, __LINE__,
                event->param.start_scan.status);
        }
        break;
        case BT_DONGLE_EVENT_STOP_SCAN:
        {
            TRACE(0, "[%s][%d]:dongle_scan_stop: %d", __func__, __LINE__,
                event->param.start_scan.status);
        }
        break;
        case BT_DONGLE_EVENT_SCAN_REPORT:
        {
            TRACE(0, "[%s][%d]:dongle_scan_report: name=%s, rssi=%d", __func__, __LINE__,
                event->param.scan_report.name? event->param.scan_report.name: "dev no name",
                event->param.scan_report.rssi);

            bt_dongle_get_info(&bt_dongle_info);
            if (bt_dongle_info->open_param.output_type == BT_SVC_AUD_PATH_LEA_UC)
            {
                local_name = factory_section_get_ble_name();
            }
            else
            {
                local_name = factory_section_get_bt_name();
            }

            if ((!local_name) || (!event->param.scan_report.name))
            {
                TRACE(0, "[%s][%d]:%p, %p", __func__, __LINE__,
                    local_name, event->param.scan_report.name);
                break;
            }
            else
            {
                TRACE(0, "[%s][%d]:%s, %s", __func__, __LINE__,
                    local_name, event->param.scan_report.name);
            }

            if(!strcmp((char *)local_name, event->param.scan_report.name))
            {
                bt_dongle_scan_stop();
                app_dongle_dev_conn(event->param.scan_report.addr);
            }
        }
        break;
        case BT_DONGLE_EVENT_DEV_CONN:
        {
            bt_dongle_stream_start_param_t stream_param = {0};
            TRACE(0, "[%s][%d]:dongle_conn: %d", __func__, __LINE__,
                event->param.dev_conn.status);
            DUMP8("%02x ", event->param.dev_conn.addr, 6);
            app_dongle_pairing_check();

            //Start stream
            memcpy(stream_param.addr, event->param.dev_conn.addr, 6);
            stream_param.source = 1;
            stream_param.sink   = 1;
            stream_param.cig_id = 0;
            bt_dongle_stream_start(&stream_param);
        }
        break;
        case BT_DONGLE_EVENT_DEV_DISCONN:
        {
            TRACE(0, "[%s][%d]:dongle_disconn: %d", __func__, __LINE__,
                event->param.dev_disconn.status);
            DUMP8("%02x ", event->param.dev_disconn.addr, 6);
            app_dongle_pairing_check();
        }
        break;
        case BT_DONGLE_EVENT_STREAM_START:
        {
            TRACE(0, "[%s][%d]:dongle_stream_start", __func__, __LINE__);
        }
        break;
        case BT_DONGLE_EVENT_STREAM_STOP:
        {
            TRACE(0, "[%s][%d]:dongle_stream_stop", __func__, __LINE__);
        }
        break;
        default:
        break;
    }
}

void app_dongle_dev_disconn(uint8_t* addr)
{
    const bt_dongle_info_t* bt_dongle_info = NULL;

    if (addr)
    {
        bt_dongle_disconn_dev(addr);
    }
    else
    {
        bt_dongle_get_info(&bt_dongle_info);
        if (!bt_dongle_info)
        {
            return;
        }

        for (int idx=0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (!BT_DONGLE_ADDR_EMPTY(bt_dongle_info->conn_dev[idx]))
            {
                bt_dongle_disconn_dev((uint8_t*)&bt_dongle_info->conn_dev[idx][0]);
            }
        }
    }
}

void app_dongle_dev_conn(uint8_t* addr)
{
    bt_dongle_conn_dev_param_t conn_param = {0};

    memcpy(conn_param.addr, addr, sizeof(conn_param.addr));
    bt_dongle_conn_dev(&conn_param);
}

void app_dongle_stop_scan(void)
{
    bt_dongle_scan_stop();
    app_dongle_env.scan_enable = false;
}

void app_dongle_start_scan(void)
{
    app_dongle_env.scan_enable = true;
    bt_dongle_scan_start();
}

void app_dongle_close(void)
{
    int ret;

    ret = bt_dongle_close();
    if (ret == BT_SVC_ERROR_NO)
    {
        memset(&app_dongle_env, 0, sizeof(app_dongle_env));
    }
}

void app_dongle_open(uint8_t input_type, uint8_t output_type)
{
    int ret;
    bt_dongle_open_t open_param = {0};

    open_param.input_type  = input_type;
    open_param.output_type = output_type;
    open_param.event_cb    = app_dongle_event_cb;

    app_dongle_prepare_path_param(open_param.input_type, &open_param.input_param);
    app_dongle_prepare_path_param(open_param.output_type, &open_param.output_param);

    ret = bt_dongle_open(&open_param);
    if(ret != BT_SVC_ERROR_NO)
    {
        TRACE(0, "[%s][%d]:open fail!", __func__, __LINE__);
    }
}

void app_dongle_init(uint8_t mode)
{
    switch (mode)
    {
        case NV_APP_DONGLE_A2DP:
        {
            app_dongle_open(APP_BT_DONGLE_INPUT_TYPE, BT_SVC_AUD_PATH_A2DP);
            app_dongle_start_scan();
        } break;
        case NV_APP_DONGLE_SCO:
        {
            app_dongle_open(APP_BT_DONGLE_INPUT_TYPE, BT_SVC_AUD_PATH_SCO);
            app_dongle_start_scan();
        } break;
        case NV_APP_DONGLE_LEA_UC:
        {
            app_dongle_open(APP_BT_DONGLE_INPUT_TYPE, BT_SVC_AUD_PATH_LEA_UC);
            app_dongle_start_scan();
        } break;
        case NV_APP_DONGLE_LEA_BC:
        {
            app_dongle_open(APP_BT_DONGLE_INPUT_TYPE, BT_SVC_AUD_PATH_LEA_BC);
            app_dongle_start_scan();
        } break;
        default:
        break;
    }

    app_dongle_test_init();
}

void app_dongle_deinit(void)
{
    app_dongle_test_deinit();
}

