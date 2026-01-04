#ifndef __SNDP_INTERACT_BOX_H__
#define __SNDP_INTERACT_BOX_H__

#if defined(__SNDP_PROJ__)




#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_INTERACT_BOX_TRACE__

#if defined(__SNDP_INTERACT_BOX_TRACE__)
#define INTERACT_BOX_TRACE(num, str, ...)       SNDP_TRACE(1 + num,	"[INTERACT_BOX] %s, " str, __func__, ##__VA_ARGS__)
#else
#define INTERACT_BOX_TRACE(num, str, ...)
#endif



int32_t sndp_interact_box_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PROJ__ */
#endif /* __SNDP_INTERACT_BOX_H__ */

