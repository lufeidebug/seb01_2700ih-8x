#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)

#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_sleep_role_switch.h"
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
    Role_Switch_Reason_t pending_reason;    /* Pending switch reason */
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
    return (role_ctx.role == ROLE_ACTIVE || role_ctx.role == ROLE_SWITCHING);
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
 * Internal: Suspend / Resume helpers
 **************************************************************************************************/
static void sndp_sleep_role_suspend(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)) {
        sndp_hr_suspend(0x00);
        ROLE_TRACE(0, "suspend heart rate");
    }

    if(sndp_dev_sleep_app_get_stage_onoff(false)) {
        sndp_sleep_analysis_suspend();
        ROLE_TRACE(0, "suspend sleep analysis");
    }
#endif
}

static void sndp_sleep_role_resume(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)) {
        sndp_hr_resume(0x00);
        ROLE_TRACE(0, "resume heart rate");
    }

    if(sndp_dev_sleep_app_get_stage_onoff(false)) {
        sndp_sleep_analysis_resume();
        ROLE_TRACE(0, "resume sleep analysis");
    }
#endif
}

/**************************************************************************************************
 * Start / Stop role management
 **************************************************************************************************/
void sndp_sleep_role_start(void)
{
    ROLE_TRACE(0, "starting...");
    role_ctx.role = ROLE_ACTIVE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Active");
}

void sndp_sleep_role_stop(void)
{
    ROLE_TRACE(0, "stopping...");
    role_ctx.role = ROLE_IDLE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Stopped");
}

/**************************************************************************************************
 * Role Switch Protocol (reuses LR_SYNC_SLEEP_ROLE_STATUS + LR_SYNC_SLEEP_SNAPSHOT):
 *
 *   REQUEST:  Old Primary sends ROLE_SWITCHING to peer
 *   ACK:      New Primary sends ROLE_ACTIVE back (peer is worn, taking over)
 *   NACK:     Peer sends ROLE_IDLE back (peer also removed, cannot take over)
 *   SNAPSHOT: Old Primary sends algorithm snapshot via LR_SYNC_SLEEP_SNAPSHOT
 *
 * Flow:
 *   L (old Primary) detects trigger -> sends ROLE_SWITCHING (= REQUEST)
 *   R receives ROLE_SWITCHING:
 *     - If R worn  -> R sends ROLE_ACTIVE (= ACK), waits for snapshot
 *     - If R removed -> R sends ROLE_IDLE (= NACK), both suspend
 *   L receives ACK (ROLE_ACTIVE):
 *     -> L stops algo, gets snapshot, sends snapshot, becomes STANDBY
 *   L receives NACK (ROLE_IDLE):
 *     -> L suspends, R already suspended
 *   R receives snapshot:
 *     -> R opens sensors, set_snapshot, initialize_heartrate_data,
 *        initialize_sleep_data, becomes ROLE_ACTIVE
 **************************************************************************************************/

/**************************************************************************************************
 * Old Primary side: send REQUEST to peer
 **************************************************************************************************/
static void sndp_sleep_role_send_request(Role_Switch_Reason_t reason)
{
    ROLE_TRACE(0, "sending REQUEST (ROLE_SWITCHING), reason=%d", reason);

    role_ctx.role = ROLE_SWITCHING;
    role_ctx.pending_reason = reason;

    /* Send ROLE_SWITCHING to peer = REQUEST to switch */
    sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_SWITCHING);
}

/**************************************************************************************************
 * New Primary side: handle incoming REQUEST from old Primary
 * Called when peer sends ROLE_SWITCHING.
 *
 * If this ear is worn -> send ACK (ROLE_ACTIVE), wait for snapshot
 * If this ear is also removed -> send NACK (ROLE_IDLE), suspend both
 **************************************************************************************************/
void sndp_sleep_role_switch_on_request(void)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    ROLE_TRACE(0, "REQUEST received, current role=%d", role_ctx.role);

    /* If already Active, reject (prevent dual-Primary conflict) */
    if (role_ctx.role == ROLE_ACTIVE) {
        ROLE_TRACE(0, "Already Active, rejecting takeover");
        return;
    }

    /* Check if this ear is worn */
    if (sndp_dev_wear_is_worn(false)) {
        /* ACK: I can take over. Send ROLE_ACTIVE to old Primary */
        role_ctx.role = ROLE_SWITCHING;
        ROLE_TRACE(0, "ACK: worn, taking over, sending ROLE_ACTIVE");
        sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_ACTIVE);
        /* Wait for snapshot from old Primary */
    } else {
        /* NACK: I'm also removed, cannot take over */
        ROLE_TRACE(0, "NACK: also removed, sending ROLE_IDLE");
        sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_IDLE);

        /* Suspend this ear */
        sndp_sleep_role_suspend();
    }
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
 * Called while role == SWITCHING, so data-plane gate allows the sensor close ops.
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
    /* proximity 不随角色切换关闭: STANDBY耳也需要周期读+TWS转发给Primary,
       tick 保持运行, 仅"上报app"由 task5 内 is_active 门控.
       (sleep_comm_execute_cmd_hdlr 对 GET_PROXIMITY_NOTIFICATION 放行,
        Secondary 也能通过 app 命令开/关 tick) */
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
 * Called while role == SWITCHING, so data-plane gate allows sensor open ops.
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
 * Old Primary side: handle ACK (peer sends ROLE_ACTIVE back)
 *
 * Stop all streams, build migration payload (5 streams + algo snapshot), send it,
 * then become STANDBY.
 **************************************************************************************************/
static void sndp_sleep_role_handle_ack(void)
{
    ROLE_TRACE(0, "ACK received from peer, migrating streams...");

    /* Step 1: Build migration payload FIRST, while streams/flags/algo are still live
     * (stop_all_streams below clears hr_running/dump_state/proximity_onoff/stage_onoff
     *  and the algo state, so they must be captured before stopping). */
#if defined(__SNDP_COMM_MGR__)
    uint8_t send_buf[ROLE_MIG_BUF_SIZE];
    uint16_t send_len = 0;
    sndp_sleep_role_build_migration(send_buf, &send_len);
#endif

    /* Step 2: Stop all running streams on old Primary (role still SWITCHING => ops execute) */
    sndp_sleep_role_stop_all_streams();

    /* Step 3: Send migration payload to new Primary */
#if defined(__SNDP_COMM_MGR__)
    sndp_comm_cmd_send_lr_sync_sleep_snapshot(send_buf, send_len);
    ROLE_TRACE(0, "migration sent len=%d", send_len);
#endif

    /* Step 4: Become STANDBY (Secondary: sensors closed, log only) */
    role_ctx.role = ROLE_STANDBY;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_STANDBY);
    ROLE_TRACE(0, "Switched to STANDBY");
}

/**************************************************************************************************
 * Old Primary side: handle NACK (peer sends ROLE_IDLE back)
 *
 * Both ears suspend
 **************************************************************************************************/
static void sndp_sleep_role_handle_nack(void)
{
    ROLE_TRACE(0, "NACK received: peer also removed, suspending both");

    /* Suspend this ear (peer already suspended itself) */
    sndp_sleep_role_suspend();

    /* Notify App: both ears removed, monitoring suspended */
    /* TODO: send suspend notification to App */
}

/**************************************************************************************************
 * Peer role status receiver: called by comm_cmd when LR_SYNC_SLEEP_ROLE_STATUS arrives.
 *
 * Reuses the role value to implement REQUEST/ACK/NACK:
 *   ROLE_SWITCHING received -> peer wants to switch (REQUEST) -> call on_request
 *   ROLE_ACTIVE received     -> peer has become Primary (ACK)    -> migrate snapshot, become STANDBY
 *   ROLE_IDLE received       -> peer can't take over (NACK)      -> suspend both
 *   ROLE_STANDBY received    -> peer became Secondary (snapshot sent) -> just update peer state
 **************************************************************************************************/
void sndp_sleep_role_switch_on_peer_status(Device_Role_t peer_role)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    role_ctx.peer_role = peer_role;
    ROLE_TRACE(0, "peer status=%d, my role=%d", peer_role, role_ctx.role);

    switch (peer_role) {
    case ROLE_SWITCHING:
        /* Peer (old Primary) wants to switch -> I'm being asked to take over */
        sndp_sleep_role_switch_on_request();
        break;

    case ROLE_ACTIVE:
        if (role_ctx.role == ROLE_SWITCHING) {
            /* ACK during a switch -> old Primary migrates streams to peer */
            sndp_sleep_role_handle_ack();
        } else if (role_ctx.role == ROLE_IDLE) {
            /* Initial assignment: peer became Primary on mobile connect -> I'm Secondary */
            role_ctx.role = ROLE_STANDBY;
            ROLE_TRACE(0, "initial: peer ACTIVE, I become STANDBY");
        }
        break;

    case ROLE_IDLE:
        /* Peer can't take over (NACK) -> suspend both */
        if (role_ctx.role == ROLE_SWITCHING) {
            sndp_sleep_role_handle_nack();
        }
        break;

    case ROLE_STANDBY:
        /* Peer became Secondary -> just update state, no action */
        ROLE_TRACE(0, "peer is now STANDBY");
        break;

    default:
        break;
    }
}

/**************************************************************************************************
 * New Primary side: receive migration snapshot from old Primary
 *
 * Restore all streams (HR/sleep/PPG/ACC/proximity uploads) + algo snapshot, then
 * become ROLE_ACTIVE. Called while role == SWITCHING, so data-plane gate allows
 * the sensor open ops.
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

    /* Must be in SWITCHING state (sent ACK, waiting for snapshot) */
    if (role_ctx.role != ROLE_SWITCHING) {
        ROLE_TRACE(0, "Not in SWITCHING state (role=%d), rtn", role_ctx.role);
        return;
    }

    /* Restore streams + algo snapshot (role still SWITCHING => start ops execute) */
    sndp_sleep_role_restore_streams(snapshot_data, len);

    /* Become ROLE_ACTIVE (new Primary) */
    role_ctx.role = ROLE_ACTIVE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(ROLE_ACTIVE);
    ROLE_TRACE(0, "Switched to ACTIVE (new Primary)");
}

/**************************************************************************************************
 * Conflict handler (both ears think they're Primary)
 **************************************************************************************************/
void sndp_sleep_role_switch_handle_conflict(void)
{
    ROLE_TRACE(0, "Handling conflict");
    /* TODO: implement conflict resolution (e.g., battery-based tiebreaker) */
}

/**************************************************************************************************
 * Public API: trigger role switch
 *
 * Called when:
 *   1) Primary ear removed while Secondary still worn
 *   2) Primary battery < Secondary battery - 5%
 **************************************************************************************************/
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    /* If no TWS link or peer not worn, just suspend/resume locally */
    if (!sndp_is_tws_link_connected() || !sndp_dev_wear_is_worn(true)) {
        ROLE_TRACE(0, "TWS link %d, peer worn %d rs:%d", sndp_is_tws_link_connected(), sndp_dev_wear_is_worn(true), reason);
        if(ROLE_SWITCH_REASON_WEAR_OFF == reason){
            sndp_sleep_role_suspend();
        }else if(ROLE_SWITCH_REASON_WEAR_ON == reason){
            sndp_sleep_role_resume();
        }
        return;
    }

    /* If peer is already Active, no need to switch */
    if(sndp_dev_wear_is_worn(true) && sndp_sleep_role_is_peer_active()) {
        ROLE_TRACE(0, "Peer is active, rtn");
        return;
    }

    ROLE_TRACE(0, "Triggered by reason=%d", reason);

    /* Send REQUEST to peer (old Primary -> new Primary) */
    if(ROLE_SWITCH_REASON_WEAR_ON != reason) {
        if(sndp_dev_sleep_app_get_stage_onoff(false)) {
            if(sndp_sleep_role_is_peer_standby() || sndp_sleep_role_is_peer_idle()) {
                sndp_sleep_role_send_request(reason);
            }
        }
    }
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

#endif /* __SNDP_SLEEP_APP_ROLE_SWITCH__ */
