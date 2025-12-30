cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y := \
			-Iapps/soundplus/common \
			-Iapps/soundplus/interface \
			-Iplatform/drivers/ana
			


ifeq ($(SNDP_BAT_LOOKUP_TABLE),1)
ccflags-y += -Iapps/soundplus/device/driver/bat_pwr/lookup_table
endif

ifeq ($(SNDP_BAT_CUMULATIVE),1)
ccflags-y += -Iapps/soundplus/device/driver/bat_pwr/cumulative
endif

ifeq ($(SNDP_BAT_COULOMBMETER),1)
ccflags-y += -Iapps/soundplus/device/driver/bat_pwr/coulombmeter
endif

ifeq ($(SNDP_CHARGER_PLUG_PMU_INT),1)
ccflags-y += -Iapps/soundplus/device/driver/charger_plug/pmu_int
endif

ifeq ($(SNDP_CHARGER_HP4554),1)
ccflags-y += -Iapps/soundplus/device/driver/charger/hp4554
endif

ifeq ($(SNDP_TEMPERATURE_NTC),1)
ccflags-y += -Iapps/soundplus/device/driver/temperature/ntc
endif

ifeq ($(SNDP_GSENSOR_XXXX),1)
ccflags-y += -Iapps/soundplus/device/driver/gsensor/gsxxxx
endif

ifeq ($(SNDP_IOBOX_PMU_INT),1)
ccflags-y += -Iapps/soundplus/device/driver/iobox/pmu_int
endif

ifeq ($(SNDP_IOBOX_GPIO_INT),1)
ccflags-y += -Iapps/soundplus/device/driver/iobox/gpio_int
endif

ifeq ($(SNDP_PSENSOR_STK_W2101),1)
ccflags-y += -Iapps/soundplus/device/driver/psensor/stk_w2101
endif


ifeq ($(SNDP_COVER_SWITCH_HALL),1)
ccflags-y += -Iapps/soundplus/device/driver/cover_switch/hall
endif

ifeq ($(SNDP_COVER_SWITCH_BOX_NOTIFY),1)
ccflags-y += -Iapps/soundplus/device/driver/cover_switch/box_notify
endif

ifeq ($(SNDP_COVER_SWITCH_INOUT_SIMULATE),1)
ccflags-y += -Iapps/soundplus/device/driver/cover_switch/inout_simulate
endif


ifeq ($(SNDP_TOUCH_GH6210),1)
ccflags-y += -Iapps/soundplus/device/driver/touch/gh610
endif

ifeq ($(SNDP_POGOPIN_SINGLE_UART),1)
ccflags-y += \
		-Iservices/communication \
		-Iapps/soundplus/device/driver/pogopin_comm/single_uart
endif

ifeq ($(SNDP_POGOPIN_DUAL_UART),1)
ccflags-y += \
		-Iapps/soundplus/device/driver/pogopin_comm/dual_uart
endif

ifeq ($(SNDP_GESTURE_KEY),1)
ccflags-y += -Iapps/soundplus/device/driver/key
endif
			
ifeq ($(SNDP_CHARGER_SY5501),1)
ccflags-y += -Iapps/soundplus/device/driver/charger/sy5501
endif		

ifeq ($(SNDP_TOUCH_GH6210),1)
ccflags-y += -Iapps/soundplus/device/driver/touch/gh6210
endif		

ifeq ($(SNDP_COULOMETER_CW2015),1)
ccflags-y += -Iapps/soundplus/device/driver/bat_pwr/coulometer/cw2015
endif


