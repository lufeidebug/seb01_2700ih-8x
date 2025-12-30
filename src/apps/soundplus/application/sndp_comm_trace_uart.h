#ifndef __SNDP_COMM_TRACE_UART_H__
#define __SNDP_COMM_TRACE_UART_H__

#if defined(__SNDP_COMM_TRACE_UART__)



#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_TRACE_UART_TRACE__
#if defined(__SNDP_COMM_TRACE_UART_TRACE__)
#define COMM_UART_ENTER()            		TRACE(2, 		"[COMM_UART] %s, line=%d, enter", __func__, __LINE__)
#define COMM_UART_TRACE(num, str, ...)	    TRACE(1 + num,	"[COMM_UART] %s, " str, __func__, ##__VA_ARGS__)
#define COMM_UART_EXIT()            		TRACE(2,		"[COMM_UART] %s, line=%d, exit", __func__, __LINE__)

#else
#define COMM_UART_ENTER()
#define COMM_UART_TRACE(num, str, ...)
#define COMM_UART_EXIT()
#endif



int32_t sndp_comm_trace_uart_send_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_trace_uart_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_TRACE_UART__ */
#endif /* __SNDP_COMM_TRACE_UART_H__ */

