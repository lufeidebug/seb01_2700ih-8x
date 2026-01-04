#ifndef __SNDP_COMM_MS_H__
#define __SNDP_COMM_MS_H__

#if defined(__SNDP_COMM_MS__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_COMM_MS_TRACE__
#if defined(__SNDP_COMM_MS_TRACE__)
#define COMM_MS_TRACE_ENTER()                   SNDP_TRACE(2, 		"[COMM_MS] %s, line=%d, enter", __func__, __LINE__)
#define COMM_MS_TRACE(num, str, ...)            SNDP_TRACE(1 + num,  "[COMM_MS] %s, " str, __func__, ##__VA_ARGS__)
#define COMM_MS_TRACE_EXIT()                    SNDP_TRACE(2,		"[COMM_MS] %s, line=%d, exit", __func__, __LINE__)

#else
#define COMM_MS_TRACE_ENTER() 
#define COMM_MS_TRACE(num, str, ...)
#define COMM_MS_TRACE_EXIT()
#endif


int32_t sndp_comm_ms_send_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_ms_recv_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_ms_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_MS__ */
#endif /* __SNDP_COMM_MS_H__ */
