#include <stdlib.h>
#include <string.h>
#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_aud.h"
#include "ai_voice_dbg.h"
#include "audio_dump.h"
#include "audioflinger.h"
#include "app_overlay.h"
#include "app_utils.h"
#include "apps.h"
#include "app_thread.h"
#include "hal_location.h"
#include "app_audio.h"
#include "bluetooth_bt_api.h"
#include "app_bt_media_manager.h"
#include "app_ai_if_thirdparty.h"
#include "app_ai_voice.h"
#include "voice_enhancement.h"
#include "voice_compression.h"
#include "ai_control.h"
#include "ai_thread.h"
#include "ai_transport.h"
#include "app_ai_ble.h"
#include "ai_manager.h"
#include "app_ai_tws.h"
#include "app_ai_if.h"

#ifdef IBRT
#include "bts_tws_api.h"
#endif

#ifdef BISTO_ENABLED
#include "gsound_custom_audio.h"
#endif

#if defined(ANC_ASSIST_ENABLED)
#include "app_voice_assist_ai_voice.h"
#else
#include "speech_ssat.h"
#endif

#if defined(STEREO_RECORD_PROCESS)
#include "stereo_record_process.h"
#endif

#ifdef __BIXBY_VOICE__
#include "app_bixby_thirdparty_if.h"
#endif

#ifdef VOICE_DETECTOR_SENS_EN
#include "mcu_sensor_hub_app_ai.h"
#endif

#ifdef ALEXA_M55_ENABLED
#include "mcu_dsp_m55_app_ai.h"
#endif

//#define  TEST_VOICE_DUMP_AUDIO
/// used for trace log control for AI voice module
#define MODULE_TRACE_LIMIT                      TR_LEVEL_DEBUG
/**< configure the voice settings   */
/// ping-pang count, MUST be a constant value
#define CAPTURE_PINGPANG_CNT                    (2)
/// capture int interval, here use 15ms at Jay's request
#ifndef DUAL_MIC_RECORDING
#define CAPTURE_INTERVAL_MS                     (15)
#else
#define CAPTURE_INTERVAL_MS                     (20)
#endif
/// convert 8bit PCM data to 16bit sample count
#define BIT16_PCM_SIZE_TO_SAMPLE_CNT(size)      ((size) * 8 / AUD_BITS_16)
/// don't know the useage
#define VOB_IGNORED_FRAME_CNT                   (0)
/// AI voice mail max number
#define AIV_MAIL_MAX                            (25)
/// invalid value for read offset
#define INVALID_READ_OFFSET                     0xFFFFFFFF
/// Reserved space in PCM cache buffer for concrete AI
#define AI_RESERVED_SPACE_IN_CACHE_BUF          (512)

#undef _ENUM_DEF
#define _ENUM_DEF(a, b) #b

#ifdef __AMA_VOICE__
#define MSG_DATA_CNT                            (3)
#else
#define MSG_DATA_CNT                            (0)
#endif

/**************************************************************************************************
 *                          self-defined strcutres                                                *
 * ***********************************************************************************************/
typedef struct
{
    /// AI voice work mode, @see AIV_WORK_MODE_E to get more info
    uint8_t                 mode;
    /// flag budle
    AIV_FLAG_BUDLE_T        flag;
    /// handler bundle
    AIV_HANDLER_BUNDLE_T    handlers;
} SPECIFIC_AI_T;

typedef struct
{
    /// AI mic state
    uint8_t micState;
    /// a2dp blocked by AI stream flag
    bool a2dpBlocked;
    /// AI voice stream monopolized flag
    bool monopolizeFlag;
    /// AI voice encode running flag
    uint8_t encodeState;
    /// current running codec
    uint8_t runningCodec;
    /// read offset when hotword detected
    uint32_t readOffset;
    /// size of PCM data cache queue
    uint32_t pcmQueueSize;
    /// stream monopolizer
    uint8_t monopolizer;
    /// stream state
    uint8_t streamState;
    /// specific AI info
    SPECIFIC_AI_T ai[AIV_USER_CNT];
    /// Stream trigger init handler
    TRIGGER_INIT_T triggerInit;
    /// capture stream configurations
    struct AF_STREAM_CONFIG_T streamCfg;
} AIV_CTL_T;

/**************************************************************************************************
 *                          private function declearation                                         *
 * ***********************************************************************************************/
#ifdef VOC_ENCODE_ENABLE
/**
 * @brief Encode PCM data
 *  NOTE: input data must be 16bit width
 *
 * @param ptrBuf        pointer of PCM data
 * @param length        length of PCM data
 * @param purchasedBytes Encode consumed PCM data length
 * @return uint32_t     encoded data length
 */
static uint32_t _encode_pcm_data(uint8_t *ptrBuf, uint32_t length, uint32_t *purchasedBytes);
#endif

/**
 * @brief Start capture stream
 *
 * @param user          AI to request start capture stream
 * @return int          0 - start successfully
 *                      -1 - fail to start
 */
static int _start_stream(uint8_t user);

/**
 * @brief Callback function when capture stream is started for concrete AI
 *
 * @param user          AI to request start capture stream
 * @return int          0 - start successfully
 *                      other - user not start successfully
 */
static int __on_concrete_ai_started(uint8_t user);

/**
 * @brief Callback function when capture stream is started
 *
 * @return int          0 - start successfully
 *                      other - user not start successfully
 */
static int _on_stream_started(void);

/**
 * @brief Stop capture stream
 *
* @param user          AI to request stop capture stream
 * @return int          0 - stop successfully
 *                      -1 - fail to stop
 */
static int _stop_stream(uint8_t user);

/**
 * @brief Callback function when capture stream is stopped for conceret AI
 *
 * @param user          AI user to stop capture stream
 */
static void _on_concrete_ai_stream_stopped(uint8_t user);

/**
 * @brief Callback function when capture stream is stopped
 *
 */
static void _on_stream_stopped(void);

/**
 * @brief Start upstream
 *
 * @param user          AI user to start upstream
 * @param offset        Read offset in PCM data cache buffer queue
 * @return int          0 - upstream start successfully
 *                      -1 - upstream start failed
 */
static int _start_upstream(uint8_t user, uint32_t offset);
/**
 * @brief Callback function when upstream is started
 *
 * @param user          AI voice user to request upstream
 * @param offset        Read offset in PCM data cache buffer queue
 */
static void _on_upstream_started(uint8_t user, uint32_t offset);

/**
 * @brief Stop upstream
 *
 * @param user          AI user to stop upstream
 * @param offset        Read offset in PCM data cache buffer queue
 * @return int          0 - upstream stop successfully
 *                      -1 - upstream stop failed
 */
static int _stop_upstream(uint8_t user, uint32_t offset);

/**
 * @brief Callback function when upstream is stopped
 *
 * @param user          AI user who was upstreaming
 */
static void _on_upstream_stopped(uint8_t user);

/**
 * @brief Update mic state
 *
 * @param state         mic state, @see AIV_MIC_STATE_E
 *
 */
static void _update_mic_state(uint8_t state);

/**
 * @brief Get is-super-user flag( @see AIV_FLAG_BUDLE_T)
 *
 * @param user          AI voice user, @see AIV_USER_E
 * @return true         AI user is super user
 * @return false        AI user is not super user
 */
static bool _get_super_user_flag(uint8_t user);

/**
 * @brief Check if super user exist
 *
 * @return true         Super user exist
 * @return false        Super user not exist
 */
static bool _is_super_user_exist(void);

/**
 * @brief Check if any AI is active
 *
 * @return true         at least one AI is active
 * @return false        no AI is active
 */
static bool _is_any_active(void);

/**************************************************************************************************
 *                          external function declearation                                        *
 * ***********************************************************************************************/
extern "C" void app_audio_set_a2dp_freq(uint32_t freq);
/**************************************************************************************************
 *                          static variables                                                      *
 * ***********************************************************************************************/
static osThreadId _ai_voice_tid = NULL;
static void _ai_voice_thread(void const *argument);

#ifndef AI_VOICE_THREAD_SIZE
#ifdef VOC_ENCODE_OPUS
#define AI_VOICE_THREAD_SIZE    (1024 * 14)
#else
#define AI_VOICE_THREAD_SIZE    (1024 * 4)
#endif
#endif

osThreadDef(_ai_voice_thread, osPriorityAboveNormal, 1, AI_VOICE_THREAD_SIZE, "ai_voice");

osMailQDef(AIV_mailbox, AIV_MAIL_MAX, AIV_MESSAGE_BLOCK);
static osMailQId AIV_mailbox = NULL;
static uint8_t AIV_mailCnt = 0;

/// PCM data cache buffer/queue related
uint8_t *pcm_cache_buf  = NULL;
uint8_t *bt_audio_buf   = NULL;
CQueue pcm_queue;
osMutexDef(pcm_queue_mutex);
osMutexId pcm_queue_mutex_id = NULL;

osMutexDef(ai_voice_mutex);
osMutexId ai_voice_mutex_id = NULL;

osMutexDef(pcm_encode_queue_mutex);
osMutexId pcm_encode_queue_mutex_id = NULL;

static AIV_CTL_T _ctl = {
    .micState       = AIV_MIC_STATE_CLOSED,
    .a2dpBlocked    = false,
    .monopolizeFlag = false,
    .encodeState    = AIV_ENCODE_STATE_IDLE,
    .runningCodec   = 0,
    .readOffset     = INVALID_READ_OFFSET,
    .pcmQueueSize   = PCM_DATA_CACHE_BUF_SIZE,
    .monopolizer    = AIV_USER_INVALID,
    .streamState    = AIV_OP_STOP_CAPTURE,
    .ai             = {{0,},},
    .triggerInit    = NULL,
    .streamCfg      = {
        .sample_rate    = AUD_SAMPRATE_16000, //!< default sample rate 16K
        .channel_num    = AUD_CHANNEL_NUM_1, //!< default use 1 channel
        .bits           = AUD_BITS_16, //!< default use 16 bit
        .device         = AUD_STREAM_USE_INT_CODEC,
        .io_path        = AUD_INPUT_PATH_ASRMIC,
        .vol            = 12, //!< default gain for capture
        .handler        = _mic_data_come,
        .data_ptr       = NULL,
        .data_size      = 0,
    },
};

static const char *_mode_str[AIV_WORK_MODE_CNT] = {
    AIV_WORK_MODE_LIST,
};

static const char *_user_str[AIV_USER_CNT] = {
    AIV_USER_LIST,
};

static const char *_mic_str[AIV_MIC_STATE_CNT] = {
    AIV_MIC_STATE_LIST,
};

static const char *_op_str[AIV_OP_CNT] = {
    AIV_OP_LIST,
};

POSSIBLY_UNUSED static const char *_encode_str[AIV_ENCODE_STATE_CNT] = {
    AIV_ENCODE_STATE_LIST,
};

static const uint8_t _user2spec[AIV_USER_CNT] = {
    [AIV_USER_INVALID] = AI_SPEC_INIT,
    [AIV_USER_BES] = AI_SPEC_BES,
    [AIV_USER_GVA] = AI_SPEC_GSOUND,
    [AIV_USER_AMA] = AI_SPEC_AMA,
    [AIV_USER_GMA] = AI_SPEC_ALI,
    [AIV_USER_DMA] = AI_SPEC_BAIDU,
    [AIV_USER_PENGUIN] = AI_SPEC_TENCENT,
    [AIV_USER_REC] = AI_SPEC_RECORDING,
    [AIV_USER_SSB] = AI_SPEC_BIXBY,
    [AIV_USER_COMM] = AI_SPEC_COMMON,   
};

static const uint8_t _spec2user[AI_SPEC_COUNT] = {
    [AI_SPEC_INIT] = AIV_USER_INVALID,
    [AI_SPEC_GSOUND] = AIV_USER_GVA,
    [AI_SPEC_AMA] = AIV_USER_AMA,
    [AI_SPEC_BES] =  AIV_USER_BES,
    [AI_SPEC_BAIDU] = AIV_USER_DMA,
    [AI_SPEC_TENCENT] = AIV_USER_PENGUIN,
    [AI_SPEC_ALI] = AIV_USER_GMA,
    [AI_SPEC_COMMON] = AIV_USER_COMM,
    [AI_SPEC_BIXBY] = AIV_USER_SSB,
    [AI_SPEC_RECORDING] = AIV_USER_REC,
};

static const uint8_t _byteWidthMap[5] = {
    0,
    1,
    2,
    4,
    4,
};

#ifdef ALEXA_M55_ENABLED
static bool informAppFlag = false;
#endif
extern "C" uint8_t is_sco_mode(void);

/// data length for one-time shot process
static uint32_t _handleFrameSize = AI_VOICE_ONESHOT_PROCESS_MAX_LEN;

static bool ai_mic_data_discard_flag = false;
static uint32_t ai_mic_data_discard_cnt = 0;
#define AI_DISCARD_MIC_DATA_TIME_AFTER_OPEN_MIC         (500)
static RAW_DATA_PROCESS_CB app_ai_voice_raw_data_process_handler = NULL;
/**************************************************************************************************
 *                          function definition                                                   *
 * ***********************************************************************************************/
static void _update_read_offset(uint32_t offset)
{
    osMutexWait(pcm_encode_queue_mutex_id, osWaitForever);
    uint32_t buf_len = app_ai_voice_get_ai_voice_buf_len();
    AI_VOICE_TRACE(0,"AIV update read offset:%d->%d", _ctl.readOffset, offset % buf_len);
    _ctl.readOffset = offset % buf_len;
    osMutexRelease(pcm_encode_queue_mutex_id);
}

static uint32_t _get_read_offset(void)
{
    return _ctl.readOffset;
}

static uint8_t _get_mic_state(void)
{
    return _ctl.micState;
}

static void _update_mic_state(uint8_t state)
{
    if (state == _get_mic_state())
    {
        AI_VOICE_TRACE(0,"mic state already [%s]", _mic_str[state]);
    }
    else
    {
        AI_VOICE_TRACE(0,"mic state update [%s]->[%s]", _mic_str[_get_mic_state()], _mic_str[state]);
        _ctl.micState = state;
    }
}

static void _update_stream_monopolizer(uint8_t monopolizer)
{
    AI_VOICE_TRACE(0,"Upstream monopolizer update:%s->%s", _user_str[_ctl.monopolizer], _user_str[monopolizer]);
    _ctl.monopolizer = monopolizer;
}

uint8_t _get_stream_monopolizer(void)
{
    return _ctl.monopolizer;
}

#ifdef VOC_ENCODE_ENABLE
static void _update_encode_state(uint8_t state)
{
    AI_VOICE_TRACE(0,"Encode state update:%s->%s", _encode_str[_ctl.encodeState], _encode_str[state]);
    _ctl.encodeState = state;
}

static void _update_running_codec(uint8_t codec)
{
    AI_VOICE_TRACE(0,"Running codec update:%d->%d", _ctl.runningCodec, codec);
    _ctl.runningCodec = codec;
}
#endif

static int _mailbox_init(void)
{
    AIV_mailbox = osMailCreate(osMailQ(AIV_mailbox), NULL);
    if (AIV_mailbox == NULL)
    {
        AI_VOICE_TRACE(0,"Failed to Create AIV_mailbox");
        return -1;
    }
    AIV_mailCnt = 0;
    return 0;
}

static int _mailbox_put(AIV_MESSAGE_BLOCK *msg_src)
{
    osStatus status = osOK;

    AIV_MESSAGE_BLOCK *msg = NULL;
    msg = (AIV_MESSAGE_BLOCK *)osMailAlloc(AIV_mailbox, 0);

    if (!msg)
    {
        osEvent evt;
        AI_VOICE_TRACE(0,"osMailAlloc error dump");
        for (uint8_t i = 0; i < AIV_MAIL_MAX; i++)
        {
            evt = osMailGet(AIV_mailbox, 0);
            if (evt.status == osEventMail)
            {
                AI_VOICE_TRACE(0,"cnt:%d op:%s param:%08x/%08x",
                      i,
                      _op_str[((AIV_MESSAGE_BLOCK *)(evt.value.p))->op],
                      ((AIV_MESSAGE_BLOCK *)(evt.value.p))->param0,
                      ((AIV_MESSAGE_BLOCK *)(evt.value.p))->param1);
            }
            else
            {
                AI_VOICE_TRACE(0,"cnt:%d %d", i, evt.status);
                break;
            }
        }
        AI_VOICE_TRACE(0,"osMailAlloc error dump end");
        ASSERT(0, "osMailAlloc error");
    }
    else
    {
        msg->op = msg_src->op;
        msg->param0 = msg_src->param0;
        msg->param1 = msg_src->param1;

        status = osMailPut(AIV_mailbox, msg);
        if (osOK == status)
        {
            AIV_mailCnt++;
        }
    }

    return (int)status;
}

static int _mailbox_get(AIV_MESSAGE_BLOCK **msg)
{
    int ret = 0;
    osEvent evt;
    evt = osMailGet(AIV_mailbox, osWaitForever);

    if (evt.status == osEventMail)
    {
        *msg = (AIV_MESSAGE_BLOCK *)evt.value.p;
    }
    else
    {
        ret = -1;
    }

    return ret;
}

static int _mailbox_free(AIV_MESSAGE_BLOCK *msg)
{
    osStatus status;

    status = osMailFree(AIV_mailbox, msg);
    if (osOK == status)
    {
        AIV_mailCnt--;
    }

    return (int)status;
}

static void _update_work_mode(uint8_t user, uint8_t mode)
{
    AI_VOICE_TRACE(0,"AIV state update for [%s]:%s->%s",
          _user_str[user], _mode_str[_ctl.ai[user].mode], _mode_str[mode]);
    _ctl.ai[user].mode = mode;
}

SRAM_TEXT_LOC
uint8_t _get_work_mode(uint8_t user)
{
    return _ctl.ai[user].mode;
}

void app_ai_voice_update_work_mode(uint8_t user, uint8_t mode)
{
    _update_work_mode(user, mode);
}

#ifdef VOC_ENCODE_ENABLE
bool app_ai_voice_pcm_data_encoding_handler(void)
{
    bool isDataConsumed = false;
    uint32_t purchasedBytes = 0;
    uint32_t queuedLen = app_ai_voice_get_cached_len_according_to_read_offset(_get_read_offset());
    uint8_t _oneShotProcessBuf[AI_VOICE_ONESHOT_PROCESS_MAX_LEN] = {0};

    if (queuedLen >= _handleFrameSize)
    {
        if (app_ai_voice_get_pcm_data(_get_read_offset(), _oneShotProcessBuf, _handleFrameSize))
        {
            // 20210424: fix the issue when close_mic has been done while ai_voice_thread still has some pending
            // message to do.
            uint32_t ret = 0;
            osMutexWait(ai_voice_mutex_id, osWaitForever);
            if (_ctl.runningCodec != 0)
            {
                ret = _encode_pcm_data(_oneShotProcessBuf, _handleFrameSize, &purchasedBytes);
            }
            osMutexRelease(ai_voice_mutex_id);
            if (ret)
            {
                isDataConsumed = true;
                if (purchasedBytes != _handleFrameSize)
                {
                    AI_VOICE_TRACE(0,"NOT all data request to handle is consumed(%d|%d)!!!", purchasedBytes, _handleFrameSize);
                }
                /// move the read cursor in AI data buffer
                _update_read_offset(_get_read_offset() + purchasedBytes);
            }
        }
    }
    return isDataConsumed;
}

uint32_t app_ai_voice_get_encoding_pcm_buf_offset(void)
{
    return _get_read_offset();
}

static void _handle_upstreaming_data(void)
{
    AI_connect_info *ai_info = app_ai_get_connect_info(ai_manager_get_foreground_ai_conidx());

    if (ai_info) //BES intentional code. ai_info has been checked.
    {
        AI_VOICE_TRACE(0,"%s", __func__);
        uint8_t ai_index = ai_info->ai_spec;
#ifdef ALEXA_M55_ENABLED    
        AI_VOICE_TRACE(1, "[ALEXA]mcu informAppFlag:%d", informAppFlag);
        if (informAppFlag == true)
        {            
             informAppFlag = false;
             /// inform specific AI to transmit data to APP
             if(ai_info ->ai_connect_type == AI_TRANSPORT_BLE)
                 ai_function_handle(API_DATA_SEND, NULL, 0, ai_index, SET_BLE_FLAG(ai_info->conidx));
             else
                 ai_function_handle(API_DATA_SEND, NULL, 0, ai_index, ai_info->device_id);
        }
#else
        bool isDataConsumed = app_ai_voice_pcm_data_encoding_handler();
        if (isDataConsumed)
        {
             /// inform specific AI to transmit data to APP
             if(ai_info ->ai_connect_type == AI_TRANSPORT_BLE)
                 ai_function_handle(API_DATA_SEND, NULL, 0, ai_index, SET_BLE_FLAG(ai_info->conidx));
             else
                 ai_function_handle(API_DATA_SEND, NULL, 0, ai_index, ai_info->device_id);
        }
#endif
    }
}
#endif

void app_ai_voice_default_upstream_data_handler(void)
{
#ifdef VOC_ENCODE_ENABLE
    _handle_upstreaming_data();
#endif
   
}

void app_ai_voice_default_hotword_detect_handler(uint8_t user)
{
#ifdef __THIRDPARTY
    app_thirdparty_specific_lib_event_handle(THIRDPARTY_FUNC_KWS,
                                             THIRDPARTY_MIC_DATA_COME,
                                             _user2spec[user]);
#endif
}

static void _ai_voice_thread(void const *argument)
{
    while (1)
    {
        AIV_MESSAGE_BLOCK *msg = NULL;

        if (!_mailbox_get(&msg))
        {
            AI_connect_info *ai_info;
            if (AIV_OP_HANDLE_DATA != msg->op)
            {
                AI_VOICE_TRACE(0,"incoming op:%s %d %d %d %p", _op_str[msg->op], _ctl.monopolizeFlag, _ctl.monopolizer, 
                    _get_work_mode(_ctl.monopolizer), _ctl.ai[_ctl.monopolizer].handlers.upstreamDataHandler);
            }

#ifdef MAI_TYPE_REBOOT_WITHOUT_OEM_APP
            if(ai_manager_get_spec_update_flag())
            {
                AI_VOICE_TRACE(0,"reboot ongoing, ignore ai operation:%s", _op_str[msg->op]);
                if((msg->op == AIV_OP_START_CAPTURE) && (_get_mic_state() == AIV_MIC_STATE_OPENING))
                    _update_mic_state(AIV_MIC_STATE_CLOSED);
                
                _mailbox_free(msg);
                continue;
            }
#endif
            switch (msg->op)
            {
            case AIV_OP_START_UPSTREAM:
                _ctl.streamState = AIV_OP_START_UPSTREAM;
                _on_upstream_started(msg->param0, msg->param1);
                break;
            case AIV_OP_STOP_UPSTREAM:
                /// inform AI voice that upstream is stopped
                _ctl.streamState = AIV_OP_STOP_UPSTREAM;
                _on_upstream_stopped(_ctl.monopolizer);
                /// fetch AI info
                ai_info = app_ai_get_connect_info(ai_manager_get_foreground_ai_conidx());
                if (ai_info && !is_sco_mode())
                {
                    if ((AI_IS_CONNECTED == ai_info->ai_connect_state) &&
                        app_ai_is_use_thirdparty(ai_info->ai_spec) && app_ai_is_enabled_wakeword(ai_info->ai_spec))
                    {
                        /// infrom specific AI that current conversation is done
                        #ifdef __THIRDPARTY
                        app_ai_if_thirdparty_event_handle(THIRDPARTY_FUNC_KWS, THIRDPARTY_HW_DETECT_START, ai_info->ai_spec);
                        #endif
                    }
                }
                else
                {
                    AI_VOICE_TRACE(0,"null ai info, foreground AI is %d", ai_manager_get_foreground_ai_conidx());
                }
                break;
            case AIV_OP_START_CAPTURE:
                _ctl.streamState = AIV_OP_START_CAPTURE;
                app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,
                                              BT_STREAM_AI_VOICE,
                                              0,
                                              0);
                break;
            case AIV_OP_STOP_CAPTURE:
                _ctl.streamState = AIV_OP_STOP_CAPTURE;
                app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,
                                              BT_STREAM_AI_VOICE,
                                              0,
                                              0);
                break;
            case AIV_OP_HANDLE_DATA:
                /// capture stream is monopolized
                _ctl.streamState = AIV_OP_HANDLE_DATA;
                if (_ctl.monopolizeFlag)
                {
                    if ((AIV_WORK_MODE_UPSTREAMING == _get_work_mode(_ctl.monopolizer)) &&
                        _ctl.ai[_ctl.monopolizer].handlers.upstreamDataHandler)
                    {
                        _ctl.ai[_ctl.monopolizer].handlers.upstreamDataHandler();
                    }
                    else
                    {
                        AI_VOICE_TRACE(0,"[%s] upstream data handler: %p, work mode:%s",
                              _user_str[_ctl.monopolizer],
                              _ctl.ai[_ctl.monopolizer].handlers.upstreamDataHandler,
                              _mode_str[_get_work_mode(_ctl.monopolizer)]);
                    }
                }
                else //!< capture stream is shared with all AIs
                {
                    for (uint8_t i = 0; i < AIV_USER_CNT; i++)
                    {
                        if (app_ai_voice_get_active_flag(i) &&
                            (AIV_WORK_MODE_HW_DETECTING == _get_work_mode(i)) &&
                            _ctl.ai[i].handlers.hwDetectHandler)
                        {
                            _ctl.ai[i].handlers.hwDetectHandler(i);
                        }
                    }
                }
                break;
            default:
                break;
            }

            _mailbox_free(msg);
        }
    }
}

void app_ai_voice_update_handle_frame_len(uint32_t len)
{
    /// add this check because of the limitation of one-shot process buffer(_oneShotProcessBuf) size
    ASSERT(AI_VOICE_ONESHOT_PROCESS_MAX_LEN >= len, "Length(%d) exceeded the limit(%d)", len, AI_VOICE_ONESHOT_PROCESS_MAX_LEN);
    AI_VOICE_TRACE(0,"One-shot process length update:%d->%d", _handleFrameSize, len);
    _handleFrameSize = len;
#ifdef TEST_VOICE_DUMP_AUDIO
    audio_dump_init(_handleFrameSize / 2, sizeof(short), 1);
#endif
}

void app_ai_voice_register_trigger_init_handler(TRIGGER_INIT_T handler)
{
    AI_VOICE_TRACE(0,"Capture stream trigger config handler update:%p->%p", _ctl.triggerInit, handler);
    _ctl.triggerInit = handler;
}

void app_ai_voice_thread_init(void)
{
    if (NULL == _ai_voice_tid)
    {
        /// creat AI voice thread
        _ai_voice_tid = osThreadCreate(osThread(_ai_voice_thread), NULL);
        /// creat mailbox
        _mailbox_init();

        if (NULL == ai_voice_mutex_id)
        {
            ai_voice_mutex_id = osMutexCreate((osMutex(ai_voice_mutex)));
        }
    }
}

bool app_ai_voice_is_mic_open(void)
{
    return (AIV_MIC_STATE_OPENED == _ctl.micState || AIV_MIC_STATE_OPENING == _ctl.micState) ? true:false;
}

bool app_ai_voice_is_in_upstream_state(void)
{
    return (_ctl.streamState == AIV_OP_START_UPSTREAM) ? true:false;
}

void app_ai_voice_register_raw_data_process_handler(RAW_DATA_PROCESS_CB handler)
{
    app_ai_voice_raw_data_process_handler = handler;
}

void * app_ai_voice_interface_of_raw_input_handler_get(void)
{
    return (void*)_mic_data_come;
}

uint32_t _mic_data_come(uint8_t* ptrBuf, uint32_t length)
{
    //AI_VOICE_TRACE(0,"%s dataLen:%d %d %d %d", __func__, length, _ctl.streamState, ai_mic_data_discard_flag, _ctl.monopolizeFlag);
    static uint8_t mic_data_cnt = 0;
    if(_ctl.streamState == AIV_OP_STOP_CAPTURE)
    {
        return 0;
    }
    //discard the first 500ms data after open mic.
    if (ai_mic_data_discard_flag)
    {
        ai_mic_data_discard_cnt++;
        if (ai_mic_data_discard_cnt < (AI_DISCARD_MIC_DATA_TIME_AFTER_OPEN_MIC*32/length))
        {
            return length;
        }
        else
        {
            ai_mic_data_discard_flag = false;
            ai_mic_data_discard_cnt = 0;
        }
    }

    if (app_ai_voice_raw_data_process_handler)
    {
        app_ai_voice_raw_data_process_handler(ptrBuf, length);
    }

#if !defined(ANC_ASSIST_ENABLED)
    int16_t *pcm_buf = (int16_t *)ptrBuf;
    for (uint32_t i = 0; i < length / sizeof(int16_t); i++)
    {
        pcm_buf[i] = speech_ssat_int16((int32_t)pcm_buf[i] * 12); // 12: 21.58dB
    }
#endif

#if defined(STEREO_RECORD_PROCESS)
    stereo_record_process_run((short *)ptrBuf, length / sizeof(short));
#endif

#ifdef TEST_VOICE_DUMP_AUDIO
    audio_dump_add_channel_data(0, (short *)ptrBuf, length / 2);
    audio_dump_run();
#endif

#ifdef ALEXA_M55_ENABLED
    ALEXA_PCM_TO_M55_T alexa_pcm;
    alexa_pcm.in_ptr = ptrBuf;
    //alexa_pcm.out_ptr = encodedDataBuf;
    alexa_pcm.length = length;
#ifdef TEST_VOICE_DUMP_AUDIO
    audio_dump_add_channel_data(0, (short *)ptrBuf, length / 2);
    audio_dump_run();
#endif
    app_dsp_m55_mcu_ai_send_msg_with_waiting_rsp(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_MIC_DATA, &alexa_pcm, sizeof(alexa_pcm));
    //app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_MIC_DATA, &alexa_pcm, sizeof(alexa_pcm));
#else
    /// enqueue the data
    app_ai_voice_lock_buf();
    EnCQueue_AI(&pcm_queue, ptrBuf, length, NULL);
    app_ai_voice_release_buf();

    /// analysis if AI need to update its read offset
    for (uint8_t user = AIV_USER_INVALID; user < AIV_USER_CNT; user++)
    {
        if (_ctl.ai[user].flag.active &&
            (AIV_WORK_MODE_HW_DETECTING == _ctl.ai[user].mode) &&
            _ctl.ai[user].handlers.readOffsetGetter &&
            _ctl.ai[user].handlers.readOffsetSetter)
        {
            /// ensure that the space sized in AI_RESERVED_SPACE_IN_CACHE_BUF is reserved for conceret AI
            uint32_t cachedLen = app_ai_voice_get_cached_len_according_to_read_offset(_ctl.ai[user].handlers.readOffsetGetter());
            if (cachedLen + AI_RESERVED_SPACE_IN_CACHE_BUF > (uint32_t)pcm_queue.size)
            {
                //AI_VOICE_TRACE(0,"!!!PCM data process too slow[%s]", _user_str[user]);
                _ctl.ai[user].handlers.readOffsetSetter(_ctl.ai[user].handlers.readOffsetGetter() + cachedLen + AI_RESERVED_SPACE_IN_CACHE_BUF - pcm_queue.size);
            }
        }
    }
#ifndef __BIXBY_VOICE__
    if (_ctl.monopolizeFlag)
    {
        uint32_t cachedLen = app_ai_voice_get_cached_len_according_to_read_offset(_ctl.readOffset);
        if (cachedLen + AI_RESERVED_SPACE_IN_CACHE_BUF > (uint32_t)pcm_queue.size)
        {
            AI_VOICE_TRACE(0,"!!!PCM data process too slow[UPSTREAM]");
            _update_read_offset(_ctl.readOffset + cachedLen + AI_RESERVED_SPACE_IN_CACHE_BUF - pcm_queue.size);
        }
    }
#endif
#endif
    /// inform AI voice thread to handle data
    if(mic_data_cnt >= MSG_DATA_CNT)
    {
        mic_data_cnt = 0;
#ifdef ALEXA_M55_ENABLED
        informAppFlag = true;
#endif
        /// inform AI voice thread to handle data
        AIV_MESSAGE_BLOCK msg = {
            .op = AIV_OP_HANDLE_DATA,
        };
        _mailbox_put(&msg);
    }
    mic_data_cnt++;

    return length;
}

#ifdef VOC_ENCODE_ENABLE
/**
 * @brief Encode PCM data
 *  NOTE: input data must be 16bit width
 *
 * @param ptrBuf        pointer of PCM data
 * @param length        length of PCM data
 * @param purchasedBytes Encode consumed PCM data length
 * @return uint32_t     encoded data length
 */
static uint32_t _encode_pcm_data(uint8_t *ptrBuf, uint32_t length, uint32_t *purchasedBytes)
{
    uint8_t ai_index = voice_compression_get_user();
    uint32_t outputLen = 0;

    if (AI_SPEC_INIT == ai_index)
    {
        AI_VOICE_TRACE(0,"%s shouldn't encode, compression user:%s",
              __func__, _user_str[app_ai_voice_get_user_from_spec(ai_index)]);
        return 0;
    }

    //BES intentional code. This statement is reachable
    if (ai_struct[ai_index].ignoredPcmDataRounds < VOB_IGNORED_FRAME_CNT)
    {
        AI_VOICE_TRACE(0,"%s skip encode, ai:%d ignoredPcmDataRounds:%d",
              __func__, ai_index, ai_struct[ai_index].ignoredPcmDataRounds);
        ai_struct[ai_index].ignoredPcmDataRounds++;
        return 0;
    }

    /// encode the voice PCM data
#ifdef SPEECH_CAPTURE_TWO_CHANNEL
    uint8_t data_buf[3200];
    uint32_t i = 0;
    for (i = 0; i < (length / 4); i++)
    {
        data_buf[i * 2] = ptrBuf[i * 4];
        data_buf[i * 2 + 1] = ptrBuf[i * 4 + 1];
    }

    outputLen += voice_compression_handle(app_ai_get_encode_type(ai_index), data_buf,
                                          BIT16_PCM_SIZE_TO_SAMPLE_CNT((length / 2)),
                                          purchasedBytes,
                                          app_ai_is_algorithm_engine_reset(ai_index));

    /// compression ok
    if (outputLen)
    {
        app_ai_set_algorithm_engine_reset(false, ai_index);
    }

    for (i = 0; i < (length / 4); i++)
    {
        data_buf[i * 2] = ptrBuf[i * 4 + 2];
        data_buf[i * 2 + 1] = ptrBuf[i * 4 + 3];
    }

    outputLen += voice_compression_handle(app_ai_get_encode_type(ai_index), data_buf,
                                          BIT16_PCM_SIZE_TO_SAMPLE_CNT((length / 2)),
                                          purchasedBytes,
                                          app_ai_is_algorithm_engine_reset(ai_index));

    *purchasedBytes = ((*purchasedBytes) * 2);
#else

    outputLen += voice_compression_handle(app_ai_get_encode_type(ai_index), ptrBuf,
                                          BIT16_PCM_SIZE_TO_SAMPLE_CNT(length),
                                          purchasedBytes,
                                          app_ai_is_algorithm_engine_reset(ai_index));
    /// compression ok
    if (outputLen)
    {
        app_ai_set_algorithm_engine_reset(false, ai_index);
    }
#endif

    return outputLen;
}
#endif

void app_ai_voice_lock_buf(void)
{
    osMutexWait(pcm_queue_mutex_id, osWaitForever);
}

void app_ai_voice_release_buf(void)
{
    osMutexRelease(pcm_queue_mutex_id);
}

uint32_t app_ai_voice_get_ai_voice_buf_len(void)
{
    return _ctl.pcmQueueSize;
}

uint32_t app_ai_voice_get_buf_read_offset(void)
{
    app_ai_voice_lock_buf();
    uint32_t read_offset = GetCQueueReadOffset(&pcm_queue);
    app_ai_voice_release_buf();

    return read_offset;
}

uint32_t app_ai_voice_get_buf_write_offset(void)
{
    app_ai_voice_lock_buf();
    uint32_t write_offset = GetCQueueWriteOffset(&pcm_queue);
    app_ai_voice_release_buf();

    return write_offset;
}

uint32_t app_ai_voice_get_pcm_queue_len(void)
{
    app_ai_voice_lock_buf();
    uint32_t queue_len = LengthOfCQueue(&pcm_queue);
    app_ai_voice_release_buf();

    return queue_len;
}

uint32_t app_ai_voice_pcm_queue_read_dec(uint32_t readOffset, uint32_t dec)
{
    ASSERT(PCM_DATA_CACHE_BUF_SIZE >= readOffset, "%s invalid read offset(%d) received", __func__, readOffset);
    ASSERT(PCM_DATA_CACHE_BUF_SIZE >= dec, "%s invalid dec len(%d) received", __func__, dec);

    uint32_t newRead = 0;

    /// calculate the new read offset
    if (readOffset >= dec)
    {
        newRead = readOffset - dec;
    }
    else
    {
        newRead = PCM_DATA_CACHE_BUF_SIZE - dec + readOffset;
    }

    return newRead;
}

uint32_t app_ai_voice_get_cached_len_according_to_read_offset(uint32_t offset)
{
    ASSERT((offset <= PCM_DATA_CACHE_BUF_SIZE),
           "invalid offset %d received.", offset);

    uint32_t cached_len = 0;
    uint32_t write_offset = 0;

    app_ai_voice_lock_buf();
    write_offset = GetCQueueWriteOffset(&pcm_queue);

    if (write_offset >= offset)
    {
        cached_len = write_offset - offset;
    }
    else
    {
        cached_len = PCM_DATA_CACHE_BUF_SIZE - offset + write_offset;
    }
    app_ai_voice_release_buf();

    // AI_VOICE_TRACE(0,"read offset:%d write offset %d len %d", offset, write_offset, cached_len);

    return cached_len;
}

bool app_ai_voice_get_pcm_data(uint32_t offset, uint8_t *buf, uint32_t len)
{
    ASSERT(buf, "%s null pointer received", __func__);

    app_ai_voice_lock_buf();
    if (AIV_MIC_STATE_CLOSED != _get_mic_state())
    {
        uint32_t queue_len = app_ai_voice_get_cached_len_according_to_read_offset(offset);
        //AI_VOICE_TRACE(0,"queued len:%d", queue_len);
        if (len > queue_len)
        {
            AI_VOICE_TRACE(0,"[app_ai_voice_get_pcm_data]invalid len received: %d|%d!!", len, queue_len);
            app_ai_voice_release_buf();
            return false;
        }
        PeekCQueueToBufWithOffset(&pcm_queue, buf, len, offset);
    }
    app_ai_voice_release_buf();
    return true;
}

int app_ai_voice_stream_init(uint8_t user)
{
    int ret = -1;
    uint8_t ai_index = _user2spec[user];
    AI_VOICE_TRACE(0,"%s AI(%s) running %d", __func__,
          _user_str[user],
          app_ai_is_stream_init(ai_index));

    if (!app_ai_is_stream_init(ai_index))
    {
        app_ai_set_stream_init(true, ai_index);

#ifdef __IAG_BLE_INCLUDE__
        app_ai_ble_update_conn_param_mode(true, ai_index, ai_manager_get_foreground_ai_conidx());
#endif

        app_ai_set_algorithm_engine_reset(true, ai_index);
        ai_struct[ai_index].ai_stream.sentDataSizeWithInFrame = 0;
        ai_struct[ai_index].ai_stream.seqNumWithInFrame = 0;

        ai_transport_fifo_deinit(ai_index);
        // TODO: check last parameter
        ai_function_handle(API_DATA_INIT, NULL, 0, ai_index, 0); //!< get memory for transfer
        ret = 0;
    }

    return ret;
}

static int _upstream_deinit(uint8_t ai_index)
{
    if (AI_SPEC_GSOUND == ai_index)
    {
        return 0;
    }

    AI_VOICE_TRACE(0,"%s running %d", __func__, app_ai_is_stream_init(ai_index));

    if (AIV_WAKEUP_TRIGGER_PRESS_AND_HOLD != app_ai_get_wake_up_type(ai_index))
    {
        if (NULL != app_ai_voice_timeout_timer_id)
        {
            osTimerStop(app_ai_voice_timeout_timer_id);
        }
    }

    ai_function_handle(API_DATA_DEINIT, NULL, 0, ai_index, 0);
    ai_audio_stream_allowed_to_send_set(false, ai_index);
    app_ai_set_stream_opened(false, ai_index);
    ai_transport_fifo_deinit(ai_index);
    ai_set_can_wake_up(true, ai_index);
    app_ai_check_if_need_set_thirdparty(ai_index);

    _update_read_offset(0);

#ifdef OPUS_IN_OVERLAY
    if (app_ai_is_opus_in_overlay(ai_index))
    {
        if (app_get_current_overlay() == APP_OVERLAY_OPUS)
        {
            app_overlay_unloadall();
        }
    }
#endif

    return 0;
}

int app_ai_voice_deinit(uint8_t ai_index, uint8_t ai_connect_index)
{
    AI_VOICE_TRACE(0,"%s running %d", __func__, app_ai_is_stream_init(ai_index));

    app_ai_set_stream_init(false, ai_index);

    _upstream_deinit(ai_index);

#ifdef __IAG_BLE_INCLUDE__
    // app_ai_if_ble_update_conn_param(false, ai_index, ai_connect_index);
#endif

    return 0;
}

static int _open_mic(void)
{
    int ret = 0;
    bool micOpened = (AIV_MIC_STATE_OPENED == _get_mic_state());
    AI_VOICE_TRACE(0,"%s current mic state:%s", __func__, micOpened ? "OPEN" : "CLOSE");

    if (!micOpened)
    {
        /// system configuration
        af_set_priority(AF_USER_AI, osPriorityHigh);
        /// config sysfreq to optimize performance
        app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, AIV_DEFAULT_FREQ_VALUE);
        // AI_VOICE_TRACE(0,"====>sys freq calc : %d\n", hal_sys_timer_calc_cpu_freq(50, 0));

#if defined(A2DP_THIRDPARTY_ALGO)
        if (bt_media_cur_is_bt_stream_music())
        {
            AI_VOICE_TRACE(1, "music on,gva hotword request 26M");
            app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, APP_SYSFREQ_26M);
        }
#endif
        app_ai_open_mic_user_set(app_ai_manager_get_current_spec());

#ifdef TEST_VOICE_DUMP_AUDIO
        audio_dump_init(AI_VOICE_ONESHOT_PROCESS_MAX_LEN / 2, sizeof(short), 1);
#endif
        /// system resource initialization
        if (NULL == pcm_queue_mutex_id)
        {
            pcm_queue_mutex_id = osMutexCreate((osMutex(pcm_queue_mutex)));
        }
        if (NULL == pcm_encode_queue_mutex_id)
        {
            pcm_encode_queue_mutex_id = osMutexCreate((osMutex(pcm_encode_queue_mutex)));
        }
        if (NULL == pcm_cache_buf)
        {
            AI_VOICE_TRACE(0,"PCM data cache queue size:%d", PCM_DATA_CACHE_BUF_SIZE);
            app_ai_if_mempool_get_buff(&pcm_cache_buf, PCM_DATA_CACHE_BUF_SIZE, AI_CAPTURE_MEMPOOL_USER_PCM_CACHE);
        }
        InitCQueue(&pcm_queue, PCM_DATA_CACHE_BUF_SIZE, (CQItemType *)pcm_cache_buf);
        /// update read offset in AI voice module
        _update_read_offset(0);
        AI_VOICE_TRACE(0,"capture stream sample rate:%d, bitwidth:%d, channel_num:%d",
              _ctl.streamCfg.sample_rate, _ctl.streamCfg.bits, _ctl.streamCfg.channel_num);
        /// init stream configurations
        _ctl.streamCfg.data_size = (uint32_t)CAPTURE_INTERVAL_MS * CAPTURE_PINGPANG_CNT *
                                   (_ctl.streamCfg.sample_rate / 1000) *
                                   _byteWidthMap[_ctl.streamCfg.bits / AUD_BITS_8] *
                                   _ctl.streamCfg.channel_num;
        ASSERT(PCM_AUDIO_FLINGER_BUF_SIZE >= _ctl.streamCfg.data_size, "Audio flinger PCM data cache buffer exceeded");
        if (NULL == bt_audio_buf)
        {
            app_ai_if_mempool_get_buff(&bt_audio_buf, _ctl.streamCfg.data_size, AI_CAPTURE_MEMPOOL_USER_AUDIOFLINGER);
        }
        _ctl.streamCfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buf);
        AI_VOICE_TRACE(0,"Audio-flinger PCM data buffer size:%d", _ctl.streamCfg.data_size);

#if defined(STEREO_RECORD_PROCESS)
        ASSERT(_ctl.streamCfg.channel_num == AUD_CHANNEL_NUM_1, "[%s] channel_num(%d) != AUD_CHANNEL_NUM_1", __func__, _ctl.streamCfg.channel_num);
        ASSERT(_ctl.streamCfg.bits == AUD_BITS_16, "[%s] bits(%d) != AUD_BITS_16", __func__, _ctl.streamCfg.bits);

        stereo_record_process_init(_ctl.streamCfg.sample_rate, _ctl.streamCfg.data_size / 2 / sizeof(short), app_ai_if_mempool_get_buff);
#endif

#ifndef VOICE_DETECTOR_SENS_EN
#ifdef VOICE_DETECTOR_EN
        app_voice_detector_open(VOICE_DETECTOR_ID_0, AUD_VAD_TYPE_DIG);

        app_voice_detector_setup_stream(VOICE_DETECTOR_ID_0,
                                        AUD_STREAM_CAPTURE,
                                        &_ctl.streamCfg);

        app_voice_detector_setup_callback(VOICE_DETECTOR_ID_0,
                                          VOICE_DET_CB_RUN_DONE,
                                          NULL,
                                          NULL);

        alexa_wwe_detect_times = 0;
        app_voice_detector_capture_enable_vad(VOICE_DETECTOR_ID_0, VOICE_DET_USER_AI);

#elif defined(ANC_ASSIST_ENABLED)
        // TODO: Dynamic switch between ASSIST_AI_VOICE_MODE_KWS and ASSIST_AI_VOICE_MODE_RECORD
        if (app_ai_voice_get_active_flag(AIV_USER_REC))
        {
            app_voice_assist_ai_voice_open(ASSIST_AI_VOICE_MODE_RECORD, &_ctl.streamCfg, app_ai_if_mempool_get_buff);
        }
        else
        {
            app_voice_assist_ai_voice_open(ASSIST_AI_VOICE_MODE_KWS, &_ctl.streamCfg, app_ai_if_mempool_get_buff);
        }
#else
#ifdef GSOUND_M33_ENABLED
        _ctl.streamCfg.channel_map =  (enum AUD_CHANNEL_MAP_T)AUD_CHANNEL_MAP_CH0;
#endif 
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &_ctl.streamCfg);
#endif
#else
        app_sensor_hub_ai_mcu_request_vad_data_start();
#endif
        /// initialization for concrete AI
        ret = _on_stream_started();

        if (0 == ret)
        {
#if !defined(VOICE_DETECTOR_SENS_EN)
            /// trigger hardware
            af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
#endif
            /// update flag
            _update_mic_state(AIV_MIC_STATE_OPENED);
            ai_mic_data_discard_flag = true;
            ai_mic_data_discard_cnt = 0;
#ifdef AI_M55_ENABLED
            app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_INIT, DSP_M55_AI_COMMON_CMD_OPEN_MIC, NULL, 0);
#endif
        }
        else
        {
            app_ai_voice_stream_control(false, ret);
        }
    }

    return 0;
}

static int _close_mic(void)
{
    int ret = -1;
    bool micOpened = (AIV_MIC_STATE_CLOSED != _get_mic_state());
    AI_VOICE_TRACE(0,"%s current mic state:%s", __func__, micOpened ? "OPEN" : "CLOSE");

    osMutexWait(ai_voice_mutex_id, osWaitForever);
    if (micOpened)
    {
#ifdef IS_MULTI_AI_ENABLED
        if (app_ai_open_mic_user_get() == AI_SPEC_GSOUND)
        {
#if defined(A2DP_THIRDPARTY_ALGO)
            if (bt_media_cur_is_bt_stream_music())
            {
                AI_VOICE_TRACE(1, "gva hotword off, music request 78M.");
                app_audio_set_a2dp_freq(APP_SYSFREQ_78M);
            }
#endif
        }
        app_ai_open_mic_user_set(AI_SPEC_INIT);
#endif
        /// close AI capture stream
#ifdef AI_M55_ENABLE
        AI_VOICE_TRACE(1, "[ALEXA]:%s", __func__);
        app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_INIT, DSP_M55_AI_COMMON_CMD_CLOSE_MIC, NULL, 0);
#endif
#ifndef VOICE_DETECTOR_SENS_EN
#ifdef VOICE_DETECTOR_EN
        app_voice_detector_close(VOICE_DETECTOR_ID_0);
#elif defined(ANC_ASSIST_ENABLED)
        app_voice_assist_ai_voice_close();
#else
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
#endif
#else
        app_sensor_hub_ai_mcu_request_vad_data_close();
#endif
        /// excute callback function of stream stopped
        _on_stream_stopped();
        ret = 0;
    }
    osMutexRelease(ai_voice_mutex_id);

    return ret;
}

int app_ai_voice_start_mic_stream(void)
{
    int ret = -1;
    if (AIV_MIC_STATE_OPENED != _get_mic_state())
    {
        if (_is_any_active())
        {
            /// open the capture stream
            ret = _open_mic();
        }
        else
        {
            /// to inform audio manager to clear the AI active flag
            /// put message to AI voice thread to close mic
            AIV_MESSAGE_BLOCK msg = {
                .op = AIV_OP_STOP_CAPTURE,
            };
            _mailbox_put(&msg);
        }
    }
    else
    {
        AI_VOICE_TRACE(0,"mic open skip, reason: already opened");
    }

    return ret;
}

// close stream
int app_ai_voice_stop_mic_stream(void)
{
    uint8_t foreground_ai = ai_manager_get_foreground_ai_conidx(); 
    AI_connect_info *ai_info =  app_ai_get_connect_info(foreground_ai);
    if(NULL == ai_info) {   //BES intentional code. ai_info has been checked.
        return -1;
    }

    AI_VOICE_TRACE(0,"%s mic state:%s", __func__, _mic_str[_get_mic_state()]);

    if (AIV_MIC_STATE_CLOSED != _get_mic_state())
    {
        app_ai_set_stream_running(false, ai_info->ai_spec);

        //if (!app_ai_is_use_thirdparty(ai_index))
        {
            _close_mic();
            app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, APP_SYSFREQ_32K);
        }

        app_ai_voice_deinit(ai_info->ai_spec, foreground_ai);

        return 0;
    }

    return -1;
}

extern "C" bool get_alexa_service_enable_state();
uint32_t app_ai_voice_thirdparty_wake_up_callback(void *wakeup_info, uint32_t read_offset)
{
    AI_VOICE_TRACE(0,"%s", __func__);
    /// incoming parameter check
    ASSERT(wakeup_info, "%s NULL pointer received", __func__);
    AIV_WAKEUP_INFO_T *pInfo = (AIV_WAKEUP_INFO_T *)wakeup_info;
    ASSERT((AIV_USER_INVALID < pInfo->user) && (AIV_USER_CNT > pInfo->user), "Illegal user:%d", pInfo->user);
    ASSERT((AIV_WAKEUP_TRIGGER_NONE < pInfo->triggerType) && (AIV_WAKEUP_TRIGGER_CNT > pInfo->triggerType),
           "Illegal trigger type:%d", pInfo->triggerType);

    uint8_t ai = _user2spec[pInfo->user];
    uint32_t ret = 1; //!< error code

#ifdef __AMA_VOICE__
    //play NC tone and return
    if(AI_SPEC_AMA == ai && (app_ai_dont_have_mobile_connect(ai) || get_alexa_service_enable_state() == 0)) {
        AI_VOICE_TRACE(0,"%s AMA not connected", __func__);
        //media_PlayAudio(AUDIO_ID_BT_ALEXA_NC, 0);
        return 0;
    }
#endif

    if (app_ai_is_use_thirdparty(ai))
    {
        uint8_t dest_id;
        /// fetch the wake info
        AI_VOICE_TRACE(0,"@@@AI wakeup, user:%s trigger type:%d, start idx:%d, end indx:%d, score:%d",
              _user_str[pInfo->user], pInfo->triggerType, pInfo->sIdx, pInfo->eIdx, pInfo->score);

        ai_struct[ai].wake_word_start_index = pInfo->sIdx;
        ai_struct[ai].wake_word_end_index = pInfo->eIdx;
        app_ai_set_wake_up_type(pInfo->triggerType, ai);
        ai_set_can_wake_up(true, ai);
        // TODO: check the last parameter
        dest_id = app_ai_get_dest_id_by_ai_spec(ai);
        AI_VOICE_TRACE(0,"ai:%d, dest_id:%d",ai, dest_id);
        ret = ai_function_handle(CALLBACK_WAKE_UP, wakeup_info, read_offset, ai, dest_id);
    }

    return ret;
}

static int _start_stream(uint8_t user)
{
    int ret = -1;
    bool startAllowed = true;
    char *reason = NULL;

    /// update active flag for requester
    app_ai_voice_set_active_flag(user, true);
    /// check if start stream allowed
#if defined(IBRT) && !defined(FREEMAN_ENABLED_STERO)
    if (bts_ui_role_is_slave() &&
        !app_ai_voice_get_slave_open_mic_flag(user))
    {
        AI_VOICE_TRACE(0,"%s start stream skip because slave role", _user_str[user]);
        startAllowed = false;
        reason = (char *)"slave role";
    }
#endif
    if (app_ai_is_sco_mode())
    {
        AI_VOICE_TRACE(0,"%s start stream skip because in sco mode", _user_str[user]);
        startAllowed = false;
        reason = (char *)"in sco mode";
    }

    /// excute request
    if (startAllowed)
    {
        switch (_ctl.micState)
        {
        case AIV_MIC_STATE_OPENED:
            ret = __on_concrete_ai_started(user);
            break;
        case AIV_MIC_STATE_OPENING:
            ret = 0;
            /// callback will excute in @see _open_mic, do nothing here
            break;
        default:
            ret = 0;
            /// update mic state
            _update_mic_state(AIV_MIC_STATE_OPENING);
            /// put message to AI voice thread to open mic
            AIV_MESSAGE_BLOCK msg = {
                .op = AIV_OP_START_CAPTURE,
                .param0 = user,
            };
            _mailbox_put(&msg);
            break;
        }
    }
    else
    {
        AI_VOICE_TRACE(0,"%s start stream skip, reason:[%s]", _user_str[user], reason);
    }

    return ret;
}

static int __on_concrete_ai_started(uint8_t user)
{
    int ret = 0;
    AI_VOICE_TRACE(0,"%s active AI user %s",__func__, _user_str[user]);

    if (app_ai_voice_get_active_flag(user) &&
        (!_is_super_user_exist() || _get_super_user_flag(user)))
    {
        if (!app_ai_is_stream_running(_user2spec[user]) &&
            _ctl.ai[user].handlers.streamStarted)
        {
            ret = _ctl.ai[user].handlers.streamStarted();
            if (ret)
            {
                AI_VOICE_TRACE(0,"start stream denied by %s", _user_str[user]);
                ret = user;
            }
            else
            {
                app_ai_set_stream_running(true, _user2spec[user]);
                if(_ctl.monopolizeFlag) {
                    _update_work_mode(_ctl.monopolizer, AIV_WORK_MODE_UPSTREAMING);
                }
            }
        }
    }

    return ret;
}

static int _on_stream_started(void)
{
    int ret = 0;
    uint8_t spec = ai_manager_get_current_spec();
    ret = __on_concrete_ai_started(_spec2user[spec]);

    return ret;
}

static int _stop_stream(uint8_t user)
{
    int ret = -1;
    bool stopAllowed = true;

    /// update active flag for requester
    app_ai_voice_set_active_flag(user, false);
    /// check if stop stream is allowed
    if (!_get_super_user_flag(user)) //!< super user could forcely stop the stream
    {
        for (uint8_t i = 0; i < AIV_USER_CNT; i++)
        {
            if (_ctl.ai[i].flag.active)
            {
                AI_VOICE_TRACE(0,"%s stop stream skip, reason:[active AI %s]", _user_str[user], _user_str[i]);
                stopAllowed = false;
                break;
            }
        }
    }

    if (stopAllowed)
    {
        ret = 0;
        switch (_ctl.micState)
        {
        case AIV_MIC_STATE_CLOSED:
            /// close mic already excuted
            break;
        case AIV_MIC_STATE_CLOSING:
            /// callback will excute in @see _close_mic, do nothing here
            break;
        default:
            _update_mic_state(AIV_MIC_STATE_CLOSING);
            AIV_MESSAGE_BLOCK msg = {
                .op = AIV_OP_STOP_CAPTURE,
                .param0 = user,
            };
            _mailbox_put(&msg);
            break;
        }
    }
    else
    {
        _on_concrete_ai_stream_stopped(user);
    }

    return ret;
}

static void _on_concrete_ai_stream_stopped(uint8_t user)
{
    if ((AIV_MIC_STATE_CLOSED != _get_mic_state()) &&
        (AIV_MIC_STATE_CLOSING != _get_mic_state()))
    {
        /// stop upstream if necessary
        if (_ctl.monopolizeFlag &&
            (user == _ctl.monopolizer))
        {
            _on_upstream_stopped(_ctl.monopolizer);
        }

        /// excute stream stop callback for conceret AI
        if ((AIV_WORK_MODE_IDLE != _get_work_mode(user)) &&
            _ctl.ai[user].handlers.streamStopped)
        {
            _ctl.ai[user].handlers.streamStopped();
        }
        /// update stream state
        app_ai_set_stream_running(false, _user2spec[user]);
        /// update workmode for concrete AI
        _update_work_mode(user, AIV_WORK_MODE_IDLE);
    }
    else
    {
        AI_VOICE_TRACE(0,"%s wait for capture stream close", _user_str[user]);
    }
}

static void _on_stream_stopped(void)
{
    AI_VOICE_TRACE(0,"%s", __func__);
    if (AIV_MIC_STATE_CLOSED != _get_mic_state())
    {
        /// inform AI voice that upstream is stopped
        if (_ctl.monopolizeFlag)
        {
            _on_upstream_stopped(_ctl.monopolizer);
        }
        /// excute stream stopped callback functions for AIs
        for (uint8_t i = 0; i < AIV_USER_CNT; i++)
        {
            /// excute stream stop callback for conceret AI
            if ((AIV_WORK_MODE_IDLE != _get_work_mode(i)) &&
                _ctl.ai[i].handlers.streamStopped)
            {
                _ctl.ai[i].handlers.streamStopped();
            }
            /// update stream state
            app_ai_set_stream_running(false, _user2spec[i]);
            /// update workmode for concrete AI
            _update_work_mode(i, AIV_WORK_MODE_IDLE);
        }

        /// update capture stream state
        _update_mic_state(AIV_MIC_STATE_CLOSED);

#if defined(STEREO_RECORD_PROCESS)
        stereo_record_process_deinit();
#endif

#ifdef VOC_ENCODE_ENABLE
        /// deinit compression buffer
        voice_compression_dinit_buffer();
#endif
        /// deinit AI stream used buffer
        //when close mic, need reset pcm_queue, because AI module maybe get PCM buffer queue information.
        app_ai_voice_lock_buf();
        ResetCQueue(&pcm_queue);
        app_ai_voice_release_buf();
        app_ai_if_mempool_deinit();
        pcm_cache_buf = NULL;
        bt_audio_buf = NULL;
        /// restore the audio-flinger priority
        af_set_priority(AF_USER_AI, osPriorityAboveNormal);
        /// reset the one-shot handle data length
        app_ai_voice_update_handle_frame_len(AI_VOICE_ONESHOT_PROCESS_MAX_LEN);
    }
    else
    {
        AI_VOICE_TRACE(0,"%s skip, already closed", __func__);
    }
}

int app_ai_voice_stream_control(bool on, uint8_t user)
{
    int ret = 0;

    if (AIV_USER_CNT == user)
    {
        return ret;
    }

    if (on)
    {
        ret = _start_stream(user);
    }
    else
    {
        ret = _stop_stream(user);
    }

    return ret;
}

static int _start_upstream(uint8_t user, uint32_t offset)
{
    int ret = 0;
    /// open mic if it is closed
    if (!app_ai_voice_is_mic_open())
    {
        ret = _start_stream(user);
    }

    /// start upstream request
    if (0 == ret)
    {
#ifdef ALEXA_M55_ENABLED
        AI_VOICE_TRACE(0, "entry _start_upstream send to m55++++++");
        ALEXA_UPSTREAM_STATUS_T alexaUptreamStatus;
        //alexaUptreamStarted.user = user;
        alexaUptreamStatus.flag = UPSTREAM_START;
        app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_UPSTREAM_STATUS, &alexaUptreamStatus, sizeof(alexaUptreamStatus));
        AI_VOICE_TRACE(0, "entry _start_upstream send to m55------");
#else
#endif
        AIV_MESSAGE_BLOCK msg = {
            .op = AIV_OP_START_UPSTREAM,
            .param0 = user,
            .param1 = offset,
        };
        _mailbox_put(&msg);
    }
    else
    {
        AI_VOICE_TRACE(0,"Skip upstream start, reason: start stream failed");
    }

    return ret;
}


static void _on_upstream_started(uint8_t user, uint32_t offset)
{
    AI_VOICE_TRACE(0,"%s %s", _user_str[user], __func__);

    /// update the read offset for AI voice module
    uint32_t readOffset = offset;
    uint8_t aiIndex = _user2spec[user];

    if (UPSTREAM_INVALID_READ_OFFSET == readOffset)
    {
        AI_VOICE_TRACE(0,"_on_upstream_started reset AIV readoffset to pcm cache queue start point");
        readOffset = app_ai_voice_get_buf_read_offset();
    }
    _update_read_offset(readOffset);

    ASSERT(AIV_USER_BES <= user && AIV_USER_CNT > user, "Invalid user %d received", user);

    /// monopolize the stream
    app_ai_voice_request_monopolize_stream(user);
    /// config connection param to optimize
    app_ai_voice_stay_active(aiIndex);
    /// acquire system frequency to optimize the performance
    app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, AIV_UPLOADING_BOOST_FREQ_VALUE);

#ifdef VOC_ENCODE_ENABLE 
    /// get the codec type
    uint8_t codec = app_ai_get_encode_type(aiIndex);
#ifdef ALEXA_M55_ENABLED  
    mcu_init_encode_data_queue();
    
    ALEXA_ENCODE_INIT_T encodeInit;
    encodeInit.codec = codec;
    encodeInit.user = aiIndex;
    encodeInit.deinit = false;
    app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_INIT, &encodeInit, sizeof(encodeInit));
#else
    
    /// init encoder
    app_ai_voice_init_encoder(user, codec);
#endif
#endif

}

static int _stop_upstream(uint8_t user, uint32_t offset)
{
#ifdef IBRT
    if (bts_ui_role_is_slave() &&
        AIV_USER_REC != user)
    {
        AI_VOICE_TRACE(0,"Slave will not close mic for AI");
        return -1;
    }
#endif

    int ret = 0;
    if ((AIV_WORK_MODE_UPSTREAMING == _get_work_mode(user)) &&
        (_ctl.monopolizer == user) &&
        !app_ai_is_sco_mode())
    {
#ifdef ALEXA_M55_ENABLED
    ALEXA_UPSTREAM_STATUS_T alexaUptreamStatus;
    //alexaUptreamStarted.user = user;
    alexaUptreamStatus.flag = UPSTREAM_STOP;
    app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_UPSTREAM_STATUS, &alexaUptreamStatus, sizeof(alexaUptreamStatus));
#else
#endif
        AIV_MESSAGE_BLOCK msg = {
            .op = AIV_OP_STOP_UPSTREAM,
            .param0 = user,
            .param1 = offset,
        };
        _mailbox_put(&msg);
    }
    else
    {
        AI_VOICE_TRACE(0,"AIV work mode:%s, monopolizer:%s, incoming user:%s, in_sco:%d",
              _mode_str[_get_work_mode(user)],
              _user_str[_ctl.monopolizer],
              _user_str[user],
              app_ai_is_sco_mode());
    }

    return ret;
}

int app_ai_voice_upstream_control(bool on, uint8_t user, uint32_t offset)
{
    int ret = 0;

    if (on) //!< try start upstream
    {
        ret = _start_upstream(user, offset);
    }
    else //!< try to stop upstream
    {
        ret = _stop_upstream(user, offset);
    }

    return ret;
}

#ifdef VOC_ENCODE_ENABLE
void app_ai_voice_init_encoder(uint8_t user, uint8_t codec)
{
    osMutexWait(ai_voice_mutex_id, osWaitForever);
    if (codec != _ctl.runningCodec)
    {
        _update_encode_state(AIV_ENCODE_STATE_RUNNING);
        _update_running_codec(codec);
        /// get AI type
        uint8_t aiIndex = _user2spec[user];       
        /// check if need to block a2dp
        if (app_ai_voice_is_need2block_a2dp())
        {
            /// AI stream block A2DP
            app_ai_voice_block_a2dp();
        }
        /// update stream running state
        app_ai_set_stream_running(true, aiIndex);
        /// reset the encode engine
        app_ai_set_algorithm_engine_reset(true, aiIndex);
        /// start the compression module
        voice_compression_start(aiIndex, codec, _handleFrameSize);
    }
    else
    {
        AI_VOICE_TRACE(0,"will not init encoder, user:%s, codec:%d|%d, state:%s",
              _user_str[user], _ctl.runningCodec, codec, _encode_str[_ctl.encodeState]);
    }
    osMutexRelease(ai_voice_mutex_id);
}

void app_ai_voice_deinit_encoder(uint8_t aiIndex)
{
    osMutexWait(ai_voice_mutex_id, osWaitForever);
    /// enable AI weakup
    ai_set_can_wake_up(true, aiIndex);
    /// stop the compression module
    voice_compression_stop(aiIndex, app_ai_get_encode_type(aiIndex), true);
    /// update encode state to idle
    _update_encode_state(AIV_ENCODE_STATE_IDLE);
    /// update running codec to invalid
    _update_running_codec(0);
    osMutexRelease(ai_voice_mutex_id);
}
#endif

static void _on_upstream_stopped(uint8_t user)
{
    AI_VOICE_TRACE(0,"%s %s", _user_str[user], __func__);

    if (AIV_WORK_MODE_UPSTREAMING == _get_work_mode(user))
    {
        uint8_t spec = _user2spec[user];
        /// enable AI weakup
        ai_set_can_wake_up(true, spec);
#ifdef VOC_ENCODE_ENABLE
        
#ifdef ALEXA_M55_ENABLED    
        ALEXA_ENCODE_INIT_T encodeInit;
        encodeInit.codec = 0;
        encodeInit.user = spec;
        encodeInit.deinit = true;
        app_dsp_m55_mcu_ai_send_msg(DSP_M55_AI_USER_ALEXA, DSP_M55_AI_ALEXA_CMD_DEINIT, &encodeInit, sizeof(encodeInit));
#else
        app_ai_voice_deinit_encoder(spec);
#endif
#endif
        app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, AIV_DEFAULT_FREQ_VALUE);
        /// inform AI voice that stream monopolize is terminated
        uint32_t readOffset = _get_read_offset();
        app_ai_voice_request_share_stream(_ctl.monopolizer, readOffset);
        /// check if the A2DP stream is blocked, and resume it if so
        if (_ctl.a2dpBlocked)
        {
            app_ai_voice_resume_blocked_a2dp();
        }

        app_ai_voice_resume_sleep(spec);
    }
    else
    {
        AI_VOICE_TRACE(0,"upstream not on-going, skip");
    }
}

bool app_ai_voice_is_need2block_a2dp(void)
{
    bool block = false;

    if (_ctl.a2dpBlocked)
    {
        block = true;
    }
    else
    {
#ifdef VOC_ENCODE_OPUS
        AI_connect_info *ai_info = app_ai_get_connect_info(ai_manager_get_foreground_ai_conidx());
        if(NULL == ai_info) {
            return block;
        }

        uint8_t ai_index = ai_info->ai_spec;

        if (app_ai_is_opus_in_overlay(ai_index) &&
            (VOC_ENCODE_OPUS == app_ai_get_encode_type(ai_index)) &&
            (AIV_WORK_MODE_UPSTREAMING == _get_work_mode(app_ai_voice_get_user_from_spec(ai_index))))
        {
            block = true;
        }
        else
        {
            AI_VOICE_TRACE(0,"No need to block a2dp, codec:%d, compression_state:%d, opus_in_overlay:%d",
                  voice_compression_get_codec(),
                  voice_compression_get_state(),
                  app_ai_is_opus_in_overlay(ai_index));
        }
#endif
    }

    return block;
}

void app_ai_voice_block_a2dp(void)
{
    if (_ctl.a2dpBlocked)
    {
        AI_VOICE_TRACE(0,"A2DP already blocked by AI stream");
    }
    else
    {
        /// update A2DP block flag
        _ctl.a2dpBlocked = true;
        AI_VOICE_TRACE(0,"A2DP blocked by AI upstream");

#if defined(IBRT)  && !defined(SINGLE_DEVICE_REC)
        if (APP_AI_TWS_MASTER == app_ai_tws_get_local_role())
        {
            /// inform peer to block A2DP
            ai_cmd_instance_t blockA2dpCmd = {
                .cmdcode = AI_CMD_PEER_MIC_OPEN,
                .param_length = 0,
            };
            app_ai_send_cmd_to_peer(&blockA2dpCmd);
        }else {
            // _update_mic_state(AIV_MIC_STATE_OPENED);
        }
#endif

        /// bolck the A2DP stream
        if (bt_media_is_media_active_by_type(BT_STREAM_MUSIC) &&
            (bt_media_get_current_media() & BT_STREAM_MUSIC))
        {
            AI_VOICE_TRACE(0,"Close A2DP stream because of AI voice upstream");
            /// send request to close the A2DP stream
            app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_CLOSE, 0);
            /// clear the A2DP mark in current media
            bt_media_clear_current_media(BT_STREAM_MUSIC);
        }
    }
}

void app_ai_voice_resume_blocked_a2dp(void)
{
    if (_ctl.a2dpBlocked)
    {
        /// update A2DP block flag
        _ctl.a2dpBlocked = false;
        AI_VOICE_TRACE(0,"AI block A2DP flag cleared");

#ifdef IBRT
        if (APP_AI_TWS_MASTER == app_ai_tws_get_local_role())
        {
            /// inform peer to resume A2DP
            ai_cmd_instance_t resumeA2dpCmd = {
                .cmdcode = AI_CMD_PEER_MIC_CLOSE,
                .param_length = 0,
            };
            app_ai_send_cmd_to_peer(&resumeA2dpCmd);
        }else {
            _update_mic_state(AIV_MIC_STATE_CLOSED);
        }
#endif

        /// A2DP stream from phone still on
        if (bt_media_is_media_active_by_type(BT_STREAM_MUSIC))
        {
            AI_VOICE_TRACE(0,"%s resume the A2DP stream", __func__);
            app_audio_sendrequest(APP_BT_STREAM_A2DP_SBC, (uint8_t)APP_BT_SETTING_OPEN, 0);
        }
    }
}

bool app_ai_voice_request_monopolize_stream(uint8_t requestor)
{
    bool ret = true;

    if (_ctl.monopolizeFlag)
    {
        ret = false;
        AI_VOICE_TRACE(0,"Stream already monopolized by [%s]!", _user_str[_ctl.monopolizer]);
    }
    else
    {
        _ctl.monopolizeFlag = true;
        _update_stream_monopolizer(requestor);
        _update_work_mode(requestor, AIV_WORK_MODE_UPSTREAMING);
    }

    return ret;
}

void app_ai_voice_request_share_stream(uint8_t requestor, uint32_t offset)
{
    if (_ctl.monopolizeFlag &&
        (requestor == _ctl.monopolizer))
    {
        /// update state to hotword detecting
        _update_work_mode(requestor, AIV_WORK_MODE_HW_DETECTING);

        AI_VOICE_TRACE(0,"Stream start to share by [%s], offset:%d", _user_str[requestor], offset);
        _ctl.monopolizeFlag = false;
        _update_stream_monopolizer(AIV_USER_INVALID);
#ifdef VOICE_DETECTOR_SENS_EN
        app_sensor_hub_ai_mcu_request_vad_data_stop();
#endif
        /// call the monopolize terminated callback
        for (uint8_t user = AIV_USER_BES; user < AIV_USER_CNT; user++)
        {
            if (_ctl.ai[user].flag.active &&
                _ctl.ai[user].handlers.upstreamStopped)
            {
                _ctl.ai[user].handlers.upstreamStopped(offset);
            }
        }

        uint8_t spec = _user2spec[requestor];
#ifdef VOC_ENCODE_ENABLE
        /// stop the compression module
        voice_compression_stop(spec, app_ai_get_encode_type(spec), false);

        if (!app_ai_is_do_encoding_during_detecting(requestor))
        {
            /// update encode state to idle
            _update_encode_state(AIV_ENCODE_STATE_IDLE);
            /// update running codec to invalid
            _update_running_codec(0);
        }
#endif
        _upstream_deinit(spec);
    }
    else
    {
        AI_VOICE_TRACE(0,"Stream share request denied, current info(flag:%d, requestor:%d)",
              _ctl.monopolizeFlag, _ctl.monopolizer);
    }
}

void app_ai_voice_register_ai_handler(uint8_t user, AIV_HANDLER_BUNDLE_T *cb)
{
    ASSERT((AIV_USER_CNT > user) && (AIV_USER_INVALID < user), "invalid user %d received", user);
    AI_VOICE_TRACE(0,"Register handler bundle for [%s]", _user_str[user]);
    memcpy(&_ctl.ai[user].handlers, cb, sizeof(AIV_HANDLER_BUNDLE_T));
}

// void app_ai_voice_update_upstream_data_read_offset(uint32_t offset)
// {
//     _update_read_offset(offset);
// }

void *app_ai_voice_get_stream_config(void)
{
    return (void *)&_ctl.streamCfg;
}

uint8_t app_ai_voice_get_user_from_spec(uint8_t spec)
{
    uint8_t user = AIV_USER_CNT;

    for (uint8_t i = 0; i < AIV_USER_CNT; i++)
    {
        if (_user2spec[i] == spec)
        {
            user = i;
            break;
        }
    }

    return user;
}

void app_ai_voice_stream_restart(uint8_t user)
{
    if (app_ai_voice_is_mic_open())
    {
        app_ai_voice_stream_control(false, user);
        app_ai_voice_stream_control(true, user);
    }
}

bool app_ai_voice_get_active_flag(uint8_t user)
{
    return _ctl.ai[user].flag.active;
}

static bool _is_any_active(void)
{
    bool ret = false;
    for (uint8_t i = 0; i < AIV_USER_CNT; i++)
    {
        if (app_ai_voice_get_active_flag(i))
        {
            ret = true;
            break;
        }
    }

    return ret;
}

void app_ai_voice_set_active_flag(uint8_t user, bool active)
{
    if (AIV_USER_CNT == user)
    {
        return;
    }

    if (active != _ctl.ai[user].flag.active)
    {
        AI_VOICE_TRACE(0,"%s active flag update:%d->%d", _user_str[user], _ctl.ai[user].flag.active, active);
        _ctl.ai[user].flag.active = active;
    }
    else
    {
        AI_VOICE_TRACE(0,"%s active flag already %d", _user_str[user], active);
    }
}

bool app_ai_voice_get_slave_open_mic_flag(uint8_t user)
{
    return _ctl.ai[user].flag.slaveOpenMic;
}

void app_ai_voice_set_slave_open_mic_flag(uint8_t user, bool slaveOpenMic)
{
    if (slaveOpenMic != _ctl.ai[user].flag.slaveOpenMic)
    {
        AI_VOICE_TRACE(0,"%s slave open mic flag update:%d->%d", _user_str[user], _ctl.ai[user].flag.slaveOpenMic, slaveOpenMic);
        _ctl.ai[user].flag.slaveOpenMic = slaveOpenMic;
    }
    else
    {
        AI_VOICE_TRACE(0,"%s active flag already %d", _user_str[user], slaveOpenMic);
    }
}

static bool _get_super_user_flag(uint8_t user)
{
    return _ctl.ai[user].flag.superUser;
}

static bool _is_super_user_exist(void)
{
    bool isSuperUserExist = false;
    for (uint8_t i = 0; i < AIV_USER_CNT; i++)
    {
        if (_ctl.ai[i].flag.superUser)
        {
            isSuperUserExist = true;
        }
    }

    return isSuperUserExist;
}

void app_ai_voice_set_super_user_flag(uint8_t user, bool isSuperUser)
{
    if (isSuperUser != _ctl.ai[user].flag.superUser)
    {
        AI_VOICE_TRACE(0,"%s super user flag update:%d->%d", _user_str[user], _ctl.ai[user].flag.superUser, isSuperUser);
        _ctl.ai[user].flag.superUser = isSuperUser;
    }
    else
    {
        AI_VOICE_TRACE(0,"%s is super user flag already %d", _user_str[user], isSuperUser);
    }
}

bool app_ai_voice_is_upstreaming(void)
{
    return _ctl.monopolizeFlag;
}

void app_ai_voice_set_buf_read_offset(uint32_t offset)
{
    _update_read_offset(offset);
}

void app_ai_voice_restore_capture_stream_after_super_user_stream_closed(void)
{
    for (uint8_t i = 0; i < AIV_USER_CNT; i++)
    {
        if (_ctl.ai[i].flag.active)
        {
            _start_stream(i);
        }
    }
}

uint32_t app_ai_voice_upstream_read_offset_update(uint32_t pre_roll_len, uint32_t one_frame_len, uint32_t detect_offset)
{
    //need atom handle
    uint32_t encode_read_offset = 0;
    uint32_t out_len = 0;
    app_ai_voice_lock_buf();
    uint32_t pcm_queue_len = LengthOfCQueue(&pcm_queue);
    if (pcm_queue_len >= pre_roll_len)
    {
        out_len = pcm_queue_len - pre_roll_len;
        DeCQueue(&pcm_queue, NULL, out_len);
        if (detect_offset >= pre_roll_len)
        {
            encode_read_offset = detect_offset - pre_roll_len;
        }
        else
        {
            encode_read_offset = detect_offset + PCM_DATA_CACHE_BUF_SIZE - pre_roll_len;
        }
    }
    else
    {
        encode_read_offset = GetCQueueReadOffset(&pcm_queue);
    }
    AI_VOICE_TRACE(0,"pcm queue discard data len:%d Qlen_left:%d", out_len, LengthOfCQueue(&pcm_queue));
    app_ai_voice_release_buf();
    _update_read_offset(encode_read_offset);
    return encode_read_offset;
}

#ifdef IBRT
void app_ai_voice_start_capture_stream_for_ibrt_rs(void)
{
    /*
    bool needStart = false;
    if ((BT_IBRT_MASTER == bts_core_get_ui_role())&&
        should open mic &&
        AIV_MIC_STATE_OPENED != _get_mic_state())
    {
        needStart = true;
    }

    /// open mic if necessary
    if (needStart)
    {
        /// update mic state
        _update_mic_state(AIV_MIC_STATE_OPENING);
        /// put message to AI voice thread to open mic
        AIV_MESSAGE_BLOCK msg = {
            .op = AIV_OP_START_CAPTURE,
            .param0 = user,
        };
        _mailbox_put(&msg);
    }
    */
}

void app_ai_voice_stop_capture_stream_for_ibrt_rs(void)
{

}
#endif
