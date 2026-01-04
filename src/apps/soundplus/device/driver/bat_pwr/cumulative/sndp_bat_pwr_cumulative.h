#ifndef __SNDP_BAT_PWR_CUMULATIVE_H__
#define __SNDP_BAT_PWR_CUMULATIVE_H__

#if defined(__SNDP_BAT_CUMULATIVE__)

#ifdef __cplusplus
extern "C" {
#endif

#define __BAT_CUMULATIVE_TRACE__

#ifdef __BAT_CUMULATIVE_TRACE__
#define BAT_CUMU_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[BAT_CUMU] %s, " str, __func__, ##__VA_ARGS__)
#else
#define BAT_CUMU_TRACE(num, str, ...)
#endif

extern const sndp_hal_battery_s sndp_hal_bat_cumulative;

#ifdef __cplusplus
}
#endif

#endif /* __SNDP_BAT_CUMULATIVE__ */

#endif /* __SNDP_BAT_PWR_CUMULATIVE_H__ */