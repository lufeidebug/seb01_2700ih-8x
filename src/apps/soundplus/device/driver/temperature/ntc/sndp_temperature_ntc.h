#ifndef __SPDRV_TEMPERATURE_NTC_H__
#define __SPDRV_TEMPERATURE_NTC_H__

#if defined(__SNDP_TEMPERATURE_NTC__)


#ifdef __cplusplus
extern "C" {
#endif

#define TEMP_NTC_STABLE_COUNT				(5)


#define __TEMP_NTC_DEBUG__
#if defined(__TEMP_NTC_DEBUG__)
#define TEMP_NTC_LOG_TAG					"[TEMP_NTC]"
#define TEMP_NTC_TRACE(num, str, ...)   	SNDP_TRACE(1 + num, TEMP_NTC_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#else
#define TEMP_NTC_TRACE(num, str, ...)	
#endif


typedef struct {
	uint16_t measure_volt[TEMP_NTC_STABLE_COUNT];
	uint16_t measure_index;

	uint16_t voltage;

	int16_t last_temp;
    int16_t curr_temp;

} sndp_ntc_ctx_s;

typedef struct {
	int16_t temperature;
	uint32_t resistence;
    
} sndp_ntc_temp_table_s;



extern const sndp_hal_temperature_s sndp_hal_temperature_ntc;

#ifdef __cplusplus
	}
#endif


#endif /* _SNDP_TEMPERATURE_NTC__ */
#endif /* __SPDRV_TEMPERATURE_NTC_H__ */
