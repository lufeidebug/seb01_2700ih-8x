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
    ROLE_SWITCHING      = 3,    /* (deprecated) transitional state, no longer used in flow */
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
 * @brief Get current device role
 */
/**
 * @brief Check if current device is Primary (ROLE_ACTIVE)
 *
 * Command-plane gate: App command dispatch + proximity report to app.
 * Only the steady-state Primary may execute/report.
 */
bool sndp_sleep_tws_is_master_with_ble(void);

/**
 * @brief Module init
 */

/**
 * @brief Handle TWS role change (master/slave switch).
 *
 * primary/secondary is determined solely by TWS master/slave.
 * Called when:
 *   1) TWS link established (SNDP_BT_CONN_STATUS_TWS_CONNECTED)
 *   2) TWS role changed (SNDP_BT_CONN_ROLE_ROLE_CHANGED)
 *
 * - TWS master → primary (ROLE_ACTIVE):
 *   - From IDLE:  resume (fresh start, no migration)
 *   - From STANDBY: wait for migration snapshot from old primary,
 *     restore in recv_snapshot, then become ACTIVE
 * - TWS slave → secondary (ROLE_STANDBY):
 *   - From ACTIVE: build migration → stop all streams → send snapshot → set STANDBY
 */
void sndp_sleep_role_switch_on_bt_role_change(void);

/**
 * @brief Receive migration snapshot from old Primary (called by new Primary)
 *
 * Restores all streams (HR/sleep/PPG/ACC/proximity) + algo snapshot, then
 * becomes ROLE_ACTIVE.
 */
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot, uint16_t len);

/**
 * @brief Handle peer role status update (called by comm_cmd recv handler).
 *
 * Just updates peer role state. No protocol handling needed in TWS-based flow.
 *
 * @param peer_role  Role value received from peer via LR_SYNC_SLEEP_ROLE_STATUS
 */
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_SLEEP_APP_ROLE_SWITCH__ */
#endif /* __SNDP_SLEEP_ROLE_SWITCH_H__ */
