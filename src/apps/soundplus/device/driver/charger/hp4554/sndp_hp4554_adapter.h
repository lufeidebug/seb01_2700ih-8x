
#ifndef __SNDP_HP4554_ADAPTER_H__
#define __SNDP_HP4554_ADAPTER_H__
#if defined(__SNDP_CHARGER_HP4554__)

#ifdef __cplusplus
extern "C" {
#endif


#define __HP4554_TRACE__

#ifdef __HP4554_TRACE__
#define HP4554_TRACE(num, str, ...)   TRACE(1 + num, "[HP4554] %s, " str, __func__, ##__VA_ARGS__)
#else
#define HP4554_TRACE(num, str, ...)
#endif




extern const sndp_hal_charger_s sndp_hal_charger_hp4554;

#ifdef __cplusplus
}
#endif

#endif  //__SNDP_CHARGER_HP4554__
#endif  //__SNDP_HP4554_ADAPTER_H__





