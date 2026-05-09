#!/bin/bash

COMMAND=$1
set -e
build_cust_cmd="make T=anc_usb -j CHIP=best1502p USB_AUDIO_UAC2=1 USB_HIGH_SPEED=1 USB_PLUGOUT_DET=1 USB_AUDIO_SEND_CHAN=1 USB_AUDIO_APP=1 ULTRA_LOW_POWER=1 PMU_USB_PIN_CHECK=0 \
OSC_26M_X4_AUD2BB=1 USB_AUDIO_DYN_CFG=1 RTOS=1 USB_AUDIO_384K=1 USB_HID_COMMAND_ENABLE=1 BT_SERVICE_ENABLE=0 ANDROID_ACCESSORY_SPEC=0 BES_LIB_DIR=lib/bes/best1502p/PRO_0003 -j64 \
TRACE_BUF_SIZE=12*1024 TRACE_BAUD_RATE=10*115200 NO_PWRKEY=0 USB_AUDIO_PWRKEY_TEST=1"
# USB_AUDIO_SPEECH=1 SPEECH_TX_DC_FILTER=1 SPEECH_TX_1MIC_PREAF=1 SPEECH_TX_1MIC_NS=1 SPEECH_TX_EQ=1 NO_OVERLAY=1  HW_DAC_IIR_EQ_PROCESS=1 "

# AI_RECORDING_CFG="DUAL_MIC_RECORDING=1 RECORDING_USE_OPUS=1 SINGLE_DEVICE_REC=1 AI_VOICE_TRACE_ENABLE=1 \
#     RAMCP_SIZE=128*1024 ANC_ASSIST_ENABLED=1 ANC_ASSIST_PROCESS_THREAD=1 TOTA_v2=1"

#test for 2710ibp(4m flash) evb ,nandflash
# FS_NAND_CFG="FLASH_SIZE=0x400000 AOS_FS_ENABLE=1 NAND_IOMUX_INDEX=0 -j FLASH_QSPI_ENABLE=1 FLASH_CALIB_DEBUG=1 FLASH_DMA=0 FLASH_DMA_RX_M2M=0 NANDFLASH_ENABLE=1 \
#        NANDFLASH_SUPPORT=1 DRIVER_NANDFLASH_ENABLE=1 NANDFLASH_MTD_ENABLE=0 LD_NOSYS_SPEC=1"

if [[ "$COMMAND" == "clean" ]];
then
    $build_cust_cmd clean ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_copy_cmd clean ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == "gen_lib" ]];
then
    $build_cust_cmd CAPSENSOR_TRACE_DEBUG=1 GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    $build_cust_cmd $AI_RECORDING_CFG GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
    # $build_ota_copy_cmd GEN_LIB=1 ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'lst' ]];
then
    $build_cust_cmd lst all ||{ echo "$LINENO command failed"; exit 1; }
elif [[ "$COMMAND" == 'ota' ]];
then
    $build_ota_copy_cmd ||{ echo "$LINENO command failed"; exit 1; }
else
    $build_cust_cmd $AI_RECORDING_CFG ||{ echo "$LINENO command failed"; exit 1; }
    $build_ota_copy_cmd ||{ echo "$LINENO command failed"; exit 1; }
fi
echo $build_cust_cmd 