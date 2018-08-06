cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj-y := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c $(cur_dir)*.cpp $(cur_dir)*.S))

ifneq ($(wildcard $(cur_dir)../../config/$(T)/app_status_ind.c),)
obj-y += ../../config/$(T)/app_status_ind.c
else
obj-y += ../../config/$(DEFAULT_CFG_SRC)/app_status_ind.c
endif

obj-y := $(obj-y:.c=.o)
obj-y := $(obj-y:.cpp=.o)
obj-y := $(obj-y:.S=.o)

ccflags-y +=  \
    -Iservices/bt_profiles/inc \
    -Iservices/bt_profiles \
    -Iservices/bt_app \
    -Iservices/audioflinger \
    -Iservices/nvrecord \
    -Iservices/overlay \
    -Iservices/resources \
    -Iapps/apptester \
    -Iapps/factory \
    -Iutils/crc32 \
    -Iutils/cqueue \
    -Iplatform/drivers/bt \
    -Iapps/audioplayers/rbplay \
    -Itests/anc_usb \
    -Iservices/tws/inc \
    -Iapps/anc/inc

ifeq ($(APP_TEST_AUDIO),1)
CFLAGS_apps_tester.o += -DAPP_TEST_AUDIO
endif

ifeq ($(ANC_TEST),1)
CFLAGS_apps_tester.o += -DANC_TEST
endif

ifeq ($(VOICE_PROMPT),1)
CFLAGS_apps.o += -DMEDIA_PLAYER_SUPPORT
endif

ifeq ($(ENGINEER_MODE),1)
CFLAGS_apps.o += -D__ENGINEER_MODE_SUPPORT__
endif

ifeq ($(MCU_HIGH_PERFORMANCE_MODE),1)
CFLAGS_apps.o += -DMCU_HIGH_PERFORMANCE_MODE
endif

