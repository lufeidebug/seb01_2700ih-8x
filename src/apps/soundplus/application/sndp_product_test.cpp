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
#include "sndp_comm_cmd.h"
#if defined(__SNDP_PSENSOR_JSA1227__)
#include "sndp_jsa1227.h"
#endif

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


void sndp_pt_touch_test(const uint8_t *data, uint16_t data_len, uint8_t *rsp_data, uint16_t *rsp_data_len)
{
    uint16_t rsp_len = 0;
    uint8_t opCode;

    if(data == NULL || rsp_data == NULL || rsp_data_len == NULL) {
        return;
    }

    *rsp_data_len = 0;
    opCode = data[0];
    rsp_data[rsp_len++] = opCode;

    switch(opCode) {
        case TOUCH_TEST_OPCODE_TEST_INTPIN_STA:
            break;
    
        case TOUCH_TEST_OPCODE_READ_INTPIN_STA:
            rsp_data[rsp_len++] = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)app_touch_status_pin_cfg.pin);
            break;
    }

    PT_TRACE(0, "opCode=%d, rsp_len=%d", opCode, rsp_len);

    *rsp_data_len = rsp_len;
}


void sndp_pt_ir_test(const uint8_t *data, uint16_t data_len, uint8_t *rsp_data, uint16_t *rsp_data_len)
{
    uint16_t rsp_len = 0;
    uint8_t opCode;

    *rsp_data_len = 0;
    
    if(data == NULL || rsp_data == NULL || rsp_data_len == NULL) {
        return;
    }

    *rsp_data_len = 0;
    opCode = data[0];
    rsp_data[rsp_len++] = opCode;
    
#if defined(__SNDP_PSENSOR_JSA1227__)
    switch(data[0]) {
        case IR_TEST_OPCODE_READ_CHIP_ID:
            {
                uint16_t chip_id = jsa1227_get_chip_id();
                rsp_data[rsp_len++] = (uint8_t)((chip_id>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(chip_id&0xff);
            }
            break;
    
        case IR_TEST_OPCODE_TEST_INTPIN_STA:
            rsp_data[rsp_len++] = jsa1227_int_check();
            break;
            
        case IR_TEST_OPCODE_READ_PS_DATA:
            {
                uint16_t ps = jsa1227_get_ps_data();
                rsp_data[rsp_len++] = (uint8_t)((ps>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(ps&0xff);
            }
            break;

        case IR_TEST_OPCODE_READ_CALIB_DATA:
            {
                jsa1227_calib_param_s calib_para;
                
                jsa1227_load_calib_param(&calib_para);
                
                rsp_data[rsp_len++] = calib_para.flag;
                
                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_pga_value>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_pga_value&0xff);

                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_low_threshold>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_low_threshold&0xff);
                
                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_high_threshold>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_high_threshold&0xff);
                
            }
            break;

        case IR_TEST_OPCODE_1P_CALIB_START:
            
            break;

        case IR_TEST_OPCODE_1P_CALIB_PGA:
            {
                jsa1227_calib_param_s calib_para;

                jsa1227_calib_auto(true);
                jsa1227_load_calib_param(&calib_para);

                rsp_data[rsp_len++] = calib_para.flag;
                
                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_pga_value>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_pga_value&0xff);
                
                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_high_threshold>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_high_threshold&0xff);
                
                rsp_data[rsp_len++] = (uint8_t)((calib_para.ps_low_threshold>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(calib_para.ps_low_threshold&0xff);
            }
            break;

        case IR_TEST_OPCODE_3P_CALIB_START:
            break;

        case IR_TEST_OPCODE_3P_CALIB_CT_PGA:
            {
                uint16_t val = jsa1227_calib_pga();
                rsp_data[rsp_len++] = (uint8_t)((val>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(val&0xff);
            }
            break;
        
        case IR_TEST_OPCODE_3P_CALIB_FP_PS:
            {
                uint16_t val = jsa1227_calib_lth();
                rsp_data[rsp_len++] = (uint8_t)((val>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(val&0xff);
            }
            break;

        case IR_TEST_OPCODE_3P_CALIB_NP_PS:
            {
                uint16_t val = jsa1227_calib_hth();
                rsp_data[rsp_len++] = (uint8_t)((val>>8)&0xff);
                rsp_data[rsp_len++] = (uint8_t)(val&0xff);

                jsa1227_calib_save_result();
            }
            break;

    }
#endif

    PT_TRACE(0, "opCode=%d, rsp_len=%d", opCode, rsp_len);
    *rsp_data_len = rsp_len;
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

int32_t sndp_pt_init(void)
{
	pt_ctx.test_mode = 0;
    pt_ctx.test_mic_index = 0;

	PT_TRACE(0, "done.");
    return 0;
}

#endif	/* __SNDP_PRODUCT_TEST__ */


