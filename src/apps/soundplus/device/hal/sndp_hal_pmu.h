#ifndef __SNDP_HAL_PMU_H__
#define __SNDP_HAL_PMU_H__

#if defined(__SNDP_PMU_MGR__)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	SNDP_HAL_PMU_CHARGER_PLUGIN,
	SNDP_HAL_PMU_CHARGER_PLUGOUT,

	SNDP_HAL_PMU_CHARGER_UNKNOWN,
} sndp_hal_pmu_charger_plug_status_e;


typedef void (*sndp_hal_pmu_charger_plug_status_changed_callback)(sndp_hal_pmu_charger_plug_status_e status);


uint32_t sndp_hal_pmu_init(void);
uint32_t sndp_hal_pmu_add_charger_plug_status_changed_callback(sndp_hal_pmu_charger_plug_status_changed_callback callback);
uint32_t sndp_hal_pmu_clear_charger_plug_status_changed_callback(sndp_hal_pmu_charger_plug_status_changed_callback callback);
sndp_hal_pmu_charger_plug_status_e sndp_hal_pmu_get_curr_plug_status(void);
void sndp_hal_pmu_force_check(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_PMU_MGR__ */

#endif /* __SNDP_HAL_PMU_H__ */
