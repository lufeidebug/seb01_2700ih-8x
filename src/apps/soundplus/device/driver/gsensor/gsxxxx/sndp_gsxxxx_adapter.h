
#ifndef __SPDRV_GSXXXX_ADAPTER_H__
#define __SPDRV_GSXXXX_ADAPTER_H__
#if defined(__SNDP_GSENSOR_XXXX__)

#ifdef __cplusplus
extern "C" {
#endif


#define __GSXXXX_TRACE__

#ifdef __GSXXXX_TRACE__
#define GSXXXX_TRACE(num, str, ...)   TRACE(1 + num, "[GSXXXX] %s, " str, __func__, ##__VA_ARGS__)
#else
#define GSXXXX_TRACE(num, str, ...)
#endif



#ifdef __cplusplus
}
#endif

#endif  //__SNDP_GSENSOR_XXXX__
#endif  //__SPDRV_GSXXXX_ADAPTER_H__





