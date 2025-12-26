/***************************************************************************
 *
 * Copyright 2015-2024 BES.
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
#include "app_anc_assist.h"
#include "app_voice_assist_adaptive_volume.h"
#include "adaptive_volume.h"
#include "anc_assist.h"
#include "app_voice_assist_anc.h"
#include "hal_trace.h"

#ifdef AUDIO_ADAPTIVE_VOLUME
static uint32_t _index;
static const uint8_t* const user_name = (const uint8_t*)"ada_volume";
extern void audio_adaptive_volume_set_gain_db(float gain);
extern AdaptiveVolumeState *get_audio_adaptive_volume_st(void);
static AdaptiveVolumeState *audio_adaptive_volume_st = NULL;
static bool audio_adaptive_volume_is_running = 0;

extern void speech_adaptive_volume_set_gain_db(float gain);
extern AdaptiveVolumeState *get_speech_adaptive_volume_st(void);
static AdaptiveVolumeState *speech_adaptive_volume_st = NULL;

static AdaptiveVolumeState *adaptive_volume_st = NULL;

static int32_t _voice_assist_adaptive_volume_callback(void *buf, uint32_t len, void *other);

static int32_t voice_assist_ada_volume_process(voice_assist_process_frame_data_t *process_frame_data)
{
    static const uint8_t* const ptr_name = (const uint8_t*)"anc";
    AncAssistRes *anc_assist_res = ((voice_anc_msg_t *)voice_assist_get_user_ptr(ptr_name))->res;
    ASSERT(anc_assist_res != NULL, "[%s] anc_assist_res is NULL", __func__);

  
    float res[2];
    res[0] = anc_assist_res->ada_volume_noise_energy;
    res[1] = anc_assist_res->ref_detect_energy;
    // VOICE_ASSIST_TRACE(0, "[%s] ff_mic_noise_engery_db(x10): %d", __func__, (int)res[0] * 10);
    // VOICE_ASSIST_TRACE(0, "[%s] ref_mic_noise_engery_db(x10): %d", __func__, (int)res[1] * 10);
    if (anc_assist_res->ada_volume_noise_status) {
        _voice_assist_adaptive_volume_callback(res, 3, NULL);
    }

    return 0;
}

static int32_t voice_assist_ada_volume_get_fs(void)
{
    return 16000;
}

static int32_t voice_assist_ada_volume_get_mic_map()
{
    uint32_t mic_map = 0;

    mic_map |= ANC_FF_MIC_CH_L | ANC_REF_MIC_CH_L;
#if defined(FREEMAN_ENABLED_STERO)
    mic_map |= ANC_FF_MIC_CH_R;
#endif
    AncAssistConfig *cfg = app_voice_assist_anc_get_cfg();
    cfg->adaptive_volume_en = true;

    return mic_map;
}

static const voice_assist_algo_callback_t ada_volume_assist_algo_dsp = {
    .cap_process    = voice_assist_ada_volume_process,
    .get_fs         = voice_assist_ada_volume_get_fs,
};

static const voice_assist_stream_callback_t voice_assist_ada_volume_stream = {
    .get_mic_ch_map       = voice_assist_ada_volume_get_mic_map,
};

static voice_assist_user_register_cfg voice_assist_ada_volume_stream_cfg = {
    .voice_assist_callback = &voice_assist_ada_volume_stream,
    .assist_algo_dsp = &ada_volume_assist_algo_dsp,
};

int32_t app_voice_assist_adaptive_volume_init(void)
{
    // app_anc_assist_register(_index, _voice_assist_adaptive_volume_callback);
    _index = app_voice_assist_stream_register(user_name, &voice_assist_ada_volume_stream_cfg, NULL);
    app_voice_assist_dsp_register(&_index, user_name , &voice_assist_ada_volume_stream_cfg);

    return 0;
}

int32_t app_voice_assist_adaptive_volume_open(void)
{
    VOICE_ASSIST_TRACE(0, "[%s] adaptive volume start stream", __func__);
    app_voice_assist_open_anc();
    audio_adaptive_volume_st = get_audio_adaptive_volume_st();
    speech_adaptive_volume_st = get_speech_adaptive_volume_st();
    ASSERT(audio_adaptive_volume_st != NULL, "%s warning, audio_process.adaptive_volume is not created", __func__);

    if (audio_adaptive_volume_st != NULL && speech_adaptive_volume_st == NULL) {
        adaptive_volume_st = audio_adaptive_volume_st;
    } else if (audio_adaptive_volume_st == NULL && speech_adaptive_volume_st != NULL) {
        adaptive_volume_st = speech_adaptive_volume_st;
    } else {
        ASSERT(1, "%s warning, adaptive_volume_st is not created", __func__);
    }

    app_anc_assist_open(_index);
    audio_adaptive_volume_is_running = 1;
    return 0;
}

int32_t app_voice_assist_adaptive_volume_close(void)
{
    if(audio_adaptive_volume_is_running == 1){
        VOICE_ASSIST_TRACE(0, "[%s] adaptive volume close stream", __func__);
        audio_adaptive_volume_is_running = 0;
        app_anc_assist_close(_index);
        audio_adaptive_volume_set_gain_db(0.0f);
        speech_adaptive_volume_set_gain_db(0.0f);
        app_voice_assist_close_anc();
    }

    return 0;
}

static float adaptive_gain = 0.f;
static int32_t _voice_assist_adaptive_volume_callback(void *buf, uint32_t len, void *other)
{
    if(audio_adaptive_volume_is_running != 1){
        return 0;
    }

    // ff mic noise engery
    float *res = (float *)buf;
    float ff_mic_noise_engery_db = res[0];
    // VOICE_ASSIST_TRACE(0, "[%s] ff_mic_noise_engery_db(x10): %d", __func__, (int)(ff_mic_noise_engery_db * 10));

    // ref
    POSSIBLY_UNUSED float ref_engery_db = res[1];
    POSSIBLY_UNUSED float *ref = (float *)other;
    POSSIBLY_UNUSED uint32_t ref_frame_len = len;

    ref_engery_db = ref_engery_db - adaptive_gain;
    // VOICE_ASSIST_TRACE(0, "[%s] ref_engery_db(x10): %d", __func__, (int)(ref_engery_db * 10));

    if(adaptive_volume_st != NULL){ 
        float adaptive_volume_gain_db = adaptive_volume_gain_estimate(adaptive_volume_st, ff_mic_noise_engery_db, ref_engery_db);
        if(audio_adaptive_volume_st){
            audio_adaptive_volume_set_gain_db(adaptive_volume_gain_db);
        }else if(speech_adaptive_volume_st){
            speech_adaptive_volume_set_gain_db(adaptive_volume_gain_db);
        }
        adaptive_gain = adaptive_volume_gain_db;
    }else{
        VOICE_ASSIST_TRACE(0, "%s warning, audio_process.adaptive_volume is not created",__func__);
    }

    return 0;
}

#endif