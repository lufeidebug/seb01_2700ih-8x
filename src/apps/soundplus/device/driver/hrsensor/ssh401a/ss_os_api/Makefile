cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)


ccflags-y := \
	-Iapps/soundplus/interface \
	-Iapps/soundplus/device/hal \
	-Iapps/soundplus/device/driver/bus/hw_i2c \
	-Iapps/soundplus/device/driver/hrsensor/ssh401a \
	-Iapps/soundplus/device/driver/hrsensor/ssh401a/ss_os_api \
	-Iapps/soundplus/device/driver/hrsensor/ssh401a/ss_ppg_example \
	-Iapps/soundplus/device/driver/hrsensor/ssh401a/ss_utils
	



