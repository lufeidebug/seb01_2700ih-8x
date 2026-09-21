
#ifndef __SPDRV_DA217E_ADAPTER_H__
#define __SPDRV_DA217E_ADAPTER_H__
#if defined(__SNDP_GSENSOR_DA217E__)

#ifdef __cplusplus
extern "C" {
#endif


#define __DA217E_TRACE__

#ifdef __DA217E_TRACE__
#define DA217E_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[DA217E] %s, " str, __func__, ##__VA_ARGS__)
#else
#define DA217E_TRACE(num, str, ...)
#endif



#ifdef __cplusplus
}
#endif

#endif  //__SNDP_GSENSOR_DA217E__
#endif  //__SPDRV_DA217E_ADAPTER_H__


