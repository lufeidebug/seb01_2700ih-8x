#ifndef SS_PPG_REGISTER_H_
#define SS_PPG_REGISTER_H_

/**
 * @file ss_ppg_register.h
 * @brief This file defines the register addresses of the PPG sensor.
 */

/**
 * @brief Configure the Photo Diode for a specific sequence.
 *
 * This register allows selecting between Internal and External Photo Diodes,\n
 * or disabling the Photo Diode altogether.\n
 * Refer to the ss_ppg_pd_select() function for proper usage.
 *
 * @see ss_ppg_pd_select()
 */
#define REG_SEQ_PD_SEL          0x3F

/**
 * @brief Register to configure the LED light source for a specific sequence.
 *
 * This register allows assigning the light source (e.g., Green, IR, etc.) to a particular sequence.\n
 * Refer to the ss_ppg_led_config() function for proper usage.
 *
 * @see ss_ppg_led_config()
 */
#define REG_SEQ_LED_SEL         0x22

/**
 * @brief Register to set the output current of the LED assigned to Sequence 0.
 *
 * A higher value increases the brightness of the LED.\n
 * This register should be configured together with the 'REG_SEQ0_LED_RANGE' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage.
 *
 * @see ss_ppg_set_led_current()
 */
#define REG_SEQ0_LED_CUR        0x23

/**
 * @brief Register to set the output current limit of the LED assigned to Sequence 0.
 *
 * A higher value increases the LED brightness.\n
 * This register should be configured together with the 'REG_SEQ0_LED_CUR' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage. 
 *
 * @see ss_ppg_set_led_current()
 */

#define REG_SEQ0_LED_RANGE      0x24

/**
 * @brief Register to set the output current of the LED assigned to Sequence 1.
 *
 * A higher value increases the brightness of the LED.\n
 * This register should be configured together with the 'REG_SEQ1_LED_RANGE' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage. 
 *
 * @see ss_ppg_set_led_current()
 */
#define REG_SEQ1_LED_CUR        0x25

/**
 * @brief Register to set the output current limit of the LED assigned to Sequence 1.
 *
 * A higher value increases the LED brightness.\n
 * This register should be configured together with the 'REG_SEQ1_LED_CUR' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage. 
 *
 * @see ss_ppg_set_led_current()
 */
#define REG_SEQ1_LED_RANGE      0x26

/**
 * @brief Register to set the output current of the LED assigned to Sequence 2.
 *
 * A higher value increases the brightness of the LED.\n
 * This register should be configured together with the 'REG_SEQ2_LED_RANGE' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage.
 *
 * @see ss_ppg_set_led_current()
 */
#define REG_SEQ2_LED_CUR        0x27

/**
 * @brief Register to set the output current limit of the LED assigned to Sequence 2.
 *
 * A higher value increases the LED brightness.\n
 * This register should be configured together with the 'REG_SEQ2_LED_CUR' register.\n
 * Refer to the ss_ppg_set_led_current() function for proper usage. 
 *
 * @see ss_ppg_set_led_current()
 */
#define REG_SEQ2_LED_RANGE      0x28

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit[0] of this register corresponds to the MSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ0_INTG_TIME_H    0x30

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit [0] of this register corresponds to the LSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ0_INTG_TIME_L    0x31

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit[0] of this register corresponds to the MSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ1_INTG_TIME_H    0x32

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit [0] of this register corresponds to the LSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ1_INTG_TIME_L    0x33

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit[0] of this register corresponds to the MSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ2_INTG_TIME_H    0x34

/**
 * @brief Register for setting the integration time to read the light from the LED assigned to a sequence.
 *
 * The integration time is a 9-bit value,\n
 * where bit [0] of this register corresponds to the LSB of the 9-bit integration time.\n
 * Refer to the ss_ppg_led_integrate_time() function for proper usage.
 *
 * @see ss_ppg_led_integrate_time()
 */
#define REG_SEQ2_INTG_TIME_L    0x35

/**
 * @brief Register to set the DAC range for Sequence 0.
 *
 * For detailed information, please refer to the register map in the datasheet.
 */
#define REG_SEQ0_DAC_RANGE      0xA0

/**
 * @brief Register to set the DAC range for Sequence 1.
 *
 * For detailed information, please refer to the register map in the datasheet.
 */
#define REG_SEQ1_DAC_RANGE      0xA1

/**
 * @brief Register to set the DAC range for Sequence 2.
 *
 * For detailed information, please refer to the register map in the datasheet.
 */
#define REG_SEQ2_DAC_RANGE      0xA2

/**
 * @brief Register to read the 16-bit proximity measurement value (MSB).
 */
#define REG_PROX_DATA_H         0x1A

/**
 * @brief Register to read the 16-bit proximity measurement value (LSB).
 */
#define REG_PROX_DATA_L         0x1B

/**
 * @brief Proximity threshold setting register (MSB).
 *
 * This 16-bit threshold register's MSB part is set here.\n
 * If the measured proximity value is less than or equal to the set 16-bit value,\n
 * the 'INT_STAT_PROX_LOW' interrupt is triggered.\n
 * Refer to the ss_ppg_proximity_threshold() function for proper usage.
 *
 * @see ss_ppg_proximity_threshold()
 */
#define REG_PROX_THRES_LOW_H    0x40

/**
 * @brief Proximity threshold setting register (LSB).
 *
 * This 16-bit threshold register's LSB part is set here.\n
 * If the measured proximity value is less than or equal to the set 16-bit value,\n
 * the 'INT_STAT_PROX_LOW' interrupt is triggered.\n
 * Refer to the ss_ppg_proximity_threshold() function for proper usage.
 *
 * @see ss_ppg_proximity_threshold()
 */
#define REG_PROX_THRES_LOW_L    0x41

/**
 * @brief Proximity threshold setting register (MSB).
 *
 * This register holds the MSB of a 16-bit threshold value.\n
 * When the measured proximity value is greater than or equal to this 16-bit threshold,\n
 * the 'INT_STAT_PROX_HIGH' interrupt is triggered.\n
 * Refer to the ss_ppg_proximity_threshold() function for proper usage. 
 *
 * @see ss_ppg_proximity_threshold()
 */
#define REG_PROX_THRES_HIGH_H   0x42

/**
 * @brief Proximity threshold setting register (LSB).
 *
 * This register holds the LSB of a 16-bit threshold value.\n
 * When the measured proximity value is greater than or equal to this 16-bit threshold,\n
 * the 'INT_STAT_PROX_HIGH' interrupt is triggered.\n
 * Refer to the ss_ppg_proximity_threshold() function for proper usage.
 *
 * @see ss_ppg_proximity_threshold()
 */
#define REG_PROX_THRES_HIGH_L   0x43

/**
 * @brief Register to enable or disable interrupts.
 *
 * Refer to the ss_ppg_interrupt_setting() function for proper usage.
 * 
 * @see ss_ppg_interrupt_setting()
 */
#define REG_INTREEUPT_CONF      0x04

/**
 * @brief Register to check the interrupt status.
 *
 * Refer to the ss_ppg_read_interrupt_status() function for proper usage.
 *
 * @see ss_ppg_read_interrupt_status()
 */
#define REG_INTERRUPT_STAT      0x05

/**
 * @brief Register to set the sample rate.
 *
 * The sample rate can be configured using the ss_ppg_init() function.
 *
 * @see ss_ppg_init()
 */
#define REG_SAMPLE_RATE         0x20

/**
 * @brief Register to control FIFO operation mode and start/stop measurements.
 *
 * The FIFO operation mode can be configured using the ss_ppg_operation_mode() function.\n
 * To start or stop measurements, refer to ss_ppg_start_measurement() and ss_ppg_stop_measurement().
 *
 * @see ss_ppg_start_measurement()
 */
#define REG_MEASUREMENT         0x00

/**
 * @brief Register to configure the interrupt mode.
 *
 * This register allows you to set the conditions for proximity interrupt generation\n
 * and how the interrupt is cleared.\n
 * Refer to ss_ppg_proximity_interrupt_mode() and ss_ppg_interrupt_clear_mode() for configuration.
 
 * @see ss_ppg_proximity_interrupt_mode()
 * @see ss_ppg_interrupt_clear_mode()
 */
#define REG_INT_MODE            0x02

/**
 * @brief Register to read the number of accumulated measurement samples in the FIFO.
 *
 * The FIFO can store up to 255 data samples. Stored data will expire over time.\n
 * For example, when measuring at 25Hz, data begins to expire one by one after approximately 10.2 seconds.\n
 * (1000 ms / 25 Hz * 255 samples = 10.2 sec).\n
 * Refer to the ss_ppg_read_fifo_count() function for proper usage.
 *
 * @see ss_ppg_read_fifo_count()
 */
#define REG_FIFO_DATA_CNT       0x13

/**
 * @brief Register to read measurement data from the FIFO.
 *
 * Each measurement consists of 3 bytes and must be read in 3-byte units.\n
 * Refer to the ss_ppg_read_fifo() function for proper usage.
 *
 * @see ss_ppg_read_fifo()
 */
#define REG_FIFO_DATA           0x14

/**
 * @brief Register to set the threshold count for triggering a A_FIFO_FULL interrupt.
 *
 * This value can be configured using the ss_ppg_init() function.
 *
 * @see ss_ppg_init()
 */
#define REG_FIFO_FULL_CNT       0x15

/**
 * @brief Register to clear the accumulated measurement data in the sensor's FIFO.
 *
 * Refer to the ss_ppg_clear_fifo() function for usage.
 *
 * @see ss_ppg_clear_fifo()
 */
#define REG_FIFO_FLUSH          0x17

/**
 * @brief Register to configure the filter settings for PPG measurement data.
 *
 * For detailed information, please refer to the datasheet.
 */
#define REG_PPG_FILTER          0x2E

/**
 * @brief Register to configure DC cut settings.
 *
 * For detailed information, please refer to the datasheet.
 */
#define REG_DC_CUT_MODE         0x5A

/**
 * @brief Register used to read the sensor chip ID.
 *
 * The chip ID is 0xA3 as of the document creation date and may change due to revisions or other reasons.
 * @see SS_PPG_CHIP_ID
 */
#define REG_CHIP_ID             0xFA


/** @cond INTERNAL */
#define REG_HIDDEN_KEY          0xFF
#define REG_H_AFE_TIME          0x37
#define REG_H_TIA_GAIN          0x86
#define REG_H_INT_GAIN          0x87
/** @endcond */

#endif /* SS_PPG_REGISTER_H_ */
