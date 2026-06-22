#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"

#include <stdarg.h>
#include "ss_ppg.h"
#include "ss_os_api.h"
#include "ss_ppg_errno.h"

/**
 * @file ss_os_api.c
 * @brief This code in ss_ppg.c is used to call the Host MCU's OS API.
 * @warning This function MUST be called before calling any 'ss_ppg_xxxx()' functions.
 * @see ss_os_api_config()
 */
 

SS_OS_API* g_func_config;

void ss_os_api_config(SS_OS_API* os_api_config)
{
    g_func_config = os_api_config;
}

/** @private */
static int ss_i2c_read(unsigned char slave_addr, unsigned char reg_addr, unsigned char* reg_value, int read_length)
{
    int ret;
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_i2c_read == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        ret = g_func_config->os_i2c_read(slave_addr, reg_addr, reg_value, read_length);
        if (ret != 0)
            return ret;
        else
            return SS_SUCCESS;
    }
}

/** @private */
static int ss_i2c_write(unsigned char slave_addr, unsigned char reg_addr, unsigned char reg_value)
{
    int ret;
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_i2c_write_byte == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        ret = g_func_config->os_i2c_write_byte(slave_addr, reg_addr, reg_value);
        if (ret != 0)
            return ret;
        else
            return SS_SUCCESS;
    }
}

int os_api_i2c_write_byte(unsigned char reg_addr, unsigned char reg_value)
{
    return os_api_i2c_write_byte_with_slave_addr(SS_PPG_DEFAULT_SLAVE_ADDR, reg_addr, reg_value);
}

int os_api_i2c_read_byte(unsigned char reg_addr, unsigned char* read_value)
{
    return os_api_i2c_read_with_slave_addr(SS_PPG_DEFAULT_SLAVE_ADDR, reg_addr, read_value, 1);
}

int os_api_i2c_read_burst(unsigned char reg_addr, unsigned char* read_value, int read_length)
{
    return os_api_i2c_read_with_slave_addr(SS_PPG_DEFAULT_SLAVE_ADDR, reg_addr, read_value, read_length);
}

int os_api_i2c_write_byte_with_slave_addr(unsigned char slave_addr, unsigned char reg_addr, unsigned char reg_value)
{
    return ss_i2c_write(slave_addr, reg_addr, reg_value);
}

int os_api_i2c_read_with_slave_addr(unsigned char slave_addr, unsigned char reg_addr, unsigned char* read_value, int read_length)
{
    return ss_i2c_read(slave_addr, reg_addr, read_value, read_length);
}

int os_api_memset(void *ptr, int value, size_t num)
{
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_memset == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        g_func_config->os_memset(ptr, value, num);
    }
    
    return SS_SUCCESS;
}

void* os_api_malloc(size_t size)
{
    if (g_func_config == (void*)0)
    {
        return (void*)0;
    }
    else if (g_func_config->os_malloc == (void*)0)
    {
        return (void*)0;
    }
    else
    {
        return g_func_config->os_malloc(size);
    }
}

int os_api_free(void* ptr)
{
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_free == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        g_func_config->os_free(ptr);
    }

    return SS_SUCCESS;
}

int os_api_delay_ms(unsigned int ms)
{
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_delay_ms == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        g_func_config->os_delay_ms(ms);
    }

    return SS_SUCCESS;
}

/** @private */
int os_api_print_log(const char *format, ...)
{
    if (g_func_config == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else if (g_func_config->os_print_log == (void*)0)
    {
        return SS_ERROR_FUNC_NOT_INIT;
    }
    else
    {
        char buffer[64];
        va_list args;
        va_start(args, format);
        POSSIBLY_UNUSED int len = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
    
        g_func_config->os_print_log(buffer);
    }

    return SS_SUCCESS;
}

void os_api_callback_proximity(unsigned char is_wearing)
{
    if (g_func_config == (void*)0)
    {
        return;
    }
    else if (g_func_config->os_print_log == (void*)0)
    {
        return;
    }
    else
    {
        g_func_config->callback_proximity_interrupt(is_wearing);
    }
}

void os_api_callback_ppg_data(SS_PPG* ppg_data, int cnt)
{
    if (g_func_config == (void*)0)
    {
        return;
    }
    else if (g_func_config->callback_ppg_data == (void*)0)
    {
        return;
    }
    else
    {
        g_func_config->callback_ppg_data(ppg_data, cnt);
    }
}

void os_api_callback_ppg_test_data(unsigned char* fifo_data, int fifo_cnt)
{
    if (g_func_config == (void*)0)
    {
        return;
    }
    else if (g_func_config->callback_ppg_test_data == (void*)0)
    {
        return;
    }
    else
    {
        g_func_config->callback_ppg_test_data(fifo_data, fifo_cnt);
    }
}

void os_api_callback_ppg_read_samplerate(uint16_t samplerate)
{
    if (g_func_config == (void*)0)
    {
        return;
    }
    else if (g_func_config->callback_ppg_read_samplerate == (void*)0)
    {
        return;
    }
    else
    {
        g_func_config->callback_ppg_read_samplerate(samplerate);
    }
}

