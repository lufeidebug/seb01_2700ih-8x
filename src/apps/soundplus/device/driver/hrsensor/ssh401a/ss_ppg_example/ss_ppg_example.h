/**
 * @file ss_ppg_example.h
 */
 
#ifndef SS_PPG_EXAMPLE_H_
#define SS_PPG_EXAMPLE_H_

/**
 * @brief Enum defining the operation modes for the example code.
 * @see ss_ppg_example_main()
 */ 
typedef enum {
    EXAM_GREEN_ONLY = 0, /**< work green only */
    EXAM_PROX_GREEN = 1, /**< work green with proximity */
} ExampleMode;

/* --------------------------------------------------------------
 * If the function name contains the word "example",
 * it is intended for example purposes only.
 * Proper tuning is required in the actual assembly environment.
 * -------------------------------------------------------------- */

/*
 * @brief Enables proximity-triggered activation of Green LED for blood flow measurement.
 *
 * If the 'EXAMPLE_WORK_WITH_PROXIMITY' define is enabled\n
 * the system uses the built-in IR LED of the PPG sensor to detect proximity interrupts.\n
 * When proximity is detected the GREEN LED is activated and green data\n
 * which reflects blood flow changes is received.
 */
//#define EXAMPLE_WORK_WITH_PROXIMITY

/**
 * @brief Demonstrates how to use the PPG sensor control API.
 *
 * This function shows an example usage of the PPG sensor API.\n
 * When called, the sensor operates at 25Hz with the Green LED enabled.\n
 * An interrupt is triggered whenever 8 samples are collected in the FIFO buffer,\n
 * and the measured values are printed via log output.
 *
 * @param exam_mode @ref EXAM_GREEN_ONLY : The GREEN LED lights up immediately and continuously measures blood flow.\n
 *                  @ref EXAM_PROX_GREEN : Uses IR to detect object proximity.\n
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
 * The GREEN LED lights up and measures blood flow only while proximity is maintained.
 *
 */
int ss_ppg_example_main(ExampleMode exam_mode);

/**
 * This proximity configuration function uses\n
 * the sensor's built-in IR to detect the presence of an object nearby\n
 * and if detected, enables measurement of blood flow changes using the Green LED.\n
 * 
 * To call this function, enable the `EXAMPLE_WORK_WITH_PROXIMITY` define.
 */
int ss_ppg_example_use_proximity(void);

#endif /* SS_PPG_EXAMPLE_H_ */