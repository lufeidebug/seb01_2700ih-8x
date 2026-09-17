#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)

#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_sleep_role_switch.h"
#include "sndp_ui.h"
#include "sndp_heart_rate.h"
#include "sndp_comm_cmd.h"
#include "sndp_sensor_service.h"
#include "sndp_interact_app.h"
#if defined(__SNDP_HR_ALGO__)
#include "sleepsense.h"
#endif
#include "sndp_comm_ble.h"

/**************************************************************************************************
 * Trace
 **************************************************************************************************/
#define ROLE_TRACE(num, str, ...)   SNDP_TRACE(num, "[SLEEP_ROLE_LOG] " str, ##__VA_ARGS__)

/**************************************************************************************************
 * Types & Constants
 **************************************************************************************************/

/**************************************************************************************************
 * Global State
 **************************************************************************************************/

/**************************************************************************************************
 * Role Query API
 **************************************************************************************************/
bool sndp_sleep_tws_is_master_with_ble(void)
{
    if (sndp_comm_ble_get_conn_type() != SNDP_COMM_CONN_TYPE_GATT_OVER_BREDR)
    {
        //on ble connect, always master
        //because ble connect only connect one device
        return true;
    }

    if(sndp_is_tws_master_mode())
    {
        return true;
    }
    return false;
}

/**************************************************************************************************
 * Stream migration payload (reuses LR_SYNC_SLEEP_SNAPSHOT channel)
 *
 * Layout:
 *  [0]      hr_running
 *  [1]      sleep_running (stage onoff)
 *  [2]      sampling_rate
 *  [3]      hr_dump_state  (HR_DUMP_STATE)
 *  [4]      ppg_notif_on   (PPG_DUMP_STATE)
 *  [5]      acc_notif_on   (ACC_DUMP_STATE)
 *  [6]      proximity_on
 *  [7..10]  sleep_control (int32 LE)
 *  [11..12] algo_snap_size (uint16 LE)
 *  [13..]   algo snapshot data
 **************************************************************************************************/
#define ROLE_MIG_HEADER_SIZE     13
#define ROLE_MIG_SNAPSHOT_MAX    256
#define ROLE_MIG_BUF_SIZE        (ROLE_MIG_HEADER_SIZE + ROLE_MIG_SNAPSHOT_MAX)

/**************************************************************************************************
 * Old Primary side: stop all running streams (HR + sleep + PPG/ACC/proximity uploads).
 **************************************************************************************************/
static void sndp_sleep_role_stop_all_streams(void)
{
    /* Raw-data upload streams first (release PPG/ACC notification users) */
    if(sndp_hr_is_ppg_enabled_only_ppg_user()) {
        sndp_sensor_service_set_ppg_notification_local(false);
    }
    if(sndp_hr_is_acc_enabled_only_acc_user()) {
        sndp_sensor_service_set_acc_notification_local(false);
    }
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)) {
        sndp_sleep_comm_cmd_heartrate_stop();
    }

    if(sndp_dev_sleep_app_get_stage_onoff(false)) {
        sndp_sleep_comm_cmd_analysis_stop();
    }
}

/**************************************************************************************************
 * Old Primary side: capture all stream state + algo snapshot into migration buffer.
 **************************************************************************************************/
static void sndp_sleep_role_build_migration(uint8_t *buf, uint16_t *out_len)
{
    uint16_t idx = 0;

    buf[idx++] = sndp_dev_sleep_app_get_stage_onoff(false) ? 1 : 0;
    buf[idx++] = sndp_hr_mearsuring_get_sampling_rate();
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE);
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE);
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE);
    buf[idx++] = sndp_dev_sleep_app_get_heartrate_onoff(false) ? 1 : 0;
    buf[idx++] = sndp_hr_is_ppg_enabled_only_ppg_user();
    buf[idx++] = sndp_hr_is_acc_enabled_only_acc_user();
    buf[idx++] = sndp_dev_sleep_app_get_proximity_onoff(false) ? 1 : 0;

    int32_t sleep_ctrl = sndp_hr_get_sleep_control();
    buf[idx++] = (uint8_t)(sleep_ctrl & 0xFF);
    buf[idx++] = (uint8_t)((sleep_ctrl >> 8) & 0xFF);
    buf[idx++] = (uint8_t)((sleep_ctrl >> 16) & 0xFF);
    buf[idx++] = (uint8_t)((sleep_ctrl >> 24) & 0xFF);

    uint16_t snap_size = 0;
    buf[idx++] = 0; /* snap size low placeholder */
    buf[idx++] = 0; /* snap size high placeholder */

#if defined(__SNDP_HR_ALGO__)
    struct Snapshot lib_snapshot;
    dbbeats_get_snapshot(&lib_snapshot);
    snap_size = lib_snapshot.size;
    if(snap_size > ROLE_MIG_SNAPSHOT_MAX) {
        snap_size = ROLE_MIG_SNAPSHOT_MAX;
    }
    memcpy(&buf[idx], lib_snapshot.data, snap_size);
#endif
    idx += snap_size;

    /* backfill snapshot size at [11..12] */
    buf[11] = (uint8_t)(snap_size & 0xFF);
    buf[12] = (uint8_t)((snap_size >> 8) & 0xFF);

    *out_len = idx;
}

/**************************************************************************************************
 * New Primary side: restore all streams + algo snapshot from migration buffer.
 **************************************************************************************************/
static void sndp_sleep_role_restore_streams(const uint8_t *buf, uint16_t len)
{
    if(len < ROLE_MIG_HEADER_SIZE) {
        ROLE_TRACE(0, "migration too short: %d", len);
        return;
    }

    uint16_t idx = 0;
    uint8_t sleep_run  = buf[idx++];
    uint8_t samp_rate  = buf[idx++];
    uint8_t hr_dump    = buf[idx++];
    POSSIBLY_UNUSED uint8_t ppg_dump   = buf[idx++];
    POSSIBLY_UNUSED uint8_t acc_dump   = buf[idx++];
    uint8_t hr_on      = buf[idx++];
    uint8_t ppg_notif  = buf[idx++];
    uint8_t acc_notif  = buf[idx++];
    uint8_t prox_on    = buf[idx++];
    int32_t sleep_ctrl = (int32_t)buf[idx] | ((int32_t)buf[idx+1] << 8) |
                         ((int32_t)buf[idx+2] << 16) | ((int32_t)buf[idx+3] << 24);
    idx += 4;
    uint16_t snap_size = (uint16_t)buf[idx] | ((uint16_t)buf[idx+1] << 8);
    idx += 2;

    ROLE_TRACE(1, "restore: hr=%d sleep=%d ppgN=%d accN=%d prox=%d",
               hr_on, sleep_run, ppg_notif, acc_notif, prox_on);

    /* 1. HR measurement: opens PPG+ACC with HR users + init HR algo */
    if(hr_on) {
        sndp_sensor_service_start_heartrate_local(samp_rate, hr_dump);
    }
    /* 2. Raw-data upload streams: open PPG/ACC notification users */
    if(ppg_notif) {
        sndp_sensor_service_set_ppg_notification_local(0x01);
    }
    if(acc_notif) {
        sndp_sensor_service_set_acc_notification_local(0x01);
    }
    /* 3. Proximity reporting */
    if(prox_on) {
        sndp_sensor_service_start_proximity_local();
    }
    /* 4. Sleep analysis: init sleep algo */
    if(sleep_run) {
        sndp_sleep_analysis_start(sleep_ctrl);
    }

    /* 5. Restore algorithm state from snapshot */
#if defined(__SNDP_HR_ALGO__)
    if(snap_size > 0 && (uint32_t)(idx + snap_size) <= len) {
        struct Snapshot lib_snapshot;
        lib_snapshot.size = snap_size;
        memcpy(lib_snapshot.data, &buf[idx], snap_size);
        dbbeats_set_snapshot(&lib_snapshot);
        ROLE_TRACE(0, "algo snapshot restored size=%d", snap_size);
    }
#endif
}

/**************************************************************************************************
 * Peer role status receiver: called by comm_cmd when LR_SYNC_SLEEP_ROLE_STATUS arrives.
 * Just update peer role state, no protocol handling needed.
 **************************************************************************************************/

/**************************************************************************************************
 * New Primary side: receive migration snapshot from old Primary
 *
 * Restore all streams (HR/sleep/PPG/ACC/proximity uploads) + algo snapshot, then
 * become ROLE_ACTIVE.
 **************************************************************************************************/
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot_data, uint16_t len)
{

    if (snapshot_data == NULL || len < ROLE_MIG_HEADER_SIZE) {
        ROLE_TRACE(0, "Invalid migration data (len=%d), rtn", len);
        return;
    }
    
    /* Become ROLE_ACTIVE (new Primary) */
    ROLE_TRACE(0, "Migration received, became ACTIVE (new Primary)");

    /* Restore streams + algo snapshot */
    sndp_sleep_role_restore_streams(snapshot_data, len);
}

/**************************************************************************************************
 * Public API: module init
 **************************************************************************************************/

static void sndp_set_role_standby(void)
{
    ROLE_TRACE(0, "Became STANDBY (migrated to peer)");
}

static void sndp_set_role_active(void)
{
    ROLE_TRACE(0, "Became ACTIVE (new Primary)");
}

/**************************************************************************************************
 * Public API: handle TWS role change (master/slave switch)
 *
 * primary/secondary 由 TWS master/slave 决定.
 * - TWS master → primary (ROLE_ACTIVE):
 *   - 从 IDLE 变来: 直接 resume (fresh start, 无需 migration)
 *   - 从 STANDBY 变来: 等待 old primary 发送 migration snapshot,
 *     收到后在 recv_snapshot 中 restore streams 并设为 ACTIVE
 * - TWS slave → secondary (ROLE_STANDBY):
 *   - 从 ACTIVE 变来: build migration → stop all streams → send snapshot → set STANDBY
 **************************************************************************************************/
void sndp_sleep_role_switch_on_bt_role_change(void)
{

    if(!sndp_is_tws_link_connected())
    {
        ROLE_TRACE(0, "Not TWS connected, rtn");
        return;
    }

    bool is_now_tws_master = sndp_is_tws_master_mode();
    ROLE_TRACE(0, "BT role change: TWS master=%d",is_now_tws_master);

    if (is_now_tws_master) {
            sndp_set_role_active();
    } else {
            /* Was primary, becoming secondary.
             * Build migration payload, stop all streams, send to new primary. */
            if(sndp_comm_ble_get_conn_type() == SNDP_COMM_CONN_TYPE_GATT_OVER_BREDR){
                // only GATT_OVER_BREDR need migration payload
                /* Step 1: Build migration payload FIRST, while streams/flags/algo are still live
                * (stop_all_streams below clears hr_running/dump_state/proximity_onoff/stage_onoff
                *  and the algo state, so they must be captured before stopping). */
#if defined(__SNDP_COMM_MGR__)
                uint8_t send_buf[ROLE_MIG_BUF_SIZE];
                uint16_t send_len = 0;
                sndp_sleep_role_build_migration(send_buf, &send_len);
#endif

                /* Step 2: Stop all running streams on old Primary */
                sndp_sleep_role_stop_all_streams();

                /* Step 3: Send migration payload to new Primary */
#if defined(__SNDP_COMM_MGR__)
                sndp_comm_cmd_send_lr_sync_sleep_snapshot(send_buf, send_len);
                ROLE_TRACE(0, "migration sent len=%d", send_len);
#endif
            }
            sndp_set_role_standby();
    }
}

#endif /* __SNDP_SLEEP_APP_ROLE_SWITCH__ */
