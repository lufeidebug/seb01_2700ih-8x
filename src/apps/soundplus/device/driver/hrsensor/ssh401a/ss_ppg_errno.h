#ifndef SS_PPG_ERRNO_H_
#define SS_PPG_ERRNO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ss_ppg_errno.h
 * @brief This file defines the error codes returned by the SS_PPG API.
 *
 */

/**
 * Operation completed successfully.
 */
#define SS_SUCCESS                    (0)

/**
 * Errors returned by the SS_PPG API start from the 1000 range.
 */
#define SS_ERROR_BASE                 (1000) 

/**
 * The function pointers for the host MCU's OS API have not been set.\n
 * Please call ss_os_api_config first.
 *
 * @see SS_OS_API
 * @see ss_os_api_config()
 */
#define SS_ERROR_FUNC_NOT_INIT       -(SS_ERROR_BASE + 1)

/**
 * Invalid parameter.\n
 * Please check the values passed to the function.
 */
#define SS_ERROR_INVALID_ARGUMENT    -(SS_ERROR_BASE + 2)

/**
 * An error occurred during I2C write communication.\n
 * Please check whether the OS API function pointers are properly set and verify the I2C function of the host MCU.
 *
 * @see SS_OS_API
 * @see ss_os_api_config()
 */
#define SS_ERROR_I2C_WRITE_FAIL      -(SS_ERROR_BASE + 3)

/**
 * An error occurred during I2C read communication.\n
 * Please check whether the OS API function pointers are properly set and verify the I2C function of the host MCU.
 *
 * @see SS_OS_API
 * @see ss_os_api_config()
 */
#define SS_ERROR_I2C_READ_FAIL       -(SS_ERROR_BASE + 4)

/**
 * There is not enough memory available on the host MCU for allocation.
 */
#define SS_ERROR_NOT_ENOUGH_MEMORY   -(SS_ERROR_BASE + 5)

/**
 * The chip ID read from the sensor does not match the chip ID defined in the SS_PPG API.
 */
#define SS_ERROR_NOT_MATCH_ID        -(SS_ERROR_BASE + 6)

#ifdef __cplusplus
}
#endif

#endif /* SS_PPG_ERRNO_H_ */
