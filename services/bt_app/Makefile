
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))

obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))

obj-y := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)


ifeq ($(A2DP_AAC_ON),1)
AAC_INCLUDES = -Iservices/multimedia/rbcodec/codecs/libhelixaac/
else
AAC_INCLUDES =
endif

ccflags-y += \
	-Iservices/audio_eq \
	-Iservices/fs/fat \
	-Iservices/fs/sd \
	-Iservices/fs/fat/ChaN \
	-Iservices/bt_profiles/inc \
	-Iservices/bt_profiles/inc/sys \
	-Iservices/overlay \
	-Iservices/nvrecord \
	-Iservices/resources \
	-Iservices/multimedia/rbcodec \
	-Iservices/multimedia/audio/process/resample/include \
	-Iservices/multimedia/audio/process/filters/include \
	-Iservices/multimedia/audio/process/drc/include \
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
	-Iapps/audioplayers/rbplay \
	-Iapps/common \
	-Iapps/factory \
	-Iservices/resources \
	-Iservices/ble_app \
	-Iservices/tws/inc \
	-Iutils/hwtimer_list  \
	$(AAC_INCLUDES) \
	
	

CFLAGS_app_bt_stream.o += -DAUDIO_OUTPUT_VOLUME_DEFAULT=$(AUDIO_OUTPUT_VOLUME_DEFAULT)

ifeq ($(BT_ONE_BRING_TWO),1)
ccflags-y += -D__BT_ONE_BRING_TWO__
endif

ifeq ($(VOICE_PROMPT),1)
ccflags-y += -DMEDIA_PLAYER_SUPPORT
endif

ifeq ($(TWS),1)
ccflags-y += -D__TWS__
endif


ifeq ($(AUDIO_QUEUE_SUPPORT),1)
CFLAGS_app_bt_media_manager.o += -D__AUDIO_QUEUE_SUPPORT__
endif

ifeq ($(BTADDR_GEN),1)
CFLAGS_besmain.o += -DRANDOM_BTADDR_GEN
endif

ifeq ($(AUDIO_SCO_BTPCM_CHANNEL),1)
ccflags-y += -D_SCO_BTPCM_CHANNEL_
endif

ifeq ($(AUDIO_OUTPUT_MONO),1)
CFLAGS_app_bt_stream.o += -D__AUDIO_OUTPUT_MONO_MODE__
endif

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_app_bt_stream.o += -D__AUDIO_RESAMPLE__
CFLAGS_app_tws_if.o += -D__AUDIO_RESAMPLE__
endif
ifeq ($(SW_PLAYBACK_RESAMPLE),1)
CFLAGS_app_bt_stream.o += -DSW_PLAYBACK_RESAMPLE
endif
ifeq ($(SW_CAPTURE_RESAMPLE),1)
CFLAGS_app_bt_stream.o += -DSW_CAPTURE_RESAMPLE
endif
ifeq ($(SW_SCO_RESAMPLE),1)
CFLAGS_app_bt_stream.o += -DSW_SCO_RESAMPLE
endif
ifeq ($(NO_SCO_RESAMPLE),1)
CFLAGS_app_bt_stream.o += -DNO_SCO_RESAMPLE
endif
ifeq ($(RESAMPLE_ANY_SAMPLE_RATE),1)
CFLAGS_app_bt_stream.o += -DRESAMPLE_ANY_SAMPLE_RATE
CFLAGS_app_tws_if.o += -DRESAMPLE_ANY_SAMPLE_RATE
endif

ifeq ($(BT_XTAL_SYNC),1)
CFLAGS_app_bt_stream.o += -DBT_XTAL_SYNC
endif

ifeq ($(HW_FIR_EQ_PROCESS),1)
CFLAGS_app_bt_stream.o += -D__HW_FIR_EQ_PROCESS__
endif

ifeq ($(SW_IIR_EQ_PROCESS),1)
CFLAGS_app_bt_stream.o += -D__SW_IIR_EQ_PROCESS__
endif

ifeq ($(HW_CODEC_IIR_EQ_PROCESS),1)
CFLAGS_app_bt_stream.o += -D__HW_CODEC_IIR_EQ_PROCESS__
ifeq ($(ANC_TEST),1)
$(error "error: ANC and Hardware codec iir can't be used in the same")
endif
endif


ifeq ($(APP_MUSIC_26M),1)
CFLAGS_app_bt_stream.o += -DAPP_MUSIC_26M
endif

