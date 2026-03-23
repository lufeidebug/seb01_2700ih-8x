/***************************************************************************
 *
 * Copyright 2015-2019 BES.
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
#ifndef __RES_AUDIO_DATA_CN_H
#define __RES_AUDIO_DATA_CN_H

const uint8_t CN_POWER_ON [] = {
#include "res/cn/SOUND_POWER_ON.txt"
};

const uint8_t CN_POWER_OFF [] = {
0//#include "res/cn/SOUND_POWER_OFF.txt"
};

const uint8_t CN_SOUND_ZERO[] = { 
0//#include "res/cn/SOUND_ZERO.txt"
};

const uint8_t CN_SOUND_ONE[] = { 
0//#include "res/cn/SOUND_ONE.txt"
};

const uint8_t CN_SOUND_TWO[] = { 
0//#include "res/cn/SOUND_TWO.txt"
};

const uint8_t CN_SOUND_THREE[] = { 
0//#include "res/cn/SOUND_THREE.txt"
};

const uint8_t CN_SOUND_FOUR[] = { 
0//#include "res/cn/SOUND_FOUR.txt"
};

const uint8_t CN_SOUND_FIVE[] = { 
0//#include "res/cn/SOUND_FIVE.txt"
};

const uint8_t CN_SOUND_SIX[] = { 
0//#include "res/cn/SOUND_SIX.txt"
};

const uint8_t CN_SOUND_SEVEN[] = { 
0//#include "res/cn/SOUND_SEVEN.txt"
};

const uint8_t CN_SOUND_EIGHT[] = { 
0//#include "res/cn/SOUND_EIGHT.txt"
};

const uint8_t CN_SOUND_NINE[] = { 
0//#include "res/cn/SOUND_NINE.txt"
};

const uint8_t CN_BT_PAIR_ENABLE[] = {
0//#include "res/cn/SOUND_PAIR_ENABLE.txt"
};

const uint8_t CN_BT_PAIRING [] = {
0//#include "res/cn/SOUND_PAIRING.txt"
};

const uint8_t CN_BT_PAIRING_FAIL[] = {
0//#include "res/cn/SOUND_PAIRING_FAIL.txt"
};

const uint8_t CN_BT_PAIRING_SUCCESS[] = { 
0//#include "res/cn/SOUND_PAIRING_SUCCESS.txt"
};

const uint8_t CN_BT_REFUSE[] = { 
0//#include "res/cn/SOUND_REFUSE.txt"
};

const uint8_t CN_BT_OVER[] = { 
0//#include "res/cn/SOUND_OVER.txt"
};

const uint8_t CN_BT_ANSWER[] = { 
0//#include "res/cn/SOUND_ANSWER.txt"
};


const uint8_t CN_BT_HUNG_UP[] = { 
0//#include "res/cn/SOUND_HUNG_UP.txt"
};

const uint8_t CN_BT_INCOMING_CALL [] = {
0//#include "res/cn/SOUND_INCOMING_CALL.txt"
};

const uint8_t CN_BT_CONNECTED [] = {
0//#include "res/cn/SOUND_CONNECTED.txt"    
};

const uint8_t CN_BT_DIS_CONNECT [] = {
0//#include "res/cn/SOUND_DIS_CONNECT.txt"
};

const uint8_t CN_CHARGE_PLEASE[] = {
0//#include "res/cn/SOUND_CHARGE_PLEASE.txt"
};

const uint8_t CN_CHARGE_FINISH[] = {
0//#include "res/cn/SOUND_CHARGE_FINISH.txt"
};

const uint8_t CN_LANGUAGE_SWITCH[] = { 
0//#include "res/cn/SOUND_LANGUAGE_SWITCH.txt"
};

const uint8_t CN_BT_WARNING[] = { 
#include "res/cn/SOUND_WARNING.txt"
};

const uint8_t CN_BT_ALEXA_START[] = { 

};

const uint8_t CN_BT_ALEXA_STOP[] = { 

};

const uint8_t CN_BT_GSOUND_MIC_OPEN[] = {

};

const uint8_t CN_BT_GSOUND_MIC_CLOSE[] = {

};

const uint8_t CN_BT_GSOUND_NC[] = {

};

const uint8_t CN_SOUND_CUSTOM_LEAK_DETECT [] = {
0//#include "res/ld/SOUND_PROMPT_CUSTOM_LEAK.txt"
};

#ifdef __INTERACTION__
const uint8_t CN_BT_FINDME[] = {
0//#include "res/cn/SOUND_FINDME.txt"
};
#endif

/*doesn't have chinese version sound mute */
const uint8_t CN_BT_MUTE[] = {
#include "res/SOUND_MUTE.txt"
};

#if defined(__SNDP_UI__)
const uint8_t CN_SOUND_ANC_ON[] = {
0//#include "res/cn/SOUND_ANC_ON_16K.txt"
};

const uint8_t CN_SOUND_ANC_OFF[] = {
0//#include "res/cn/SOUND_ANC_OFF_16K.txt"
};

const uint8_t CN_SOUND_TRANSPARENT[] = {
0//#include "res/cn/SOUND_TRANSPARENT_16k.txt"
};

const uint8_t CN_SOUND_WORKING_MODE_SLEEP[] = {
0//#include "res/cn/SOUND_WORKING_MODE_SLEEP_16k.txt"
};

const uint8_t CN_SOUND_WORKING_MODE_BT[] = {
0//#include "res/cn/SOUND_WORKING_MODE_BT_16k.txt"
};

#endif

#endif

