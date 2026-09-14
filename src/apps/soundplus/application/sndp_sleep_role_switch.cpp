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

/**************************************************************************************************
 * Trace
 **************************************************************************************************/
#define ROLE_TRACE(num, str, ...)   SNDP_TRACE(num, "[SLEEP_ROLE_LOG] %s, " str, __func__, ##__VA_ARGS__)

/**************************************************************************************************
 * Types & Constants
 **************************************************************************************************/
typedef struct {
    Device_Role_t       role;               /* Current role */
    Device_Role_t       peer_role;          /* Peer role */
    bool                initialized;        /* Initialized flag */
} sndp_sleep_role_ctx_s;

/**************************************************************************************************
 * Global State
 **************************************************************************************************/
static sndp_sleep_role_ctx_s role_ctx;

/**************************************************************************************************
 * Role Query API
 **************************************************************************************************/
Device_Role_t sndp_sleep_role_get_current(void)
{
    return role_ctx.role;
}

bool sndp_sleep_role_is_active(void)
{
    return (role_ctx.role == ROLE_ACTIVE);
}

bool sndp_sleep_role_data_plane_active(void)
{
    return (role_ctx.role == ROLE_ACTIVE);
}

bool sndp_sleep_role_is_standby(void)
{
    return (role_ctx.role == ROLE_STANDBY);
}

bool sndp_sleep_role_is_switching(void)
{
    return (role_ctx.role == ROLE_SWITCHING);
}

Device_Role_t sndp_sleep_role_get_peer(void)
{
    return role_ctx.peer_role;
}

bool sndp_sleep_role_is_peer_active(void)
{
    return (role_ctx.peer_role == ROLE_ACTIVE);
}

bool sndp_sleep_role_is_peer_idle(void)
{
    return (role_ctx.peer_role == ROLE_IDLE);
}

bool sndp_sleep_role_is_peer_standby(void)
{
    return (role_ctx.peer_role == ROLE_STANDBY);
}

void sndp_sleep_role_set_peer(Device_Role_t role)
{
    ROLE_TRACE(0, "peer role set to %d", role);
    role_ctx.peer_role = role;
}

/**************************************************************************************************
 * Start / Stop role management
 **************************************************************************************************/
void sndp_sleep_role_start(void)
{
    ROLE_TRACE(0, "starting...");
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Active");
}

void sndp_sleep_role_stop(void)
{
    ROLE_TRACE(0, "stopping...");
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Stopped");
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
    if(sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE)) {
        sndp_ppg_notification_stop();
    }
    if(sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE)) {
        sndp_acc_notification_stop();
    }
    /* proximity 不随角色切换关闭: 两耳 task 同步跑, 不需要 suspend/resume 转换.
       TWS 主从切换后 primary/secondary 交换, 但 task 还在跑, 仅"上报app"由 is_active 门控.
       (Primary 收到 app cmd 后 TWS 同步 ONOFF 给 Secondary, 两耳都开启/关闭 tick) */
    /* HR measurement + sleep analysis (release HR users, close sensors) */
    sndp_hr_mearsuring_stop();
    sndp_sleep_analysis_stop();
}

/**************************************************************************************************
 * Old Primary side: capture all stream state + algo snapshot into migration buffer.
 **************************************************************************************************/
static void sndp_sleep_role_build_migration(uint8_t *buf, uint16_t *out_len)
{
    uint16_t idx = 0;

    buf[idx++] = sndp_hr_running_state() ? 1 : 0;
    buf[idx++] = sndp_dev_sleep_app_get_stage_onoff(false) ? 1 : 0;
    buf[idx++] = sndp_hr_mearsuring_get_sampling_rate();
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE);
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(PPG_DUMP_STATE);
    buf[idx++] = sndp_hr_mearsuring_get_dump_state(ACC_DUMP_STATE);
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
    uint8_t hr_running = buf[idx++];
    uint8_t sleep_run  = buf[idx++];
    uint8_t samp_rate  = buf[idx++];
    uint8_t hr_dump    = buf[idx++];
    uint8_t ppg_notif  = buf[idx++];
    uint8_t acc_notif  = buf[idx++];
    uint8_t prox_on    = buf[idx++];
    int32_t sleep_ctrl = (int32_t)buf[idx] | ((int32_t)buf[idx+1] << 8) |
                         ((int32_t)buf[idx+2] << 16) | ((int32_t)buf[idx+3] << 24);
    idx += 4;
    uint16_t snap_size = (uint16_t)buf[idx] | ((uint16_t)buf[idx+1] << 8);
    idx += 2;

    ROLE_TRACE(1, "restore: hr=%d sleep=%d ppgN=%d accN=%d prox=%d",
               hr_running, sleep_run, ppg_notif, acc_notif, prox_on);

    /* 1. HR measurement: opens PPG+ACC with HR users + init HR algo */
    if(hr_running) {
        sndp_hr_mearsuring_start((int8_t)samp_rate, hr_dump);
    }
    /* 2. Raw-data upload streams: open PPG/ACC notification users */
    if(ppg_notif) {
        sndp_ppg_notification_start(0x01);
    }
    if(acc_notif) {
        sndp_acc_notification_start(0x01);
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
void sndp_sleep_role_switch_on_peer_status(Device_Role_t peer_role)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    role_ctx.peer_role = peer_role;
    ROLE_TRACE(0, "peer status=%d, my role=%d", peer_role, role_ctx.role);
}

/**************************************************************************************************
 * New Primary side: receive migration snapshot from old Primary
 *
 * Restore all streams (HR/sleep/PPG/ACC/proximity uploads) + algo snapshot, then
 * become ROLE_ACTIVE.
 **************************************************************************************************/
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot_data, uint16_t len)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    if (snapshot_data == NULL || len < ROLE_MIG_HEADER_SIZE) {
        ROLE_TRACE(0, "Invalid migration data (len=%d), rtn", len);
        return;
    }
    
    /* Become ROLE_ACTIVE (new Primary) */
    role_ctx.role = ROLE_ACTIVE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_ACTIVE);
    ROLE_TRACE(0, "Migration received, became ACTIVE (new Primary)");

    /* Restore streams + algo snapshot */
    sndp_sleep_role_restore_streams(snapshot_data, len);
}

/**************************************************************************************************
 * Public API: module init
 **************************************************************************************************/
void sndp_sleep_role_switch_init(void)
{
    ROLE_TRACE(0, "Initializing...");

    memset(&role_ctx, 0, sizeof(role_ctx));

    role_ctx.role = ROLE_IDLE;
    role_ctx.peer_role = ROLE_IDLE;
    role_ctx.initialized = true;
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
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    bool is_now_tws_master = sndp_is_tws_master_mode();
    ROLE_TRACE(0, "BT role change: TWS master=%d, my role=%d, peer role=%d",
               is_now_tws_master, role_ctx.role, role_ctx.peer_role);

    if (is_now_tws_master) {
        /* TWS master → primary (ACTIVE) */
        if (role_ctx.role == ROLE_ACTIVE) {
            ROLE_TRACE(0, "Already ACTIVE, no change");
            return;
        }

        if (role_ctx.role == ROLE_STANDBY) {
            /* Was secondary, old primary will send migration snapshot.
             * restore_streams + set ACTIVE will happen in recv_snapshot. */
            ROLE_TRACE(0, "Was STANDBY, waiting for migration snapshot");
        } else {
            /* Was IDLE (fresh start, e.g. TWS just connected) */
            sndp_sleep_app_resume(SLEEP_APP_OP_USER_ROLE_SWITCH);
            role_ctx.role = ROLE_ACTIVE;
            sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_ACTIVE);
            ROLE_TRACE(0, "Was IDLE, became ACTIVE (fresh start)");
        }
    } else {
        /* TWS slave → secondary (STANDBY) */
        if (role_ctx.role == ROLE_ACTIVE) {
            /* Was primary, becoming secondary.
             * Build migration payload, stop all streams, send to new primary. */

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

            /* Step 4: Become STANDBY (Secondary: sensors closed) */
            role_ctx.role = ROLE_STANDBY;
            sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_STANDBY);
            ROLE_TRACE(0, "Became STANDBY (migrated to peer)");
        } else {
            ROLE_TRACE(0, "Already STANDBY/IDLE, no change");
        }
    }
}

#endif /* __SNDP_SLEEP_APP_ROLE_SWITCH__ */
