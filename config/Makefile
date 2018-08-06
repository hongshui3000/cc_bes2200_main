cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

#obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))

FILE_LIST := tgt_hardware.c app_customized_touch_keyhandle.cpp
obj-y := $(patsubst $(cur_dir)%,%,$(foreach f,$(FILE_LIST), \
    $(if $(wildcard $(cur_dir)$(T)/$(f)),$(cur_dir)$(T)/$(f),$(cur_dir)$(DEFAULT_CFG_SRC)/$(f))))

obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y += \
	-Iplatform/hal \
	-Iplatform/drivers/usb/usb_dev/inc \
	-Iservices/nv_section/aud_section \
	-Iservices/nv_section/include \
	-Iapps/audioplayers \
	-Iservices/multimedia/speech/process/vad/include \
	-Iservices/multimedia/speech/process/agc/include \
	-Iservices/multimedia/speech/process/speech_drc/include \
	-Iservices/multimedia/speech/process/lc_mmse_n/include \
	-Iservices/multimedia/speech/process/dual_mic_denoise/include \
	-Iservices/multimedia/speech/process/coherent_denoise/include \
	-Iservices/multimedia/speech/process/far_field_speech_enhancement/include \
	-Iservices/multimedia/audio/process/filters/include \
	-Iutils/hwtimer_list \
	-Iapps/key \
	-Iapps/main
ifeq ($(BT_DEV_NAME),)
BT_DEV_NAME := $(T)
endif

ccflags-y += -DBT_DEV_NAME=$(BT_DEV_NAME) $(CUST_TGT_FLAGS)

ifneq ($(BT_DEV_ADDR),)
ccflags-y += -DBT_DEV_ADDR=$(BT_DEV_ADDR)
endif

ifneq ($(BLE_DEV_ADDR),)
ccflags-y += -DBT_DEV_ADDR=$(BLE_DEV_ADDR)
endif

