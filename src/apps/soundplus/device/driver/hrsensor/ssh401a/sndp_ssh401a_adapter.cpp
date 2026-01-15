#if defined(__SNDP_HRSENSOR_SSH401A__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_iomux.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"
#include "multi_heap.h"
#include "hal_i2c.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_hr.h"
#include "sndp_hal_wear_detect.h"
#include "sndp_ssh401a_adapter.h"
#include "sndp_i2c.h"

#include "ss_os_api.h"
#include "ss_ppg.h"
#include "ss_ppg_errno.h"
#include "ss_ppg_example.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SSH401A_IRQ_DEBOUNCE_REPEAT_MS            (20) //ms
#define SSH401A_IRQ_DEBOUNCE_DELAY_MS             (50) //ms
    
#define SSH401A_I2C_TYPE                          (SNDP_I2C_HW_TASK)
#define SSH401A_I2C_ID                            (HAL_I2C_ID_3)


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void ssh401a_irq_debounce_delay_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool ssh401a_inited = false;

osTimerDef(SSH401A_IRQ_DEBOUNCE_TIMER, ssh401a_irq_debounce_delay_handler);
static osTimerId ssh401a_irq_debounce_timer = NULL;


static sndp_hal_hr_measure_callback  ssh401a_hr_measure_cb_ptr = NULL;
static sndp_hal_hr_read_raw_data_callback ssh401a_hr_read_raw_data_cb_ptr = NULL;
static sndp_hal_hr_calib_callback ssh401a_hr_calib_cb_ptr = NULL;


static sndp_hal_wear_status_changed_callback ssh401a_wear_status_changed_cb_ptr = NULL;
static sndp_hal_wear_status_e ssh401a_wear_status = SNDP_HAL_WEAR_OFF;


static multi_heap_handle_t ssh401a_heap;
static uint8_t ssh401a_heap_buf[1024*10];

static SS_OS_API ssh401a_os_api_config;

/**************************************************************************************************
* Function
**************************************************************************************************/
POSSIBLY_UNUSED static int32_t ssh401a_nv_read(uint8_t *ptrData, uint16_t num)
{
#if 0    
    int ret = sndp_da_read_field_data_from_running_param(SNDP_DA_FIELD_TOUCH_CALIB_DATA, ptrData, num, true);
    if(ret){
        SSH401A_TRACE(1, "ret=%d", ret);
    }
#endif    
    return 0;
}

POSSIBLY_UNUSED static int32_t ssh401a_nv_write(uint8_t *ptrData, uint16_t num)
{
#if 0     
    int ret = sndp_da_write_field_data_to_running_param(SNDP_DA_FIELD_TOUCH_CALIB_DATA, ptrData, num, true);
    if(ret){
        SSH401A_TRACE(1, "ret=%d", ret);
    }
#endif    
    return 0;
}


static bool ssh401a_i2c_init(void)
{   
    sndp_i2c_open(SSH401A_I2C_TYPE, SSH401A_I2C_ID);
    return true;
}

POSSIBLY_UNUSED static int ssh401a_i2c_read(unsigned char slave_addr, unsigned char reg_addr, unsigned char* reg_value, int read_length)
{
    return (int32_t)sndp_i2c_read(SSH401A_I2C_TYPE, SSH401A_I2C_ID, slave_addr, &reg_addr, 1, reg_value, read_length);
}

POSSIBLY_UNUSED static int32_t ssh401a_i2c_write(unsigned char slave_addr, unsigned char reg_addr, unsigned char *regData, uint8_t num)
{
    uint8_t write_data[10];
    uint8_t write_len = 0;

    write_data[write_len++] = reg_addr;
    for(uint8_t i = 0; i < num && i < sizeof(write_data); i++) {
        write_data[write_len++] = regData[i];
    }
    return (int32_t)sndp_i2c_write(SSH401A_I2C_TYPE, SSH401A_I2C_ID, slave_addr, write_data, write_len);
}

POSSIBLY_UNUSED static int ssh401a_i2c_write_byte(unsigned char slave_addr, unsigned char reg_addr, unsigned char reg_value)
{
    uint8_t write_data[5];
    uint8_t write_len = 0;

    write_data[write_len++] = reg_addr;
    write_data[write_len++] = reg_value;
    return (int32_t)sndp_i2c_write(SSH401A_I2C_TYPE, SSH401A_I2C_ID, slave_addr, write_data, write_len);
}


void ssh401a_delay_ms(unsigned int ms)
{
    osDelay(ms);
}

static int32_t ssh401a_heap_init(void)
{
    ssh401a_heap = multi_heap_register(ssh401a_heap_buf, sizeof(ssh401a_heap_buf));
    return 0;
}

static void * ssh401a_heap_malloc(size_t size)
{
    return multi_heap_malloc(ssh401a_heap, size);
}

static void ssh401a_heap_free(void *ptr)
{
    multi_heap_free(ssh401a_heap, ptr);
}

static void * ssh401a_heap_memset(void *ptr, int value, size_t num)
{
    memset(ptr, value, num);
    return ptr;
}


void ssh401a_callback_proximity_interrupt(unsigned char is_wearing)
{
    SSH401A_TRACE(1, "is_wearing=%d", is_wearing);
    
    ssh401a_wear_status = is_wearing ? SNDP_HAL_WEAR_ON : SNDP_HAL_WEAR_OFF;
    if(ssh401a_wear_status_changed_cb_ptr) {
        ssh401a_wear_status_changed_cb_ptr(ssh401a_wear_status);
    }
}

static void ssh401a_callback_ppg_data(SS_PPG* ppg_data)
{

}


static void ssh401a_deal_irq_data(void)
{
    
}

POSSIBLY_UNUSED static void ssh401a_irq_debounce_delay_handler(void const *param)
{
    ssh401a_deal_irq_data();
}

void ssh401a_irq_debounce(void)
{
    SSH401A_TRACE(1, "...");
    //osTimerStop(ssh401a_irq_debounce_timer);
    //osTimerStart(ssh401a_irq_debounce_timer, SSH401A_IRQ_DEBOUNCE_DELAY_MS);
    ss_ppg_interrupt_handler();
}

static void ssh401a_irq_handler(enum HAL_GPIO_PIN_T pin)
{
    static uint32_t last_time = 0;
    uint32_t curr_time = hal_sys_timer_get();
    uint32_t passed_ticks = hal_timer_get_passed_ticks(curr_time, last_time);

    SSH401A_TRACE(1, "passed_ms=%d, repeat_ms=%d", TICKS_TO_MS(passed_ticks), SSH401A_IRQ_DEBOUNCE_REPEAT_MS);
    
    if(TICKS_TO_MS(passed_ticks) >= SSH401A_IRQ_DEBOUNCE_REPEAT_MS) {
        last_time = hal_sys_timer_get();
        sndp_call_func_in_app_thread((uint32_t)ssh401a_irq_debounce, 0, 0, 0);
    }

}

static void ssh401a_irq_init(void)
{
    if (ssh401a_irq_debounce_timer == NULL) {
        ssh401a_irq_debounce_timer = osTimerCreate(osTimer(SSH401A_IRQ_DEBOUNCE_TIMER), osTimerOnce, NULL);
		ASSERT(ssh401a_irq_debounce_timer != NULL, "%s, %d", __func__, __LINE__);
    }
    
    if(app_hrsensor_status_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
        struct HAL_GPIO_IRQ_CFG_T gpiocfg;
        
        gpiocfg.irq_enable = true;
        gpiocfg.irq_debounce = true;
        gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
        gpiocfg.irq_handler = ssh401a_irq_handler;
        gpiocfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
        hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_hrsensor_status_pin_cfg.pin, &gpiocfg);
    }
}

static void ssh401a_power_switch(uint8_t onoff)
{
    if(app_hrsensor_en_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&app_hrsensor_en_pin_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)app_hrsensor_en_pin_cfg.pin, HAL_GPIO_DIR_OUT, onoff);
    }
}

#define SSH401A_LOGGER(str, ...)        SNDP_TRACE(0, "[SSH401A] %s, " str, __func__, ##__VA_ARGS__)

int32_t ssh401a_init(void)
{
    if(ssh401a_inited) {
        SSH401A_TRACE(0, "inited, rtn");
        return SNDP_HAL_RET_OK;
    }

    ssh401a_heap_init();
    
    if(!ssh401a_i2c_init()) {
        SSH401A_TRACE(0, "i2c init failed");
        return SNDP_HAL_RET_FAIL;
    }

    ssh401a_power_switch(1);
    
    ssh401a_os_api_config.os_i2c_write_byte = ssh401a_i2c_write_byte;
    ssh401a_os_api_config.os_i2c_read = ssh401a_i2c_read;
    ssh401a_os_api_config.os_delay_ms = ssh401a_delay_ms;
    ssh401a_os_api_config.os_print_log = sndp_trace_printf;
    ssh401a_os_api_config.os_malloc = ssh401a_heap_malloc;
    ssh401a_os_api_config.os_memset = ssh401a_heap_memset;
    ssh401a_os_api_config.os_free = ssh401a_heap_free;
    ssh401a_os_api_config.callback_proximity_interrupt = ssh401a_callback_proximity_interrupt;
    ssh401a_os_api_config.callback_ppg_data = ssh401a_callback_ppg_data;
    
    ss_os_api_config(&ssh401a_os_api_config);

    if (ss_ppg_example_main(EXAM_PROX_GREEN) != SS_SUCCESS) {
        SSH401A_TRACE(0, "ppg init failed");
        return SNDP_HAL_RET_FAIL;
    }

    ssh401a_irq_init();

    ssh401a_inited = true;
    SSH401A_TRACE(0, "done.");
    
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_enter_standby_mode(void)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t ssh401a_enter_detection_mode(void)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t ssh401a_set_hr_measure_callback(sndp_hal_hr_measure_callback callback)
{
    ssh401a_hr_measure_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_start_hr_measure(void)
{
    if (ss_ppg_start_measurement() != SS_SUCCESS) {
        SSH401A_TRACE(0, "start_measurement failed");
        return SNDP_HAL_RET_FAIL;
    }
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_stop_hr_measure(void)
{
    if (ss_ppg_stop_measurement() != SS_SUCCESS) {
        SSH401A_TRACE(0, "stop_measurement failed");
        return SNDP_HAL_RET_FAIL;
    }
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_set_read_raw_data_callback(sndp_hal_hr_read_raw_data_callback callback)
{
    ssh401a_hr_read_raw_data_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_read_raw_data(void)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t ssh401a_set_calib_callback(sndp_hal_hr_calib_callback callback)
{
    ssh401a_hr_calib_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

int32_t ssh401a_exec_calib(uint8_t *data, uint16_t data_len)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t ssh401a_exec_self_calib(void)
{
    return SNDP_HAL_RET_FAIL;
}


const sndp_hal_hr_s sndp_hr_ssh401a = {
    .init                           = ssh401a_init,
    .enter_standby_mode             = ssh401a_enter_standby_mode,
    .enter_detection_mode           = ssh401a_enter_detection_mode,
    .set_hr_measure_callback        = ssh401a_set_hr_measure_callback,
    .start_hr_measure               = ssh401a_start_hr_measure,
    .stop_hr_measure                = ssh401a_stop_hr_measure,
    .set_read_raw_data_callback     = ssh401a_set_read_raw_data_callback,
    .read_raw_data                  = ssh401a_read_raw_data,
    .set_calib_callback             = ssh401a_set_calib_callback,
    .exec_calib                     = ssh401a_exec_calib,
    .exec_self_calib                = ssh401a_exec_self_calib,

};


static int32_t ssh401a_set_wear_status_changed_callback(sndp_hal_wear_status_changed_callback callback)
{
    ssh401a_wear_status_changed_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

static int32_t ssh401a_get_curr_status(sndp_hal_wear_status_e *status)
{
    if(status) {
        *status = ssh401a_wear_status;
    }
    return SNDP_HAL_RET_OK;
}

static int32_t ssh401a_check_curr_status()
{
    return SNDP_HAL_RET_FAIL;
}

const sndp_hal_wear_detection_s sndp_wear_detection_ssh401a = {
    .init                   = ssh401a_init,
    .set_wear_status_changed_callback = ssh401a_set_wear_status_changed_callback,
    .get_curr_status        = ssh401a_get_curr_status,
    .check_curr_status      = ssh401a_check_curr_status,
    .enter_standby_mode     = ssh401a_enter_standby_mode,
    .enter_detection_mode   = ssh401a_enter_detection_mode,
    .set_calibration_send_data_func = NULL,
    .recv_calibration_data  = NULL,
    .exec_calibration_self_calib    = NULL,
    .read_chip_id           = NULL,
};

#endif	//__SNDP_GSENSOR_XXXX__

