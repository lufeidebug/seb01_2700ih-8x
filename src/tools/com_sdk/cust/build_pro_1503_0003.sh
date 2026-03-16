#!/bin/bash

TARGET_LIST="best1503"
CHIPID="best1503"
COMMAND=$1
set -e

BUILD_CUSTOMER_CFG="POWER_ON_ENTER_TWS_PAIRING_ENABLED=1 TRACE_BUF_SIZE=32*1024 \
                    BLE=1 GATT_OVER_BR_EDR=1 CTKD_ENABLE=1 IS_CTKD_OVER_BR_EDR_ENABLED=1 BLE_PRF_HID=1 BLE_HID_ENABLE=1 \
                    BES_OTA=1 OTA_BIN_COMPRESSED=1 APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 \
                    SPP_EQ_TUNING=1 TOTA_v2=1 A2DP_VIRTUAL_SURROUND=1 AUDIO_BASS_ENHANCER=1 \
                    SPEECH_TX_2MIC_NS8=1 SPEECH_TX_1MIC_NS=0 SPEECH_TX_3MIC_NS=0 \
                    FLASH_REMAP=0 OTA_REBOOT_FLASH_REMAP=0 BT_HID_DEVICE=1 BT_SERVICE_ENABLE=1 \
                    ANC_ASSIST_ENABLED=1 ANC_ENABLE=0 ANC_APP=1 AUDIO_ADAPTIVE_VOLUME=1 \
                    SPEECH_RX_COMPEXP=1 SPEECH_RX_AGC=1 FLASH_UNIQUE_ID=1 \
                    SPEECH_TX_2MIC_PREAF=1 SPEECH_TX_DC_FILTER=1 SPEECH_TX_EQ=1 SPEECH_RX_NS2FLOAT=1 SPEECH_RX_EQ=1 \
                    BES_LIB_DIR=lib/bes/best1503/PRO_0003"
 
 

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 -j64 GEN_LIB=1 BES_LIB_DIR=lib/bes/best1503/PRO_0003 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 BES_LIB_DIR=lib/bes/best1503/PRO_0003 -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $BUILD_CUSTOMER_CFG