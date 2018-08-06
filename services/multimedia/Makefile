
cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

obj_c :=
obj_cpp :=
obj_s :=

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/common/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_dc_filter/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_peak_detector/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/resample/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/plc/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/ns_and_ec/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/application/Pcm8kCvsd/src/*.c))
#obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/signal_processing/src/*.c))

obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/common/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_dc_filter/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_peak_detector/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/resample/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/plc/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/ns_and_ec/src/*.cpp))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/application/Pcm8kCvsd/src/*.cpp))
#obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/signal_processing/src/*.cpp))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/anc/src/$(CHIP)/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/anc/src/$(CHIP)/*.cpp))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/adp/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/adp/src/*.cpp))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio_dump/src/*.c))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/lc_mmse_ns/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/lc_mmse_ns/src/*.cpp))

# ec2 float
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/ec2_float/src/*.c))

# speech resample
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/iir_resample/src/*.c))

# speech eq
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_eq/src/*.c))

# ns3
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_process_lib/modules/audio_processing/ns/noise_suppression_x.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_process_lib/modules/audio_processing/ns/nsx_core.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_process_lib/modules/audio_processing/ns/nsx_core_c.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_process_lib/common_audio/signal_processing/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/ns3/src/*.c))

# noise gate
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_noise_gate/src/*.c))

# compexp
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/compexp/src/*.c))

# dual mic denoise2
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/coherent_denoise/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/coherent_denoise/src/*.cpp))

# ifeq ($(SPEECH_IIR),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_iir/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_iir/src/*.cpp))
# endif

ifeq ($(VOICE_DETECT),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech_process_lib/src/common_audio/vad/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech_process_lib/src/common_audio/signal_processing/division_operations.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech_process_lib/src/common_audio/signal_processing/energy.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech_process_lib/src/common_audio/signal_processing/get_scaling_square.c))
endif

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/resample/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/resample/src/*.cpp))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/filters/src/$(CHIP)/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/filters/src/$(CHIP)/*.cpp))

ifeq ($(USB_AUDIO_TEST),1)
obj_c := $(filter-out %/iir_process_$(CHIP).c,$(obj_c))
CFLAGS_fir_process.o += -DFIR_USE_IRQ -DFIR_USE_PINGPONG
else
obj_c := $(filter-out %/iir_process_usb_$(CHIP).c,$(obj_c))
endif

ifeq ($(AUDIO_RESAMPLE),1)
CFLAGS_anc_process_best2000.o += -D__AUDIO_RESAMPLE__
CFLAGS_anc_cfg.o += -D__AUDIO_RESAMPLE__
endif


obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/drc/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/drc/src/*.cpp))

ifeq ($(AUDIO_EQ_PROCESS),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/dsp/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/dsp/common/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/dsp/common/*.cpp))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/metadata/metadata*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/metadata/replaygain.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/eq.cpp))
obj_s += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/dsp/filter_process.S))
endif

ifeq ($(RB_CODEC),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs.c))
# codec lib
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/lib/codeclib.c))
# mpa
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/metadata/id3tags.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/metadata/mp3*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/mpa.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/libmad/*.c))
# wav
#obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/metadata/wave.c))
#obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/wav.c))
#obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/libpcm/*.c))
endif

# aac
ifeq ($(A2DP_AAC_ON),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)rbcodec/codecs/libhelixaac/*.c))
endif

ifeq ($(SPEECH_TX_2MIC_NS),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/dual_mic_denoise/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/vad/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_drc/src/*.c))
endif

ifeq ($(SPEECH_TX_2MIC_NS3),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/far_field_speech_enhancement/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/vad/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_drc/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)algorithm/fft/src/*.c))
else
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)algorithm/fft/src/*.cpp))
endif

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)fm/src/*.c))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/recognition/src/*.c))
obj_cpp += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/recognition/src/*.cpp))

obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_process_lib/modules/audio_processing/agc/legacy/digital_agc.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/agc/src/agc.c))

ifeq ($(SPEECH_TX_AEC2),1)
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_aec2/src/*.c))
obj_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)speech/process/speech_aec_fixed/src/*.c))
endif

src_obj := $(obj_c:.c=.o) $(obj_s:.S=.o) $(obj_cpp:.cpp=.o)


	


MULTIMEDIA_LIB_NAME := libmultimedia

$(MULTIMEDIA_LIB_NAME)-y := $(src_obj)

rel_src_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/anc/cfg/*.c))
rel_src_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/filters/cfg/*.c))
rel_src_c += $(patsubst $(cur_dir)%,%,$(wildcard $(cur_dir)audio/process/resample/coef/*.c))

rel_src_obj := $(rel_src_c:.c=.o)

obj-y := $(MULTIMEDIA_LIB_NAME).a $(rel_src_obj)

ccflags-y := \
	-Iplatform/cmsis/inc \
	-Iplatform/drivers/ana \
	-Irtos/rtos \
	-Iplatform/dsp/ \
	-Irtos/rtx/TARGET_CORTEX_M \
	-Iservices/multimedia/speech/process/speech_aec2/include \
	-Iservices/multimedia/speech/process/speech_aec_fixed/include \
	-Iservices/multimedia/speech/process/speech_process_lib \
	-Iservices/multimedia/speech/process/common/include \
	-Iservices/multimedia/speech/process/speech_iir/include \
	-Iservices/multimedia/speech/process/speech_dc_filter/include \
	-Iservices/multimedia/speech/process/speech_peak_detector/include \
	-Iservices/multimedia/speech/process/speech_drc/include \
	-Iservices/multimedia/speech/process/resample/include \
	-Iservices/multimedia/speech/process/plc/include \
	-Iservices/multimedia/speech/process/ns_and_ec/include \
	-Iservices/multimedia/speech/process/ns_and_ec/speex \
	-Iservices/multimedia/speech/process/recognition/include \
	-Iservices/multimedia/speech/process/ec2_float/include \
	-Iservices/multimedia/speech/process/iir_resample/include \
	-Iservices/multimedia/speech/process/speech_eq/include \
	-Iservices/multimedia/speech/process/agc/include \
	-Iservices/multimedia/speech/process/ns3/include \
	-Iservices/multimedia/speech/process/speech_noise_gate/include \
	-Iservices/multimedia/speech/process/compexp/include \
	-Iservices/multimedia/speech/process/lc_mmse_ns/include \
	-Iservices/multimedia/speech/process/coherent_denoise/include \
	-Iservices/multimedia/speech/process/dual_mic_denoise/include \
	-Iservices/multimedia/speech/process/far_field_speech_enhancement/include \
	-Iservices/multimedia/speech/process/vad/include \
	-Iservices/multimedia/speech/codec/adpcm \
	-Iservices/multimedia/speech/application/Pcm8kCvsd/include \
	-Iservices/multimedia/speech/codec/codecs/cvsd/include \
	-Iservices/multimedia/rbcodec \
	-Iservices/multimedia/rbcodec/codecs \
	-Iservices/multimedia/rbcodec/codecs/lib \
	-Iservices/multimedia/rbcodec/dsp \
	-Iservices/multimedia/rbcodec/dsp/common \
	-Iservices/multimedia/rbcodec/test \
	-Iservices/multimedia/rbcodec/metadata \
	-Iservices/multimedia/audio_dump/include \
	-Iservices/multimedia/audio/process/anc/include \
	-Iservices/multimedia/audio/process/adp/include \
	-Iservices/multimedia/audio/process/resample/include \
	-Iservices/multimedia/audio/process/filters/include \
	-Iservices/multimedia/audio/process/filters/cfg \
	-Iservices/multimedia/audio/process/drc/include \
	-Iservices/multimedia/algorithm/fft/include \
	-Iservices/multimedia/fm/inc \
	-Iservices/nv_section/aud_section \
	-Iservices/nv_section/include \
	-Iservices/bt_profiles/inc \

# ec2
ccflags-y += -DVOS_ARM -D_MED_C89_ -DUSE_MED_MALLOC -DUSE_DSPFNS -DMED_USE_KISS_FFT -DSPEECH_STREAM_UNIT_128

# ec2 float
ccflags-y += -DUSE_MED_MALLOC -DUSE_CMSIS_F32_FFT

# ns3
ccflags-y += -DUSE_MED_MALLOC -D__ARMEL__ -DUSE_FFTWRAP -DWEBRTC_ARCH_ARM_V7


ccflags-y += -DFLAC__INTEGER_ONLY_LIBRARY -DVERSION="\"1.3.1\"" -DHAVE_CONFIG_H

ifeq ($(RESAMPLE_ANY_SAMPLE_RATE),1)
CFLAGS_audio_resample_ex.o += -DRESAMPLE_ANY_SAMPLE_RATE
ifeq ($(RESAMPLE_ANY_SAMPLE_RATE_ACCURATE),1)
CFLAGS_audio_resample_ex.o += -DRESAMPLE_ANY_SAMPLE_RATE_ACCURATE
endif
endif

