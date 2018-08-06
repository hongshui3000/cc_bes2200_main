
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.c))

obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)src/*.cpp))

src_obj := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

TWS_LIB_NAME := libtws

ifeq ($(OPUS_ON),1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_opus
endif

ifeq ($(SBC_FUNC_IN_ROM), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_sbc_in_rom
endif

ifeq ($(A2DP_AAC_ON), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_aac
endif

ifeq ($(A2DP_AAC_DIRECT_TRANSFER), 1)
TWS_LIB_NAME := $(TWS_LIB_NAME)_dir_trans
endif

$(TWS_LIB_NAME)-y := $(src_obj)

obj-y += $(TWS_LIB_NAME).a

ifeq ($(OPUS_ON),1)
subdir-ccflags-y += -O3 -DARM_MATH_CM4 -DOPUS_HAVE_CONFIG_H
OPUS_INCLUDES = \
	-Iservices/opus113/celt \
	-Iservices/opus113/celt/fixed \
	-Iservices/opus113/silk \
	-Iservices/opus113/silk/fixed \
	-Iservices/opus113/include \
	-Iservices/opus113/src \
	-Iservices/multimedia/rbcodec/codecs/libhelixaac/
else
OPUS_INCLUDES = 
endif

ifeq ($(A2DP_AAC_ON),1)
AAC_INCLUDES = -Iservices/multimedia/rbcodec/codecs/libhelixaac/
else
AAC_INCLUDES =
endif

subdir-ccflags-y += \
	-Iservices/fs/fat \
	-Iservices/fs/sd \
	-Iservices/fs/fat/ChaN \
	-Iservices/bt_profiles/inc \
	-Iservices/overlay \
	-Iservices/nvrecord \
	-Iservices/resources \
	-Iservices/multimedia/rbcodec \
	-Iservices/multimedia/audio/process/resample/include \
	-Iplatform/drivers/uarthci \
	-Iplatform/drivers/ana \
	-Iplatform/drivers/bt \
	-Iutils/cqueue \
	-Iservices/audioflinger \
	-Iutils/lockcqueue \
	-Iutils/intersyshci \
	-Iapps/key \
	-Iapps/main \
	-Iapps/common \
	-Iapps/audioplayers \
	-Iapps/common \
	-Iapps/factory \
	-Iservices/resources \
	-Iservices/ble_app \
	-Iservices/bt_app \
	-Iservices/tws/inc \
	$(OPUS_INCLUDES) \
	$(AAC_INCLUDES) \
     -Iutils/hwtimer_list

KBUILD_CPPFLAGS += -DAUDIO_OUTPUT_VOLUME_DEFAULT=$(AUDIO_OUTPUT_VOLUME_DEFAULT)

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_app_tws.o += -D__AUDIO_RESAMPLE__
endif
ifeq ($(SW_PLAYBACK_RESAMPLE),1)
CFLAGS_app_tws.o += -DSW_PLAYBACK_RESAMPLE
endif
ifeq ($(SW_CAPTURE_RESAMPLE),1)
CFLAGS_app_tws.o += -DSW_CAPTURE_RESAMPLE
endif
ifeq ($(RESAMPLE_ANY_SAMPLE_RATE),1)
CFLAGS_app_tws.o += -DRESAMPLE_ANY_SAMPLE_RATE
endif

