#ifndef __SNDPUI_H__
#define __SNDPUI_H__

#if defined(__SNDP_UI__)

#include <stdio.h>
#include "hal_trace.h"
#include "sndp_if_device.h"


#ifdef __cplusplus
extern "C" {
#endif



#define __SNDP_UI_DEBUG__

#if defined(__SNDP_UI_DEBUG__)
#define SPUI_LOG_TAG                "[SNDP_UI]"
#define SPUI_TRACE(num, str, ...)   SNDP_TRACE(num, SPUI_LOG_TAG" %s, " str, __func__, ##__VA_ARGS__)
#define SPUI_TRACE_ENTER()			SNDP_TRACE(0, SPUI_LOG_TAG" %s, %d, ++++\n", __func__, __LINE__)
#define SPUI_TRACE_EXIT()			SNDP_TRACE(0, SPUI_LOG_TAG" %s, %d, ----\n", __func__, __LINE__)
#define SPUI_DUMP					DUMP8
#else
#define SPUI_TRACE(num, str, ...)
#define SPUI_TRACE_ENTER()
#define SPUI_TRACE_EXIT()
#define SPUI_DUMP
#endif




typedef struct {
	uint32_t charging_time;
	uint32_t charging_full_time;

	bool lowpwr_check_enable;
	uint16_t lowpwr_warning_cnt;
	uint32_t lowpwr_warning_last_time;
	uint16_t lowpwr_shutdown_cnt;
    
	uint32_t close_discharge_time;

	bool temperature_check_enable;
	uint16_t temperature_exp_shutdown_time; 

	bool wear_play_music_allowed;
} sndp_ui_ctx_s;



void sndp_ui_pwron_pairing_type_set(uint8_t type);
uint8_t sndp_ui_pwron_pairing_type_get(void);
bool sndp_ui_pwron_pairing_type_is_tws(void);
bool sndp_ui_pwron_pairing_type_is_freeman(void);


void sndp_ui_volume_set(uint8_t type, uint8_t level);
void sndp_ui_volume_inc(uint8_t type, uint8_t level);
void sndp_ui_volume_dec(uint8_t type, uint8_t level);


void sndp_ui_anc_onoff(bool onoff);

void sndp_ui_key_init(void);

bool sndp_ui_is_prompt_playing(void);
int sndp_ui_prompt_finish_cb(int aud_id);
int sndp_ui_prompt_start_cb(int aud_id);

void sndp_ui_bt_enter_mobile_pairing(bool play_tone);


void sndp_ui_timing_to_do(void);
void sndp_ui_init_pre(void);
void sndp_ui_init(void);



#ifdef __cplusplus
	}
#endif

#endif	/* __SNDP_UI__*/
#endif	/* __SNDPUI_H__*/

