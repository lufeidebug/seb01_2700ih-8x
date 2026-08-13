#if defined(__SNDP_PROJ__)
#ifndef SNDP_SENSOR_SERVICE_H
#define SNDP_SENSOR_SERVICE_H

#include "sndp_if_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sndp_sensor_service - sensor orchestration utility module
 *
 * This module provides unified APIs for controlling sensors (HR, PPG, ACC,
 * proximity, sleep mode). It is designed to decouple BLE command handlers
 * and TWS sync handlers from direct dependencies on sensor driver internals.
 *
 * Design principles:
 *   - "Local" functions perform only local operations, no TWS sync
 *   - Callers (BLE handlers) add TWS sync separately if needed
 *   - TWS recv handlers call local functions directly (no sync needed)
 */

/* ---- HR Measurement ---- */

/**
 * @brief Start heart rate measurement (local only)
 *
 * Sets HR flag, configures sampling rate and dump state,
 * and starts the measurement thread via app thread.
 *
 * @param sampling_rate  PPG sampling rate
 * @param dump_data      Dump state for raw data logging
 */
void sndp_sensor_service_start_heartrate_local(uint8_t sampling_rate, uint8_t dump_data);

/**
 * @brief Stop heart rate measurement (local only)
 *
 * Clears HR flag and stops measurement thread.
 */
void sndp_sensor_service_stop_heartrate_local(void);

/* ---- PPG Notification ---- */

/**
 * @brief Start/stop PPG raw data notification (local only)
 *
 * @param onoff  true=start, false=stop
 */
void sndp_sensor_service_set_ppg_notification_local(bool onoff);

/* ---- ACC Notification ---- */

/**
 * @brief Start/stop accelerometer notification (local only)
 *
 * @param onoff  true=start, false=stop
 */
void sndp_sensor_service_set_acc_notification_local(bool onoff);

/* ---- Proximity ---- */

/**
 * @brief Start proximity detection (local only)
 *
 * Enables proximity flag, starts tick, and triggers immediate reading.
 */
void sndp_sensor_service_start_proximity_local(void);

/**
 * @brief Stop proximity detection (local only)
 *
 * Clears proximity flag and disables tick.
 */
void sndp_sensor_service_stop_proximity_local(void);

/* ---- Sleep Mode ---- */

/**
 * @brief Enter sleep mode (local only)
 *
 * Sets stage flag, switches UI to sleep mode, configures sleep control,
 * enables sleep ANC, and starts sleep analysis algorithm.
 *
 * @param sleep_control  Sleep control parameter for algorithm
 */
void sndp_sensor_service_enter_sleep_mode_local(int32_t sleep_control);

/**
 * @brief Exit sleep mode (local only)
 *
 * Clears stage flag, restores UI to BT mode, disables sleep ANC,
 * and stops sleep analysis algorithm.
 */
void sndp_sensor_service_exit_sleep_mode_local(void);

#ifdef __cplusplus
}
#endif

#endif
#endif
