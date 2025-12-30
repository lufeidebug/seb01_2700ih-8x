#if defined(__SNDP_COMM_TRACE_UART__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_comm_main.h"
#include "sndp_comm_trace_uart.h"
#include "sndp_product_test.h"


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_COMM_TRACE_RX_BUF_SIZE    128
#define SNDP_COMM_TRACE_RX_OPEN_TIME       (1000*60*2)

/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_comm_trace_uart_rx_disable_timeout_handler(void const* param);
static void sndp_comm_trace_uart_rx_disable_timer_stop(void);
static void sndp_comm_trace_uart_rx_disable_timer_start(void);


/**************************************************************************************************
* Variable
**************************************************************************************************/
osTimerDef(SNDP_COMM_TRACE_RX_DISABLE_TIMER, sndp_comm_trace_uart_rx_disable_timeout_handler);
static osTimerId sndp_comm_trace_rx_disable_timer = NULL;

static uint8_t sndp_comm_trace_rx_buf[SNDP_COMM_TRACE_RX_BUF_SIZE];

/**************************************************************************************************
* Function
**************************************************************************************************/
static void sndp_comm_trace_uart_rx_disable_timeout_handler(void const* param)
{
    COMM_UART_ENTER();
#if defined(__SNDP_PRODUCT_TEST__)
    if(sndp_pt_is_in_test_mode()) {
        osTimerStart(sndp_comm_trace_rx_disable_timer, SNDP_COMM_TRACE_RX_OPEN_TIME);
        return;
    }
#endif

	hal_trace_rx_close();
	sndp_comm_trace_uart_rx_disable_timer_stop();
}

static void sndp_comm_trace_uart_rx_disable_timer_stop(void)
{
    COMM_UART_ENTER();

    if(sndp_comm_trace_rx_disable_timer) {
        osStatus status;
		
        status = osTimerStop(sndp_comm_trace_rx_disable_timer);
		COMM_UART_TRACE(1, "00 status2=%d.", status);
        status = osTimerDelete(sndp_comm_trace_rx_disable_timer);
        COMM_UART_TRACE(1, "01 status2=%d.", status);
        sndp_comm_trace_rx_disable_timer = NULL;
    }
}

void sndp_comm_trace_uart_rx_disable_timer_start(void)
{
    if (sndp_comm_trace_rx_disable_timer == NULL) {
        sndp_comm_trace_rx_disable_timer = osTimerCreate(osTimer(SNDP_COMM_TRACE_RX_DISABLE_TIMER), osTimerOnce, NULL);
    }

	COMM_UART_ENTER();
    osTimerStart(sndp_comm_trace_rx_disable_timer, SNDP_COMM_TRACE_RX_OPEN_TIME);
}

static uint32_t sndp_comm_trace_uart_received_data(uint8_t *data, uint32_t data_len)
{
	COMM_UART_TRACE(1, "data_len=%d", data_len);
	//DUMP8("%02X ", data, data_len > 32 ? 32 : data_len);

    sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_TRACE_UART, data, data_len);
	return 0;
}

int32_t sndp_comm_trace_uart_send_data(uint8_t *data, uint16_t data_len)
{
	COMM_UART_TRACE(1, "data_len=%d", data_len);
    //hal_trace_output(data, data_len);
    sndp_hal_trace_output(data, data_len);
    return 0;
}

int32_t sndp_comm_trace_uart_init(void)
{
	COMM_UART_TRACE(0, "done.");
	hal_trace_rx_open(sndp_comm_trace_rx_buf, SNDP_COMM_TRACE_RX_BUF_SIZE, sndp_comm_trace_uart_received_data);
	sndp_comm_trace_uart_rx_disable_timer_start();
	return 0;
}

#endif	/* __SNDP_COMM_TRACE_UART__ */

