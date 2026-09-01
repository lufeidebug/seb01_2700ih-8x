#if defined(__SNDP_RF_TEST__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "hal_bootmode.h"
#include "tgt_hardware.h"
#include "app_utils.h"
#include "audioflinger.h"
#include "audio_dump.h"
#include "cqueue.h"
#include "math.h"

#include "sndp_rf_test.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_RF_TEST_TRACE__
#if defined(__SNDP_RF_TEST_TRACE__)
#define RF_TEST_TRACE(num, str, ...)                    SNDP_TRACE(1 + num, "[RF_TEST] %s, " str, __func__, ##__VA_ARGS__)
#else
#define RF_TEST_TRACE(num, str, ...)
#endif


#if defined(__SNDP_RF_DESENCE_TEST__)
#define DESENCE_TEST_FRAME_MS                           (15)	//MS

#define DESENCE_TEST_PLAYBACK_SAMPLE_RATE                   (AUD_SAMPRATE_16000)
#define DESENCE_TEST_PLAYBACK_SAMPLE_BITS                   (AUD_BITS_16)
#define DESENCE_TEST_PLAYBACK_SAMPLE_BYTE                   (1)
#define DESENCE_TEST_PLAYBACK_CHANN_NUMBER                  (AUD_CHANNEL_NUM_1)
#define DESENCE_TEST_PLAYBACK_FRAME_MS                      (DESENCE_TEST_FRAME_MS)	//MS
#define DESENCE_TEST_PLAYBACK_FRAME_LEN                     ((DESENCE_TEST_PLAYBACK_SAMPLE_RATE / 1000) * DESENCE_TEST_PLAYBACK_FRAME_MS)
#define DESENCE_TEST_PLAYBACK_FRAME_1CH_DATA_LEN            ((DESENCE_TEST_PLAYBACK_FRAME_LEN) * (DESENCE_TEST_PLAYBACK_SAMPLE_BYTE))
#define DESENCE_TEST_PLAYBACK_FRAME_ALLCH_DATA_LEN          ((DESENCE_TEST_PLAYBACK_FRAME_1CH_DATA_LEN) * (DESENCE_TEST_PLAYBACK_CHANN_NUMBER))

#define DESENCE_TEST_PLAYBACK_STREAM_BUFF_SIZE              (DESENCE_TEST_PLAYBACK_FRAME_ALLCH_DATA_LEN*2)		//pingpang buff 长度大一样
#define	DESENCE_TEST_PLAYBACK_STREAM_VOLUME                 (TGT_VOLUME_LEVEL_16)
#define DESENCE_TEST_PLAYBACK_STREAM_ID                     (AUD_STREAM_ID_1)

#endif

/**************************************************************************************************
* Prototype
**************************************************************************************************/



/**************************************************************************************************
* Variable
**************************************************************************************************/
#if defined(__SNDP_RF_DESENCE_TEST__)
static uint8_t desence_playback_stream_buff[DESENCE_TEST_PLAYBACK_STREAM_BUFF_SIZE];
static bool desence_playback_stream_is_running = false;

static const uint8_t desence_audio_data[] = {
    #include "res/rf_test/rf_desence_audio_mono_16khz_16bit.txt"
};

static const uint32_t c_desence_audio_data_size = sizeof(desence_audio_data)/sizeof(desence_audio_data[0]);

#endif



/**************************************************************************************************
* Function
**************************************************************************************************/

#if defined(__SNDP_RF_DESENCE_TEST__)
uint32_t desence_test_playback_more_data(uint8_t *buf, uint32_t len)
{
	if(!desence_playback_stream_is_running) {
		memset(buf, 0, len);
		return len;
	}

    
#if 1    
    static uint32_t offset = 0;
    int16_t *play_pcm16 = (int16_t *)buf;
    uint32_t play_pcm16_len = len / 2;
    int16_t *src_pcm16 = (int16_t *)desence_audio_data;
    uint32_t src_pcm16_size = c_desence_audio_data_size/2;

    //RF_TEST_TRACE(1, "play_len=%d, offset=%d, src_size=%d", play_pcm16_len, offset, src_pcm16_size);
    
    for(uint32_t i = 0; i < play_pcm16_len; i++) {
        if(offset >= src_pcm16_size) {
                offset = 0;
        }
        play_pcm16[i] = src_pcm16[offset++];
    }
#endif

    return len;
}

static uint32_t desence_test_playback_start(void) 
{
	RF_TEST_TRACE(2, "isRun=%d", desence_playback_stream_is_running);
	
	if(desence_playback_stream_is_running)
		return 0;

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_RF_DESENCE_TEST, APP_SYSFREQ_104M);

	struct AF_STREAM_CONFIG_T stream_cfg;
	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.sample_rate = DESENCE_TEST_PLAYBACK_SAMPLE_RATE;
	stream_cfg.bits = DESENCE_TEST_PLAYBACK_SAMPLE_BITS;
	stream_cfg.channel_num = DESENCE_TEST_PLAYBACK_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
	stream_cfg.vol = DESENCE_TEST_PLAYBACK_STREAM_VOLUME;
	stream_cfg.handler = desence_test_playback_more_data;
	stream_cfg.data_ptr = desence_playback_stream_buff;
	stream_cfg.data_size = DESENCE_TEST_PLAYBACK_STREAM_BUFF_SIZE;
	af_stream_open(DESENCE_TEST_PLAYBACK_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);

	af_stream_start(DESENCE_TEST_PLAYBACK_STREAM_ID, AUD_STREAM_PLAYBACK);

	desence_playback_stream_is_running = true;
	RF_TEST_TRACE(1, "starting");
	return 0;
}

static uint32_t desence_test_playback_stop(void) 
{
	RF_TEST_TRACE(2, "isRun=%d", desence_playback_stream_is_running);
	
	if(!desence_playback_stream_is_running)
		return 0;
	desence_playback_stream_is_running = false;

	af_stream_stop(DESENCE_TEST_PLAYBACK_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_close(DESENCE_TEST_PLAYBACK_STREAM_ID, AUD_STREAM_PLAYBACK);

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_RF_DESENCE_TEST, APP_SYSFREQ_32K);

	RF_TEST_TRACE(1, "stopped");
	return 0;
}

void sndp_rf_desece_test_playback_onoff(uint8_t onoff)
{
    RF_TEST_TRACE(1, "...");
    
    if(onoff) {
        desence_test_playback_start();
    } else {
        desence_test_playback_stop();
    }
}
#endif 	/* __SNDP_RF_DESENCE_TEST__ */



#endif	/* __SNDP_RF_TEST__ */


