/***************************************************************************
 *
 * Copyright 2015-2025 BES.
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
const IIR_CFG_T audio_eq_sw_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 5,
    .param = {
        {IIR_TYPE_PEAK, .0,   200,   2},
        {IIR_TYPE_PEAK, .0,   600,  2},
        {IIR_TYPE_PEAK, .0,   2000.0, 2},
        {IIR_TYPE_PEAK, .0,  6000.0, 2},
        {IIR_TYPE_PEAK, .0,  12000.0, 2}
    }
};

const IIR_CFG_T * const audio_eq_sw_iir_cfg_list[EQ_SW_IIR_LIST_NUM]={
    &audio_eq_sw_iir_cfg,
};

const FIR_CFG_T audio_eq_hw_fir_cfg_44p1k = {
    .gain = 0.0f,
    .len = 384,
    .coef =
    {
        (1<<23)-1,
    }
};

const FIR_CFG_T audio_eq_hw_fir_cfg_48k = {
    .gain = 0.0f,
    .len = 384,
    .coef =
    {
        (1<<23)-1,
    }
};


const FIR_CFG_T audio_eq_hw_fir_cfg_96k = {
    .gain = 0.0f,
    .len = 384,
    .coef =
    {
        (1<<23)-1,
    }
};

FIR_CFG_T audio_eq_hw_fir_adaptive_eq_cfg = {
    .gain = 0.0f,
    .len = 384,
    .coef =
    {
        (1<<23)-1,
    }
};

const FIR_CFG_T * const audio_eq_hw_fir_cfg_list[EQ_HW_FIR_LIST_NUM]={
    &audio_eq_hw_fir_cfg_44p1k,
    &audio_eq_hw_fir_cfg_48k,
    &audio_eq_hw_fir_cfg_96k,
};

#if !defined(BESUI_TWS_EN) && !defined(BESUI_STEREO_EN)
//hardware dac iir eq
const IIR_CFG_T audio_eq_hw_dac_iir_cfg = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = -3,
    .gain1 = 0,
#endif
    .num = 6,
    .param = {
        {IIR_TYPE_PEAK, -4, 200, 0.9},
        {IIR_TYPE_PEAK, -2, 70, 0.7},
        {IIR_TYPE_PEAK, -4, 800, 0.8},
        {IIR_TYPE_PEAK, -8, 450, 0.6},
        {IIR_TYPE_PEAK, 3, 3500, 0.5},
        {IIR_TYPE_PEAK, -2, 1500, 2.2},
    }
};
#if defined(__SNDP_SLEEP_APP__)
const IIR_CFG_T audio_eq_hw_dac_iir_jazz = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 4,
    .gain1 = 4,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,    -6.0,    60.0,    0.8},    // 明显削减低频嗡声
        {IIR_TYPE_PEAK,     6.0,   120.0,    0.7},    // 显著增强低音温暖感
        {IIR_TYPE_PEAK,    -4.0,   400.0,    1.0},    // 明显减少中频浑浊
        {IIR_TYPE_PEAK,     5.0,  1000.0,    1.2},    // 显著提升人声清晰度
        {IIR_TYPE_PEAK,     8.0,  3000.0,    0.9},    // 大幅突出萨克斯和小号
        {IIR_TYPE_PEAK,     6.0,  6000.0,    0.8},    // 大幅提升细节和临场感
        {IIR_TYPE_PEAK,     4.0, 10000.0,    1.0},    // 显著增强高频空气感
        {IIR_TYPE_PEAK,    -5.0, 15000.0,    1.2},    // 明显控制超高频刺耳感
    }
};
const IIR_CFG_T audio_eq_hw_dac_iir_rock = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 7,
    .gain1 = 7,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,     8.0,    80.0,    0.6},    // 强力低音冲击
        {IIR_TYPE_PEAK,     5.0,   200.0,    0.7},    // 显著增强低音饱满度
        {IIR_TYPE_PEAK,    -4.0,   500.0,    0.9},    // 明显减少浑浊感
        {IIR_TYPE_PEAK,     6.0,  1500.0,    1.0},    // 显著突出人声
        {IIR_TYPE_PEAK,    10.0,  3000.0,    0.7},    // 强力提升吉他冲击力
        {IIR_TYPE_PEAK,     8.0,  5000.0,    0.8},    // 大幅增强鼓点清晰度
        {IIR_TYPE_PEAK,     6.0,  8000.0,    1.0},    // 显著提升镲片亮度
        {IIR_TYPE_PEAK,     3.0, 12000.0,    1.2},    // 明显增加空气感
    }
};
const IIR_CFG_T audio_eq_hw_dac_iir_classic = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 2,
    .gain1 = 2,
#endif
    .num = 8,
     .param = {
        {IIR_TYPE_PEAK,     4.0,    40.0,    1.0},    // 显著增强极低频延展
        {IIR_TYPE_PEAK,     2.0,   100.0,    1.2},    // 明显提升低音自然度
        {IIR_TYPE_PEAK,    -5.0,   300.0,    1.5},    // 大幅削减低频浑浊
        {IIR_TYPE_PEAK,     0.0,  1000.0,    1.8},    // 保持中频平坦
        {IIR_TYPE_PEAK,     4.0,  3000.0,    1.2},    // 显著提升弦乐清晰度
        {IIR_TYPE_PEAK,     6.0,  6000.0,    1.0},    // 大幅增强细节呈现
        {IIR_TYPE_PEAK,     8.0, 10000.0,    0.9},    // 强力提升高频泛音
        {IIR_TYPE_PEAK,     5.0, 16000.0,    1.0},    // 显著增强空气感和空间感
    }
};
const IIR_CFG_T audio_eq_hw_dac_iir_relaxed = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 0,
    .gain1 = 0,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,     5.0,    80.0,    1.2},    // 大幅增强温暖低音
        {IIR_TYPE_PEAK,     3.0,   200.0,    1.5},    // 明显提升柔和低频
        {IIR_TYPE_PEAK,    -8.0,   800.0,    1.0},    // 强力削减刺耳中频
        {IIR_TYPE_PEAK,    -5.0,  2000.0,    1.2},    // 显著软化人声
        {IIR_TYPE_PEAK,   -10.0,  4000.0,    0.8},    // 大幅降低刺激性频率
        {IIR_TYPE_PEAK,    -6.0,  6000.0,    1.0},    // 强力软化高频
        {IIR_TYPE_PEAK,     3.0, 10000.0,    1.5},    // 适度增强空气感
        {IIR_TYPE_PEAK,     0.0, 12000.0,    1.8},    // 保持超高频平缓
    }
};
const IIR_CFG_T audio_eq_hw_dac_iir_custom_mode = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 0,
    .gain1 = 0,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK, 0,   31.0,   0.75},
        {IIR_TYPE_PEAK, 0,   62.0,   0.75},
        {IIR_TYPE_PEAK, 0,   125.0,   0.75},
        {IIR_TYPE_PEAK, 0,   250.0,   0.75},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.75},
        {IIR_TYPE_PEAK, 0,   2000.0,   0.75},
        {IIR_TYPE_PEAK, 0,   4000.0,   0.75},
        {IIR_TYPE_PEAK, 0,   8000.0,   0.75},
    }
};

//anc eq
const IIR_CFG_T audio_eq_anc_hw_dac_iir_jazz = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 4,
    .gain1 = 4,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,    -6.0,    60.0,    0.8},    // 明显削减低频嗡声
        {IIR_TYPE_PEAK,     6.0,   120.0,    0.7},    // 显著增强低音温暖感
        {IIR_TYPE_PEAK,    -4.0,   400.0,    1.0},    // 明显减少中频浑浊
        {IIR_TYPE_PEAK,     5.0,  1000.0,    1.2},    // 显著提升人声清晰度
        {IIR_TYPE_PEAK,     8.0,  3000.0,    0.9},    // 大幅突出萨克斯和小号
        {IIR_TYPE_PEAK,     6.0,  6000.0,    0.8},    // 大幅提升细节和临场感
        {IIR_TYPE_PEAK,     4.0, 10000.0,    1.0},    // 显著增强高频空气感
        {IIR_TYPE_PEAK,    -5.0, 15000.0,    1.2},    // 明显控制超高频刺耳感
    }
};

const IIR_CFG_T audio_eq_anc_hw_dac_iir_rock = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 7,
    .gain1 = 7,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,     8.0,    80.0,    0.6},    // 强力低音冲击
        {IIR_TYPE_PEAK,     5.0,   200.0,    0.7},    // 显著增强低音饱满度
        {IIR_TYPE_PEAK,    -4.0,   500.0,    0.9},    // 明显减少浑浊感
        {IIR_TYPE_PEAK,     6.0,  1500.0,    1.0},    // 显著突出人声
        {IIR_TYPE_PEAK,    10.0,  3000.0,    0.7},    // 强力提升吉他冲击力
        {IIR_TYPE_PEAK,     8.0,  5000.0,    0.8},    // 大幅增强鼓点清晰度
        {IIR_TYPE_PEAK,     6.0,  8000.0,    1.0},    // 显著提升镲片亮度
        {IIR_TYPE_PEAK,     3.0, 12000.0,    1.2},    // 明显增加空气感
    }
};

const IIR_CFG_T audio_eq_anc_hw_dac_iir_classic = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 2,
    .gain1 = 2,
#endif
    .num = 8,
     .param = {
        {IIR_TYPE_PEAK,     4.0,    40.0,    1.0},    // 显著增强极低频延展
        {IIR_TYPE_PEAK,     2.0,   100.0,    1.2},    // 明显提升低音自然度
        {IIR_TYPE_PEAK,    -5.0,   300.0,    1.5},    // 大幅削减低频浑浊
        {IIR_TYPE_PEAK,     0.0,  1000.0,    1.8},    // 保持中频平坦
        {IIR_TYPE_PEAK,     4.0,  3000.0,    1.2},    // 显著提升弦乐清晰度
        {IIR_TYPE_PEAK,     6.0,  6000.0,    1.0},    // 大幅增强细节呈现
        {IIR_TYPE_PEAK,     8.0, 10000.0,    0.9},    // 强力提升高频泛音
        {IIR_TYPE_PEAK,     5.0, 16000.0,    1.0},    // 显著增强空气感和空间感
    }
};
const IIR_CFG_T audio_eq_anc_hw_dac_iir_relaxed = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 0,
    .gain1 = 0,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK,     5.0,    80.0,    1.2},    // 大幅增强温暖低音
        {IIR_TYPE_PEAK,     3.0,   200.0,    1.5},    // 明显提升柔和低频
        {IIR_TYPE_PEAK,    -8.0,   800.0,    1.0},    // 强力削减刺耳中频
        {IIR_TYPE_PEAK,    -5.0,  2000.0,    1.2},    // 显著软化人声
        {IIR_TYPE_PEAK,   -10.0,  4000.0,    0.8},    // 大幅降低刺激性频率
        {IIR_TYPE_PEAK,    -6.0,  6000.0,    1.0},    // 强力软化高频
        {IIR_TYPE_PEAK,     3.0, 10000.0,    1.5},    // 适度增强空气感
        {IIR_TYPE_PEAK,     0.0, 12000.0,    1.8},    // 保持超高频平缓
    }
};
#endif

const IIR_CFG_T audio_eq_anc_hw_dac_iir_cfg = {
#if defined(AUDIO_HEARING_COMPSATN)
    .gain0 = -22,
    .gain1 = -22,
#else
    .gain0 = 0,
    .gain1 = 0,
#endif
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
        {IIR_TYPE_PEAK, 0,   1000.0,   0.7},
    }
};
#endif //#if !defined(BESUI_TWS_EN) && !defined(BESUI_STEREO_EN)

IIR_CFG_T audio_eq_hw_dac_iir_adaptive_eq_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 1,
    .param = {
        {IIR_TYPE_PEAK, 0,    1000,    0.7},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_dac_iir_cfg_list[EQ_HW_DAC_IIR_LIST_NUM]={
    &audio_eq_hw_dac_iir_cfg,  //normal mode
#if defined(__SNDP_SLEEP_APP__)
    &audio_eq_hw_dac_iir_jazz,
    &audio_eq_hw_dac_iir_rock,
    &audio_eq_hw_dac_iir_classic,
    &audio_eq_hw_dac_iir_relaxed,
#endif
    &audio_eq_anc_hw_dac_iir_cfg,
#if defined(__SNDP_SLEEP_APP__)
    &audio_eq_anc_hw_dac_iir_jazz,
    &audio_eq_anc_hw_dac_iir_rock,
    &audio_eq_anc_hw_dac_iir_classic,
    &audio_eq_anc_hw_dac_iir_relaxed,
#endif
#if defined(EQ_SET_CUSTOMER_EN)
    &audio_eq_hw_dac_iir_cfg_1,
    &audio_eq_hw_dac_iir_cfg_2,
    &audio_eq_hw_dac_iir_cfg_3,
    &audio_eq_hw_dac_iir_cfg_4,
    &audio_eq_hw_dac_iir_cfg_5,
#endif
};

//hardware dac iir eq
const IIR_CFG_T audio_eq_hw_adc_iir_adc_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 1,
    .param = {
        {IIR_TYPE_PEAK, 0.0,   1000.0,   0.7},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_adc_iir_cfg_list[EQ_HW_ADC_IIR_LIST_NUM]={
    &audio_eq_hw_adc_iir_adc_cfg,
};

//hardware iir eq
const IIR_CFG_T audio_eq_hw_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 8,
    .param = {
        {IIR_TYPE_PEAK, 0,   100.0,   7},
        {IIR_TYPE_PEAK, 0,   400.0,   7},
        {IIR_TYPE_PEAK, 0,   700.0,   7},
        {IIR_TYPE_PEAK, 0,   1000.0,   7},
        {IIR_TYPE_PEAK, 0,   3000.0,   7},
        {IIR_TYPE_PEAK, 0,   5000.0,   7},
        {IIR_TYPE_PEAK, 0,   7000.0,   7},
        {IIR_TYPE_PEAK, 0,   9000.0,   7},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_iir_cfg_list[EQ_HW_IIR_LIST_NUM]={
    &audio_eq_hw_iir_cfg,
};

const DrcConfig audio_drc_cfg = {
     .knee = 3,
     .filter_type = {1000, -1},
     .band_num = 2,
     .look_ahead_time = 10,
     .band_settings = {
         {0, 0, 2, 3, 3000, 1},
         {0, 0, 2, 3, 3000, 1},
     }
};

const LimiterConfig audio_limiter_cfg = {
    .knee = 2,
    .look_ahead_time = 10,
    .threshold = -1,
    .makeup_gain = 0,
    .ratio = 1000,
    .attack_time = 3,
    .release_time = 3000,
};

const SpectrumFixConfig audio_spectrum_cfg = {
    .freq_num = 9,
    .freq_list = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800},
};

const ReverbConfig audio_reverb_cfg = {
    .bypass = 0,
    .high_pass_f0 = 100,
    .gain = 0,
};

const BassEnhancerConfig audio_bass_cfg =
{
    .low_cut_freq = 100,
    .high_cut_freq = 200,
    .gain0 = 0,
    .gain1 = -40,
};

const DynamicBoostConfig audio_dynamic_boost_cfg = {
    .debug = 1,
    .xover_freq = {200},
    .order = 4,
    .CT = -40,
    .CS = 0.18,
    .WT = -40,
    .WS = 0.3,
    .ET = -60,
    .ES = 0,
    .attack_time        = 0.0001f,
    .release_time       = 0.0001f,
    .makeup_gain        = -6,
    .delay              = 128,
    .tav                = 1.0f,
    .eq_num = 2,
    .boost_eq = {
        {
            .type = IIR_TYPE_PEAK,
            .gain = 10,
            .freq = 33,
            .Q = 0.5,
        },
        {
            .type = IIR_TYPE_PEAK,
            .gain = -1,
            .freq = 240,
            .Q = 1.1,
        },
        {
            .type = IIR_TYPE_PEAK,
            .gain = -1,
            .freq = 1000, // -1 for unused eq
            .Q = 0.7,
        },
        {
            .type = IIR_TYPE_PEAK,
            .gain = -1,
            .freq = 2000, // -1 for unused eq
            .Q = 0.7,
        }
    }
};

const VirtualSurroundConfig audio_virtual_surround_cfg =
{
    .switch_on = false,
    .pre_delay_1 = 115,
    .pre_delay_2 = 215,
    .pre_delay_3 = 215,
    .allpass_len1 = 400,
    .allpass_len2 = 353,
    .allpass_len3 = 215,
    .allpass_len4 = 400,
    .allpass_len5 = 130,
    .allpass_len6 = 400,

    // percent
    .reverb_input_ratio_1 = 100,
    .reverb_input_ratio_2 = 100,

    .k1 = 50,
    .k2 = 40,
    .k3 = 50,
    .k4 = 40,
    .k5 = 20,
    .k6 = 100,

    .center_channel_ratio = 30,
    .surround_channel_ratio = 100,
    .output_ratio = 100,

    .hrtf_input_ratio = 100,
    .hrtf_output_ratio = 100,
    .hrtf_select_index = 0,
};

const BassEnhancerConfig audio_bass_enhancer_cfg =
{
    .switch_on = true,
    .low_cut_freq = 100,  // low_cut_freq
    .high_cut_freq = 700, // high_cut_freq
    .gain0 = 0, // invalid
    .gain1 = 7.95,  // gain(db)
};

const DynamicEqConfig audio_dynamic_eq_cfg = {
    .switch_on = 1,
    .debug = 0,
    .offset = 3.9,
    .gain   = 0.f,
    .eq_num = 6,
    .dyeq_param_cfg = {
        /*
            {
                .dyeq_eq_cfg = {type,   gain,   fc,   q}
                .dyeq_drc_cfg = {threshold,  attack,   release}
            }
        */
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 6.0,   100.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 0.0,   500.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 0.0,   2000.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 0.0,   4000.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 0.0,   6000.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
        {
            .dyeq_eq_cfg = {IIR_BIQUARD_PEAKINGEQ, 0.0,   8000.0,   1.0},
            .dyeq_drc_cfg = {-50.0,   20.0,   200.0},
        },
    }
};

const AdaptiveVolumeConfig audio_adaptive_volume_cfg =
{
    .debug = 0,
    .CT = -50,
    .CS = -0.3,
    .WT = -50,
    .WS = -0.22,
    .ET = -65,
    .ES = 0.0,
    .makeup_gain = 0,
    .attack_time = 0.0001,
    .release_time = 0.0001,
    .smooth_factor = 0.6,
};

/* AUDIO_VOL_CTRL_EQ */
const IIR_CFG_T audio_eq_cfg_vol_0 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_1 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_2 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_3 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_4 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_5 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_6 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_7 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_8 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_9 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_10 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_11 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_12 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_13 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_14 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_15 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T audio_eq_cfg_vol_16 = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 2,
    .param = {
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
        {IIR_TYPE_PEAK, 0,  1000,   0.707},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_cfg_vol_list[VOL_CTRL_EQ_LIST_NUM] = {
    &audio_eq_cfg_vol_0,
    &audio_eq_cfg_vol_1,
    &audio_eq_cfg_vol_2,
    &audio_eq_cfg_vol_3,
    &audio_eq_cfg_vol_4,
    &audio_eq_cfg_vol_5,
    &audio_eq_cfg_vol_6,
    &audio_eq_cfg_vol_7,
    &audio_eq_cfg_vol_8,
    &audio_eq_cfg_vol_9,
    &audio_eq_cfg_vol_10,
    &audio_eq_cfg_vol_11,
    &audio_eq_cfg_vol_12,
    &audio_eq_cfg_vol_13,
    &audio_eq_cfg_vol_14,
    &audio_eq_cfg_vol_15,
    &audio_eq_cfg_vol_16,
};

static const struct_psap_cfg POSSIBLY_UNUSED hw_dac_drc_para_bypass_mode0 = {
    .psap_cfg_l = {
        .psap_total_gain = 512,
        .psap_band_num = 2,
        .psap_band_gain={0x08000000, 0x08000000},
        .psap_iir_coef[0].iir0.coef_b={0x08000000, 0x08000000, 0x08000000, 0x08000000},
        .psap_iir_coef[0].iir0.coef_a={0x08000000, 0x08000000, 0x08000000, 0x08000000},
        .psap_iir_coef[0].iir1.coef_b={0x08000000, 0x08000000, 0x08000000, 0x08000000},
        .psap_iir_coef[0].iir1.coef_a={0x08000000, 0x08000000, 0x08000000, 0x08000000},
        .psap_cpd_cfg[0]={0x5740, 0x0000, 0x4e02, 0x0000, 0x3b85, 0x0000, 0x7cf7, 0x0309, 0x7ff0, 0x0010, 0x7333, 0x0ccd, 0, 0x0000},
        .psap_limiter_cfg={524287, 0x7cf7, 0x0309, 0x7ff0, 0x0010, 96},
        .psap_dehowling_cfg.dehowling_delay=0,
        .psap_dehowling_cfg.dehowling_l.total_gain=0,
        .psap_dehowling_cfg.dehowling_l.iir_bypass_flag=0,
        .psap_dehowling_cfg.dehowling_l.iir_counter=1,
        .psap_dehowling_cfg.dehowling_l.iir_coef[0].coef_b={0x08000000, 0xf08d9c03, 0x077b49d0},
        .psap_dehowling_cfg.dehowling_l.iir_coef[0].coef_a={0x08000000, 0xf08d9c03, 0x077b49d0},
        // AUD_SECTION_STRUCT_VERSION = 3
        // .psap_dehowling_cfg.dehowling_l.limiter_flag=0,
        // .psap_dehowling_cfg.dehowling_l.limiter_reserved=0,
        // .psap_dehowling_cfg.dehowling_l.limiter_th=0.0,
        // .psap_dehowling_cfg.dehowling_l.limiter_att_time=0.0,
        // .psap_dehowling_cfg.dehowling_l.limiter_rls_time=0.0,
        .psap_dehowling_cfg.dehowling_l.dac_gain_offset=0,
        .psap_dehowling_cfg.dehowling_l.adc_gain_offset=0,
        .psap_type = 3,
        .psap_dac_gain_offset=0,
        .psap_adc_gain_offset=-24,
    }
};

const struct_psap_cfg * hw_dac_drc_para_list_15p6k[HW_DAC_DRC_PARA_LIST_NUM] = {
    &hw_dac_drc_para_bypass_mode0,
};

const struct_psap_cfg * hw_dac_drc_para_list_46p8k[HW_DAC_DRC_PARA_LIST_NUM] = {
    &hw_dac_drc_para_bypass_mode0,
};

const struct_psap_cfg * hw_dac_drc_para_list_93p7k[HW_DAC_DRC_PARA_LIST_NUM] = {
    &hw_dac_drc_para_bypass_mode0,
};

const struct_psap_cfg * hw_dac_drc_para_list_187p5k[HW_DAC_DRC_PARA_LIST_NUM] = {
    &hw_dac_drc_para_bypass_mode0,
};
