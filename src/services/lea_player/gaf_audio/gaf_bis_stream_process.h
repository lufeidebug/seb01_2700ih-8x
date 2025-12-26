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
#ifndef GAF_BIS_STREAM_PROCESS_H_
#define GAF_BIS_STREAM_PROCESS_H_

#include "plat_types.h"
#include "audioflinger.h"
#include "gaf_media_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t gaf_bis_stream_process_init(void);
int32_t gaf_bis_stream_process_deinit(void);

uint32_t gaf_bis_stream_process_need_conversion_buf_size(uint32_t data_size, uint32_t bits_depth);
int32_t gaf_bis_stream_process_set_conversion_buf(uint8_t *buf, uint32_t size);

uint32_t gaf_bis_stream_process_need_playback_buf_size(void);
int32_t gaf_bis_stream_process_set_playback_buf(uint8_t *buf, uint32_t size);
int32_t gaf_bis_stream_process_playback_open(struct AF_STREAM_CONFIG_T *stream_cfg);
int32_t gaf_bis_stream_process_playback_close(void);
int32_t gaf_bis_stream_process_playback_run(uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif // GAF_BIS_STREAM_PROCESS_H_
