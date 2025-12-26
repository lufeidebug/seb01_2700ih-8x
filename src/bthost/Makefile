cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

include $(cur_dir)/config.mk

# Git revision
BTH_VERSION := $(shell git -C $(cur_dir) rev-parse HEAD || echo "unknown")
KBUILD_CPPFLAGS += -DBTH_VERSION=\"$(BTH_VERSION)\"

export BLE_IF_DIR_PATH := $(BLUETOOTH_STACK_DIR_PATH)/ble_if
export BLE_APP_DIR_PATH ?= bthost/host/src/service/ble_app_new
export BLE_GAF_CORE_DIR_PATH ?= bthost/host/src/service/ble_audio/blueelf_adapter

ifeq ($(BLE_ONLY_ENABLED),1)
export BT_SVC_FW_PRODUCT ?= BT_SVC_FW_PRODUCT_BLE_ONLY
export BT_SVC_FW_ENABLED := 1
endif

ifeq ($(BTHOST_MIDDLEWARE),)
ifneq ($(BT_FRAMEWORK),1)
export BT_SVC_FW_PRODUCT ?= BT_SVC_FW_PRODUCT_BLE_ONLY
export BT_SVC_FW_ENABLED := 1
endif
endif

obj-y := host/
obj-y += apps/
