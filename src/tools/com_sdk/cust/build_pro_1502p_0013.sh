#!/bin/bash

TARGET_LIST="best1502p"
CHIPID="best1502p"
COMMAND=$1

APP_CUSTOMER_CFG="IGNORE_POWER_ON_KEY_DURING_BOOT_UP=1 IS_AUTOPOWEROFF_ENABLED=0 APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 AUDIO_DEBUG_CMD=0 \
                 TRACE_BUF_SIZE=40*1024 USE_TRACE_ID=0"
OTA_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=0"
BLE_CFG="BLE=1 GATT_OVER_BR_EDR=0"
PAIR_CFG="POWER_ON_OPEN_BOX_ENABLED=1"
HEADSET_CFG="FREEMAN_ENABLED_STERO=1 BT_SVC_FW_PRODUCT=BT_SVC_FW_PRODUCT_HEADSET IBRT=0 IBRT_UI=0"
SPEECH_CFG="SPEECH_TX_DC_FILTER=1 SPEECH_TX_1MIC_PREAF=1 SPEECH_TX_1MIC_NS=1 SPEECH_TX_EQ=1"
ANC_CFG="ANC_ENABLE=1 ANC_ASSIST_ENABLED=0 VOICE_ASSIST_NOISE=0 VOICE_ASSIST_CUSTOM_LEAK_DETECT=0 VOICE_ASSIST_CUSTOM_LEAK_DETECT=0 TOTA_v2=1"
LIB_CFG="BES_LIB_DIR=lib/bes/best1502p/PRO_0013"
################################### end of var define ###################################

build_app_cmd="make T=$TARGET_LIST $APP_CUSTOMER_CFG $BLE_CFG $OTA_CFG $PAIR_CFG $SPEECH_CFG $HEADSET_CFG $ANC_CFG $LIB_CFG -j64"
build_ota_cmd="make T=prod_test/ota_copy CHIP=$CHIPID $ROM_CFG OTA_BIN_COMPRESSED=0 DEBUG=1 $LIB_CFG -j64"
set -e

if [[ "$COMMAND" == "clean" ]];
then
    $build_app_cmd clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    $build_app_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    $build_app_cmd lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    $build_app_cmd ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_cmd ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $build_ota_cmd
echo $build_app_cmd