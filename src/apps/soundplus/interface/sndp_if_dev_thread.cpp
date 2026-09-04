#if defined(__SNDP_DEV_THREAD__)
#include "cmsis_os.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "app_utils.h"

#include "sndp_if_common.h"
#include "sndp_if_dev_thread.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#ifndef DEV_THREAD_STACK_SIZE
#define DEV_THREAD_STACK_SIZE 				(1024*2)
#endif


/**************************************************************************************************
* Prototype
**************************************************************************************************/
static void sndp_dev_thread(void const *argument);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_dev_thread_modual_handler_t sndp_dev_thread_mod_handler[SNDP_DEV_THREAD_MODUAL_NUM];

osThreadDef(sndp_dev_thread, osPriorityNormal, 1, DEV_THREAD_STACK_SIZE, "sndp_dev_thread");
osMailQDef (sndp_dev_thread_mailbox, DEV_THREAD_MAILBOX_MAX, sndp_dev_thread_block_s);
static osMailQId sndp_dev_thread_mailbox = NULL;
static uint8_t sndp_dev_thread_mailbox_cnt = 0;
osThreadId sndp_dev_thread_tid;



/**************************************************************************************************
* Function
**************************************************************************************************/

static int sndp_dev_thread_mailbox_init(void)
{
    sndp_dev_thread_mailbox = osMailCreate(osMailQ(sndp_dev_thread_mailbox), NULL);
    if (sndp_dev_thread_mailbox == NULL)  {
        SNDP_IF_TRACE(0, "Failed to Create sndp_dev_thread_mailbox\n");
        return -1;
    }
    sndp_dev_thread_mailbox_cnt = 0;

    return 0;
}

int sndp_dev_thread_mailbox_put(sndp_dev_thread_block_s* msg_src)
{
    osStatus status;
    sndp_dev_thread_block_s *msg_p = NULL;

    msg_p = (sndp_dev_thread_block_s*)osMailAlloc(sndp_dev_thread_mailbox, 0);

    if (!msg_p){
        osEvent evt;
        SNDP_TRACE_IMM(0,"osMailAlloc error dump");
	
        for (uint8_t i = 0; i < DEV_THREAD_MAILBOX_MAX; i++) {
            evt = osMailGet(sndp_dev_thread_mailbox, 0);
            if (evt.status == osEventMail) {
                SNDP_TRACE_IMM(9,"cnt:%d mod:%d src:%08x tim:%d id:%8x ptr:%08x para:%08x/%08x/%08x", 
							i,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->mod_id,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->src_thread,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->system_time,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->msg_body.message_id,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->msg_body.message_ptr,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->msg_body.message_Param0,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->msg_body.message_Param1,
					       ((sndp_dev_thread_block_s *)(evt.value.p))->msg_body.message_Param2);
            } else{                
                SNDP_TRACE_IMM(2,"cnt:%d %d", i, evt.status); 
                break;
            }
        }
        SNDP_TRACE_IMM(0,"osMailAlloc error dump end");
    }
    
    ASSERT(msg_p, "osMailAlloc error");
    msg_p->src_thread = (uint32_t)osThreadGetId();
    msg_p->dest_thread = (uint32_t)NULL;
    msg_p->system_time = hal_sys_timer_get();
    msg_p->mod_id = msg_src->mod_id;
    msg_p->msg_body.message_id = msg_src->msg_body.message_id;
    msg_p->msg_body.message_ptr = msg_src->msg_body.message_ptr;
    msg_p->msg_body.message_Param0 = msg_src->msg_body.message_Param0;
    msg_p->msg_body.message_Param1 = msg_src->msg_body.message_Param1;
    msg_p->msg_body.message_Param2 = msg_src->msg_body.message_Param2;

    status = osMailPut(sndp_dev_thread_mailbox, msg_p);
    if (osOK == status)
        sndp_dev_thread_mailbox_cnt++;
    return (int)status;
}


int sndp_dev_thread_mailbox_free(sndp_dev_thread_block_s* msg_p)
{
    osStatus status;

    status = osMailFree(sndp_dev_thread_mailbox, msg_p);
    if (osOK == status)
        sndp_dev_thread_mailbox_cnt--;

    return (int)status;
}

int sndp_dev_thread_mailbox_get(sndp_dev_thread_block_s** msg_p)
{
    osEvent evt;
    evt = osMailGet(sndp_dev_thread_mailbox, osWaitForever);
    if (evt.status == osEventMail) {
        *msg_p = (sndp_dev_thread_block_s *)evt.value.p;
        return 0;
    }
    return -1;
}

static void sndp_dev_thread(void const *argument)
{
    while(1){
        sndp_dev_thread_block_s *msg_p = NULL;

        if (!sndp_dev_thread_mailbox_get(&msg_p)) {
            if (msg_p->mod_id < SNDP_DEV_THREAD_MODUAL_NUM) {
                if (sndp_dev_thread_mod_handler[msg_p->mod_id]) {
                    int ret = sndp_dev_thread_mod_handler[msg_p->mod_id](&(msg_p->msg_body));
                    if (ret)
                        SNDP_TRACE(2,"mod_handler[%d] ret=%d", msg_p->mod_id, ret);
                }
            }
            sndp_dev_thread_mailbox_free(msg_p);
        }
    }
}

bool sndp_dev_thread_is_module_registered(sndp_dev_thread_modual_id_e mod_id)
{
    if (mod_id >= SNDP_DEV_THREAD_MODUAL_NUM)
        return false;
    
    return (sndp_dev_thread_mod_handler[mod_id] != NULL);
}

int sndp_dev_thread_set_threadhandle(sndp_dev_thread_modual_id_e mod_id, sndp_dev_thread_modual_handler_t handler)
{
    if (mod_id >= SNDP_DEV_THREAD_MODUAL_NUM)
        return -1;

    sndp_dev_thread_mod_handler[mod_id] = handler;
    return 0;
}

void* sndp_dev_thread_os_tid_get(void)
{
    return (void *)sndp_dev_thread_tid;
}

int sndp_dev_thread_os_init(void)
{
    if (sndp_dev_thread_mailbox_init())
        return -1;

    sndp_dev_thread_tid = osThreadCreate(osThread(sndp_dev_thread), NULL);
    if (sndp_dev_thread_tid == NULL)  {
        SNDP_TRACE(0,"Failed to create sndp_dev_thread\n");
        return 0;
    }

    return 0;
}

#endif	/* __SNDP_DEV_THREAD__ */



