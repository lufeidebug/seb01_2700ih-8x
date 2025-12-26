#!/bin/bash

TARGET_LIST="best1306p"
CHIPID="best1306p"
COMMAND=$1
set -e

BUILD_CUSTOMER_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=1 POWER_ON_ENTER_TWS_PAIRING_ENABLED=1 APP_TRACE_RX_ENABLE=0 APP_RX_API_ENABLE=1 \
                    BLE=1 GATT_OVER_BR_EDR=1 SPEECH_TX_2MIC_NS8=0 SPEECH_TX_1MIC_NS=0 SPEECH_TX_3MIC_NS=1 \
                    CAPSENSOR_MEDIAN_FILTERING=1 CAPSENSOR_TRACE_DEBUG=0 CAPSENSOR_SPP_SERVER=1 CAPSENSOR_ENABLE=1 \
                    SPP_EQ_TUNING=1 TOTA_v2=1 SW_IIR_EQ_PROCESS=1 HW_DAC_IIR_EQ_PROCESS=0 A2DP_LDAC_ON=1 \
                    AUDIO_DYNAMIC_EQ=1 AUDIO_DRC=1 AUDIO_LIMITER=1 A2DP_VIRTUAL_SURROUND=1 \
                    BES_LIB_DIR=lib/bes/best1306p/PRO_0002"


if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 -j64 GEN_LIB=1 BES_LIB_DIR=lib/bes/best1306p/PRO_0002 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 BES_LIB_DIR=lib/bes/best1306p/PRO_0002 -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $BUILD_CUSTOMER_CFG