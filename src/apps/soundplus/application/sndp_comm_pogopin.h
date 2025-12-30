#ifndef __SNDP_COMM_POGOPIN_H__
#define __SNDP_COMM_POGOPIN_H__

#if defined(__SNDP_COMM_POGOPIN__)
#include "hal_uart.h"


#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_POGOPIN_TRACE__
#if defined(__SNDP_COMM_POGOPIN_TRACE__)
#define SNDP_COMM_PGP_ENTER()            		TRACE(2,		"[SNDP_COMM_PGP] %s, line=%d, enter", __func__, __LINE__)
#define SNDP_COMM_PGP_TRACE(num, str, ...)		TRACE(1 + num,	"[SNDP_COMM_PGP] %s, " str, __func__, ##__VA_ARGS__)
#define SNDP_COMM_PGP_EXIT()            		TRACE(2,		"[SNDP_COMM_PGP] %s, line=%d, exit", __func__, __LINE__)

#else
#define SNDP_COMM_PGP_ENTER() 
#define SNDP_COMM_PGP_TRACE(num, str, ...)
#define SNDP_COMM_PGP_EXIT()
#endif


int32_t sndp_comm_pogopin_send_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_pogopin_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_POGOPIN__ */
#endif /* __SNDP_COMM_POGOPIN_H__ */

