cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

#Only allowed to be used within bthost
export BTH_STACK_INT_INCLUDES ?= \
	-I$(BLUETOOTH_STACK_DIR_PATH)/ble_stack_new/inc \
	-I$(BLUETOOTH_STACK_DIR_PATH)/common

#***********************************
# Build app
#***********************************
ifeq ($(BT_SVC_FW_ENABLED),1)
obj-y += src/bta/
endif

#***********************************
# Build service
#***********************************
ifeq ($(BTHOST_MIDDLEWARE),)

ifneq ($(wildcard $(cur_dir)src/adapter/Makefile),)
src_obj += src/adapter/
endif

ifneq ($(wildcard $(cur_dir)src/service/Makefile),)
src_obj += src/service/
endif

ifneq ($(wildcard $(cur_dir)src/bt_vendor/Makefile),)
src_obj += src/bt_vendor/
endif
endif #BTHOST_MIDDLEWARE

ifneq ($(wildcard $(cur_dir)src/porting/Makefile),)
src_obj += src/porting/
endif


#***********************************
# Build middleware
#***********************************
ifneq ($(BTHOST_MIDDLEWARE),)
obj-y += src/middleware/
KBUILD_CPPFLAGS += -DBTHOST_MIDDLEWARE

ifneq ($(BT_FRAMEWORK),)
src_obj += src/framework/
endif
endif

#***********************************
# Build stack
#***********************************
ifeq ($(BTH_IN_ROM),1)
ifneq ($(wildcard $(cur_dir)src/rom/Makefile),)
src_obj += src/rom/
endif
else
obj-y += src/stack/
endif

#***********************************
# SDK build
#***********************************
BTHOST_SERVICE_LIB_NAME := libbthost_service

ifeq ($(BLE_AUDIO_ENABLED), 1)
BTHOST_SERVICE_LIB_NAME := $(BTHOST_SERVICE_LIB_NAME)_aob
endif

ifeq ($(BISTO_ENABLE), 1)
BTHOST_SERVICE_LIB_NAME := $(BTHOST_SERVICE_LIB_NAME)_bisto
endif

ifneq ($(GEN_LIB_NAME_BT),)
BTHOST_SERVICE_LIB_NAME := $(BTHOST_SERVICE_LIB_NAME)$(GEN_LIB_NAME_BT)
endif

ifeq ($(USE_TRACE_ID), 1)
BTHOST_SERVICE_LIB_NAME := $(BTHOST_SERVICE_LIB_NAME)_log_compressed
endif

$(BTHOST_SERVICE_LIB_NAME)-y := $(src_obj)
obj-y += $(BTHOST_SERVICE_LIB_NAME).a

# Git revision
BESLIB_INFO := $(shell cd $(cur_dir) && (which git >/dev/null 2>&1) && (git rev-parse --short HEAD 2>/dev/null))
