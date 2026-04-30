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
#if !defined(BESUI_TWS_EN) && !defined(BESUI_STEREO_EN)
#define IIR_COUNTER_FF_L (6)
#define IIR_COUNTER_FF_R (6)
#define IIR_COUNTER_FB_L (5)
#define IIR_COUNTER_FB_R (5)

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode0 = {
    .anc_cfg_ff_l = {
        .total_gain = 768,  // 较高总增益
        .iir_bypass_flag = 0,
        .iir_counter = 8,   // 使用全部8阶IIR
        .iir_coef[0].coef_b = {0x08fa53ed,0xee0df650,0x08f7b661},
        .iir_coef[0].coef_a = {0x08000000,0xf0018f12,0x07fe712d},
        .iir_coef[1].coef_b = {0x080176ef,0xf006a0aa,0x07f7f5a1},
        .iir_coef[1].coef_a = {0x08000000,0xf006a0aa,0x07f96c90},
        .iir_coef[2].coef_b = {0x07ff4a84,0xf00e7f14,0x07f24ddf},
        .iir_coef[2].coef_a = {0x08000000,0xf00e7f14,0x07f19863},
        .iir_coef[3].coef_b = {0x08015e14,0xf006e729,0x07f800b8},
        .iir_coef[3].coef_a = {0x08000000,0xf006e729,0x07f95ecd},
        .iir_coef[4].coef_b = {0x07f84877,0xf027221e,0x07e18bd6},
        .iir_coef[4].coef_a = {0x08000000,0xf027221e,0x07d9d44d},
        .iir_coef[5].coef_b = {0x2c3d768a,0xa895e713,0x2b3597e7},
        .iir_coef[5].coef_a = {0x08000000,0xf0783b62,0x0790ba22},
        .iir_coef[6].coef_b = {0x07a1710a,0xf0f98a51,0x07788993},
        .iir_coef[6].coef_a = {0x08000000,0xf0f98a51,0x0719fa9d},
        .iir_coef[7].coef_b = {0x056721d8,0xf5d562be,0x0517e66f},
        .iir_coef[7].coef_a = {0x08000000,0xf5d562be,0x027f0848},
        .dac_gain_offset = 8,     // 增强DAC增益
        .adc_gain_offset = -12,   // 适当降低ADC增益避免饱和
    },
    .anc_cfg_fb_l = {
        .total_gain = 768,
        .iir_bypass_flag = 0,
        .iir_counter = 6,   // 使用6阶IIR
        .iir_coef[0].coef_b = {0x08a63e28,0xeedf402a,0x08820e96},
        .iir_coef[0].coef_a = {0x08000000,0xf0bc1689,0x074aa42c},
        .iir_coef[1].coef_b = {0x08147903,0xf00272b0,0x07e9166c},
        .iir_coef[1].coef_a = {0x08000000,0xf00272b0,0x07fd8f6e},
        .iir_coef[2].coef_b = {0x06eb6087,0xf30c734f,0x0673b71c},
        .iir_coef[2].coef_a = {0x08000000,0xf30c734f,0x055f17a2},
        .iir_coef[3].coef_b = {0x08066949,0xf010976a,0x07e91cfd},
        .iir_coef[3].coef_a = {0x08000000,0xf010976a,0x07ef8646},
        .iir_coef[4].coef_b = {0x08f97201,0xee1c3d64,0x08ea888a},
        .iir_coef[4].coef_a = {0x08000000,0xf00e4bdb,0x07f1ec14},
        .iir_coef[5].coef_b = {0x07c2a8f3,0xf072b022,0x07a8c8c8},
        .iir_coef[5].coef_a = {0x08000000,0xf072b022,0x076b6b7b},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 8,
        .adc_gain_offset = -12,
    },
    .anc_cfg_tt_l = {
        .total_gain = 128,  // 适度开启通透模式增益
        .iir_bypass_flag = 0,
        .iir_counter = 3,   // 使用3阶IIR
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0x07f8a5c4,0xf00e8b78,0x07f2144c},
        .iir_coef[1].coef_a = {0x08000000,0xf00e8b78,0x07e9f0b0},
        .iir_coef[2].coef_b = {0x07e8c3a2,0xf023872c,0x07c8e9f2},
        .iir_coef[2].coef_a = {0x08000000,0xf023872c,0x07acb8c6},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 4,
        .adc_gain_offset = -8,
    },
    .anc_cfg_mc_l = {
        .total_gain = 256,  // 中等MC增益
        .iir_bypass_flag = 0,
        .iir_counter = 2,   // 使用2阶IIR
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0x07e9c3a1,0xf032a87d,0x07b7db1e},
        .iir_coef[1].coef_a = {0x08000000,0xf032a87d,0x0799e6c2},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 4,
        .adc_gain_offset = 0,
    },
};

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode1 = {
    .anc_cfg_ff_l = {
        .total_gain = 640,
        .iir_bypass_flag = 0,
        .iir_counter = 6,
        .iir_coef[0].coef_b = {0x08fa53ed,0xee0df650,0x08f7b661},
        .iir_coef[0].coef_a = {0x08000000,0xf0018f12,0x07fe712d},
        .iir_coef[1].coef_b = {0x080176ef,0xf006a0aa,0x07f7f5a1},
        .iir_coef[1].coef_a = {0x08000000,0xf006a0aa,0x07f96c90},
        .iir_coef[2].coef_b = {0x07ff4a84,0xf00e7f14,0x07f24ddf},
        .iir_coef[2].coef_a = {0x08000000,0xf00e7f14,0x07f19863},
        .iir_coef[3].coef_b = {0x08015e14,0xf006e729,0x07f800b8},
        .iir_coef[3].coef_a = {0x08000000,0xf006e729,0x07f95ecd},
        .iir_coef[4].coef_b = {0x07f84877,0xf027221e,0x07e18bd6},
        .iir_coef[4].coef_a = {0x08000000,0xf027221e,0x07d9d44d},
        .iir_coef[5].coef_b = {0x2c3d768a,0xa895e713,0x2b3597e7},
        .iir_coef[5].coef_a = {0x08000000,0xf0783b62,0x0790ba22},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 4,
        .adc_gain_offset = -16,
    },
    .anc_cfg_fb_l = {
        .total_gain = 640,
        .iir_bypass_flag = 0,
        .iir_counter = 5,
        .iir_coef[0].coef_b = {0x08a63e28,0xeedf402a,0x08820e96},
        .iir_coef[0].coef_a = {0x08000000,0xf0bc1689,0x074aa42c},
        .iir_coef[1].coef_b = {0x08147903,0xf00272b0,0x07e9166c},
        .iir_coef[1].coef_a = {0x08000000,0xf00272b0,0x07fd8f6e},
        .iir_coef[2].coef_b = {0x06eb6087,0xf30c734f,0x0673b71c},
        .iir_coef[2].coef_a = {0x08000000,0xf30c734f,0x055f17a2},
        .iir_coef[3].coef_b = {0x08066949,0xf010976a,0x07e91cfd},
        .iir_coef[3].coef_a = {0x08000000,0xf010976a,0x07ef8646},
        .iir_coef[4].coef_b = {0x08f97201,0xee1c3d64,0x08ea888a},
        .iir_coef[4].coef_a = {0x08000000,0xf00e4bdb,0x07f1ec14},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 4,
        .adc_gain_offset = -16,
    },
    .anc_cfg_tt_l = {
        .total_gain = 64,
        .iir_bypass_flag = 0,
        .iir_counter = 2,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0x07f8a5c4,0xf00e8b78,0x07f2144c},
        .iir_coef[1].coef_a = {0x08000000,0xf00e8b78,0x07e9f0b0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 2,
        .adc_gain_offset = -12,
    },
    .anc_cfg_mc_l = {
        .total_gain = 128,
        .iir_bypass_flag = 0,
        .iir_counter = 2,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0x07e9c3a1,0xf032a87d,0x07b7db1e},
        .iir_coef[1].coef_a = {0x08000000,0xf032a87d,0x0799e6c2},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 2,
        .adc_gain_offset = 0,
    },
};

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode2 = {
    .anc_cfg_ff_l = {
        .total_gain = 512,
        .iir_bypass_flag = 0,
        .iir_counter = 4,
        .iir_coef[0].coef_b = {0x08fa53ed,0xee0df650,0x08f7b661},
        .iir_coef[0].coef_a = {0x08000000,0xf0018f12,0x07fe712d},
        .iir_coef[1].coef_b = {0x080176ef,0xf006a0aa,0x07f7f5a1},
        .iir_coef[1].coef_a = {0x08000000,0xf006a0aa,0x07f96c90},
        .iir_coef[2].coef_b = {0x07ff4a84,0xf00e7f14,0x07f24ddf},
        .iir_coef[2].coef_a = {0x08000000,0xf00e7f14,0x07f19863},
        .iir_coef[3].coef_b = {0x08015e14,0xf006e729,0x07f800b8},
        .iir_coef[3].coef_a = {0x08000000,0xf006e729,0x07f95ecd},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 2,
        .adc_gain_offset = -20,
    },
    .anc_cfg_fb_l = {
        .total_gain = 512,
        .iir_bypass_flag = 0,
        .iir_counter = 4,
        .iir_coef[0].coef_b = {0x08a63e28,0xeedf402a,0x08820e96},
        .iir_coef[0].coef_a = {0x08000000,0xf0bc1689,0x074aa42c},
        .iir_coef[1].coef_b = {0x08147903,0xf00272b0,0x07e9166c},
        .iir_coef[1].coef_a = {0x08000000,0xf00272b0,0x07fd8f6e},
        .iir_coef[2].coef_b = {0x06eb6087,0xf30c734f,0x0673b71c},
        .iir_coef[2].coef_a = {0x08000000,0xf30c734f,0x055f17a2},
        .iir_coef[3].coef_b = {0x08066949,0xf010976a,0x07e91cfd},
        .iir_coef[3].coef_a = {0x08000000,0xf010976a,0x07ef8646},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 2,
        .adc_gain_offset = -20,
    },
    .anc_cfg_tt_l = {
        .total_gain = 32,
        .iir_bypass_flag = 0,
        .iir_counter = 1,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = -16,
    },
    .anc_cfg_mc_l = {
        .total_gain = 64,
        .iir_bypass_flag = 0,
        .iir_counter = 1,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = 0,
    },
};

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode3 = {
    .anc_cfg_ff_l = {
        .total_gain = 384,
        .iir_bypass_flag = 0,
        .iir_counter = 3,
        .iir_coef[0].coef_b = {0x08fa53ed,0xee0df650,0x08f7b661},
        .iir_coef[0].coef_a = {0x08000000,0xf0018f12,0x07fe712d},
        .iir_coef[1].coef_b = {0x080176ef,0xf006a0aa,0x07f7f5a1},
        .iir_coef[1].coef_a = {0x08000000,0xf006a0aa,0x07f96c90},
        .iir_coef[2].coef_b = {0x07ff4a84,0xf00e7f14,0x07f24ddf},
        .iir_coef[2].coef_a = {0x08000000,0xf00e7f14,0x07f19863},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = -24,
    },
    .anc_cfg_fb_l = {
        .total_gain = 384,
        .iir_bypass_flag = 0,
        .iir_counter = 3,
        .iir_coef[0].coef_b = {0x08a63e28,0xeedf402a,0x08820e96},
        .iir_coef[0].coef_a = {0x08000000,0xf0bc1689,0x074aa42c},
        .iir_coef[1].coef_b = {0x08147903,0xf00272b0,0x07e9166c},
        .iir_coef[1].coef_a = {0x08000000,0xf00272b0,0x07fd8f6e},
        .iir_coef[2].coef_b = {0x06eb6087,0xf30c734f,0x0673b71c},
        .iir_coef[2].coef_a = {0x08000000,0xf30c734f,0x055f17a2},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = -24,
    },
    .anc_cfg_tt_l = {
        .total_gain = 16,
        .iir_bypass_flag = 0,
        .iir_counter = 1,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = -20,
    },
    .anc_cfg_mc_l = {
        .total_gain = 32,
        .iir_bypass_flag = 0,
        .iir_counter = 1,
        .iir_coef[0].coef_b = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[0].coef_a = {0x08000000,0xf022991b,0x07ddf8db},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = 0,
    },
};

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_50p7k_mode4 = {
    .anc_cfg_ff_l = {
        .total_gain = 1536,  // 更高增益，更强环境音增强
        .iir_bypass_flag = 0,
        .iir_counter = 6,    // 使用6阶IIR，优化人声频段
        .iir_coef[0].coef_b = {0x089abcde,0xee012345,0x08987654},
        .iir_coef[0].coef_a = {0x08000000,0xf0187654,0x07f87654},
        .iir_coef[1].coef_b = {0x08abcdef,0xeedcba98,0x08a98765},
        .iir_coef[1].coef_a = {0x08000000,0xf0198765,0x07f98765},
        .iir_coef[2].coef_b = {0x08bcdef0,0xeeabcdef,0x08ba9876},
        .iir_coef[2].coef_a = {0x08000000,0xf01a9876,0x07fa9876},
        .iir_coef[3].coef_b = {0x08cdef01,0xeefedcba,0x08cb9876},
        .iir_coef[3].coef_a = {0x08000000,0xf01b9876,0x07fb9876},
        .iir_coef[4].coef_b = {0x08def012,0xee123456,0x08dcba98},
        .iir_coef[4].coef_a = {0x08000000,0xf01cba98,0x07fcba98},
        .iir_coef[5].coef_b = {0x08ef0123,0xeedcba98,0x08edcba9},
        .iir_coef[5].coef_a = {0x08000000,0xf01dcba9,0x07fdcba9},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 16,    // 显著增强DAC增益
        .adc_gain_offset = 28,    // 显著增强ADC增益
    },
    .anc_cfg_fb_l = {
        .total_gain = 0,
        .iir_bypass_flag = 1,
        .iir_counter = 0,
        .iir_coef[0].coef_b = {0,0,0},
        .iir_coef[0].coef_a = {0,0,0},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = 0,
    },
    .anc_cfg_tt_l = {
        .total_gain = 0,
        .iir_bypass_flag = 1,
        .iir_counter = 0,
        .iir_coef[0].coef_b = {0,0,0},
        .iir_coef[0].coef_a = {0,0,0},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = 0,
    },
    .anc_cfg_mc_l = {
        .total_gain = 0,
        .iir_bypass_flag = 1,
        .iir_counter = 0,
        .iir_coef[0].coef_b = {0,0,0},
        .iir_coef[0].coef_a = {0,0,0},
        .iir_coef[1].coef_b = {0,0,0},
        .iir_coef[1].coef_a = {0,0,0},
        .iir_coef[2].coef_b = {0,0,0},
        .iir_coef[2].coef_a = {0,0,0},
        .iir_coef[3].coef_b = {0,0,0},
        .iir_coef[3].coef_a = {0,0,0},
        .iir_coef[4].coef_b = {0,0,0},
        .iir_coef[4].coef_a = {0,0,0},
        .iir_coef[5].coef_b = {0,0,0},
        .iir_coef[5].coef_a = {0,0,0},
        .iir_coef[6].coef_b = {0,0,0},
        .iir_coef[6].coef_a = {0,0,0},
        .iir_coef[7].coef_b = {0,0,0},
        .iir_coef[7].coef_a = {0,0,0},
        .dac_gain_offset = 0,
        .adc_gain_offset = 0,
    },
};


static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_48k_mode0 = {
    .anc_cfg_ff_l = {
        .total_gain = 512*2,
        .iir_bypass_flag=0,
        .iir_counter=IIR_COUNTER_FF_L,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

/*
        .fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
 */
        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
    .anc_cfg_fb_l = {
        .total_gain = 512/2,
        .iir_bypass_flag=0,
        .iir_counter=IIR_COUNTER_FB_L,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

/*
        .fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
 */
        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
    .anc_cfg_mc_l = {
        .total_gain = 512*2,
        .iir_bypass_flag=0,
        .iir_counter=5,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
};

static const struct_anc_cfg POSSIBLY_UNUSED AncFirCoef_44p1k_mode0 = {
    .anc_cfg_ff_l = {
        .total_gain =512*2,

        .iir_bypass_flag=0,
        .iir_counter=IIR_COUNTER_FF_L,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

/*
        .fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
 */
        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
    .anc_cfg_fb_l = {
        .total_gain = 512/2,
        .iir_bypass_flag=0,
        .iir_counter=IIR_COUNTER_FB_L,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

/*
        .fir_bypass_flag=1,
        .fir_len = AUD_COEF_LEN,
        .fir_coef =
        {
            32767,
        },
 */
        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
    .anc_cfg_mc_l = {
        .total_gain = 512,
        .iir_bypass_flag=0,
        .iir_counter=5,

        .iir_coef[0].coef_b={0x8000000,0,0},
        .iir_coef[0].coef_a={0x8000000,0,0},

        .iir_coef[1].coef_b={0x8000000,0,0},
        .iir_coef[1].coef_a={0x8000000,0,0},

        .iir_coef[2].coef_b={0x8000000,0,0},
        .iir_coef[2].coef_a={0x8000000,0,0},

        .iir_coef[3].coef_b={0x8000000,0,0},
        .iir_coef[3].coef_a={0x8000000,0,0},

        .iir_coef[4].coef_b={0x8000000,0,0},
        .iir_coef[4].coef_a={0x8000000,0,0},

        .iir_coef[5].coef_b={0x8000000,0,0},
        .iir_coef[5].coef_a={0x8000000,0,0},

        .dac_gain_offset=0,
        .adc_gain_offset=(0)*4,
    },
};
#endif //#if !defined(BESUI_TWS_EN) && !defined(BESUI_STEREO_EN)

const struct_anc_cfg * anc_coef_list_50p7k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_50p7k_mode0, //mode1
    &AncFirCoef_50p7k_mode1, //mode2
    &AncFirCoef_50p7k_mode2, //mode3
    &AncFirCoef_50p7k_mode3, //mode4
    &AncFirCoef_50p7k_mode4, //transparent
};

const struct_anc_cfg * anc_coef_list_48k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_48k_mode0,
};

const struct_anc_cfg * anc_coef_list_44p1k[ANC_COEF_LIST_NUM] = {
    &AncFirCoef_44p1k_mode0,
};

static const struct_psap_cfg POSSIBLY_UNUSED PsapFirCoef_50p7k_mode0 = {
    .psap_cfg_l = {
        .psap_total_gain = 723,
        .psap_band_num = 9,
        .psap_band_gain={0, 476222470, 672682118, 534330399, 950188747, 1066129310, 1066129310, 424433723, 0},
        .psap_iir_coef[0].iir0.coef_b={131557370, -265754754, 134217728, 0},
        .psap_iir_coef[0].iir0.coef_a={134217728, -265754754, 131557370, 0},
        .psap_iir_coef[0].iir1.coef_b={-131557370, 397299677, -399959783, 134217728},
        .psap_iir_coef[0].iir1.coef_a={134217728, -399959783, 397299677, -131557370},
        .psap_iir_coef[1].iir0.coef_b={129986488, -264152445, 134217728, 0},
        .psap_iir_coef[1].iir0.coef_a={134217728, -264152445, 129986488, 0},
        .psap_iir_coef[1].iir1.coef_b={-129986488, 394107454, -398337669, 134217728},
        .psap_iir_coef[1].iir1.coef_a={134217728, -398337669, 394107454, -129986488},
        .psap_iir_coef[2].iir0.coef_b={125134130, -259108702, 134217728, 0},
        .psap_iir_coef[2].iir0.coef_a={134217728, -259108702, 125134130, 0},
        .psap_iir_coef[2].iir1.coef_b={-125134129, 384097929, -393171000, 134217728},
        .psap_iir_coef[2].iir1.coef_a={134217728, -393171000, 384097929, -125134129},
        .psap_iir_coef[3].iir0.coef_b={120220377, -253849171, 134217728, 0},
        .psap_iir_coef[3].iir0.coef_a={134217728, -253849171, 120220377, 0},
        .psap_iir_coef[3].iir1.coef_b={-120220366, 373726258, -387683554, 134217728},
        .psap_iir_coef[3].iir1.coef_a={134217728, -387683554, 373726258, -120220366},
        .psap_iir_coef[4].iir0.coef_b={109848832, -242200690, 134217728, 0},
        .psap_iir_coef[4].iir0.coef_a={134217728, -242200690, 109848832, 0},
        .psap_iir_coef[4].iir1.coef_b={-109848632, 351018377, -375156857, 134217728},
        .psap_iir_coef[4].iir1.coef_a={134217728, -375156857, 351018377, -109848632},
        .psap_iir_coef[5].iir0.coef_b={81171724, -205166742, 134217728, 0},
        .psap_iir_coef[5].iir0.coef_a={134217728, -205166742, 81171724, 0},
        .psap_iir_coef[5].iir1.coef_b={-81156770, 281801460, -331721088, 134217728},
        .psap_iir_coef[5].iir1.coef_a={134217728, -331721088, 281801460, -81156770},
        .psap_iir_coef[6].iir0.coef_b={66181540, -181899702, 134217728, 0},
        .psap_iir_coef[6].iir0.coef_a={134217728, -181899702, 66181540, 0},
        .psap_iir_coef[6].iir1.coef_b={-66113394, 241306967, -301500735, 134217728},
        .psap_iir_coef[6].iir1.coef_a={134217728, -301500735, 241306967, -66113394},
        .psap_iir_coef[7].iir0.coef_b={62863906, -176244865, 134217728, 0},
        .psap_iir_coef[7].iir0.coef_a={134217728, -176244865, 62863906, 0},
        .psap_iir_coef[7].iir1.coef_b={-62771443, 231892680, -293809571, 134217728},
        .psap_iir_coef[7].iir1.coef_a={134217728, -293809571, 231892680, -62771443},
        .psap_cpd_cfg[0]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[1]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[2]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[3]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[4]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[5]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[6]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_cpd_cfg[7]={27069, 0, 27069, 0, 10505, -29491, 32689, 79, 32766, 2, 29491, 3277, 0},
        .psap_limiter_cfg={524287, 31991, 777, 32752, 16, 0},
        .psap_dac_gain_offset=0,
        .psap_adc_gain_offset=-12,
    }

};

const struct_psap_cfg * psap_coef_list_50p7k[PSAP_COEF_LIST_NUM] = {
    &PsapFirCoef_50p7k_mode0,
};

const struct_psap_cfg * psap_coef_list_48k[PSAP_COEF_LIST_NUM] = {
    &PsapFirCoef_50p7k_mode0,
};

const struct_psap_cfg * psap_coef_list_44p1k[PSAP_COEF_LIST_NUM] = {
    &PsapFirCoef_50p7k_mode0,
};
