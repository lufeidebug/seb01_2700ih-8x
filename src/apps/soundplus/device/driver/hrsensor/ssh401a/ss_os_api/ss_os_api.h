/**
 * @file ss_os_api.h
 */

#ifndef SS_OS_API_H_
#define SS_OS_API_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "ss_ppg.h"

/** @cond INTERNAL */
#ifndef size_t
typedef unsigned int size_t;
#endif
/** @endcond */


/**
 * @brief Structure to configure function pointers required by the ss_ppg API for the host MCU.
 *
 * This structure holds pointers to functions for I2C communication, delay, logging, and memory management.
 *
 * @see ss_os_api_config()
 */
typedef struct
{
    int (*os_i2c_write_byte)(unsigned char slave_addr, unsigned char reg_addr, unsigned char reg_value);
    /**< Set the function pointer for 1-byte I2C write operation on the Host MCU side.
    * @warning A valid pointer must be set. 
    */
   
    int (*os_i2c_read)(unsigned char slave_addr, unsigned char reg_addr, unsigned char* reg_value, int read_length);
    /**< Set the function pointer for burst reading n bytes via I2C on the Host MCU side.
    * @warning A valid pointer must be set. 
    */
    
    void (*os_delay_ms)(unsigned int ms);
    /**< Set the function pointer to delay for a specified number of milliseconds.
    * @warning A valid pointer must be set. 
    */
    
    void (*os_print_log)(const char *format, ...);
    /**< Set the function pointer for the Host MCU side function that outputs log messages.
    * @note This variable is optional.\n
    * However, if the pointer is not assigned, log messages will not be output.
    */
    
    void* (*os_malloc)(size_t size);
    /**< Set the function pointer for the 'malloc' function of the OS used on the Host MCU side.
    * @warning A valid pointer must be set. 
    */
    
    void* (*os_memset)(void *ptr, int value, size_t num);
    /**< Set the function pointer for the 'memset' function used by the OS on the Host MCU side.
    * @warning A valid pointer must be set. 
    */
    
    void (*os_free)(void *ptr);
    /**< Set the function pointer for the 'free' function used by the OS on the Host MCU side.
    * @warning A valid pointer must be set. 
    */

    void (*callback_proximity_interrupt)(unsigned char is_wearing);
    /**< Specifies the pointer to be called when a proximity detection interrupt occurs.
    * @see ss_ppg_proximity_threshold()
    */

    void (*callback_ppg_data)(SS_PPG* ppg_data);
    /**< Specifies the pointer to which the PPG data read from the sensor's FIFO will be delivered on the Host MCU side.
    */
} SS_OS_API;


/**
 * @brief This function assigns function pointers to the host MCU's functions for use by the ss_ppg API.
 *
 * @param func_config Function pointers required for I2C communication, log output, and memory management 
 * @warning This function MUST be called before calling any 'ss_ppg_xxxx()' functions.
 * @see SS_OS_API
 */
void ss_os_api_config(SS_OS_API* os_api_config);

/**
 * @brief i2c write byte with slave address
 */
int os_api_i2c_write_byte_with_slave_addr(unsigned char slave_addr, unsigned char reg_addr, unsigned char reg_value);

/**
 * @brief i2c write byte
 */
int os_api_i2c_write_byte(unsigned char reg_addr, unsigned char reg_value);

/**
 * @brief i2c read with slave address
 */
int os_api_i2c_read_with_slave_addr(unsigned char slave_addr, unsigned char reg_addr, unsigned char* read_value, int read_length);

/**
 * @brief i2c read byte
 */
int os_api_i2c_read_byte(unsigned char reg_addr, unsigned char* read_value);

/**
 * @brief i2c read burst
 */
int os_api_i2c_read_burst(unsigned char reg_addr, unsigned char* read_value, int read_length);

/**
 * @brief memset
 */
int os_api_memset(void *ptr, int value, size_t num);

/**
 * @brief malloc
 */
void* os_api_malloc(size_t size);

/**
 * @brief free
 */
int os_api_free(void* ptr);

/**
 * @brief delay milliseconds
 */
int os_api_delay_ms(unsigned int ms);

/** @private */
int os_api_print_log(const char *format, ...);

/**
 * @brief callback proximity state to Host MCU
 *
 * On proximity interrupt, the sensor triggers a callback to the Host MCU with the proximity detection status.
 * @param is_wearing A value of 0 means proximity is not detected, while a value of 1 means proximity is detected.
 *
 * @note This works only when the PROX mode is enabled in the PPG operation mode.
 *
 * @see ss_os_api_config()
 * @see ss_ppg_operation_mode()
 * @see ss_ppg_proximity_threshold()
 * @see ss_ppg_interrupt_handler()
 */
void os_api_callback_proximity(unsigned char is_wearing);

/**
 * @brief The measured PPG data is passed to the Host MCU via this callback.
 * @param ppg_data PPG data measured from SEQ0, SEQ1, and SEQ2
 * @see ss_os_api_config()
 * @see SS_PPG
 */
void os_api_callback_ppg_data(SS_PPG* ppg_data);

#ifdef __cplusplus
}
#endif

#endif /* SS_OS_API_ */
