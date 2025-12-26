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
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "cmsis_os2.h"

#include "bluetooth.h"
#include "bluetooth_ble_api.h"

#include "adapter_service.h"

#include "app_bis_selfscan.h"

#include "gaf_bis_media_stream.h"

#if (IBRT)
#include "app_bt_sync.h"
#include "bts_core_if.h"
#include "bts_tws_api.h"
#endif

#ifdef BIS_SELFSCAN_ENABLED
/*DEFINITIONS*/

#define BIS_SELFSCAN_TWS_PAST_SUPPORT (0)
#define BISSELFSCAN_DFT_CON_LID (0x30)
#define BIS_SELFSCAN_HCI_TWS_PAST_CMD_OPCODE (0xfdf8)
#define BIS_SELFSCAN_HCI_ERR_TWS_PAST_CANCEL_SYNC (0xb8)
#define BIS_SELFSCAN_PAST_PA_LOST_TIMEOUT_10MS (1000)
#define BISSELFSCAN_SYNC_SCAN_OFFSET_10MS (50)

uint8_t empty_bcast_id[3] = {0};
uint8_t empty_bcast_code[16] = {0};

static const app_bis_selfscan_init_cfg_t init_cfg_default = {10, 10, 10};

#define APP_BIS_SELFSCAN_SET_SCAN_STATE(_start)     do { p_app_bis_selfscan_env->state &= ~APP_BIS_SELFSCAN_STATE_SCANNING_BIT;\
                                                         p_app_bis_selfscan_env->state |= (_start & APP_BIS_SELFSCAN_STATE_SCANNING_BIT); } while (0);
#define APP_BIS_SELFSCAN_SET_SYNC_STATE(_state)     do { p_app_bis_selfscan_env->state &= ~APP_BIS_SELFSCAN_STATE_SYN_MASK;\
                                                         p_app_bis_selfscan_env->state |= (_state & APP_BIS_SELFSCAN_STATE_SYN_MASK); } while (0);
#define APP_BIS_SELFSCAN_GET_SCAN_STATE()           ((p_app_bis_selfscan_env->state & APP_BIS_SELFSCAN_STATE_SCANNING_BIT) != 0)
#define APP_BIS_SELFSCAN_GET_SYNC_STATE()           (p_app_bis_selfscan_env->state & APP_BIS_SELFSCAN_STATE_SYN_MASK)
#define APP_BIS_SELFSCAN_GET_LEFT_TIMER(curr_t_ms)  (((int)(p_app_bis_selfscan_env->scan_end_to_ms - (curr_t_ms)) > 0) ? \
                                                        (p_app_bis_selfscan_env->scan_end_to_ms - (curr_t_ms)) : 0)

static void app_bis_selfscan_scan_timer_handler(void const *param);
osTimerDef(APP_BIS_SELFSCAN_SCAN_TIMER, app_bis_selfscan_scan_timer_handler);

static void app_bis_selfscan_play_timer_handler(void const *param);
osTimerDef(APP_BIS_SELFSCAN_PLAY_TIMER, app_bis_selfscan_play_timer_handler);

static void app_bis_selfscan_resume_latest_sync(uint8_t device_id, uint32_t param);
static void app_bis_selfscan_tws_past_req_recv(void);

/*ENUMERATIONS*/

enum app_bis_selfscan_state
{
    /// Indicate scan state
    APP_BIS_SELFSCAN_STATE_SCANNING_BIT = 0x01,
    /// Indicate sync state idle
    APP_BIS_SELFSCAN_STATE_SYNC_IDLE    = 0b000,
    /// Indicate sync state ing
    APP_BIS_SELFSCAN_STATE_SYNCING      = 0b010,
    /// Indicate sync state ing
    APP_BIS_SELFSCAN_STATE_SYNCED       = 0b100,
    /// Indicate sync state
    APP_BIS_SELFSCAN_STATE_SYN_MASK     = 0b110,
};

enum app_bis_selfscan_tws_op
{
    /// TWS start scan
    APP_BIS_SELFSCAN_TWS_OP_START_SCAN = 0x70,
    /// TWS stop scan
    APP_BIS_SELFSCAN_TWS_OP_STOP_SCAN  = 0x71,
    /// TWS start sync
    APP_BIS_SELFSCAN_TWS_OP_START_SYNC = 0x80,
    /// TWS stop sync
    APP_BIS_SELFSCAN_TWS_OP_STOP_SYNC  = 0x81,
    /// TWS sync state
    APP_BIS_SELFSCAN_TWS_OP_SYNC_STATE = 0xA0,
    /// TWS PAST request
    APP_BIS_SELFSCAN_TWS_OP_PAST_REQ   = 0xB0,
};

enum app_bis_selfscan_scan_user
{
    /// Scan is started by app
    APP_BIS_SELFSCAN_SCAN_USER_APP = 0x01,
    /// Scan is started by sync cmd
    APP_BIS_SELFSCAN_SCAN_USER_SYNC = 0x02,
    /// Scan is refresh callback by tws
    APP_BIS_SELFSCAN_SCAN_USER_TWS = 0x04,
};

struct app_bis_selfscan_hci_tws_past_cmd
{
    uint16_t sync_hdl;
    /// Service data
    uint16_t serv_data;
};

struct app_bis_selfscan_src
{
    struct list_node node_head;
    // Scan result
    struct app_bis_src_scan_result scan_result;
};

struct app_bis_selfscan_env
{
    /// Module callbacks
    const app_bis_selfscan_evt_cb_t *p_evt_cb;
    /// Maximum scan result list size
    uint8_t max_result_present;
    /// Scan sync state
    uint8_t state;
    /// Scan user bitfield
    uint8_t scan_user_bf;
    /// Scan timer ID
    osTimerId scan_timer_id;
    /// Sync play timer ID
    osTimerId play_timer_id;
    /// Scan result list current size
    uint8_t scan_result_size;
    /// Scan timeout in second, 0 means forever
    uint16_t scan_timeout_s;
    /// Sync PA/BIS timeout in second, 0 means forever
    uint16_t sync_timeout_s;
    /// Restart PA sync after PA sync failed
    uint8_t resync_pa_times_max;
    /// Scan result for bis src
    struct list_node scan_result_list;
    /// Sync select
    struct
    {
        /// Address info
        ble_bdaddr_t src_addr;
        /// Adv sid
        uint8_t adv_sid;
        /// Is encrypted
        bool encrypted;
        /// Sync err
        uint16_t err_code;
        /// Broadcast code
        uint8_t bcast_code[GAP_KEY_LEN];
        /// Current Sync PA lid
        uint8_t pa_lid;
    } select_src;
    /// Peer device sync state
    bool tws_peer_sync_estb;
    /// Wait resume sync
    bool need_resume_sync;
    /// backgroud scan enable
    bool bg_scan_running;
    /// Restart sync pa count
    uint8_t resync_pa_cnt;
    /// Scan timer running time end to
    uint32_t scan_end_to_ms;
} *p_app_bis_selfscan_env = NULL;

uint8_t app_bis_selfscan_buf[sizeof(struct app_bis_selfscan_env)] = {0};

/*INTERNAL FUNCTIONS DECLARATION*/
static struct app_bis_selfscan_src *app_bis_selfscan_get_scan_result_by_src_lid(uint8_t src_lid_input);
static struct app_bis_selfscan_src *app_bis_selfscan_get_scan_result_by_addr_info(const ble_bdaddr_t *p_src_addr, uint8_t adv_sid);
static uint16_t app_bis_selfscan_store_scan_result_into_list(const struct app_bis_src_scan_result *scan_result);
static void app_bis_selfscan_cleanup_scan_result_list(void);
static void app_bis_selfscan_dp_event_cb(DP_EVENT_TYPE_E event_type, ble_if_app_dp_param_u *para_p);
static int app_bis_selfscan_start_sync_impl(ble_bdaddr_t *p_addr, uint8_t adv_sid, bool is_enc, const uint8_t *p_bcast_code);
static int app_bis_selfscan_stop_sync_impl(void);
static int app_bis_selfscan_restart_sync_impl(ble_bdaddr_t *p_addr, uint8_t adv_sid, bool is_enc, const uint8_t *p_bcast_code);
static int app_bis_selfscan_start_scan(uint8_t user_bf, uint16_t scan_timeout_10ms);
static int app_bis_selfscan_stop_scan(uint8_t user_bf);
static int app_bis_selfscan_start_sync(uint8_t select_src_lid, const uint8_t *p_bcast_code);
static int app_bis_selfscan_stop_sync(void);
static int app_bis_selfscan_set_bcast_code(const uint8_t *bcast_code);
#if (IBRT)
static void app_bis_selfscan_tws_sync_play_handler(void);
static void app_bis_selfscan_tws_sync_timeout_handler(uint32_t opCode, bool triStatus, bool triInfoSentStatus);
static int app_bis_slefscan_tws_set_sync_time_to_play(void);
#endif
static void app_bis_selfscan_bg_scan_enable(bool start_scan, ble_bdaddr_t *src_addr);
static bool app_bis_selfscan_get_ad_ltv(const uint8_t *p_data_ltv, uint8_t total_len, uint8_t ad_type, const uint8_t **pp_ltv_get);

#if (IBRT)
// BT SYNC ADD TABLE
APP_BT_SYNC_COMMAND_TO_ADD(APP_BT_SYNC_OP_BIS_SELFSCAN, app_bis_selfscan_tws_sync_play_handler, app_bis_selfscan_tws_sync_timeout_handler);
#endif

/*EXTERNAL FUNCTIONS DECLARATION*/
int app_bis_selfscan_init(const app_bis_selfscan_init_cfg_t *p_init_cfg, const app_bis_selfscan_evt_cb_t *p_evt_cb)
{
    uint16_t size = 0;

    if (p_app_bis_selfscan_env != NULL)
    {
        return BT_STS_SUCCESS;
    }

    if (p_init_cfg == NULL)
    {
        p_init_cfg = &init_cfg_default;
    }

    if (p_init_cfg->max_result_present == 0)
    {
        return BT_STS_INVALID_PARM;
    }

    if (p_evt_cb != NULL &&
            (p_evt_cb->cb_bcast_code_req == NULL ||
             p_evt_cb->cb_scan_result == NULL ||
             p_evt_cb->cb_scan_state == NULL ||
             p_evt_cb->cb_sync_state == NULL))
    {
        return BT_STS_INVALID_PARM;
    }

    size = sizeof(app_bis_selfscan_buf);

    p_app_bis_selfscan_env = (struct app_bis_selfscan_env *)app_bis_selfscan_buf;

    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_NO_RESOURCES;
    }

    memset(p_app_bis_selfscan_env, 0, size);
#if (IBRT == 0)
    p_app_bis_selfscan_env->tws_peer_sync_estb = true;
#endif /* IBRT */
    // Scan result list
    INIT_LIST_HEAD(&p_app_bis_selfscan_env->scan_result_list);
    // Scan result list capacity
    p_app_bis_selfscan_env->max_result_present = p_init_cfg->max_result_present;
    // Store custom callbacks
    p_app_bis_selfscan_env->p_evt_cb = p_evt_cb;
    // Parameter for scan and sync
    p_app_bis_selfscan_env->scan_timeout_s = p_init_cfg->scan_timeout_s;
    p_app_bis_selfscan_env->sync_timeout_s = p_init_cfg->sync_timeout_s;
    p_app_bis_selfscan_env->resync_pa_times_max = p_init_cfg->resync_pa_times_max;
    // Scan timer
    p_app_bis_selfscan_env->scan_timer_id =
        osTimerCreate(osTimer(APP_BIS_SELFSCAN_SCAN_TIMER), osTimerOnce, NULL);
    // PA lid default invalid
    p_app_bis_selfscan_env->select_src.pa_lid = APP_BIS_SELFSCAN_INVALID_SRC_LID;

    if (p_app_bis_selfscan_env->scan_timer_id == NULL)
    {
        p_app_bis_selfscan_env = NULL;
        return BT_STS_NO_RESOURCES;
    }

    // Play timer
    p_app_bis_selfscan_env->play_timer_id =
        osTimerCreate(osTimer(APP_BIS_SELFSCAN_PLAY_TIMER), osTimerOnce, NULL);

    if (p_app_bis_selfscan_env->play_timer_id == NULL)
    {
        osTimerDelete(p_app_bis_selfscan_env->scan_timer_id);
        p_app_bis_selfscan_env = NULL;
        return BT_STS_NO_RESOURCES;
    }

    if (p_evt_cb == NULL)
    {
        bes_ble_datapath_server_register_event_cb(app_bis_selfscan_dp_event_cb);
    }

#if (IBRT)
    uint32_t select_chn_bf = bts_tws_if_is_nv_master() ?
                             (BES_BLE_LOC_FRONT_RIGHT | BES_BLE_LOC_SIDE_RIGHT) : \
                             (BES_BLE_LOC_FRONT_LEFT | BES_BLE_LOC_SIDE_LEFT);
#else
    uint32_t select_chn_bf = BES_BLE_LOC_FRONT_RIGHT | BES_BLE_LOC_FRONT_LEFT;
#endif

    bes_ble_bis_sink_set_aud_location_bf(select_chn_bf);

    BTAPP_TRACE(1, "%s scan %d, sync %d, resync %d, select bf 0x%x", __func__,
          p_app_bis_selfscan_env->scan_timeout_s,
          p_app_bis_selfscan_env->sync_timeout_s,
          p_app_bis_selfscan_env->resync_pa_times_max, select_chn_bf);

    return BT_STS_SUCCESS;
}

int app_bis_selfscan_deinit(void)
{
    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_SUCCESS;
    }

    if (APP_BIS_SELFSCAN_GET_SCAN_STATE() != 0
            || APP_BIS_SELFSCAN_GET_SYNC_STATE() != 0)
    {
        return BT_STS_NOT_ALLOW;
    }

    // Scan result list clear
    app_bis_selfscan_cleanup_scan_result_list();
    // Stop timer and delete it
    osTimerStop(p_app_bis_selfscan_env->scan_timer_id);
    osTimerDelete(p_app_bis_selfscan_env->scan_timer_id);
    osTimerStop(p_app_bis_selfscan_env->play_timer_id);
    osTimerDelete(p_app_bis_selfscan_env->play_timer_id);

    p_app_bis_selfscan_env = NULL;

    return BT_STS_SUCCESS;
}

#if (APP_BIS_SELFSCAN_SCAN_USER_TWS)
static void app_bis_selfscan_cmd_cmp_handle(uint16_t cmd_opcode, struct hci_cmd_evt_param_t *param)
{
    BTAPP_TRACE(1, "%s opcode 0x%x, status 0x%x", __func__, cmd_opcode, param->event_status);
}
#endif /* APP_BIS_SELFSCAN_SCAN_USER_TWS */

static uint16_t app_bis_selfscan_start_tws_past(uint16_t pa_sync_hdl)
{
#if (APP_BIS_SELFSCAN_SCAN_USER_TWS)
    if (pa_sync_hdl == 0xFFFF)
    {
        return BT_STS_INVALID_PARM;
    }

    struct app_bis_selfscan_hci_tws_past_cmd *cmd = NULL;
    struct pp_buff *ppb = NULL;

    cmd = (struct app_bis_selfscan_hci_tws_past_cmd *)
          gap_create_hci_cmd_packet(&ppb, BIS_SELFSCAN_HCI_TWS_PAST_CMD_OPCODE,
                                    sizeof(struct app_bis_selfscan_hci_tws_past_cmd),
                                    app_bis_selfscan_cmd_cmp_handle, NULL);
    if (cmd == NULL)
    {
        return BT_STS_NO_RESOURCES;
    }

    cmd->sync_hdl = pa_sync_hdl;

    return gap_send_hci_cmd_packet(ppb);
#else
    return BT_STS_NOT_SUPPORT;
#endif
}

#if (IBRT)
static void app_bis_selfscan_start_sync_play_timer(uint32_t timeout_ms)
{
    if (osTimerIsRunning(p_app_bis_selfscan_env->play_timer_id))
    {
        osTimerStop(p_app_bis_selfscan_env->play_timer_id);
    }

    if (timeout_ms != 0)
    {
        osTimerStart(p_app_bis_selfscan_env->play_timer_id, timeout_ms);
    }
}

static void app_bis_selfscan_tws_sync_play_handler(void)
{
    // Stop sync play timer
    app_bis_selfscan_start_sync_play_timer(0);

    gaf_bis_audio_stream_set_stream_volume(TGT_VOLUME_LEVEL_7);
}

static void app_bis_selfscan_tws_sync_timeout_handler(uint32_t opCode, bool triStatus, bool triInfoSentStatus)
{
    if ((!triStatus) && (APP_BT_SYNC_OP_BIS_SELFSCAN == opCode))
    {
        app_bis_selfscan_tws_sync_play_handler();
    }
}
#endif

static uint8_t app_bis_selfscan_get_scan_state(void)
{
    return APP_BIS_SELFSCAN_GET_SCAN_STATE();
}

static void app_bis_selfscan_send_scan_state(void)
{
    uint8_t scan_state = app_bis_selfscan_get_scan_state();

    bes_ble_datapath_server_send_data_via_notification(BISSELFSCAN_DFT_CON_LID, &scan_state, sizeof(scan_state));
}

static void app_bis_selfscan_send_scan_result(uint8_t src_lid, struct app_bis_src_scan_result *p_result)
{
    uint16_t size = sizeof(ble_bdaddr_t) + 7 * sizeof(uint8_t) + p_result->broadcast_name_len;

    uint8_t *value = bes_bt_buf_malloc(size);

    if (value)
    {
        value[0] = APP_BIS_SELFSCAN_OP_SCAN_RESULT_NTF;
        value += sizeof(uint8_t);
        value[0] = src_lid;
        value += sizeof(uint8_t);
        memcpy(value, &p_result->src_addr, sizeof(ble_bdaddr_t));
        value += sizeof(ble_bdaddr_t);
        value[0] = p_result->adv_sid;
        value += sizeof(uint8_t);
        value[0] = APP_BIS_SELFSCAN_ASCLL;
        value += sizeof(uint8_t);
        value[0] = p_result->broadcast_name_len;
        value += sizeof(uint8_t);
        memcpy(value, p_result->broadcast_name, p_result->broadcast_name_len);
        value += p_result->broadcast_name_len;
        value[0] = APP_BIS_SELFSCAN_ASCLL;
        value += sizeof(uint8_t);
        /// TODO:extended descrptor
        value[0] = 0;
        value += sizeof(uint8_t);
        bes_ble_datapath_server_send_data_via_notification(BISSELFSCAN_DFT_CON_LID, value - size, size);
        bes_bt_buf_free(value - size);
    }
}

static uint8_t app_bis_selfscan_get_auracast_state(void)
{
    uint8_t state = APP_BIS_SELFSCAN_GET_SCAN_STATE();

    if (state == APP_BIS_SELFSCAN_STATE_SYNC_IDLE)
    {
        state = APP_BIS_SELFSCAN_STATE_IDLE;
    }
    else if (state == APP_BIS_SELFSCAN_STATE_SYNCING)
    {
        state = APP_BIS_SELFSCAN_STATE_BUSY;
    }
    else
    {
        state = APP_BIS_SELFSCAN_STATE_RECV;
    }

    return state;
}

static void app_bis_selfscan_send_src_select_state(bool need_op_present)
{
    uint16_t size = sizeof(ble_bdaddr_t) + (3 + need_op_present) * sizeof(uint8_t) + 0;

    uint8_t *value = bes_bt_buf_malloc(size);

    if (value)
    {
        if (need_op_present)
        {
            value[0] = APP_BIS_SELFSCAN_OP_SCAN_RESULT_NTF;
            value += sizeof(uint8_t);
        }
        value[0] = app_bis_selfscan_get_auracast_state();
        value += sizeof(uint8_t);
        value[0] = p_app_bis_selfscan_env->select_src.err_code;
        value += sizeof(uint8_t);
        memcpy(value, &p_app_bis_selfscan_env->select_src.src_addr, sizeof(ble_bdaddr_t));
        value += sizeof(ble_bdaddr_t);
        value[0] = p_app_bis_selfscan_env->select_src.adv_sid;
        value += sizeof(uint8_t);
        /*memcpy(value, p_result->broadcast_name, p_result->broadcast_name_len);
        value += p_result->broadcast_name_len;*/
        bes_ble_datapath_server_send_data_via_notification(BISSELFSCAN_DFT_CON_LID, value - size, size);
        bes_bt_buf_free(value - size);
    }
}

static void app_bis_selfscan_inform_upper_scan_state(bool started)
{
    if (p_app_bis_selfscan_env->p_evt_cb != NULL && p_app_bis_selfscan_env->p_evt_cb->cb_scan_state != NULL)
    {
        p_app_bis_selfscan_env->p_evt_cb->cb_scan_state(started);
    }
}

static void app_bis_selfscan_inform_upper_scan_result(bool src_lid, const struct app_bis_src_scan_result *p_result)
{
    if (p_app_bis_selfscan_env->p_evt_cb != NULL && p_app_bis_selfscan_env->p_evt_cb->cb_scan_result != NULL)
    {
        p_app_bis_selfscan_env->p_evt_cb->cb_scan_result(src_lid, p_result);
    }
}

static void app_bis_selfscan_inform_upper_sync_state(enum selfscan_sync_state sync_state, uint16_t err_code)
{
    if (p_app_bis_selfscan_env->p_evt_cb != NULL && p_app_bis_selfscan_env->p_evt_cb->cb_sync_state != NULL)
    {
        p_app_bis_selfscan_env->p_evt_cb->cb_sync_state(&p_app_bis_selfscan_env->select_src.src_addr,
                                                        p_app_bis_selfscan_env->select_src.adv_sid,
                                                        sync_state, err_code);
    }
}

static void app_bis_selfscan_inform_upper_bcast_code_req(bool src_lid)
{
    if (p_app_bis_selfscan_env->p_evt_cb != NULL && p_app_bis_selfscan_env->p_evt_cb->cb_bcast_code_req != NULL)
    {
        p_app_bis_selfscan_env->p_evt_cb->cb_bcast_code_req(src_lid);
    }
}

static int app_bis_selfscan_tws_sync_start_scan_cmd(bool start)
{
#if (IBRT)
    uint8_t scan_start_op = start ? APP_BIS_SELFSCAN_TWS_OP_START_SCAN : APP_BIS_SELFSCAN_TWS_OP_STOP_SCAN;
    return tws_ctrl_send_cmd(APP_TWS_CMD_SEND_BIS_SELFSCAN_INFO, &scan_start_op, sizeof(scan_start_op));
#else
    return -1;
#endif /* IBRT */
}

static int app_bis_selfscan_tws_sync_start_sync_cmd(ble_bdaddr_t *p_addr, uint8_t adv_sid, const uint8_t *p_bcast_code)
{
    uint8_t buf[sizeof(ble_bdaddr_t) + 3 * sizeof(uint8_t) + GAP_KEY_LEN] = {0};

    uint8_t *ptr = buf;
    *ptr = APP_BIS_SELFSCAN_TWS_OP_START_SYNC;
    ptr++;
    memcpy(ptr, p_addr, sizeof(ble_bdaddr_t));
    ptr += sizeof(ble_bdaddr_t);
    *ptr = adv_sid;
    ptr++;
    if (p_bcast_code)
    {
        // Encrption
        *ptr = true;
        ptr++;
        memcpy(ptr, p_bcast_code, GAP_KEY_LEN);
    }
    else
    {
        *ptr = false;
    }

#if (IBRT)
    return tws_ctrl_send_cmd(APP_TWS_CMD_SEND_BIS_SELFSCAN_INFO, buf, sizeof(buf));
#else
    return -1;
#endif
}

static int app_bis_selfscan_tws_sync_stop_sync_cmd(void)
{
#if (IBRT)
    uint8_t buf[sizeof(uint8_t)] = {APP_BIS_SELFSCAN_TWS_OP_STOP_SYNC};
    return tws_ctrl_send_cmd(APP_TWS_CMD_SEND_BIS_SELFSCAN_INFO, buf, sizeof(buf));
#else
    return -1;
#endif
}

POSSIBLY_UNUSED static int app_bis_selfscan_tws_sync_state(bool success)
{
#if (IBRT)
    uint8_t buf[sizeof(uint8_t) + sizeof(uint8_t) +
                sizeof(ble_bdaddr_t) + 2 * sizeof(uint8_t) + GAP_KEY_LEN] = {APP_BIS_SELFSCAN_TWS_OP_SYNC_STATE};
    uint8_t *ptr = buf + 1;
    *ptr = success;
    ptr++;
    memcpy(ptr, &p_app_bis_selfscan_env->select_src.src_addr, sizeof(ble_bdaddr_t));
    ptr += sizeof(ble_bdaddr_t);
    *ptr = p_app_bis_selfscan_env->select_src.adv_sid;
    ptr++;
    if (p_app_bis_selfscan_env->select_src.encrypted)
    {
        // Encrption
        *ptr = true;
        ptr++;
        memcpy(ptr, p_app_bis_selfscan_env->select_src.bcast_code, GAP_KEY_LEN);
    }
    else
    {
        *ptr = false;
    }

    return tws_ctrl_send_cmd(APP_TWS_CMD_SEND_BIS_SELFSCAN_INFO, buf, sizeof(buf));
#else
    return -1;
#endif
}

POSSIBLY_UNUSED static int app_bis_selfscan_tws_past_request(void)
{
#if (IBRT)
    uint8_t buf[sizeof(uint8_t)] = {APP_BIS_SELFSCAN_TWS_OP_PAST_REQ};
    return tws_ctrl_send_cmd(APP_TWS_CMD_SEND_BIS_SELFSCAN_INFO, buf, sizeof(buf));
#else
    return -1;
#endif
}

static void app_bis_selfscan_tws_connected_defer_cb(void)
{
    // Check local PA sync state
    uint8_t pa_lid = p_app_bis_selfscan_env->select_src.pa_lid;
    // Trigger Peer update select src info
    app_bis_selfscan_tws_sync_state(pa_lid != APP_BIS_SELFSCAN_INVALID_SRC_LID);

    BTAPP_TRACE(1, "%s pa_lid = %d", __func__, pa_lid);

    if (pa_lid != APP_BIS_SELFSCAN_INVALID_SRC_LID)
    {
        // Defer start PAST over TWS
        bt_thread_call_func_1(app_bis_selfscan_start_tws_past,
                              bt_fixed_param(bes_ble_bis_sink_get_pa_sync_hdl_by_pa_lid(pa_lid)));
    }
    else
    {
        // Allow PAST when tws is connected
        gap_set_default_past_parameters(3, 0, BIS_SELFSCAN_PAST_PA_LOST_TIMEOUT_10MS, 0);
        // Local need peer's PAST
        app_bis_selfscan_tws_past_request();
    }
}

void app_bis_selfscan_tws_state_handler(bool connected)
{
    BTAPP_TRACE(0, "%s connected %d", __func__, connected);

    if (p_app_bis_selfscan_env == NULL)
    {
        return;
    }

    if (connected)
    {
        bt_thread_call_func_0(app_bis_selfscan_tws_connected_defer_cb);
    }
    else
    {
        p_app_bis_selfscan_env->tws_peer_sync_estb = true;
    }
}

static bool app_bis_selfscan_dp_rx_validation(const uint8_t *rx_data, uint16_t data_len)
{
    BTAPP_TRACE(1, "%p %d", rx_data, data_len);
    if (rx_data == NULL || data_len == 0)
    {
        return false;
    }

    switch (rx_data[0])
    {
        case APP_BIS_SELFSCAN_OP_GET_SCAN_STATE:
        case APP_BIS_SELFSCAN_OP_GET_SELECT_SRC:
        case APP_BIS_SELFSCAN_OP_STOP_SYNC:
        {
            if (data_len != sizeof(uint8_t))
            {
                return false;
            }
        }
        break;
        // Stop scan
        case APP_BIS_SELFSCAN_OP_SCAN_COMMAND:
        {
            if (data_len != 2 * sizeof(uint8_t) ||
                    rx_data[1] > APP_BIS_SELFSCAN_SCAN_STOP)
            {
                return false;
            }
        }
        break;
        // Start sync
        case APP_BIS_SELFSCAN_OP_SRC_SELECT_CMD:
        {
            /// TODO:
        }
        break;
        // Set broadcast code
        case APP_BIS_SELFSCAN_OP_BCAST_CODE:
        {
            if (data_len != sizeof(uint8_t) + GAP_KEY_LEN)
            {
                return false;
            }
        }
        break;
        default:
            return false;
    }

    return true;
}

static void app_bis_selfscan_bg_scan_data_report_handler(bes_ble_bdaddr_t *bleAddr, int8_t rssi,
                                                         uint8_t evt_type, uint8_t *adv_buf, uint8_t len)
{
    const uint8_t *p_bcast_name = NULL;
    uint8_t broadcast_name_len = 0;

    if (app_bis_selfscan_get_ad_ltv(adv_buf, len, BES_GAP_AD_TYPE_ADV_BCAST_NAME, &p_bcast_name))
    {
        broadcast_name_len = (p_bcast_name[0] - 1);
        p_bcast_name = p_bcast_name + 2;
        DUMP8("%c", p_bcast_name, broadcast_name_len);
    }
    else
    {
        return;
    }

    if (memcmp(bleAddr, &p_app_bis_selfscan_env->select_src.src_addr, sizeof(bes_ble_bdaddr_t)) == 0)
    {
        BTAPP_TRACE(1, "Find select src");
        DUMP8("%02x ", bleAddr, 7);
        // TWS inform peer start sync
        app_bis_selfscan_tws_sync_start_sync_cmd(&p_app_bis_selfscan_env->select_src.src_addr,
                                                 p_app_bis_selfscan_env->select_src.adv_sid,
                                                 p_app_bis_selfscan_env->select_src.encrypted ?
                                                 p_app_bis_selfscan_env->select_src.bcast_code : NULL);
        // Start resume latest sync
        app_bis_selfscan_resume_latest_sync(0, 0);
    }
}

static void app_bis_selfscan_bg_scan_enable(bool start_scan, ble_bdaddr_t *src_addr)
{
    BTAPP_TRACE(1, "%s start %d", __func__, start_scan);

    if (start_scan == true)
    {
        p_app_bis_selfscan_env->bg_scan_running = true;

        bes_ble_gap_adv_report_callback_register(BES_BLE_SCAN_RESULT_USER_ID_0, app_bis_selfscan_bg_scan_data_report_handler);

        if (src_addr != NULL)
        {
            bes_ble_gap_set_white_list(BLE_WHITE_LIST_USER_MOBILE, src_addr, 1);
        }

        bes_ble_scan_param_t scan_param =
        {
            .scanFolicyType = src_addr ? BLE_SCAN_ALLOW_ADV_WLST : BLE_SCAN_ALLOW_ADV_ALL,
            .scanWindowMs = 20,
            .scanIntervalMs = 500,
            /// Means never stop
            .scanDurationMs = 0,
        };

        bes_ble_gap_start_scan(&scan_param);
    }
    else
    {
        bes_ble_gap_stop_scan();
        bes_ble_gap_adv_report_callback_register(BES_BLE_SCAN_RESULT_USER_ID_0, NULL);

        p_app_bis_selfscan_env->bg_scan_running = false;
    }
}

void app_bis_selfscan_dp_rx_data_handler(const uint8_t *rx_data, uint16_t data_len)
{
    uint8_t opcode = rx_data[0];

    switch (opcode)
    {
        // Scan state
        case APP_BIS_SELFSCAN_OP_GET_SCAN_STATE:
        {
            app_bis_selfscan_send_scan_state();
        }
        break;
        // Src select state
        case APP_BIS_SELFSCAN_OP_GET_SELECT_SRC:
        {
            app_bis_selfscan_send_src_select_state(false);
        }
        break;
        // Stop scan
        case APP_BIS_SELFSCAN_OP_SCAN_COMMAND:
        {
            if (rx_data[1] == APP_BIS_SELFSCAN_SCAN_START)
            {
                app_bis_selfscan_tws_sync_start_scan_cmd(true);
                app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_APP, 0);
            }
            else if (rx_data[1] == APP_BIS_SELFSCAN_SCAN_STOP)
            {
                app_bis_selfscan_tws_sync_start_scan_cmd(false);
                app_bis_selfscan_stop_scan(APP_BIS_SELFSCAN_SCAN_USER_APP);
            }
        }
        break;
        // Start sync
        case APP_BIS_SELFSCAN_OP_SRC_SELECT_CMD:
        {
            app_bis_selfscan_start_sync(rx_data[1], data_len == GAP_KEY_LEN + 2 ? &rx_data[2] : NULL);
        }
        break;
        // Stop sync
        case APP_BIS_SELFSCAN_OP_STOP_SYNC: //
            app_bis_selfscan_stop_sync();
            break;
        // Set broadcast code
        case APP_BIS_SELFSCAN_OP_BCAST_CODE:
            app_bis_selfscan_set_bcast_code(&rx_data[1]);
            break;
        default:
            break;
    }
}

static void app_bis_selfscan_dp_event_cb(DP_EVENT_TYPE_E event_type, ble_if_app_dp_param_u *para_p)
{
    switch (event_type)
    {
        case DP_DATA_RECEIVED:
        {
            if (app_bis_selfscan_dp_rx_validation(para_p->dp_recv_data.data, para_p->dp_recv_data.data_len) == false)
            {
                BTAPP_TRACE(1, "Invalid RX data");
                break;
            }

            bt_thread_call_func_2(app_bis_selfscan_dp_rx_data_handler,
                                  bt_alloc_param_size(para_p->dp_recv_data.data, para_p->dp_recv_data.data_len),
                                  bt_fixed_param(para_p->dp_recv_data.data_len));
        }
        break;
        default:
            break;
    }
}

static bool app_bis_selfscan_restart_pa_sync(void)
{
    if (p_app_bis_selfscan_env->resync_pa_cnt >= p_app_bis_selfscan_env->resync_pa_times_max)
    {
        return false;
    }

    // If peer is pa sync established and alt use past and pa sync
    if (p_app_bis_selfscan_env->tws_peer_sync_estb &&
            (p_app_bis_selfscan_env->resync_pa_cnt % 2 == 0))
    {
        // Request peer's PAST
        app_bis_selfscan_tws_past_request();
    }
    // Start local sync and also wait peer past if needed
    bt_thread_call_func_2(app_bis_selfscan_resume_latest_sync, bt_fixed_param(0), bt_fixed_param(0));
    // Increase restart pa sync count
    BTAPP_TRACE(1, "Restart PA sync, curr try cnt %d, max %d",
          p_app_bis_selfscan_env->resync_pa_cnt + 1,
          p_app_bis_selfscan_env->resync_pa_times_max);
    p_app_bis_selfscan_env->resync_pa_cnt++;
    return true;
}

static void app_bis_selfscan_pa_sync_state_handler(bool synced, uint8_t pa_lid, uint16_t reason)
{
    if (synced == false)
    {
        p_app_bis_selfscan_env->select_src.pa_lid = APP_BIS_SELFSCAN_INVALID_SRC_LID;
        // Allow PAST when pa sync is lost
        gap_set_default_past_parameters(3, 0, BIS_SELFSCAN_PAST_PA_LOST_TIMEOUT_10MS, 0);
        // PA sync is failed
        if (pa_lid == APP_BIS_SELFSCAN_INVALID_SRC_LID)
        {
            // PA sync canceled due to tws past is incoming, ignore it
            if (reason == BIS_SELFSCAN_HCI_ERR_TWS_PAST_CANCEL_SYNC)
            {
                BTAPP_TRACE(1, "TWS PAST canceled PA SYNC");
                return;
            }

            APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNC_IDLE);
            // If Faile to be established or timeout, retry if needed
            if (reason == BT_LL_ERR_CON_FAILED_TO_BE_EST ||
                    reason == BT_LL_ERR_OP_CANCELED_BY_HOST)
            {
                // Restart PA sync success
                if (app_bis_selfscan_restart_pa_sync() == true)
                {
                    return;
                }
            }

            app_bis_selfscan_send_src_select_state(true);
            app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_FAILED, reason);
        }
        else // PA sync is terminated
        {
            // PA lost or term befor BIG sync established
            if (APP_BIS_SELFSCAN_GET_SYNC_STATE() != APP_BIS_SELFSCAN_STATE_SYNCED)
            {
                APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNC_IDLE);

                if (reason == BT_LL_ERR_CON_TIMEOUT)
                {
                    // Restart PA sync success
                    if (app_bis_selfscan_restart_pa_sync() == true)
                    {
                        return;
                    }
                }
                else if (p_app_bis_selfscan_env->select_src.err_code == BT_LL_ERR_REMOTE_USER_TERM_CON)
                {
                    BTAPP_TRACE(1, "Sink is stopped due to %d, start BG scan", reason);
                    app_bis_selfscan_bg_scan_enable(true, NULL);
                }
            }

            if (p_app_bis_selfscan_env->need_resume_sync)
            {
                p_app_bis_selfscan_env->need_resume_sync = false;
                bt_thread_call_func_2(app_bis_selfscan_resume_latest_sync,
                                      bt_fixed_param(0), bt_fixed_param(0));
                return;
            }
        }
    }
    else
    {
        // Record PA sync lid
        p_app_bis_selfscan_env->select_src.pa_lid = pa_lid;

        if (pa_lid != APP_BIS_SELFSCAN_INVALID_SRC_LID)
        {
            // Avoid past when pa sync is established
            gap_set_default_past_parameters(0, 0, 0, 0);
            // Only start PAST when peer is not synced to pa yet
            if (p_app_bis_selfscan_env->tws_peer_sync_estb == false)
            {
                app_bis_selfscan_start_tws_past(bes_ble_bis_sink_get_pa_sync_hdl_by_pa_lid(pa_lid));
            }
        }
    }
    // Clear count
    p_app_bis_selfscan_env->resync_pa_cnt = 0;
}

static void app_bis_selfscan_scan_state_callback(bool scan_or_pa_sync, bool started, uint32_t param)
{
    uint8_t pa_lid = (param & 0xFF0000) >> 16;
    uint16_t err_code = (param & 0xFFFF);

    BTAPP_TRACE(0, "scan_state_cb: scan_or_pa=%d, started=%d, pa_lid=%d, err_code=%d", scan_or_pa_sync, started, pa_lid, err_code);

    if (scan_or_pa_sync == true)
    {
        if (started == false)
        {
            osTimerStop(p_app_bis_selfscan_env->scan_timer_id);
            p_app_bis_selfscan_env->scan_user_bf = 0;
            p_app_bis_selfscan_env->scan_end_to_ms = 0;
            APP_BIS_SELFSCAN_SET_SCAN_STATE(false);
            app_bis_selfscan_inform_upper_scan_state(false);
        }
    }
    else
    {
        app_bis_selfscan_pa_sync_state_handler(started, pa_lid, err_code);
    }
}

static bool app_bis_selfscan_get_ad_ltv(const uint8_t *p_data_ltv, uint8_t total_len, uint8_t ad_type, const uint8_t **pp_ltv_get)
{
    const uint8_t *p_end = p_data_ltv + total_len;

    while (p_end > p_data_ltv)
    {
        if (p_data_ltv[1] == ad_type)
        {
            if (pp_ltv_get != NULL)
            {
                *pp_ltv_get = p_data_ltv;
                return true;
            }
        }

        p_data_ltv += p_data_ltv[0] + 1;
    }

    return false;
}

static bool app_bis_selfscan_source_report_callback(ble_bdaddr_t *addr, uint8_t adv_sid, uint8_t *bcast_id,
                                                    uint8_t *adv_data, uint8_t adv_data_len, int8_t rssi)
{
    uint8_t broadcast_name_len = 0;
    const uint8_t *p_bcast_name = NULL;
    const uint8_t *p_pba_info = adv_data;
    bool encrypted = false;

    if (APP_BIS_SELFSCAN_GET_SCAN_STATE() == 0)
    {
        return false;
    }

    if (memcmp(bcast_id, empty_bcast_id, sizeof(empty_bcast_id)) == 0)
    {
        BTAPP_TRACE(1, "no bcast id, view as no pa related");
        return false;
    }

    if (app_bis_selfscan_get_ad_ltv(adv_data, adv_data_len, BES_GAP_AD_TYPE_ADV_BCAST_NAME, &p_bcast_name))
    {
        broadcast_name_len = (p_bcast_name[0] - 1);
        p_bcast_name = p_bcast_name + 2;
    }
    else
    {
        BTAPP_TRACE(1, "no broadcast name");
    }

    while (app_bis_selfscan_get_ad_ltv(p_pba_info, adv_data_len - (p_pba_info - adv_data),
                                       BES_GAP_AD_TYPE_SERVICE_16_BIT_DATA, &p_pba_info))
    {
        if (GATT_UUID_PBA_SERVICE != CO_COMBINE_UINT16_LE(p_pba_info + 2))
        {
            p_pba_info += (p_pba_info[0] + 1);
            continue;
        }

        BTAPP_TRACE(1, "pba feature: 0x%x", *(p_pba_info + 4));
        encrypted = (*(p_pba_info + 4) & 0x01) != 0;
        break;
    }

    uint16_t status = BT_STS_SUCCESS;

    uint16_t size = (sizeof(struct app_bis_src_scan_result) + broadcast_name_len);

    struct app_bis_src_scan_result *p_result = (struct app_bis_src_scan_result *)bes_bt_buf_malloc(size);

    if (p_result != NULL)
    {
        memset(p_result, 0, size);
        p_result->adv_sid = adv_sid;
        p_result->src_addr = *addr;
        p_result->encrypted = encrypted;
        memcpy(p_result->broadcast_id, bcast_id, sizeof(p_result->broadcast_id));
        p_result->broadcast_name_len = broadcast_name_len;

        if (p_bcast_name != NULL)
        {
            memcpy(p_result->broadcast_name, p_bcast_name, broadcast_name_len);
        }

        status = app_bis_selfscan_store_scan_result_into_list(p_result);

        if (status == BT_STS_SUCCESS)
        {
            app_bis_selfscan_send_scan_result(p_app_bis_selfscan_env->scan_result_size - 1, p_result);
            app_bis_selfscan_inform_upper_scan_result(p_app_bis_selfscan_env->scan_result_size - 1, p_result);
        }

        bes_bt_buf_free((uint8_t *)p_result);

        BTAPP_TRACE(1, "store result: %d", status);
    }

    return false;
}

static void app_bis_selfscan_sink_receive_md_callback(uint8_t subgrp_lid, uint8_t *buf, uint8_t buf_len)
{
    BTAPP_TRACE(1, "subgrp_lid %d", subgrp_lid);
}

#if (IBRT)
static void app_bis_selfscan_sink_started_defer_handler(bt_ui_role_t tws_ui_role)
{
    if (tws_ui_role == BT_IBRT_SLAVE)
    {
        app_bis_selfscan_tws_sync_state(true);
    }
    else if (tws_ui_role == BT_IBRT_MASTER)
    {
        app_bis_slefscan_tws_set_sync_time_to_play();
    }
}
#endif /* IBRT */

static void app_bis_selfscan_sink_started_handler(uint8_t grp_lid)
{
    BTAPP_TRACE(1, "grp_lid %d sink started", grp_lid);
    APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNCED);

    app_bis_selfscan_send_src_select_state(true);
    app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_SUCCESS, 0);

    // Close PA report
    bes_ble_bis_scan_pa_report_ctrl(p_app_bis_selfscan_env->select_src.pa_lid, false);

#if (IBRT)
    bt_ui_role_t tws_ui_role = bts_core_get_ui_role();

    if (tws_ui_role == BT_IBRT_UNKNOWN)
    {
        gaf_bis_audio_stream_set_stream_volume(TGT_VOLUME_LEVEL_7);
        return;
    }

    // Mute stream
    gaf_bis_audio_stream_set_stream_volume(TGT_VOLUME_LEVEL_MUTE);
    // Start timer
    app_bis_selfscan_start_sync_play_timer(p_app_bis_selfscan_env->sync_timeout_s * 1000);

    bt_thread_call_func_1(app_bis_selfscan_sink_started_defer_handler, bt_fixed_param(tws_ui_role));
#endif /* IBRT */
}

static void app_bis_selfscan_sink_stopped_handler(uint8_t grp_lid, uint16_t err_code)
{
    struct app_bis_selfscan_src *p_scan_result = NULL;

    BTAPP_TRACE(1, "grp_lid %d sink stopped %d", grp_lid, err_code);
    p_app_bis_selfscan_env->select_src.err_code = err_code;

    if (err_code == BT_LL_ERR_TERMINATED_MIC_FAILURE)
    {
        p_app_bis_selfscan_env->select_src.encrypted = true;

        p_scan_result = app_bis_selfscan_get_scan_result_by_addr_info(&p_app_bis_selfscan_env->select_src.src_addr,
                                                                      p_app_bis_selfscan_env->select_src.adv_sid);

        if (p_scan_result != NULL)
        {
            p_scan_result->scan_result.encrypted = true;
        }
    }

    APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNC_IDLE);

    app_bis_selfscan_send_src_select_state(true);
    app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_FAILED, err_code);

#if (IBRT)
    bt_ui_role_t tws_ui_role = BT_IBRT_UNKNOWN;

    tws_ui_role = bts_core_get_ui_role();

    if (tws_ui_role == BT_IBRT_SLAVE)
    {
        // Local term means peer term too
        p_app_bis_selfscan_env->tws_peer_sync_estb = false;
        app_bis_selfscan_tws_sync_state(false);
    }
    else
#endif /* IBRT */
    {
        uint8_t pa_lid = p_app_bis_selfscan_env->select_src.pa_lid;

        if (pa_lid == APP_BIS_SELFSCAN_INVALID_SRC_LID)
        {
            BTAPP_TRACE(1, "Sink is stopped due to %d, start BG scan", err_code);
            app_bis_selfscan_bg_scan_enable(true, NULL);
        }
    }
}

static int app_bis_selfscan_start_scan(uint8_t user_bf, uint16_t scan_timeout_10ms)
{
    if (bt_defer_curr_func_2(app_bis_selfscan_start_scan,
                             bt_fixed_param(user_bf),
                             bt_fixed_param(scan_timeout_10ms)))
    {
        return BT_STS_PENDING;
    }

    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_NOT_READY;
    }

    if (p_app_bis_selfscan_env->bg_scan_running)
    {
        BTAPP_TRACE(1, "BG scan is enabled, disable it first");
        app_bis_selfscan_bg_scan_enable(false, NULL);
    }

    uint32_t curr_time = TICKS_TO_MS(hal_sys_timer_get());

    p_app_bis_selfscan_env->scan_user_bf |= user_bf;

    if (p_app_bis_selfscan_env->scan_end_to_ms != 0 || osTimerIsRunning(p_app_bis_selfscan_env->scan_timer_id))
    {
        if (scan_timeout_10ms != 0 &&
                (APP_BIS_SELFSCAN_GET_LEFT_TIMER(curr_time) > (scan_timeout_10ms * 10)))
        {
            BTAPP_TRACE(1, "user:%x, scan with smaller TO %u", user_bf, scan_timeout_10ms * 10);
            return BT_STS_SUCCESS;
        }

        osTimerStop(p_app_bis_selfscan_env->scan_timer_id);
        p_app_bis_selfscan_env->scan_end_to_ms = 0;
    }

    if (scan_timeout_10ms != 0)
    {
        osTimerStart(p_app_bis_selfscan_env->scan_timer_id, scan_timeout_10ms * 10);
        // Record end to time for scan
        p_app_bis_selfscan_env->scan_end_to_ms = curr_time + scan_timeout_10ms * 10;
    }

    if (APP_BIS_SELFSCAN_GET_SCAN_STATE() != 0)
    {
        return BT_STS_SUCCESS;
    }

    // Clear cache
    app_bis_selfscan_cleanup_scan_result_list();

#if (IBRT)
    uint32_t select_chn_bf = bts_tws_if_is_nv_master() ?
                             (BES_BLE_LOC_FRONT_RIGHT | BES_BLE_LOC_SIDE_RIGHT) : \
                             (BES_BLE_LOC_FRONT_LEFT | BES_BLE_LOC_SIDE_LEFT);
#else
    uint32_t select_chn_bf = BES_BLE_LOC_FRONT_RIGHT | BES_BLE_LOC_FRONT_LEFT;
#endif

    bes_ble_bis_sink_start_param_t param =
    {
        .ch_bf = select_chn_bf,
        .bc_id = empty_bcast_id,
        .bc_code = p_app_bis_selfscan_env->select_src.bcast_code,
        .event_callback =
        {
            .bis_sink_scan_state_cb    = app_bis_selfscan_scan_state_callback,
            .bis_sink_select_source    = app_bis_selfscan_source_report_callback,
            .bis_sink_started_callback = app_bis_selfscan_sink_started_handler,
            .bis_sink_stoped_callback  = app_bis_selfscan_sink_stopped_handler,
            .bis_sink_metadata_cb      = app_bis_selfscan_sink_receive_md_callback,
        },
    };

    bes_ble_bis_sink_start(&param);

    APP_BIS_SELFSCAN_SET_SCAN_STATE(true);
    app_bis_selfscan_inform_upper_scan_state(true);

    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_stop_scan(uint8_t user_bf)
{
    if (bt_defer_curr_func_1(app_bis_selfscan_stop_scan, bt_fixed_param(user_bf)))
    {
        return BT_STS_PENDING;
    }

    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_NOT_READY;
    }

    p_app_bis_selfscan_env->scan_user_bf &= ~user_bf;

    if (p_app_bis_selfscan_env->scan_user_bf != 0)
    {
        BTAPP_TRACE(1, "scan user bf %x", p_app_bis_selfscan_env->scan_user_bf);
        return BT_STS_BUSY;
    }

    if (APP_BIS_SELFSCAN_GET_SCAN_STATE() == 0)
    {
        return BT_STS_SUCCESS;
    }

    osTimerStop(p_app_bis_selfscan_env->scan_timer_id);
    p_app_bis_selfscan_env->scan_end_to_ms = 0;

    bes_ble_bis_stop_scan();

    APP_BIS_SELFSCAN_SET_SCAN_STATE(false);
    app_bis_selfscan_inform_upper_scan_state(false);

    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_start_sync_impl(ble_bdaddr_t *p_addr, uint8_t adv_sid, bool is_enc, const uint8_t *p_bcast_code)
{
    p_app_bis_selfscan_env->select_src.src_addr = *p_addr;
    p_app_bis_selfscan_env->select_src.adv_sid = adv_sid;
    p_app_bis_selfscan_env->select_src.encrypted = is_enc;
    p_app_bis_selfscan_env->select_src.err_code = 0;

    if (p_bcast_code != NULL)
    {
        memcpy(p_app_bis_selfscan_env->select_src.bcast_code, p_bcast_code, GAP_KEY_LEN);
        bes_ble_bis_sink_set_src_id_key(NULL, p_app_bis_selfscan_env->select_src.bcast_code);
    }

    app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_SYNC,
                                p_app_bis_selfscan_env->sync_timeout_s * 100 +
                                BISSELFSCAN_SYNC_SCAN_OFFSET_10MS);

    bes_ble_bis_scan_pa_sync_with_to((ble_bdaddr_t *)p_addr, adv_sid, p_app_bis_selfscan_env->sync_timeout_s);

    APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNCING);

    app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_RUNNING, 0);

    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_start_sync(uint8_t select_src_lid, const uint8_t *p_bcast_code)
{
    if (bt_defer_curr_func_2(app_bis_selfscan_start_sync,
                             bt_fixed_param(select_src_lid),
                             bt_alloc_param_size(p_bcast_code, GAP_KEY_LEN)))
    {
        return BT_STS_PENDING;
    }

    struct app_bis_selfscan_src *p_src_to_sync = NULL;

    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_NOT_READY;
    }

    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() != 0)
    {
        BTAPP_TRACE(1, "sync state %x", APP_BIS_SELFSCAN_GET_SYNC_STATE());
        return BT_STS_BUSY;
    }

    BTAPP_TRACE(1, "select src_lid %d", select_src_lid);

    p_src_to_sync = app_bis_selfscan_get_scan_result_by_src_lid(select_src_lid);

    if (p_src_to_sync == NULL)
    {
        return BT_STS_NOT_FOUND;
    }

    BTAPP_TRACE(1, "adv sid %d enc %d", p_src_to_sync->scan_result.adv_sid,
          p_src_to_sync->scan_result.encrypted);
    BTAPP_TRACE(1, "bcastid %d %d %d", p_src_to_sync->scan_result.broadcast_id[0],
          p_src_to_sync->scan_result.broadcast_id[1],
          p_src_to_sync->scan_result.broadcast_id[2]);

    p_app_bis_selfscan_env->select_src.src_addr = p_src_to_sync->scan_result.src_addr;
    p_app_bis_selfscan_env->select_src.adv_sid = p_src_to_sync->scan_result.adv_sid;
    p_app_bis_selfscan_env->select_src.encrypted = p_src_to_sync->scan_result.encrypted;

    if (p_bcast_code == NULL && p_src_to_sync->scan_result.encrypted == true)
    {
        p_app_bis_selfscan_env->select_src.err_code = BT_LL_ERR_TERMINATED_MIC_FAILURE;
        app_bis_selfscan_send_src_select_state(true);
        app_bis_selfscan_inform_upper_bcast_code_req(select_src_lid);
        return BT_STS_REQUEST_CENTRAL_ENCRYPT;
    }

    app_bis_selfscan_tws_sync_start_sync_cmd(&p_src_to_sync->scan_result.src_addr,
                                             p_src_to_sync->scan_result.adv_sid,
                                             p_bcast_code);
    app_bis_selfscan_start_sync_impl(&p_src_to_sync->scan_result.src_addr,
                                     p_src_to_sync->scan_result.adv_sid,
                                     p_src_to_sync->scan_result.encrypted,
                                     p_bcast_code);
    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_stop_sync_impl(void)
{
    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() == APP_BIS_SELFSCAN_STATE_SYNCING)
    {
        bes_ble_bis_scan_pa_sync_cancel();
    }
    else if (APP_BIS_SELFSCAN_GET_SYNC_STATE() == APP_BIS_SELFSCAN_STATE_SYNCED)
    {
        bes_ble_bis_sink_stop();
    }

    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_stop_sync(void)
{
    if (bt_defer_curr_func_0(app_bis_selfscan_stop_sync))
    {
        return BT_STS_PENDING;
    }

    if (p_app_bis_selfscan_env == NULL)
    {
        return BT_STS_NOT_READY;
    }

    app_bis_selfscan_tws_sync_stop_sync_cmd();

    return app_bis_selfscan_stop_sync_impl();
}

static int app_bis_selfscan_restart_sync_impl(ble_bdaddr_t *p_addr, uint8_t adv_sid, bool is_enc, const uint8_t *p_bcast_code)
{
    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() == APP_BIS_SELFSCAN_STATE_SYNC_IDLE)
    {
        return app_bis_selfscan_start_sync_impl(p_addr, adv_sid, is_enc, p_bcast_code);
    }
    else if (APP_BIS_SELFSCAN_GET_SYNC_STATE() == APP_BIS_SELFSCAN_STATE_SYNCING)
    {
        bes_ble_bis_scan_pa_sync_cancel();
    }
    else
    {
        bes_ble_bis_sink_stop();
    }

    /* stash select_src */
    p_app_bis_selfscan_env->select_src.src_addr = *p_addr;
    p_app_bis_selfscan_env->select_src.adv_sid = adv_sid;
    p_app_bis_selfscan_env->select_src.encrypted = is_enc;
    if (p_bcast_code != NULL)
    {
        memcpy(p_app_bis_selfscan_env->select_src.bcast_code, p_bcast_code, GAP_KEY_LEN);
    }
    p_app_bis_selfscan_env->need_resume_sync = true;

    return BT_STS_SUCCESS;
}

static int app_bis_selfscan_set_bcast_code(const uint8_t *bcast_code)
{
    if (bt_defer_curr_func_1(app_bis_selfscan_set_bcast_code,
                             bt_alloc_param_size(bcast_code, GAP_KEY_LEN)))
    {
        return BT_STS_PENDING;
    }

    if (bcast_code == NULL)
    {
        return BT_STS_INVALID_PARM;
    }

    // Restart sync
    if (p_app_bis_selfscan_env->select_src.err_code != BT_LL_ERR_TERMINATED_MIC_FAILURE)
    {
        return BT_STS_NOT_ALLOW;
    }

    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() == APP_BIS_SELFSCAN_STATE_SYNC_IDLE)
    {
        memcpy(p_app_bis_selfscan_env->select_src.bcast_code, bcast_code, GAP_KEY_LEN);
    }
    else
    {
        BTAPP_TRACE(1, "Invalid sync state to set bcast code");
        return BT_STS_NOT_ALLOW;
    }

    // Restart sync
    //if (p_app_bis_selfscan_env->select_src.err_code == BT_LL_ERR_TERMINATED_MIC_FAILURE)
    {
        app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_SYNC,
                                    p_app_bis_selfscan_env->sync_timeout_s * 100 +
                                    BISSELFSCAN_SYNC_SCAN_OFFSET_10MS);

        bes_ble_bis_sink_set_src_id_key(NULL, p_app_bis_selfscan_env->select_src.bcast_code);

        bes_ble_bis_scan_pa_sync_with_to(&p_app_bis_selfscan_env->select_src.src_addr,
                                         p_app_bis_selfscan_env->select_src.adv_sid,
                                         p_app_bis_selfscan_env->sync_timeout_s);

        APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNCING);

        app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_RUNNING, 0);
    }

    return BT_STS_SUCCESS;
}

#if (IBRT)
static int app_bis_slefscan_tws_set_sync_time_to_play(void)
{
    bt_ui_role_t tws_ui_role = bts_core_get_ui_role();

    if (p_app_bis_selfscan_env->tws_peer_sync_estb == false)
    {
        return BT_STS_NOT_READY;
    }

    if (tws_ui_role == BT_IBRT_MASTER)
    {
        if (app_bt_sync_enable(APP_BT_SYNC_OP_BIS_SELFSCAN, 0, NULL, 0))
        {
            return BT_STS_SUCCESS;
        }
    }

    return BT_STS_FAILED;
}
#endif /* IBRT */

static void app_bis_selfscan_tws_past_req_recv(void)
{
    BTAPP_TRACE(0, "%s pa_lid = %d", __func__, p_app_bis_selfscan_env->select_src.pa_lid);

    uint8_t pa_lid = p_app_bis_selfscan_env->select_src.pa_lid;

    if (pa_lid == APP_BIS_SELFSCAN_INVALID_SRC_LID)
    {
        return;
    }

    app_bis_selfscan_start_tws_past(bes_ble_bis_sink_get_pa_sync_hdl_by_pa_lid(pa_lid));
}

static void app_bis_selfscan_tws_sync_state_recv(ble_bdaddr_t *p_addr, uint8_t adv_sid, const uint8_t *p_bcast_code)
{
    p_app_bis_selfscan_env->tws_peer_sync_estb = false;

    if (p_addr != NULL)
    {
        p_app_bis_selfscan_env->tws_peer_sync_estb = true;

#if (APP_BIS_SELFSCAN_SCAN_USER_TWS)
        p_app_bis_selfscan_env->select_src.src_addr = *p_addr;
        p_app_bis_selfscan_env->select_src.adv_sid = adv_sid;
        p_app_bis_selfscan_env->select_src.encrypted = (p_bcast_code != NULL);
        // Bcast code
        if (p_bcast_code != NULL)
        {
            memcpy(p_app_bis_selfscan_env->select_src.bcast_code, p_bcast_code, GAP_KEY_LEN);
        }
        // Set callback avoid init with no callback past
        app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_TWS, 1);
#endif /* APP_BIS_SELFSCAN_SCAN_USER_TWS */
    }

    BTAPP_TRACE(0, "%s sync estb = %d", __func__, p_app_bis_selfscan_env->tws_peer_sync_estb);

#if (IBRT)
    bt_ui_role_t tws_ui_role = bts_core_get_ui_role();

    if (tws_ui_role != BT_IBRT_MASTER)
    {
        BTAPP_TRACE(0, "%s curr role is not master", __func__);
    }

    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() != APP_BIS_SELFSCAN_STATE_SYNCED)
    {
        BTAPP_TRACE(0, "%s local sync is not estb = %d", __func__, APP_BIS_SELFSCAN_GET_SYNC_STATE());
#if (APP_BIS_SELFSCAN_SCAN_USER_TWS == 0)
        // Start sync directly
        if (p_addr != NULL)
        {
            app_bis_selfscan_restart_sync_impl(p_addr, adv_sid, (p_bcast_code != NULL), p_bcast_code);
        }
#endif /* APP_BIS_SELFSCAN_SCAN_USER_TWS */
        return;
    }

    app_bis_slefscan_tws_set_sync_time_to_play();
#endif /* IBRT */
}

void app_bis_selfscan_tws_sync_info_recv_handler(uint8_t *p_info, uint16_t len)
{
    if (p_app_bis_selfscan_env == NULL)
    {
        return;
    }

    BTAPP_TRACE(1, "op %x", p_info[0]);

    switch (p_info[0])
    {
        case APP_BIS_SELFSCAN_TWS_OP_START_SCAN:
        {
            app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_APP, 0);
        }
        break;
        case APP_BIS_SELFSCAN_TWS_OP_STOP_SCAN:
        {
            app_bis_selfscan_stop_scan(APP_BIS_SELFSCAN_SCAN_USER_APP);
        }
        break;
        case APP_BIS_SELFSCAN_TWS_OP_START_SYNC:
        {
            // Skip opcode
            p_info++;
            ble_bdaddr_t *p_addr = (ble_bdaddr_t *)p_info;
            p_info += sizeof(ble_bdaddr_t);
            uint8_t adv_sid = p_info[0];
            p_info += sizeof(uint8_t);
            bool is_enc = p_info[0];
            p_info += sizeof(uint8_t);
            uint8_t *bcast_code = p_info;
            app_bis_selfscan_restart_sync_impl(p_addr, adv_sid, is_enc, bcast_code);
        }
        break;
        case APP_BIS_SELFSCAN_TWS_OP_STOP_SYNC:
        {
            app_bis_selfscan_stop_sync_impl();
        }
        break;
        case APP_BIS_SELFSCAN_TWS_OP_SYNC_STATE:
        {
            // Skip op
            p_info++;
            if (*p_info == false)
            {
                app_bis_selfscan_tws_sync_state_recv(NULL, 0, NULL);
                break;
            }
            // Skip sync state
            p_info++;
            ble_bdaddr_t *p_addr = (ble_bdaddr_t *)p_info;
            p_info += sizeof(ble_bdaddr_t);
            uint8_t adv_sid = p_info[0];
            p_info += sizeof(uint8_t);
            bool is_enc = p_info[0];
            p_info += sizeof(uint8_t);
            uint8_t *bcast_code = is_enc ? p_info : NULL;
            app_bis_selfscan_tws_sync_state_recv(p_addr, adv_sid, bcast_code);
        }
        break;
        case APP_BIS_SELFSCAN_TWS_OP_PAST_REQ:
        {
            app_bis_selfscan_tws_past_req_recv();
        }
        break;
    }
}

POSSIBLY_UNUSED static void app_bis_selfscan_resume_latest_sync(uint8_t device_id, uint32_t param)
{
    BTAPP_TRACE(1, "Resume BIS selfscan:%d %d %d %d", device_id, param,
          APP_BIS_SELFSCAN_GET_SYNC_STATE(),
          p_app_bis_selfscan_env->select_src.err_code);

    if (APP_BIS_SELFSCAN_GET_SYNC_STATE() != APP_BIS_SELFSCAN_STATE_SYNC_IDLE)
    {
        BTAPP_TRACE(1, "Invalid state to resume sync, %x, %d", APP_BIS_SELFSCAN_GET_SYNC_STATE(),
              p_app_bis_selfscan_env->select_src.err_code);
        return;
    }

    app_bis_selfscan_start_scan(APP_BIS_SELFSCAN_SCAN_USER_SYNC,
                                p_app_bis_selfscan_env->sync_timeout_s * 100 +
                                BISSELFSCAN_SYNC_SCAN_OFFSET_10MS);

    bes_ble_bis_sink_set_src_id_key(NULL, p_app_bis_selfscan_env->select_src.bcast_code);

    bes_ble_bis_scan_pa_sync_with_to(&p_app_bis_selfscan_env->select_src.src_addr,
                                     p_app_bis_selfscan_env->select_src.adv_sid,
                                     p_app_bis_selfscan_env->sync_timeout_s);

    APP_BIS_SELFSCAN_SET_SYNC_STATE(APP_BIS_SELFSCAN_STATE_SYNCING);

    app_bis_selfscan_inform_upper_sync_state(APP_BIS_SELFSCAN_SYNC_RUNNING, 0);
}

static void app_bis_selfscan_scan_timer_handler(void const *param)
{
    if (p_app_bis_selfscan_env == NULL)
    {
        return;
    }

    BTAPP_TRACE(1, "%s", __func__);

    app_bis_selfscan_stop_scan(APP_BIS_SELFSCAN_SCAN_USER_APP |
                               APP_BIS_SELFSCAN_SCAN_USER_SYNC |
                               APP_BIS_SELFSCAN_SCAN_USER_TWS);
}

static void app_bis_selfscan_play_timer_handler(void const *param)
{
    if (p_app_bis_selfscan_env == NULL)
    {
        return;
    }

    BTAPP_TRACE(1, "%s", __func__);

    gaf_bis_audio_stream_set_stream_volume(TGT_VOLUME_LEVEL_7);
}

static void app_bis_selfscan_cleanup_scan_result_list(void)
{
    // Scan result list clear
    struct list_node *p, *n;
    struct list_node *p_result_list = &p_app_bis_selfscan_env->scan_result_list;

    // Check all records in this list
    colist_iterate_safe(p, n, p_result_list)
    {
        colist_delete(p);
        bes_bt_buf_free((uint8_t *)p);
    }

    p_app_bis_selfscan_env->scan_result_size = 0;
}

static uint16_t app_bis_selfscan_store_scan_result_into_list(const struct app_bis_src_scan_result *scan_result)
{
    struct app_bis_selfscan_src *src_node = NULL;
    struct list_node *p_result_list = &p_app_bis_selfscan_env->scan_result_list;
    uint16_t size = (sizeof(*src_node) + scan_result->broadcast_name_len);

    if (app_bis_selfscan_get_scan_result_by_addr_info(&scan_result->src_addr, scan_result->adv_sid))
    {
        return BT_STS_ALREADY_EXIST;
    }

    src_node = (struct app_bis_selfscan_src *)bes_bt_buf_malloc(size);

    if (src_node == NULL)
    {
        return BT_STS_NO_RESOURCES;
    }

    memcpy(&src_node->scan_result, scan_result, sizeof(*scan_result) + scan_result->broadcast_name_len);

    if (p_app_bis_selfscan_env->scan_result_size >= p_app_bis_selfscan_env->max_result_present)
    {
#if 0
        src_node_tail = colist_structure(p_result_list->prev, struct app_bis_selfscan_src, node_head);
        colist_delete(p_result_list->prev);
        bes_bt_buf_free(src_node_tail);
        p_app_bis_selfscan_env->scan_result_size--;
#else
        return BT_STS_QUEUE_FULL;
#endif
    }

    colist_addto_head(&(src_node->node_head), p_result_list);

    p_app_bis_selfscan_env->scan_result_size++;

    BTAPP_TRACE(1, "now list size:%d max:%d", p_app_bis_selfscan_env->scan_result_size, p_app_bis_selfscan_env->max_result_present);

    return BT_STS_SUCCESS;
}

static struct app_bis_selfscan_src *app_bis_selfscan_get_scan_result_by_src_lid(uint8_t src_lid_input)
{
    uint8_t src_id = 0;
    struct list_node *p, *n;
    struct list_node *p_result_list = &p_app_bis_selfscan_env->scan_result_list;
    struct app_bis_selfscan_src *src_node = NULL;

    // Check all records in this list
    colist_iterate_safe(p, n, p_result_list)
    {
        if (src_id == src_lid_input)
        {
            src_node = colist_structure(p, struct app_bis_selfscan_src, node_head);
            break;
        }

        src_id++;
    }

    return src_node;
}

static struct app_bis_selfscan_src *app_bis_selfscan_get_scan_result_by_addr_info(const ble_bdaddr_t *p_src_addr, uint8_t adv_sid)
{
    struct list_node *p, *n;
    struct list_node *p_result_list = &p_app_bis_selfscan_env->scan_result_list;
    struct app_bis_selfscan_src *src_node = NULL;
    struct app_bis_src_scan_result *p_result = NULL;

    if (p_src_addr == NULL)
    {
        return NULL;
    }

    // Check all records in this list
    colist_iterate_safe(p, n, p_result_list)
    {
        src_node = colist_structure(p, struct app_bis_selfscan_src, node_head);
        p_result = &src_node->scan_result;

        if (p_result->adv_sid != adv_sid ||
                memcmp(p_src_addr, &p_result->src_addr, sizeof(p_result->src_addr)) != 0)
        {
            continue;
        }

        return src_node;
    }

    return NULL;
}

#endif /* BIS_SELFSCAN_ENABLED */