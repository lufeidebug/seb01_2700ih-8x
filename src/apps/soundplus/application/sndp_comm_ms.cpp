#if defined(__SNDP_COMM_MS__)
#include "stdio.h"
#include "string.h"
#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "app_ibrt_customif_cmd.h"
#include "app_tws_ctrl_thread.h"


#include "sndp_if_common.h"
#include "sndp_if_platform.h"

#include "sndp_comm_main.h"
#include "sndp_comm_ms.h"




/**************************************************************************************************
* Constant
**************************************************************************************************/


/**************************************************************************************************
* Prototype
**************************************************************************************************/


/**************************************************************************************************
* Extern
**************************************************************************************************/


/**************************************************************************************************
* Variable
**************************************************************************************************/


/**************************************************************************************************
* Function
**************************************************************************************************/


int32_t sndp_comm_ms_send_data(uint8_t *data, uint16_t data_len)
{
    if(sndp_is_besaud_connected()) {
        tws_ctrl_send_cmd(APP_TWS_CMD_SNDP_MS_SYNC, (uint8_t *)data, data_len);
    } else {
        COMM_MS_TRACE(0, "tws is not connected.");
    }
    return 0;
}

int32_t sndp_comm_ms_recv_data(uint8_t *data, uint16_t data_len)
{
    COMM_MS_TRACE(1, "data_len=%d", data_len);
	sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_MS, data, data_len);  
	return 0;
}

int32_t sndp_comm_ms_init(void)
{	
	COMM_MS_TRACE(0, "done.");
	return 0;
}

#endif	/* __SNDP_COMM_MS__ */

