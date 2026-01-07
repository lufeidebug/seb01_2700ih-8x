
#ifndef __SNDP_COVER_SWITCH_BOX_NOTIFY_H__
#define __SNDP_COVER_SWITCH_BOX_NOTIFY_H__

#if defined(__SNDP_COVER_SWITCH_BOX_NOTIFY__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_CS_NOTIFY_TRACE__
#ifdef __SNDP_CS_NOTIFY_TRACE__
#define CS_NOTIFY_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[CS_NOTIFY] %s, " str, __func__, ##__VA_ARGS__)
#else
#define CS_NOTIFY_TRACE(num, str, ...)
#endif


void sndp_box_notify_cs_status_changed(sndp_hal_cover_status_e cover_status);


extern const sndp_hal_cover_switch_s sndp_hal_cover_switch_box_notify;

#ifdef __cplusplus
}
#endif

#endif  //__SNDP_COVER_SWITCH_BOX_NOTIFY__
#endif  //__SNDP_COVER_SWITCH_BOX_NOTIFY_H__


