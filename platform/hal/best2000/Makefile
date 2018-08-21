cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))
obj-y := $(obj-y:.c=.o)

CFLAGS_hal_cmu_best2000.o += $(CMU_CFG_FLAGS)

CFLAGS_hal_codec_best2000.o += $(CODEC_CFG_FLAGS)

CFLAGS_hal_iomux_best2000.o += $(IOMUX_CFG_FLAGS)

ifeq ($(USB_HIGH_SPEED),1)
CFLAGS_hal_cmu_best2000.o += -DUSB_HIGH_SPEED
endif

ifeq ($(ANA_26M_X4_ENABLE),1)
CFLAGS_hal_cmu_best2000.o += -DANA_26M_X4_ENABLE
endif

ifeq ($(OSC_26M_X4_AUD2BB),1)
CFLAGS_hal_cmu_best2000.o += -DOSC_26M_X4_AUD2BB
endif

ifeq ($(USB_CLK_SRC_104M),1)
CFLAGS_hal_cmu_best2000.o += -DUSB_CLK_SRC_104M
else
ifeq ($(USB_CLK_SRC_52M),1)
CFLAGS_hal_cmu_best2000.o += -DUSB_CLK_SRC_52M
endif
endif

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_hal_codec_best2000.o += -D__AUDIO_RESAMPLE__
endif

ifeq ($(DAC_CLASSG_ENABLE),1)
CFLAGS_hal_codec_best2000.o += -DDAC_CLASSG_ENABLE
endif

ifeq ($(DAC_DRE_ENABLE),1)
CFLAGS_hal_codec_best2000.o += -DDAC_DRE_ENABLE
endif

ifeq ($(JTAG_BT),1)
CFLAGS_hal_psc_best2000.o += -DJTAG_BT
endif

ifeq ($(JTAG_WIFI),1)
CFLAGS_hal_psc_best2000.o += -DJTAG_WIFI
endif

ifeq ($(AUDIO_OUTPUT_DIFF),1)
CODEC_CFG_FLAGS += -DAUDIO_OUTPUT_DIFF
endif

