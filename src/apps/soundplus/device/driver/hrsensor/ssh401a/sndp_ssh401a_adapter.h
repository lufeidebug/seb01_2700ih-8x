
#ifndef __SNDP_SSH401A_ADAPTER_H__
#define __SNDP_SSH401A_ADAPTER_H__
#if defined(__SNDP_HRSENSOR_SSH401A__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SSH401A_TRACE__

#ifdef __SSH401A_TRACE__
#define SSH401A_TRACE(num, str, ...)    SNDP_TRACE(1 + num, "[SSH401A] %s, " str, __func__, ##__VA_ARGS__)
#else
#define SSH401A_TRACE(num, str, ...)
#endif


extern const sndp_hal_hr_s sndp_hr_ssh401a;


#ifdef __cplusplus
}
#endif

#endif  //__SNDP_HRSENSOR_SSH401A__
#endif  //__SNDP_SSH401A_ADAPTER_H__


