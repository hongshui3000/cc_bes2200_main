cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y += ../../tests/anc_usb/anc_test.c
obj-y += ../../tests/anc_usb/usb_audio_test.c
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

subdir-ccflags-y += \
	-Iutils/cqueue/ \
	-Iplatform/drivers/ana/ \
	-Iservices/multimedia/rbcodec/ \
	-Iservices/multimedia/rbcodec/dsp/ \
	-Iservices/multimedia/rbcodec/dsp/common \
	-Iservices/multimedia/rbcodec/codecs/ \
	-Iservices/multimedia/rbcodec/codecs/lib \
	-Iservices/multimedia/rbcodec/test/ \
	-Iservices/multimedia/rbcodec/metadata/ \
	-Iservices/multimedia/audio/process/anc/include \
	-Iplatform/drivers/usb/usb_dev/inc \
	-Itests/anc_usb

CFLAGS_usb_audio_test.o += -DAUDIO_OUTPUT_VOLUME_DEFAULT=$(AUDIO_OUTPUT_VOLUME_DEFAULT)

ifeq ($(APP_TEST_AUDIO),1)
ccflags-y += -DAPP_TEST_AUDIO
endif

ANC_USB_CFG_FLAGS :=

# ANC option
ifeq ($(ANC_TEST),1)
ANC_USB_CFG_FLAGS += -DANC_TEST
endif

# USB audio option
ifeq ($(USB_AUDIO_TEST),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_TEST
endif

ifeq ($(USB_HIGH_SPEED),1)
ANC_USB_CFG_FLAGS += -DUSB_HIGH_SPEED
endif

ifeq ($(AUDIO_INPUT_MONO),1)
ANC_USB_CFG_FLAGS += -DAUDIO_INPUT_MONO
endif

ifeq ($(AUDIO_RESAMPLE),1)
ANC_USB_CFG_FLAGS += -D__AUDIO_RESAMPLE__
endif

ifeq ($(ADC_CH_SEP_BUFF),1)
ANC_USB_CFG_FLAGS += -DADC_CH_SEP_BUFF
endif

# USB audio configuration
ifeq ($(USB_AUDIO_DYN_CFG),1)

ANC_USB_CFG_FLAGS += -DUSB_AUDIO_DYN_CFG

else # USB_AUDIO_DYN_CFG != 1

ifeq ($(USB_AUDIO_192K),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_192K
else
ifeq ($(USB_AUDIO_96K),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_96K
else
ifeq ($(USB_AUDIO_44_1K),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_44_1K
endif
endif
endif

ifeq ($(USB_AUDIO_24BIT),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_24BIT
else
ifeq ($(USB_AUDIO_32BIT),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_32BIT
endif
endif

endif # USB_AUDIO_DYN_CFG != 1

ifeq ($(USB_AUDIO_MIC_MONO),1)
ANC_USB_CFG_FLAGS += -DUSB_AUDIO_MIC_MONO
endif

ifeq ($(AUDIO_PLAYBACK_32BIT),1)
ANC_USB_CFG_FLAGS += -DAUDIO_PLAYBACK_32BIT
endif

# EQ configuration
ifeq ($(HW_FIR_EQ_PROCESS),1)
ANC_USB_CFG_FLAGS += -D__HW_FIR_EQ_PROCESS__
endif

ifeq ($(SW_IIR_EQ_PROCESS),1)
ANC_USB_CFG_FLAGS += -D__SW_IIR_EQ_PROCESS__
endif

ifeq ($(ANC_EQ_TYPE),1)
ifeq ($(SW_IIR_EQ_PROCESS),1)
ANC_USB_CFG_FLAGS += -D__ANC_EQ_TYPE__=1
else
$(error "error: ANC_EQ_TYPE=1, but SW_IIR_EQ_PROCESS=0")
endif
endif

ifeq ($(ANC_EQ_TYPE),2)
ifeq ($(HW_FIR_EQ_PROCESS),1)
ANC_USB_CFG_FLAGS += -D__ANC_EQ_TYPE__=2
else
$(error "error: ANC_EQ_TYPE=2, but HW_FIR_EQ_PROCESS=0")
endif
endif

CFLAGS_app_audtest.o += $(ANC_USB_CFG_FLAGS)
CFLAGS_usb_audio_test.o += $(ANC_USB_CFG_FLAGS)
CFLAGS_anc_test.o += $(ANC_USB_CFG_FLAGS)

ifeq ($(DELAY_STREAM_OPEN),1)
CFLAGS_usb_audio_test.o += -DDELAY_STREAM_OPEN
endif

ifeq ($(ANC_FF_ENABLED),1)
CFLAGS_anc_test.o += -DANC_FF_ENABLED
endif

ifeq ($(ANC_FB_ENABLED),1)
CFLAGS_anc_test.o += -DANC_FB_ENABLED
endif

ifeq ($(ANC_KEY_DOUBLE_CLICK_ON_OFF),1)
CFLAGS_anc_test.o += -DANC_KEY_DOUBLE_CLICK_ON_OFF
endif

ifeq ($(NOISE_GATING),1)
CFLAGS_usb_audio_test.o += -DNOISE_GATING
endif

ifeq ($(NOISE_REDUCTION),1)
CFLAGS_usb_audio_test.o += -DNOISE_REDUCTION
endif

ifeq ($(ANC_L_R_MISALIGN_WORKAROUND),1)
CFLAGS_usb_audio_test.o += -DANC_L_R_MISALIGN_WORKAROUND
endif

ifeq ($(ANDROID_ACCESSORY_SPECIFICATION),1)
CFLAGS_usb_audio_test.o += -DANDROID_ACCESSORY_SPECIFICATION
endif

ifeq ($(DUAL_AUX_MIC_MORE_FILTER),1)
CFLAGS_usb_audio_test.o += -DDUAL_AUX_MIC_MORE_FILTER
endif

ifeq ($(FREQ_RESP_EQ),1)
CFLAGS_usb_audio_test.o += -DFREQ_RESP_EQ
endif

ifeq ($(AUDIO_SECTION_SUPPT),1)
CFLAGS_anc_test.o += -D__AUDIO_SECTION_SUPPT__
endif

