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





void sndp_hr_init(void);



#ifdef __cplusplus
	}
#endif

#endif	/* __SNDP_HEART_RATE_MGR__*/
#endif	/* __SNDP_HEART_RATE_H__*/

