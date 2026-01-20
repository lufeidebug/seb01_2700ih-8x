cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.S=.o)
obj-y := $(obj-y:.cpp=.o)


hr_algo_lib := libSleepSense.a
obj-y += $(hr_algo_lib)

ccflags-y := \
    -Iplatform/cmsis/inc  \
	-Iplatform/hal \
	-Iservices/bt_app \
	-Iutils/list \
	-Iutils/heap \
	-Iservices/bt_profiles/inc


