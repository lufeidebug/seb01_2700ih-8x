#if defined(__SNDP_COMM_POGOPIN__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "cmsis.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_hal_pogopin_comm.h"

#include "sndp_comm_main.h"
#include "sndp_comm_pogopin.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/


/**************************************************************************************************
* Function
**************************************************************************************************/

static void sndp_comm_pogopin_data_recv(uint8_t *data, uint32_t data_len)
{
	//SNDP_COMM_PGP_TRACE(1, "data_len=%d", data_len);

	sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_POGOPIN, data, data_len);
}

int32_t sndp_comm_pogopin_send_data(uint8_t *data, uint16_t data_len)
{
	//SNDP_COMM_PGP_TRACE(1, "data_len=%d", data_len);
    sndp_hal_pogopin_comm_send_data(data, data_len);
    return 0;
}

int32_t sndp_comm_pogopin_init(void)
{
	sndp_hal_pogopin_comm_init();
	sndp_hal_pogopin_comm_set_data_recv_callback(sndp_comm_pogopin_data_recv);

	/* 功耗优化: 开机按 入仓+充电 状态设定产测UART模式.
	 * 出仓或充电 -> 低功耗(关UART); 入仓且不充电 -> COMM_RX开UART等盒子通信 */
	sndp_hal_pogopin_comm_set_mode((sndp_dev_iobox_is_in_box(false) && !sndp_dev_charger_is_plugin(false)) ?
		SNDP_HAL_POGOPIN_MODE_COMM_RX : SNDP_HAL_POGOPIN_MODE_CHARGING);

	SNDP_COMM_PGP_TRACE(0, "done.");
    return 0;
}


#endif	/* __SNDP_COMM_POGOPIN__ */


