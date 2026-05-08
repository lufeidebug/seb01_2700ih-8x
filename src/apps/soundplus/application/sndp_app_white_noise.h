#ifndef __SNDP_APP_WHITE_NOISE_H__
#define __SNDP_APP_WHITE_NOISE_H__

#if defined(__SNDP_APP_WHITE_NOISE__)

#ifdef __cplusplus
extern "C" {
#endif


uint32_t sndp_white_noise_play_start(void);
uint32_t sndp_white_noise_play_stop(void);
bool sndp_white_noise_is_running(void);
bool sndp_white_noise_is_turnon(void);
void sndp_white_noise_turnon_and_play(bool play);
void sndp_white_noise_turnoff(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_APP_WHITE_NOISE__ */
#endif /* __SNDP_APP_WHITE_NOISE_H__ */

