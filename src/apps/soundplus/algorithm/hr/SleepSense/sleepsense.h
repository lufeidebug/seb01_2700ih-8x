#ifndef __SLEEPSENSE_H__
#define __SLEEPSENSE_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define DBBEATS_ACC_SAMPLINGRATE  125
#define DBBEATS_PPG_SAMPLINGRATE  64
#define DBBEATS_DEV_SAMPLINGRATE  1
#define COMBINE_TIMEOUT_MS   3000   // put_sensor_data & put_app_data max diff ms

struct HrvIndices {
    int16_t HR;
    int16_t SDNN;
    int16_t coherence;    
};

struct SleepHrvIndices {
    short HR[30];
    short SDNN[30];
};

struct Dump {
    int16_t accel_data[DBBEATS_ACC_SAMPLINGRATE*3];
    int32_t ppg_data[DBBEATS_PPG_SAMPLINGRATE];
    int8_t  dev_source[DBBEATS_DEV_SAMPLINGRATE];
    uint8_t is_contact;
    int8_t led_state;
    int32_t pkt_interval;
};

typedef void (*dbbeats_print_log_ptr)(const char *msg) ;
void dbbeats_print_log_cfg(dbbeats_print_log_ptr ptr);

void dbbeats_initialize_heartrate_data(int8_t ppg_sampling_rate,
                                       uint8_t dump_state);

void dbbeats_put_heartrate_data(int16_t accel_data[],
                                int32_t ppg_data[],
                                int8_t dev_source[],
                                uint8_t is_contact,
                                int8_t led_state,
                                int32_t cnt_accel_h,
                                int32_t cnt_ppg_h,
                                int32_t cnt_dev_h,
                                int32_t pkt_interval);

void dbbeats_get_heartrate_data(struct HrvIndices *hrv_indices,
                                int16_t *result_code,
                                int8_t *hr_count,
                                int8_t *led_control,
                                struct Dump *debug_dump);

typedef void (*SLEEP_CALLBACK)(int8_t *sleep_stage,
                               int8_t sleep_position,
                               int8_t sound_control,
                               int16_t result_code);

void dbbeats_initialize_sleep_data(int32_t sleep_control, SLEEP_CALLBACK callback);

void dbbeats_put_sleep_sensor_data(void);

void dbbeats_put_sleep_app_data(int16_t accel_data_m[],
                                uint8_t screen_status[],
                                int8_t sound_state);

void dbbeats_get_sleep_data(int8_t *sleep_stage,
                            int8_t *sleep_position,
                            int8_t *sound_control,
                            int16_t *result_code);

const char* lib_engine_version(void);

#ifdef __cplusplus
}
#endif


#endif // __SLEEPSENSE_H__
