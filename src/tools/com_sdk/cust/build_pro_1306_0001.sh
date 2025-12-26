#!/bin/bash

TARGET_LIST="best1306"
CHIPID="best1306"
COMMAND=$1
set -e

BUILD_CUSTOMER_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=1 POWER_ON_ENTER_TWS_PAIRING_ENABLED=1 \
                    ANC_ENABLE=1 APP_ANC_TEST=1 SPEECH_TX_2MIC_NS8=1 SPEECH_TX_1MIC_NS=0 \
                    AUDIO_DYNAMIC_EQ=1 SPP_EQ_TUNING=0 TOTA_v2=1 BLE=1 \
                    BESUI_APP_EN=1 BES_TWSPRO_EN=1 BESUI_TWS_EN=1 USER_BURN_ADDR_PAIR_EN=1 \
                    RAMCP_SIZE=155*1024 RAMCPX_SIZE=47*1024 \
                    BES_LIB_DIR=lib/bes/best1306/PRO_0001"

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 -j64 GEN_LIB=1 BES_LIB_DIR=lib/bes/best1306/PRO_0001 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 BES_LIB_DIR=lib/bes/best1306/PRO_0001 -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $BUILD_CUSTOMER_CFG