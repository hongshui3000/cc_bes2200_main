CHIP		?= best2000

DEBUG		?= 1

NOSTD		?= 1

LIBC_ROM	?= 1

PROGRAMMER	:= 1

FAULT_DUMP	?= 0

init-y		:= 
core-y		:= tests/programmer/ platform/cmsis/ platform/hal/

LDS_FILE	:= programmer.lds

KBUILD_CPPFLAGS += -Iplatform/cmsis/inc -Iplatform/hal

KBUILD_CFLAGS +=

LIB_LDFLAGS +=

CFLAGS_IMAGE +=

LDFLAGS_IMAGE +=

