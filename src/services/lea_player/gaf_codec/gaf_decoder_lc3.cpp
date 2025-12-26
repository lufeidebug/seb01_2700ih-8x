/***************************************************************************
 *
 * Copyright 2015-2023 BES.
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
#include <string.h>
#include "cmsis.h"
#include "cmsis_os.h"
#include "app_utils.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_aud.h"
#include "hal_location.h"
#include "audio_dump.h"
#include "app_overlay.h"
#include "lc3_process.h"
#include "gaf_codec_lc3.h"
#include "gaf_dbg.h"
#include "gaf_codec_common.h"
#include "cc_stream_common.h"
#if defined(GAF_LC3_BES_PLC_ON)
#include "sbcplc.h"
#endif

/************************private macro defination***************************/
#define AOB_CALCULATE_CODEC_MIPS       0
#define LC3_AUDIO_DOWNLOAD_DUMP        0

/************************private type defination****************************/
typedef enum
{
    LC3_DEC_FREQ_SET_CHN_1_MIN = 0,
    LC3_DEC_FREQ_SET_1_8000_16000_10 = LC3_DEC_FREQ_SET_CHN_1_MIN,
    LC3_DEC_FREQ_SET_1_16000_16000_10,
    LC3_DEC_FREQ_SET_1_16000_32000_10,
    LC3_DEC_FREQ_SET_1_32000_32000_10,
    LC3_DEC_FREQ_SET_1_32000_48000_10,
    LC3_DEC_FREQ_SET_1_32000_64000_10,
    LC3_DEC_FREQ_SET_1_44100_88200_10,
    LC3_DEC_FREQ_SET_1_48000_32000_10,
    LC3_DEC_FREQ_SET_1_48000_96000_10,
    LC3_DEC_FREQ_SET_CHN_1_MAX,

    LC3_DEC_FREQ_SET_CHN_2_MIN = LC3_DEC_FREQ_SET_CHN_1_MAX,
    LC3_DEC_FREQ_SET_2_8000_32000_10 = LC3_DEC_FREQ_SET_CHN_2_MIN,
    LC3_DEC_FREQ_SET_2_16000_32000_10,
    LC3_DEC_FREQ_SET_2_16000_64000_10,
    LC3_DEC_FREQ_SET_2_32000_64000_10,
    LC3_DEC_FREQ_SET_2_32000_96000_10,
    LC3_DEC_FREQ_SET_2_32000_128000_10,
    LC3_DEC_FREQ_SET_2_44100_176400_10,
    LC3_DEC_FREQ_SET_2_48000_192000_10,
    LC3_DEC_FREQ_SET_2_96000_192000_5,

    LC3_DEC_FREQ_SET_MAX,
}lc3_decoder_freq_setting_e;

/************************private variable defination************************/
#if defined(GAF_LC3_BES_PLC_ON)
static PLC_State *lc3_plc_state1 = NULL;
static POSSIBLY_UNUSED PLC_State *lc3_plc_state2 = NULL;
static float *lc3_cos_buf = NULL;
static int lc3_packeet_len = 0;
static int lc3_smooth_len = 0;
static int plc_is_16bit = 0;
#endif

const codec_freq_setting_t lc3_dec_m55_freq_table[LC3_DEC_FREQ_SET_MAX] =
{
    /* channels, sample_rate, bit_rate, frame_ms, freq */
    {1,         8000,       16000,      10,     APP_SYSFREQ_26M},
    {1,         16000,      16000,      10,     APP_SYSFREQ_26M},
    {1,         16000,      32000,      10,     APP_SYSFREQ_26M},
    {1,         32000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      48000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      64000,      10,     APP_SYSFREQ_52M},
    {1,         44100,      88200,      10,     APP_SYSFREQ_52M},
    {1,         48000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         48000,      96000,      10,     APP_SYSFREQ_52M},

    {2,         8000,       32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      64000,      10,     APP_SYSFREQ_52M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_52M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_52M},
    {2,         32000,      128000,     10,     APP_SYSFREQ_52M},
    {2,         44100,      176400,     10,     APP_SYSFREQ_52M},
    {2,         48000,      192000,     10,     APP_SYSFREQ_52M},
    {2,         96000,      192000,     5,      APP_SYSFREQ_78M},
};

const codec_freq_setting_t lc3_dec_cp_freq_table[LC3_DEC_FREQ_SET_MAX] =
{
    /* channels, sample_rate, bit_rate, frame_ms, freq */
    {1,         8000,       16000,      10,     APP_SYSFREQ_52M},
    {1,         16000,      16000,      10,     APP_SYSFREQ_52M},
    {1,         16000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      48000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      64000,      10,     APP_SYSFREQ_52M},
    {1,         44100,      88200,      10,     APP_SYSFREQ_52M},
    {1,         48000,      96000,      10,     APP_SYSFREQ_52M},

    {2,         8000,       32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      64000,      10,     APP_SYSFREQ_52M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_78M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_78M},
    {2,         32000,      128000,     10,     APP_SYSFREQ_78M},
    {2,         44100,      176400,     10,     APP_SYSFREQ_78M},
    {2,         48000,      192000,     10,     APP_SYSFREQ_78M},
    {2,         96000,      192000,     5,      APP_SYSFREQ_78M},
};

const codec_freq_setting_t lc3_dec_m33_freq_table[LC3_DEC_FREQ_SET_MAX] =
{
    /* channels, sample_rate, bit_rate, frame_ms, freq */
    {1,         8000,       16000,      10,     APP_SYSFREQ_52M},
    {1,         16000,      16000,      10,     APP_SYSFREQ_52M},
    {1,         16000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      32000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      48000,      10,     APP_SYSFREQ_52M},
    {1,         32000,      64000,      10,     APP_SYSFREQ_52M},
    {1,         44100,      88200,      10,     APP_SYSFREQ_52M},
    {1,         48000,      96000,      10,     APP_SYSFREQ_52M},

    {2,         8000,       32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      32000,      10,     APP_SYSFREQ_52M},
    {2,         16000,      64000,      10,     APP_SYSFREQ_52M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_78M},
    {2,         32000,      64000,      10,     APP_SYSFREQ_78M},
    {2,         32000,      128000,     10,     APP_SYSFREQ_78M},
    {2,         44100,      176400,     10,     APP_SYSFREQ_78M},
    {2,         48000,      192000,     10,     APP_SYSFREQ_78M},
    {2,         96000,      192000,     5,      APP_SYSFREQ_78M},
};

LC3_Dec_Info g_lc3_dec_info[PLAYBACK_INSTANCE_MAX];

#if AOB_CALCULATE_CODEC_MIPS
aob_codec_time_info_t aob_decode_time_info = {0};
#endif

/********************** function declaration*************************/

static void lc3_free(void *pool, void *data)
{
    return;
}

static uint32_t gaf_audio_lc3_decoder_set_freq(void *_codec_info,
                                    uint32_t base_freq, uint32_t core_type)
{
    CODEC_INFO_T* codec_info = (CODEC_INFO_T*)_codec_info;
    uint32_t lc3_dec_freq = base_freq;
    uint32_t sample_rate = codec_info->sample_rate;
    uint16_t frame_size = codec_info->frame_size * codec_info->num_channels;
    uint32_t bit_rate = (uint32_t)(frame_size * 8000 / codec_info->frame_ms);
    const codec_freq_setting_t *freq_table = NULL;
    uint32_t index = LC3_DEC_FREQ_SET_CHN_1_MIN;
    uint32_t indexMax = LC3_DEC_FREQ_SET_CHN_1_MAX;

    if (44100 == sample_rate){
        bit_rate = bit_rate * 44100 / 48000;
    }

    if (CP_CORE == core_type) {
        freq_table = lc3_dec_cp_freq_table;
    }
    else if (M55_CORE == core_type) {
        freq_table = lc3_dec_m55_freq_table;
    }
    else {
        freq_table = lc3_dec_m33_freq_table;
    }

    if (2 == codec_info->num_channels) {
        index = LC3_DEC_FREQ_SET_CHN_2_MIN;
        indexMax = LC3_DEC_FREQ_SET_MAX;
    }
    for (; index < indexMax; index++)
    {
        if (sample_rate == freq_table[index].sample_rate)
        {
            lc3_dec_freq = freq_table[index].freq;
        }
    }

    LEA_PLAYER_TRACE(0, "lc3 dec freq:%d bit_rate:%d", lc3_dec_freq, bit_rate);
    return lc3_dec_freq;
}

POSSIBLY_UNUSED static uint32_t aob_lc3_get_sysfreq(void)
{
    enum HAL_CMU_FREQ_T cpu_freq = hal_sysfreq_get();

    switch(cpu_freq)
    {
        case HAL_CMU_FREQ_32K:
            return 0;
            break;
        case HAL_CMU_FREQ_26M:
            return 26;
            break;
        case HAL_CMU_FREQ_52M:
            return 52;
            break;
        case HAL_CMU_FREQ_78M:
            return 78;
            break;
        case HAL_CMU_FREQ_104M:
            return 104;
            break;
        case HAL_CMU_FREQ_208M:
            return 208;
            break;
        default:
            return 0;
            break;
    }
}

FRAM_TEXT_LOC
static int aob_stream_lc3_decode_bits(LC3_Dec_Info *p_lc3_dec_info,
        void *input_bytes, int32_t num_bytes, void *output_samples, bool isPlc)
{
    int32_t bfi_ext = 0;
    int32_t err = LC3_API_OK;
    if (isPlc)
    {
        bfi_ext = 1;
    }

    if(num_bytes == 0)
    {
        memset(input_bytes, 0, p_lc3_dec_info->frame_size);
        num_bytes = p_lc3_dec_info->frame_size;
        bfi_ext = 1;
        LEA_PLAYER_TRACE(0, "%s, get empty packet, num_bytes:%d", __func__, num_bytes);
    }

#if AOB_CALCULATE_CODEC_MIPS
    uint32_t cpu_freq = 0;
    POSSIBLY_UNUSED uint32_t stime = 0, etime = 0;
    uint32_t use_time_in_us = 0, total_etime_in_ms = 0;
    if (!aob_decode_time_info.codec_started)
    {
        aob_decode_time_info.codec_started = true;
        aob_decode_time_info.total_time_in_us = 0;
        aob_decode_time_info.start_time_in_ms = hal_sys_timer_get();
    }

    stime = hal_sys_timer_get();
#endif

    err = p_lc3_dec_info->cb_decode_interlaced(p_lc3_dec_info, p_lc3_dec_info->scratch,
            input_bytes, num_bytes, output_samples, bfi_ext);

#if defined(GAF_LC3_BES_PLC_ON)
   if (p_lc3_dec_info->channels == 1)
   {
        if(plc_is_16bit == 1)
        {
            if (bfi_ext)
            {
                a2dp_plc_bad_frame(lc3_plc_state1, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 1, 0);
            }
            else {
                a2dp_plc_good_frame(lc3_plc_state1, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 1, 0);
            }
        }else{
            if (bfi_ext)
            {
                a2dp_plc_bad_frame_24bit(lc3_plc_state1, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 1, 0);
            }
            else {
                a2dp_plc_good_frame_24bit(lc3_plc_state1, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 1, 0);
            }
        }
    }
   else {
        if(plc_is_16bit == 1)
        {
            if (bfi_ext)
            {
                a2dp_plc_bad_frame(lc3_plc_state1, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 0);
                a2dp_plc_bad_frame(lc3_plc_state2, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 1);
            }
            else {
                a2dp_plc_good_frame(lc3_plc_state1, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 0);
                a2dp_plc_good_frame(lc3_plc_state2, (short *)output_samples, (short *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 1);
            }
        }else{
            if (bfi_ext)
            {
                a2dp_plc_bad_frame_24bit(lc3_plc_state1, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 0);
                a2dp_plc_bad_frame_24bit(lc3_plc_state2, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 1);
            }
            else {
                a2dp_plc_good_frame_24bit(lc3_plc_state1, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 0);
                a2dp_plc_good_frame_24bit(lc3_plc_state2, (int32_t *)output_samples, (int32_t *)output_samples, lc3_cos_buf, lc3_packeet_len, 2, 1);
            }
        }
    }
#endif

#if AOB_CALCULATE_CODEC_MIPS
    etime = hal_sys_timer_get();
    use_time_in_us = TICKS_TO_US(etime - stime);
    total_etime_in_ms = TICKS_TO_MS(etime - aob_decode_time_info.start_time_in_ms);
    aob_decode_time_info.total_time_in_us += use_time_in_us;
    cpu_freq = aob_lc3_get_sysfreq();
    if (total_etime_in_ms)
    {
        aob_decode_time_info.codec_mips =
            cpu_freq * (aob_decode_time_info.total_time_in_us / 1000) / total_etime_in_ms;
    }
    LEA_PLAYER_TRACE(0, "err %d ticks:%d time:%d us",
        err, (etime - stime), use_time_in_us);
    LEA_PLAYER_TRACE(0, "freq %d use:%d ms total:%d ms mips: %d M", cpu_freq,
        aob_decode_time_info.total_time_in_us/1000,
        total_etime_in_ms, aob_decode_time_info.codec_mips);
#endif
    return (int)err;
}

static void gaf_audio_lc3_decoder_buf_init(uint8_t instance_handle,
                                        void *_codec_info, void *alloc_cb)
{
    LEA_PLAYER_TRACE(0, "%s instance_handle:%d", __func__, instance_handle);
    g_lc3_dec_info[instance_handle].cb_alloc = (CC_HEAP_ALLOC)alloc_cb;
    g_lc3_dec_info[instance_handle].cb_free = lc3_free;
}

static void gaf_audio_lc3_decoder_init(uint8_t instance_handle, void *_codec_info)
{
    LEA_PLAYER_TRACE(0, "%s instance_handle:%d", __func__, instance_handle);
    CODEC_INFO_T *codec_info = (CODEC_INFO_T*)_codec_info;
    LC3_Dec_Info *p_lc3_dec_info = &g_lc3_dec_info[instance_handle];

    p_lc3_dec_info->sample_rate = codec_info ->sample_rate;
    p_lc3_dec_info->channels   = codec_info ->num_channels;
    p_lc3_dec_info->bitwidth   = codec_info ->bits_depth;
    p_lc3_dec_info->bitalign   = (p_lc3_dec_info->bitwidth == 24) ? 32 : 0;
    p_lc3_dec_info->frame_dms  = (uint8_t)(codec_info ->frame_ms * 10);
    p_lc3_dec_info->frame_size = codec_info ->frame_size * p_lc3_dec_info->channels;
    p_lc3_dec_info->bitrate    = 0;
    p_lc3_dec_info->plcMeth    = LC3_API_PLC_ADVANCED;
    p_lc3_dec_info->epmode     = LC3_API_EP_OFF;
    p_lc3_dec_info->is_interlaced = 1;

    int err = lc3_api_decoder_init(p_lc3_dec_info);
    ASSERT(LC3_API_OK == err, "%s err %d", __func__, err);

    LEA_PLAYER_TRACE(0, "[lc3]");
    LEA_PLAYER_TRACE(0, "Bits:             %d", p_lc3_dec_info->bitwidth);
    LEA_PLAYER_TRACE(0, "Bits align:       %d", p_lc3_dec_info->bitalign);
    LEA_PLAYER_TRACE(0, "Sample rate:      %d", p_lc3_dec_info->sample_rate);
    LEA_PLAYER_TRACE(0, "Channels:         %d", p_lc3_dec_info->channels);
    LEA_PLAYER_TRACE(0, "Frame samples:    %d", p_lc3_dec_info->frame_samples);
    LEA_PLAYER_TRACE(0, "Frame length:     %d", p_lc3_dec_info->frame_size);
    LEA_PLAYER_TRACE(0, "PLC mode:         %d", p_lc3_dec_info->plcMeth);
    LEA_PLAYER_TRACE(0, "Bitrate:          %d", p_lc3_dec_info->bitrate);

#if defined(GAF_LC3_BES_PLC_ON)
        lc3_packeet_len = p_lc3_dec_info->sample_rate * p_lc3_dec_info->frame_dms / 10000 * 2;
        lc3_smooth_len = lc3_packeet_len * 4;
        if(p_lc3_dec_info->bitwidth == 16){
            plc_is_16bit = 1;
        }else{
            plc_is_16bit = 0;
        }
        lc3_plc_state1 = (PLC_State *)p_lc3_dec_info->cb_alloc(0, sizeof(PLC_State));
        a2dp_plc_lc3_init(lc3_plc_state1, A2DP_PLC_CODEC_TYPE_LC3, lc3_packeet_len / 2);
        if(p_lc3_dec_info->channels == 2)
        {
            lc3_plc_state2 = (PLC_State *)p_lc3_dec_info->cb_alloc(0, sizeof(PLC_State));
            a2dp_plc_lc3_init(lc3_plc_state2, A2DP_PLC_CODEC_TYPE_LC3, lc3_packeet_len / 2);
        }
        lc3_cos_buf = (float *)p_lc3_dec_info->cb_alloc(0, lc3_smooth_len * sizeof(float));
        cos_generate(lc3_cos_buf, lc3_smooth_len, lc3_packeet_len);
#endif

#if AOB_CALCULATE_CODEC_MIPS
    memset(&aob_decode_time_info, 0, sizeof(aob_codec_time_info_t));
#endif

    /* lc3 audio dump parameters init */
#if (LC3_AUDIO_DOWNLOAD_DUMP)
    uint32_t num_channels = codec_info->num_channels;
    uint32_t dump_frame_len = (uint32_t)(codec_info->sample_rate * codec_info->frame_ms/1000);
    auto bitalign = (codec_info->bits_depth == 24)? 32 : codec_info->bits_depth;
    audio_dump_init(dump_frame_len, bitalign/8, num_channels);
    LEA_PLAYER_TRACE(0, "dump_frame_len:%d, bitalign:%d", dump_frame_len, bitalign);
#endif

    LEA_PLAYER_TRACE(0, "%s end", __func__);
}

static void gaf_audio_lc3_decoder_deinit(uint8_t instance_handle)
{
    LEA_PLAYER_TRACE(0, "%s", __func__);
}

FRAM_TEXT_LOC
static int gaf_audio_lc3_decode(uint8_t instance_handle, void *codec_info,
            uint32_t inputDataLength, void *input, void *output, bool isPlc)
{
    int ret = LC3_API_ERROR;
    LC3_Dec_Info *p_lc3_dec_info = &g_lc3_dec_info[instance_handle];
    ret = aob_stream_lc3_decode_bits(p_lc3_dec_info, input, inputDataLength, output, isPlc);
 #if LC3_AUDIO_DOWNLOAD_DUMP
    auto frame_samples = p_lc3_dec_info->frame_samples;
    audio_dump_clear_up();
    audio_dump_add_channel_data(0, output, frame_samples);
    audio_dump_run();
#endif

    if (LC3_API_OK == ret) {
        return CODEC_OK;
    }
    return CODEC_ERR;
}

static void gaf_audio_lc3_decoder_buf_deinit(uint8_t instance_handle)
{
    LEA_PLAYER_TRACE(0, "%s instance_handle:%d", __func__, instance_handle);
    LC3_Dec_Info *p_lc3_dec_info = &g_lc3_dec_info[instance_handle];
    if(p_lc3_dec_info->cb_uninit) {
        p_lc3_dec_info->cb_uninit(p_lc3_dec_info);
    }
#if defined(GAF_LC3_BES_PLC_ON)
    p_lc3_dec_info->cb_free(0, lc3_plc_state1);
    lc3_plc_state1 = NULL;
    if(lc3_plc_state2 != NULL)
    {
        p_lc3_dec_info->cb_free(0, lc3_plc_state2);
        lc3_plc_state2 = NULL;
    }
    p_lc3_dec_info->cb_free(0, lc3_cos_buf);
    lc3_cos_buf = NULL;
#endif
    p_lc3_dec_info->instance = NULL;
    p_lc3_dec_info->scratch = NULL;
    p_lc3_dec_info->enabled_frame_buff = NULL;
}

static GAF_DECODER_FUNC_LIST_T gaf_lc3_decoder_func_list =
{
    .decoder_init_buf_func = gaf_audio_lc3_decoder_buf_init,
    .decoder_init_func = gaf_audio_lc3_decoder_init,
    .decoder_deinit_func = gaf_audio_lc3_decoder_deinit,
    .decoder_decode_frame_func = gaf_audio_lc3_decode,
    .decoder_deinit_buf_func = gaf_audio_lc3_decoder_buf_deinit,
    .decoder_set_freq = gaf_audio_lc3_decoder_set_freq,
};

void gaf_audio_lc3_update_decoder_func_list(void *_dec_func_list)
{
    GAF_DECODER_FUNC_LIST_T **dec_func_list = (GAF_DECODER_FUNC_LIST_T **)_dec_func_list;
    *dec_func_list = &gaf_lc3_decoder_func_list;
}
