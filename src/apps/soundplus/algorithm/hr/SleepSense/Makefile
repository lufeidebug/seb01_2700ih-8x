cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.S=.o)
obj-y := $(obj-y:.cpp=.o)


HR_ALGO_LIB_NAME := libSleepSense.a
#LIB_BIN_DIR := $(cur_dir)lib
LIB_BIN_DIR := lib/thirdparty/SleepSense
obj-y += $(HR_ALGO_LIB_NAME)

ccflags-y := \
    -Iplatform/cmsis/inc  \
	-Iplatform/hal \
	-Iservices/bt_app \
	-Iutils/list \
	-Iutils/heap \
	-Iservices/bt_profiles/inc


