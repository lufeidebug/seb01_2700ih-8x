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


typedef enum {
    SNDP_UI_PAIRING_NONE,
    SNDP_UI_PAIRING_TWS,
    SNDP_UI_PAIRING_FREEMAN,
    
} sndp_ui_pairing_type_e;


void sndp_ui_pairing_type_set(sndp_ui_pairing_type_e type);
bool sndp_ui_pairing_type_is(sndp_ui_pairing_type_e type);


void sndp_ui_volume_set(uint8_t type, uint8_t level);
void sndp_ui_volume_inc(uint8_t type, uint8_t level);
void sndp_ui_volume_dec(uint8_t type, uint8_t level);


void sndp_ui_wear_action(sndp_dev_wear_status_e wear_action, bool remote);

#if defined(__SNDP_KEY_TEST__)
void sndp_ui_key_init(void);
#endif

bool sndp_ui_is_prompt_playing(void);
int sndp_ui_prompt_finish_cb(int aud_id);
int sndp_ui_prompt_start_cb(int aud_id);


void sndp_ui_all_status_sync_recv(uint8_t *data, uint16_t len);

void sndp_ui_timing_to_do(void);
void sndp_ui_init_pre(void);
void sndp_ui_init(void);



#ifdef __cplusplus
	}
#endif

#endif	/* __SNDP_UI__*/
#endif	/* __SNDPUI_H__*/

