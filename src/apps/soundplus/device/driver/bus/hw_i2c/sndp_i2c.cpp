#if defined(__SNDP_BUS_HW_I2C__)
#include "plat_types.h"
#include "stdint.h"
#include "string.h"
#include "hal_key.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_timer.h"
#include "hal_bootmode.h"
#include "apps.h"
#include "app_thread.h"

#include "sndp_i2c.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_I2C_CNT 				(4)

/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
osMutexId app_i2c_mutex_id[SNDP_I2C_CNT];
osMutexDef(app_i2c_0_mutex);
#if (SNDP_I2C_CNT > 1)
osMutexDef(app_i2c_1_mutex);
#endif
#if (SNDP_I2C_CNT > 2)
osMutexDef(app_i2c_2_mutex);
#endif
#if (SNDP_I2C_CNT > 3)
osMutexDef(app_i2c_3_mutex);
#endif
#if (SNDP_I2C_CNT > 4)
osMutexDef(app_i2c_4_mutex);
#endif
#if (SNDP_I2C_CNT > 5)
osMutexDef(app_i2c_5_mutex);
#endif


static struct HAL_I2C_CONFIG_T sndp_i2c_cfg[SNDP_I2C_CNT];
static struct HAL_GPIO_I2C_CONFIG_T sndp_gpio_i2c_cfg[SNDP_I2C_CNT];


/**************************************************************************************************
* Function
**************************************************************************************************/

void sndp_i2c_mutex_Wait(enum HAL_I2C_ID_T i2c_id)
{
	if(app_i2c_mutex_id[i2c_id]) {
		osMutexWait(app_i2c_mutex_id[i2c_id], osWaitForever);
	}
}

void sndp_i2c_mutex_release(enum HAL_I2C_ID_T i2c_id)
{
	if(app_i2c_mutex_id[i2c_id]) {
		osMutexRelease(app_i2c_mutex_id[i2c_id]);
	}
}


uint32_t sndp_i2c_open(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id)
{
    ASSERT(type < SNDP_I2C_TYPE_CNT, "%s, type(%d) >= max(%d)", __func__, type, SNDP_I2C_TYPE_CNT);
    ASSERT(i2c_id < SNDP_I2C_CNT, "%s, i2c_id(%d) >= max(%d)", __func__, i2c_id, SNDP_I2C_CNT);
        
	if(type == SNDP_I2C_HW_SIMPLE) {
	    if (i2c_id == HAL_I2C_ID_0) {
	        hal_iomux_set_i2c0();
	    }
#if (SNDP_I2C_CNT > 1)	
		else if (i2c_id == HAL_I2C_ID_1) {
	        hal_iomux_set_i2c1();
	    }
#endif
#if (SNDP_I2C_CNT > 2)	
		else if (i2c_id == HAL_I2C_ID_2) {
	        hal_iomux_set_i2c2();
	    }
#endif 
#if (SNDP_I2C_CNT > 3)	
        else if (i2c_id == HAL_I2C_ID_3) {
            hal_iomux_set_i2c3();
            sndp_i2c_cfg[i2c_id].mode = HAL_I2C_API_MODE_SIMPLE;
        }
#endif
#if (SNDP_I2C_CNT > 4)	
        else if (i2c_id == HAL_I2C_ID_4) {
            hal_iomux_set_i2c4();
        }
#endif
#if (SNDP_I2C_CNT > 5)	
        else if (i2c_id == HAL_I2C_ID_5) {
            hal_iomux_set_i2c5();
        }
#endif

        sndp_i2c_cfg[i2c_id].mode = HAL_I2C_API_MODE_SIMPLE;
        sndp_i2c_cfg[i2c_id].use_dma  = 0;
        sndp_i2c_cfg[i2c_id].use_sync = 1;
        sndp_i2c_cfg[i2c_id].speed = 400000;
        sndp_i2c_cfg[i2c_id].as_master = 1;
		hal_i2c_open(i2c_id, &sndp_i2c_cfg[i2c_id]);
		
	 }else if(type == SNDP_I2C_HW_TASK) {
	    if (i2c_id == HAL_I2C_ID_0) {
	        hal_iomux_set_i2c0();
	    }
#if (SNDP_I2C_CNT > 1)	
		else if (i2c_id == HAL_I2C_ID_1) {
	        hal_iomux_set_i2c1();
	    }
#endif
#if (SNDP_I2C_CNT > 2)	
		else if (i2c_id == HAL_I2C_ID_2) {
	        hal_iomux_set_i2c2();
	    }
#endif 
#if (SNDP_I2C_CNT > 3)	
        else if (i2c_id == HAL_I2C_ID_3) {
            hal_iomux_set_i2c3();
        }
#endif 
#if (SNDP_I2C_CNT > 4)	
        else if (i2c_id == HAL_I2C_ID_4) {
            hal_iomux_set_i2c4();
        }
#endif
#if (SNDP_I2C_CNT > 5)	
        else if (i2c_id == HAL_I2C_ID_5) {
            hal_iomux_set_i2c5();
        }
#endif

        sndp_i2c_cfg[i2c_id].mode = HAL_I2C_API_MODE_TASK;
        sndp_i2c_cfg[i2c_id].use_dma  = 0;
        sndp_i2c_cfg[i2c_id].use_sync = 1;
        sndp_i2c_cfg[i2c_id].speed = 400000;
        sndp_i2c_cfg[i2c_id].as_master = 1;
		hal_i2c_open(i2c_id, &sndp_i2c_cfg[i2c_id]);

	} else if(type == SNDP_I2C_GPIO){
		if (i2c_id == HAL_I2C_ID_0) {
#if (I2C0_IOMUX_INDEX == 4)
            sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P0_4;
	        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P0_5;
#else
	        sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P0_0;
	        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P0_1;
#endif            
	    }
#if (SNDP_I2C_CNT > 1)	
		else if (i2c_id == HAL_I2C_ID_1) {
	        sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P0_2;
	        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P0_3;
	    }   
#endif
#if (SNDP_I2C_CNT > 2)	
		else if (i2c_id == HAL_I2C_ID_2) {
#if (I2C2_IOMUX_INDEX == 34)
            sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P3_4;
	        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P3_5;
#else
            sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P0_4;
	        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P0_5;
#endif            
	        
	    }
#endif
#if (SNDP_I2C_CNT > 3)	
        else if (i2c_id == HAL_I2C_ID_3) {
#if (I2C3_IOMUX_INDEX == 36)
        sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P3_6;
        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P3_7;
#else
        sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P0_6;
        sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P0_7;
#endif            
        }   
#endif
#if (SNDP_I2C_CNT > 4)	
        else if (i2c_id == HAL_I2C_ID_4) {
            sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P1_0;
            sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P1_1;
        }
#endif
#if (SNDP_I2C_CNT > 5)	
        else if (i2c_id == HAL_I2C_ID_5) {
            sndp_gpio_i2c_cfg[i2c_id].scl  = HAL_GPIO_PIN_P2_0;
            sndp_gpio_i2c_cfg[i2c_id].sda  = HAL_GPIO_PIN_P2_1;
        }
#endif

        sndp_gpio_i2c_cfg[i2c_id].speed = 100000;
		hal_gpio_i2c_open(&sndp_gpio_i2c_cfg[i2c_id]);
	}


    if(app_i2c_mutex_id[0] == NULL) {
        app_i2c_mutex_id[0] = osMutexCreate((osMutex(app_i2c_0_mutex)));
    }

#if (SNDP_I2C_CNT > 1)
	if(app_i2c_mutex_id[1] == NULL) {
		app_i2c_mutex_id[1] = osMutexCreate((osMutex(app_i2c_1_mutex)));
	}
#endif

#if (SNDP_I2C_CNT > 2)
	if(app_i2c_mutex_id[2] == NULL) {
		app_i2c_mutex_id[2] = osMutexCreate((osMutex(app_i2c_2_mutex)));
	}
#endif
#if (SNDP_I2C_CNT > 3)
    if(app_i2c_mutex_id[3] == NULL) {
        app_i2c_mutex_id[3] = osMutexCreate((osMutex(app_i2c_3_mutex)));
    }
#endif
#if (SNDP_I2C_CNT > 4)
    if(app_i2c_mutex_id[4] == NULL) {
        app_i2c_mutex_id[4] = osMutexCreate((osMutex(app_i2c_4_mutex)));
    }
#endif
#if (SNDP_I2C_CNT > 4)
    if(app_i2c_mutex_id[5] == NULL) {
        app_i2c_mutex_id[5] = osMutexCreate((osMutex(app_i2c_5_mutex)));
    }
#endif


    return 0;
}

uint32_t sndp_i2c_close(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id)
{
	if(type == SNDP_I2C_GPIO) {
		hal_gpio_i2c_close(&sndp_gpio_i2c_cfg[i2c_id]);
	} else if(type == SNDP_I2C_HW_SIMPLE) {
		hal_i2c_close(i2c_id);
	} else if(type == SNDP_I2C_HW_TASK) {
		hal_i2c_close(i2c_id);
	}

    return 0;
}

uint32_t sndp_i2c_write(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id, uint16_t dev_addr, uint8_t *write_data, uint16_t write_len)
{
	uint32_t ret = 0;

	sndp_i2c_mutex_Wait(i2c_id);
	if(type == SNDP_I2C_GPIO) {
		ret = hal_gpio_i2c_simple_send(&sndp_gpio_i2c_cfg[i2c_id], dev_addr, write_data, write_len);
	} else if(type == SNDP_I2C_HW_SIMPLE) {
    	ret = hal_i2c_simple_send(i2c_id, dev_addr,  write_data, write_len);
	} else if(type == SNDP_I2C_HW_TASK) {
		ret = hal_i2c_task_send(i2c_id, dev_addr, write_data, write_len, 0, NULL);
	}

	sndp_i2c_mutex_release(i2c_id);

	return ret;
}

uint32_t sndp_i2c_read(sndp_i2c_type_e type, enum HAL_I2C_ID_T i2c_id, uint16_t dev_addr, uint8_t *write_data, uint16_t write_len, uint8_t *read_buf, uint16_t read_len)
{
	uint32_t ret = 0;
	
	sndp_i2c_mutex_Wait(i2c_id);

	if(type == SNDP_I2C_GPIO) {
		ret = hal_gpio_i2c_simple_recv(&sndp_gpio_i2c_cfg[i2c_id], dev_addr, write_data, write_len, read_buf, read_len);
	} if(type == SNDP_I2C_HW_SIMPLE) {
    	ret = hal_i2c_simple_recv(i2c_id, dev_addr, write_data, write_len, read_buf, read_len);
	} else if(type == SNDP_I2C_HW_TASK) {
		ret = hal_i2c_task_recv(i2c_id, dev_addr, write_data, write_len, read_buf, read_len, 0, NULL);
	}

	sndp_i2c_mutex_release(i2c_id);
	return ret;
}

#endif	//__SNDP_BUS_HW_I2C__

