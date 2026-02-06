#if defined(__SNDP_APP_WHITE_NOISE__)
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

#include "sndp_audio_test.h"

/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_WN_TRACE__
#if defined(__SNDP_WN_TRACE__)
#define WN_TRACE(num, str, ...)                 MAIN_TRACE(1 + num, "[WN] %s, " str, __func__, ##__VA_ARGS__)
#else
#define WN_TRACE(num, str, ...)
#endif


#define SNDP_WN_PLAY_SAMPLE_RATE                    (AUD_SAMPRATE_44100)
#define SNDP_WN_PLAY_SAMPLE_BITS                    (AUD_BITS_16)
#define SNDP_WN_PLAY_SAMPLE_BYTE                    (2)
#define SNDP_WN_PLAY_CHANN_NUMBER                   (AUD_CHANNEL_NUM_1)
#define SNDP_WN_PLAY_FRAME_MS                       (15)	//MS
#define SNDP_WN_PLAY_FRAME_LEN                      ((SNDP_WN_PLAY_FRAME_MS) * (SNDP_WN_PLAY_SAMPLE_RATE) / 1000)
#define SNDP_WN_PLAY_FRAME_1CH_DLEN                 ((SNDP_WN_PLAY_FRAME_LEN) * (SNDP_WN_PLAY_SAMPLE_BYTE))
#define SNDP_WN_PLAY_FRAME_ALLCH_DLEN               ((SNDP_WN_PLAY_FRAME_1CH_DLEN) * (SNDP_WN_PLAY_CHANN_NUMBER))

#define SNDP_WN_PLAY_STREAM_BUFF_SIZE               ((SNDP_WN_PLAY_FRAME_ALLCH_DLEN) * 2)		//pingpang buff 长度大一样
#define	SNDP_WN_PLAY_VOLUME                         (TGT_VOLUME_LEVEL_8)

#define SNDP_WN_STREAM_ID                           (AUD_STREAM_ID_2)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool is_playback_running;
} sndp_wn_ctx_s;


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_wn_ctx_s wn_ctx;

static uint8_t wn_play_stream_buff[SNDP_WN_PLAY_STREAM_BUFF_SIZE];

static const short wn_audio_data[] = {
	#include "res/white_noise/white_noise_mono_44p1khz_16bit.txt"
};

static const uint32_t c_wn_audio_data_size = sizeof(wn_audio_data)/sizeof(wn_audio_data[0]);


/**************************************************************************************************
* Function
**************************************************************************************************/

POSSIBLY_UNUSED uint32_t sndp_white_noise_play_more_data(uint8_t *buf, uint32_t len)
{
	//WN_TRACE(1, "len=%d", len);

	if(!wn_ctx.is_playback_running) {
		memset(buf, 0, len);
		return len;
	}

	static uint32_t offset = 0;
	uint8_t *p_data = (uint8_t *)wn_audio_data;
	
	for(uint32_t i = 0; i < len; i++) {
		buf[i] = p_data[offset++];
		if(offset >= c_wn_audio_data_size)
			offset = 0;
	}

    return len;
}

uint32_t sndp_white_noise_play_start(void) 
{
	WN_TRACE(2, "isRun=%d", wn_ctx.is_playback_running);
	
	if(wn_ctx.is_playback_running)
		return 0;

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_WHITE_NOISE, APP_SYSFREQ_104M);

	struct AF_STREAM_CONFIG_T stream_cfg;
	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.sample_rate = SNDP_WN_PLAY_SAMPLE_RATE;
	stream_cfg.bits = SNDP_WN_PLAY_SAMPLE_BITS;
	stream_cfg.channel_num = SNDP_WN_PLAY_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC2;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
	stream_cfg.vol = SNDP_WN_PLAY_VOLUME;
	stream_cfg.handler = sndp_white_noise_play_more_data;
	stream_cfg.data_ptr = wn_play_stream_buff;
	stream_cfg.data_size = SNDP_WN_PLAY_STREAM_BUFF_SIZE;
	af_stream_open(SNDP_WN_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);
	af_stream_start(SNDP_WN_STREAM_ID, AUD_STREAM_PLAYBACK);

	wn_ctx.is_playback_running = true;
	WN_TRACE(1, "starting");
	return 0;
}

uint32_t sndp_white_noise_play_stop(void) 
{
	WN_TRACE(2, "isRun=%d", wn_ctx.is_playback_running);
	
	if(!wn_ctx.is_playback_running)
		return 0;
	wn_ctx.is_playback_running = false;

	af_stream_stop(SNDP_WN_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_close(SNDP_WN_STREAM_ID, AUD_STREAM_PLAYBACK);

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_WHITE_NOISE, APP_SYSFREQ_32K);

	WN_TRACE(1, "stopped");
	return 0;
}

void sndp_white_noise_switch(void)
{
    if(wn_ctx.is_playback_running) {
        sndp_white_noise_play_stop();
    } else {
        sndp_white_noise_play_start();
    }
}


#endif	/* __SNDP_APP_WHITE_NOISE__ */


