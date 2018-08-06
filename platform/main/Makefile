cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

#obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))
obj-y :=

ifeq ($(ROM_BUILD),1)
obj-y += ../../tests/rom/startup_ARMCM4.S
else
obj-y += startup_main.S
endif

ifeq ($(FACTORY_SUITE),1)
obj-y += ../../tests/factory_suite/
else
ifeq ($(RELIABILITY_TEST),1)
obj-y += ../../tests/reliability_test/
else
ifeq ($(NOSTD),1)
asflags-y += -DNOSTD
obj-y += nostd_main.c
else
ifeq ($(NOAPP),1)
obj-y += noapp_main.cpp
else
obj-y += main.cpp
endif
endif
endif
endif

ifneq ($(filter 1,$(USB_SERIAL_TEST) $(USB_AUDIO_TEST)),)
obj-y += ../../tests/hwtest/usb_dev_test.c
ccflags-y += -Itests/hwtest
ifeq ($(USB_SERIAL_TEST),1)
ccflags-y += -DUSB_SERIAL_TEST
else
ccflags-y += -DUSB_AUDIO_TEST
endif
endif

obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

obj-y += ../../utils/hwtimer_list/

ccflags-y +=  \
	-Iutils/hwtimer_list \
	-Iplatform/drivers/ana \
	-Iplatform/drivers/usb/usb_dev/inc \
	-Iapps/main \
	-Iapps/factory \
	-Iutils/list  \
	-Itests/rom1 \
	-Iapps/audioplayers  \
	-Iservices/bt_app  \
	-Iapps/common  \
	-Iservices/bt_profiles/inc \
	-Iplatform/drivers/norflash

ifneq ($(FLASH_FILL),)
ccflags-y += -DFLASH_FILL=$(FLASH_FILL)
endif

ifneq ($(DEBUG_PORT),)
ccflags-y += -DDEBUG_PORT=$(DEBUG_PORT)
endif

