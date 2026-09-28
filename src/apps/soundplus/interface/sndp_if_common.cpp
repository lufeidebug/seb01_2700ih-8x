#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "app_thread.h"
#include "cmsis.h"


#include "sndp_if_common.h"

#if defined(__SNDP_DEV_THREAD__)
#include "sndp_if_dev_thread.h"
#endif



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define COMIF_DELAY_EXEC_CNT		(20)

/* Delay exec runs on its own thread. Its priority is deliberately lower than
 * af_thread(osPriorityHigh) so that a long-running sync task can never starve
 * the audio stream and cause A2DP DMA underrun. */
#define COMIF_DELAY_EXEC_THREAD_STACK	(1024*3)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
	bool in_app;			/* true: post to app thread; false: run in delay thread */
	uint32_t ptr;
	uint32_t param0;
	uint32_t param1;
	uint32_t param2;

	uint32_t start_time;	/* unit:ms, for debug */
	uint32_t end_time;	/* unit:ms */

} sndp_delay_exec_func_s;

static void sndp_delay_exec_thread(void const *argument);
static uint16_t sndp_delay_exec_find_earliest(uint32_t *remain_ms);


/**************************************************************************************************
* Variable
**************************************************************************************************/
osThreadDef(sndp_delay_exec_thread, osPriorityAboveNormal, 1, COMIF_DELAY_EXEC_THREAD_STACK, "sndp_delay_exec");
static osThreadId sndp_delay_exec_thread_tid = NULL;

osSemaphoreDef(COMIF_DELAY_EXEC_SEM);
static osSemaphoreId sndp_delay_exec_sem = NULL;
static sndp_delay_exec_func_s sndp_delay_exec_funcs[COMIF_DELAY_EXEC_CNT];


/**************************************************************************************************
* Function
**************************************************************************************************/

/* Pick the pending task with the earliest deadline.
 * Returns COMIF_DELAY_EXEC_CNT when the list is empty. */
static uint16_t sndp_delay_exec_find_earliest(uint32_t *remain_ms)
{
	uint16_t i;
	uint16_t index = COMIF_DELAY_EXEC_CNT;
	uint32_t end_time_min = 0xFFFFFFFF;
	uint32_t curr_time;
	uint32_t lock;

	*remain_ms = 0;

	lock = int_lock();
	for(i = 0; i < COMIF_DELAY_EXEC_CNT; i++) {
		if(sndp_delay_exec_funcs[i].ptr) {
			if(sndp_delay_exec_funcs[i].end_time < end_time_min) {
				end_time_min = sndp_delay_exec_funcs[i].end_time;
				index = i;
			}
		}
	}
	curr_time = TICKS_TO_MS(hal_sys_timer_get());
	int_unlock(lock);

	if(index != COMIF_DELAY_EXEC_CNT) {
		*remain_ms = (curr_time >= end_time_min) ? 0 : (end_time_min - curr_time);
	}

	return index;
}

static void sndp_delay_exec_thread(void const *argument)
{
	sndp_delay_exec_func_s delay_func;
	uint16_t index;
	uint32_t remain_ms;
	uint32_t lock;

	while(1) {
		index = sndp_delay_exec_find_earliest(&remain_ms);

		if(index == COMIF_DELAY_EXEC_CNT) {
			/* nothing pending, sleep until a new task is queued */
			osSemaphoreWait(sndp_delay_exec_sem, osWaitForever);
			continue;
		}

		if(remain_ms > 0) {
			/* interruptible wait: a newly queued earlier task wakes us up */
			osSemaphoreWait(sndp_delay_exec_sem, remain_ms);
			continue;
		}

		/* deadline reached: take a local copy then clear the slot */
		lock = int_lock();
		delay_func = sndp_delay_exec_funcs[index];
		memset(&sndp_delay_exec_funcs[index], 0, sizeof(sndp_delay_exec_func_s));
		int_unlock(lock);

		if(delay_func.ptr) {
			SNDP_IF_TRACE(1, "exec ptr=%08X, index=%d, in_app=%d", delay_func.ptr, index, delay_func.in_app);
			if(delay_func.in_app) {
				sndp_call_func_in_app_thread(delay_func.ptr, delay_func.param0, delay_func.param1, delay_func.param2);
			} else {
				((sndp_call_func_ptr)delay_func.ptr)(delay_func.param0, delay_func.param1, delay_func.param2);
			}
		}
	}
}

static void sndp_delay_exec_start_impl(uint32_t delay_ms, uint32_t ptr, bool in_app, uint32_t param0, uint32_t param1, uint32_t param2)
{
	uint16_t i;
	sndp_delay_exec_func_s * p_delay_funcs;
	uint16_t exist_idx = COMIF_DELAY_EXEC_CNT;
	uint16_t empty_idx = COMIF_DELAY_EXEC_CNT;
	uint16_t index = COMIF_DELAY_EXEC_CNT;
	uint32_t lock;

	lock = int_lock();

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

	if(exist_idx != COMIF_DELAY_EXEC_CNT) {
		index = exist_idx;
	} else if(empty_idx != COMIF_DELAY_EXEC_CNT) {
		index = empty_idx;
	} else {
		int_unlock(lock);
		ASSERT(0, "%s, no space to fill", __func__);
		return;
	}

	p_delay_funcs = &sndp_delay_exec_funcs[index];
	p_delay_funcs->in_app = in_app;
	p_delay_funcs->ptr = ptr;
	p_delay_funcs->param0 = param0;
	p_delay_funcs->param1 = param1;
	p_delay_funcs->param2 = param2;
	p_delay_funcs->start_time = TICKS_TO_MS(hal_sys_timer_get());
	p_delay_funcs->end_time = p_delay_funcs->start_time + delay_ms;

	int_unlock(lock);

	SNDP_IF_TRACE(1,"ptr=%08X, delay_ms=%d, index=%d, in_app=%d", ptr, delay_ms, index, in_app);

	/* wake up the delay thread to re-evaluate the earliest deadline */
	osSemaphoreRelease(sndp_delay_exec_sem);
}

/* Run the callback in the delay thread (default). */
void sndp_delay_exec_start(uint32_t delay_ms, uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	sndp_delay_exec_start_impl(delay_ms, ptr, false, param0, param1, param2);
}

/* Only do the timing in the delay thread, run the callback in app thread. */
void sndp_delay_exec_start_in_app(uint32_t delay_ms, uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	sndp_delay_exec_start_impl(delay_ms, ptr, true, param0, param1, param2);
}

void sndp_delay_exec_stop(uint32_t ptr)
{
	uint16_t i;
	uint16_t exist_idx = COMIF_DELAY_EXEC_CNT;
	uint32_t lock;

	lock = int_lock();

	/* Check whether it already exists, and clear it */
	for(i = 0; i < COMIF_DELAY_EXEC_CNT; i++) {
		if(sndp_delay_exec_funcs[i].ptr == ptr) {
			exist_idx = i;
			break;
		}
	}

	if(exist_idx != COMIF_DELAY_EXEC_CNT) {
		memset(&sndp_delay_exec_funcs[exist_idx], 0, sizeof(sndp_delay_exec_func_s));
	}

	int_unlock(lock);

	/* let the delay thread re-evaluate: the stopped task may be the earliest one */
	osSemaphoreRelease(sndp_delay_exec_sem);
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
            if(msg_body->message_ptr != 0) {
			    ((sndp_call_func_ptr)(msg_body->message_ptr))(msg_body->message_Param0, msg_body->message_Param1, msg_body->message_Param2);
            }
            break;
	}

	return 0;
}

void sndp_call_func_in_app_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2)
{
	APP_MESSAGE_BLOCK msg;

	//SNDP_IF_TRACE(1,"ptr:%08X", ptr);
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

	memset(sndp_delay_exec_funcs, 0, sizeof(sndp_delay_exec_funcs));

	/* sndp_if_common_init may be called more than once; only create once */
	if(sndp_delay_exec_sem == NULL) {
		sndp_delay_exec_sem = osSemaphoreCreate(osSemaphore(COMIF_DELAY_EXEC_SEM), 0);
		ASSERT(sndp_delay_exec_sem != NULL, "%s, sndp_delay_exec_sem == NULL", __func__);
	}

	if(sndp_delay_exec_thread_tid == NULL) {
		sndp_delay_exec_thread_tid = osThreadCreate(osThread(sndp_delay_exec_thread), NULL);
		ASSERT(sndp_delay_exec_thread_tid != NULL, "%s, sndp_delay_exec_thread_tid == NULL", __func__);
	}
}

#endif	/* __SNDP_PROJ__ */

