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
 * Old Primary side: handle ACK (peer sends ROLE_ACTIVE back)
 *
 * Stop algo, get snapshot, send snapshot, become STANDBY
 **************************************************************************************************/
static void sndp_sleep_role_handle_ack(void)
{
    Algorithm_Snapshot_t snapshot;
    struct Snapshot lib_snapshot;

    ROLE_TRACE(0, "ACK received from peer, migrating algo state...");

    /* Step 1: Stop algo and sensors on old Primary */
    sndp_sleep_analysis_stop();
    sndp_hr_mearsuring_stop();

    /* Step 2: Get algorithm snapshot */
#if defined(__SNDP_HR_ALGO__)
    dbbeats_get_snapshot(&lib_snapshot);

    snapshot.size = lib_snapshot.size;
    if (snapshot.size > SNDP_SLEEP_SNAPSHOT_DATA_SIZE) {
        snapshot.size = SNDP_SLEEP_SNAPSHOT_DATA_SIZE;
    }
    memcpy(snapshot.data, lib_snapshot.data, snapshot.size);
#else
    memset(&snapshot, 0, sizeof(snapshot));
#endif

    /* Step 3: Send snapshot to new Primary */
#if defined(__SNDP_COMM_MGR__)
    uint8_t send_buf[sizeof(Algorithm_Snapshot_t) + 2];
    send_buf[0] = (uint8_t)(snapshot.size & 0xFF);
    send_buf[1] = (uint8_t)((snapshot.size >> 8) & 0xFF);
    memcpy(&send_buf[2], snapshot.data, snapshot.size);

    sndp_comm_cmd_send_lr_sync_sleep_snapshot(send_buf, snapshot.size + 2);
    ROLE_TRACE(0, "Snapshot sent, size=%d", snapshot.size);
#endif

    /* Step 4: Become STANDBY (Secondary: log only) */
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
        /* Peer became Primary (ACK) -> I should stop algo, send snapshot, become STANDBY */
        if (role_ctx.role == ROLE_SWITCHING) {
            sndp_sleep_role_handle_ack();
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
 * New Primary side: receive snapshot from old Primary
 *
 * Open sensors, restore snapshot, initialize algo, become ROLE_ACTIVE
 **************************************************************************************************/
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot_data, uint16_t len)
{
    Algorithm_Snapshot_t snapshot;
    struct Snapshot lib_snapshot;

    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    if (snapshot_data == NULL || len < 2) {
        ROLE_TRACE(0, "Invalid snapshot data, rtn");
        return;
    }

    /* Parse: first 2 bytes = size, rest = data */
    snapshot.size = (uint16_t)snapshot_data[0] | ((uint16_t)snapshot_data[1] << 8);
    if (snapshot.size > SNDP_SLEEP_SNAPSHOT_DATA_SIZE) {
        snapshot.size = SNDP_SLEEP_SNAPSHOT_DATA_SIZE;
    }

    uint16_t data_offset = 2;
    uint16_t copy_len = (len > data_offset) ? (len - data_offset) : 0;
    if (copy_len > snapshot.size) {
        copy_len = snapshot.size;
    }
    memset(snapshot.data, 0, sizeof(snapshot.data));
    memcpy(snapshot.data, &snapshot_data[data_offset], copy_len);

    ROLE_TRACE(0, "Snapshot received, size=%d, copy_len=%d", snapshot.size, copy_len);

    /* Must be in SWITCHING state (sent ACK, waiting for snapshot) */
    if (role_ctx.role != ROLE_SWITCHING) {
        ROLE_TRACE(0, "Not in SWITCHING state (role=%d), rtn", role_ctx.role);
        return;
    }

    /* Step 1: Start sensors and algo */
    sndp_hr_mearsuring_start(sndp_hr_mearsuring_get_sampling_rate(), sndp_hr_mearsuring_get_dump_state(HR_DUMP_STATE));
    sndp_sleep_analysis_start(sndp_hr_get_sleep_control());

    /* Step 2: Restore algorithm state from snapshot */
#if defined(__SNDP_HR_ALGO__)
    if (snapshot.size > 0) {
        lib_snapshot.size = snapshot.size;
        if (lib_snapshot.size > SNAPSHOT_DATA_MAX_SIZE) {
            lib_snapshot.size = SNAPSHOT_DATA_MAX_SIZE;
        }
        memcpy(lib_snapshot.data, snapshot.data, lib_snapshot.size);
        dbbeats_set_snapshot(&lib_snapshot);
        ROLE_TRACE(0, "Snapshot restored, size=%d", lib_snapshot.size);
    }
#endif

    /* Step 3: Re-initialize algo (per flowchart: initialize_heartrate_data + initialize_sleep_data) */
    /* Note: sndp_hr_mearsuring_start and sndp_sleep_analysis_start above already call
     * dbbeats_initialize_heartrate_data and dbbeats_initialize_sleep_data internally.
     * The flowchart shows these as explicit steps, but they are encapsulated in those functions. */

    /* Step 4: Become ROLE_ACTIVE (new Primary) */
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
