cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y +=  \
    $(BLUETOOTH_ADAPTER_INCLUDES) \
    $(EPLAYER_INCLUDES) \
    $(ECOMM_INCLUDES) \
    $(BT_SERVICE_UX_INCLUDES) \
    -Iservices/audio_bt \
    -Iservices/audioflinger \
    -Iservices/norflash_api \
    -Iservices/nv_section/factory_section \
    -Iservices/nv_section/log_section \
    -Iservices/overlay \
    -Iservices/resources \
    -Iservices/activity_manager \
    -Iservices/audio_process \
    -Iapps/app_test/apptester \
    -Iapps/factory \
    -Iapps/voice_detector \
    -Iservices/ux/common/sound \
    -Iutils/crc32 \
    -Iutils/list \
    -Iutils/codec_calib \
    -Iplatform/drivers/bt \
    -Iplatform/drivers/ana \
    -Iapps/audioplayers/rbplay \
    -Itests/anc_usb \
    -Iapps/anc/inc \
    -Ithirdparty/userapi \
    -Iservices/communication \
    -Iutils/cqueue \
    -Iservices/ai_voice/audio \
    -Iservices/ai_voice/manager \
    -Iservices/ai_voice/transport \
    -Iservices/app_ai/inc \
    -Iservices/interconnection/red \
    -Iservices/interconnection/green \
    -Iservices/bridge/ \
    -Iservices/voicepath \
    -Iservices/voicepath/gsound/gsound_service \
    -Iservices/voicepath/gsound/gsound_target \
    -Iservices/voicepath/gsound/gsound_custom/inc \
    -Iservices/voicepath/gsound/gsound_target_api_read_only \
    -Iservices/ota/ai_ota/ \
    -Iservices/bes_bth/ble/aob_app/inc \
    -Iservices/aob_bes_ux/inc \
    -Iservices/aob_custom_ux \
    -Ithirdparty/tile \
    -Ithirdparty/tile/tile_target \
    -Iinclude/ \
    -Iapps/btwatch \
    -Iapps/sensorhub \
    -Iapps/dsp_hifi4 \
    -Iapps/dsp_m55 \
    -Iapps/cross_core_common \
    -Iplatform/drivers/cp_accel \
    -Iplatform/drivers/hifi4 \
    -Iplatform/drivers/dsp_m55c1 \
    -Iplatform/drivers/fw_linux/inc \
    -Iplatform/main \
    -Iapps/mem_scan \
    -Imultimedia/inc/speech/inc \
    -Iservices/audio_manager \
    -Iapps/anc_assist/inc \
    -Iapps/earbuds/conn \
    -Iapps/earbuds/test \
	-Iservices/ux/common/ibrt/ibrt_core/inc \
	-Iapps/audioplayers/a2dp_decoder \
	-Iservices/ux/product/earbud_tws/inc \
	-Iservices/audio_manager/audio_policy/bredr_policy/inc \
	-Ibthost/service/ble_app/inc \
	-Ibthost/stack/ble_stack/inc \
	-Ibthost/host/src/bta/tws/legacy/earbuds_ui/inc \
	-Ibthost/host/src/service/audio_policy/bt_focus/inc \
	-Ibthost/apps/bta_apps/deprecated/inc \
	-Ibthost/porting \
    -Iapps/soundplus/algorithm/hearaid_dsp \
    -Iapps/psap_sw \
	-Iutils/hsm \
	-Iutils/cqueue \
	-Iservices/audio_dump/include \
	-Ibthost/apps/bta_apps/earbuds/inc\
    -Iapps/main\
    -Ibthost/host/src/service/ble_app_new/inc \
    -Ibthost/host/src/adapter/inc/ble/common \
    -Ibthost/host/src/stack/ble_if/inc \
    -Ibthost/host/src/adapter/inc/ble/gap\
    -Iapps/earbuds/conn\
    -Ibthost/host/src/service/module/common/if



ccflags-y += -Wno-error=date-time -Wno-date-time

ccflags-y += \
	-Iapps/soundplus/interface \
	-Iapps/soundplus/device/hal \
	-Iapps/soundplus/device/drivers


ifeq ($(SNDP_ALG_SHIELD),1)
ccflags-y += -Iapps/soundplus/algorithm/shield
endif

ifeq ($(SNDP_ALG_HEAR_AID),1)
ccflags-y += -Iapps/soundplus/algorithm/hearaid
endif

ifeq ($(SNDP_ALG_TONE_DETECT),1)
ccflags-y += -Iapps/soundplus/algorithm/tonedetect
endif

ifeq ($(SNDP_ALG_RUB_FIT),1)
ccflags-y += -Iapps/soundplus/algorithm/rubfit
endif

ifeq ($(SNDP_PSENSOR_JSA1227),1)
ccflags-y += -Iapps/soundplus/device/driver/psensor/jsa1227
endif

ifeq ($(SNDP_ALG_ENC),1)
ifeq ($(SNDP_SPEECH_RUN_M55),1)
ccflags-y += \
    -Iapps/soundplus/algorithm/enc/m55_enc
else
ccflags-y += -Iapps/soundplus/algorithm/enc/m33_enc
endif
endif		

ifeq ($(SNDP_COVER_SWITCH_BOX_NOTIFY),1)
ccflags-y += -Iapps/soundplus/device/hal \
			-Iapps/soundplus/device/driver/cover_switch/box_notify
endif	

ifeq ($(SNDP_HR_ALGO_SLEEPSENSE),1)
ccflags-y += -Iapps/soundplus/algorithm/hr/SleepSense
subdir-ccflags-y += -Iapps/soundplus/algorithm/hr/SleepSense
endif	
