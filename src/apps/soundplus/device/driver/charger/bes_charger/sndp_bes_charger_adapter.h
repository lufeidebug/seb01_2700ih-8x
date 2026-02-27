
#ifndef __SNDP_BES_CHARGER_ADAPTER_H__
#define __SNDP_BES_CHARGER_ADAPTER_H__
#if defined(__SNDP_CHARGER_BES__)

#ifdef __cplusplus
extern "C" {
#endif


#define __BES_CHARGER_TRACE__

#ifdef __BES_CHARGER_TRACE__
#define BESCHG_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[BESCHG] %s, " str, __func__, ##__VA_ARGS__)
#else
#define BESCHG_TRACE(num, str, ...)
#endif




#ifdef __cplusplus
}
#endif

#endif  //__SNDP_CHARGER_BES__
#endif  //__SNDP_BES_CHARGER_ADAPTER_H__





