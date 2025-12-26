#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "hal_trace.h"
#include "heap_api.h"
#include "voice_opus.h"
#include "opus_api.h"


static uint8_t is_voice_opus_initialized = 0;

static void* voice_opus_enc = NULL;
static void* voice_opus_dec = NULL;

static VOICE_OPUS_CONFIG_T  voice_opus_config = {
    .heapSize = VOICE_OPUS_HEAP_SIZE,
    .channelCnt = VOICE_OPUS_CHANNEL_COUNT,
    .complexity = VOICE_OPUS_COMPLEXITY,
    .packetLossPercentage = VOICE_OPUS_PACKET_LOSS_PERC,
    .sizePerSample = VOICE_SIZE_PER_SAMPLE,
    .appType = VOICE_OPUS_APP,
    .bandWidth = VOICE_OPUS_BANDWIDTH,
    .bitRate = VOICE_OPUS_BITRATE,
    .sampleRate = VOICE_OPUS_SAMPLE_RATE,
    .signalType = VOICE_SIGNAL_TYPE,
    .periodPerFrame = VOICE_FRAME_PERIOD,
    .isUseVbr = VOICE_OPUS_USE_VBR,
    .isConstraintUseVbr = VOICE_OPUS_CONSTRAINT_USE_VBR,
    .isUseInBandFec = VOICE_OPUS_USE_INBANDFEC,
    .isUseDtx = VOICE_OPUS_USE_DTX,
    .reserve = 0,
};


// #define LOG(fmt,...) TRACE(0, "(%s)[%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define LOG(fmt,...) AI_VOICE_TRACE(0, "(%s)[%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

static char heap_pool[80*1024];
static multi_heap_handle_t heap;

static void* opus_malloc(int size)
{
    void*buf = heap_malloc(heap,size);
    LOG("%d,%p", size,buf);
    return buf;
}

static void* opus_realloc(void* rmem, int newsize)
{
    void *buf = heap_realloc(heap,rmem,newsize);
    LOG("%d,%p,%p", newsize,rmem,buf);
    return buf;
}

static void opus_free(void* rmem)
{
    heap_free(heap,rmem);
    LOG("%p", rmem);
    return;
}

void opus_print(const char* fmt, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    LOG("%s", buf);
}

#if 1
static int voice_opus_init_encoder(uint16_t sampleRate);
static int voice_opus_deinit_encoder(void);
static int voice_opus_init_decoder(uint16_t sampleRate);
static int voice_opus_deinit_decoder(void);
#endif

static int voice_opus_init_encoder(uint16_t sampleRate)
{
    LOG("%p,(%d,%d,%d,%d)",voice_opus_enc,sampleRate,voice_opus_config.channelCnt,voice_opus_config.bitRate,voice_opus_config.periodPerFrame);
    if (voice_opus_enc){
        return -1;
    }
    void *enc = 0;
    const int rate = sampleRate;
    const int channels = voice_opus_config.channelCnt;
    const int bitrate=voice_opus_config.bitRate;
    const int frameDMs=voice_opus_config.periodPerFrame;
    const int complexity=0;
    bool haveHead=false;
    OpusApi_BasePort_t opusApiBasePort;
	opusApiBasePort.malloc_cb = opus_malloc;
	opusApiBasePort.realloc_cb = opus_realloc;
	opusApiBasePort.free_cb = opus_free;
	opusApiBasePort.print_cb = opus_print;
    OpusApiRet_t ret = opus_api_create_encoder(&enc, &opusApiBasePort, rate, channels, haveHead, OPUS_API_ENC_CHOOSE_NORMAL);
    if (ret != OPUS_API_RET_SUCCESS) {
        LOG("opus create fail, %d,(%p,%d,%d,%d)", ret, enc, rate, channels, haveHead);
        return -2;
    }
    ret = opus_api_encoder_set(enc, OPUS_API_ENC_SET_BIT_RATE, (void*)bitrate);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus set fail, %d", ret);
        return -2;
    }
    ret = opus_api_encoder_set(enc, OPUS_API_ENC_SET_FRAME_DURATION_0P1MS, (void*)frameDMs);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus set fail, %d", ret);
        return -2;
    }
    ret = opus_api_encoder_set(enc, OPUS_API_ENC_SET_USE_VBR, (void*)false);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus set fail, %d", ret);
        return -2;
    }
    ret = opus_api_encoder_set(enc, OPUS_API_ENC_SET_COMPLEXITY, (void*)complexity);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus set fail, %d", ret);
        return -2;
    }
    ret = opus_api_encoder_set(enc, OPUS_API_ENC_SET_APPLICATION, (void*)(int)voice_opus_config.appType);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus set fail, %d", ret);
        return -2;
    }
    voice_opus_enc = enc;
    LOG("%s init done", __func__);
    return 0;
}

int32_t voice_opus_encode(uint8_t *bitstream, uint8_t *speech, uint32_t sampleCount, uint8_t isReset)
{
    if (isReset)
    {
        voice_opus_deinit_encoder();
        int8_t ret = voice_opus_init_encoder(voice_opus_config.sampleRate);
        if (0 != ret){
            return ret;
        }
    }
    int32_t outputBytes = sampleCount*4;
    OpusApiRet_t ret = opus_api_encoder_run(voice_opus_enc, (int16_t*)bitstream, sampleCount, speech, &outputBytes);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus run fail, %d", ret);
    }
    return outputBytes;
}

static int voice_opus_init_decoder(uint16_t sampleRate)
{
    LOG("%p,(%d,%d,%d,%d)", voice_opus_dec,sampleRate,voice_opus_config.channelCnt,voice_opus_config.bitRate,voice_opus_config.periodPerFrame);
    if (voice_opus_dec){
        return -1;
    }
    void *dec = 0;
    const int rate = sampleRate;
    const int channels = voice_opus_config.channelCnt;
    bool haveHead=false;
    OpusApi_BasePort_t opusApiBasePort;
    opusApiBasePort.malloc_cb = opus_malloc;
    opusApiBasePort.realloc_cb = opus_realloc;
    opusApiBasePort.free_cb = opus_free;
    opusApiBasePort.print_cb = opus_print;
    OpusApiRet_t ret = opus_api_create_decoder(&dec, &opusApiBasePort, rate, channels, OPUS_API_DEC_CHOOSE_NORMAL);
    if (ret != OPUS_API_RET_SUCCESS) {
        LOG("create fail, %d,(%p,%d,%d,%d)", ret, dec, rate, channels, haveHead);
        return -2;
    }
    voice_opus_dec = dec;
    return 0;
}

uint32_t voice_opus_decode(uint8_t *speech, uint32_t speechLen, uint8_t *bitstream, uint32_t sampleCount, uint8_t isReset)
{
    if (isReset){
        voice_opus_deinit_decoder();
        int8_t ret = voice_opus_init_decoder(voice_opus_config.sampleRate);
        if (0 != ret){
            return ret;
        }
    }
    int32_t outputPcmCount = sampleCount;
    OpusApiRet_t ret = opus_api_decoder_run(voice_opus_dec, speech, speechLen, (short*)bitstream, &outputPcmCount ,0);
    if(ret != OPUS_API_RET_SUCCESS){
        LOG("opus run fail, %d", ret);
    }
    return outputPcmCount;
}


int voice_opus_init(VOICE_OPUS_CONFIG_T* pConfig, uint8_t* heapPtr, uint8_t* stackPtr)
{
    LOG("%d", is_voice_opus_initialized);
    if (!is_voice_opus_initialized) {
        voice_opus_config = *pConfig;
        heap = heap_register(heap_pool,sizeof(heap_pool));
        is_voice_opus_initialized = 1;
    }
    return 0;
}

int voice_opus_deinit(void)
{
    LOG("%d", is_voice_opus_initialized);
    if (is_voice_opus_initialized){
        voice_opus_deinit_encoder();
        voice_opus_deinit_decoder();
        is_voice_opus_initialized = 0;
    }
    return 0;
}

static int voice_opus_deinit_encoder(void)
{
    LOG("%p", voice_opus_enc);
    if(voice_opus_enc){
        opus_api_destory_encoder(voice_opus_enc);
        voice_opus_enc = 0;
    }
    return 0;
}

static int voice_opus_deinit_decoder(void)
{
    LOG("%p", voice_opus_dec);
    if(voice_opus_dec){
        opus_api_destory_decoder(voice_opus_dec);
        voice_opus_dec = 0;
    }
    return 0;
}

