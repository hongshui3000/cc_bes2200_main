
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/adapter/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/bta/dm/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/bta/gatt/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/bta/sys/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/btif/co/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/btif/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/gki/common/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/gki/ulinux/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/hardware/drv/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/hci/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/main/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/osinone/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/btm/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/btu/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/gap/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/gatt/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/hcic/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/l2cap/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/stack/smp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/utils/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)/vnd/ble/*.c))

libbluedroid-y := $(obj_c:.c=.o)

obj-y := libbluedroid.a

subdir-ccflags-y += \
	-Iservices/bluedroid/bta/dm/ \
	-Iservices/bluedroid/bta/gatt/ \
	-Iservices/bluedroid/bta/include/ \
	-Iservices/bluedroid/bta/sys/ \
	-Iservices/bluedroid/btif/include/ \
	-Iservices/bluedroid/gki/common/ \
	-Iservices/bluedroid/gki/ulinux/ \
	-Iservices/bluedroid/hardware/ \
	-Iservices/bluedroid/hci/include/ \
	-Iservices/bluedroid/include/ \
	-Iservices/bluedroid/osinone/include/ \
	-Iservices/bluedroid/stack/btm/ \
	-Iservices/bluedroid/stack/gap/ \
	-Iservices/bluedroid/stack/gatt/ \
	-Iservices/bluedroid/stack/include/ \
	-Iservices/bluedroid/stack/l2cap/ \
	-Iservices/bluedroid/stack/smp/ \
	-Iservices/bluedroid/stack/srvc/ \
	-Iservices/bluedroid/utils/include/ \
	-Iservices/bluedroid/vnd/ble/ \
	-Iservices/bluedroid/vnd/include/ \
	-Iservices/bridge/ \
	-Iservices/bt_profiles/inc/ \
	-Iservices/bt_profiles/inc/sys/ \
	-Irtos/rtx/TARGET_CORTEX_M/ \
	-Iutils/jansson/ \
	-Iutils/list/ \
	-Iutils/hwtimer_list \

