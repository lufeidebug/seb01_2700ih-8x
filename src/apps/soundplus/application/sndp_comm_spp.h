#ifndef __SNDP_COMM_SPP_H__
#define __SNDP_COMM_SPP_H__

#if defined(__SNDP_COMM_SPP__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_COMM_SPP_TRACE__
#if defined(__SNDP_COMM_SPP_TRACE__)
#define COMM_SPP_ENTER()            		SNDP_TRACE(2, 		"[COMM_SPP] %s, line=%d, enter", __func__, __LINE__)
#define COMM_SPP_TRACE(num, str, ...)		SNDP_TRACE(1 + num,	"[COMM_SPP] %s, " str, __func__, ##__VA_ARGS__)
#define COMM_SPP_EXIT()            		    SNDP_TRACE(2,		"[COMM_SPP] %s, line=%d, exit", __func__, __LINE__)

#else
#define COMM_SPP_ENTER() 
#define COMM_SPP_TRACE(num, str, ...)
#define COMM_SPP_EXIT()
#endif


int32_t sndp_comm_spp_send_data(uint8_t *data, uint16_t data_len);
bool sndp_comm_spp_is_connected(void);
int32_t sndp_comm_spp_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_SPP__ */
#endif /* __SNDP_COMM_SPP_H__ */
