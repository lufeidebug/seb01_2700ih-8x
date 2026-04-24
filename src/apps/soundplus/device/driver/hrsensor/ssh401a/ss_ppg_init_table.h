/**
 * @file ss_ppg_init_table.h
 */

#ifndef SS_PPG_INIT_TABLE_H_
#define SS_PPG_INIT_TABLE_H_

#include "ss_ppg.h"

/**
 * @brief Initialization settings for driving the PPG sensor.
 *
 * These settings are written to the sensor all at once when the ss_ppg_init() function is called.
 *
 * @note These settings are not optimized values.\n
 * Proper tuning is required based on the actual application.
 */
extern const unsigned char g_ss_ppg_init_register[][2];

/**
 * @brief Returns the number of entries in the sensor register initialization table.
 *
 * The initialization table is defined as a const array in ss_ppg_init_table.c.
 *
 * @param sensor specific sensor
 *
 * @return Number of initialization entries.
 */
int ss_ppg_register_init_count(Sensor sensor);

/**
 * @brief Returns a pointer to the initialization table for the specified sensor's register configuration.
 *
 * @param sensor specific sensor
 *
 * @return Sensor register initialization table pointer
 */
const unsigned char (*ss_ppg_register_get_table(Sensor sensor))[2];

extern const unsigned char ss_ppg_init_register_seq1_ir_ssh401[][2];
extern const int ss_ppg_register_seq1_ir_ssh401_count;

extern const unsigned char ss_ppg_init_register_seq1_green_ssh401[][2];
extern const int ss_ppg_register_seq1_green_ssh401_count;

#endif /* SS_PPG_INIT_TABLE_H_ */
