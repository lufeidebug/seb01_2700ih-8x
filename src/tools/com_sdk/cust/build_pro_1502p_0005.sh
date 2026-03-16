#!/bin/bash

TARGET_LIST="best1502p"
CHIPID="best1502p"
COMMAND=$1

TYPE_CFG="FREEMAN_ENABLED_STERO=0"
COM_CFG="DEBUG=1 APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 TRACE_BUF_SIZE=16*1024"
OTA_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=1"
ALGO_CFG="A2DP_VIRTUAL_SURROUND=1 A2DP_VIRTUAL_SURROUND_STEREO=0 HEAD_TRACK_ENABLE=1 AUDIO_BASS_ENHANCER=1 AUDIO_LIMITER=1 APP_DEBUG_TOOL=TOTA AUDIO_EQ_TUNING=1"
ENC_CFG="SPEECH_TX_2MIC_NS8=1"
ADV_CFG="TOTA_v2=1 BLE=1 GFPS_ENABLE=0 SPOT_ENABLE=0"
ANC_CFG="ANC_ENABLE=1 ANC_FF_ENABLED=1 ANC_FB_ENABLED=1 APP_ANC_TEST=1"
LIB_CFG="BES_LIB_DIR=lib/bes/best1502p/PRO_0005"
PAIR_CFG="POWER_ON_ENTER_TWS_PAIRING_ENABLED=1"
set -e

BUILD_CUSTOMER_CFG="-j $TYPE_CFG $COM_CFG $OTA_CFG $ALGO_CFG $ENC_CFG $ADV_CFG $ANC_CFG $PAIR_CFG"

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST -j clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
        make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 DEBUG=0 $LIB_CFG GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
        make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=0 -j64 GEN_LIB=1 $LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }

        make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 GEN_LIB=1 $LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }
        make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 -j64 GEN_LIB=1 $LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG $LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 $LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $BUILD_CUSTOMER_CFG