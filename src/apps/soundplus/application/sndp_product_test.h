#ifndef __SNDP_PRODUCT_TEST_H__
#define __SNDP_PRODUCT_TEST_H__

#if defined(__SNDP_PRODUCT_TEST__)




#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_PT_TRACE__
#if defined(__SNDP_PT_TRACE__)
#define PT_TRACE(num, str, ...)            TRACE(1 + num, "[PT] %s, " str, __func__, ##__VA_ARGS__)
#else
#define PT_TRACE(num, str, ...)
#endif


typedef enum {
    TOUCH_TEST_OPCODE_TEST_INTPIN_STA   = 0x00,
    TOUCH_TEST_OPCODE_READ_INTPIN_STA   = 0x01,
    
    
} pt_touch_test_opcode_e;
    

typedef enum {
    IR_TEST_OPCODE_READ_CHIP_ID         = 0x00,
    IR_TEST_OPCODE_TEST_INTPIN_STA      = 0x01,
    IR_TEST_OPCODE_READ_PS_DATA         = 0x02,
    IR_TEST_OPCODE_READ_CALIB_DATA      = 0x03,
    IR_TEST_OPCODE_1P_CALIB_START       = 0x04, //单点校准开始。
    IR_TEST_OPCODE_1P_CALIB_PGA         = 0x05, //单点校准读底噪
    IR_TEST_OPCODE_3P_CALIB_START       = 0x06, //三点校准开始
    IR_TEST_OPCODE_3P_CALIB_CT_PGA      = 0x07, //三点校准读底噪
    IR_TEST_OPCODE_3P_CALIB_FP_PS       = 0x08, //三点校准读灰卡远离时的ps值
    IR_TEST_OPCODE_3P_CALIB_NP_PS       = 0x09, //三点校准读灰卡靠近时的ps值
    
} pt_ir_test_opcode_e;


bool sndp_pt_is_in_test_mode(void);
void sndp_pt_set_test_mode(uint8_t mode);
uint8_t sndp_pt_get_test_mode(void);

uint8_t sndp_pt_get_test_mic_index(void);
void sndp_pt_set_test_mic_index(uint8_t index);

void sndp_pt_touch_test(const uint8_t *data, uint16_t data_len, uint8_t *rsp_data, uint16_t *rsp_data_len);
void sndp_pt_ir_test(const uint8_t *data, uint16_t data_len, uint8_t *rsp_data, uint16_t *rsp_data_len);

void sndp_pt_switch_wear_status_report(uint8_t onoff);
bool sndp_pt_is_wear_status_report(void);

uint8_t sndp_pt_read_hall_status(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PRODUCT_TEST__ */
#endif /* __SNDP_PRODUCT_TEST_H__ */


