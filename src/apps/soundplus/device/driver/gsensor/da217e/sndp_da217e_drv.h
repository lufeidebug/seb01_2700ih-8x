
#ifndef __SPDRV_DA217E_DRV_H__
#define __SPDRV_DA217E_DRV_H__
#if defined(__SNDP_GSENSOR_DA217E__)

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {

    int32_t (* i2c_write_byte)(uint8_t slave_addr, uint8_t reg_addr, uint8_t reg_value);

    int32_t (* i2c_read)(uint8_t slave_addr, uint8_t reg_addr, uint8_t *reg_value, uint8_t read_length);
    
    void (* delay_ms)(uint32_t ms);

    void (* tap_event_cb)(uint8_t tap_cnt);
    

} da217e_drv_if_s;


typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
} da217e_drv_acc_data_s;


int32_t da217e_set_enable(uint8_t enable);

int32_t da217e_open_double_tap_interrupt(uint8_t th);
int32_t da217e_close_double_tap_interrupt(void);
int32_t da217e_open_single_tap_interrupt(uint8_t th);
int32_t da217e_close_single_tap_interrupt(void);

int32_t da217e_open_fifo(void);
int32_t da217e_read_fifo(int16_t *x, int16_t *y, int16_t *z);
int32_t da217e_open_full_fifo_int(void);
int32_t da217e_open_fifo_watermark_int(uint8_t num);
int32_t da217e_close_fifo_int(void);
int32_t da217e_read_full_int_fifo(short *x, short *y, short *z);
int32_t da217e_read_water_int_fifo(short *x, short *y, short *z);

void da217e_drv_deal_tap_interruption(void);
void da217e_drv_deal_fifo_interruption(void);
int32_t da217e_drv_init(da217e_drv_if_s * drv_if);



#ifdef __cplusplus
}
#endif

#endif  //__SNDP_GSENSOR_DA217E__
#endif  //__SPDRV_DA217E_DRV_H__


