cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.S))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))
obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))

obj-y += $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

ifeq ($(IOS_IAP2_BES_OTA_SUPPORT),1)
obj-y += modules/
endif

subdir-ccflags-y += \
    -Ibthost/host/inc/bta/bt \
    -Iapps/btapp/iap2/modules/iap2_ota