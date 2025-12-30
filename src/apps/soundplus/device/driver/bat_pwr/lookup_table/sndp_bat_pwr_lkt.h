
#ifndef __SNDP_BAT_LKT_H__
#define __SNDP_BAT_LKT_H__

#if defined(__SNDP_BAT_LOOKUP_TABLE__)

#ifdef __cplusplus
extern "C" {
#endif


#define __BAT_LKT_TRACE__

#ifdef __BAT_LKT_TRACE__
#define BAT_LKT_TRACE(num, str, ...)   TRACE(1 + num, "[BAT_LKT] %s, " str, __func__, ##__VA_ARGS__)
#else
#define BAT_LKT_TRACE(num, str, ...)
#endif


#define SP_BAT_STABLE_COUNT				(5)

typedef struct {
    uint8_t charge_status;

    sndp_hal_bat_info_s last;
    sndp_hal_bat_info_s curr;

	uint16_t measure_volt[SP_BAT_STABLE_COUNT];
	uint16_t measure_index;

} sndp_bat_lkt_ctx_s;


extern const sndp_hal_battery_s sndp_hal_bat_lkt;


#ifdef __cplusplus
}
#endif

#endif  //__SNDP_BAT_LOOKUP_TABLE__
#endif  //__SNDP_BAT_LKT_H__





