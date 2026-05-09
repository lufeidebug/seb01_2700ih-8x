#ifndef __SNDP_HAL_TEMPERATURE_H__
#define __SNDP_HAL_TEMPERATURE_H__

#if defined(__SNDP_TEMPERATURE_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*sndp_hal_temperature_measure_callback)(int16_t temperature, uint16_t voltage);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

    /** 
     * callback: 
     * return: 0 no error.
     */
    int32_t (* set_measure_callback)(sndp_hal_temperature_measure_callback callback);

	/**
     * return: 0 no error. 
     */
    int32_t (* measure)(void);

} sndp_hal_temperature_s;


int32_t sndp_hal_temperature_init(void);
int32_t sndp_hal_temperature_set_measure_callback(sndp_hal_temperature_measure_callback callback);
int32_t sndp_hal_temperature_measure(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_TEMPERATURE_MGR__ */

#endif /* __SNDP_HAL_TEMPERATURE_H__ */

