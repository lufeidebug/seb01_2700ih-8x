#ifndef __SNDP_IF_DEV_THREAD_H__
#define __SNDP_IF_DEV_THREAD_H__

#if defined(__SNDP_DEV_THREAD__)

#include "stdbool.h"


#ifdef __cplusplus
extern "C" {
#endif


#define DEV_THREAD_MAILBOX_MAX (20)

typedef enum {
    SNDP_DEV_THREAD_MODUAL_MAIN = 0,

    SNDP_DEV_THREAD_MODUAL_NUM
} sndp_dev_thread_modual_id_e;

typedef enum {
	SNDP_DEV_THREAD_MSGID_FUNC_CALL,
		
} sndp_dev_thread_msgid_e;



typedef struct {
    uint32_t message_id;
    uint32_t message_ptr;
    uint32_t message_Param0;
    uint32_t message_Param1;
    uint32_t message_Param2;
	
} sndp_dev_thread_message_body_s;

typedef struct {
    uint32_t src_thread;
    uint32_t dest_thread;
    uint32_t system_time;
    uint32_t mod_id;
    sndp_dev_thread_message_body_s msg_body;
	
} sndp_dev_thread_block_s;


typedef int (* sndp_dev_thread_modual_handler_t)(sndp_dev_thread_message_body_s *);

int sndp_dev_thread_mailbox_put(sndp_dev_thread_block_s* msg_src);
int sndp_dev_thread_mailbox_free(sndp_dev_thread_block_s* msg_p);
int sndp_dev_thread_mailbox_get(sndp_dev_thread_block_s** msg_p);

bool sndp_dev_thread_is_module_registered(sndp_dev_thread_modual_id_e mod_id);
int sndp_dev_thread_set_threadhandle(sndp_dev_thread_modual_id_e mod_id, sndp_dev_thread_modual_handler_t handler);
void *sndp_dev_thread_os_tid_get(void);
int sndp_dev_thread_os_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_DEV_THREAD__ */
#endif /* __SNDP_IF_DEV_THREAD_H__ */

