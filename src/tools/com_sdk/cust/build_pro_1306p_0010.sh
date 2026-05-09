#!/bin/bash

# 1.支持杜比音效
# 2.tws耳机应用，无ANC降噪需求(为了省RAM及资源，如果不影响可默认添加到SDK内);
# 3.支持音频编解码 SBC、AAC(关PNS)、LDAC(其他编码若占RAM及资源，请裁减掉);
# 4.支持一拖二;
# 5.支持BLE、SPP
# 6.音频支持DRC
# 7.EQ要求增加平滑处理；
# 8.bitpool可修改（默认53）
# 9. HID 拍照功能

TARGET_LIST="best1306p"
CHIPID="best1306p"
COMMAND=$1
COMMAND_CUST=$2

LIB_DIR_CFG="BES_LIB_DIR=lib/bes/"$CHIPID"/PRO_0010"

POWER_ON_CFG="POWER_ON_ENTER_TWS_PAIRING_ENABLED=1"

AUDIO_CODEC_CFG="AAC_PNS_SKIP=1 A2DP_AAC_ON=1 A2DP_LDAC_ON=0 A2DP_LHDC_ON=0 A2DP_LHDC_V3=0 A2DP_LHDCV5_ON=0"
AUDIO_PROCESS_CFG="A2DP_VIRTUAL_SURROUND=0 A2DP_VIRTUAL_SURROUND_STEREO=0 AUDIO_BASS_ENHANCER=0 AUDIO_LIMITER=0 AUDIO_DRC=0"
#  APP_DEBUG_TOOL=TOTA
COM_CFG="DEBUG=1 APP_TRACE_RX_ENABLE=1 APP_RX_API_ENABLE=1 TRACE_BUF_SIZE=4*1024 FAST_XRAM_SECTION_SIZE=0x5000"

OTA_BASE="OTA_BIN_COMPRESSED=1"
OTA_APP_CFG="$OTA_BASE BES_OTA=1"
OTA_BOOTLOADER_CFG="$OTA_BASE"

ENC_CFG=""

AI_RECORDING_CFG=""

BT_APP_FEATURE_CFG="BLE=1 GFPS_ENABLE=1 GATT_OVER_BR_EDR=1"

CAPSENSOR_CFG="CAPSENSOR_ENABLE=1 CAPSENSOR_WEAR=1 CAPSENSOR_TOUCH=1 CAPSENSOR_SLIDE=1 CAPSENSOR_TRACE_DEBUG=0 CAPSENSOR_SPP_SERVER=0"

FREEMAN_ENABLED_STERO=0

set -e

BUILD_CUSTOMER_CFG="-j FREEMAN_ENABLED_STERO=$FREEMAN_ENABLED_STERO FREEMAN_OTA_ENABLE=1 USER_BURN_ADDR_PAIR_EN=1 \
                    $POWER_ON_CFG $AUDIO_CODEC_CFG $AUDIO_PROCESS_CFG $COM_CFG \
                    $OTA_APP_CFG $ENC_CFG $AI_RECORDING_CFG $BT_APP_FEATURE_CFG $CAPSENSOR_CFG "

build_otacopy_cmd="make T=prod_test/ota_copy  CHIP=$CHIPID DEBUG=1 -j40 $OTA_BOOTLOADER_CFG $LIB_DIR_CFG"
build_app_cmd="make T=$TARGET_LIST -j40 DEBUG=1 $BUILD_CUSTOMER_CFG $LIB_DIR_CFG $COMMAND_CUST"


############################## trust zone ############################################
if [[ "$COMMAND" == '' ]];
then
COMMAND="tz"
fi

if  [[ "$COMMAND" == 'tz_se' ]] || [[ "$COMMAND" == 'dolby' ]];
then 
SECURE_FLAG=1
OTA_TZ_FLAGS="OTA_TZ_ENABLE=1 ALLOW_WARNING=1"
SECURE_BOOT_CFG="SECURE_BOOT=$SECURE_FLAG USER_SECURE_BOOT=$SECURE_FLAG FLASH_SECURITY_REGISTER=1"
else
SECURE_FLAG=0
OTA_TZ_FLAGS=""
SECURE_BOOT_CFG="SECURE_BOOT=$SECURE_FLAG USER_SECURE_BOOT=$SECURE_FLAG FLASH_SECURITY_REGISTER=1"
OTA_COPY_OFFSET_CFG="OTA_BOOT_SIZE=0x18000 OTA_CODE_OFFSET=0x20000 FLASH_S_SIZE=0x80000 OTA_SEC_BOOT_INFO_OFFSET=0x18000"
OTA_APP_OFFSET_CFG="OTA_SEC_BOOT_INFO_OFFSET=0x18000"
fi
PROJECT_CFG_FLAGS="FLASH_SIZE=0x400000"

if [[ "$COMMAND" == 'dolby' ]];
then
PROJECT_CFG_FLAGS="$PROJECT_CFG_FLAGS SPA_AUDIO_ENABLE=1 SPA_AUDIO_SEC=1 DOLBY_AUDIO_ENABLE=1 DOLBY_AUDIO_DAW_ENABLE=0 "
else
PROJECT_CFG_FLAGS="$PROJECT_CFG_FLAGS SPA_AUDIO_ENABLE=0 SPA_AUDIO_SEC=0 DOLBY_AUDIO_ENABLE=0 DOLBY_AUDIO_DAW_ENABLE=0 DLB_SEC_LOAD_DEMO=0"
OTA_TZ_FLAGS="$OTA_TZ_FLAGS ARM_CMNS_OTA=1 COMBO_CUSBIN_IN_FLASH=1"
fi

build_secure_otacopy_cmd="$build_otacopy_cmd \
    $SECURE_BOOT_CFG $OTA_TZ_FLAGS $OTA_COPY_OFFSET_CFG\
    "
build_cmse_cmd="make T=arm_cmse DEBUG=1 -j40  CHIP=$CHIPID \
    $PROJECT_CFG_FLAGS $SECURE_BOOT_CFG $OTA_TZ_FLAGS \
    TZ_ROM_UTILS_IF=0 ROM_UTILS_ON=0 CRC32_ROM=1 CMSE_RAM_RAMX_LEND_NSE=1 \
    SE_OTP_DEMO_TEST=0 CMSE_CRYPT_TEST_DEMO=0 MBEDTLS_CONFIG_FILE="config-rsa.h" \
    LARGE_SE_RAM=1 FLASH_LOW_SPEED=1 ULTRA_LOW_POWER=1 \
"
build_app_trust_zone_cmd="$build_app_cmd \
    $PROJECT_CFG_FLAGS $OTA_TZ_FLAGS SECURE_BOOT=$SECURE_FLAG USER_SECURE_BOOT=0 OTA_CODE_OFFSET=0x80000  \
    LIBC_ROM=0 ARM_CMNS=1 TZ_ROM_UTILS_IF=0 ROM_UTILS_ON=0 LARGE_SE_RAM=1 $OTA_APP_OFFSET_CFG \
    \
"
# AMCP_SIZE=0x4E000 RAMCPX_SIZE=0x11000 OS_DYNAMIC_MEM_SIZE=0x5900 FAST_XRAM_SECTION_SIZE=0x18000 \
#######################################################################################

if [[ "$COMMAND" == "clean" ]];
then
    make T=$TARGET_LIST clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
        $build_otacopy_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
        $build_app_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }

        $build_secure_otacopy_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
        $build_cmse_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
        $build_app_trust_zone_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    make T=$TARGET_LIST $BUILD_CUSTOMER_CFG -j64 lst all ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'tz' ]] || [[ "$COMMAND" == 'tz_se' ]] || [[ "$COMMAND" == 'dolby' ]];
then
        echo "Build Bootloader"
        echo $build_secure_otacopy_cmd
        $build_secure_otacopy_cmd ||{ echo "$LINENO command failed"; exit 1; }

        echo "Build cmse"
        echo $build_cmse_cmd
        $build_cmse_cmd ||{ echo "$LINENO command failed"; exit 1; }

        echo "Build cmns"
        echo $build_app_trust_zone_cmd
        $build_app_trust_zone_cmd ||{ echo "$LINENO command failed"; exit 1; }
else
    echo "Build Bootloader"
    $build_otacopy_cmd ||{ echo "$LINENO command failed"; exit 1; }
    echo $build_otacopy_cmd

    echo "Build App"
    $build_app_cmd ||{ echo "$LINENO command failed"; exit 1; }
    echo $build_app_cmd
fi
