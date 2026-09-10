#ifndef __SNDP_SLEEP_ROLE_SWITCH_H__
#define __SNDP_SLEEP_ROLE_SWITCH_H__

#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * Role state enum
 **************************************************************************************************/

typedef enum {
    ROLE_IDLE           = 0,    /* Initial state, role not yet determined */
    ROLE_ACTIVE         = 1,    /* Primary: running PPG/ACC, executing algo, reporting to App */
    ROLE_STANDBY        = 2,    /* Secondary: sensors closed, algo stopped, low power */
    ROLE_SWITCHING      = 3,    /* Switching: critical transitional state */
} Device_Role_t;

/**
 * @brief Algorithm snapshot struct (for TWS transfer)
 */
#define SNDP_SLEEP_SNAPSHOT_DATA_SIZE   213

typedef struct {
    uint16_t size;
    uint8_t  data[SNDP_SLEEP_SNAPSHOT_DATA_SIZE];
} Algorithm_Snapshot_t;

/**
 * @brief Role switch trigger reason
 */
typedef enum {
    ROLE_SWITCH_REASON_WEAR_OFF     = 0,    /* Primary ear removed */
    ROLE_SWITCH_REASON_WEAR_ON      = 1,    /* Primary ear put on */
    ROLE_SWITCH_REASON_LOW_BATTERY  = 2,    /* Low battery trigger */
    ROLE_SWITCH_REASON_APP_FORCE    = 3,    /* App force switch */
} Role_Switch_Reason_t;

/**
 * @brief Get current device role
 */
Device_Role_t sndp_sleep_role_get_current(void);

/**
 * @brief Check if current device is Primary (ROLE_ACTIVE)
 *
 * Command-plane gate: App command dispatch + proximity report to app.
 * Only the steady-state Primary may execute/report.
 */
bool sndp_sleep_role_is_active(void);

/**
 * @brief Check if the data plane (sensors/algo/FIFO tasks) should be running.
 *
 * Data-plane gate: sensor on/off, algo execute, FIFO tasks (PPG/ACC/HR/sleep).
 * Returns true for ROLE_ACTIVE (steady Primary) AND ROLE_SWITCHING (migration
 * in progress: old Primary is stopping streams while new Primary is starting
 * them, so hardware sensor ops must be allowed in both).
 * Returns false for ROLE_STANDBY / ROLE_IDLE.
 */
bool sndp_sleep_role_data_plane_active(void);

/**
 * @brief Check if current device is Secondary (ROLE_STANDBY)
 */
bool sndp_sleep_role_is_standby(void);

/**
 * @brief Check if role switch is in progress (ROLE_SWITCHING)
 */
bool sndp_sleep_role_is_switching(void);

/**
 * @brief Module init
 */
void sndp_sleep_role_switch_init(void);

/**
 * @brief Trigger role switch (called by Primary)
 *
 * Flow:
 * 1. Set ROLE_SWITCHING
 * 2. Send ROLE_SWITCHING to peer (= REQUEST)
 * 3. Wait for ACK/NACK from peer
 */
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason);

/**
 * @brief Receive snapshot from peer (called by new Primary)
 *
 * Restores algo state, opens sensors, becomes ROLE_ACTIVE
 */
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot, uint16_t len);

/**
 * @brief Handle peer role status update (called by comm_cmd recv handler).
 *
 * Reuses role value to implement REQUEST/ACK/NACK protocol:
 *   ROLE_SWITCHING received -> peer wants to switch (REQUEST) -> call on_request
 *   ROLE_ACTIVE received    -> peer became Primary (ACK) -> migrate snapshot, become STANDBY
 *   ROLE_IDLE received       -> peer can't take over (NACK) -> suspend both
 *   ROLE_STANDBY received    -> peer became Secondary -> update state only
 *
 * @param peer_role  Role value received from peer via LR_SYNC_SLEEP_ROLE_STATUS
 */
void sndp_sleep_role_switch_on_peer_status(Device_Role_t peer_role);

/**
 * @brief Handle role switch request from peer (Secondary side).
 *
 * Called when peer sends ROLE_SWITCHING (= REQUEST).
 * If this ear is worn -> send ACK (ROLE_ACTIVE), wait for snapshot.
 * If this ear is also removed -> send NACK (ROLE_IDLE), suspend.
 */
void sndp_sleep_role_switch_on_request(void);

/**
 * @brief Handle conflict (both ears think they're Primary).
 */
void sndp_sleep_role_switch_handle_conflict(void);

void sndp_sleep_role_start(void);

void sndp_sleep_role_stop(void);

void sndp_sleep_role_set_peer(Device_Role_t role);
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_SLEEP_APP_ROLE_SWITCH__ */
#endif /* __SNDP_SLEEP_ROLE_SWITCH_H__ */
