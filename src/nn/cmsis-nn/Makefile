cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/Source/*/*.c))
obj-y := $(obj-y:.c=.o)

LIC_FILE_PATTERN := NOTICE

ccflags-y := $(NN_INCLUDES)

