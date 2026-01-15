/**
 * @file ss_ppg_enum.h
 */

#ifndef SS_PPG_ENUM_H_
#define SS_PPG_ENUM_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enum used to select the target sensor.
 *
 * For SSH401, the type of LED mounted in the sensor package is different,\n
 * so the sensor must be distinguished.
 * @see ss_ppg_init()
 * @see Led
 */
typedef enum {
    SENSOR_SSH202 = 0, /**< For SSH202 */
    SENSOR_SSH401 = 1, /**< For SSH401 */
} Sensor;

/**
 * @brief Sensor interrupt enable bits used with ss_ppg_interrupt_setting().
 *
 * This enumeration defines bit masks to enable specific sensor interrupts\n
 * via the function @ref ss_ppg_interrupt_setting().
 *
 * @see ss_ppg_interrupt_setting()
 */
typedef enum {
    A_FIFO_FULL_EN   = 0x80, /**< Enable FIFO full interrupt. */
    FIFO_DATA_RDY_EN = 0x40, /**< Enable FIFO data ready interrupt. */
    ALC_SAT_EN       = 0x20, /**< Enable Ambient Light Cancellation (ALC) saturation interrupt. */
    PPG_SAT_EN       = 0x10, /**< Enable PPG saturation interrupt. */
    PROX_SAT_EN      = 0x08, /**< Enable proximity saturation interrupt. */
    PROX_INT_EN      = 0x02, /**< Enable proximity interrupt. */
} SensorInterrupt;

/**
 * @brief Enum for selecting a specific sequence in the PPG sensor.
 *
 * The PPG sensor outputs data in 3 different sequences.\n
 * This enum is used to specify a particular sequence, which controls\n
 * the LED light source type, output current, and related settings.
 *
 * @see ss_ppg_led_config()
 * @see ss_ppg_led_integrate_time()
 * @see ss_ppg_pd_select()
 * @see ss_ppg_set_led_current()
 */
typedef enum {
    SEQ0 = 0, /**< Sequence 0 */
    SEQ1 = 1, /**< Sequence 1 */
    SEQ2 = 2, /**< Sequence 2 */
}Seq;

/**
 * @brief Used to configure the light source for a specific sequence.
 *
 * The type of LEDs mounted on the sensor varies by model.\n
 * SSH202 have RED, GREEN, and IR LEDs mounted,\n
 * SSH401 have IR, VCSEL, and GREEN LEDs mounted.
 *
 * @see Sensor
 * @see ss_ppg_init()
 * @note Use the correct LED values depending on the target sensor.
 */
typedef enum {
    LED_RED = 0, /**< RED */
    LED_GREEN = 1, /**< GREEN */
    LED_IR = 2, /**< IR (Infrared Radiation) */
    LED_VCSEL = 3, /**< VCSEL (Valid only for SSH401) */
    LED_OFF = 4, /**< OFF */
}Led;

/**
 * @brief Enum to select the type of PD (Photo Diode) that receives the light when the LED is emitting.
 * @see ss_ppg_pd_select()
 */
typedef enum {
    PD_INT = 0, /**< Internal Photo Diode */
    PD_EXT = 1, /**< External Photo Diode */
    PD_DISABLE = 2, /**< Photo Diode disabled */
}PD;

/**
 * @brief Sampling rates for the PPG sensor operation.
 *
 * Supports various frequencies ranging from 25Hz to 4096Hz.
 *
 * @see ss_ppg_init()
 *
 * @note For some rates, if the 'LP_MODE' bit is set to 1, the effective\n
 * operating frequency slightly differs.
 */
typedef enum {
    SPS_25 = 6, /**< 25Hz */
    SPS_32 = 0, /**< 32Hz */
    SPS_50 = 7, /**< 50Hz */
    SPS_64 = 1,  /**< 64Hz (If the 'LP_MODE' bit is set to 1, operates at 63.5Hz) */
    SPS_100 = 8, /**< 100Hz */
    SPS_128 = 2, /**< 128Hz (If the 'LP_MODE' bit is set to 1, operates at 125Hz) */
    SPS_192 = 3, /**< 192Hz (If the 'LP_MODE' bit is set to 1, operates at 190.5Hz) */
    SPS_200 = 9, /**< 200Hz */
    SPS_256 = 4, /**< 256Hz (If the 'LP_MODE' bit is set to 1, operates at 250Hz) */
    SPS_400 = 10, /**< 400Hz */
    SPS_500 = 11, /**< 500Hz */
    SPS_512 = 5, /**< 512Hz (If the 'LP_MODE' bit is set to 1, operates at 500Hz) */
    SPS_4096 = 12 /**< 4096Hz */
} SamplingRate;

/**
 * @brief Selects the interrupt trigger condition when operating in Proximity mode.
 *
 * For detailed information, please refer to the 'Proximity Interrupt' section in the datasheet.
 * @see ss_ppg_proximity_interrupt_mode
 */
typedef enum {
    THRESHOLD = 0, /**< Interrupt triggered based on threshold */
    STATE = 1     /**< Interrupt triggered based on state */
} ProxIntMode;

/**
 * @brief Enum to select the operation mode of the PPG sensor.
 *
 * This enum defines various operation modes that determine the sequences\n
 * and sensor combinations used during measurement.
 *
 * For detailed information, please refer to the 'Operation Mode Selection'\n
 * section in the datasheet.
 *
 * @see ss_ppg_operation_mode()
 */
typedef enum {
    PPG_0      = 0, /**< SEQ0=PPG0                        */
    PPG_1      = 1, /**< SEQ0=PPG0, SEQ1=PPG1             */
    PPG_2      = 2, /**< SEQ0=PPG0, SEQ1=PPG1,  SEQ2=PPG2 */
    PPG_3      = 3, /**<            SEQ1=PPG1             */
    PPG_4      = 4, /**<            SEQ1=PPG1,  SEQ2=PPG2 */
    PROX       = 5, /**< SEQ0=Prox */
    PROX_PPG_0 = 6, /**< SEQ0=Prox, SEQ1=PPG1             */
    PROX_PPG_1 = 7, /**< SEQ0=Prox, SEQ1=PPG1,  SEQ2=PPG2 */
} OperationMode;


/**
 * @brief Enum to define the output current limits for the LED.
 *
 * Specifies the allowable current ranges for the LED output.
 *
 * @see ss_ppg_set_led_current()
 */
typedef enum {
    RANGE_16_7mA = 0, /**< 16.7 mA */
    RANGE_30_1mA = 1, /**< 30.1 mA */
    RANGE_43_4mA = 2, /**< 43.4 mA */
    RANGE_56_7mA = 3, /**< 56.7 mA */
    RANGE_70_0mA = 4, /**< 70.0 mA */
} LEDCurrentRange;

#ifdef __cplusplus
}
#endif


#endif /* SS_PPG_ENUM_H_ */
