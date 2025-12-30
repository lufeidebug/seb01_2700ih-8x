#ifndef __SNDP_I2C_H__
#define __SNDP_I2C_H__

#if defined(__SNDP_BUS_HW_I2C__)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SNDP_I2C_GPIO,
	SNDP_I2C_HW_SIMPLE,
	SNDP_I2C_HW_TASK,

    SNDP_I2C_TYPE_CNT
} sndp_i2c_type_e;


uint32_t sndp_i2c_open(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id);
uint32_t sndp_i2c_close(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id);
uint32_t sndp_i2c_write(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id, uint16_t dev_addr, uint8_t *write_data, uint16_t write_len);
uint32_t sndp_i2c_read(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id, uint16_t dev_addr, uint8_t *write_data, uint16_t write_len, uint8_t *read_buf, uint16_t read_len);

#ifdef __cplusplus
}
#endif

#endif	//__SNDP_BUS_HW_I2C__
#endif	//__SNDP_I2C_H__

