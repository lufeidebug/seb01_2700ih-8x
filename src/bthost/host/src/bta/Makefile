cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifneq (,$(findstring EARBUDS,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)tws/Makefile),)
src_obj += tws/
endif
endif

ifneq (,$(findstring DONGLE,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)dongle/Makefile),)
src_obj += dongle/
endif
endif

ifneq (,$(findstring HEADSET,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)normal/Makefile),)
src_obj += normal/
endif
endif

ifneq (,$(findstring GLASSES,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)normal/Makefile),)
src_obj += normal/
endif
endif

ifneq (,$(findstring SPEAKER,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)normal/Makefile),)
src_obj += normal/
endif
endif

ifneq (,$(findstring SOUND,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)sound/Makefile),)
src_obj += sound/
endif
endif

ifneq (,$(findstring WIRELESSMIC,$(BT_SVC_FW_PRODUCT)))
ifneq ($(wildcard $(cur_dir)wmic/Makefile),)
src_obj += wmic/
endif
endif

src_obj += bt/

subdir-ccflags-y += -DMODULE=SERVICE

BTHOST_APP_TWS_LIB_NAME := libbthost_app_tws

ifeq ($(BLE_AUDIO_ENABLED), 1)
BTHOST_APP_TWS_LIB_NAME := $(BTHOST_APP_TWS_LIB_NAME)_aob
endif

ifneq ($(GEN_LIB_NAME_BT),)
BTHOST_APP_TWS_LIB_NAME := $(BTHOST_APP_TWS_LIB_NAME)$(GEN_LIB_NAME_BT)
endif

ifeq ($(USE_TRACE_ID), 1)
BTHOST_APP_TWS_LIB_NAME := $(BTHOST_APP_TWS_LIB_NAME)_log_compressed
endif

$(BTHOST_APP_TWS_LIB_NAME)-y := $(src_obj)
obj-y += $(BTHOST_APP_TWS_LIB_NAME).a
