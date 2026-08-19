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
#include "math.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"
#include "sndp_ui.h"
#include "sndp_comm_cmd.h"
#include "sndp_app_white_noise.h"

/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_WN_TRACE__
#if defined(__SNDP_WN_TRACE__)
#define WN_TRACE(num, str, ...)                 MAIN_TRACE(1 + num, "[WN] %s, " str, __func__, ##__VA_ARGS__)
#else
#define WN_TRACE(num, str, ...)
#endif


#define SNDP_WN_PLAY_SAMPLE_RATE                    (AUD_SAMPRATE_16000)//(AUD_SAMPRATE_44100)
#define SNDP_WN_PLAY_SAMPLE_BITS                    (AUD_BITS_16)
#define SNDP_WN_PLAY_SAMPLE_BYTE                    (2)
#define SNDP_WN_PLAY_CHANN_NUMBER                   (AUD_CHANNEL_NUM_1)
#define SNDP_WN_PLAY_FRAME_MS                       (15)	//MS
#define SNDP_WN_PLAY_FRAME_LEN                      ((SNDP_WN_PLAY_FRAME_MS) * (SNDP_WN_PLAY_SAMPLE_RATE) / 1000)
#define SNDP_WN_PLAY_FRAME_1CH_DLEN                 ((SNDP_WN_PLAY_FRAME_LEN) * (SNDP_WN_PLAY_SAMPLE_BYTE))
#define SNDP_WN_PLAY_FRAME_ALLCH_DLEN               ((SNDP_WN_PLAY_FRAME_1CH_DLEN) * (SNDP_WN_PLAY_CHANN_NUMBER))

#define SNDP_WN_PLAY_STREAM_BUFF_SIZE               ((SNDP_WN_PLAY_FRAME_ALLCH_DLEN) * 2)		//pingpang buff 长度大一样
#define	SNDP_WN_PLAY_VOLUME                         (TGT_VOLUME_LEVEL_4)

#define SNDP_WN_STREAM_ID                           (AUD_STREAM_ID_4)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool onoff;

    bool is_playback_running;
} sndp_wn_ctx_s;


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_wn_ctx_s wn_ctx;

static uint8_t wn_play_stream_buff[SNDP_WN_PLAY_STREAM_BUFF_SIZE];

POSSIBLY_UNUSED  static const uint8_t wn_audio_data[] = {
	//#include "res/white_noise/white_noise_mono_44p1khz_16bit.txt"
    #include "res/white_noise/white_noise_mono_16khz_16bit.txt"
};

static const uint32_t c_wn_audio_data_size = sizeof(wn_audio_data)/sizeof(wn_audio_data[0]);


/**************************************************************************************************
* Function
**************************************************************************************************/

POSSIBLY_UNUSED uint32_t sndp_white_noise_play_more_data(uint8_t *buf, uint32_t len)
{
	if(!wn_ctx.is_playback_running) {
        //WN_TRACE(1, "%d, rtn", __LINE__);
		memset(buf, 0, len);
		return len;
	}

    
#if 1    
	static uint32_t offset = 0;
    int16_t *play_pcm16 = (int16_t *)buf;
    uint32_t play_pcm16_len = len / 2;
    int16_t *src_pcm16 = (int16_t *)wn_audio_data;
    uint32_t src_pcm16_size = c_wn_audio_data_size/2;

    //WN_TRACE(1, "play_len=%d, offset=%d, src_size=%d", play_pcm16_len, offset, src_pcm16_size);
	
	for(uint32_t i = 0; i < play_pcm16_len; i++) {
        if(offset >= src_pcm16_size) {
    			offset = 0;
        }
		play_pcm16[i] = src_pcm16[offset++];
	}
#endif

#if 0 //for test. Generate 1khz sinusoidal curve wave.
        float A = 5000;
        uint32_t curve_freq = 1000;
        uint32_t sample_freq = 16000;
        uint32_t cycle_samples = sample_freq/curve_freq;
        float degree_per = 360.0f / cycle_samples;
        float PI = 3.1415926f;
        float degree;
        static uint32_t idx = 0;
        int16_t *pcm16 = (int16_t *)buf;
    
        for(int i = 0; i < 240; i++) {
            degree = degree_per * idx;
            pcm16[i] = (int16_t)(A * sin(PI/180*degree));
            
            idx++;
            if(idx >= cycle_samples)
                idx = 0;
        }
        //DUMP16("%04x, ", radio_buf, 16);
#endif

    return len;
}

static uint32_t sndp_white_noise_play_start(void) 
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
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
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

static uint32_t sndp_white_noise_play_stop(void) 
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

bool sndp_white_noise_is_playing(void)
{
    //WN_TRACE(1, "is_playback_running=%d", wn_ctx.is_playback_running);
    return wn_ctx.is_playback_running;
}

bool sndp_white_noise_is_turnon(void)
{
    //WN_TRACE(1, "onoff=%d", wn_ctx.onoff);
    return wn_ctx.onoff;
}


bool sndp_white_noise_can_play(void)
{
    if(!sndp_white_noise_is_turnon()) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }
        
    if(!sndp_dev_wear_is_worn(false)) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }

    if(sndp_dev_iobox_is_in_box(false)) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }

    if(sndp_dev_cover_is_closed(false)) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }
    
    if(sndp_call_is_active()) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }

    if(sndp_music_is_playing()) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }

    if(sndp_ui_is_prompt_playing()) {
        WN_TRACE(1, "%d, rtn", __LINE__);
        return false;
    }
    
    return true;
}

void sndp_white_noise_onoff_sync_recv(bool onoff) 
{
    WN_TRACE(1, "onoff=%d", onoff);

    if(onoff != wn_ctx.onoff) {
        wn_ctx.onoff = onoff;

        if(onoff) {
        	if(sndp_white_noise_can_play()) {
                sndp_white_noise_play_start();
            }
        } else {
            sndp_white_noise_play_stop();
        }
    }
}

void sndp_white_noise_onoff(bool onoff, bool need_sync) 
{
    WN_TRACE(1, "onoff=%d", onoff);
    
    wn_ctx.onoff = onoff;

#if defined(__SNDP_COMM_MGR__)
    sndp_comm_cmd_send_lr_white_noise_onoff(onoff);
#endif

    if(onoff) {
    	if(sndp_white_noise_can_play()) {
            sndp_white_noise_play_start();
        }
    } else {
        sndp_white_noise_play_stop();
    }
}

void sndp_white_noise_resume(void) 
{
    WN_TRACE(1, "...");
    
    if(sndp_white_noise_can_play()) {
        sndp_white_noise_play_start();
    }
}

void sndp_white_noise_pause(void) 
{
	WN_TRACE(1, ".");
    sndp_white_noise_play_stop();
}

#endif	/* __SNDP_APP_WHITE_NOISE__ */


