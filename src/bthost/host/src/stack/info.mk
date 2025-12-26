BT_STACK_INFO :=

#*********************** SET BT INFO ***************************
ifneq ($(BLE_ONLY_ENABLED),1)
BT_STACK_INFO := bt

ifneq ($(mHDT_SUPPORT),1)
BT_STACK_INFO := $(BT_STACK_INFO)_hdt
endif
endif #BLE_ONLY_ENABLED


#*********************** SET BLE INFO *************************
ifeq ($(BLE),1)
ifeq ($(BT_STACK_INFO),)
BT_STACK_INFO := $(BT_STACK_INFO)ble
else
BT_STACK_INFO := $(BT_STACK_INFO)_ble
endif

ifeq ($(BLE_CS_ENABLED),1)
BT_STACK_INFO := $(BT_STACK_INFO)_cs
endif #BLE_CS_ENABLED

ifeq ($(BLE_AUDIO_ENABLED),1)
BT_STACK_INFO := $(BT_STACK_INFO)_cis
ifeq ($(BAP_BROADCAST_SINK),1)
BT_STACK_INFO := $(BT_STACK_INFO)_bis
endif #BAP_BROADCAST_SINK
endif #BLE_AUDIO_ENABLED

endif #BLE

#************************ SET MACRO **************************
$(info [BT_STACK_INFO]: $(BT_STACK_INFO))
KBUILD_CPPFLAGS += -DBT_STACK_INFO=\"$(BT_STACK_INFO)\"

