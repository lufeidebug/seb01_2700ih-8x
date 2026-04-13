#ifndef __SNDP_HEART_RATE_H__
#define __SNDP_HEART_RATE_H__

#if defined(__SNDP_HEART_RATE_MGR__)



#ifdef __cplusplus
extern "C" {
#endif



#define __SNDP_HR_DEBUG__

#if defined(__SNDP_HR_DEBUG__)
#define HR_LOG_TAG                  "[HR]"
#define HR_TRACE(num, str, ...)     SNDP_TRACE(num, HR_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#define HR_DUMP					    DUMP8
#else
#define HR_TRACE(num, str, ...)
#define HR_DUMP
#endif



/**
 * @brief       Start heartrate mearsuring
 * @param[in]   ppg_sampling_rate  1:64Hz, 2:128Hz, 3:256Hz
 * @param[in]   dump_state  Dump Data enable, 0:OFF, 1:ON
 * @return      void
 */
void sndp_hr_mearsuring_start(int8_t ppg_sampling_rate, uint8_t dump_state);

/**
 * @brief       Stop heartrate mearsuring
 * @return      void
 */
void sndp_hr_mearsuring_stop(void);

/**
 * @brief       Start heartrate mearsuring
 * @param[in]   sleep_control default:0
 * @return      void
 */
void sndp_sleep_analysis_start(int32_t sleep_control);

/**
 * @brief       Stop sleep analysis.
 * @return      void
 */
void sndp_sleep_analysis_stop(void);

/**
 * @brief       Start PPG notifiycation.
 * @return      void
 */
void sndp_ppg_notification_start(void);

/**
 * @brief       Stop PPG notifiycation.
 * @return      void
 */
void sndp_ppg_notification_stop(void);


/**
 * @brief       Heartrate app init.
 * @return      void
 */
void sndp_hr_app_init(void);



#ifdef __cplusplus
	}
#endif

#endif	/* __SNDP_HEART_RATE_MGR__*/
#endif	/* __SNDP_HEART_RATE_H__*/

