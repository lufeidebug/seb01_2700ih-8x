#!/bin/bash
[ -z "$1" ] &&{
    echo The Chip is Null !!!
    exit
}

CHIPID=$1
COMMAND=$2

single_dac=0

# cd ./bthost/
# git checkout origin/cust/ep_comm_sdk_v2.0
# cd ../
# cd ./multimedia/algorithms/
# git checkout origin/cust/ep_comm_sdk_v2.0
# cd ../../

if [[ $CHIPID == "best1306p" ]];
then
    TARGET_LIST="best1306p"
    echo "1306p"
elif [[ $CHIPID == "best1502p" ]];
then
    TARGET_LIST="best1502p"
    echo "1502p"
elif [[ $CHIPID == "best1503" ]];
then
    TARGET_LIST="best1503"
    single_dac=1
    echo "1503"
elif [[ $CHIPID == "best1503p" ]];
then
    TARGET_LIST="best1503p"
    single_dac=1
    echo "1503p"
elif [[ $CHIPID == "best1307p" ]];
then
    # cd ./bthost/
    # git checkout origin/cust/ep_comm_sdk_v2.0_1307p
    # cd ../
    # cd ./multimedia/algorithms/
    # git checkout origin/cust/ep_comm_sdk_v2.0_1307p
    # cd ../../
    TARGET_LIST="best1307p"
    single_dac=1
    echo "1307p"
elif [[ $CHIPID == "best1306" ]];
then
    TARGET_LIST="best1306"
    echo "best1306"
fi

if [[ $CHIPID == "best1307p" ]];
then
    ROM_CFG="BTH_IN_ROM=1 BTH_ROM_VERSION=3 AUDIO_EQ_DYNAMICS_MEM=1"
    LEA_CFG="BLE_AUDIO_ENABLED=1 LEA_ENABLE=1 APP_BLE_BIS_DELEG_ENABLE=0 APP_BLE_BIS_SINK_ENABLE=0 APP_BLE_BIS_ASSIST_ENABLE=0 APP_BLE_BIS_SRC_ENABLE=0"
else
    ROM_CFG=""
    LEA_CFG="BLE_AUDIO_ENABLED=1 LEA_ENABLE=1"
fi

set -e

# ./tools/com_sdk/cust/build_pro_1502p_0007.sh best1502p
################################### var define ###################################
APP_CUSTOMER_CFG="IGNORE_POWER_ON_KEY_DURING_BOOT_UP=1 IS_AUTOPOWEROFF_ENABLED=0 APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 AUDIO_DEBUG_CMD=0 \
                 TRACE_BUF_SIZE=20*1024 USE_TRACE_ID=0\
                 CTKD_ENABLE=1\
                 OPUS_UNSUPPORT=1"

OTA_CFG="BES_OTA=1 OTA_BIN_COMPRESSED=1"

BLE_CFG="BLE=1 GATT_OVER_BR_EDR=0"

TWS_CFG="POWER_ON_ENTER_TWS_PAIRING_ENABLED=1"

FREEMAN_CFG="POWER_ON_ENTER_FREEMAN_PAIRING_ENABLED=1 FREEMAN_ENABLED_STERO=1"

SPEECH_CFG="SPEECH_TX_DC_FILTER=1 SPEECH_TX_1MIC_PREAF=1 SPEECH_TX_1MIC_NS=1 SPEECH_TX_EQ=1"

SPEECH_2MIC_CFG="SPEECH_TX_DC_FILTER=1 SPEECH_TX_2MIC_NS8=1 SPEECH_TX_EQ=0"

DECODE_DECODER_CFG="A2DP_AAC_ON=1 A2DP_LDAC_ON=0 A2DP_LHDC_ON=0 A2DP_LHDC_V3=0 A2DP_LHDCV5_ON=0"

SYS_CFG=" ALLOW_WARNING=1 RAMCP_SIZE=0x20000 FAST_XRAM_SECTION_SIZE=0x6000 "

ANC_CFG="ANC_ENABLE=1 ANC_ASSIST_ENABLED=0 VOICE_ASSIST_NOISE=0 VOICE_ASSIST_CUSTOM_LEAK_DETECT=0 VOICE_ASSIST_CUSTOM_LEAK_DETECT=0 TOTA_v2=1"

ANC_USB_CFG="USB_AUDIO_APP=1 ULTRA_LOW_POWER=1 PMU_USB_PIN_CHECK=0 OSC_26M_X4_AUD2BB=1 USB_AUDIO_DYN_CFG=0 RTOS=0"

BES_LIB_DIR="BES_LIB_DIR=lib/bes/best1502p/PRO_0007"
################################### end of var define ###################################

build_app_cmd="make T=$TARGET_LIST $APP_CUSTOMER_CFG $BLE_CFG $OTA_CFG $TWS_CFG $SPEECH_CFG $DECODE_DECODER_CFG $SYS_CFG $ANC_CFG $ROM_CFG $SPEECH_2MIC_CFG $BES_LIB_DIR -j64"
build_ota_cmd="make T=prod_test/ota_copy CHIP=$CHIPID $ROM_CFG OTA_BIN_COMPRESSED=1 DEBUG=1 -j64"

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    $build_app_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    $build_app_cmd lst all ||{ echo "$LINENO command failed"; exit 1; }
    echo $build_app_cmd lst all
else
    $build_app_cmd ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_cmd ||{ echo "$LINENO command failed"; exit 1; }
    echo $build_app_cmd
fi
# cd ./bthost/
# git checkout origin/cust/ep_comm_sdk_v2.0
# cd ../
# cd ./multimedia/algorithms/
# git checkout origin/cust/ep_comm_sdk_v2.0
# cd ../../