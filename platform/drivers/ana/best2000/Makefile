cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.S))
obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.S=.o)

CFLAGS_pmu_best2000.o += $(PMU_CFG_FLAGS)

CFLAGS_analog_best2000.o += $(ANA_CFG_FLAGS)

CFLAGS_usbphy_best2000.o += $(USBPHY_CFG_FLAGS)

CFLAGS_pmu_best2000.o += -Iplatform/drivers/norflash

ifeq ($(USB_HIGH_SPEED),1)
CFLAGS_pmu_best2000.o += -DUSB_HIGH_SPEED
endif

ifeq ($(ANA_26M_X4_ENABLE),1)
CFLAGS_pmu_best2000.o += -DANA_26M_X4_ENABLE
endif
ifeq ($(OSC_26M_X4_AUD2BB),1)
CFLAGS_pmu_best2000.o += -DOSC_26M_X4_AUD2BB
endif

ifeq ($(VCORE_LDO_OFF),1)
CFLAGS_pmu_best2000.o += -DVCORE_LDO_OFF
endif

ifeq ($(VANA_DEC_50MV),1)
CFLAGS_pmu_best2000.o += -DVANA_DEC_50MV
endif

LOW_POWER_ADC ?= 1
ifeq ($(LOW_POWER_ADC),1)
CFLAGS_analog_best2000.o += -DLOW_POWER_ADC
endif

ifeq ($(VCRYSTAL_OFF),1)
CFLAGS_pmu_best2000.o += -DVCRYSTAL_OFF
endif



