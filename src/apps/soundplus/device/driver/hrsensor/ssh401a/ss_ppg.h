/**
 * @file ss_ppg.h
 */
 
#ifndef SS_PPG_H_
#define SS_PPG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ss_ppg_register.h"
#include "ss_ppg_enum.h"

/** @cond INTERNAL */
#ifndef size_t
typedef unsigned int size_t;
#endif

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
/** @endcond */


/** 
 * @brief Default I2C slave address for the PPG sensor.
 */
#define SS_PPG_DEFAULT_SLAVE_ADDR  0x3A

/**
 * @brief Chip ID of the PPG sensor.
 *
 * This value is used to verify the identity of the PPG sensor.\n
 * If the chip ID read by the @c ss_ppg_verify() function does not match this value,\n
 * the verification fails.
 *
 * @see ss_ppg_read_chip_id()
 * @see ss_ppg_verify()
 * @see REG_CHIP_ID
 */
#define SS_PPG_CHIP_ID  0xA3


/**
 * @brief Bit mask for the FIFO FULL interrupt status.
 *
 * This interrupt is triggered when the number of samples in the FIFO reaches the threshold\n
 * defined by the REG_FIFO_FULL_CNT register.
 */
#define INT_STAT_FIFO_FULL      (1 << 7)

/**
 * @brief Interrupt triggered when data is ready to be read from the FIFO.
 */
#define INT_STAT_FIFO_DATA_RDY  (1 << 6)

/**
 * @brief Interrupt triggered when the ambient light cancellation function of the photodiode\n
 * reaches its maximum limit due to overflow.
 *
 * This indicates that ambient light is affecting the output of the ADC.
 */
#define INT_STAT_ALC_SAT        (1 << 5)

/**
 * @brief Interrupt triggered when the measurement signal of the photodiode\n
 * reaches its maximum limit due to overflow.
 *
 * This indicates that ambient light is affecting the output of the ADC.
 */
#define INT_STAT_PPG_SAT        (1 << 4)

/**
 * @brief Interrupt triggered when the proximity measurement signal of the photodiode\n
 * reaches its maximum limit due to overflow.
 *
 * This indicates that ambient light is affecting the output of the ADC.
 */
#define INT_STAT_PROX_SAT       (1 << 3)

/**
 * @brief Interrupt triggered when the "PROX_TH_HIGH" function is completed.
 */
#define INT_STAT_PROX_HIGH      (1 << 1)

/**
 * @brief Interrupt triggered when the "PROX_TH_LOW" function is completed.
 */
#define INT_STAT_PROX_LOW       (1 << 0)

/** @cond INTERNAL */
#define CURRENT_RANGE_16_7 (16.7f)
#define CURRENT_RANGE_30_1 (30.1f)
#define CURRENT_RANGE_43_4 (43.4f)
#define CURRENT_RANGE_56_7 (56.7f)
#define CURRENT_RANGE_70_0 (70.0f)
/** @endcond */

/**
 * @brief Structure to store FIFO output data.
 *
 * This structure holds the data for sequences 0, 1, and 2.\n
 * The buffer is filled with this structure type by the ss_ppg_fifo_parse() function,\n
 * and individual entries can be retrieved one by one by calling ss_ppg_mem_fifo_data_pop().\n
 * Refer to the ss_ppg_example_main() function for usage examples.
 */
typedef struct
{
    unsigned int seq0; /**< This is the measurement value for the light source assigned to SEQ0. */
    unsigned int seq1; /**< This is the measurement value for the light source assigned to SEQ1. */
    unsigned int seq2; /**< This is the measurement value for the light source assigned to SEQ2. */
}SS_PPG;


/**
 * @brief Initializes the PPG sensor with the specified sampling rate and FIFO interrupt threshold.
 * 
 * @param target_sensor Specify the target sensor.
 *
 * @param sample_rate The rate at which the sensor detects signals.\n
 *                    Refer to the SamplingRate enum or sensor datasheet for the supported values.
 * 
 * @param fifo_full_int_cnt The number of data points that must accumulate\n
 *                          in the sensor's FIFO buffer before triggering an interrupt.\n
 *                          If set to 1, the sensor will trigger an interrupt every time it detects data.\n
 *                          If set to 32, the interrupt will occur after 32 data points have accumulated.
 * 
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_INVALID_ARGUMENT
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 * @warning ss_os_api_config() MUST be called before this function.
 */
int ss_ppg_init(Sensor target_sensor, SamplingRate sample_rate, unsigned char fifo_full_int_cnt);

/**
 * @brief The LED light source is configured for SEQn.
 *
 * If SEQ0 is set to `LED_GREEN`,
 * the data received through SEQ0 in the FIFO will be the measurement result using the GREEN LED.
 *
 * @param seq The 'Seq' enumeration
 * @param Led The 'Led' enumeration
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_led_config(Seq seq, Led led);

/**
 * @brief Returns the chip ID of the sensor as a pointer.
 *
 * @param chip_id A Pointer to store the returned id
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT 
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_read_chip_id(unsigned char* chip_id);

/**
 * @brief Returns the number of data entries stored in the sensor's FIFO.
 *
 * If 10 raw data entries are stored, the return value will be 10.
 *
 * @param fifo_count A pointer through which the function returns the FIFO count value.
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_read_fifo_count(unsigned char* fifo_count);

/**
 * @brief Reads the raw data stored in the sensor's FIFO.
 *
 * Each data entry accumulated in the FIFO consists of 3 bytes, so be sure to read in units of 3 bytes.\n
 * Also, the data read is immediately removed from the FIFO.
 *
 * @param read_length This specifies the length of the data to be read.\n
 *                    Specify the value as the return value of the\n
 *                    'ss_ppg_read_fifo_count()' function multiplied by 3.\n
 *                    Since each 'raw_data' is 3 bytes,\n
 *                    this value should be set to 30 if you want to read 10 data entries.
 *                    
 * @param fifo_data A Pointer to a memory buffer allocated with 'read_length' bytes.
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_read_fifo(int read_length, unsigned char* fifo_data);

/**
 * @brief Parses the data read from the sensor's FIFO and pushes it into the ring buffer.\n
 * You can access the data using the 'ss_ppg_mem_fifo_data_pop()' function.
 *
 * @param fifo_data This is the address of the data obtained through the 'ss_ppg_fifo_read()' function.
 * @param fifo_length This is the length of 'fifo_data'.
 */
void ss_ppg_fifo_parse(unsigned char* fifo_data, unsigned char fifo_length);

/**
 * @brief Clears all raw data stored in the sensor's FIFO.
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 */
int ss_ppg_clear_fifo(void);

/**
 * @brief Checks whether the sensor responds to I2C commands.
 * 
 * Reads the sensor's chip ID and verifies whether it matches the defined '@ref SS_PPG_CHIP_ID'.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_NOT_MATCH_ID
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_verify(void);

int ss_ppg_run_seq1_ir(void);
int ss_ppg_run_seq1_green(void);

/**
 * @brief Starts the PPG measurement.
 *
 * The sensor starts emitting light and begins data measurement according to the LED's detailed settings.\n
 * When the sensor is ready to read data from the FIFO, it triggers the 'A_FIFO_FULL' interrupt.\n
 *
 * @note The 'A_FIFO_FULL' interrupt occurs at the same frequency as the sample rate,\n
 * and the interrupt must be cleared faster than this interval after it occurs.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL 
 */
int ss_ppg_start_measurement(void);

/**
 * @brief Stops the measurement of sensor data.
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_stop_measurement(void);

/**
 * @brief Clears the interrupt.
 *
 * At this time, the sensor's INT pin is pulled up to a non-active state.
 *
 * @note The 'A_FIFO_FULL' interrupt occurs at the same frequency as the sample rate,\n
 * and the interrupt must be cleared faster than this interval after it occurs.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_interrupt_clear(void);

/**
 * @brief Sets the interrupt clear mode.
 *
 * If @p is_write_clear is set to 1, writing '1' to a specific bit in register 0x05 will clear the interrupt.\n
 * If @p is_write_clear is set to 0, reading register 0x05 will clear the interrupt.
 *
 * @param is_write_clear 1 to enable write-to-clear mode, 0 to enable read-to-clear mode.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_INVALID_ARGUMENT
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_interrupt_clear_mode(unsigned char is_write_clear);

/**
 * @brief Configures an interrupt to detect a specific state of the sensor.
 *
 * @param sensor_int The 'SensorInterrupt' enumeration.
 * @param is_enable A value of 1 enables the interrupt for SensorInterrupt, and 0 disables it.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_interrupt_setting(SensorInterrupt sensor_int, unsigned char is_enable);


/**
 * @brief Sets the condition to trigger the A_FIFO_FULL interrupt.
 *
 * Sets the number of data samples that must accumulate in the sensor’s FIFO before the A_FIFO_FULL interrupt is triggered.
 *
 * For example,\n
 * If @c fifo_full_int_count is set to 8, an interrupt will be triggered each time 8 samples are stored in the FIFO.\n
 * If the SamplingRate is 25Hz, the interrupt will be triggered every 320ms (40ms × 8 samples).
 *
 * @param fifo_full_int_count It must be greater than 0 and less than or equal to 255.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 *
 * @see ss_ppg_interrupt_handler()
 * @warning If the FIFO is not cleared when the interrupt is triggered, no further interrupts will occur.
 */
int ss_ppg_interrupt_fifo_full_count(unsigned char fifo_full_int_count);

/**
 * @brief Selects the operating mode of the sensor.
 *
 * There are a total of 8 modes.\n
 * for detailed information, please refer to the datasheet.
 *
 * @param op_mode The 'OperationMode' enumeration.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_operation_mode(OperationMode op_mode);

/**
 * @brief Sets the data collection time for the specific light source.
 *
 * The longer this time is, the greater the amount of collected light.
 *
 * @param seq The 'Seq' enumeration.
 *
 * @param time_us This is the data collection time for a specific light source measured in microseconds.\n
 *                The allowable range for this value is from 0.5 to 256.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_INVALID_ARGUMENT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 */
int ss_ppg_led_integrate_time(Seq seq, float time_us);

/**
 * @brief Changes the trigger conditions for the proximity interrupt.
 *
 * Changes the conditions under which an interrupt is triggered\n
 * and cleared when the sensor is operating in proximity mode.\n
 * For detailed information, please refer to the "Proximity Interrupt" section of the datasheet.
 * 
 * @param seq The 'ProxIntMode' enumeration.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL 
 */
int ss_ppg_proximity_interrupt_mode(ProxIntMode mode);

/**
 * @brief Specifies the interrupt threshold for proximity detection when the sensor is operating in proximity mode.
 *
 * @param high_threshold When the 'PROX_INT_EN' interrupt is enabled\n
 *                       and the measured value (seq0) reaches this threshold,\n
 *                       a 'PROX_INT_H' interrupt is triggered.
 * @param low_threshold When the 'PROX_INT_EN' interrupt is enabled\n
 *                       and the measured value (seq0) reaches this threshold,\n
 *                       a 'PROX_INT_L' interrupt is triggered.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 */
int ss_ppg_proximity_threshold(unsigned short high_threshold, unsigned short low_threshold);

/**
 * @brief Reads the currently measured proximity value.
 *
 * @param value A Pointer to a 16-bit variable.
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_read_proximity_value(unsigned short* value);

/**
 * @brief Reads the interrupt status of the sensor.
 *
 * The value of @c status can be masked with the following defined constants\n
 * to check whether specific interrupts have occurred:
 * 
 * - @ref INT_STAT_FIFO_FULL
 * - @ref INT_STAT_FIFO_DATA_RDY
 * - @ref INT_STAT_ALC_SAT
 * - @ref INT_STAT_PPG_SAT
 * - @ref INT_STAT_PROX_SAT
 * - @ref INT_STAT_PROX_HIGH
 * - @ref INT_STAT_PROX_LOW
 *
 * @param status A Pointer to a 8-bit variable
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_READ_FAIL 
 */
int ss_ppg_read_interrupt_status(unsigned char* status);

/**
 * @brief Handles and processes interrupts generated by the PPG sensor.
 *
 * This function should be called when the sensor's interrupt pin is triggered from high to low.\n
 * Call this function from the host MCU when an interrupt is detected.
 *
 * When this function is called,\n
 * it checks the proximity status or reads data from the FIFO and stores it in the buffer.
 */
void ss_ppg_interrupt_handler(void);

/**
 * @brief Sets the LED current for a specific sequence.
 *
 * The higher the current value, the brighter the LED emits.\n
 * Be aware that using excessively high current increases power consumption\n
 * and may cause the measurement values to saturate.
 *
 * @param seq The 'ProxIntMode' enumeration.
 * @param range The 'LEDCurrentRange' enumeration.
 *              This is the limit for current control.
 * @param current This value is in mA(milliamperes)
 *                and must not exceed the LEDCurrentRange.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_INVALID_ARGUMENT
 *
 * @note @c current cannot exceed the @c range.\n
 * If the @c range is 16.7 mA, then the @c current cannot be greater than 16.7 mA.
 */
int ss_ppg_set_led_current(Seq seq, LEDCurrentRange range, float current);

/**
 * @brief The PPG sensor has both internal and external photodiodes (PDs) for measuring light.
 *
 * You can select which PD to use for data acquisition, and disable the unused one if necessary.
 *
 * @param seq The 'Seq' enumeration.
 * @param pd The 'PD' enumeration.
 *
 * @return Returns SS_SUCCESS on success, and a negative value on failure.
 * @retval SS_SUCCESS
 * @retval SS_ERROR_FUNC_NOT_INIT
 * @retval SS_ERROR_I2C_WRITE_FAIL
 * @retval SS_ERROR_I2C_READ_FAIL
 */
int ss_ppg_pd_select(Seq seq, PD pd);

/**
 * @brief It retrieves PPG data from the ring buffer, and the data is removed from the buffer.
 *
 * The 'ss_ppg_fifo_parse()' function parses raw data and stores it in the ring buffer.\n
 * When this function is called,\n
 * it retrieves the data from the ring buffer and simultaneously removes it from the buffer.
 *
 * @return Returns a pointer to the PPG structure.\n
 *         This structure contains the measurement values\n
 *         for seq0 to seq2 read from the sensor's FIFO\n
 *         when the A_FIFO_FULL interrupt occurs.
 *
 * @see ss_ppg_fifo_parse()
 */
SS_PPG* ss_ppg_mem_fifo_data_pop(void);

/**
 * @brief Returns the number of FIFO data entries stored in the ring buffer.
 * 
 * Returns the number of FIFO data entries read from the sensor that have been parsed into the PPG structure format.\n
 * If a total of 4 data entries are accumulated in the FIFO\n
 * according to the PPG sensor's operation mode,\n
 * and the sequence data is stacked as 1, 2, 1, 2, ...\n
 * the data for SEQ1 and SEQ2 will be stored together in a single PPG structure.\n
 * In other words, in this case, the ring buffer count will be 2.
 *
 * @return Returns the number of parsed PPG data entries.
 *
 * @see ss_ppg_operation_mode()
 * @see ss_ppg_fifo_parse()
 */
int ss_ppg_mem_get_fifo_data_count(void);


int ss_ppg_open_fifo(void);

int ss_ppg_close_fifo(void);

int ss_ppg_test_mode_switch(unsigned char en);

unsigned int ss_ppg_get_ppg_samples_count(void);

void ss_ppg_clear_ppg_samples_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SS_PPG_H_ */