#ifndef __SNDP_AUDIO_TEST_H__
#define __SNDP_AUDIO_TEST_H__

#if defined(__SNDP_AUDIO_TEST__)

#ifdef __cplusplus
extern "C" {
#endif



uint32_t sndp_audio_test_start16k16bit(void);
uint32_t sndp_audio_test_start48k16bit(void);
uint32_t sndp_audio_test_start48k24bit(void);
uint32_t sndp_audio_test_stop(void);
void sndp_audio_test_switch(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_ALG_MGR__ */
#endif /* __SNDP_AUDIO_TEST_H__ */

