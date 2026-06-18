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


#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_gesture.h"
#include "sndp_da217e_adapter.h"
#include "sndp_da217e_drv.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/



#define DA217E_REG_SPI_CONFIG               0x00
#define DA217E_REG_CHIP_ID                  0x01
#define DA217E_REG_ACC_X_LSB                0x02
#define DA217E_REG_ACC_X_MSB                0x03
#define DA217E_REG_ACC_Y_LSB                0x04
#define DA217E_REG_ACC_Y_MSB                0x05
#define DA217E_REG_ACC_Z_LSB                0x06
#define DA217E_REG_ACC_Z_MSB                0x07 
#define DA217E_REG_FIFO_STATUS              0x08
#define DA217E_REG_MOTION_FLAG              0x09
#define DA217E_REG_TAP_ACTIVE_STATUS        0x0B
#define DA217E_REG_ORIENT_STATUS            0x0C
#define DA217E_REG_STEPS_MSB                0x0D
#define DA217E_REG_STEPS_LSB                0x0E
#define DA217E_REG_RESOLUTION_RANGE         0x0F
#define DA217E_REG_ODR_AXIS                 0x10
#define DA217E_REG_MODE_BW                  0x11
#define DA217E_REG_SWAP_POLARITY            0x12
#define DA217E_REG_FIFO_CTRL                0x14
#define DA217E_REG_INT_SET0				    0x15
#define DA217E_REG_INT_SET1                 0x16
#define DA217E_REG_INT_SET2                 0x17
#define DA217E_REG_INT_MAP1                 0x19
#define DA217E_REG_INT_MAP2                 0x1A
#define DA217E_REG_INT_MAP3                 0x1B
#define DA217E_REG_INT_CONFIG               0x20
#define DA217E_REG_INT_LATCH                0x21
#define DA217E_REG_FREEFALL_DUR             0x22
#define DA217E_REG_FREEFALL_THS             0x23
#define DA217E_REG_FREEFALL_HYST            0x24
#define DA217E_REG_ACTIVE_DUR               0x27
#define DA217E_REG_ACTIVE_THS               0x28
#define DA217E_REG_TAP_DUR                  0x2A
#define DA217E_REG_TAP_THS                  0x2B
#define DA217E_REG_ORIENT_HYST              0x2C
#define DA217E_REG_Z_BLOCK                  0x2D
#define DA217E_REG_RESET_STEP               0x2E
#define DA217E_REG_STEP_FILTER				0x33
#define DA217E_REG_SM_THRESHOLD             0x34

#define DA217E_REG_ENGINEERING_MODE         0x7F
#define DA217E_REG_SENS_COMP                0x8C


#define DA217E_I2C_SLAVE_ADDR				0x26
#define DA217E_CHIP_ID				        0x13
    
#define DA217E_TAP_THRESHOLD_DEFAULT        0x05
#define DA217E_TAP_THRESHOLD_BOOST        0x07
/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/



/**************************************************************************************************
* Variable
**************************************************************************************************/
static da217e_drv_if_s da217e_drv_if;
static uint8_t da217e_tap_cnt = 0;

static void da217_tap_timer_handler(void const *param);
osTimerDef(DA217_TAP_TIMER, da217_tap_timer_handler);
static osTimerId da217_tap_timer = NULL;
static uint32_t da217e_acc_samples_count = 0;

/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t da217e_reg_write(uint8_t reg, uint8_t val)
{
    if(da217e_drv_if.i2c_read) {
        return da217e_drv_if.i2c_write_byte(DA217E_I2C_SLAVE_ADDR, reg, val);
    }

    return -1;
}

int32_t da217e_reg_read(uint8_t reg, uint8_t *data)
{
    if(da217e_drv_if.i2c_read) {
        return da217e_drv_if.i2c_read(DA217E_I2C_SLAVE_ADDR, reg, data, 1);
    }

    return -1;
}

int32_t da217e_reg_read_data(uint8_t reg, uint8_t *data, uint8_t len)
{
    if(da217e_drv_if.i2c_read) {
        return da217e_drv_if.i2c_read(DA217E_I2C_SLAVE_ADDR, reg, data, len);
    }

    return -1;
}


static int32_t da217e_reg_mask_write(uint8_t addr, uint8_t mask, uint8_t data)
{
    int32_t ret;
    uint8_t tmp_data;

    ret = da217e_reg_read(addr, &tmp_data);
    tmp_data &= ~mask;
    tmp_data |= data & mask;
    ret |= da217e_reg_write(addr, tmp_data);
   
    return ret;
}


POSSIBLY_UNUSED static void da217e_drv_delay_ms(uint32_t ms)
{
    if(da217e_drv_if.delay_ms) {
        da217e_drv_if.delay_ms(ms);
    }
}

int32_t da217e_set_enable(uint8_t enable)
{
	int32_t ret;
    
	if(enable)
		ret = da217e_reg_write(DA217E_REG_MODE_BW, 0x10);
	else	
		ret = da217e_reg_write(DA217E_REG_MODE_BW, 0x80);
	
	return ret;	
}

//Read three axis data, 1024 LSB = 1 g
int32_t da217e_read_acc_data(int16_t *x, int16_t *y, int16_t *z)
{
    int32_t ret = 0;
    uint8_t tmp_data[6] = {0};

#if 1
    ret |= da217e_reg_read_data(DA217E_REG_ACC_X_LSB, tmp_data,6);
#else
	ret |= da217e_reg_read(DA217E_REG_ACC_X_LSB, &tmp_data[0]);
	ret |= da217e_reg_read(DA217E_REG_ACC_X_MSB, &tmp_data[1]);
	ret |= da217e_reg_read(DA217E_REG_ACC_Y_LSB, &tmp_data[2]);
	ret |= da217e_reg_read(DA217E_REG_ACC_Y_MSB, &tmp_data[3]);
	ret |= da217e_reg_read(DA217E_REG_ACC_Z_LSB, &tmp_data[4]);
	ret |= da217e_reg_read(DA217E_REG_ACC_Z_MSB, &tmp_data[5]);
 #endif
	
    *x = ((int16_t)(tmp_data[1] << 8 | tmp_data[0]))>> 3;
    *y = ((int16_t)(tmp_data[3] << 8 | tmp_data[2]))>> 3;
    *z = ((int16_t)(tmp_data[5] << 8 | tmp_data[4]))>> 3;	
	
    return ret;
}

//open double tap interrupt 0x05-0x1f
int32_t da217e_open_double_tap_interrupt(uint8_t th)
{
	int32_t ret = 0;

	ret |= da217e_reg_write(DA217E_REG_TAP_DUR, 0x46);
	ret |= da217e_reg_write(DA217E_REG_TAP_THS, th); //0x05-0x1f
	ret |= da217e_reg_write(DA217E_REG_INT_MAP1, 0x10);
	ret |= da217e_reg_write(DA217E_REG_INT_LATCH, 0xEE);  //latch 100ms
	ret |= da217e_reg_write(DA217E_REG_INT_SET1, 0x10);

	return ret;
}

//close double tap interrupt
int32_t da217e_close_double_tap_interrupt(void)
{
	int32_t ret = 0;

	ret |= da217e_reg_mask_write(DA217E_REG_INT_SET1, 0x10, 0x00);
	ret |= da217e_reg_mask_write(DA217E_REG_INT_MAP1, 0x10, 0x00);

	return ret;
}

//open single tap interrupt 0x05-0x1f
static bool da217e_single_tap_int_enabled = false;
int32_t da217e_open_single_tap_interrupt(uint8_t th)
{
	int32_t ret = 0;

	ret |= da217e_reg_write(DA217E_REG_TAP_DUR, 0x00); //0x00改为00后，解决三击时只触发两次中断的问题。
	ret |= da217e_reg_write(DA217E_REG_TAP_THS, th); //0x05-0x1f
	ret |= da217e_reg_write(DA217E_REG_INT_MAP1, 0x20);
	ret |= da217e_reg_write(DA217E_REG_INT_LATCH, 0xEE);  //latch 100ms
	ret |= da217e_reg_write(DA217E_REG_INT_SET1, 0x20);

	return ret;
}

int32_t da217e_open_single_with_default_threshold(void)
{
    if(!da217e_single_tap_int_enabled){
        da217e_single_tap_int_enabled = true;
        return da217e_open_single_tap_interrupt(DA217E_TAP_THRESHOLD_DEFAULT);
    } else {
        DA217E_TRACE(0, "single tap int already enabled");
        return 0;
    }
}

//close double tap interrupt
int32_t da217e_close_single_tap_interrupt(void)
{
	int32_t ret = 0;

    if(da217e_single_tap_int_enabled){
        da217e_single_tap_int_enabled = false;
    } else {
        DA217E_TRACE(0, "single tap int already disabled");
        return 0;
    }
	ret |= da217e_reg_mask_write(DA217E_REG_INT_SET1, 0x20, 0x00);
	ret |= da217e_reg_mask_write(DA217E_REG_INT_MAP1, 0x20, 0x00);

	return ret;
}

int32_t da217e_open_fifo(void)
{
	int32_t ret = 0;
	
	ret = da217e_reg_write(DA217E_REG_FIFO_CTRL, 0x80);
	
	return ret;
}


int32_t da217e_read_fifo(da217e_drv_acc_data_s *data)
{
	int32_t ret = 0;
    uint8_t tmp_data = 0;
    uint8_t i;
	
	ret |= da217e_reg_read(DA217E_REG_FIFO_STATUS, &tmp_data);
	if(tmp_data&0x40) {
        tmp_data = 32;
	} else {
        tmp_data &= 0x3f;
	}	
	
	for(i = 0; i < tmp_data; i++) {
	 	ret |= da217e_read_acc_data(&data[i].ax, &data[i].ay, &data[i].az);
	}
	
    return ret;
}

int32_t da217e_open_full_fifo_int(void)
{
	int32_t ret = 0;
	
	ret = da217e_reg_write(DA217E_REG_FIFO_CTRL, 0x80);
	ret = da217e_reg_write(DA217E_REG_INT_SET0, 0x04);
	ret = da217e_reg_write(DA217E_REG_INT_MAP2, 0x04);

	return ret;
}

int32_t da217e_read_full_int_fifo(da217e_drv_acc_data_s *data)
{
   int32_t i;
   uint8_t tmp_data = 0;  
   
   da217e_reg_read(DA217E_REG_FIFO_STATUS, &tmp_data);

   if((tmp_data & 0x40)) {
	   for(i = 0; i < 32; i++) {
			da217e_read_acc_data(&data[i].ax, &data[i].ay, &data[i].az);		
	   }
   }
   
    return 0;
}


//num 1-31
int32_t da217e_open_fifo_watermark_int(uint8_t num)
{
	int32_t ret = 0;
	
	ret |= da217e_reg_write(DA217E_REG_FIFO_CTRL, 0x80|num); //bit[7:6]=fifo_mode=10=stream mode
	ret |= da217e_reg_write(DA217E_REG_INT_SET0, 0x08); //bit[3]=wartermark_int_en=1
	ret |= da217e_reg_write(DA217E_REG_INT_MAP2, 0x40); //bit[6]=Int2_watermark=1

	return ret;
}

int32_t da217e_fifo_polling_data(da217e_drv_acc_data_s *data)
{
    uint8_t tmp_data = 0;
    da217e_reg_read(DA217E_REG_FIFO_STATUS, &tmp_data);
    // if(tmp_data&0x40) {
    //         tmp_data = 32;
    // } else {
    //         tmp_data &= 0x3f;
    // }
    tmp_data &= 0x3f;
    DA217E_TRACE(0, "fifo_status=%d", tmp_data &= 0x3f);
    // if(tmp_data >= 25) {
        for(int i = 0; i < tmp_data; i++){
            da217e_read_acc_data(&data[i].ax, &data[i].ay, &data[i].az);	
        }       
        return tmp_data;
    // }

    // return 0;
    
}

int32_t da217e_read_water_int_fifo(da217e_drv_acc_data_s *data)
{
   uint8_t i = 0, num = 0, tmp_data = 0;  
   
   da217e_reg_read(DA217E_REG_FIFO_CTRL, &num);
   num &= 0x3f;
   da217e_reg_read(DA217E_REG_FIFO_STATUS, &tmp_data);
   
    tmp_data &= 0x3f;

    for(i = 0; i < num; i++){
        da217e_read_acc_data(&data[i].ax, &data[i].ay, &data[i].az);	
    }

    return num;
}

int32_t da217e_close_fifo_int(void)
{
	int32_t ret = 0;

	ret = da217e_reg_write(DA217E_REG_FIFO_CTRL, 0x00);
	ret = da217e_reg_write(DA217E_REG_INT_SET0, 0x00);
	ret = da217e_reg_write(DA217E_REG_INT_MAP2, 0x00);

	return ret;
}


int32_t da217e_close_fifo(void)
{
	int32_t ret = 0;

	ret = da217e_reg_write(DA217E_REG_FIFO_CTRL, 0x00);

	return ret;
}

static void da217_tap_timer_handler(void const *param)
{
    da217e_reg_write(DA217E_REG_TAP_THS, DA217E_TAP_THRESHOLD_DEFAULT);
	if(da217e_drv_if.tap_event_cb) {
        da217e_drv_if.tap_event_cb(da217e_tap_cnt);
    }

    da217e_tap_cnt = 0;
}


void da217e_drv_deal_tap_interruption(void)
{
    uint8_t motion_flag = 0;
    uint8_t tap_staus = 0;
    uint8_t fifo_status = 0;

    da217e_reg_read(DA217E_REG_MOTION_FLAG, &motion_flag);
    da217e_reg_read(DA217E_REG_TAP_ACTIVE_STATUS, &tap_staus);
    da217e_reg_read(DA217E_REG_FIFO_STATUS, &fifo_status);
    DA217E_TRACE(1, "mflag=%02X, tstaus=%02X, fstatus=%02X", motion_flag, tap_staus, fifo_status);

    if(motion_flag&0x20) { //S_tap_int
#if 0
        if(tap_staus&0x10) {
            da217e_tap_cnt++;
            osTimerStop(da217_tap_timer);
            osTimerStart(da217_tap_timer, 600);
        }
#else
        da217e_tap_cnt++;
        if(1 == da217e_tap_cnt) {
            da217e_reg_write(DA217E_REG_TAP_THS, DA217E_TAP_THRESHOLD_BOOST);
        }
        osTimerStop(da217_tap_timer);
        osTimerStart(da217_tap_timer, 600);
#endif

    }
}

uint32_t da217e_get_acc_samples_count(void)
{
    return da217e_acc_samples_count;
}

void da217e_clear_acc_samples_count(void)
{
    da217e_acc_samples_count = 0;
}

void da217e_drv_deal_fifo_interruption(void)
{
    da217e_drv_acc_data_s data[32];
    int32_t num;
    
    num = da217e_read_water_int_fifo(data);
    if(num > 0) {
        da217e_acc_samples_count += num;
        if(da217e_drv_if.read_fifo_cb) {
            da217e_drv_if.read_fifo_cb(data, num);
        }
    }
}

void da217e_drv_deal_fifo_polling(void)
{
    da217e_drv_acc_data_s data[32];
    int32_t num;
    DA217E_TRACE(1, "enter %d",TICKS_TO_MS(hal_sys_timer_get()));
    num = da217e_fifo_polling_data(data);
    DA217E_TRACE(1, "exit%d",TICKS_TO_MS(hal_sys_timer_get()));
    // num = 30;
    if(num > 0) {
        if(da217e_drv_if.read_fifo_cb) {
            da217e_drv_if.read_fifo_cb(data, num);
        }
    }
}

int32_t da217e_read_chipid(uint8_t *chip_id)
{
    return da217e_reg_read(DA217E_REG_CHIP_ID, chip_id);
}

int32_t da217e_drv_init(da217e_drv_if_s * drv_if)
{
    int32_t ret = 0;
    uint8_t chip_id;
    uint8_t retry;
    
    ASSERT(drv_if != NULL, "%s, %d", __func__, __LINE__);

     if(da217_tap_timer == NULL) {
        da217_tap_timer = osTimerCreate (osTimer(DA217_TAP_TIMER), osTimerOnce, NULL);
        ASSERT(da217_tap_timer != NULL, "%s, da217_tap_timer == NULL", __func__);
	}
     
    memcpy(&da217e_drv_if, drv_if, sizeof(da217e_drv_if_s));

    retry = 0;
    while(retry < 5) {
        retry++;
        if(!da217e_reg_read(DA217E_REG_CHIP_ID, &chip_id)) {
            //DA217E_TRACE(0, "chipid=%02X", chip_id);
            if(chip_id == DA217E_CHIP_ID) {
                //DA217E_TRACE(0, "chipid match");
                break;
            }
        }
    } 
    
    if(retry >= 5) {
        DA217E_TRACE(0, "chipid not match, rtn");
        return -1;
    }

    da217e_reg_mask_write(0x00, 0x24, 0x24);
	da217e_drv_delay_ms(50); //delay 50ms
 
	//printf("------da217e chip id = %x-----\r\n",data_m); 

	ret |= da217e_reg_write(DA217E_REG_RESOLUTION_RANGE, 0x01);               //+/-4G,14bit
	ret |= da217e_reg_write(DA217E_REG_MODE_BW, 0x10);          //normal mode
	ret |= da217e_reg_write(DA217E_REG_ODR_AXIS, 0x07);      //ODR = 125hz

#if 0	
	//Engineering mode
	ret |= da217e_reg_write(DA217E_REG_ENGINEERING_MODE, 0x83);
	ret |= da217e_reg_write(DA217E_REG_ENGINEERING_MODE, 0x69);
	ret |= da217e_reg_write(DA217E_REG_ENGINEERING_MODE, 0xBD);


	//Reduce power consumption
	if(DA217E_I2C_SLAVE_ADDR == 0x26){
		da217e_reg_mask_write(DA217E_REG_SENS_COMP, 0x40, 0x00);
	}

	da217e_reg_mask_write(DA217E_REG_ENGINEERING_MODE, 0x02, 0x00);

#endif

	ret |= da217e_open_single_with_default_threshold(); //enalbe single tap and set tap threshold 0x05 - 0x1f   
	return ret;
}


#endif	//__SNDP_GSENSOR_DA217E__
