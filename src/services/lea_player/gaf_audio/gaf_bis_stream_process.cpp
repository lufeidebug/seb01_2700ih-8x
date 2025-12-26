/***************************************************************************
 *
 * Copyright 2015-2022 BES.
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
 *
 ****************************************************************************/
#include "gaf_bis_stream_process.h"
// Platform
#include "cmsis.h"
#include "string.h"
#include "hal_trace.h"
#include "audio_dump.h"
// Algo process
#include "audio_process.h"
#include "arm_math_ex.h"
#include "signal_generator.h"

#define FORCE_BIS_PLAYBACK_24BITS

static struct AF_STREAM_CONFIG_T g_playback_stream_cfg;
static bool g_bis_playback_status = false;

static uint8_t *g_playback_stream_buf = NULL;
static uint32_t g_playback_stream_buf_size = 0;

static uint8_t *g_playback_conversion_buf = NULL;
static uint32_t g_playback_conversion_buf_size = 0;

int32_t gaf_bis_stream_process_init(void)
{
    return 0;
}

int32_t gaf_bis_stream_process_deinit(void)
{
    return 0;
}

int32_t gaf_bis_stream_process_ctrl(void)
{
    return 0;
}

extern "C" bool is_bis_media_mode(void)
{
    return g_bis_playback_status;
}

uint32_t gaf_bis_stream_process_need_conversion_buf_size(uint32_t data_size, uint32_t bits_depth)
{
    uint32_t size = 0;
    if (bits_depth == 16) {
#ifdef FORCE_BIS_PLAYBACK_24BITS
        size = data_size * 2;
#else
        size = 0;
#endif
    } else {
        size = 0;
    }
    LEA_PLAYER_TRACE(3, "[%s] size: %d", __func__, size);

    return size;
}

int32_t gaf_bis_stream_process_set_conversion_buf(uint8_t *buf, uint32_t size)
{
    LEA_PLAYER_TRACE(2, "[%s] size: %d", __func__, size);

    g_playback_conversion_buf = buf;
    g_playback_conversion_buf_size = size;

    return 0;
}

/******************** Playback API ********************/
uint32_t gaf_bis_stream_process_need_playback_buf_size(void)
{
    uint32_t size = audio_process_need_audio_buf_size();

    LEA_PLAYER_TRACE(3, "[%s] size: %d", __func__, size);

    return size;
}

int32_t gaf_bis_stream_process_set_playback_buf(uint8_t *buf, uint32_t size)
{
    LEA_PLAYER_TRACE(2, "[%s] size: %d", __func__, size);

    g_playback_stream_buf = buf;
    g_playback_stream_buf_size = size;

    return 0;
}

int32_t gaf_bis_stream_process_playback_open(struct AF_STREAM_CONFIG_T *stream_cfg)
{
    uint32_t frame_len = stream_cfg->data_size / stream_cfg->channel_num / (stream_cfg->bits <= AUD_BITS_16 ? 2 : 4) / 2;
    g_bis_playback_status = true;

    LEA_PLAYER_TRACE(5, "[%s] sample_rate: %d, bits: %d, ch_num: %d, frame_len: %d", __func__,
        stream_cfg->sample_rate,
        stream_cfg->bits,
        stream_cfg->channel_num,
        frame_len);

#ifdef FORCE_BIS_PLAYBACK_24BITS
    if (stream_cfg->bits != AUD_BITS_24) {
        LEA_PLAYER_TRACE(5, "[%s] Stream bits:%d, process bits:%d", __func__, stream_cfg->bits, AUD_BITS_24);
        ASSERT(g_playback_conversion_buf != NULL, "[%s] g_playback_conversion_buf is NULL", __func__);
    }
#endif

    memcpy(&g_playback_stream_cfg, stream_cfg, sizeof(struct AF_STREAM_CONFIG_T));

#ifdef FORCE_BIS_PLAYBACK_24BITS
    audio_process_open(stream_cfg->sample_rate, AUD_BITS_24, stream_cfg->channel_num, stream_cfg->channel_num, frame_len, g_playback_stream_buf, g_playback_stream_buf_size);
#else
    audio_process_open(stream_cfg->sample_rate, stream_cfg->bits, stream_cfg->channel_num, stream_cfg->channel_num, frame_len, g_playback_stream_buf, g_playback_stream_buf_size);
#endif
    void app_le_set_dac_eq(void);
    app_le_set_dac_eq();

    return 0;
}

int32_t gaf_bis_stream_process_playback_close(void)
{
    LEA_PLAYER_TRACE(2, "[%s]", __func__);

    audio_process_close();
    g_bis_playback_status = false;

    return 0;
}

int32_t gaf_bis_stream_process_playback_run(uint8_t *buf, uint32_t len)
{
    POSSIBLY_UNUSED uint32_t pcm_len = len / (g_playback_stream_cfg.bits <= AUD_BITS_16 ? 2 : 4);

#ifdef FORCE_BIS_PLAYBACK_24BITS
    if (g_playback_stream_cfg.bits != AUD_BITS_24) {
        if (g_playback_conversion_buf != NULL) {
            arm_q15_to_q23((int16_t *)buf, (int32_t *)g_playback_conversion_buf, pcm_len);
            audio_process_run(g_playback_conversion_buf, pcm_len * sizeof(uint32_t));
            arm_q23_to_q15((int32_t *)g_playback_conversion_buf, (int16_t *)buf, pcm_len);
        }
    } else {
        audio_process_run(buf, len);
    }
#else
    audio_process_run(buf, len);
#endif
    return 0;
}

