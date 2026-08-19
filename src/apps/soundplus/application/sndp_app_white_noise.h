#ifndef __SNDP_APP_WHITE_NOISE_H__
#define __SNDP_APP_WHITE_NOISE_H__

#if defined(__SNDP_APP_WHITE_NOISE__)

#ifdef __cplusplus
extern "C" {
#endif


bool sndp_white_noise_is_playing(void);
bool sndp_white_noise_is_turnon(void);
void sndp_white_noise_onoff_sync_recv(bool onoff);
void sndp_white_noise_onoff(bool onoff, bool need_sync);
void sndp_white_noise_resume(void);
void sndp_white_noise_pause(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_APP_WHITE_NOISE__ */
#endif /* __SNDP_APP_WHITE_NOISE_H__ */

