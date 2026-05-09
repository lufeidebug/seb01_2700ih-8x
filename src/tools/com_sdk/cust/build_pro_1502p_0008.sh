#!/bin/bash

COMMAND=$1
set -e

################################### var define ###################################
LIB_DIR_CFG="BES_LIB_DIR=lib/bes/best1502p/PRO_0008"

SPEECH_3MIC_CFG="SPEECH_TX_3MIC_NS=1 SPEECH_TX_3MIC_PREAF=1 SPEECH_TX_EQ=1 SPEECH_RX_EQ=1"

build_ota_copy_cmd="make T=prod_test/ota_copy -j64 CHIP=best1502p ALLOW_WARNING=1 $LIB_DIR_CFG BES_OTA=1"

build_app_cmd="make T=best1502p -j64 ALLOW_WARNING=1 $LIB_DIR_CFG $SPEECH_3MIC_CFG \
                POWER_ON_ENTER_TWS_PAIRING_ENABLED=1 ANC_ENABLE=1 ANC_PROD_TEST=1 TOTA_v2=1 AUDIO_SECTION_ENABLE=1 "



if [[ "$COMMAND" == "clean" ]];
then
    $build_app_cmd clean
    $build_ota_copy_cmd clean
    echo "clean end"
elif [[ "$COMMAND" == "gen_lib" ]];
then
    $build_app_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_copy_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    $build_app_cmd lst all ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_copy_cmd lst all ||{ echo "$LINENO command failed"; exit 1; }
    echo "lst build end"
else
    $build_app_cmd ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_copy_cmd ||{ echo "$LINENO command failed"; exit 1; }
fi