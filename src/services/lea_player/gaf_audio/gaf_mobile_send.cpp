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
#ifdef USB_BLE_AUDIO_HW_TIMER_TRIGGER
#include "cmsis.h"
#include "cmsis_os.h"
#include "cqueue.h"
#include "hal_trace.h"
#include "plat_types.h"
#include "app_audio.h"
#include "gaf_dbg.h"
#include "bt_drv_interface.h"
#include "gaf_media_common.h"
#include "gaf_source_data.h"
#include "hwtimer_list.h"
#include "bt_service_thread.h"

extern void gaf_mobile_usb_feed_data(uint8_t* ptrBuf, uint32_t length);
extern void gaf_mobile_usb_processing_received_data(uint8_t* ptrBuf, uint32_t length);

/************************private macro defination***************************/
#define HW_TIMER_PORCESS_PLAYBACK               (1 << 0)
#define HW_TIMER_PORCESS_CAPTURE                (1 << 1)
#define GAF_PCM_Q_CACHE_SIZE                    (10*1024)

/************************private strcuture defination****************************/
typedef struct
{
    bool     tx_packet_start;
    bool     rx_packet_start;
    CQueue   tx_pcm_queue;
    CQueue   rx_pcm_queue;

    uint8_t stream_flg;
    uint8_t playback_count;
    uint8_t capture_count;

    HWTIMER_ID playback_timer_id;
    HWTIMER_ID capture_timer_id;

    GAF_AUDIO_STREAM_ENV_T* playbackStreamEnv ;
    GAF_AUDIO_STREAM_ENV_T* captureStreamEnv;
} gaf_cource_send_env_t;

/************************private variable defination************************/
static gaf_cource_send_env_t gaf_cource_send_env = {0};

/**********************private function declaration*************************/

/****************************function defination****************************/
static uint32_t gaf_source_capture_get_trigger_offset(void *_pStreamEnv)
{
    GAF_AUDIO_STREAM_ENV_T *pStreamEnv = (GAF_AUDIO_STREAM_ENV_T*)_pStreamEnv;

    uint32_t current_bt_time = bt_syn_ble_bt_time_to_bts(btdrv_syn_get_curr_ticks(), 0);
    uint32_t latest_iso_bt_time = gaf_media_common_get_latest_tx_iso_evt_timestamp(pStreamEnv);

    int32_t gapUs = (int32_t)(latest_iso_bt_time - current_bt_time);

    while (gapUs < 1000)
    {
        latest_iso_bt_time += pStreamEnv->stream_info.captureInfo.isoIntervalUs;
        gapUs = (int32_t)(latest_iso_bt_time - current_bt_time);
    }

    if (gapUs <= (int32_t)(pStreamEnv->stream_info.captureInfo.isoIntervalUs / 2) ||
        gapUs >= (int32_t)(pStreamEnv->stream_info.captureInfo.isoIntervalUs * 1.5))
    {
        return pStreamEnv->stream_info.captureInfo.isoIntervalUs;
    }
    return gapUs;
}

static uint32_t gaf_source_playback_get_trigger_offset(void *_pStreamEnv)
{
    GAF_AUDIO_STREAM_ENV_T *pStreamEnv = (GAF_AUDIO_STREAM_ENV_T*)_pStreamEnv;

    uint32_t current_bt_time = bt_syn_ble_bt_time_to_bts(btdrv_syn_get_curr_ticks(), 0);
    uint32_t latest_iso_bt_time = gaf_media_common_get_latest_rx_iso_evt_timestamp(pStreamEnv);
    int32_t gapUs = (int32_t)(latest_iso_bt_time - current_bt_time);

    while (gapUs < 1000)
    {
        latest_iso_bt_time += pStreamEnv->stream_info.playbackInfo.isoIntervalUs;
        gapUs = (int32_t)(latest_iso_bt_time - current_bt_time);
    }

    if (gapUs <= (int32_t)(pStreamEnv->stream_info.playbackInfo.isoIntervalUs / 2) ||
        gapUs >= (int32_t)(pStreamEnv->stream_info.playbackInfo.isoIntervalUs * 1.5))
    {
        return pStreamEnv->stream_info.playbackInfo.isoIntervalUs;
    }
    return gapUs;
}

static uint32_t gaf_source_calculate_playback_trigger_time(GAF_AUDIO_STREAM_ENV_T* pStreamEnv)
{
    uint32_t anch_time = gaf_media_common_get_latest_rx_iso_evt_timestamp(pStreamEnv);
    uint32_t curr_bt_time = bt_syn_ble_bt_time_to_bts(btdrv_syn_get_curr_ticks(), 0);
    uint32_t trigger_time = anch_time;

    while ((int32_t)(trigger_time - curr_bt_time) < 1000)
    {
        trigger_time += pStreamEnv->stream_info.playbackInfo.isoIntervalUs;
    }

    LOG_I("usb playback iso anch %u cur time %u trigger time %u", anch_time, curr_bt_time, trigger_time);
    return trigger_time;
}

static void gaf_source_playback_hw_timer_handler(void *_pStreamEnv)
{
    gaf_cource_send_env.playback_count++;
    bt_svc_base_thread_send_signal();
}

static void gaf_source_capture_hw_timer_handler(void *_pStreamEnv)
{
    gaf_cource_send_env.capture_count++;
    bt_svc_base_thread_send_signal();
}

void gaf_source_stream_data_tx_read(uint8_t* data, uint32_t len)
{
    if (!gaf_cource_send_env.captureStreamEnv)
    {
        return;
    }

    if (!gaf_cource_send_env.tx_packet_start)
    {
        gaf_cource_send_env.tx_packet_start = true;
        LOG_I("[%s][%d]: read start!", __FUNCTION__, __LINE__);
        return;
    }

    if (CQ_OK != DeCQueue(&gaf_cource_send_env.tx_pcm_queue, data, len))
    {
        LOG_E("[%s][%d]: %d, %d", __FUNCTION__, __LINE__,
              LengthOfCQueue(&gaf_cource_send_env.tx_pcm_queue), len);
    }
}

void gaf_source_stream_data_tx_write(uint8_t* data, uint32_t len, uint8_t bits_byte)
{
    if (!gaf_cource_send_env.captureStreamEnv)
    {
        return;
    }

    if (!gaf_cource_send_env.tx_packet_start)
    {
        return;
    }

    if (bits_byte >= 3)
    {
        int32_t *ptr = (int32_t *)data;
        for (uint32_t i = 0; i < len/4; i++) {
            ptr[i] = ptr[i] >> 8;
        }
    }

    if (CQ_OK != EnCQueue(&gaf_cource_send_env.tx_pcm_queue, data, len))
    {
        LOG_I("[%s][%d]: %d", __FUNCTION__, __LINE__, len);
    }
}

void gaf_source_stream_data_rx_read(uint8_t* data, uint32_t len)
{
    if (!gaf_cource_send_env.playbackStreamEnv)
    {
        return;
    }

    if (!gaf_cource_send_env.rx_packet_start)
    {
        return;
    }

    if (CQ_OK != DeCQueue(&gaf_cource_send_env.rx_pcm_queue, data, len))
    {
        LOG_E("[%s][%d]: %d, %d", __FUNCTION__, __LINE__,
              LengthOfCQueue(&gaf_cource_send_env.rx_pcm_queue), len);
    }
}

void gaf_source_stream_data_rx_write(uint8_t* data, uint32_t len)
{
    if (!gaf_cource_send_env.playbackStreamEnv)
    {
        return;
    }

    if (!gaf_cource_send_env.rx_packet_start)
    {
        gaf_cource_send_env.rx_packet_start = true;
        LOG_I("[%s][%d]: read start!", __FUNCTION__, __LINE__);
        return;
    }

    if (CQ_OK != EnCQueue(&gaf_cource_send_env.rx_pcm_queue, data, len))
    {
        LOG_I("[%s][%d]: %d", __FUNCTION__, __LINE__, len);
    }
}

static void gaf_source_send_data_thread(void)
{
    GAF_AUDIO_STREAM_COMMON_INFO_T* stream=NULL;

    if (gaf_cource_send_env.playback_count)
    {
        stream = &gaf_cource_send_env.playbackStreamEnv->stream_info.playbackInfo;
        if (gaf_cource_send_env.playback_count > 1)
        {
            LOG_W("send data miss: %d", gaf_cource_send_env.playback_count);
        }

        if (gaf_stream_is_playback_stream_iso_created(gaf_cource_send_env.playbackStreamEnv)) {
            uint32_t trigger_offset = gaf_source_playback_get_trigger_offset(gaf_cource_send_env.playbackStreamEnv);
            hwtimer_start(gaf_cource_send_env.playback_timer_id, US_TO_TICKS(trigger_offset));
            gaf_mobile_usb_feed_data(stream->dma_info.dmaBufPtr, stream->dma_info.dmaChunkSize);
            gaf_source_stream_data_rx_write(stream->dma_info.dmaBufPtr, stream->dma_info.dmaChunkSize);
        }
        else
        {
            LOG_I("playback iso stream stopped");
        }

        gaf_cource_send_env.playback_count = 0;
    }

    if (gaf_cource_send_env.capture_count)
    {
        stream = &gaf_cource_send_env.captureStreamEnv->stream_info.captureInfo;

        if (gaf_cource_send_env.capture_count > 1)
        {
            LOG_W("send data miss: %d", gaf_cource_send_env.playback_count);
        }

        if (gaf_stream_is_capture_stream_iso_created(gaf_cource_send_env.captureStreamEnv)) {
            uint32_t trigger_offset = gaf_source_capture_get_trigger_offset(gaf_cource_send_env.captureStreamEnv);
            hwtimer_start(gaf_cource_send_env.capture_timer_id, US_TO_TICKS(trigger_offset));
            gaf_source_stream_data_tx_read(stream->dma_info.dmaBufPtr, stream->dma_info.dmaChunkSize);
            gaf_mobile_usb_processing_received_data(stream->dma_info.dmaBufPtr, stream->dma_info.dmaChunkSize);
        }
        else {
            LOG_I("capture iso stream stopped");
        }

        gaf_cource_send_env.capture_count = 0;
    }
}

void gaf_source_playback_start(void *_pStreamEnv)
{
    uint8_t *pcm_queue_buf = NULL;
    uint32_t trig_tick;
    uint32_t current_bt_time;

    if (NULL == _pStreamEnv)
    {
         LOG_E("%s, pStreamEnv is NULL", __func__);
         return;
    }

    if ((!gaf_cource_send_env.capture_timer_id) &&
        (!gaf_cource_send_env.playback_timer_id))
    {
        bt_svc_base_thread_add_task(gaf_source_send_data_thread);
        bt_svc_base_thread_set_priority(osPriorityHigh);
    }

    if (NULL == gaf_cource_send_env.playback_timer_id)
    {
        gaf_cource_send_env.playback_timer_id =
            hwtimer_alloc(gaf_source_playback_hw_timer_handler, _pStreamEnv);
    }
    gaf_cource_send_env.playbackStreamEnv = (GAF_AUDIO_STREAM_ENV_T*)_pStreamEnv;

    gaf_cource_send_env.rx_packet_start = false;
    app_audio_mempool_get_buff(&pcm_queue_buf, GAF_PCM_Q_CACHE_SIZE);
    InitCQueue(&gaf_cource_send_env.rx_pcm_queue, GAF_PCM_Q_CACHE_SIZE, pcm_queue_buf);

    app_audio_mempool_get_buff(&gaf_cource_send_env.playbackStreamEnv->stream_info.playbackInfo.dma_info.dmaBufPtr,
        gaf_cource_send_env.playbackStreamEnv->stream_info.playbackInfo.dma_info.dmaChunkSize);

    trig_tick = gaf_source_calculate_playback_trigger_time((GAF_AUDIO_STREAM_ENV_T*)_pStreamEnv);
    current_bt_time = bt_syn_ble_bt_time_to_bts(btdrv_syn_get_curr_ticks(), 0);
    LOG_I("hwtimer playback trigger_tick:%u curr_time:%u", trig_tick, current_bt_time);
    hwtimer_update_then_start(gaf_cource_send_env.playback_timer_id,
        gaf_source_playback_hw_timer_handler, _pStreamEnv, US_TO_TICKS(trig_tick - current_bt_time));
}

void gaf_source_capture_start(void *_pStreamEnv, uint32_t trig_tick)
{
    uint8_t *pcm_queue_buf = NULL;

    if (NULL == _pStreamEnv)
    {
         LOG_E("%s please check param", __func__);
         return;
    }

    if ((!gaf_cource_send_env.capture_timer_id) &&
        (!gaf_cource_send_env.playback_timer_id))
    {
        bt_svc_base_thread_add_task(gaf_source_send_data_thread);
        bt_svc_base_thread_set_priority(osPriorityHigh);
    }

    if (NULL == gaf_cource_send_env.capture_timer_id)
    {
        gaf_cource_send_env.capture_timer_id =
            hwtimer_alloc(gaf_source_capture_hw_timer_handler, _pStreamEnv);
    }
    gaf_cource_send_env.captureStreamEnv = (GAF_AUDIO_STREAM_ENV_T*)_pStreamEnv;

    gaf_cource_send_env.tx_packet_start = false;
    app_audio_mempool_get_buff(&pcm_queue_buf, GAF_PCM_Q_CACHE_SIZE);
    InitCQueue(&gaf_cource_send_env.tx_pcm_queue, GAF_PCM_Q_CACHE_SIZE, pcm_queue_buf);

    app_audio_mempool_get_buff(&gaf_cource_send_env.captureStreamEnv->stream_info.captureInfo.dma_info.dmaBufPtr,
        gaf_cource_send_env.captureStreamEnv->stream_info.captureInfo.dma_info.dmaChunkSize);

    uint32_t current_bt_time = bt_syn_ble_bt_time_to_bts(btdrv_syn_get_curr_ticks(), 0);
    LOG_I("hwtimer capture trigger_tick:%u curr_time:%u", trig_tick, current_bt_time);
    hwtimer_update_then_start(gaf_cource_send_env.capture_timer_id,
        gaf_source_capture_hw_timer_handler, _pStreamEnv, US_TO_TICKS(trig_tick - current_bt_time));
}

void gaf_source_stop(uint32_t stream)
{
    if (AUD_STREAM_PLAYBACK == stream)
    {
        if (gaf_cource_send_env.capture_timer_id)
        {
            hwtimer_stop(gaf_cource_send_env.capture_timer_id);
            hwtimer_free(gaf_cource_send_env.capture_timer_id);
            gaf_cource_send_env.capture_timer_id = NULL;
        }
        gaf_cource_send_env.playbackStreamEnv = NULL;
        gaf_cource_send_env.rx_packet_start = false;
    }
    else if (AUD_STREAM_CAPTURE == stream)
    {
        if (gaf_cource_send_env.playback_timer_id)
        {
            hwtimer_stop(gaf_cource_send_env.playback_timer_id);
            hwtimer_free(gaf_cource_send_env.playback_timer_id);
            gaf_cource_send_env.playback_timer_id = NULL;
        }
        gaf_cource_send_env.captureStreamEnv = NULL;
        gaf_cource_send_env.tx_packet_start = false;
    }

    if ((!gaf_cource_send_env.capture_timer_id) &&
        (!gaf_cource_send_env.playback_timer_id))
    {
        bt_svc_base_thread_delete_task(gaf_source_send_data_thread);
        bt_svc_base_thread_set_priority(osPriorityNormal);
    }
}


#endif
