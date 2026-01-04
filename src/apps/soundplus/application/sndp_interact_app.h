#ifndef __SNDP_INTERACT_APP_H__
#define __SNDP_INTERACT_APP_H__

#if defined(__SNDP_PROJ__)



#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_INTERACT_APP_TRACE__

#if defined(__SNDP_INTERACT_APP_TRACE__)
#define INTERACT_APP_TRACE(num, str, ...)       SNDP_TRACE(1 + num,	"[INTERACT_APP] %s, " str, __func__, ##__VA_ARGS__)
#else
#define INTERACT_APP_TRACE(num, str, ...)
#endif



int32_t sndp_interact_app_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */
#endif /* __SNDP_INTERACT_APP_H__ */

