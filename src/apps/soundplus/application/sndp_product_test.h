#ifndef __SNDP_PRODUCT_TEST_H__
#define __SNDP_PRODUCT_TEST_H__

#if defined(__SNDP_PRODUCT_TEST__)




#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_PT_TRACE__
#if defined(__SNDP_PT_TRACE__)
#define PT_TRACE(num, str, ...)            SNDP_TRACE(1 + num, "[PT] %s, " str, __func__, ##__VA_ARGS__)
#else
#define PT_TRACE(num, str, ...)
#endif


bool sndp_pt_is_in_test_mode(void);
void sndp_pt_set_test_mode(uint8_t mode);
uint8_t sndp_pt_get_test_mode(void);

uint8_t sndp_pt_get_test_mic_index(void);
void sndp_pt_set_test_mic_index(uint8_t index);

void sndp_pt_switch_wear_status_report(uint8_t onoff);
bool sndp_pt_is_wear_status_report(void);

uint8_t sndp_pt_read_hall_status(void);

void sndp_pt_switch_click_test(uint8_t onoff);
bool sndp_pt_click_test_is_opened(void);

bool sndp_pt_ir_test(uint8_t op_code, uint8_t *recv_data, uint8_t recv_len, uint8_t *rsp_data, uint8_t rsp_len);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PRODUCT_TEST__ */
#endif /* __SNDP_PRODUCT_TEST_H__ */


