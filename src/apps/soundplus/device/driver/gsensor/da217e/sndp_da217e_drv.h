
#ifndef __SPDRV_DA217E_DRV_H__
#define __SPDRV_DA217E_DRV_H__
#if defined(__SNDP_GSENSOR_DA217E__)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
} da217e_drv_acc_data_s;


typedef struct {

    int32_t (* i2c_write_byte)(uint8_t slave_addr, uint8_t reg_addr, uint8_t reg_value);

    int32_t (* i2c_read)(uint8_t slave_addr, uint8_t reg_addr, uint8_t *reg_value, uint8_t read_length);
    
    void (* delay_ms)(uint32_t ms);

    void (* tap_event_cb)(uint8_t tap_cnt);

    void (* read_fifo_cb)(da217e_drv_acc_data_s *data, uint16_t cnt);
    
    void (* read_samples_rate)(uint16_t data);
} da217e_drv_if_s;


int32_t da217e_reg_write(uint8_t reg, uint8_t val);
int32_t da217e_reg_read(uint8_t reg, uint8_t *data);
int32_t da217e_reg_read_data(uint8_t reg, uint8_t *data, uint8_t len);

int32_t da217e_set_enable(uint8_t enable);
int32_t da217e_read_acc_data(int16_t *x, int16_t *y, int16_t *z);

int32_t da217e_open_double_tap_interrupt(uint8_t th);
int32_t da217e_close_double_tap_interrupt(void);
int32_t da217e_open_single_tap_interrupt(uint8_t th);
int32_t da217e_close_single_tap_interrupt(void);
int32_t da217e_open_single_with_default_threshold(void);

int32_t da217e_open_fifo(void);
int32_t da217e_read_fifo(da217e_drv_acc_data_s *data);

int32_t da217e_open_full_fifo_int(void);
int32_t da217e_read_full_int_fifo(da217e_drv_acc_data_s *data);

int32_t da217e_open_fifo_watermark_int(uint8_t num);
int32_t da217e_read_water_int_fifo(da217e_drv_acc_data_s *data);

int32_t da217e_close_fifo_int(void);

void da217e_drv_deal_tap_interruption(void);
void da217e_drv_deal_fifo_interruption(void);
void da217e_drv_deal_fifo_polling(void);
uint32_t da217e_get_acc_samples_count(void);
void da217e_clear_acc_samples_count(void);
int32_t da217e_close_fifo(void);
int32_t da217e_open_fifo(void);

int32_t da217e_read_chipid(uint8_t *chip_id);
int32_t da217e_drv_init(da217e_drv_if_s * drv_if);
void da217e_start_acc_samples_measurement(int duration_s);


#ifdef __cplusplus
}
#endif

#endif  //__SNDP_GSENSOR_DA217E__
#endif  //__SPDRV_DA217E_DRV_H__


