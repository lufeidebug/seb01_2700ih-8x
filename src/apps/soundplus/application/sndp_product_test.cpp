#if defined(__SNDP_PRODUCT_TEST__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_gpio.h"
#include "tgt_hardware.h"
#include "app_utils.h"
#include "cqueue.h"


#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_product_test.h"

/**************************************************************************************************
* Constant
**************************************************************************************************/



/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    uint8_t test_mode;
    
    uint8_t test_mic_index;

    bool wear_status_report;

    bool click_test_en;
    
} sndp_pt_ctx_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
extern "C" int32_t bt_sco_chain_bypass_tx_algo(uint32_t sel_ch);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_pt_ctx_s pt_ctx;


/**************************************************************************************************
* Function
**************************************************************************************************/
bool sndp_pt_is_in_test_mode(void)
{
    if(pt_ctx.test_mode > 0) {
        return true;
    }
    
	return false;
}

void sndp_pt_set_test_mode(uint8_t mode)
{
	pt_ctx.test_mode = mode;
}

uint8_t sndp_pt_get_test_mode(void)
{
	return pt_ctx.test_mode;
}


uint8_t sndp_pt_get_test_mic_index(void)
{
	return pt_ctx.test_mic_index;
}

void sndp_pt_set_test_mic_index(uint8_t index)
{
	PT_TRACE(1, "index=%d", index);
    pt_ctx.test_mic_index = index;
	bt_sco_chain_bypass_tx_algo(index);
}


void sndp_pt_switch_wear_status_report(uint8_t onoff)
{
    pt_ctx.wear_status_report = onoff ? true : false;
}

bool sndp_pt_is_wear_status_report(void)
{
    return pt_ctx.wear_status_report;
}

uint8_t sndp_pt_read_hall_status(void)
{
    if (app_hall_int_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
        return hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_hall_int_pin_cfg.pin);
    }
    
    return 0xFF;
}

void sndp_pt_switch_click_test(uint8_t onoff)
{
    pt_ctx.click_test_en = onoff ? true : false;
}

bool sndp_pt_click_test_is_opened(void)
{
    return pt_ctx.click_test_en;
}


int32_t sndp_pt_init(void)
{
	pt_ctx.test_mode = 0;
    pt_ctx.test_mic_index = 0;
    pt_ctx.wear_status_report = false;
    pt_ctx.click_test_en = false;

	PT_TRACE(0, "done.");
    return 0;
}

#endif	/* __SNDP_PRODUCT_TEST__ */


