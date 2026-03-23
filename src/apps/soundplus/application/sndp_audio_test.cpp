#if defined(__SNDP_AUDIO_TEST__)
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
#include "sndp_audio_test.h"

/**************************************************************************************************
* Constant
**************************************************************************************************/
#define __SNDP_AUDIO_TEST_TRACE__
#if defined(__SNDP_AUDIO_TEST_TRACE__)
#define AUDIO_TEST_TRACE(num, str, ...)                     SNDP_TRACE(1 + num, "[AUDIO_TEST] %s, " str, __func__, ##__VA_ARGS__)
#else
#define AUDIO_TEST_TRACE(num, str, ...)
#endif


#define SNDP_AUDIO_TEST_FRAME_MS                           (15)	//MS

#define SNDP_AUDIO_TEST_CAPTUER_SAMPLE_RATE                (AUD_SAMPRATE_16000)
#define SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BITS                (AUD_BITS_16)
#define SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BYTE                (2)

#define SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER               (AUD_CHANNEL_NUM_2)
#define SNDP_AUDIO_TEST_CAPTUER_FRAME_MS                   (SNDP_AUDIO_TEST_FRAME_MS)
#define SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN                  ((SNDP_AUDIO_TEST_CAPTUER_SAMPLE_RATE / 1000) * SNDP_AUDIO_TEST_CAPTUER_FRAME_MS)										
#define SNDP_AUDIO_TEST_CAPTUER_FRAME_1CH_DATA_LEN         ((SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN) * (SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BYTE))
#define SNDP_AUDIO_TEST_CAPTUER_FRAME_ALLCH_DATA_LEN       ((SNDP_AUDIO_TEST_CAPTUER_FRAME_1CH_DATA_LEN) * (SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER))


#define SNDP_AUDIO_TEST_PLAY_SAMPLE_RATE                   (AUD_SAMPRATE_16000)
#define SNDP_AUDIO_TEST_PLAY_SAMPLE_BITS                   (AUD_BITS_16)
#define SNDP_AUDIO_TEST_PLAY_SAMPLE_BYTE                   (1)
#define SNDP_AUDIO_TEST_PLAY_CHANN_NUMBER                  (AUD_CHANNEL_NUM_1)
#define SNDP_AUDIO_TEST_PLAY_FRAME_MS                      (SNDP_AUDIO_TEST_FRAME_MS)	//MS
#define SNDP_AUDIO_TEST_PLAY_FRAME_LEN                     ((SNDP_AUDIO_TEST_PLAY_SAMPLE_RATE / 1000) * SNDP_AUDIO_TEST_PLAY_FRAME_MS)
#define SNDP_AUDIO_TEST_PLAY_FRAME_1CH_DATA_LEN            ((SNDP_AUDIO_TEST_PLAY_FRAME_LEN) * (SNDP_AUDIO_TEST_PLAY_SAMPLE_BYTE))
#define SNDP_AUDIO_TEST_PLAY_FRAME_ALLCH_DATA_LEN          ((SNDP_AUDIO_TEST_PLAY_FRAME_1CH_DATA_LEN) * (SNDP_AUDIO_TEST_PLAY_CHANN_NUMBER))


#define SNDP_AUDIO_TEST_CAPTUER_BUFF_SIZE                  (SNDP_AUDIO_TEST_CAPTUER_FRAME_ALLCH_DATA_LEN*2)	//pingpang buff 长度大一样
#define SNDP_AUDIO48K24BIT_TEST_CAPTUER_BUFF_SIZE          (SNDP_AUDIO_TEST_CAPTUER_FRAME_ALLCH_DATA_LEN*2*3*2)	//pingpang buff 长度大一样
#define SNDP_AUDIO48K16BIT_TEST_CAPTUER_BUFF_SIZE          (SNDP_AUDIO_TEST_CAPTUER_FRAME_ALLCH_DATA_LEN*2*3)	//pingpang buff 长度大一样

#define SNDP_AUDIO_TEST_PLAY_BUFF_SIZE                     (SNDP_AUDIO_TEST_PLAY_FRAME_ALLCH_DATA_LEN*2)		//pingpang buff 长度大一样
#define SNDP_AUDIO48K24BIT_TEST_PLAY_BUFF_SIZE             (SNDP_AUDIO_TEST_PLAY_FRAME_ALLCH_DATA_LEN*2*3*2)		//pingpang buff 长度大一样
#define SNDP_AUDIO48K16BIT_TEST_PLAY_BUFF_SIZE             (SNDP_AUDIO_TEST_PLAY_FRAME_ALLCH_DATA_LEN*2*3)		//pingpang buff 长度大一样

#define	SNDP_AUDIO_TEST_CAPTUER_VOLUME                     (TGT_VOLUME_LEVEL_8)
// #define	SNDP_AUDIO_TEST_PLAY_VOLUME                    (TGT_VOLUME_LEVEL_1)
int SNDP_AUDIO_TEST_PLAY_VOLUME = TGT_VOLUME_LEVEL_16;

//#define SNDP_AUDIO_TEST_STREAM_ID                        (AUD_STREAM_ID_1)
#define SNDP_AUDIO_TEST_STREAM_ID                          (AUD_STREAM_ID_1)
#define SNDP_AUDIO48K24BIT_TEST_STREAM_ID                  (AUD_STREAM_ID_1)
#define SNDP_AUDIO48K16BIT_TEST_STREAM_ID                  (AUD_STREAM_ID_1)

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
//#define __SNDP_AUDIO_TEST_LOOPBACK__
#else
#define __SNDP_AUDIO_TEST_LOOPBACK__
#endif

/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool is_running;
} sndp_audio_test_ctx_s;


/**************************************************************************************************
* Variable
**************************************************************************************************/
static uint8_t sndp_audio_capture_buff[SNDP_AUDIO_TEST_CAPTUER_BUFF_SIZE];
static uint8_t sndp_audio48k24bit_capture_buff[SNDP_AUDIO_TEST_CAPTUER_BUFF_SIZE*3*2];
static uint8_t sndp_audio48k16bit_capture_buff[SNDP_AUDIO_TEST_CAPTUER_BUFF_SIZE*3];
static uint8_t sndp_audio_play_buff[SNDP_AUDIO_TEST_PLAY_BUFF_SIZE];
#ifdef PLAYBACK_USE_I2S
static uint8_t sndp_audio48k24bit_play_buff[SNDP_AUDIO_TEST_PLAY_BUFF_SIZE*3*2*2];
#else
static uint8_t sndp_audio48k24bit_play_buff[SNDP_AUDIO_TEST_PLAY_BUFF_SIZE*3*2];
#endif
static uint8_t sndp_audio48k16bit_play_buff[SNDP_AUDIO_TEST_PLAY_BUFF_SIZE*3];
static sndp_audio_test_ctx_s sndp_audio_ctx;

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)
#define SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE			(SNDP_AUDIO_TEST_CAPTUER_FRAME_1CH_DATA_LEN)
#define SNDP_AUDIO_LOOPBACK_BUFF_FRAME_CNT				(3)
#define SNDP_AUDIO_LOOPBACK_BUFF_SIZE					(SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE*SNDP_AUDIO_LOOPBACK_BUFF_FRAME_CNT*3*2)

static uint8_t sndp_audio_loopback_queue_buf[SNDP_AUDIO_LOOPBACK_BUFF_SIZE];
static uint8_t sndp_audio_loopback_fill_buf[SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE];
static uint8_t sndp_audio48k24bit_loopback_fill_buf[SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE*3*2];
static uint8_t sndp_audio48k16bit_loopback_fill_buf[SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE*3];

static CQueue sndp_audio_loopback_queue;	
static osMutexId sndp_audio_loopback_queue_mutex_id = NULL;
osMutexDef(sndp_audio_loopback_queue_mutex);
#endif

/**************************************************************************************************
* Function
**************************************************************************************************/
#define CAPTURE_USE_DIG_MIC
#ifdef CAPTURE_USE_DIG_MIC
static void sndp_bt_sco_digital_mic_volt_enable(bool enable)
{
    uint32_t gpiopin;
    struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_gpio;

    // gpiopin = HAL_GPIO_PIN_PMU_P0_0 + 24 / 10 * 8 + 24 % 10;  //P15
		gpiopin = HAL_IOMUX_PIN_P2_4;
		
    cfg_hw_gpio.pin = (enum HAL_IOMUX_PIN_T) gpiopin;
    cfg_hw_gpio.function = HAL_IOMUX_FUNC_AS_GPIO;
    cfg_hw_gpio.volt = HAL_IOMUX_PIN_VOLTAGE_VIO;
    cfg_hw_gpio.pull_sel = HAL_IOMUX_PIN_PULLUP_ENABLE;
    hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&cfg_hw_gpio, 1);

    hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)cfg_hw_gpio.pin, HAL_GPIO_DIR_OUT, enable);
}
#endif

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)
void sndp_audio_loopback_fill_data(uint8_t *buf, uint32_t len)
{
    osMutexWait(sndp_audio_loopback_queue_mutex_id, osWaitForever);
    EnCQueue(&sndp_audio_loopback_queue, (CQItemType *)buf, len);
	osMutexRelease(sndp_audio_loopback_queue_mutex_id);
}

void sndp_audio_loopback_fetch_data(uint8_t *buf, uint32_t len)
{
	osMutexWait(sndp_audio_loopback_queue_mutex_id, osWaitForever);
    DeCQueue(&sndp_audio_loopback_queue, (CQItemType *)buf, len);
	osMutexRelease(sndp_audio_loopback_queue_mutex_id);
}

void sndp_audio_loopback_init(void)
{
    if (sndp_audio_loopback_queue_mutex_id == NULL) {
        sndp_audio_loopback_queue_mutex_id = osMutexCreate((osMutex(sndp_audio_loopback_queue_mutex)));
        ASSERT(sndp_audio_loopback_queue_mutex_id != NULL, "%s, sndp_audio_loopback_queue_mutex_id == NULL", __func__);
    }

    osMutexWait(sndp_audio_loopback_queue_mutex_id, osWaitForever);
    memset(sndp_audio_loopback_queue_buf, 0x00, sizeof(sndp_audio_loopback_queue_buf));
    InitCQueue(&sndp_audio_loopback_queue, sizeof(sndp_audio_loopback_queue_buf), sndp_audio_loopback_queue_buf);
    osMutexRelease(sndp_audio_loopback_queue_mutex_id);

}
#endif

static void sndp_generate_1khz_wave(uint8_t *buf, uint8_t len)
{
#if 1 //for test. Generate 1khz sinusoidal curve wave.
	float A = 5000;
	uint32_t curve_freq = 1000;
	uint32_t sample_freq = 16000;
	uint32_t cycle_samples = sample_freq/curve_freq;
	float degree_per = 360.0f / cycle_samples;
	float PI = 3.1415926f;
	float degree;
	static uint32_t idx = 0;

	for(int i = 0; i < len; i++) {
		degree = degree_per * idx;
		buf[i] = (int16_t)(A * sin(PI/180*degree));
		
		idx++;
		if(idx >= cycle_samples)
			idx = 0;
	}
	//DUMP16("%04x, ", buf, 16);
#endif
}

extern int mic_digvolume;
uint32_t sndp_audio48k24bit_test_data_come(uint8_t *buf, uint32_t len)
{
	
#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
	audio_dump_clear_up();
	for(int32_t i = 0; i < SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER; i++) {
		audio_dump_add_channel_data_from_multi_channels(i, buf, SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER, i);
	}
    audio_dump_run();
#endif


#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
    int32_t *in_pcm32 = (int32_t *)buf;
    int32_t *fill_pcm32 = (int32_t *)&sndp_audio48k24bit_loopback_fill_buf[0];
    uint32_t ch_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
#ifdef PLAYBACK_USE_I2S
    uint32_t frame_len = len / 4 / ch_num;
    uint32_t  ch_idx = 0;
    // AUDIO_TEST_TRACE(1, "len=%d ch_idx=%d", len,ch_idx);
    for(uint32_t i = 0; i < frame_len; i++) {
        fill_pcm32[i] = in_pcm32[ch_idx]*mic_digvolume;
        ch_idx += ch_num;
    }
    
	sndp_audio_loopback_fill_data((uint8_t *)fill_pcm32, len);
#else
    uint32_t frame_len = len / 2 / ch_num;
    uint32_t  ch_idx = 0;
    // AUDIO_TEST_TRACE(1, "len=%d ch_idx=%d", len,ch_idx);
    for(uint32_t i = 0; i < frame_len; i++) {
        fill_pcm32[i] = in_pcm32[ch_idx]*mic_digvolume;
        ch_idx += ch_num;
    }
    
	sndp_audio_loopback_fill_data((uint8_t *)fill_pcm32, frame_len*2);
#endif
#endif


    return len;
}

uint32_t sndp_audio48k16bit_test_data_come(uint8_t *buf, uint32_t len)
{
	
#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
	audio_dump_clear_up();
	for(int32_t i = 0; i < SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER; i++) {
		audio_dump_add_channel_data_from_multi_channels(i, buf, SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER, i);
	}
    audio_dump_run();
#endif


#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
    int16_t *in_pcm32 = (int16_t *)buf;
    int16_t *fill_pcm32 = (int16_t *)&sndp_audio48k16bit_loopback_fill_buf[0];
    uint32_t ch_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
    uint32_t frame_len = len / 2 / ch_num;
    uint32_t  ch_idx = 0;
    // AUDIO_TEST_TRACE(1, "len=%d ch_idx=%d", len,ch_idx);
    for(uint32_t i = 0; i < frame_len; i++) {
        fill_pcm32[i] = in_pcm32[ch_idx];
        ch_idx += ch_num;
    }
    
	sndp_audio_loopback_fill_data((uint8_t *)fill_pcm32, frame_len*2);
#endif


    return len;
}

uint32_t sndp_audio_test_data_come(uint8_t *buf, uint32_t len)
{
	
#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
#if 0
    int16_t *pcm16 = (int16_t *)buf;
    static int16_t val = 0;
    static uint8_t add = 1;
    
    for(uint16_t i = 0;  i< len/2; i++) {
        pcm16[i] = val;

        if(add) {
            val++;
            if(val >= 20000) {
                add = 0;
            }
        } else {
            val--;
            if(val < -20000) {
                add = 1;
            }
        }
    }
#endif

	audio_dump_clear_up();
	for(int32_t i = 0; i < SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER; i++) {
		audio_dump_add_channel_data_from_multi_channels(i, buf, SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER, i);
	}
    audio_dump_run();
#endif


#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
    int16_t *in_pcm16 = (int16_t *)buf;
    int16_t *fill_pcm16 = (int16_t *)&sndp_audio_loopback_fill_buf[0];
    uint32_t ch_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
    uint32_t frame_len = len / 2 / ch_num;
    uint32_t  ch_idx = 0;
    // AUDIO_TEST_TRACE(1, "len=%d ch_idx=%d", len,ch_idx);
    for(uint32_t i = 0; i < frame_len; i++) {
        fill_pcm16[i] = in_pcm16[ch_idx];
        ch_idx += ch_num;
    }
    
	sndp_audio_loopback_fill_data((uint8_t *)fill_pcm16, frame_len*2);
#endif


    return len;
}

uint32_t sndp_audio_test_more_data(uint8_t *buf, uint32_t len)
{
	//AUDIO_TEST_TRACE(1, "len=%d", len);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
    memset(buf, 0, len);
    return len;
#endif

	if(sndp_audio_ctx.is_running == false) {
		memset(buf, 0, len);
		return len;
	}

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
#if 0
	AUDIO_TEST_TRACE(4, "capture_data: %02x %02x %02x %02x", 
		sndp_audio_loopback_capture_data[0],
		sndp_audio_loopback_capture_data[1],
		sndp_audio_loopback_capture_data[2],
		sndp_audio_loopback_capture_data[3]);
#endif

	sndp_audio_loopback_fetch_data(buf, len);

#else
    sndp_generate_1khz_wave(buf, len);
#endif

    return len;
}


uint32_t sndp_audio48k24bit_test_more_data(uint8_t *buf, uint32_t len)
{
	//AUDIO_TEST_TRACE(1, "len=%d", len);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
    memset(buf, 0, len);
    return len;
#endif

	// if(sndp_audio_ctx.is_running == false) {
	// 	memset(buf, 0, len);
	// 	return len;
	// }

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
#if 0
	AUDIO_TEST_TRACE(4, "capture_data: %02x %02x %02x %02x", 
		sndp_audio_loopback_capture_data[0],
		sndp_audio_loopback_capture_data[1],
		sndp_audio_loopback_capture_data[2],
		sndp_audio_loopback_capture_data[3]);
#endif
#ifdef PLAYBACK_USE_I2S
	uint8_t fetchout_micbuf[SNDP_AUDIO_LOOPBACK_BUFF_FRAME_SIZE*3*2];
	uint32_t* pcm32_bufptr = (uint32_t*)buf;
	uint32_t* fetchout32_ptr = (uint32_t*)fetchout_micbuf;

	// sndp_audio_loopback_fetch_data(buf, len);
	sndp_audio_loopback_fetch_data(fetchout_micbuf, sizeof(sndp_audio48k24bit_loopback_fill_buf));
	for(uint32_t i=0; i<len/4/2; i++)
	{
		pcm32_bufptr[2*i] = fetchout32_ptr[i];
		pcm32_bufptr[2*i + 1] = fetchout32_ptr[i];
	}
#else
	sndp_audio_loopback_fetch_data(buf, len);
#endif
#else
	sndp_generate_1khz_wave(buf, len);
#endif

    return len;
}

uint32_t sndp_audio48k16bit_test_more_data(uint8_t *buf, uint32_t len)
{
	//AUDIO_TEST_TRACE(1, "len=%d", len);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)

    memset(buf, 0, len);
    return len;
#endif

	// if(sndp_audio_ctx.is_running == false) {
	// 	memset(buf, 0, len);
	// 	return len;
	// }

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)	
#if 0
	AUDIO_TEST_TRACE(4, "capture_data: %02x %02x %02x %02x", 
		sndp_audio_loopback_capture_data[0],
		sndp_audio_loopback_capture_data[1],
		sndp_audio_loopback_capture_data[2],
		sndp_audio_loopback_capture_data[3]);
#endif

	sndp_audio_loopback_fetch_data(buf, len);

#else
	sndp_generate_1khz_wave(buf, len);
#endif

    return len;
}

uint32_t sndp_audio_test_start48k24bit(void) 
{
	AUDIO_TEST_TRACE(2, "isRun=%d", sndp_audio_ctx.is_running);
	
	if(sndp_audio_ctx.is_running)
		return 0;

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_AUDIO_TEST, APP_SYSFREQ_104M);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
    audio_dump_init(SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BYTE, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER);
#endif

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)
    sndp_audio_loopback_init();
#endif

#ifdef CAPTURE_USE_DIG_MIC
    sndp_bt_sco_digital_mic_volt_enable(true);
#endif

#ifdef PLAYBACK_USE_I2S
	hal_cmu_audio_resample_disable();
#endif

	struct AF_STREAM_CONFIG_T stream_cfg;
	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.sample_rate = AUD_SAMPRATE_48000;
	stream_cfg.bits = AUD_BITS_24;
	stream_cfg.channel_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_INPUT_PATH_SNDP_AUDIO_TEST;
	stream_cfg.vol = SNDP_AUDIO_TEST_CAPTUER_VOLUME;
	stream_cfg.handler = sndp_audio48k24bit_test_data_come;
	stream_cfg.data_ptr = sndp_audio48k24bit_capture_buff;
	stream_cfg.data_size = SNDP_AUDIO48K24BIT_TEST_CAPTUER_BUFF_SIZE;	
	af_stream_open(SNDP_AUDIO48K24BIT_TEST_STREAM_ID, AUD_STREAM_CAPTURE, &stream_cfg);
	stream_cfg.sample_rate = AUD_SAMPRATE_48000;
	stream_cfg.bits = AUD_BITS_24;
#ifdef PLAYBACK_USE_I2S
	stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
	stream_cfg.device = AUD_STREAM_USE_I2S0_SLAVE;
	stream_cfg.io_path = AUD_IO_PATH_NULL;
#else
	stream_cfg.channel_num = SNDP_AUDIO_TEST_PLAY_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
#endif
	stream_cfg.vol = SNDP_AUDIO_TEST_PLAY_VOLUME;
	stream_cfg.handler = sndp_audio48k24bit_test_more_data;
	stream_cfg.data_ptr = sndp_audio48k24bit_play_buff;
#ifdef PLAYBACK_USE_I2S
	stream_cfg.data_size = SNDP_AUDIO48K24BIT_TEST_PLAY_BUFF_SIZE*2;
#else
	stream_cfg.data_size = SNDP_AUDIO48K24BIT_TEST_PLAY_BUFF_SIZE;
#endif
	af_stream_open(SNDP_AUDIO48K24BIT_TEST_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);

	af_stream_start(SNDP_AUDIO48K24BIT_TEST_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_start(SNDP_AUDIO48K24BIT_TEST_STREAM_ID, AUD_STREAM_CAPTURE);

	sndp_audio_ctx.is_running = true;
	AUDIO_TEST_TRACE(1, "starting");
	return 0;
}

uint32_t sndp_audio_test_start48k16bit(void) 
{
	AUDIO_TEST_TRACE(2, "isRun=%d", sndp_audio_ctx.is_running);
	
	if(sndp_audio_ctx.is_running)
		return 0;

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_AUDIO_TEST, APP_SYSFREQ_104M);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
    audio_dump_init(SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BYTE, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER);
#endif

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)
    sndp_audio_loopback_init();
#endif


	struct AF_STREAM_CONFIG_T stream_cfg;
	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.sample_rate = AUD_SAMPRATE_48000;
	stream_cfg.bits = SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BITS;
	stream_cfg.channel_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_INPUT_PATH_SNDP_AUDIO_TEST;
	stream_cfg.vol = SNDP_AUDIO_TEST_CAPTUER_VOLUME;
	stream_cfg.handler = sndp_audio48k16bit_test_data_come;
	stream_cfg.data_ptr = sndp_audio48k16bit_capture_buff;
	stream_cfg.data_size = SNDP_AUDIO48K16BIT_TEST_CAPTUER_BUFF_SIZE;	
	af_stream_open(SNDP_AUDIO48K16BIT_TEST_STREAM_ID, AUD_STREAM_CAPTURE, &stream_cfg);

	stream_cfg.sample_rate = AUD_SAMPRATE_48000;
	stream_cfg.bits = SNDP_AUDIO_TEST_PLAY_SAMPLE_BITS;
	stream_cfg.channel_num = SNDP_AUDIO_TEST_PLAY_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
	stream_cfg.vol = SNDP_AUDIO_TEST_PLAY_VOLUME;
	stream_cfg.handler = sndp_audio48k16bit_test_more_data;
	stream_cfg.data_ptr = sndp_audio48k16bit_play_buff;
	stream_cfg.data_size = SNDP_AUDIO48K16BIT_TEST_PLAY_BUFF_SIZE;
	af_stream_open(SNDP_AUDIO48K16BIT_TEST_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);

	af_stream_start(SNDP_AUDIO48K16BIT_TEST_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_start(SNDP_AUDIO48K16BIT_TEST_STREAM_ID, AUD_STREAM_CAPTURE);

	sndp_audio_ctx.is_running = true;
	AUDIO_TEST_TRACE(1, "starting");
	return 0;
}

uint32_t sndp_audio_test_start16k16bit(void) 
{
	AUDIO_TEST_TRACE(2, "isRun=%d", sndp_audio_ctx.is_running);
	
	if(sndp_audio_ctx.is_running)
		return 0;

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_AUDIO_TEST, APP_SYSFREQ_104M);

#if defined(__SNDP_AUDIO_TEST_MIC_DUMP__)
    audio_dump_init(SNDP_AUDIO_TEST_CAPTUER_FRAME_LEN, SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BYTE, SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER);
#endif

#if defined(__SNDP_AUDIO_TEST_LOOPBACK__)
    sndp_audio_loopback_init();
#endif

#ifdef CAPTURE_USE_DIG_MIC
    sndp_bt_sco_digital_mic_volt_enable(true);
#endif
	struct AF_STREAM_CONFIG_T stream_cfg;
	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.sample_rate = SNDP_AUDIO_TEST_CAPTUER_SAMPLE_RATE;
	stream_cfg.bits = SNDP_AUDIO_TEST_CAPTUER_SAMPLE_BITS;
	stream_cfg.channel_num = SNDP_AUDIO_TEST_CAPTUER_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_INPUT_PATH_SNDP_AUDIO_TEST;
	stream_cfg.vol = SNDP_AUDIO_TEST_CAPTUER_VOLUME;
	stream_cfg.handler = sndp_audio_test_data_come;
	stream_cfg.data_ptr = sndp_audio_capture_buff;
	stream_cfg.data_size = SNDP_AUDIO_TEST_CAPTUER_BUFF_SIZE;	
	af_stream_open(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_CAPTURE, &stream_cfg);

	stream_cfg.sample_rate = SNDP_AUDIO_TEST_PLAY_SAMPLE_RATE;
	stream_cfg.bits = SNDP_AUDIO_TEST_PLAY_SAMPLE_BITS;
	stream_cfg.channel_num = SNDP_AUDIO_TEST_PLAY_CHANN_NUMBER;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
	stream_cfg.vol = SNDP_AUDIO_TEST_PLAY_VOLUME;
	stream_cfg.handler = sndp_audio_test_more_data;
	stream_cfg.data_ptr = sndp_audio_play_buff;
	stream_cfg.data_size = SNDP_AUDIO_TEST_PLAY_BUFF_SIZE;
	af_stream_open(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_PLAYBACK, &stream_cfg);

	af_stream_start(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_start(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_CAPTURE);

	sndp_audio_ctx.is_running = true;
	AUDIO_TEST_TRACE(1, "starting");
	return 0;
}


uint32_t sndp_audio_test_stop(void) 
{
	AUDIO_TEST_TRACE(2, "isRun=%d", sndp_audio_ctx.is_running);
	
	if(!sndp_audio_ctx.is_running)
		return 0;
	sndp_audio_ctx.is_running = false;

#ifdef CAPTURE_USE_DIG_MIC
    sndp_bt_sco_digital_mic_volt_enable(false);
#endif

	af_stream_stop(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_CAPTURE);
	af_stream_stop(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_PLAYBACK);
	af_stream_close(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_CAPTURE);
	af_stream_close(SNDP_AUDIO_TEST_STREAM_ID, AUD_STREAM_PLAYBACK);

	app_sysfreq_req(APP_SYSFREQ_USER_SNDP_AUDIO_TEST, APP_SYSFREQ_32K);

#ifdef PLAYBACK_USE_I2S
	hal_cmu_audio_resample_enable();
#endif
	AUDIO_TEST_TRACE(1, "stopped");
	return 0;
}


void sndp_audio_test_switch(void)
{
    AUDIO_TEST_TRACE(1, "...");
    if(sndp_audio_ctx.is_running) {
        sndp_audio_test_stop();
    } else {
        sndp_audio_test_start16k16bit();
    }
}


#endif	/* __SNDP_ALG_MGR__ */


