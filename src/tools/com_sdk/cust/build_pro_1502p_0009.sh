#!/bin/bash

TARGET_LIST="best1502p"
CHIPID="best1502p"
COMMAND=$1
set -e

BUILD_CUSTOMER_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=1 POWER_ON_ENTER_TWS_PAIRING_ENABLED=1 BLE=1 GATT_OVER_BR_EDR=1 \
                    SPP_EQ_TUNING=1 TOTA_v2=1 AUDIO_DRC=1 SMART_VOICE=1 SMART_VOICE_KWS_AQE=0 \
                    CAPSENSOR_ENABLE=1 CAPSENSOR_AT_MCU=1 CAPSENSOR_TOUCH=0 CAPSENSOR_WEAR=1 CAPSENSOR_SLIDE=0 CAPSENSOR_TRACE_DEBUG=0 \
                    CAPSENSOR_FP_MODE=1 CAPSENSOR_SPP_SERVER=1 CAPSENSOR_WEAR_USE_ONE_PAD=1 CAPSENSOR_READ_DATA_POLLING=0 \
                    SENSOR_HUB=1 SENS_TRC_TO_MCU=1 SENSORHUB_START_WHEN_MAIN_MCU_ON=1 \
                    BES_LIB_DIR=lib/bes/best1502p/PRO_0009"


if [[ "$COMMAND" == "clean" ]];
then
    make T=sensor_hub CHIP=best1502p clean
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    make T=sensor_hub -j64 CHIP=best1502p SENSOR_HUB=1 SENS_CAP_SENS_DMA_ENABLE=1 SENS_TRC_TO_MCU=1 GEN_LIB=1
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 -j64 GEN_LIB=1 BES_LIB_DIR=lib/bes/best1502p/PRO_0009 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
else
    make T=sensor_hub CHIP=best1502p SENSOR_HUB=1 SENS_CAP_SENS_DMA_ENABLE=1 SENS_TRC_TO_MCU=1 -j64
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID OTA_BIN_COMPRESSED=1 DEBUG=1 BES_LIB_DIR=lib/bes/best1502p/PRO_0009 -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $BUILD_CUSTOMER_CFG