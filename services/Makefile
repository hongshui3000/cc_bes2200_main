obj-y := audioflinger/ bt_profiles/ fs/ bt_app/ overlay/ resources/ ../utils/crc32/
ifeq ($(BLE),1)
obj-y += bluedroid/ bridge/ ble_app/
endif

ifeq ($(BT_ANC),1)
obj-y += nv_section/
endif

ifneq ($(FPGA),1)
obj-y += nvrecord/
endif

ifneq ($(filter 1, $(HW_FIR_EQ_PROCESS) $(SW_IIR_EQ_PROCESS) $(HW_CODEC_IIR_EQ_PROCESS) $(AUDIO_DRC)),)
obj-y += audio_eq/
endif

ifeq ($(VOICE_RECOGNITION),1)
obj-y += voice_recognition/
endif


ifeq ($(TWS),1)
obj-y += tws/
endif