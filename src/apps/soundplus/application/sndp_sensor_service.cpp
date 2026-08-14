#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "string.h"
#include "hal_trace.h"

#include "sndp_sensor_service.h"
#include "sndp_if_device.h"
#include "sndp_if_common.h"
#include "sndp_heart_rate.h"
#include "sndp_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sndp_sensor_service - sensor orchestration utility module
 *
 * Provides unified APIs for controlling sensors (HR, PPG, ACC, proximity,
 * sleep mode). This module wraps the coordination between device-level
 * flag management, sensor driver calls, and UI transitions.
 *
 * Design:
 *   - All functions are "local only" - no TWS sync is performed
 *   - Callers (BLE handlers) add TWS sync separately via sndp_comm_cmd
 *   - TWS recv handlers call these directly (no sync loop)
 *   - This module has NO dependency on sndp_interact_app or sndp_comm_cmd
 */

/* ---- HR Measurement ---- */

void sndp_sensor_service_start_heartrate_local(uint8_t sampling_rate, uint8_t dump_data)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: start_hr rate=%d dump=%d", sampling_rate, dump_data);

    sndp_dev_sleep_app_set_heartrate_onoff(false, 0x01);

    if (sampling_rate != 0xFF) {
        sndp_hr_mearsuring_set_sampling_rate(sampling_rate);
    }
    if (dump_data != 0xFF) {
        sndp_hr_mearsuring_set_dump_state(PPG_DUMP_STATE, dump_data ? 1 : 0);
    }

    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_start,
                                  sampling_rate, dump_data, 0);
#endif
}

void sndp_sensor_service_stop_heartrate_local(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: stop_hr");

    sndp_dev_sleep_app_set_heartrate_onoff(false, 0x00);
    sndp_call_func_in_app_thread((uint32_t)sndp_hr_mearsuring_stop, 0, 0, 0);
#endif
}

/* ---- PPG Notification ---- */

void sndp_sensor_service_set_ppg_notification_local(bool onoff)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: ppg_ntf=%d", onoff);

    if (onoff) {
        sndp_ppg_notification_start(0x01);
    } else {
        sndp_ppg_notification_stop();
    }
#endif
}

/* ---- ACC Notification ---- */

void sndp_sensor_service_set_acc_notification_local(bool onoff)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: acc_ntf=%d", onoff);

    if (onoff) {
        sndp_acc_notification_start(0x01);
    } else {
        sndp_acc_notification_stop();
    }
#endif
}

/* ---- Proximity ---- */

void sndp_sensor_service_start_proximity_local(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: start_proximity");

    sndp_dev_sleep_app_set_proximity_onoff(false, 0x01);
    sndp_hr_proximity_tick_enable(true);
#endif
}

void sndp_sensor_service_stop_proximity_local(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    TR_INFO(1, "sensor_svc: stop_proximity");

    sndp_dev_sleep_app_set_proximity_onoff(false, 0x00);
    sndp_hr_proximity_tick_enable(false);
#endif
}

/* ---- Sleep Mode ---- */

void sndp_sensor_service_enter_sleep_mode_local(int32_t sleep_control)
{
#if defined(__SNDP_SLEEP_APP__)
    TR_INFO(1, "sensor_svc: enter_sleep ctrl=%d", sleep_control);

    sndp_dev_sleep_app_set_stage_onoff(false, 0x01);
    sndp_ui_working_mode_sleep_app_set(SNDP_DEV_WORKING_MODE_SLEEP);

#if defined(__SNDP_HEART_RATE_MGR__)
    sndp_hr_set_sleep_control(sleep_control);
    sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_start, 0, 0, 0);
#endif
    sndp_ui_sleep_anc_mode_on();
#endif
}

void sndp_sensor_service_exit_sleep_mode_local(void)
{
#if defined(__SNDP_SLEEP_APP__)
    TR_INFO(1, "sensor_svc: exit_sleep");

    sndp_dev_sleep_app_set_stage_onoff(false, 0x00);
    sndp_ui_working_mode_sleep_app_set(SNDP_DEV_WORKING_MODE_BT);
    sndp_ui_sleep_anc_mode_off();
#if defined(__SNDP_HEART_RATE_MGR__)
    sndp_call_func_in_app_thread((uint32_t)sndp_sleep_analysis_stop, 0, 0, 0);
#endif
#endif
}

#ifdef __cplusplus
}
#endif
#endif
