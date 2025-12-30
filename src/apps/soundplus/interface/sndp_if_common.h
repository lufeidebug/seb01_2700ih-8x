#ifndef __SNDP_IF_COMMON_H__
#define __SNDP_IF_COMMON_H__

#if defined(__SNDP_PROJ__)



#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_IF_TRACE__
#if defined(__SNDP_IF_TRACE__)
#define SNDP_IF_LOG_TAG                   "[SNDP_IF]"
#define SNDP_IF_TRACE(num, str, ...)      SNDP_TRACE(1+num, SNDP_IF_LOG_TAG"%s, " str, __func__,  ##__VA_ARGS__)
#define SNDP_IF_TRACE_ENTER()             SNDP_TRACE(2,     SNDP_IF_LOG_TAG"%s, %d, ++++\n", __func__, __LINE__)
#define SNDP_IF_TRACE_EXIT()              SNDP_TRACE(2,     SNDP_IF_LOG_TAG"%s, %d, ----\n", __func__, __LINE__)
#define SNDP_IF_TRACE_DUMP                DUMP8
#else
#define SNDP_IF_TRACE(num, str, ...)
#define SNDP_IF_TRACE_ENTER()
#define SNDP_IF_TRACE_EXIT()
#define SNDP_IF_TRACE_DUMP
#endif


typedef enum {
	SNDP_COMIF_MSGID_FUNC_CALL,
		
} sndp_comif_msgid_e;


typedef void (*sndp_call_func_ptr)(uint32_t param0, uint32_t param1, uint32_t param2);

void sndp_delay_exec_start(uint32_t delay_ms, uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2);
void sndp_delay_exec_stop(uint32_t ptr);
void sndp_call_func_in_dev_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2);
void sndp_call_func_in_app_thread(uint32_t ptr, uint32_t param0, uint32_t param1, uint32_t param2);
void sndp_if_common_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */
#endif /* __SNDP_IF_COMMON_H__ */
