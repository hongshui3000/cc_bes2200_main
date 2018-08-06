
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_s := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.s))
obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)add-ins/at/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)add-ins/sync/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hcitrans/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)hcitrans/modinit/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)besport/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/*.c))

ifeq ($(OPUS_ON),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/celt/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/celt/fixed/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/silk/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/a2dp/opus113/silk/fixed/*.c))
endif

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avctp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avdtp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/avrcp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/common/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/hf/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/spp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/hci/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/l2cap/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/me/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/rfcomm/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)stack/sdp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)profiles/hid/*.c))

obj_cpp := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.cpp))




BTPROFILE_LIB_NAME := libbt_profiles

ifeq ($(OPUS_ON),1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_opus
endif

ifeq ($(HFP_1_6_ENABLE), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_sbc_enc
endif
 
ifeq ($(SBC_FUNC_IN_ROM), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_sbc_in_rom
endif

ifeq ($(A2DP_AAC_ON), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_aac
endif

ifeq ($(A2DP_AAC_DIRECT_TRANSFER), 1)
BTPROFILE_LIB_NAME := $(BTPROFILE_LIB_NAME)_dir_trans
endif


$(BTPROFILE_LIB_NAME)-y := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)

obj-y += $(BTPROFILE_LIB_NAME).a

CFLAGS_a2alloc.o += -O3
CFLAGS_a2dp.o += -O3
CFLAGS_a2dp_sdp.o += -O3
CFLAGS_a2dp_sm.o += -O3
CFLAGS_sbc.o += -O3
CFLAGS_sbc_math.o += -O3

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

subdir-ccflags-y += \
    -Iservices/fs/fat \
    -Iservices/fs/sd \
    -Iservices/fs/fat/ChaN \
    -Iservices/bt_profiles/inc \
    -Iplatform/drivers/uarthci \
    -Iplatform/drivers/ana \
    -Iplatform/drivers/bt \
    -Itests/rom \
    -Iutils/cqueue \
    -Iservices/bt_app \
    -Iservices/tws/inc \
    -Iservices/audioflinger \
    -Iservices/nvrecord \
    -Iutils/lockcqueue \
    -Iapps/key \
	$(OPUS_INCLUDES) \
    -Iapps/audioplayers

