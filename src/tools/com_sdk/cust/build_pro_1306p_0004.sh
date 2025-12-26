#!/bin/bash


TARGET_LIST="best1306p"
CHIPID="best1306p"
COMMAND=$1

LIB_DIR_CFG="BES_LIB_DIR=lib/bes/"$CHIPID"/PRO_0004"


# BES2710IHC软件需求：
# 1、用于充电盒主芯片。
# 2、只需要BLE，不需要BT。BLE 需要支持GATT OVER BR EDR。
# 3、支持BLE OTA固件升级。（是否支持remap方式，固件压缩？）
# 4、支持BECO NPU, 有demo可以验证。
COM_CFG="APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 TRACE_BUF_SIZE=36*1024"

OTA_BASE="OTA_BIN_COMPRESSED=1"
OTA_APP_CFG="$OTA_BASE BES_OTA=1 FREEMAN_OTA_ENABLE=1 FLASH_REMAP=0"
OTA_BOOTLOADER_CFG="$OTA_BASE OTA_REBOOT_FLASH_REMAP=0"

BT_APP_FEATURE_CFG="SPEECH_NS_BECO=1 BLE=1 GATT_OVER_BR_EDR=0 TOTA_v2=1 IGNORE_POWER_ON_KEY_DURING_BOOT_UP=1"
# FREEMAN_ENABLED_STERO=1 FREEMAN_OTA_ENABLE=1 USER_BURN_ADDR_PAIR_EN=1"

DEBUG_LIB_CFG="$LIB_DIR_CFG/DEBUG"
NO_DEBUG_LIB_CFG="$LIB_DIR_CFG/NODEBUG"

set -e

BUILD_CUSTOMER_CFG="$OTA_APP_CFG $COM_CFG $BT_APP_FEATURE_CFG"

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
        make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -sj DEBUG=0 $NO_DEBUG_LIB_CFG GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
        make T=prod_test/ota_copy CHIP=$CHIPID $OTA_BOOTLOADER_CFG DEBUG=0 -sj GEN_LIB=1 $NO_DEBUG_LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }

        make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -sj GEN_LIB=1 DEBUG=1 $DEBUG_LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }
        make T=prod_test/ota_copy CHIP=$CHIPID $OTA_BOOTLOADER_CFG DEBUG=1 -sj GEN_LIB=1 $DEBUG_LIB_CFG ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'debugoff' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG DEBUG=0 $NO_DEBUG_LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    make T=prod_test/ota_copy CHIP=$CHIPID $OTA_BOOTLOADER_CFG DEBUG=0 $NO_DEBUG_LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
else
    echo "Build Bootloader"
    make T=prod_test/ota_copy CHIP=$CHIPID $OTA_BOOTLOADER_CFG DEBUG=1 $DEBUG_LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
    
    echo "Build App"
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG DEBUG=1 $DEBUG_LIB_CFG -j64 ||{ echo "$LINENO command failed"; exit 1; }
fi
echo "make T=$TARGET_LIST $BUILD_CUSTOMER_CFG"