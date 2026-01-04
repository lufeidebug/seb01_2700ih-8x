#if defined(__SNDP_POGOPIN_SINGLE_UART__)
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

#include "sndp_if_common.h"
#include "sndp_hal_common.h"
#include "sndp_hal_pogopin_comm.h"
#include "sndp_pogopin_single_uart.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_POGOPIN_SINGLE_UART_TRACE__
    
#if defined(__SNDP_POGOPIN_SINGLE_UART_TRACE__)
#define PGP_SUART_LOG_TAG					"[PGP_S_UART]"
#define PGP_SUART_TRACE(num, str, ...)   	SNDP_TRACE(1 + num, PGP_SUART_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#else
#define PGP_SUART_TRACE(num, str, ...)
#endif


#define PGP_SINGLE_UART_BAUD                        (115200)
#define PGP_SINGLE_UART_OTA_BAUD                    (921600)
	
#define PGP_SINGLE_UART_CMD_DATA_MAX                (64)
#define PGP_SINGLE_UART_RECV_BUF_SIZE               (6 + PGP_SINGLE_UART_CMD_DATA_MAX + 2)	/* head + cmd data len + crc16 */
#define PGP_SINGLE_UART_SEND_BUF_SIZE               (6 + PGP_SINGLE_UART_CMD_DATA_MAX + 2)

#define PGP_SINGLE_UART_READ_RETRY_CNT              (5)



/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef enum {
	PGP_SINGLE_UART_DIR_RX,
	PGP_SINGLE_UART_DIR_TX,

} pgp_single_uart_dir_e;


typedef struct{
	bool inited;
	sndp_hal_pogopin_mode_e working_mode;

	enum HAL_UART_ID_T uart_port; 
	pgp_single_uart_dir_e uart_dir;
    uint32_t read_wait_time;  //us
    
	bool tx_working;
	bool rx_working;

	uint8_t *recv_buf;
	uint32_t recv_buf_size;
	uint32_t recv_data_len;

	uint8_t *send_buf;
	uint32_t send_buf_size;
	uint32_t send_data_len;
} pgp_single_uart_ctx_s; 


static void pgp_single_uart_irq_rx_handler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status);
static void pgp_single_uart_irq_rx_start(void);
static void pgp_single_uart_irq_rx_stop(void);
static void pgp_single_uart_dma_rx_handler(uint32_t xfer_size, int dma_error, union HAL_UART_IRQ_T status);
static void pgp_single_uart_dma_tx_handler(uint32_t xfer_size, int dma_error);
static void pgp_single_uart_dma_rx_start(void);
static void pgp_single_uart_dma_rx_stop(void);
static void pgp_single_uart_dma_tx(uint8_t *data, uint32_t data_len);
static void pgp_single_uart_open(uint32_t baudrate);
static int32_t pgp_single_uart_set_mode(sndp_hal_pogopin_mode_e mode);
static void pgp_single_uart_mode_change_irq_handler(enum HAL_GPIO_PIN_T pin);
static void pgp_single_uart_mode_change_irq_disable(void);
static void pgp_single_uart_mode_change_irq_enable(void);



/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_pogopin_comm_data_recv_func sndp_hal_pogopin_comm_data_recv_ptr = NULL;

static uint8_t pgp_single_uart_recv_buf[PGP_SINGLE_UART_RECV_BUF_SIZE];
static uint8_t pgp_single_uart_send_buf[PGP_SINGLE_UART_SEND_BUF_SIZE];

static uint8_t pgp_single_uart_cache_buf[PGP_SINGLE_UART_RECV_BUF_SIZE];
static uint16_t pgp_single_uart_cache_len = 0;

static pgp_single_uart_ctx_s pgp_single_uart_ctx;

static struct HAL_UART_CFG_T pgp_single_uart_cfg = {
    HAL_UART_PARITY_NONE,
    HAL_UART_STOP_BITS_1,
    HAL_UART_DATA_BITS_8,
    HAL_UART_FLOW_CONTROL_NONE,
    HAL_UART_FIFO_LEVEL_1_2,
    HAL_UART_FIFO_LEVEL_1_8,
    PGP_SINGLE_UART_BAUD, 
    false,
    false,
    false
};

static const struct HAL_IOMUX_PIN_FUNCTION_MAP sndp_comm_uart_mode_change_pin_cfg = {
    HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENABLE,
};


osMutexDef(pogopin_recv_mutex);
static osMutexId pogopin_recv_mutex_id;

#define pogopin_recv_mutex_lock()   {osMutexWait(pogopin_recv_mutex_id, osWaitForever);}
#define pogopin_recv_mutex_unlock() {osMutexRelease(pogopin_recv_mutex_id);}


/**************************************************************************************************
* Function
**************************************************************************************************/

static void pogpin_single_uart_recv_data(void)
{
	pogopin_recv_mutex_lock();
	
	memcpy(&pgp_single_uart_ctx.recv_buf[0], pgp_single_uart_cache_buf, pgp_single_uart_cache_len);
	pgp_single_uart_ctx.recv_data_len = pgp_single_uart_cache_len;
	pgp_single_uart_cache_len = 0;

#if 1
	PGP_SUART_TRACE(0, "recv_data:");
	DUMP8("%02x ", pgp_single_uart_ctx.recv_buf, 
			(pgp_single_uart_ctx.recv_data_len > 16) ? (16) : (pgp_single_uart_ctx.recv_data_len));
#endif

	sndp_hal_pogopin_comm_data_recv_ptr(pgp_single_uart_cache_buf, pgp_single_uart_ctx.recv_data_len);
	
	pogopin_recv_mutex_unlock();
}

static void pgp_single_uart_dir_set(pgp_single_uart_dir_e dir)
{
	if(PGP_SINGLE_UART_DIR_RX == dir) {
		hal_iomux_single_wire_uart_rx(pgp_single_uart_ctx.uart_port);
	} else if(PGP_SINGLE_UART_DIR_TX == dir) {
		hal_iomux_single_wire_uart_tx(pgp_single_uart_ctx.uart_port);
	}
}

static void pgp_single_uart_irq_rx_handler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status)
{
    static uint32_t last_time = 0;
    uint32_t curr_time = 0;
    uint8_t rev_char = 0;
    uint8_t read_retry;

	//PGP_SUART_TRACE(0, "enter");
  
    if(status.RX || status.RT){
        curr_time = hal_sys_timer_get();
        if(TICKS_TO_MS(curr_time - last_time) > 20){
			last_time = curr_time;
			
            memset(pgp_single_uart_cache_buf, 0, PGP_SINGLE_UART_RECV_BUF_SIZE);
            pgp_single_uart_cache_len = 0;
        }

		pogopin_recv_mutex_lock();

        read_retry = 0;
        while(1) {
            if(hal_uart_readable(pgp_single_uart_ctx.uart_port)) {
                rev_char = hal_uart_getc(pgp_single_uart_ctx.uart_port);            
			    pgp_single_uart_cache_buf[pgp_single_uart_cache_len++] = rev_char;
                read_retry = 0;
            } else {
                read_retry++;
            }

            if(read_retry >= 5) {
                break;
            }
            
            hal_sys_timer_delay_us(pgp_single_uart_ctx.read_wait_time);
        }

		pogopin_recv_mutex_unlock();
		
		PGP_SUART_TRACE(1, "recv_data_len=%d", pgp_single_uart_cache_len); 
		if(pgp_single_uart_cache_len >= 8){
	  		sndp_call_func_in_app_thread((uint32_t)pogpin_single_uart_recv_data, 0, 0, 0);
		}
        hal_uart_flush(pgp_single_uart_ctx.uart_port, 4);        
    }else {
        PGP_SUART_TRACE(6, "status RX:%d, TX:%d, FE:%d, PE:%d, BE:%d, OE:%d",
            	status.RX, status.TX, status.FE, status.PE, status.BE, status.OE);
    }
}

static void pgp_single_uart_irq_rx_start(void)
{
    union HAL_UART_IRQ_T mask;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);
    mask.reg = 0;
    mask.BE = 1;
    mask.RT = 1;
    mask.RX = 1;
    hal_uart_irq_set_mask(pgp_single_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_single_uart_irq_rx_stop(void)
{
	PGP_SUART_TRACE(0, "enter");
	
    union HAL_UART_IRQ_T mask;

    uint32_t lock = int_lock();
    hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);    
    mask.reg = 0;
    hal_uart_irq_set_mask(pgp_single_uart_ctx.uart_port, mask);
    int_unlock(lock);
}

static void pgp_single_uart_irq_tx(uint8_t *data, uint32_t data_len)
{
	PGP_SUART_TRACE(1, "data_len=%d", data_len);
	DUMP8("%02X ", data, data_len);
    
	hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);
	for(uint32_t i = 0; i < data_len; i++) {
		//osDelay(5);
        hal_uart_blocked_putc(pgp_single_uart_ctx.uart_port, data[i]);
		//osDelay(5);
		//hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);
		while (!hal_uart_get_flag(pgp_single_uart_ctx.uart_port).TXFE || hal_uart_get_flag(pgp_single_uart_ctx.uart_port).BUSY){
		    osThreadYield();
		};
    }
}

static void pgp_single_uart_irq_disable(void)
{
	pgp_single_uart_irq_rx_stop();
	hal_uart_irq_set_handler(pgp_single_uart_ctx.uart_port, NULL);
}

static void pgp_single_uart_irq_enable(void)
{
	pgp_single_uart_cfg.dma_rx = false;
	pgp_single_uart_cfg.dma_tx = false;
	hal_uart_irq_set_handler(pgp_single_uart_ctx.uart_port, pgp_single_uart_irq_rx_handler);
}


static void pgp_single_uart_dma_rx_handler(uint32_t xfer_size, int dma_error, union HAL_UART_IRQ_T status)
{
    PGP_SUART_TRACE(0, "enter");
	
    if(dma_error || status.FE|| status.PE || status.BE || status.OE ){
        pgp_single_uart_dma_rx_stop();
        PGP_SUART_TRACE(5, "err:%d, FE:%d, PE:%d, BE:%d, OE:%d",
            dma_error, status.FE, status.PE, status.BE, status.OE);
        pgp_single_uart_dma_rx_start();
        return;
    }

    if(xfer_size){
        PGP_SUART_TRACE(1, "recv data, xfer_size=%d", xfer_size); 
		pogopin_recv_mutex_lock();
		pgp_single_uart_ctx.recv_data_len = xfer_size;
		pogopin_recv_mutex_unlock();
		
	   	sndp_call_func_in_app_thread((uint32_t)pogpin_single_uart_recv_data, 0, 0, 0);
   
    } else {
        pgp_single_uart_dma_rx_stop();   
        pgp_single_uart_dma_rx_start();
    }
}

static void pgp_single_uart_dma_tx_handler(uint32_t xfer_size, int dma_error)
{
    PGP_SUART_TRACE(2, "xfer_size=%d, dma_error=%d", xfer_size, dma_error);    
}

static void pgp_single_uart_dma_rx_start(void)
{
	PGP_SUART_TRACE(0, "enter");
	
    union HAL_UART_IRQ_T mask;
	
    uint32_t lock = int_lock();
    hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);
    mask.reg = 0;
    mask.RT = 1;
    mask.RX = 1;
    mask.TX = 1;
    mask.BE = 1;        
    hal_uart_dma_recv_mask(pgp_single_uart_ctx.uart_port, 
			(unsigned char*)&pgp_single_uart_ctx.recv_buf[0], 
			pgp_single_uart_ctx.recv_buf_size, 
			NULL, NULL, &mask);
    int_unlock(lock);        
}

static void pgp_single_uart_dma_rx_stop(void)
{
	PGP_SUART_TRACE(0, "enter");
	
    union HAL_UART_IRQ_T mask;
	
    uint32_t lock = int_lock();
    hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);    
    mask.reg = 0;
    hal_uart_irq_set_mask(pgp_single_uart_ctx.uart_port, mask);
    hal_uart_stop_dma_recv(pgp_single_uart_ctx.uart_port);
    int_unlock(lock);
}

static void pgp_single_uart_dma_tx(uint8_t *data, uint32_t data_len)
{
	hal_uart_dma_send(pgp_single_uart_ctx.uart_port, data, data_len, NULL, NULL);
	hal_uart_flush(pgp_single_uart_ctx.uart_port, 0);
}

static void pgp_single_uart_dma_disable(void)
{
	pgp_single_uart_dma_rx_stop();
	hal_uart_irq_set_dma_handler(pgp_single_uart_ctx.uart_port, NULL, NULL);
}

static void pgp_single_uart_dma_enable(void)
{
	pgp_single_uart_cfg.dma_rx = true;
	pgp_single_uart_cfg.dma_tx = true;
	hal_uart_irq_set_dma_handler(pgp_single_uart_ctx.uart_port, pgp_single_uart_dma_rx_handler, pgp_single_uart_dma_tx_handler);
}

int32_t pgp_single_uart_send_data(uint8_t *data, uint32_t data_len)
{
	PGP_SUART_TRACE(1, "data_len=%d", data_len);
	
    sndp_hal_pogopin_mode_e curr_mode = pgp_single_uart_ctx.working_mode;

    if((SNDP_HAL_POGOPIN_MODE_COMM_RX != curr_mode) 
		&& (SNDP_HAL_POGOPIN_MODE_OTA_RX != curr_mode)){
        PGP_SUART_TRACE(0, "Current not in commuication mode");
        return -1;
    }

    if(0 == data_len || NULL == data){
        PGP_SUART_TRACE(0, "Invalid parameter !");
        return -1;
    }
	
    pgp_single_uart_ctx.send_data_len = data_len;    
    memcpy((void* )pgp_single_uart_ctx.send_buf, (const void*)data, data_len);

    if(SNDP_HAL_POGOPIN_MODE_OTA_RX == curr_mode) {
        pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_OTA_TX);
		pgp_single_uart_dma_tx(pgp_single_uart_ctx.send_buf, pgp_single_uart_ctx.send_data_len);
		pgp_single_uart_ctx.send_data_len = 0;
        osDelay(3); 
        pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_OTA_RX);
		
    } else if(SNDP_HAL_POGOPIN_MODE_COMM_RX == curr_mode){
        pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_TX);

#if 0
		uint32_t start_ticks, end_ticks;
		start_ticks = hal_fast_sys_timer_get();
#endif		

        pgp_single_uart_irq_tx(pgp_single_uart_ctx.send_buf, pgp_single_uart_ctx.send_data_len);
		pgp_single_uart_ctx.send_data_len = 0;
        
#if 0
		end_ticks = hal_fast_sys_timer_get();
		TRACE(1,"[pgp_single_uart_send_data] takes %d us\n", FAST_TICKS_TO_US(end_ticks - start_ticks));
#endif

        pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);        
    }
 
    return 0;
}

static void pgp_single_uart_open(uint32_t baudrate)
{
	PGP_SUART_TRACE(1, "baudrate=%d", baudrate);
	
    if(baudrate == 0) {
        pgp_single_uart_cfg.baud = PGP_SINGLE_UART_BAUD;
    } else {
        pgp_single_uart_cfg.baud = baudrate;
    }

    pgp_single_uart_ctx.read_wait_time = (8*2*1000*1000)/pgp_single_uart_cfg.baud/(PGP_SINGLE_UART_READ_RETRY_CNT);
    if(pgp_single_uart_ctx.read_wait_time < 10) {
        pgp_single_uart_ctx.read_wait_time = 10;
    }
    
    hal_uart_open(pgp_single_uart_ctx.uart_port, &pgp_single_uart_cfg);
}

static void pgp_single_uart_close(void)
{
	PGP_SUART_TRACE(0, "enter");  
    hal_uart_close(pgp_single_uart_ctx.uart_port);
}

static void pgp_single_uart_mode_change_irq_handler(enum HAL_GPIO_PIN_T pin)
{
	PGP_SUART_TRACE(0, "enter");

	if(sndp_comm_uart_mode_change_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
		uint8_t val = hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)sndp_comm_uart_mode_change_pin_cfg.pin);

		if(val) {
			pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_CHARGING);
		} else {
			pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_COMM_RX);
		}
	}
}

static POSSIBLY_UNUSED void pgp_single_uart_mode_change_irq_disable(void)
{
	if(sndp_comm_uart_mode_change_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
		struct HAL_GPIO_IRQ_CFG_T gpio_irg_cfg;
		gpio_irg_cfg.irq_enable = false;
		hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)sndp_comm_uart_mode_change_pin_cfg.pin, &gpio_irg_cfg);
	}
}

static void pgp_single_uart_mode_change_irq_enable(void)
{
	if(sndp_comm_uart_mode_change_pin_cfg.pin != HAL_IOMUX_PIN_NUM) {
		/* pin iomux init */
		hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&sndp_comm_uart_mode_change_pin_cfg, 1);
		hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)sndp_comm_uart_mode_change_pin_cfg.pin, HAL_GPIO_DIR_IN, 0);

		/* pin int init */
		struct HAL_GPIO_IRQ_CFG_T gpio_irg_cfg;
		gpio_irg_cfg.irq_enable = true;
		gpio_irg_cfg.irq_debounce = true;
		gpio_irg_cfg.irq_type = HAL_GPIO_IRQ_TYPE_EDGE_SENSITIVE;
		gpio_irg_cfg.irq_polarity = HAL_GPIO_IRQ_POLARITY_LOW_FALLING;
		gpio_irg_cfg.irq_handler = pgp_single_uart_mode_change_irq_handler;
		hal_gpio_setup_irq((enum HAL_GPIO_PIN_T)sndp_comm_uart_mode_change_pin_cfg.pin, &gpio_irg_cfg);
	}
}

static void pgp_single_uart_default(void) 
{
	memset(&pgp_single_uart_ctx, 0, sizeof(pgp_single_uart_ctx));
	pgp_single_uart_ctx.inited = false;
	pgp_single_uart_ctx.working_mode = SNDP_HAL_POGOPIN_MODE_CHARGING;

	pgp_single_uart_ctx.uart_dir = PGP_SINGLE_UART_DIR_RX;
	pgp_single_uart_ctx.uart_port = HAL_UART_ID_1;
	pgp_single_uart_ctx.tx_working = false;
	pgp_single_uart_ctx.rx_working = false;
	
	pgp_single_uart_ctx.recv_buf = &pgp_single_uart_recv_buf[0];
	pgp_single_uart_ctx.recv_buf_size = sizeof(pgp_single_uart_recv_buf);
	pgp_single_uart_ctx.recv_data_len = 0;

	pgp_single_uart_ctx.send_buf = &pgp_single_uart_send_buf[0];
	pgp_single_uart_ctx.send_buf_size = sizeof(pgp_single_uart_send_buf);
	pgp_single_uart_ctx.send_data_len = 0;
}

int32_t pgp_single_uart_init(void)
{
	if(pgp_single_uart_ctx.inited) {
		PGP_SUART_TRACE(0, "already initialized.");
		return SNDP_HAL_RET_OK;
	}

	pogopin_recv_mutex_id = osMutexCreate(osMutex(pogopin_recv_mutex));
    if (!pogopin_recv_mutex_id) {
        ASSERT(0, "cannot create pogopin_recv_mutex_id");
    }

	pgp_single_uart_default();

	/* mode change int init */
	pgp_single_uart_mode_change_irq_enable();

	/* mode init */
    pgp_single_uart_set_mode(SNDP_HAL_POGOPIN_MODE_CHARGING);

    /* init done */
    pgp_single_uart_ctx.inited = true;
	PGP_SUART_TRACE(0, "done.");
	
	return SNDP_HAL_RET_OK;
}

int32_t pgp_single_uart_set_mode(sndp_hal_pogopin_mode_e mode)
{
	PGP_SUART_TRACE(2, "working_mode=%d, set_mode=%d", pgp_single_uart_ctx.working_mode, mode);
    
    if(mode == pgp_single_uart_ctx.working_mode) {
		PGP_SUART_TRACE(0, "same mode");
        return SNDP_HAL_RET_OK;
    }

	pgp_single_uart_ctx.working_mode = mode;
	
    switch(mode) {
		case SNDP_HAL_POGOPIN_MODE_CHARGING:
            pgp_single_uart_dma_disable();
            pgp_single_uart_irq_disable();
			pgp_single_uart_close();
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_32K); 
			break;
	
        case SNDP_HAL_POGOPIN_MODE_COMM_RX:   
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_52M);
			pgp_single_uart_dma_disable();
			pgp_single_uart_irq_enable();
            pgp_single_uart_open(PGP_SINGLE_UART_BAUD); 
			pgp_single_uart_dir_set(PGP_SINGLE_UART_DIR_RX);
			pgp_single_uart_irq_rx_start();
            break;     

		case SNDP_HAL_POGOPIN_MODE_COMM_TX:   
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_52M);
			pgp_single_uart_dma_disable();
			pgp_single_uart_irq_enable();          
            pgp_single_uart_open(PGP_SINGLE_UART_BAUD); 
			pgp_single_uart_dir_set(PGP_SINGLE_UART_DIR_TX);
            break;     

        case SNDP_HAL_POGOPIN_MODE_OTA_RX:
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_104M);  
            pgp_single_uart_irq_disable();
			pgp_single_uart_dma_enable();
            pgp_single_uart_open(PGP_SINGLE_UART_OTA_BAUD); 
			pgp_single_uart_dir_set(PGP_SINGLE_UART_DIR_RX);
            pgp_single_uart_dma_rx_start();
            break;
			
		case SNDP_HAL_POGOPIN_MODE_OTA_TX:
            app_sysfreq_req(APP_SYSFREQ_USER_SNDP_POGOPIN_COMM, APP_SYSFREQ_104M);  
            pgp_single_uart_irq_disable();
			pgp_single_uart_dma_enable();          
            pgp_single_uart_open(PGP_SINGLE_UART_OTA_BAUD); 
			pgp_single_uart_dir_set(PGP_SINGLE_UART_DIR_TX);
            break;

        default:
            break;
	}
	
	return SNDP_HAL_RET_OK;
}

int32_t pgp_single_uart_get_curr_mode(sndp_hal_pogopin_mode_e *mode)
{
	*mode = pgp_single_uart_ctx.working_mode;
	return SNDP_HAL_RET_OK;
}

int32_t pgp_single_uart_set_data_recv_callback(sndp_hal_pogopin_comm_data_recv_func callback)
{
	sndp_hal_pogopin_comm_data_recv_ptr = callback;
	return SNDP_HAL_RET_OK;
}

const sndp_hal_pogopin_comm_s sndp_hal_pogopin_comm_single_uart = {
	.init						= pgp_single_uart_init,
	.set_mode					= pgp_single_uart_set_mode,
	.get_curr_mode				= pgp_single_uart_get_curr_mode,
	.set_data_recv_callback		= pgp_single_uart_set_data_recv_callback,
	.send_data					= pgp_single_uart_send_data,
};
    
#endif	/* __SNDP_POGOPIN_SINGLE_UART__ */

