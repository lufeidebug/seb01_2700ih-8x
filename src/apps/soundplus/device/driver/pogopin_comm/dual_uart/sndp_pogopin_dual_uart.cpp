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
#define __SNDP_POGOPIN_DAUL_UART_TRACE__
        
#if defined(__SNDP_POGOPIN_DAUL_UART_TRACE__)
#define PGP_UART_LOG_TAG					"[PGP_DUAL_UART]"
#define PGP_UART_TRACE(num, str, ...)   	SNDP_TRACE(1 + num, PGP_UART_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#else
#define PGP_UART_TRACE(num, str, ...)
#endif


#define PGP_DUAL_UART_RECV_BUF_SIZE             (256)
#define PGP_DUAL_UART_SEND_BUF_SIZE             (256)


#define PGP_DUAL_UART_RX_PIN                    (HAL_IOMUX_PIN_P3_0)
#define PGP_DUAL_UART_TX_PIN                    (HAL_IOMUX_PIN_P3_1)

#define PGP_DUAL_UART_BAUD                      (115200)

#define PGP_DUAL_UART_DMA                       (0)
#define PGP_DUAL_UART_IRQ                       (1)  

#define PGP_DUAL_UART_READ_RETRY_CNT            (5)


typedef enum {
    RECV_BUF_PING,
    RECV_BUF_PANG,
    
    RECV_BUF_CNT
} pgp_dual_uart_recv_buf_index_e;

typedef enum {
    UART_PIN_TYPE_UART,
    UART_PIN_TYPE_GPIO,
    
} pgp_dual_uart_pin_type_e;


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct{
    bool inited;
    
    sndp_hal_pogopin_mode_e pogopin_mode;
    
    enum HAL_UART_ID_T uart_port; 
    uint32_t read_wait_time;  //us

    uint8_t *recv_buf;
    uint32_t recv_buf_size;
    uint32_t recv_data_len;

    bool is_sending;
    
} pgp_dual_uart_ctx_s; 


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void pgp_dual_uart_send_complete(void);
static void pgp_dual_uart_send_hdlr(void);
static int32_t pgp_dual_uart_set_mode(sndp_hal_pogopin_mode_e mode);
static int32_t pgp_dual_uart_send_data(uint8_t *data, uint32_t data_len);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static struct HAL_UART_CFG_T pgp_dual_uart_cfg = {
    HAL_UART_PARITY_NONE,
    HAL_UART_STOP_BITS_1,
    HAL_UART_DATA_BITS_8,
    HAL_UART_FLOW_CONTROL_NONE,
    HAL_UART_FIFO_LEVEL_1_2,
    HAL_UART_FIFO_LEVEL_1_2,
    PGP_DUAL_UART_BAUD,
    true,
    true,
    false,
};

static pgp_dual_uart_ctx_s pgp_dual_uart_ctx;
static sndp_hal_pogopin_comm_data_recv_func pgp_dual_uart_data_recv_ptr = NULL;

static uint8_t pgp_dual_uart_recv_buf[RECV_BUF_CNT][PGP_DUAL_UART_RECV_BUF_SIZE];
static uint8_t pgp_dual_uart_send_buf[PGP_DUAL_UART_SEND_BUF_SIZE];

//static union HAL_UART_IRQ_T pgp_dual_uart_mask;


static CQueue pgp_dual_uart_send_queue;
static uint8_t pgp_dual_uart_send_queue_buf[PGP_DUAL_UART_SEND_BUF_SIZE*2];

static osMutexId pgp_dual_uart_send_queue_mutex_id = NULL;
osMutexDef(pgp_dual_uart_send_queue_mutex);


static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_rx_pin_gpio_cfg = {
    PGP_DUAL_UART_RX_PIN, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_NOPULL,
};


static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_tx_pin_gpio_cfg = {
    PGP_DUAL_UART_TX_PIN, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_NOPULL,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_rx_pin_uart_cfg = {
    PGP_DUAL_UART_RX_PIN, HAL_IOMUX_FUNC_MCU_UART1_RX, HAL_IOMUX_PIN_VOLTAGE_MEM, HAL_IOMUX_PIN_PULLUP_ENABLE,
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP pgp_tx_pin_uart_cfg[] = {
    PGP_DUAL_UART_TX_PIN, HAL_IOMUX_FUNC_MCU_UART1_TX, HAL_IOMUX_PIN_VOLTAGE_MEM, HAL_IOMUX_PIN_NOPULL,
};



/**************************************************************************************************
* Function
**************************************************************************************************/
static int pgp_dual_uart_send_queue_push_data(uint8_t *data, uint16_t data_len)
{
    int ret;
    
    osMutexWait(pgp_dual_uart_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&pgp_dual_uart_send_queue, (CQItemType *)data, data_len);
    osMutexRelease(pgp_dual_uart_send_queue_mutex_id);
    return ret;
}

static int pgp_dual_uart_send_queue_pop_data(uint8_t *buf, uint16_t data_len)
{
    int ret;
    
    osMutexWait(pgp_dual_uart_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&pgp_dual_uart_send_queue, (CQItemType *)buf, data_len);
    osMutexRelease(pgp_dual_uart_send_queue_mutex_id);

    return ret;
}

static int pgp_dual_uart_send_queue_get_len(void)
{
    uint16_t queue_len;
    
    osMutexWait(pgp_dual_uart_send_queue_mutex_id, osWaitForever);
    queue_len = LengthOfCQueue(&pgp_dual_uart_send_queue);
    osMutexRelease(pgp_dual_uart_send_queue_mutex_id);

    return queue_len;
}

POSSIBLY_UNUSED static void pgp_dual_uart_recv_data(void)
{
    PGP_UART_TRACE(0, "...");
    DUMP8("%02X ", pgp_dual_uart_ctx.recv_buf, pgp_dual_uart_ctx.recv_data_len);

    //pgp_dual_uart_send_data(pgp_dual_uart_ctx.recv_buf, pgp_dual_uart_ctx.recv_data_len);
    
    if(pgp_dual_uart_data_recv_ptr) {
        pgp_dual_uart_data_recv_ptr(pgp_dual_uart_ctx.recv_buf, pgp_dual_uart_ctx.recv_data_len);
    }

}


#if (PGP_DUAL_UART_IRQ)
static void pgp_dual_uart_irq_rx_handler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status)
{
    static uint32_t last_time = 0;
    uint32_t curr_time = 0;
    uint8_t rev_char = 0;
    uint8_t read_retry;

	//PGP_UART_TRACE(0, "enter");

    if(status.RX || status.RT){
        curr_time = hal_sys_timer_get();
        if(TICKS_TO_MS(curr_time - last_time) > 20){
			last_time = curr_time;
			
            memset(pgp_dual_uart_ctx.recv_buf, 0, pgp_dual_uart_ctx.recv_buf_size);
            pgp_dual_uart_ctx.recv_data_len = 0;
        }

		//pogopin_recv_mutex_lock();

        read_retry = 0;
        while(1) {
            if(hal_uart_readable(pgp_dual_uart_ctx.uart_port)) {
                rev_char = hal_uart_getc(pgp_dual_uart_ctx.uart_port);            
    			pgp_dual_uart_ctx.recv_buf[pgp_dual_uart_ctx.recv_data_len++] = rev_char;
                read_retry = 0;
            } else {
                read_retry++;
            }

            if(read_retry >= 5) {
                break;
            }
            
            hal_sys_timer_delay_us(pgp_dual_uart_ctx.read_wait_time);
        } 

		//pogopin_recv_mutex_unlock();
		
		hal_uart_flush(pgp_dual_uart_ctx.uart_port, 4);   
        
		PGP_UART_TRACE(1, "recv_data_len=%d", pgp_dual_uart_ctx.recv_data_len); 
		if(pgp_dual_uart_ctx.recv_data_len){
	  		sndp_call_func_in_dev_thread((uint32_t)pgp_dual_uart_recv_data, 0, 0, 0);
		}
             
    }else {
        PGP_UART_TRACE(6, "status RX:%d, TX:%d, FE:%d, PE:%d, BE:%d, OE:%d",
            	status.RX, status.TX, status.FE, status.PE, status.BE, status.OE);
    }
}

static void pgp_dual_uart_irq_rx_start(void)
{
    union HAL_UART_IRQ_T mask;

    if(pgp_dual_uart_ctx.recv_buf == &pgp_dual_uart_recv_buf[RECV_BUF_PING][0]) {
        pgp_dual_uart_ctx.recv_buf = &pgp_dual_uart_recv_buf[RECV_BUF_PANG][0];
    } else {
        pgp_dual_uart_ctx.recv_buf = &pgp_dual_uart_recv_buf[RECV_BUF_PING][0];
    }
    
    pgp_dual_uart_ctx.recv_buf_size = PGP_DUAL_UART_RECV_BUF_SIZE;
    pgp_dual_uart_ctx.recv_data_len = 0;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_dual_uart_ctx.uart_port, 0);
    mask.reg = 0;
    mask.BE = 1;
    mask.RT = 1;
    mask.RX = 1;
    hal_uart_irq_set_mask(pgp_dual_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_dual_uart_irq_rx_stop(void)
{
	PGP_UART_TRACE(0, "enter");
	
    union HAL_UART_IRQ_T mask;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_dual_uart_ctx.uart_port, 0);    
    mask.reg = 0;
    hal_uart_irq_set_mask(pgp_dual_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_dual_uart_irq_tx(uint8_t *data, uint32_t data_len)
{
	PGP_UART_TRACE(1, "data_len=%d", data_len);
	DUMP8("%02X ", data, data_len);
    
	hal_uart_flush(pgp_dual_uart_ctx.uart_port, 0);
	for(uint32_t i = 0; i < data_len; i++) {
		//osDelay(5);
        hal_uart_blocked_putc(pgp_dual_uart_ctx.uart_port, data[i]);
		//osDelay(5);
//		hal_uart_flush(pgp_dual_uart_ctx.uart_port, 0);
		while (!hal_uart_get_flag(pgp_dual_uart_ctx.uart_port).TXFE || hal_uart_get_flag(pgp_dual_uart_ctx.uart_port).BUSY){
		    osThreadYield();
		};
    }

    sndp_delay_exec_start(10, (uint32_t) pgp_dual_uart_send_complete, 0, 0, 0);
}

static void pgp_dual_uart_irq_disable(void)
{
	pgp_dual_uart_irq_rx_stop();
	hal_uart_irq_set_handler(pgp_dual_uart_ctx.uart_port, NULL);
}

static void pgp_dual_uart_irq_enable(void)
{
	hal_uart_irq_set_handler(pgp_dual_uart_ctx.uart_port, pgp_dual_uart_irq_rx_handler);
}
#endif

#if (PGP_DUAL_UART_DMA)
static void pgp_dual_uart_dma_rx_start(void)
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

    if(pgp_dual_uart_ctx.recv_buf == &pgp_dual_uart_recv_buf[RECV_BUF_PING][0]) {
        pgp_dual_uart_ctx.recv_buf = &pgp_dual_uart_recv_buf[RECV_BUF_PANG][0];
    } else {
        pgp_dual_uart_ctx.recv_buf = &pgp_dual_uart_recv_buf[RECV_BUF_PING][0];
    }
    
    pgp_dual_uart_ctx.recv_buf_size = PGP_DUAL_UART_RECV_BUF_SIZE;
    pgp_dual_uart_ctx.recv_data_len = 0;
    
    hal_uart_dma_recv_mask(pgp_dual_uart_ctx.uart_port, pgp_dual_uart_ctx.recv_buf, pgp_dual_uart_ctx.recv_buf_size, NULL, NULL, &mask);
    int_unlock(lock);   

    PGP_UART_TRACE(0, "...");
}

static void pgp_dual_uart_dma_rx_stop(void)
{
    uint32_t lock;
    union HAL_UART_IRQ_T mask;

    mask.reg = 0;
    
    lock = int_lock();
    hal_uart_irq_set_mask(pgp_dual_uart_ctx.uart_port, mask);
    hal_uart_stop_dma_recv(pgp_dual_uart_ctx.uart_port);
    int_unlock(lock);
    PGP_UART_TRACE(0, ".");
}

static void pgp_dual_uart_dma_rx_handler(uint32_t xfer_size, int dma_error, union HAL_UART_IRQ_T status)
{
    POSSIBLY_UNUSED uint8_t *p_recv_buf;
    POSSIBLY_UNUSED uint32_t recv_len;
    
    if (dma_error) {
        PGP_UART_TRACE(1, "dma error: xfer_size=%d", xfer_size);
        pgp_dual_uart_dma_rx_stop();
        pgp_dual_uart_dma_rx_start();
    } else if (status.BE || status.FE || status.OE || status.PE) {
        PGP_UART_TRACE(2, "uart error: xfer_size=%d, status=0x%08x", xfer_size, status.reg);
        pgp_dual_uart_dma_rx_stop();
        pgp_dual_uart_dma_rx_start();
    } else if(xfer_size > 0) {
        PGP_UART_TRACE(1, "recv_data, xfer_size=%d", xfer_size);
        pgp_dual_uart_dma_rx_stop();

#if 0
        p_recv_buf = pgp_dual_uart_ctx.recv_buf;
        recv_len = xfer_size;
        pgp_dual_uart_dma_rx_start();
        
        if(pgp_dual_uart_data_recv_ptr) {
            pgp_dual_uart_data_recv_ptr(p_recv_buf, recv_len);
        }
#else
        pgp_dual_uart_ctx.recv_data_len = xfer_size;
        sndp_call_func_in_dev_thread((uint32_t) pgp_dual_uart_recv_data, 0, 0, 0);
#endif        
        
    }
}

static void pgp_dual_uart_dma_tx_handler(uint32_t xfer_size, int dma_error)
{
    uint32_t delay_ms;
    uint32_t count;
    
    PGP_UART_TRACE(1, "xfer_size=%d", xfer_size);

    count = xfer_size < 32 ? 32 : xfer_size;
    delay_ms = (count * 8) * 1000 / PGP_DUAL_UART_BAUD;
    if(delay_ms < 2) {
        delay_ms = 2;
    }
    
    sndp_delay_exec_start(delay_ms, (uint32_t) pgp_dual_uart_send_complete, 0, 0, 0);
}

static void pgp_dual_uart_dma_tx(uint8_t *data, uint32_t data_len)
{
    hal_uart_dma_send(pgp_dual_uart_ctx.uart_port, data, data_len, NULL, NULL);
}

static void pgp_dual_uart_dma_disable(void)
{
	pgp_dual_uart_dma_rx_stop();
	hal_uart_irq_set_dma_handler(pgp_dual_uart_ctx.uart_port, NULL, NULL);
}

static void pgp_dual_uart_dma_enable(void)
{
	hal_uart_irq_set_dma_handler(pgp_dual_uart_ctx.uart_port, pgp_dual_uart_dma_rx_handler, pgp_dual_uart_dma_tx_handler);
}

#endif

static void pgp_dual_uart_send_timeout(void)
{
    sndp_call_func_in_dev_thread((uint32_t) pgp_dual_uart_send_complete, 0, 0, 0);
}

static void pgp_dual_uart_send_complete(void)
{
    pgp_dual_uart_ctx.is_sending = false;
    sndp_delay_exec_stop((uint32_t)pgp_dual_uart_send_timeout);
    pgp_dual_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    pgp_dual_uart_send_hdlr();
}


static void pgp_dual_uart_send_hdlr(void)
{
    uint16_t send_queue_len;
    uint8_t *p_send_buf = &pgp_dual_uart_send_buf[0];
    uint16_t send_buf_size = sizeof(pgp_dual_uart_send_buf);
    uint16_t send_data_len;

    if(pgp_dual_uart_ctx.is_sending) {
        sndp_delay_exec_start(100, (uint32_t)pgp_dual_uart_send_hdlr, 0, 0, 0);
        return;
    }

    send_queue_len = pgp_dual_uart_send_queue_get_len();

    PGP_UART_TRACE(0, "send_queue_len=%d, send_buf_size=%d", send_queue_len, send_buf_size);
    if(send_queue_len == 0) {
        PGP_UART_TRACE(0, "There is no data in the send queue.");
        return;
    }
    
    if(send_queue_len < send_buf_size) {
        send_data_len = send_queue_len;
    } else {
        send_data_len = send_buf_size;
    }

    pgp_dual_uart_send_queue_pop_data(p_send_buf, send_data_len);
    PGP_UART_TRACE(0, "send_data_len=%d", send_data_len);
    DUMP8("%02X ", p_send_buf, send_data_len > 32 ? 32 : send_data_len);

    pgp_dual_uart_ctx.is_sending = true;
    sndp_delay_exec_start(200, (uint32_t)pgp_dual_uart_send_timeout, 0, 0, 0);

    pgp_dual_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_TX);
    
 #if (PGP_DUAL_UART_DMA) 
    pgp_dual_uart_dma_tx(p_send_buf, send_data_len);
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_irq_tx(p_send_buf, send_data_len);
#endif    
    
}


void pgp_dual_uart_open(void)
{
    int ret;
    
	PGP_UART_TRACE(1, "uart_port=%d, baudrate=%d", pgp_dual_uart_ctx.uart_port, pgp_dual_uart_cfg.baud);

#if (PGP_DUAL_UART_DMA) 
    pgp_dual_uart_cfg.dma_rx = true;
	pgp_dual_uart_cfg.dma_tx = true;
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_cfg.dma_rx = false;
	pgp_dual_uart_cfg.dma_tx = false;
#endif

    ret = hal_uart_open(pgp_dual_uart_ctx.uart_port, &pgp_dual_uart_cfg);
    if (ret) {
        PGP_UART_TRACE(1, "Failed to open uart(%d)", pgp_dual_uart_ctx.uart_port);
        return;
    }

    if(pgp_dual_uart_cfg.baud == 0) {
        pgp_dual_uart_cfg.baud = PGP_DUAL_UART_BAUD;
    }

    pgp_dual_uart_ctx.read_wait_time = (8*2*1000*1000)/pgp_dual_uart_cfg.baud/(PGP_DUAL_UART_READ_RETRY_CNT);
    if(pgp_dual_uart_ctx.read_wait_time < 10) {
        pgp_dual_uart_ctx.read_wait_time = 10;
    }

#if (PGP_DUAL_UART_DMA) 
    pgp_dual_uart_dma_enable();
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_irq_enable();
#endif    
}

void pgp_dual_uart_close(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_dual_uart_ctx.uart_port);
    
#if (PGP_DUAL_UART_DMA) 
    pgp_dual_uart_dma_disable();
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_irq_disable();
#endif

    hal_uart_close(pgp_dual_uart_ctx.uart_port);
}

void pgp_dual_uart_rx_start(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_dual_uart_ctx.uart_port);
    
#if (PGP_DUAL_UART_DMA)             
    pgp_dual_uart_dma_rx_start();
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_irq_rx_start();
#endif

}

void pgp_dual_uart_rx_stop(void)
{
	PGP_UART_TRACE(1, "uart_port=%d", pgp_dual_uart_ctx.uart_port);
    
#if (PGP_DUAL_UART_DMA)             
    pgp_dual_uart_dma_rx_stop();
#endif
#if (PGP_DUAL_UART_IRQ)             
    pgp_dual_uart_irq_rx_stop();
#endif

}

static void pgp_dual_uart_pin_config(sndp_hal_pogopin_mode_e mode)
{
    if(mode == SNDP_HAL_POGOPIN_MODE_COMM_RX) {
#if 0
        hal_iomux_set_uart1();
#else
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_rx_pin_uart_cfg, 1);
#endif
        
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_tx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_tx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0);

    } else if(mode == SNDP_HAL_POGOPIN_MODE_COMM_TX) {
#if 0
        hal_iomux_set_uart1();
#else
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_tx_pin_uart_cfg, 1);
#endif
        
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_rx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_rx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0);    
        
    } else {
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_rx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_rx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0);
        
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pgp_tx_pin_gpio_cfg, 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pgp_tx_pin_gpio_cfg.pin, HAL_GPIO_DIR_IN, 0);
        
    }
}

static int32_t pgp_dual_uart_init(void)
{
    if(pgp_dual_uart_ctx.inited) {
		PGP_UART_TRACE(0, "already initialized.");
		return SNDP_HAL_RET_OK;
	}

    if(pgp_dual_uart_send_queue_mutex_id == NULL) {
        pgp_dual_uart_send_queue_mutex_id = osMutexCreate(osMutex(pgp_dual_uart_send_queue_mutex));
        ASSERT(pgp_dual_uart_send_queue_mutex_id != NULL, "%s, cannot create pgp_dual_uart_send_queue_mutex_id", __func__);
    }

    InitCQueue(&pgp_dual_uart_send_queue, sizeof(pgp_dual_uart_send_queue_buf), pgp_dual_uart_send_queue_buf);
    
    memset(&pgp_dual_uart_ctx, 0, sizeof(pgp_dual_uart_ctx));    
    pgp_dual_uart_ctx.uart_port = HAL_UART_ID_1;   
  
    /* mode init */
    pgp_dual_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
    
    pgp_dual_uart_ctx.inited = true;
    PGP_UART_TRACE(0, "done.");
    return SNDP_HAL_RET_OK;
}

static int32_t pgp_dual_uart_set_mode(sndp_hal_pogopin_mode_e mode)
{
    if(mode == pgp_dual_uart_ctx.pogopin_mode) {
		PGP_UART_TRACE(0, "same mode");
        return SNDP_HAL_RET_OK;
    }

	pgp_dual_uart_ctx.pogopin_mode = mode;
	
    switch(mode) {
		case SNDP_HAL_POGOPIN_MODE_CHARGING:
            PGP_UART_TRACE(0, "SNDP_HAL_POGOPIN_MODE_CHARGING");
            pgp_dual_uart_ctx.is_sending = false;
            pgp_dual_uart_close();
            pgp_dual_uart_pin_config(mode);
			break;
	
        case SNDP_HAL_POGOPIN_MODE_COMM_RX:
            PGP_UART_TRACE(0, "SNDP_HAL_POGOPIN_MODE_COMM_RX");
            pgp_dual_uart_ctx.is_sending = false;
            pgp_dual_uart_pin_config(mode);
            pgp_dual_uart_open();           
            pgp_dual_uart_rx_start();          
            break;     

		case SNDP_HAL_POGOPIN_MODE_COMM_TX:
            PGP_UART_TRACE(0, "SNDP_HAL_POGOPIN_MODE_COMM_TX");
            pgp_dual_uart_ctx.is_sending = false;
            pgp_dual_uart_pin_config(mode);
            pgp_dual_uart_open();
            pgp_dual_uart_rx_stop();
            break;

        case SNDP_HAL_POGOPIN_MODE_OTA_RX:
            PGP_UART_TRACE(0, "SNDP_HAL_POGOPIN_MODE_OTA_RX");
            break;
			
		case SNDP_HAL_POGOPIN_MODE_OTA_TX:
            PGP_UART_TRACE(0, "SNDP_HAL_POGOPIN_MODE_OTA_TX");
            break;
	}
	
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_dual_uart_get_curr_mode(sndp_hal_pogopin_mode_e *mode)
{
	*mode = pgp_dual_uart_ctx.pogopin_mode;
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_dual_uart_set_data_recv_callback(sndp_hal_pogopin_comm_data_recv_func callback)
{
	pgp_dual_uart_data_recv_ptr = callback;
	return SNDP_HAL_RET_OK;
}

static int32_t pgp_dual_uart_send_data(uint8_t *data, uint32_t data_len)
{
    int ret;

    if(!pgp_dual_uart_ctx.inited) {
		return SNDP_HAL_RET_OK;
	}
    
    ASSERT(data != NULL, "%s, data == NULL", __func__);
	PGP_UART_TRACE(1, "data_len=%d", data_len);

    ret = pgp_dual_uart_send_queue_push_data(data, data_len);
    if(ret != 0) {
        PGP_UART_TRACE(0, "error: no space.");
    }
    
	sndp_call_func_in_dev_thread((uint32_t) pgp_dual_uart_send_hdlr, 0, 0, 0);

    return 0;
}

const sndp_hal_pogopin_comm_s sndp_hal_pogopin_comm_dual_uart = {
	.init						= pgp_dual_uart_init,
	.set_mode					= pgp_dual_uart_set_mode,
	.get_curr_mode				= pgp_dual_uart_get_curr_mode,
	.set_data_recv_callback		= pgp_dual_uart_set_data_recv_callback,
	.send_data					= pgp_dual_uart_send_data,
};
    
#endif	/* __SNDP_PGP_DUAL_UART__ */

