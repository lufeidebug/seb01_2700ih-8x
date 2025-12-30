#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "app_thread.h"


#include "sndp_if_common.h"

#if defined(__SNDP_DEV_THREAD__)
#include "sndp_if_dev_thread.h"
#endif



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define COMIF_DELAY_EXEC_CNT		(20)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
	uint32_t ptr;
	uint32_t param0;
	uint32_t param1;
	uint32_t param2;

	uint32_t start_time;	/* unit:ms */
	uint32_t end_time;	/* unit:ms */

} sndp_delay_exec_func_s;

typedef struct {
	bool running;
	uint16_t exec_index;
	uint32_t end_time;	/* unit:ms */
} sndp_delay_exec_ctx_s;

static void sndp_delay_exec_checker(void);
static void sndp_delay_exec_timeout_handler(void const *param);


/**************************************************************************************************
* Variable
**************************************************************************************************/
osTimerDef(COMIF_DELAY_EXEC_TIMER, sndp_delay_exec_timeout_handler);
static osTimerId sndp_delay_exec_timer = NULL;
static sndp_delay_exec_func_s sndp_delay_exec_funcs[COMIF_DELAY_EXEC_CNT];
static sndp_delay_exec_ctx_s sndp_delay_exec_ctx;


/**************************************************************************************************
* Function
**************************************************************************************************/

static void sndp_delay_exec_clear_handler(uint16_t index)
{
	if(index >= COMIF_DELAY_EXEC_CNT) {
		SNDP_IF_TRACE(1, "Invalid exec_index(%d)", index);
		return;
	}

	memset(&sndp_delay_exec_funcs[index], 0, sizeof(sndp_delay_exec_func_s));
}

static void sndp_delay_exec_handler(void)
{
	sndp_delay_exec_func_s *p_func;

	if(sndp_delay_exec_ctx.exec_index != COMIF_DELAY_EXEC_CNT) {
		p_func = &sndp_delay_exec_funcs[sndp_delay_exec_ctx.exec_index];
		if(p_func->ptr) {
			//SNDP_IF_TRACE(1, "exec_index(%d)=%08X", sndp_delay_exec_ctx.exec_index, p_func->ptr);
			sndp_call_func_in_app_thread(p_func->ptr, p_func->param0, p_func->param1, p_func->param2);
			sndp_delay_exec_clear_handler(sndp_delay_exec_ctx.exec_index);
		} else {
			//SNDP_IF_TRACE(1, "exec_index(%d) has been cleared.", sndp_delay_exec_ctx.exec_index);
		}
		sndp_delay_exec_ctx.exec_index = COMIF_DELAY_EXEC_CNT;
	}
}

static void sndp_delay_exec_timeout_handler(void const *param)
{
	sndp_delay_exec_handler();
	sndp_delay_exec_checker();
}

static void sndp_delay_exec_checker(void)
{
	uint16_t i;
	uint32_t end_time_min = 0xFFFFFFFF;
	uint16_t exec_index = COMIF_DELAY_EXEC_CNT;
	uint32_t curr_time;
	uint32_t delay_time;

	for(i = 0; i < COMIF_DELAY_EXEC_CNT; i++) {
		if(sndp_delay_exec_funcs[i].ptr) {
			if(sndp_delay_exec_funcs[i].end_time < end_time_min) {
				end_time_min = sndp_delay_exec_funcs[i].end_time;
				exec_index = i;
			}
		}
	}

	curr_time = TICKS_TO_MS(hal_sys_timer_get());
	//SNDP_IF_TRACE(3,"exec_index=%d, curr_time=%d, end_time_min=%d", exec_index, curr_time, end_time_min);
	
	if(exec_index != COMIF_DELAY_EXEC_CNT) {
		if((curr_time > end_time_min) || (end_time_min - curr_time < 10))
			delay_time = 10;
		else
			delay_time = end_time_min - curr_time;

		sndp_delay_exec_ctx.exec_index = exec_index;
		sndp_delay_exec_ctx.running = true;
		sndp_delay_exec_ctx.end_time = end_time_min;
		
		//SNDP_IF_TRACE(3, "exec_index=%d, ptr=%08X, delay=%d", exec_index, sndp_delay_exec_funcs[exec_index].ptr, delay_time);
		osTimerStop(sndp_delay_exec_timer);
		osTimerStart(sndp_delay_exec_timer, delay_time);
	} else {
		//SNDP_IF_TRACE(0, "no func need to exec");
	}
}

void sndp_delay_exec_start(uint32_t delay_ms, uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	uint16_t i;
	sndp_delay_exec_func_s * p_delay_funcs;
	uint16_t exist_idx = COMIF_DELAY_EXEC_CNT;
	uint16_t empty_idx = COMIF_DELAY_EXEC_CNT;
	uint16_t index = COMIF_DELAY_EXEC_CNT;
	
	
	/* Check whether it already exists, and find the empty index */
	for(i = 0; i < COMIF_DELAY_EXEC_CNT; i++) {
		if(sndp_delay_exec_funcs[i].ptr == ptr) {
			exist_idx = i;
			break;
		}

		if((empty_idx == COMIF_DELAY_EXEC_CNT) && (sndp_delay_exec_funcs[i].ptr == 0)) {
			empty_idx = i;
		}
	}

	if(exist_idx != COMIF_DELAY_EXEC_CNT)
		index = exist_idx;
	else if(empty_idx != COMIF_DELAY_EXEC_CNT)
		index = empty_idx;
	else
		ASSERT(0, "%s, no space to fill", __func__);

	
	SNDP_IF_TRACE(1,"ptr=%08X, delay_ms=%d, index=%d", ptr, delay_ms, index);
	
	p_delay_funcs = &sndp_delay_exec_funcs[index];
	p_delay_funcs->ptr = ptr;
	p_delay_funcs->param0 = param0;
	p_delay_funcs->param1 = param1;
	p_delay_funcs->param2 = param2;
	p_delay_funcs->start_time = TICKS_TO_MS(hal_sys_timer_get());
	p_delay_funcs->end_time = p_delay_funcs->start_time + delay_ms;

	sndp_call_func_in_app_thread((uint32_t)sndp_delay_exec_checker, 0, 0, 0);
}

void sndp_delay_exec_stop(uint32_t ptr)
{
	uint16_t exist_idx = COMIF_DELAY_EXEC_CNT;

	//osTimerStop(sndp_delay_exec_timer);

	/* Check whether it already exists, and find the empty index */
	for(uint16_t i = 0; i < COMIF_DELAY_EXEC_CNT; i++) {
		if(sndp_delay_exec_funcs[i].ptr == ptr) {
			exist_idx = i;
			break;
		}
	}

	if(exist_idx != COMIF_DELAY_EXEC_CNT) {
		//SNDP_IF_TRACE(2, "ptr=%08X, exist_idx=%d, stoped", ptr, exist_idx);
		sndp_delay_exec_clear_handler(exist_idx);
	} else {
		//SNDP_IF_TRACE(1, "ptr=%08X, is not in the list.", ptr);
	}
	
	//sndp_delay_exec_checker();
}


#if defined(__SNDP_DEV_THREAD__)
static int sndp_dev_modual_handle_process(sndp_dev_thread_message_body_s *msg_body)
{
	switch(msg_body->message_id) {
		case SNDP_DEV_THREAD_MSGID_FUNC_CALL:
			//SNDP_IF_TRACE(1,"COMIF_MSGID_FUNC_CALL,ptr:%08X", msg_body->message_ptr);
			if(msg_body->message_ptr != 0) {
				((sndp_call_func_ptr)(msg_body->message_ptr))(msg_body->message_Param0, msg_body->message_Param1, msg_body->message_Param2);
			}
			break;
	}

	return 0;
}

void sndp_call_func_in_dev_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	sndp_dev_thread_block_s msg;

	//SNDP_IF_TRACE(1,"ptr:%08X", ptr);
	if(!sndp_dev_thread_is_module_registered(SNDP_DEV_THREAD_MODUAL_MAIN)) {
		SNDP_IF_TRACE(0,"SNDP_DEV_THREAD_MODUAL_MAIN is not registered");
		return;
	}
	
    msg.mod_id = SNDP_DEV_THREAD_MODUAL_MAIN;
    msg.msg_body.message_id = SNDP_DEV_THREAD_MSGID_FUNC_CALL;
    msg.msg_body.message_ptr = ptr;
	msg.msg_body.message_Param0 = param0;
	msg.msg_body.message_Param1 = param1;
	msg.msg_body.message_Param2 = param2;
    sndp_dev_thread_mailbox_put(&msg);
}

#else
void sndp_call_func_in_dev_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	sndp_call_func_in_app_thread(ptr, param0, param1, param2);
}

#endif


static int sndp_app_module_handle_process(APP_MESSAGE_BODY *msg_body)
{
	switch(msg_body->message_id) {
		case SNDP_COMIF_MSGID_FUNC_CALL:
			((sndp_call_func_ptr)(msg_body->message_ptr))(msg_body->message_Param0, msg_body->message_Param1, msg_body->message_Param2);
			break;
	}

	return 0;
}

void sndp_call_func_in_app_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	APP_MESSAGE_BLOCK msg;

	SNDP_IF_TRACE(1,"ptr:%08X", ptr);
	if(!app_is_module_registered(APP_MODULE_SNDP_APP)) {
		SNDP_IF_TRACE(0,"APP_MODUAL_SNDP_DEV is not registered");
		return;
	}
	
    msg.mod_id = APP_MODULE_SNDP_APP;
    msg.msg_body.message_id = SNDP_COMIF_MSGID_FUNC_CALL;
    msg.msg_body.message_ptr = ptr;
	msg.msg_body.message_Param0 = param0;
	msg.msg_body.message_Param1 = param1;
	msg.msg_body.message_Param2 = param2;
    app_mailbox_put(&msg);
}

void sndp_if_common_init(void)
{
	SNDP_IF_TRACE(1,"%s", __func__);
	
    app_set_threadhandle(APP_MODULE_SNDP_APP, sndp_app_module_handle_process);
    
#if defined(__SNDP_DEV_THREAD__)
	sndp_dev_thread_os_init();
	sndp_dev_thread_set_threadhandle(SNDP_DEV_THREAD_MODUAL_MAIN, sndp_dev_modual_handle_process);
#endif

	if(sndp_delay_exec_timer == NULL)
        sndp_delay_exec_timer = osTimerCreate (osTimer(COMIF_DELAY_EXEC_TIMER), osTimerOnce, NULL);

	memset(sndp_delay_exec_funcs, 0, sizeof(sndp_delay_exec_funcs));
	sndp_delay_exec_ctx.running = false;
	sndp_delay_exec_ctx.exec_index = COMIF_DELAY_EXEC_CNT;
	sndp_delay_exec_ctx.end_time = 0;
}

#endif	/* __SNDP_PROJ__ */


