CHIP		?= best2000

DEBUG		?= 1

FPGA		?= 0

MBED		?= 1

RTOS		?= 1

WATCHER_DOG ?= 0

DEBUG_PORT	?= 1
# 0: usb
# 1: uart0
# 2: uart1

FLASH_CHIP	?= GD25Q80C GD25Q32C GD25LQ32C EN25S80B
# GD25Q80C
# GD25Q32C
#GD25LQ32C
#EN25S80B
# ALL

AUDIO_OUTPUT_MONO ?= 0

AUDIO_OUTPUT_DIFF ?= 0

HW_FIR_EQ_PROCESS ?= 0

SW_IIR_EQ_PROCESS ?= 0

HW_CODEC_IIR_EQ_PROCESS ?= 1

AUDIO_RESAMPLE ?= 1

TWS_SBC_PATCH_SAMPLES ?= 0

AUDIO_OUTPUT_VOLUME_DEFAULT ?= 12
# range:1~17

AUDIO_INPUT_CAPLESSMODE ?= 0

AUDIO_INPUT_LARGEGAIN ?= 0

#AUDIO_RESAMPLE ?= 0

AUDIO_CODEC_ASYNC_CLOSE ?= 1

AUDIO_SCO_BTPCM_CHANNEL ?= 1

AUDIO_EQ_PROCESS ?= 0

SPEECH_SPK_FIR_EQ ?= 0

SPEECH_SIDETONE ?= 0

SPEECH_TX_AEC ?= 1

SPEECH_TX_AEC2 ?= 0

SPEECH_TX_AEC2FLOAT ?= 0

SPEECH_TX_2MIC_NS ?= 0

SPEECH_TX_2MIC_NS2 ?= 0

SPEECH_TX_NS ?= 0

SPEECH_TX_NS2 ?= 1

SPEECH_TX_NS2FLOAT ?= 0

SPEECH_TX_NS3 ?= 0

SPEECH_TX_AGC ?= 0

SPEECH_TX_COMPEXP ?= 1

SPEECH_TX_NOISE_GATE ?= 0

SPEECH_TX_EQ ?= 1

SPEECH_RX_PLC ?= 1

SPEECH_RX_NS ?= 0

SPEECH_RX_NS2 ?= 0

SPEECH_RX_NS2FLOAT ?= 0

SPEECH_RX_NS3 ?= 0

SPEECH_RX_EQ ?= 1

HFP_1_6_ENABLE ?= 1

MSBC_PLC_ENABLE ?= 1

MSBC_PLC_ENCODER ?= 1

MSBC_16K_SAMPLE_RATE ?= 1

SBC_FUNC_IN_ROM ?= 1

VOICE_DETECT ?= 0

VOICE_PROMPT ?= 1

VOICE_RECOGNITION ?= 0

BLE ?= 0

BTADDR_GEN ?= 1

BT_ONE_BRING_TWO ?= 0

A2DP_AAC_ON ?= 1

A2DP_AAC_DIRECT_TRANSFER ?= 1

FACTORY_MODE ?= 1

ENGINEER_MODE ?= 1

ULTRA_LOW_POWER	?= 1

DAC_CLASSG_ENABLE ?= 1

NO_SLEEP ?= 0

export POWER_MODE	?= DIG_DCDC

ifeq ($(CURRENT_TEST),1)
export VCODEC_VOLT ?= 1.5V

export VCORE_LDO_OFF ?= 1
else
export VCODEC_VOLT ?= 1.6V
endif

export VCRYSTAL_OFF ?= 1

export TWS   ?= 1

export JTAG_BT ?= 1
export BT_MIC_GAIN_CTRL   ?= 0

OSC_26M_X4_AUD2BB ?= 1

export SW_PLAYBACK_RESAMPLE ?= 0
export RESAMPLE_ANY_SAMPLE_RATE ?= 0

HCI_DEBUG ?= 0
ifeq ($(HCI_DEBUG),1)
KBUILD_CPPFLAGS += -DHCI_DEBUG
KBUILD_CPPFLAGS += -D__HCI_TRACE_BUFFER__NUMBER__
endif


ifneq ($(SW_PLAYBACK_RESAMPLE),1)
export APP_MUSIC_26M ?= 1
endif

init-y		:=
core-y		:= platform/ services/ apps/ utils/cqueue/ utils/list/ services/multimedia/ utils/intersyshci/
KBUILD_CPPFLAGS += -Iplatform/cmsis/inc -Iservices/audioflinger -Iplatform/drivers/codec -Iplatform/hal -Iservices/fs/ -Iservices/fs/sd -Iservices/fs/fat  -Iservices/fs/fat/ChaN -Iplatform/drivers/norflash

KBUILD_CPPFLAGS += \
		-D_PROJ_2000IZ_C001__ \
		-D_SIRI_ENABLED__ \
		-D_AUTO_SWITCH_POWER_MODE__ \
        -D_BEST1000_QUAL_DCDC_ \
        -D__RESET_WHEN_CHANGER_PLUGOUT__ \
        -D__POWERKEY_CTRL_ONOFF_ONLY__ \
        -D__DIFF_HANDLE_FOR_REMOTE_KEY_ \
        -D__POWER_OFF_AFTER_PAIR_TIMEOUT__ \
        -D__DEEP_SLEEP_FAKE_POWER_OFF__ \
        -D__TWS_RIGHT_AS_MASTER__  \
        -D__LIAC_FOR_TWS_PAIRING__\
        -D__HALL_CONTROL_POWER_ \
        -D__SINGLE_TOUCH_SIMPLE_MODE_WITH_TRICPLE_ \
        -D__TWS__  \
        -D__TWS_RECONNECT_USE_BLE__  \
        -D__TWS_OUTPUT_CHANNEL_SELECT__  \
        -D__EARPHONE_STAY_BCR_SLAVE__ \
        -DSLAVE_USE_ENC_SINGLE_SBC    \
        -D__TWS_CALL_DUAL_CHANNEL__  \
        -DBT_XTAL_SYNC   \
		-D__TWS_1CHANNEL_PCM__\
    	-D__DISABLE_THREEWAY_CALL_CONTROL_\
    	-D__ALWAYS_ALLOW_ENTER_TEST_MODE_\
    	-D__AUDIO_SKIP_ONLY_WHEN_STREAMING_\
    	-D__FUNCTION_KEY_TRIPLE_CLICK_DURING_CHARGE_FOR_ENTER_TEST_MODE_\
    	-D__FUNCTION_KEY_RAMPAGE_CLICK_DURING_CHARGE_FOR_OTA_\
		-D__FUNCTION_KEY_SEVEN_CLICK_DURING_CHARGE_FOR_VERSION_CHECK_\
    	-D__FACTORY_KEY_SUPPORT_\
    	-D__UP_KEY_DOUBLE_CLICK_FOR_PAIRING_\
    	-D__UP_KEY_TRIPLE_CLICK_FOR_RESET_PAIRED_DEVICE_LIST_\
    	-D__UP_KEY_LONG_PRESS_FOR_ENTER_DUT_\
    	-D__OPEN_SIRI_WHEN_CALL__\
    	-D__HW_PWM_CONTROL_LED__\
    	-D__DISABLE_BATTERY_LOW_LED_INDICATION__\
    	-D__OPEN_AUDIO_LOOP_IN_DUT_MODE_\
    	-D__1_MB_CODESIZE_OTA__\
    	-D__DUAL_USER_SECTION_BAK__\
	-DRING_MERGE_POST_HANDLE\
	-D__BT_WARNING_TONE_MERGE_INTO_STREAM_SBC__\
	-D_TWS_MASTER_DROP_DATA_\
		-D__SAVE_FW_VERSION_IN_NV_FLASH__\
		-D__ACC_FRAGMENT_COMPATIBLE__\
		-D__TWS_CLK_SYNC__\
		-D__FORCE_ENABLE_AAC__\
		-DFLASH_REGION_BASE=0x3C018000
    	
KBUILD_CPPFLAGS += \
        				-D__TWS_CHANNEL_LEFT__\
        				-D__DISABLE_SLAVE_OPENING_RECONNECT_PHONE_\
        				-D__FUNCTION_KEY_LONG_LONG_PRESS_FOR_TWS_PAIRING_\
        				-D__FUNCTION_KEY_TRIPLE_CLICK_CLEAR_PHONE_RECORD_\
        				-D__FUNCTION_KEY_DOUBLE_CLICK_FOR_SLAVE_ENTER_PAIRING_\
    					-D__TWS_AUTO_POWER_OFF_WHEN_DISCONNECTED__					

#KBUILD_CPPFLAGS += \
        				-D__TWS_CHANNEL_RIGHT__  \
        				-D__EARPHONE_STAY_BOTH_SCAN__  \
        				-D__MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__\
        				-D__FUNCTION_KEY_CLICK_FOR_MUSIC_CTRL_\
        				-D__FUNCTION_KEY_DOUBLE_CLICK_FOR_PAIRING_\
        				-D__FUNCTION_KEY_CLICK_FOR_HF_CALL_\
        				-D__FUNCTION_REMOTE_KEY_CLICK_FOR_HF_CALL_\
        				-D__FUNCTION_KEY_REPEAT_FOR_VOL_UP_\
        				-D__FUNCTION_REMOTE_KEY_REPEAT_FOR_VOL_DOWN_\
        				-D__FUNCTION_KEY_DOUBLE_CLICK_FOR_SKIPF_\
        				-D__FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_SKIPR_\
        				-D__FUNCTION_KEY_TRIPLE_CLICK_FOR_VOICE_DIAL_\
        				-D__FUNCTION_REMOTE_KEY_TRIPLE_CLICK_FOR_VOICE_DIAL_\
        				-D__FUNCTION_KEY_LONG_LONG_PRESS_FOR_TWS_SEARCHING_\
        				-D__FUNCTION_KEY_LONG_LONG_PRESS_FOR_HFP_REJECT_CALL_\
        				-D__FUNCTION_REMOTE_KEY_LONG_LONG_PRESS_FOR_HFP_REJECT_CALL_\
        				-D__FUNCTION_KEY_LONG_LONG_PRESS_DURING_CHARGE_FOR_RESET_PHONE_PDL_ \
        				-D__HF_ATCMD_SEND_

					
#KBUILD_CPPFLAGS += \
						-D__PC_CMD_UART__

#-D_FORCE_TO_LIMIT_MAX_AAC_BITRATE_192K_\						
#-DADJUST_BITPOOL_BETWEEN_TWS							
#-D__LINK_LOSE_TIMEOUT_THEN_ENTER_PAIRING__\	
#-D__MASTER_AUTO_TWS_SEARCHING_WITH_EMPTY_PDL__\					
#-D__SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__\	
#-D__SLAVE_AUTO_PAIRING_WITH_EMPTY_PDL_\
#-D__TWS_CLK_SYNC__
#-D__3M_PACK__
#-D__HW_PWM_CONTROL_LED__\
#-D_AUTO_SWITCH_POWER_MODE__
#-D_BEST1000_QUAL_DCDC_
#-D__APP_KEY_FN_STYLE_A__
#-D__APP_KEY_FN_STYLE_B__
#-D__BT_ONE_BRING_TWO__
#-D__EARPHONE_STAY_BOTH_SCAN__
#-D__POWERKEY_CTRL_ONOFF_ONLY__
#-DFLASH_REGION_BASE=0x6C018000
#-D__TWS_1CHANNEL_PCM__
#-D__TWS_RESAMPLE_ADJUST__
#-D__TWS_FORCE_CRC_ERROR__
#-D__TWS_CLK_SYNC__\
LDS_FILE	:= best1000.lds

KBUILD_CPPFLAGS +=

KBUILD_CFLAGS +=

LIB_LDFLAGS += -lstdc++ -lsupc++

#CFLAGS_IMAGE += -u _printf_float -u _scanf_float

#LDFLAGS_IMAGE += --wrap main

