#if defined(__SNDP_COMM_POGOPIN__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_hal_common.h"
#include "sndp_hal_pogopin_comm.h"

#if defined(__SNDP_POGOPIN_SINGLE_UART__)
#include "sndp_pogopin_single_uart.h"
#endif

#if defined(__SNDP_POGOPIN_DUAL_UART__)
#include "sndp_pogopin_dual_uart.h"
#endif




/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_hal_pogopin_comm_s *p_pogopin_comm = NULL;


/**************************************************************************************************
* Function
**************************************************************************************************/




uint32_t sndp_hal_pogopin_comm_init(void)
{
#if defined(__SNDP_POGOPIN_SINGLE_UART__)	
	if((sndp_pogopin_comm_single_uart.init != NULL) && (sndp_pogopin_comm_single_uart.init() == SNDP_HAL_RET_OK)) {
		p_pogopin_comm = (sndp_hal_pogopin_comm_s *)&sndp_pogopin_comm_single_uart;
		return SNDP_HAL_RET_OK;
	}
#endif

#if defined(__SNDP_POGOPIN_DUAL_UART__)	
	if((sndp_pogopin_comm_dual_uart.init != NULL) && (sndp_pogopin_comm_dual_uart.init() == SNDP_HAL_RET_OK)) {
		p_pogopin_comm = (sndp_hal_pogopin_comm_s *)&sndp_pogopin_comm_dual_uart;
		return SNDP_HAL_RET_OK;
	}
#endif

	p_pogopin_comm = NULL;
	return SNDP_HAL_RET_FAIL;
	
}

uint32_t sndp_hal_pogopin_comm_set_mode(sndp_hal_pogopin_mode_e mode)
{
	if((p_pogopin_comm != NULL) && (p_pogopin_comm->set_mode != NULL))
		return p_pogopin_comm->set_mode(mode);
	
	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_pogopin_comm_get_curr_mode(sndp_hal_pogopin_mode_e *mode)
{
	if((p_pogopin_comm != NULL) && (p_pogopin_comm->get_curr_mode != NULL))
		return p_pogopin_comm->get_curr_mode(mode);
	
	return SNDP_HAL_RET_FAIL;

}

uint32_t sndp_hal_pogopin_comm_set_data_recv_callback(sndp_hal_pogopin_comm_data_recv_func callback)
{
	if((p_pogopin_comm != NULL) && (p_pogopin_comm->set_data_recv_callback != NULL))
		return p_pogopin_comm->set_data_recv_callback(callback);
	
	return SNDP_HAL_RET_FAIL;
}

uint32_t sndp_hal_pogopin_comm_send_data(uint8_t *data, uint32_t data_len)
{
	if((p_pogopin_comm != NULL) && (p_pogopin_comm->set_mode != NULL))
		return p_pogopin_comm->send_data(data, data_len);
	
	return SNDP_HAL_RET_FAIL;
}

#endif	/* __SNDP_COMM_POGOPIN__ */

