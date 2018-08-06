
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_c := $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)*.c))

libbridge-y := $(obj_c:.c=.o)

obj-y := libbridge.a

subdir-ccflags-y += \
	-Iservices/bridge/ \
	-Iservices/bt_profiles/inc/ \
	-Iservices/bt_profiles/inc/sys/ \
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
	-Iservices/bluedroid/adapter/src/ \

