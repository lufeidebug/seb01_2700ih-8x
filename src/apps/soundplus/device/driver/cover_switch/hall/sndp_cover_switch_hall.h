
#ifndef __SNDP_COVER_SWITCH_HALL_H__
#define __SNDP_COVER_SWITCH_HALL_H__

#if defined(__SNDP_COVER_SWITCH_HALL__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_CS_HALL_TRACE__

#ifdef __SNDP_CS_HALL_TRACE__
#define CS_HALL_TRACE(num, str, ...)   TRACE(1 + num, "[CS_HALL] %s, " str, __func__, ##__VA_ARGS__)
#else
#define CS_HALL_TRACE(num, str, ...)
#endif


extern const sndp_hal_cover_switch_s sndp_hal_cover_switch_hall;

#ifdef __cplusplus
}
#endif

#endif  //__SNDP_COVER_SWITCH_HALL__
#endif  //__SNDP_COVER_SWITCH_HALL_H__


