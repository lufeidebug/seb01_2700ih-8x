#if defined(__SNDP_POGOPIN_DUAL_UART__)
#include "stdio.h"
#include "string.h"
#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "tgt_hardware.h"
#include "app_utils.h"
#include "cqueue.h"


#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_pogopin_comm.h"
#include "sndp_pogopin_dual_uart.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_POGOPIN_UART_TRACE__
        
#if defined(__SNDP_POGOPIN_UART_TRACE__)
#define PGP_UART_LOG_TAG				    "[PGP_DUAL_UART]"
#define PGP_UART_TRACE(num, str, ...)   	SNDP_TRACE(1 + num, PGP_UART_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#else
#define PGP_UART_TRACE(num, str, ...)
#endif


#define PGP_UART_RECV_BUF_SIZE             (128)
#define PGP_UART_SEND_BUF_SIZE             (128)

#define PGP_UART_BAUD                      (115200)

#define PGP_UART_DMA                       (1)
#define PGP_UART_IRQ                       (0)  

#define PGP_UART_READ_RETRY_CNT            (5)


typedef enum {
    RECV_BUF_PING,
    RECV_BUF_PANG,
    
    RECV_BUF_CNT
} pgp_uart_recv_buf_index_e;

typedef enum {
    UART_PIN_TYPE_UART,
    UART_PIN_TYPE_GPIO,
    
} pgp_uart_pin_type_e;


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct{
    bool inited;
    
    sndp_hal_pogopin_mode_e pogopin_mode;
    
    enum HAL_UART_ID_T uart_port; 
    uint32_t read_wait_time;  //us

    bool tx_working;
	bool rx_working;
    
} pgp_uart_ctx_s; 

typedef struct {
    uint16_t len;
    uint8_t data[PGP_UART_RECV_BUF_SIZE];
} pgp_uart_recv_item_s;

typedef struct {
    uint16_t len;
    uint8_t data[PGP_UART_SEND_BUF_SIZE];
} pgp_uart_send_item_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void pgp_uart_send_complete(void);
static void pgp_uart_send_hdlr(void);
static int32_t pgp_uart_set_mode(sndp_hal_pogopin_mode_e mode);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static struct HAL_UART_CFG_T pgp_uart_cfg = {
    HAL_UART_PARITY_NONE,
    HAL_UART_STOP_BITS_1,
    HAL_UART_DATA_BITS_8,
    HAL_UART_FLOW_CONTROL_NONE,
    HAL_UART_FIFO_LEVEL_1_2,
    HAL_UART_FIFO_LEVEL_1_2,
    PGP_UART_BAUD,
    true,
    true,
    false,
};

static pgp_uart_ctx_s pgp_uart_ctx;
static sndp_hal_pogopin_comm_data_recv_func pgp_comm_data_recv_ptr = NULL;

#if (PGP_UART_DMA)
static uint8_t pgp_uart_recv_pingpang_buf[RECV_BUF_CNT][PGP_UART_RECV_BUF_SIZE];
static uint8_t *p_dma_recv_buf = NULL;
#endif

static pgp_uart_recv_item_s pgp_uart_recv_push_item;
static pgp_uart_recv_item_s pgp_uart_recv_process_item;

static pgp_uart_send_item_s pgp_uart_send_push_item;
static pgp_uart_send_item_s pgp_uart_send_process_item;


static CQueue pgp_uart_recv_queue;
static pgp_uart_recv_item_s pgp_uart_recv_queue_buf[3];

static osMutexId pgp_uart_recv_queue_mutex_id = NULL;
osMutexDef(pgp_uart_recv_queue_mutex);

static CQueue pgp_uart_send_queue;
static pgp_uart_send_item_s pgp_uart_send_queue_buf[3];

static osMutexId pgp_uart_send_queue_mutex_id = NULL;
osMutexDef(pgp_uart_send_queue_mutex);


static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_rx_pin_gpio_cfg = {
    HW_PIN_POGOPIN_UART_RX, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_NOPULL,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_tx_pin_gpio_cfg = {
    HW_PIN_POGOPIN_UART_TX, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_NOPULL,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_rx_pin_uart_cfg = {
    HW_PIN_POGOPIN_UART_RX, HAL_IOMUX_FUNC_MCU_UART1_RX, HAL_IOMUX_PIN_VOLTAGE_MEM, HAL_IOMUX_PIN_PULLUP_ENABLE,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_tx_pin_uart_cfg = {
    HW_PIN_POGOPIN_UART_TX, HAL_IOMUX_FUNC_MCU_UART1_TX, HAL_IOMUX_PIN_VOLTAGE_MEM, HAL_IOMUX_PIN_NOPULL,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_comm_en_pin_cfg = {
    HW_PIN_POGOPIN_UART_COMM_EN, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENABLE
};


/**************************************************************************************************
* Function
**************************************************************************************************/
static int pgp_uart_send_queue_push_data(pgp_uart_send_item_s *item)
{
    int ret;
    
    osMutexWait(pgp_uart_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&pgp_uart_send_queue, (CQItemType *)item, sizeof(pgp_uart_send_item_s));
    osMutexRelease(pgp_uart_send_queue_mutex_id);
    return ret;
}

static int pgp_uart_send_queue_pop_data(pgp_uart_send_item_s *item)
{
    int ret;
    
    osMutexWait(pgp_uart_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&pgp_uart_send_queue, (CQItemType *)item, sizeof(pgp_uart_send_item_s));
    osMutexRelease(pgp_uart_send_queue_mutex_id);

    return ret;
}

static int pgp_uart_send_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(pgp_uart_send_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&pgp_uart_send_queue);
    osMutexRelease(pgp_uart_send_queue_mutex_id);

    queue_len = queue_len / sizeof(pgp_uart_send_item_s);
    return queue_len;
}

static int pgp_uart_recv_queue_push_data(pgp_uart_recv_item_s *item)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(pgp_uart_recv_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&pgp_uart_recv_queue, (CQItemType *)item, sizeof(pgp_uart_recv_item_s));
    osMutexRelease(pgp_uart_recv_queue_mutex_id);
    return ret;
}

static int pgp_uart_recv_queue_pop_data(pgp_uart_recv_item_s *item)
{
    int ret;

    if(item == NULL)
        return -1;
    
    osMutexWait(pgp_uart_recv_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&pgp_uart_recv_queue, (CQItemType *)item, sizeof(pgp_uart_recv_item_s));
    osMutexRelease(pgp_uart_recv_queue_mutex_id);

    return ret;
}

static int pgp_uart_recv_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(pgp_uart_recv_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&pgp_uart_recv_queue);
    osMutexRelease(pgp_uart_recv_queue_mutex_id);

    queue_len = queue_len / sizeof(pgp_uart_recv_item_s);
    return queue_len;
}


POSSIBLY_UNUSED static void pgp_uart_recv_data(void)
{
    pgp_uart_recv_item_s *p_recv_item = &pgp_uart_recv_process_item;
    
    memset(p_recv_item, 0, sizeof(pgp_uart_recv_item_s));
    if(pgp_uart_recv_queue_pop_data(p_recv_item) == 0) {
        pgp_uart_ctx.rx_working = true;
        
        PGP_UART_TRACE(0, ":");
        DUMP8("%02X ", p_recv_item->data, p_recv_item->len);
    
        if(pgp_comm_data_recv_ptr) {
            pgp_comm_data_recv_ptr(p_recv_item->data, p_recv_item->len);
        }

        if(pgp_uart_recv_queue_get_len() > 0) {
            sndp_delay_exec_start(10, (uint32_t) pgp_uart_recv_data, 0, 0, 0);
        } else {
            pgp_uart_ctx.rx_working = false;
        }
    } else {
        pgp_uart_ctx.rx_working = false;
    }

}


#if (PGP_UART_IRQ)
static void pgp_uart_irq_rx_handler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status)
{
    static uint32_t last_time = 0;
    uint32_t curr_time = 0;
    uint8_t rev_char = 0;
    uint8_t read_retry;
    pgp_uart_recv_item_s *p_recv_item = &pgp_uart_recv_push_item;

	//PGP_UART_TRACE(0, "enter");

    if(status.RX || status.RT){
        curr_time = hal_sys_timer_get();
        if(TICKS_TO_MS(curr_time - last_time) > 20){
			last_time = curr_time;
			
            p_recv_item->len = 0;
        }

        read_retry = 0;
        while(1) {
            if(hal_uart_readable(pgp_uart_ctx.uart_port)) {
                rev_char = hal_uart_getc(pgp_uart_ctx.uart_port);            
    			p_recv_item->data[p_recv_item->len++] = rev_char;
                read_retry = 0;
            } else {
                read_retry++;
            }

            if(read_retry >= 5) {
                break;
            }
            
            hal_sys_timer_delay_us(pgp_uart_ctx.read_wait_time);
        } 
		
		hal_uart_flush(pgp_uart_ctx.uart_port, 4);   
        
		PGP_UART_TRACE(1, "recv_data_len=%d", p_recv_item->len);
        
        pgp_uart_recv_queue_push_data(p_recv_item);
        p_recv_item->len = 0;
		if(!pgp_uart_ctx.rx_working){
	  		sndp_call_func_in_dev_thread((uint32_t)pgp_uart_recv_data, 0, 0, 0);
		}
             
    }else {
        PGP_UART_TRACE(6, "status RX:%d, TX:%d, FE:%d, PE:%d, BE:%d, OE:%d",
            	status.RX, status.TX, status.FE, status.PE, status.BE, status.OE);
    }
}

static void pgp_uart_irq_rx_start(void)
{
    union HAL_UART_IRQ_T mask;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_uart_ctx.uart_port, 0);
    mask.reg = 0;
    mask.BE = 1;
    mask.RT = 1;
    mask.RX = 1;
    hal_uart_irq_set_mask(pgp_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_uart_irq_rx_stop(void)
{
	PGP_UART_TRACE(0, "enter");
	
    union HAL_UART_IRQ_T mask;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_uart_ctx.uart_port, 0);    
    mask.reg = 0;
    hal_uart_irq_set_mask(pgp_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_uart_irq_tx(uint8_t *data, uint32_t data_len)
{
	PGP_UART_TRACE(1, "data_len=%d", data_len);
	DUMP8("%02X ", data, data_len);
    
	hal_uart_flush(pgp_uart_ctx.uart_port, 0);
	for(uint32_t i = 0; i < data_len; i++) {
		//osDelay(5);
        hal_uart_blocked_putc(pgp_uart_ctx.uart_port, data[i]);
		//osDelay(5);
		//hal_uart_flush(pgp_uart_ctx.uart_port, 0);
		while (!hal_uart_get_flag(pgp_uart_ctx.uart_port).TXFE || hal_uart_get_flag(pgp_uart_ctx.uart_port).BUSY){
		    osThreadYield();
		};
    }

    sndp_delay_exec_start(20, (uint32_t) pgp_uart_send_complete, 0, 0, 0);
}

static void pgp_uart_irq_disable(void)
{
	pgp_uart_irq_rx_stop();
	hal_uart_irq_set_handler(pgp_uart_ctx.uart_port, NULL);
}

static void pgp_uart_irq_enable(void)
{
	hal_uart_irq_set_handler(pgp_uart_ctx.uart_port, pgp_uart_irq_rx_handler);
}
#endif

#if (PGP_UART_DMA)
static void pgp_uart_dma_rx_start(void)
{
    uint32_t lock;
    union HAL_UART_IRQ_T mask;
    
    mask.reg = 0;
    mask.BE = 1;
    mask.FE = 1;
    mask.OE = 1;
    mask.PE = 1;
    mask.RT = 1;

    lock = int_lock();

    if(p_dma_recv_buf == &pgp_uart_recv_pingpang_buf[RECV_BUF_PING][0]) {
        p_dma_recv_buf = &pgp_uart_recv_pingpang_buf[RECV_BUF_PANG][0];
    } else {
        p_dma_recv_buf = &pgp_uart_recv_pingpang_buf[RECV_BUF_PING][0];
    }
    
    
    hal_uart_dma_recv_mask(pgp_uart_ctx.uart_port, p_dma_recv_buf, PGP_UART_RECV_BUF_SIZE, NULL, NULL, &mask);
    int_unlock(lock);   

    PGP_UART_TRACE(0, "...");
}

static void pgp_uart_dma_rx_stop(void)
{
    uint32_t lock;
    union HAL_UART_IRQ_T mask;

    mask.reg = 0;
    
    lock = int_lock();
    hal_uart_irq_set_mask(pgp_uart_ctx.uart_port, mask);
    hal_uart_stop_dma_recv(pgp_uart_ctx.uart_port);
    int_unlock(lock);
    PGP_UART_TRACE(0, ".");
}

static void pgp_uart_dma_rx_handler(uint32_t xfer_size, int dma_error, union HAL_UART_IRQ_T status)
{
    pgp_uart_recv_item_s *p_recv_item = &pgp_uart_recv_push_item;
    
    if (dma_error) {
        PGP_UART_TRACE(1, "dma error: xfer_size=%d", xfer_size);
        pgp_uart_dma_rx_stop();
        pgp_uart_dma_rx_start();
    } else if (status.BE || status.FE || status.OE || status.PE) {
        PGP_UART_TRACE(2, "uart error: xfer_size=%d, status=0x%08x", xfer_size, status.reg);
        pgp_uart_dma_rx_stop();
        pgp_uart_dma_rx_start();
    } else if(xfer_size > 0) {
        PGP_UART_TRACE(1, "recv_data, xfer_size=%d", xfer_size);
        pgp_uart_dma_rx_stop();
        memcpy(p_recv_item->data, p_dma_recv_buf, xfer_size);
        p_recv_item->len = xfer_size;
        pgp_uart_dma_rx_start();

        pgp_uart_recv_queue_push_data(p_recv_item);
		if(!pgp_uart_ctx.rx_working){
	  		sndp_call_func_in_dev_thread((uint32_t)pgp_uart_recv_data, 0, 0, 0);
		}
    }
}

static void pgp_uart_dma_tx_handler(uint32_t xfer_size, int dma_error)
{
#if 0    
    uint32_t delay_ms;
    uint32_t count;
    
    PGP_UART_TRACE(1, "xfer_size=%d", xfer_size);

    count = xfer_size < 32 ? 32 : xfer_size;
    delay_ms = (count * 8) * 1000 / PGP_UART_BAUD;
    if(delay_ms < 2) {
        delay_ms = 2;
    }
#endif

    sndp_delay_exec_start(20, (uint32_t) pgp_uart_send_complete, 0, 0, 0);
}

static void pgp_uart_dma_tx(uint8_t *data, uint32_t data_len)
{
    hal_uart_dma_send(pgp_uart_ctx.uart_port, data, data_len, NULL, NULL);
}

static void pgp_uart_dma_disable(void)
{
	pgp_uart_dma_rx_stop();
	hal_uart_irq_set_dma_handler(pgp_uart_ctx.uart_port, NULL, NULL);
}

static void pgp_uart_dma_enable(void)
{
	hal_uart_irq_set_dma_handler(pgp_uart_ctx.uart_port, pgp_uart_dma_rx_handler, pgp_uart_dma_tx_handler);
}

#endif

static void pgp_uart_send_timeout(void)
{
    sndp_call_func_in_dev_thread((uint32_t) pgp_uart_send_complete, 0, 0, 0);
}

static void pgp_uart_send_complete(void)
{
    pgp_uart_ctx.tx_working = false;
    sndp_delay_exec_stop((uint32_t)pgp_uart_send_timeout);
    pgp_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    if(pgp_uart_send_queue_get_len() > 0) {
        sndp_delay_exec_start(20, (uint32_t)pgp_uart_send_hdlr, 0, 0, 0);
    }
}

static void pgp_uart_send_hdlr(void)
{
    pgp_uart_send_item_s *p_send_item = &pgp_uart_send_process_item;

    if(pgp_uart_ctx.tx_working) {
        sndp_delay_exec_start(50, (uint32_t)pgp_uart_send_hdlr, 0, 0, 0);
        return;
    }

    memset(p_send_item, 0, sizeof(pgp_uart_send_item_s));
    if(pgp_uart_send_queue_pop_data(p_send_item) == 0) {
        pgp_uart_ctx.tx_working = true;
        
        PGP_UART_TRACE(0, "send_data_len=%d", p_send_item->len);
        DUMP8("%02X ", p_send_item->data, p_send_item->len > 32 ? 32 : p_send_item->len);

        sndp_delay_exec_start(200, (uint32_t)pgp_uart_send_timeout, 0, 0, 0);
        pgp_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_TX);
        
#if (PGP_UART_DMA) 
        pgp_uart_dma_tx(p_send_item->data, p_send_item->len);
#endif
#if (PGP_UART_IRQ)             
        pgp_uart_irq_tx(p_send_item->data, p_send_item->len);
#endif    
    }
    
}


void pgp_uart_open(void)
{
    int ret;
    
	PGP_UART_TRACE(1, "uart_port=%d, baudrate=%d", pgp_uart_ctx.uart_port, pgp_uart_cfg.baud);

#if (PGP_UART_DMA) 
    pgp_uart_cfg.dma_rx = true;
	pgp_uart_cfg.dma_tx = true;
#endif
#if (PGP_UART_IRQ)             
    pgp_uart_cfg.dma_rx = false;
	pgp_uart_cfg.dma_tx = false;
#endif

    ret = hal_uart_open(pgp_uart_ctx.uart_port, &pgp_uart_cfg);
    if (ret) {
        PGP_UART_TRACE(1, "Failed to open uart(%d)", pgp_uart_ctx.uart_port);
        return;
    }

    if(pgp_uart_cfg.baud == 0) {
        pgp_uart_cfg.baud = PGP_UART_BAUD;
    }

    pgp_uart_ctx.read_wait_time = (8*2*1000*1000)/pgp_uart_cfg.baud/(PGP_UART_READ_RETRY_CNT);
    if(pgp_uart_ctx.read_wait_time < 10) {
        pgp_uart_ctx.read_wait_time = 10;
    }

#if (PGP_UART_DMA) 
    pgp_uart_dma_enable();
#endif
#if (PGP_UART_IRQ)             
    pgp_uart_irq_enable();
#endif    
}

void pgp_uart_close(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_uart_ctx.uart_port);
    
#if (PGP_UART_DMA) 
    pgp_uart_dma_disable();
#endif
#if (PGP_UART_IRQ)             
    pgp_uart_irq_disable();
#endif

    hal_uart_close(pgp_uart_ctx.uart_port);
}

void pgp_uart_rx_start(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_uart_ctx.uart_port);
    
#if (PGP_UART_DMA)             
    pgp_uart_dma_rx_start();
#endif
#if (PGP_UART_IRQ)             
    pgp_uart_irq_rx_start();
#endif

}

void pgp_uart_rx_stop(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_uart_ctx.uart_port);
    
#if (PGP_UART_DMA)             
    pgp_uart_dma_rx_stop();
#endif
#if (PGP_UART_IRQ)             
    pgp_uart_irq_rx_stop();
#endif

}


static void pgp_rx_pin_cfg_gpio(void)
{
    if (pgp_rx_pin_uart_cfg.pin != HAL_IOMUX_PIN_NUM) {
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_rx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_rx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0); 
    }
}

static void pgp_rx_pin_cfg_uart(void)
{
    if (pgp_rx_pin_uart_cfg.pin != HAL_IOMUX_PIN_NUM) {
#if 0
        hal_iomux_set_uart1();
#else
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_rx_pin_uart_cfg, 1);
#endif

    }
}

static void pgp_tx_pin_cfg_gpio(void)
{
    if (pgp_tx_pin_gpio_cfg.pin != HAL_IOMUX_PIN_NUM) {
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_tx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_tx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0); 
    }
}

static void pgp_tx_pin_cfg_uart(void)
{
    if (pgp_tx_pin_uart_cfg.pin != HAL_IOMUX_PIN_NUM) {
#if 0
        hal_iomux_set_uart1();
#else
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_tx_pin_uart_cfg, 1);
#endif
    }
}

static void pgp_comm_en(bool en)
{
    if (pgp_comm_en_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
        if(en) {
            hal_gpio_pin_set((enum HAL_GPIO_PIN_T)pgp_comm_en_pin_cfg.pin);
        } else {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)pgp_comm_en_pin_cfg.pin);
        }
    }
}

static void pgp_uart_pin_config(sndp_hal_pogopin_mode_e mode)
{
    if(mode == SNDP_HAL_POGOPIN_MODE_COMM_RX) {
        pgp_tx_pin_cfg_gpio();
        pgp_rx_pin_cfg_uart();
        pgp_comm_en(true);

    } else if(mode == SNDP_HAL_POGOPIN_MODE_COMM_TX) {
        pgp_rx_pin_cfg_gpio();
        pgp_tx_pin_cfg_uart();
        pgp_comm_en(true);
        
    } else {
        pgp_comm_en(false);
        pgp_rx_pin_cfg_gpio();
        pgp_tx_pin_cfg_gpio();
        
    }
}

static int32_t pgp_uart_init(void)
{
    if(pgp_uart_ctx.inited) {
		PGP_UART_TRACE(0, "inited, rtn");
		return SNDP_HAL_RET_OK;
	}

    if(pgp_uart_recv_queue_mutex_id == NULL) {
        pgp_uart_recv_queue_mutex_id = osMutexCreate((osMutex(pgp_uart_recv_queue_mutex)));
        ASSERT(pgp_uart_recv_queue_mutex_id != NULL, "%s, %d", __func__, __LINE__);
    }
    InitCQueue(&pgp_uart_recv_queue, sizeof(pgp_uart_recv_queue_buf), (CQItemType *)pgp_uart_recv_queue_buf);
    
    if(pgp_uart_send_queue_mutex_id == NULL) {
        pgp_uart_send_queue_mutex_id = osMutexCreate(osMutex(pgp_uart_send_queue_mutex));
        ASSERT(pgp_uart_send_queue_mutex_id != NULL, "%s, %d", __func__, __LINE__);
    }
    InitCQueue(&pgp_uart_send_queue, sizeof(pgp_uart_send_queue_buf), (CQItemType *)pgp_uart_send_queue_buf);

    if (pgp_comm_en_pin_cfg.pin != HAL_IOMUX_PIN_NUM){
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_comm_en_pin_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_comm_en_pin_cfg.pin, HAL_GPIO_DIR_OUT, 0); 
    }


    memset(&pgp_uart_ctx, 0, sizeof(pgp_uart_ctx));    
    pgp_uart_ctx.uart_port = HAL_UART_ID_1;
  
    /* mode init */
    pgp_uart_ctx.pogopin_mode = SNDP_HAL_POGOPIN_MODE_UNKNOWN;
    pgp_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    
    pgp_uart_ctx.inited = true;
    PGP_UART_TRACE(0, "done.");
    return SNDP_HAL_RET_OK;
}

static int32_t pgp_uart_set_mode(sndp_hal_pogopin_mode_e mode)
{
    if(mode == pgp_uart_ctx.pogopin_mode) {
		PGP_UART_TRACE(0, "same mode");
        return SNDP_HAL_RET_OK;
    }

	pgp_uart_ctx.pogopin_mode = mode;
	
    switch(mode) {
		case SNDP_HAL_POGOPIN_MODE_CHARGING:
            PGP_UART_TRACE(0, "CHARGING");
            pgp_uart_ctx.tx_working = false;
            pgp_uart_close();
            pgp_uart_pin_config(mode);
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_32K); 
			break;
	
        case SNDP_HAL_POGOPIN_MODE_COMM_RX:
            PGP_UART_TRACE(0, "COMM_RX");
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_52M); 
            pgp_uart_ctx.tx_working = false;
            pgp_uart_pin_config(mode);
            pgp_uart_open();           
            pgp_uart_rx_start();          
            break;     

		case SNDP_HAL_POGOPIN_MODE_COMM_TX:
            PGP_UART_TRACE(0, "COMM_TX");
            pgp_uart_ctx.tx_working = false;
            pgp_uart_pin_config(mode);
            pgp_uart_open();
            pgp_uart_rx_stop();
            break;
        default:
            break;
	}
	
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_uart_get_curr_mode(sndp_hal_pogopin_mode_e *mode)
{
	*mode = pgp_uart_ctx.pogopin_mode;
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_uart_set_data_recv_callback(sndp_hal_pogopin_comm_data_recv_func callback)
{
	pgp_comm_data_recv_ptr = callback;
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_uart_send_data(uint8_t *data, uint32_t data_len)
{
    int ret;

    if(!pgp_uart_ctx.inited) {
		return SNDP_HAL_RET_OK;
	}
    
    ASSERT(data != NULL, "%s, data == NULL", __func__);
	PGP_UART_TRACE(1, "data_len=%d", data_len);

    memcpy(pgp_uart_send_push_item.data, data, data_len);
    pgp_uart_send_push_item.len = data_len;
    
    ret = pgp_uart_send_queue_push_data(&pgp_uart_send_push_item);
    if(ret != 0) {
        PGP_UART_TRACE(0, "error: no space.");
    }
    
	pgp_uart_send_hdlr();
    return 0;
}

const sndp_hal_pogopin_comm_s sndp_pogopin_comm_dual_uart = {
	.init						= pgp_uart_init,
	.set_mode					= pgp_uart_set_mode,
	.get_curr_mode				= pgp_uart_get_curr_mode,
	.set_data_recv_callback		= pgp_uart_set_data_recv_callback,
	.send_data					= pgp_uart_send_data,
};
    
#endif	/* __SNDP_PGP_UART__ */

