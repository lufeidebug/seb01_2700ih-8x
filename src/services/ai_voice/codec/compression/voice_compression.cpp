/**
 * @file voice_compression.cpp
 * @author BES AI team
 * @version 0.1
 * @date 2020-12-07
 * 
 * @copyright Copyright (c) 2015-2020 BES Technic.
 * All rights reserved. All unpublished rights reserved.
 * 
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 * 
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 */

/*****************************header include********************************/
#include <string.h>
#include <stdlib.h>
#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "g726.h"
#include "app_utils.h"
#include "app_overlay.h"
#include "voice_sbc.h"
#include "cqueue.h"
#include "voice_compression.h"
#include "app_audio.h"
#include "app_ai_if.h"
#include "app_ibrt_customif_cmd.h"
#include "bts_core_if.h"
#include "app_ai_voice.h"

#if defined(VOC_ENCODE_OPUS)
#include "voice_opus.h"
#endif

#if defined(VOC_ENCODE_SCALABLE)
#include "binaural_codec.h"
#include "app_recording_handle.h"
#endif

/*********************external function declearation************************/

/************************private macro defination***************************/
#define COMPRESSION_USER_DEFAULT        (0) //!< should use an invalid user
#define COMPRESSION_CODEC_DEFAULT       (0) //!< invalid codec type

#ifndef VOB_ENCODED_DATA_STORAGE_BUF_SIZE
#if defined(AI_32KBPS_VOICE) || defined(KNOWLES_UART_DATA) || defined(__ALEXA_WWE)
#define VOB_ENCODED_DATA_STORAGE_BUF_SIZE (8000) // 80byte(20ms) 20*100=2000ms(2s)
#elif defined(DUAL_MIC_RECORDING)
#define VOB_ENCODED_DATA_STORAGE_BUF_SIZE (3840)
#else
#define VOB_ENCODED_DATA_STORAGE_BUF_SIZE (3000) // 40byte(20ms) 20*75=1500ms(1.5s)
#endif
#endif

/// defination of buffer size to cache one frame of encoded/decoded data
/// NOTE: should check if the encoded data exceeded this limit
#if defined(DUAL_MIC_RECORDING)
#define ENCODE_DECODE_FRAME_BUF_SIZE (1024)
#else
#define ENCODE_DECODE_FRAME_BUF_SIZE (512)
#endif

/// check encode buffer
#if (VOB_ENCODED_DATA_STORAGE_BUF_SIZE + ENCODE_DECODE_FRAME_BUF_SIZE > ENCODE_BUF_SIZE)
#error "Encode buffer exceed!"
#endif

/// check encode codec used buffer
#ifdef VOC_ENCODE_OPUS
#if !defined(VAR_ARRAYS) && !defined(USE_ALLOCA) && defined(NONTHREADSAFE_PSEUDOSTACK)
#if (VOICE_OPUS_HEAP_SIZE + VOICE_OPUS_STACK_SIZE > ENCODE_CODEC_BUF_SIZE)
#error "Codec buffer exceed!"
#endif
#else
#if (VOICE_OPUS_HEAP_SIZE > ENCODE_CODEC_BUF_SIZE)
#error "Codec buffer exceed!"
#endif
#endif
#endif

#undef _ENUM_DEF
#define _ENUM_DEF(a, b) #b

/************************private type defination****************************/
typedef struct
{
    /// codec type, @see COMPRESSION_CODEC_E
    uint8_t codec;
    /// compression state, @see COMPRESSION_STATE_E
    uint8_t state;
    /// compression user, @see COMPRESSION_USER_E
    uint8_t user;
    /// pointer of tempory encode data buffer, used to save a frame of encoded data
    uint8_t *encodeFrameBuf;
    /// encoded data queue
    CQueue  encodeDataQueue;
    /// to mark if compression buffer is init done
    bool    bufInitDone;
    /// slice count for one-shot handling
    uint8_t sliceCnt;
    /// compression count
    uint32_t compressionCnt;
} VOICE_COMPRESSION_CTL_T;

/**********************private function declearation************************/
/**
 * @brief Lock the encoded data queue
 * 
 */
static void _LOCK_FIFO(void);

/**
 * @brief Unlock the encoded data queue
 * 
 */
static void _UNLOCK_FIFO(void);

/**
 * @brief Pop queued encoded data from compression data queue
 * 
 * @param buff          Buffer to save the poped data
 * @param len           Length of encoded data to pop out
 */
static void _pop_queued_encoded_data(uint8_t *buff, uint32_t len);

/**
 * @brief Peek queued encoded data from compression data queue
 * 
 * @param buff          Buffer to save the peeked data
 * @param len           Length of encoded data to peek
 */
static void _peek_queued_encoded_data(uint8_t *buff, uint32_t len);

/************************private variable defination************************/
static VOICE_COMPRESSION_CTL_T ctl = {
    .codec              = COMPRESSION_CODEC_DEFAULT,
    .state              = COMPRESSION_STATE_OFF,
    .user               = COMPRESSION_USER_DEFAULT,
    .encodeFrameBuf     = NULL,
    .bufInitDone        = false,
    .sliceCnt           = 1,
    .compressionCnt     = 0,
};

static osMutexId compression_mutex_id = NULL;
osMutexDef(compression_mutex);

static const char* _codec_str[VOC_CODEC_CNT] = {
    VOC_CODEC_LIST,
};

#ifdef VOC_ENCODE_OPUS
static VOICE_OPUS_CONFIG_T opusConfig = {
    VOICE_OPUS_HEAP_SIZE,
    VOICE_OPUS_CHANNEL_COUNT,
    VOICE_OPUS_COMPLEXITY,
    VOICE_OPUS_PACKET_LOSS_PERC,
    VOICE_SIZE_PER_SAMPLE,
    VOICE_OPUS_APP,
    VOICE_OPUS_BANDWIDTH,
    VOICE_OPUS_BITRATE,
    VOICE_OPUS_SAMPLE_RATE,
    VOICE_SIGNAL_TYPE,
    VOICE_FRAME_PERIOD,

    VOICE_OPUS_USE_VBR,
    VOICE_OPUS_CONSTRAINT_USE_VBR,
    VOICE_OPUS_USE_INBANDFEC,
    VOICE_OPUS_USE_DTX,
};

POSSIBLY_UNUSED uint8_t *opus_voice_heap_ptr = NULL, *opus_voice_stackalloc_ptr = NULL;
#endif

#ifdef VOC_ENCODE_SBC
static sbc_stream_info_t sbcConfig =
{
    VOICE_SBC_BIT_POOL          ,
    VOICE_SBC_SAMPLE_RATE       ,
    VOICE_SBC_CHANNEL_MODE      ,
    VOICE_SBC_ALLOC_METHOD      ,
    VOICE_SBC_NUM_BLOCKS        ,
    VOICE_SBC_NUM_SUB_BANDS     ,
    VOICE_SBC_CHANNEL_COUNT     ,
    VOICE_SBC_MSBC_FLAG         ,
    VOICE_SBC_SIZE_PER_SAMPLE   ,
};
#endif

#ifdef VOC_ENCODE_SCALABLE
#define BINAURAL_ENC_HANDLE_LEN (2780)
void *enc = NULL;

/// encode handle, used by library
uint8_t enc_handle[BINAURAL_ENC_HANDLE_LEN];

/*
+-------------------+-----------+-----------+
|                   |   stereo  |   mono    |
+-------------------+-----------+-----------+
|   BITRATE_MODE_0  |   332     |   166     |
+-------------------+-----------+-----------+
|   BITRATE_MODE_1  |   256     |   128     |
+-------------------+-----------+-----------+
|   BITRATE_MODE_2  |   178     |   89      |
+-------------------------------+-----------+
*/
const static int frameLenMap[CHNL_MODE_IDX_NUM][BITRATE_MODE_NUM] = {
    {332, 256, 178},
    {166, 128, 89},
};
#endif

/****************************function defination****************************/
static void _LOCK_FIFO(void)
{
    osMutexWait(compression_mutex_id, osWaitForever);
}

static void _UNLOCK_FIFO(void)
{
    osMutexRelease(compression_mutex_id);
}

static void _pop_queued_encoded_data(uint8_t *buff, uint32_t len)
{
    CQueue *ptrQueue = &(ctl.encodeDataQueue);
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    _LOCK_FIFO();
    AI_VOICE_TRACE(0,"%s dataLen:%d, QLen:%d, read:%d", __func__, len, LengthOfCQueue(ptrQueue), ptrQueue->read);

    if (buff)
    {
        PeekCQueue(ptrQueue, len, &e1, &len1, &e2, &len2);
        if (len == (len1 + len2))
        {
            memcpy(buff, e1, len1);
            memcpy(buff + len1, e2, len2);
            DeCQueue(ptrQueue, 0, len);
        }
        else
        {
            memset(buff, 0x00, len);
        }
    }
    else
    {
        DeCQueue(ptrQueue, 0, len);
    }
    _UNLOCK_FIFO();
}

static void _peek_queued_encoded_data(uint8_t *buff, uint32_t len)
{
    CQueue *ptrQueue = &(ctl.encodeDataQueue);
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    _LOCK_FIFO();
    PeekCQueue(ptrQueue, len, &e1, &len1, &e2, &len2);
    if (len == (len1 + len2))
    {
        memcpy(buff, e1, len1);
        if (e2 != NULL)
        {
            memcpy(buff + len1, e2, len2);
        }
    }
    else
    {
        memset(buff, 0x00, len);
    }
    _UNLOCK_FIFO();
}

static void _update_compression_state(uint8_t state)
{
    AI_VOICE_TRACE(0,"compression state update:%d->%d", ctl.state, state);
    ctl.state = state;
}

uint8_t voice_compression_get_state(void)
{
    return ctl.state;
}

static void _update_compression_codec(uint8_t codec)
{
    AI_VOICE_TRACE(0,"compression codec update:%d->%d", ctl.codec, codec);
    ctl.codec = codec;
}

uint8_t voice_compression_get_codec(void)
{
    return ctl.codec;
}

static void _update_compression_user(uint8_t user)
{
    AI_VOICE_TRACE(0,"compression user update:%d->%d", ctl.user, user);
    ctl.user = user;
}

uint8_t voice_compression_get_user(void)
{
    return ctl.user;
}

void _update_slice_cnt(uint8_t cnt)
{
    AI_VOICE_TRACE(0,"compression slice cnt update:%d->%d", ctl.sliceCnt, cnt);
    ctl.sliceCnt = cnt;
}

#ifdef VOC_ENCODE_SCALABLE
static uint16_t _scalable_frame_len_getter(CQueue *q)
{
    uint8_t data[SCALABLE_HEADER_SIZE];
    PeekCQueueToBuf(q, (CQItemType *)data, SCALABLE_HEADER_SIZE);

    SCALABLE_FRAME_HEADER_T *hdr = (SCALABLE_FRAME_HEADER_T *)data;
    /// check the frame validity
    ASSERT(DEFAULT_SYNC_WORD == hdr->sync, "%s invalid sync word:0x%x", __func__, hdr->sync);
    ASSERT(BITRATE_MODE_NUM > hdr->bitrate, "%s invalid bitrate:%d", __func__, hdr->bitrate);
    /// get frame length
    uint16_t length = RECORD_DATA_PACKET_GATHER_LIMIT * voice_compression_get_scalable_frame_length(hdr->bitrate);

    return length;
}
#endif

#ifdef ALEXA_M55_ENABLED
static bool encodeDataQueueFlag;
void mcu_init_encode_data_queue(void)
{
    uint8_t *buf = NULL;

    if (!encodeDataQueueFlag)
    {
        encodeDataQueueFlag = true;
        AI_VOICE_TRACE(0,"[ALEXA] mcu compression buffer init done!");

        /// encoded data buffer
        app_ai_if_mempool_get_buff(&buf,
                                   VOB_ENCODED_DATA_STORAGE_BUF_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_STORAGE); //!< used to save encoded data

        _LOCK_FIFO();
        InitCQueue(&ctl.encodeDataQueue,
                   VOB_ENCODED_DATA_STORAGE_BUF_SIZE,
                   (CQItemType *)buf); //!< used to manage the encoded data
        _UNLOCK_FIFO();

        app_ai_if_mempool_get_buff(&ctl.encodeFrameBuf,
                                   ENCODE_DECODE_FRAME_BUF_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_ONE_FRAME);
     }

}

void encodeFrameBuf_enqueue(uint8_t *dataBuf, uint32_t size)
{
    if (ctl.encodeFrameBuf == NULL)
    {
        ASSERT(0, "ctl.encodeFrameBuf is NULL");
    }

    memcpy(ctl.encodeFrameBuf, dataBuf, size);
    _LOCK_FIFO();
    EnCQueue_AI(&(ctl.encodeDataQueue),
                ctl.encodeFrameBuf,
                size, NULL);
    AI_VOICE_TRACE(0,"encode enqueue %d => %d", size, LengthOfCQueue(&(ctl.encodeDataQueue)));
    _UNLOCK_FIFO();
}

#endif

static void _voice_compression_init(void)
{    
    if (COMPRESSION_STATE_ON == ctl.state)
    {
    }
    else
    {
        AI_VOICE_TRACE(0,"%s", __func__);

        /// init compression used buffer
        uint8_t *buf = NULL;

        /// init buffer
        if (!ctl.bufInitDone)
        {
            ctl.bufInitDone = true;
            AI_VOICE_TRACE(0,"compression buffer init done!");

            /// encoded data buffer
            app_ai_if_mempool_get_buff(&buf,
                                       VOB_ENCODED_DATA_STORAGE_BUF_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_STORAGE); //!< used to save encoded data

            _LOCK_FIFO();
            InitCQueue(&ctl.encodeDataQueue,
                       VOB_ENCODED_DATA_STORAGE_BUF_SIZE,
                       (CQItemType *)buf); //!< used to manage the encoded data
            _UNLOCK_FIFO();

            app_ai_if_mempool_get_buff(&ctl.encodeFrameBuf,
                                       ENCODE_DECODE_FRAME_BUF_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_ONE_FRAME);

#ifdef VOC_ENCODE_OPUS
            if (VOC_ENCODE_OPUS == ctl.codec)
            {
#if !defined(VAR_ARRAYS) && !defined(USE_ALLOCA) && defined(NONTHREADSAFE_PSEUDOSTACK)
                app_ai_if_mempool_get_buff(&opus_voice_heap_ptr,
                                           VOICE_OPUS_HEAP_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_OPUS_HEAP);
                app_ai_if_mempool_get_buff(&opus_voice_stackalloc_ptr,
                                           VOICE_OPUS_STACK_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_OPUS_STACK);
#else
                app_ai_if_mempool_get_buff(&opus_voice_heap_ptr,
                                           VOICE_OPUS_HEAP_SIZE, AI_CAPTURE_MEMPOOL_USER_ENCODE_OPUS_HEAP);
#endif
            }
#endif
        }
        /// rest buffer
        else
        {
#ifdef VOC_ENCODE_OPUS
            if (VOC_ENCODE_OPUS == ctl.codec)
            {
#if !defined(VAR_ARRAYS) && !defined(USE_ALLOCA) && defined(NONTHREADSAFE_PSEUDOSTACK)
                memset(opus_voice_heap_ptr, 0, VOICE_OPUS_HEAP_SIZE);
                memset(opus_voice_stackalloc_ptr, 0, VOICE_OPUS_STACK_SIZE);
#else
                memset(opus_voice_heap_ptr, 0, VOICE_OPUS_HEAP_SIZE);
#endif
            }
#endif /// VOC_ENCODE_OPUS
#ifdef VOC_ENCODE_SCALABLE
            if (VOC_ENCODE_SCALABLE == ctl.codec)
            {
                memset(enc_handle, 0, BINAURAL_ENC_HANDLE_LEN);
            }
#endif
        }

        /// init mutex
        if (compression_mutex_id == NULL)
        {
            compression_mutex_id = osMutexCreate((osMutex(compression_mutex)));
        }

        /// init codec
        do
        {
            if (0)
            {
            }
#ifdef VOC_ENCODE_OPUS
            else if (VOC_ENCODE_OPUS == ctl.codec)
            {
#ifdef OPUS_IN_OVERLAY
                /// make sure other overlay has already unloaded
                if (app_get_current_overlay() == APP_OVERLAY_ID_QTY)
                {
                    app_overlay_select(APP_OVERLAY_OPUS);
                }
                else
                {
                    AI_VOICE_TRACE(0,"Current overlay:%d, skip init", app_get_current_overlay());
                    break;
                }
#endif
#if !defined(VAR_ARRAYS) && !defined(USE_ALLOCA) && defined(NONTHREADSAFE_PSEUDOSTACK)
                voice_opus_init(&opusConfig, opus_voice_heap_ptr, opus_voice_stackalloc_ptr);
#else
                voice_opus_init(&opusConfig, opus_voice_heap_ptr, 0);
#endif
            }
#endif /// VOC_ENCODE_OPUS
#ifdef VOC_ENCODE_SBC
            else if (VOC_ENCODE_SBC == ctl.codec)
            {
                voice_sbc_init(&sbcConfig);
            }
#endif /// VOC_ENCODE_SBC
#ifdef VOC_ENCODE_SCALABLE
            else if (VOC_ENCODE_SCALABLE == ctl.codec)
            {
                enc = (void *)enc_handle;
                if (binaural_encoder_init(enc, CHMODE_MONO))
                {
                    enc = NULL;
                    AI_VOICE_TRACE(0,"can't initialize the Scalable Encoder");
                }
                else
                {
                    AI_VOICE_TRACE(0,"Scalable Encoderinitialize done");
                }
            }
#endif /// VOC_ENCODE_SCALABLE
            else
            {
                ASSERT(0, "%s codec %d not supported", __func__, ctl.codec);
            }

            /// update compression state
            _update_compression_state(COMPRESSION_STATE_ON);
        } while (0);
    }
}

POSSIBLY_UNUSED static void _voice_compression_deinit(uint8_t codec)
{
    AI_VOICE_TRACE(0,"%s", __func__);
    if (COMPRESSION_STATE_OFF == ctl.state)
    {
        AI_VOICE_TRACE(0,"has deinit just return");
        return;
    }

    voice_compression_reset_encode_buf();
    memset(ctl.encodeFrameBuf, 0, ENCODE_DECODE_FRAME_BUF_SIZE);

    if (0)
    {
    }
#ifdef VOC_ENCODE_OPUS
    else if (VOC_ENCODE_OPUS == codec)
    {
        voice_opus_deinit();

#ifdef OPUS_IN_OVERLAY
        if (app_get_current_overlay() == APP_OVERLAY_OPUS)
        {
            app_overlay_unloadall();
        }
#endif
    }
#endif /// VOC_ENCODE_OPUS
#ifdef VOC_ENCODE_SBC
    else if (VOC_ENCODE_SBC == codec)
    {
        voice_sbc_init(&sbcConfig);
    }
#endif /// VOC_ENCODE_SBC
#ifdef VOC_ENCODE_SCALABLE
    else if (VOC_ENCODE_SCALABLE == codec)
    {
        ctl.compressionCnt = 0;
        AI_VOICE_TRACE(0,"Reset compression cnt");
    }
#endif /// VOC_ENCODE_SCALABLE
    else
    {
        ASSERT(0, "%s codec %d is not supported", __func__, codec);
    }
    _update_compression_state(COMPRESSION_STATE_OFF);
}

void voice_compression_start(uint8_t user, uint8_t codec, uint32_t oneShotHandleLen)
{
    if (COMPRESSION_STATE_ON == voice_compression_get_state())
    {
        AI_VOICE_TRACE(0,"Compression working, user:%d, codec:%s",
              voice_compression_get_user(),
              _codec_str[voice_compression_get_codec()]);
        ASSERT((ctl.user == user) && (ctl.codec == codec), "Compression working, pls check");
    }
    else
    {
#ifdef VOC_ENCODE_OPUS
        ASSERT(0 == oneShotHandleLen % VOICE_OPUS_PCM_DATA_SIZE_PER_FRAME, "Illegal process length");
        _update_slice_cnt(oneShotHandleLen / VOICE_OPUS_PCM_DATA_SIZE_PER_FRAME);
        /// acquire system frequency to optimize the performance
        if (VOC_ENCODE_OPUS == codec)
        {
            app_sysfreq_req(APP_SYSFREQ_USER_AI_VOICE, APP_SYSFREQ_208M);
        }
        // AI_VOICE_TRACE(0,"====>sys freq calc : %d  %d %d", hal_sys_timer_calc_cpu_freq(50, 0), codec, VOC_ENCODE_OPUS);
#endif

        ctl.compressionCnt = 0;
        _update_compression_user(user);
        _update_compression_codec(codec);
        _voice_compression_init();
    }
}

void voice_compression_stop(uint8_t user, uint8_t codec, bool forceStop)
{
    if ((ctl.user != user) || (ctl.codec != codec))
    {
        AI_VOICE_TRACE(0,"WARNING: %s user(%d|%d) or codec(%s|%s) not match",
              __func__, ctl.user, user, _codec_str[ctl.codec], _codec_str[codec]);
    }
    else
#ifdef __BIXBY
    if ((AI_SPEC_BIXBY == user) && (!forceStop))
    {
        AI_VOICE_TRACE(0,"will not stop compression when BIXBY is enabled");
        //need reset encode queue when upstream finishes.
        voice_compression_reset_encode_buf();
        memset(ctl.encodeFrameBuf, 0, ENCODE_DECODE_FRAME_BUF_SIZE);
    }
    else
#endif
    {
        _voice_compression_deinit(codec);

        _update_compression_state(COMPRESSION_STATE_OFF);
        _update_compression_user(COMPRESSION_USER_DEFAULT);
        _update_compression_codec(COMPRESSION_CODEC_DEFAULT);
    }
}

void voice_compression_dinit_buffer(void)
{
    if (ctl.bufInitDone)
    {
        ctl.bufInitDone = false;
        AI_VOICE_TRACE(0,"Update buffer init done to false");
    }
    else
    {
        AI_VOICE_TRACE(0,"Compression buffer not init yet");
    }
}

uint32_t voice_compression_handle(uint8_t encode_type, uint8_t *input_buf, uint32_t sampleCount, uint32_t *purchasedBytes, uint8_t isReset)
{
    AI_VOICE_TRACE(0,"%s sampleCnt:%d, iBuf:%p", __func__, sampleCount, input_buf);

    uint32_t outputSize = 0;
    *purchasedBytes = sampleCount * 2;
    POSSIBLY_UNUSED uint8_t needRest = isReset;
    FRAME_LEN_GETTER_T frameLenGetter = NULL;

    /// disable thread swap to avoid encoder being deinitialized
    uint32_t lock = int_lock();

    /// try to init compression module
    _voice_compression_init();

    /// make sure compression
    if (COMPRESSION_STATE_OFF == voice_compression_get_state())
    {
        AI_VOICE_TRACE(0,"Compression already off, skip!");
    }
    else
    {
        if (0)
        {
        }
#ifdef VOC_ENCODE_ADPCM
        else if (VOC_ENCODE_ADPCM == encode_type)
        {
            outputSize = g726_Encode(input_buf,
                                     ((char *)(ctl.encodeFrameBuf)),
                                     sampleCount,
                                     needRest);
            AI_VOICE_TRACE(0,"adpcm outputs %d bytes", outputSize);
        }
#endif /// VOC_ENCODE_ADPCM
#ifdef VOC_ENCODE_OPUS
        else if (VOC_ENCODE_OPUS == encode_type)
        {
            uint32_t slices = 0;
            int32_t generatedDataSize = 0;
            for (slices = 0; slices < ctl.sliceCnt; slices++)
            {
                generatedDataSize = voice_opus_encode(input_buf,
                                                      (ctl.encodeFrameBuf + outputSize),
                                                      sampleCount,
                                                      needRest);
                AI_VOICE_TRACE(0,"generated data size:%d", generatedDataSize);
                ASSERT((ENCODE_DECODE_FRAME_BUF_SIZE >= generatedDataSize) && (generatedDataSize >= 0),
                       "Encoded data len exceed buf limit %d|%d",
                       ENCODE_DECODE_FRAME_BUF_SIZE, generatedDataSize);

                needRest = false;

                input_buf += (uint32_t)sampleCount * 2 / ctl.sliceCnt;
                outputSize += generatedDataSize;
            }
        }
#endif /// VOC_ENCODE_OPUS
#ifdef VOC_ENCODE_SBC
        else if (VOC_ENCODE_SBC == encode_type)
        {
            outputSize += voice_sbc_encode(input_buf,
                                           sampleCount * 2,
                                           purchasedBytes,
                                           ctl.encodeFrameBuf,
                                           ENCODE_DECODE_FRAME_BUF_SIZE,
                                           needRest);
            needRest = false;
            AI_VOICE_TRACE(0,"SBC outputs %d bytes", outputSize);
        }
#endif /// VOC_ENCODE_SBC
#ifdef VOC_ENCODE_SCALABLE
        else if (VOC_ENCODE_SCALABLE == encode_type)
        {
            frameLenGetter = (FRAME_LEN_GETTER_T)_scalable_frame_len_getter;
            if (enc != NULL)
            {
                /// check if need update bitrate
                uint8_t br = app_recording_scalable_update_bitrate_check(ctl.compressionCnt);
                outputSize = binaural_encode(enc, (short *)input_buf, sampleCount, ctl.encodeFrameBuf, br);
                /// update compression count
                if (0xFFFFFFFF == ctl.compressionCnt)
                {
                    ctl.compressionCnt = 0;
                }
                else
                {
                    ctl.compressionCnt++;
                }

                // SCALABLE_FRAME_HEADER_T *header = (SCALABLE_FRAME_HEADER_T *)ctl.encodeFrameBuf;
                // AI_VOICE_TRACE(0,"BINAURAL outputs %d bytes, syncWord:%x, crc:%x, bitrate_mode:%d, index:%d",
                //       outputSize, header->sync, header->crc, header->bitrate, header->index);
            }
        }
#endif /// VOC_ENCODE_SCALABLE
        else
        {
            int_unlock(lock);
            return sampleCount * 2;
        }

        EnCQueue_AI(&(ctl.encodeDataQueue),
                    ctl.encodeFrameBuf,
                    outputSize, frameLenGetter);
        AI_VOICE_TRACE(0,"encode enqueue %d => %d", outputSize, LengthOfCQueue(&(ctl.encodeDataQueue)));
    }
    int_unlock(lock);

    return outputSize;
}

uint8_t voice_compression_validity_check()
{
    return 0;
}

uint32_t voice_compression_get_encode_buf_size()
{
    uint32_t length = 0;

    _LOCK_FIFO();
    length = LengthOfCQueue(&(ctl.encodeDataQueue));
    _UNLOCK_FIFO();

    return length;
}

void voice_compression_reset_encode_buf(void)
{
    AI_VOICE_TRACE(0,"%s", __func__);

    _LOCK_FIFO();
    ResetCQueue(&(ctl.encodeDataQueue));
    _UNLOCK_FIFO();
}

int voice_compression_get_encoded_data(uint8_t *out_buf, uint32_t len)
{
    _pop_queued_encoded_data(out_buf, len);
    return len;
}

int voice_compression_peek_encoded_data(uint8_t *out_buf, uint32_t len)
{
    _peek_queued_encoded_data(out_buf, len);
    return len;
}

void voice_compression_dequeue_encoded_data(uint32_t len)
{
    _LOCK_FIFO();
    DeCQueue(&(ctl.encodeDataQueue), 0, len);
    _UNLOCK_FIFO();
}

uint32_t voice_compression_get_encoded_data_write_offset(void)
{
    uint32_t write_offset = 0;
    _LOCK_FIFO();
    write_offset = GetCQueueWriteOffset(&(ctl.encodeDataQueue));
    _UNLOCK_FIFO();
    return write_offset;
}


uint32_t voice_compression_get_encode_cnt(void)
{
    return ctl.compressionCnt;
}

#ifdef VOC_ENCODE_SCALABLE
int voice_compression_get_scalable_frame_length(uint8_t br)
{
    ASSERT(br < 3, "invalid bitrate:%d", br);
    return frameLenMap[CHNL_MODE_MONO_IDX][br];
}
#endif
