#if defined(__SNDP_GSENSOR_DA217E__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_iomux.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"
#include "hal_i2c.h"

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_da217e_adapter.h"
#include "sndp_da217e_drv.h"
#include "sndp_i2c.h"
#if defined(__SNDP_GSENSOR_SUPPORT__)
#include "sndp_hal_acc.h"
#endif
#if defined(__SNDP_GESTURE_MGR__)
#include "sndp_hal_gesture.h"
#endif


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define DA217E_INT1_DEBOUNCE_REPEAT_MS            (5) //ms
#define DA217E_INT1_DEBOUNCE_DELAY_MS             (10) //ms

#define DA217E_INT2_DEBOUNCE_REPEAT_MS            (5) //ms
#define DA217E_INT2_DEBOUNCE_DELAY_MS             (10) //ms

        
#define DA217E_I2C_TYPE                          (SNDP_I2C_HW_TASK)
#define DA217E_I2C_ID                            (HAL_I2C_ID_2)


//#define __DA217E_READ_RAW_DATA_MODIS__

//#define __DA217E_IRQ_DEBOUNCE__


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/



/**************************************************************************************************
* Variable
**************************************************************************************************/
static bool da217e_inited = false;
#if defined(__SNDP_GSENSOR_SUPPORT__)
static sndp_hal_acc_read_raw_data_callback  da217e_acc_read_raw_data_cb_ptr = NULL;
#endif
#if defined(__SNDP_GESTURE_MGR__)
static sndp_hal_gesture_event_callback  da217e_gesture_event_cb_ptr = NULL;
#endif

#if defined(__DA217E_IRQ_DEBOUNCE__)
static void da217e_int1_debounce_handler(void const *param);
osTimerDef(DA217_INT1_DEBOUNCE_TIMER, da217e_int1_debounce_handler);
static osTimerId da217e_int1_debounce_timer = NULL;

static void da217e_int2_debounce_handler(void const *param);
osTimerDef(DA217_INT2_DEBOUNCE_TIMER, da217e_int2_debounce_handler);
static osTimerId da217e_int2_debounce_timer = NULL;

#endif


/**************************************************************************************************
* Function
**************************************************************************************************/
POSSIBLY_UNUSED static int32_t da217e_nv_read(uint8_t *ptrData, uint16_t num)
{
#if 0    
    int ret = sndp_da_read_field(SNDP_DA_FIELD_ACC_CALIB_DATA, ptrData, num, true);
    if(ret){
        DA217E_TRACE(1, "ret=%d", ret);
    }
#endif    
    return 0;
}

POSSIBLY_UNUSED static int32_t da217e_nv_write(uint8_t *ptrData, uint16_t num)
{
#if 0     
    int ret = sndp_da_write_field(SNDP_DA_FIELD_ACC_CALIB_DATA, ptrData, num, true);
    if(ret){
        DA217E_TRACE(1, "ret=%d", ret);
    }
#endif    
    return 0;
}


static bool da217e_i2c_init(void)
{   
    sndp_i2c_open(DA217E_I2C_TYPE, DA217E_I2C_ID);
    return true;
}

POSSIBLY_UNUSED static int32_t da217e_i2c_read(uint8_t slave_addr, uint8_t reg_addr, uint8_t *reg_value, uint8_t read_length)
{
    return (int32_t)sndp_i2c_read(DA217E_I2C_TYPE, DA217E_I2C_ID, slave_addr, &reg_addr, 1, reg_value, read_length);
}

POSSIBLY_UNUSED static int32_t da217e_i2c_write(uint8_t slave_addr, uint8_t reg_addr, uint8_t *regData, uint8_t num)
{
    uint8_t write_data[32];
    uint8_t write_len = 0;

    write_data[write_len++] = reg_addr;
    for(uint8_t i = 0; i < num && i < sizeof(write_data); i++) {
        write_data[write_len++] = regData[i];
    }
    return (int32_t)sndp_i2c_write(DA217E_I2C_TYPE, DA217E_I2C_ID, slave_addr, write_data, write_len);
}

POSSIBLY_UNUSED static int32_t da217e_i2c_write_byte(uint8_t slave_addr, uint8_t reg_addr, uint8_t reg_value)
{
    uint8_t write_data[2];
    uint8_t write_len = 0;

    write_data[write_len++] = reg_addr;
    write_data[write_len++] = reg_value;
    return (int32_t)sndp_i2c_write(DA217E_I2C_TYPE, DA217E_I2C_ID, slave_addr, write_data, write_len);
}

void da217e_delay_ms(unsigned int ms)
{
    osDelay(ms);
}

static void da217e_tap_event(uint8_t tap_cnt)
{
    DA217E_TRACE(1, "tap_cnt=%d", tap_cnt);
#if defined(__SNDP_GESTURE_MGR__)    
    if(da217e_gesture_event_cb_ptr) {
        if(tap_cnt <= 3) {
            da217e_gesture_event_cb_ptr((sndp_hal_gesture_event_e)(SNDP_HAL_GESTURE_EVENT_1_CLICK + tap_cnt - 1));
        }
    }
#endif    
}

static void da217e_read_fifo_cb(da217e_drv_acc_data_s *data, uint16_t cnt)
{
    //DA217E_TRACE(1, "cnt=%d", cnt);

    //DUMP16("%6d ", (int16_t *)data, cnt > 4 ? 4*3 : cnt *3);

#if defined(__SNDP_GSENSOR_SUPPORT__)    
    if(da217e_acc_read_raw_data_cb_ptr) {
        da217e_acc_read_raw_data_cb_ptr((sndp_hal_acc_data_s *)data, cnt);
    }
#endif    
}


static void da217e_deal_int1_data(void)
{
    da217e_drv_deal_tap_interruption();
}

static void da217e_deal_int2_data(void)
{
    da217e_drv_deal_fifo_interruption();
}

#if defined(__DA217E_IRQ_DEBOUNCE__) 
static void da217e_int1_debounce_handler(void const *param)
{
    da217e_deal_int1_data();
}

static void da217e_int1_debounce(void)
{
    DA217E_TRACE(0, "...");
    osTimerStop(da217e_int1_debounce_timer);
    osTimerStart(da217e_int1_debounce_timer, DA217E_INT1_DEBOUNCE_DELAY_MS);
}

static void da217e_int2_debounce_handler(void const *param)
{
    da217e_deal_int2_data();
}

static void da217e_int2_debounce(void)
{
    DA217E_TRACE(0, "...");
    osTimerStop(da217e_int2_debounce_timer);
    osTimerStart(da217e_int2_debounce_timer, DA217E_INT2_DEBOUNCE_DELAY_MS);
    
}
#endif

static void da217e_int1_irq_handler(enum HAL_GPIO_PIN_T pin)
{
#if defined(__DA217E_IRQ_DEBOUNCE__)   
    static uint32_t last_time = 0;
    uint32_t curr_time = hal_sys_timer_get();
    uint32_t passed_ticks = hal_timer_get_passed_ticks(curr_time, last_time);

    //DA217E_TRACE(1, "passed_ms=%d, repeat_ms=%d", TICKS_TO_MS(passed_ticks), DA217E_INT1_DEBOUNCE_REPEAT_MS);
    
    if(TICKS_TO_MS(passed_ticks) >= DA217E_INT1_DEBOUNCE_REPEAT_MS) {
        last_time = hal_sys_timer_get();
        sndp_call_func_in_app_thread((uint32_t)da217e_int1_debounce, 0, 0, 0);
    }
#else
    sndp_call_func_in_app_thread((uint32_t)da217e_deal_int1_data, 0, 0, 0);
#endif
}

static void da217e_int2_irq_handler(enum HAL_GPIO_PIN_T pin)
{
#if defined(__DA217E_IRQ_DEBOUNCE__)    
    static uint32_t last_time = 0;
    uint32_t curr_time = hal_sys_timer_get();
    uint32_t passed_ticks = hal_timer_get_passed_ticks(curr_time, last_time);

    //DA217E_TRACE(1, "passed_ms=%d, repeat_ms=%d", TICKS_TO_MS(passed_ticks), DA217E_INT1_DEBOUNCE_REPEAT_MS);
    
    if(TICKS_TO_MS(passed_ticks) >= DA217E_INT1_DEBOUNCE_REPEAT_MS) {
        last_time = hal_sys_timer_get();
        sndp_call_func_in_app_thread((uint32_t)da217e_int2_debounce, 0, 0, 0);
    }
#else
    sndp_call_func_in_app_thread((uint32_t)da217e_deal_int2_data, 0, 0, 0);
#endif
}

static void da217e_irq_init(void)
{
#if defined(__SSH401A_IRQ_DEBOUNCE__)    
    if (da217e_int1_debounce_timer == NULL) {
        da217e_int1_debounce_timer = osTimerCreate(osTimer(DA217_INT1_DEBOUNCE_TIMER), osTimerOnce, NULL);
        ASSERT(da217e_int1_debounce_timer != NULL, "%s, %d", __func__, __LINE__);
    }

    if (da217e_int2_debounce_timer == NULL) {
        da217e_int2_debounce_timer = osTimerCreate(osTimer(DA217_INT2_DEBOUNCE_TIMER), osTimerOnce, NULL);
        ASSERT(da217e_int2_debounce_timer != NULL, "%s, %d", __func__, __LINE__);
    }
#endif
    
    if(app_gsensor_int1_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
        struct HAL_GPIO_IRQ_CFG_T gpiocfg;
        
        gpiocfg.irq_enable = true;
        gpiocfg.irq_debounce = true;
        gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;
        gpiocfg.irq_handler = da217e_int1_irq_handler;
        gpiocfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
        hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_gsensor_int1_pin_cfg.pin, &gpiocfg);
    }

    if(app_gsensor_int2_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
        struct HAL_GPIO_IRQ_CFG_T gpiocfg;
        
        gpiocfg.irq_enable = true;
        gpiocfg.irq_debounce = true;
        gpiocfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_HIGH_RISING;
        gpiocfg.irq_handler = da217e_int2_irq_handler;
        gpiocfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
        hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)app_gsensor_int2_pin_cfg.pin, &gpiocfg);
    }
}


int32_t da217e_init(void)
{
    if(da217e_inited) {
        DA217E_TRACE(0, "inited, rtn");
        return SNDP_HAL_RET_OK;
    }

    da217e_i2c_init();
    
    da217e_drv_if_s drv_if;
    drv_if.i2c_write_byte = da217e_i2c_write_byte;
    drv_if.i2c_read = da217e_i2c_read;
    drv_if.delay_ms = da217e_delay_ms;
    drv_if.tap_event_cb = da217e_tap_event;
    drv_if.read_fifo_cb = da217e_read_fifo_cb;
        
    if(da217e_drv_init(&drv_if)) {
        DA217E_TRACE(0, "fail.");
        return SNDP_HAL_RET_FAIL;
    } 

    da217e_irq_init();

    da217e_inited = true;
    DA217E_TRACE(0, "done.");
    
    return SNDP_HAL_RET_OK;
}

int32_t da217e_enter_standby_mode(void)
{
    DA217E_TRACE(0, "...");
    da217e_set_enable(0);
    return SNDP_HAL_RET_OK;
}


int32_t da217e_enter_detection_mode(void)
{
    DA217E_TRACE(0, "...");
    da217e_set_enable(1);
    return SNDP_HAL_RET_OK;
}


#if defined(__SNDP_GSENSOR_SUPPORT__)
int32_t da217e_set_reading_raw_data_callback(sndp_hal_acc_read_raw_data_callback callback)
{
    DA217E_TRACE(0, "%d", (uint32_t)callback);
    da217e_acc_read_raw_data_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

#if defined(__DA217E_READ_RAW_DATA_MODIS__)
void da217e_read_raw_data_test(void) 
{
    sndp_hal_acc_data_s acc_data[125] = {0};

    if(da217e_acc_read_raw_data_cb_ptr) {
        DA217E_TRACE(0, "callback");
        da217e_acc_read_raw_data_cb_ptr(acc_data, 125);
    } else {
        DA217E_TRACE(0, "null");
    }

    sndp_delay_exec_start(1000, (uint32_t)da217e_read_raw_data_test, 0, 0, 0);
}
#endif

int32_t da217e_start_reading_raw_data(void)
{
    DA217E_TRACE(0, "...");
    
#if defined(__DA217E_READ_RAW_DATA_MODIS__)
    sndp_delay_exec_start(1000, (uint32_t)da217e_read_raw_data_test, 0, 0, 0);
#else
    da217e_open_fifo_watermark_int(25);
#endif

    return SNDP_HAL_RET_OK;
}

int32_t da217e_stop_reading_raw_data(void)
{
    DA217E_TRACE(0, "...");
    
#if defined(__DA217E_READ_RAW_DATA_MODIS__)
    sndp_delay_exec_stop((uint32_t)da217e_read_raw_data_test);
#else
    da217e_close_fifo_int();
#endif

    return SNDP_HAL_RET_OK;
}

int32_t da217e_set_calibration_rsp_func(sndp_hal_acc_calibration_rsp_func func)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t da217e_recv_calibration_data(uint8_t *data, uint16_t data_len)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t da217e_exec_calibration_self_calib(void)
{
    return SNDP_HAL_RET_FAIL;
}

int32_t da217e_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
	return da217e_reg_write(reg_addr, reg_val);
}

int32_t da217e_read_reg(uint8_t reg_addr, uint8_t *read_buf, uint8_t read_len)
{
    return da217e_reg_read_data(reg_addr, read_buf, read_len);
}


extern "C" const sndp_hal_acc_s sndp_acc_da217e = {
    .init                           = da217e_init,
    .enter_standby_mode             = da217e_enter_standby_mode,
    .enter_detection_mode           = da217e_enter_detection_mode,
    .set_reading_raw_data_callback     = da217e_set_reading_raw_data_callback,
    .start_reading_raw_data            = da217e_start_reading_raw_data,
    .stop_reading_raw_data             = da217e_stop_reading_raw_data,
    .set_calibration_rsp_func       = da217e_set_calibration_rsp_func,
    .recv_calibration_data          = da217e_recv_calibration_data,
    .exec_calibration_self_calib    = da217e_exec_calibration_self_calib,
    .write_reg                      = da217e_write_reg,
    .read_reg                       = da217e_read_reg,
};

#endif


#if defined(__SNDP_GESTURE_MGR__)
int32_t da217e_set_event_callback(sndp_hal_gesture_event_callback callback)
{
    da217e_gesture_event_cb_ptr = callback;
    return SNDP_HAL_RET_OK;
}

extern "C" const sndp_hal_gesture_s sndp_gesture_da217e = {
    .init                           = da217e_init,
    .set_event_callback             = da217e_set_event_callback,
    .enter_standby_mode             = da217e_enter_standby_mode,
    .enter_detection_mode           = da217e_enter_detection_mode,
};
#endif


#endif	//__SNDP_GSENSOR_DA217E__
