
#include "plat_addr_map.h"

#define HEAP_SECTION_SIZE			0x40

#define STACK_SECTION_SIZE			0xD00

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#define OVERLAY_DATA_SECTION_SIZE	0xA000
#else
#define OVERLAY_DATA_SECTION_SIZE	0x8000
#endif

#if defined(CHIP_BEST2000)
#if defined(BTUSB_AUDIO_MODE)
#define FAST_XRAM_SECTION_SIZE		0x18000
#else
#define FAST_XRAM_SECTION_SIZE		0x8000
#endif
#else
#define FAST_XRAM_SECTION_SIZE		0x8000
#endif

#ifndef FLASH_REGION_BASE
#define FLASH_REGION_BASE			FLASH_BASE
#endif
#ifndef FLASH_REGION_SIZE
#define FLASH_REGION_SIZE			(FLASH_SIZE - (FLASH_REGION_BASE - FLASH_BASE))
#endif
#define FLASH_NC_REGION_BASE		FLASH_C_TO_NC(FLASH_REGION_BASE)
#define FLASHX_REGION_BASE			FLASH_TO_FLASHX(FLASH_REGION_BASE)

/* Linker script to configure memory regions. */
/* See plat_addr_map.h and common.mk for the macro definitions */
MEMORY
{
	ROM			(rx)	: ORIGIN = ROM_BASE,				LENGTH = ROM_SIZE
	FLASH 		(r)		: ORIGIN = FLASH_REGION_BASE,		LENGTH = FLASH_REGION_SIZE
	FLASH_NC	(r)		: ORIGIN = FLASH_NC_REGION_BASE,	LENGTH = FLASH_REGION_SIZE
	FLASHX		(rx)	: ORIGIN = FLASHX_REGION_BASE,		LENGTH = FLASH_REGION_SIZE
	RAM			(rwx)	: ORIGIN = RAM_BASE,				LENGTH = RAM_SIZE - FAST_XRAM_SECTION_SIZE
	RAMX		(rx)	: ORIGIN = RAMX_BASE,				LENGTH = RAM_SIZE - FAST_XRAM_SECTION_SIZE
	FRAMX		(rwx)	: ORIGIN = RAMX_BASE + RAM_SIZE - FAST_XRAM_SECTION_SIZE,	LENGTH = FAST_XRAM_SECTION_SIZE
	PSRAM 		(rwx)	: ORIGIN = PSRAM_BASE,				LENGTH = PSRAM_SIZE
	PSRAM_NC	(rwx)	: ORIGIN = PSRAM_NC_BASE,			LENGTH = PSRAM_SIZE
	PSRAMX		(rx)	: ORIGIN = PSRAMX_BASE,				LENGTH = PSRAM_SIZE
}

/* Library configurations */
GROUP(libgcc.a libc.a libm.a libnosys.a)

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __export_fn_rom
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 *   __free_ram
 *   __factory_start
 *   __factory_end
 *   __calib_start
 *   __calib_end
 *   __flash_start
 *   __flash_end
 *   __free_flash
 *   __boot_sram_start_flash__
 *   __boot_sram_end_flash__
 *   __boot_sram_start__
 *   __boot_bss_sram_start__
 *   __boot_bss_sram_end__
 *   __sram_text_data_start_flash__
 *   __sram_text_data_end_flash__
 *   __fast_sram_text_data_start__
 *   __fast_sram_text_data_end__
 *   __fast_sram_text_data_start_flash__
 *   __fast_sram_text_data_end_flash__
 *   __sram_text_data_start__
 *   __sram_bss_start__
 *   __sram_bss_end__
 *   __overlay_text_start__
 *   __overlay_text_exec_start__
 *   __overlay_data_start__
 */
ENTRY(Boot_Loader)

SECTIONS
{
	__export_fn_rom = (ORIGIN(ROM) + LENGTH(ROM) - ROM_BUILD_INFO_SECTION_SIZE - ROM_EXPORT_FN_SECTION_SIZE);

#if defined(__SBC_FUNC_IN_ROM_VBEST2000__)
    rom1_entry = 0x0000c000;
    set__impure_ptr = 0x0003a4e4;
#endif

#if defined(__SBC_FUNC_IN_ROM_VBEST2000__) || defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
    SBC_FrameLen = 0x000329b1;
    SBC_InitDecoder = 0x00032a65;
    SBC_DecodeFrames = 0x000348c9;
    SBC_DecodeFrames_Out_SBSamples = 0x00034dc5;
    SBC_EncodeFrames = 0x0003538d;
    SBC_InitEncoder = 0x00035381;
    SBC_EncodeFrames_With_SBSamples = 0x000377bd;
#endif

	.boot_struct (ORIGIN(FLASH)) :
	{
		__flash_start = .;
		KEEP(*(.boot_struct))
		. = ALIGN(4);
	} > FLASH

	. = FLASH_TO_FLASHX(.);

	.boot_text_flash (.) : AT (FLASHX_TO_FLASH(ADDR(.boot_text_flash)))
	{
		*(.boot_loader)
		*(.boot_text_flash*)
		*(.boot_rodata*)
		. = ALIGN(4);
	} > FLASHX

	.vector_table (ORIGIN(RAM)) (NOLOAD) :
	{
		KEEP(*(.vector_table))
		. = VECTOR_SECTION_SIZE;
		. = ALIGN(4);
	} > RAM

	.reboot_param (.) (NOLOAD) :
	{
		KEEP(*(.reboot_param))
		. = REBOOT_PARAM_SECTION_SIZE;
		. = ALIGN(4);
	} > RAM

	.userdata_pool (.) (NOLOAD) :
	{
		*(.userdata_pool)
		. = ALIGN(4);
		__userdata_pool_end__ = .;
	} > RAM

	__boot_sram_start_flash__ = LOADADDR(.boot_text_flash) + SIZEOF(.boot_text_flash);
	__boot_sram_start__ = __userdata_pool_end__;
	.boot_text_sram (RAM_TO_RAMX(__boot_sram_start__)) : AT (__boot_sram_start_flash__)
	{
		/* memcpy.o or libc_nano.a(lib_a-memcpy.o/lib_a-memcpy-stub.o) */
		*:memcpy.o(.text*)
		*:lib_a-memcpy*.o(.text*)

		/* memset.o or libc_nano.a(lib_a-memset.o) */
		*:memset.o(.text*)
		*:lib_a-memset*.o(.text*)

		*:hal_norflash*.o(.text*)
		*:norflash_*.o(.text*)

		*(.boot_text_sram*)
		. = ALIGN(4);
	} > RAMX

	.boot_data_sram (RAMX_TO_RAM(ADDR(.boot_text_sram) + SIZEOF(.boot_text_sram))) :
					AT (__boot_sram_start_flash__ + SIZEOF(.boot_text_sram))
	{
		*:hal_norflash*.o(.data* .rodata*)
		*:norflash_*.o(.data* .rodata*)
		*:hal_psram.o(.data* .rodata*)
		*(.boot_data*)
		. = ALIGN(4);
	} > RAM

	__boot_sram_end_flash__ = __boot_sram_start_flash__ +
								SIZEOF(.boot_text_sram) + SIZEOF(.boot_data_sram);

	.boot_bss_sram (.) (NOLOAD) :
	{
		__boot_bss_sram_start__ = .;
		*:hal_norflash*.o(.bss*)
		*:norflash_*.o(.bss*)
		*:hal_psram.o(.bss*)
		*(.boot_bss*)
		. = ALIGN(4);
		__boot_bss_sram_end__ = .;
	} > RAM

	.mempool_bss_sram (.) (NOLOAD) :
	{
		__mempool_bss_sram_start__ = .;
		*:app_audio.o(.bss.app_audio_buff)
		*:app_tws_if.o(.bss.tws_big_buff)
		*(.mempool_bss*)
		. = ALIGN(4);
		__mempool_bss_sram_end__ = .;
	} > RAM

	__sram_text_data_start_flash__ = __boot_sram_end_flash__;
	__sram_text_data_start__ = __mempool_bss_sram_end__;
	.sram_text (RAM_TO_RAMX(__sram_text_data_start__)) :
				AT (__sram_text_data_start_flash__)
	{
		*(.sram_text*)
		. = ALIGN(4);
	} > RAMX

	.sram_data (RAMX_TO_RAM(ADDR(.sram_text) + SIZEOF(.sram_text))) :
				AT (__sram_text_data_start_flash__ + SIZEOF(.sram_text))
	{
#if defined(BTUSB_AUDIO_MODE)
		*:hal_usb.o(.rodata*)
#endif

#if defined(SPEECH_CAPTURE_TWO_CHANNEL)
		*:list.o(.data* .rodata*)
		*:list_ext.o(.data* .rodata*)
#endif
		*:utils.o(.data* .rodata*)
		*:cqueue.o(.data* .rodata*)
		*:iir_process.o(.data* .rodata*)
#if defined(HFP_1_6_ENABLE)
		*:sbcplc.o(.data* .rodata*)
		*:sbc.o(.data* .rodata*)
		*:sbc_math.o(.data* .rodata*)
#endif
		*(.sram_data*)
		. = ALIGN(4);
	} > RAM

	__sram_text_data_end_flash__ = __sram_text_data_start_flash__ +
								SIZEOF(.sram_text) + SIZEOF(.sram_data);

	.sram_bss (.) (NOLOAD) :
	{
		__sram_bss_start__ = .;
		*:utils.o(.bss*)
		*(.sram_bss*)
		. = ALIGN(4);
		__sram_bss_end__ = .;
	} > RAM

	__fast_sram_text_data_start_flash__ = __sram_text_data_end_flash__;
	.fast_text_sram (ORIGIN(FRAMX)) : AT (__fast_sram_text_data_start_flash__)
	{
		__fast_sram_text_data_start__ = .;

#if defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
        *:main.o(.text.*p_OS_MemSet*);
        *:main.o(.text.*p_OS_Assert*);
        *:main.o(.text.*p_hal_trace_printf_without_crlf*);
        *:main.o(.text.*p_memcpy*);
#endif

#if defined(BTUSB_AUDIO_MODE)
		*:hal_dma.o(.text*)
#endif

#if defined(BTUSB_AUDIO_MODE)
		*:hal_usb.o(.text*)

		*:usb_audio.o(.text*)
		*:usb_audio_sync.o(.text*)
		*:usb_audio_cdc.o(.text*)

		*:usbaudio_entry.o(.text*)
		*:usb_audio_test.o(.text*)
#endif

		/* memmove.o or libc_nano.a(lib_a-memmove.o) */
		*:memmove.o(.text*)
		*:lib_a-memmove.o(.text*)
#if defined(SPEECH_CAPTURE_TWO_CHANNEL)
		*:intersyshci.o(.text*)
		*:hal_intersys.o(.text*)

		*:list.o(.text*)
		*:list_ext.o(.text*)
#endif
		*:utils.o(.text*)
		*:cqueue.o(.text*)
		*:audio_resample_ex.o(.text*)
#if defined(HFP_1_6_ENABLE)
#if defined(SPEECH_CAPTURE_TWO_CHANNEL)
		*:voicebtpcmplay.o(.text*)
#endif
#if defined(__SBC_FUNC_IN_ROM__) || defined(__SBC_FUNC_IN_ROM_VBEST2000__)
		*:sbc.o(.text*)
#else
		*:sbc.o(.text.SbcIsValidStreamInfo)
		*:sbc.o(.text.SBC_FrameLen)
		*:sbc.o(.text.SbcCrcSum4)
		*:sbc.o(.text.SbcMonoBitAlloc)
		*:sbc.o(.text.SbcStereoBitAlloc)
		*:sbc.o(.text.SbcParseHeader)
		*:sbc.o(.text.SbcParseScaleFactors)
		*:sbc.o(.text.SbcUnpackSample)
		*:sbc.o(.text.SbcBuildSubbandSamples)
		*:sbc.o(.text.SbcJointProcessing)
		*:sbc.o(.text.SbcSynthesisFilter4)
		*:sbc.o(.text.SbcSynthesisFilter8)
		*:sbc.o(.text.SbcResetDecoderState)
		*:sbc.o(.text.SbcMuteFrame)
		*:sbc.o(.text.SBC_DecodeFrames)
#endif
		*:sbc_math.o(.text*)
#endif
#if defined(A2DP_AAC_ON) || defined(A2DP_SCALABLE_ON)
		*:fir_process.o(.text*)
		*:iir_process.o(.text.iir_run)
		*:iir_process.o(.text.iir_run_16bits)
#endif
		*(.fast_text_sram*)
		. = ALIGN(4);
		__fast_sram_text_data_end__ = .;
	} > FRAMX
	__fast_sram_text_data_end_flash__ = __fast_sram_text_data_start_flash__ + SIZEOF(.fast_text_sram);

	__overlay_text_start__ = RAMX_TO_RAM(__fast_sram_text_data_end__);
	__overlay_text_exec_start__ = RAM_TO_RAMX(__overlay_text_start__);
	OVERLAY (__overlay_text_exec_start__) : NOCROSSREFS AT (__fast_sram_text_data_end_flash__)
	{
#ifndef NO_OVERLAY
		.overlay_text0
		{
			LONG(0);
			*:preprocess.o(.text*)
			*:filterbank.o(.text*)
			*:kiss_fft.o(.text*)
			*:kiss_fftr.o(.text*)
			*:fftwrap.o(.text*)
			*:mdf.o(.text*)
			*:plc_8000.o(.text*)
			*:iir_process.o(.text.iir_run_mono_15bits)
			*:iir_process.o(.text.iir_run_mono_16bits)
			*:iir_process.o(.text.iir_run_mono_algorithmgain)
#if defined(HFP_1_6_ENABLE)
			*:plc_16000.o(.text*)
#endif
#if defined(_CVSD_BYPASS_)
			*:Pcm8k_Cvsd.o(.text*)
#endif
#if defined(SPEECH_TX_2MIC_NS)
			*:webrtc_vad.o(.text.WebRtcVad_Process)
			*:webrtc_vad.o(.text.WebRtcVad_ValidRateAndFrameLength)
			*:webrtc_vad.o(.text.WebRtcVad_CalcVad16khz)
			*:webrtc_vad.o(.text.WebRtcVad_Downsampling)
			*:webrtc_vad.o(.text.WebRtcVad_CalcVad8khz)
			*:vad_filterbank.o(.text.*)
			*:energy.o(.text.*)
			*:get_scaling_square.o(.text.*)
			*:dual_mic_denoise.o(.text*)
			*:cross_correlation.o(.text*)
			*:min_max_operations.o(.text*)
			*:resample_by_2_internal.o(.text*)
			*:division_operations.o(.text*)
			*:downsample_fast.o(.text*)
			*:resample_fractional.o(.text*)
			*:resample_48khz.o(.text*)
			*:vad_sp.o(.text*)
			*:vad_core.o(.text*)
			*:webrtc_vad.o(.text*)
			*:vad_gmm.o(.text*)
#endif
#if defined(SPEECH_TX_NS2)
			*:lc_mmse_ns.o(.text.lc_mmse_ns_analysis)
			*:lc_mmse_ns.o(.text.lc_mmse_ns_run)
#endif
			*:coherence.o(.text*)
			*:echo_canceller.o(.text*)
			*:echo_suppression(.text*)
			*:iirfilt.o(.text*)
			*:integer_delay.o(.text*)
			*:mdf2.o(.text*)
			*:ringbuf.o(.text*)
			*(.overlay_text0)
			. = ALIGN(4);
		}
		.overlay_text1
		{
			LONG(0);
#if !defined(TWS_RBCODEC_PLAYER) && !defined(TWS_LINEIN_PLAYER)
#if !defined(HFP_1_6_ENABLE)
			*:sbc.o(.text*)
			*:sbc_math.o(.text*)
#endif
			*:fixedpoint.o(.text*)
			*:replaygain.o(.text*)
			*:dsp_core.o(.text*)
			*:dsp_misc.o(.text*)
			*:dsp_filter.o(.text*)
			*:dsp_sample_input.o(.text*)
			*:dsp_sample_output.o(.text*)
			*:eq.o(.text*)
			*:pga.o(.text*)
#if !defined(A2DP_AAC_ON) && !defined(A2DP_SCALABLE_ON)
			*:fir_process.o(.text*)
			*:iir_process.o(.text.iir_run)
			*:iir_process.o(.text.iir_run_16bits)
#endif
#endif
			*(.overlay_text1)
			. = ALIGN(4);
		}
		.overlay_text2
		{
			LONG(0);
			*:fmdec.o(.text* .data* .rodata* .bss*)
			*(.overlay_text2)
			. = ALIGN(4);
		}
		.overlay_text3
		{
			LONG(0);
			*:fft128dot.o(.text*)
			*(.overlay_text3)
			. = ALIGN(4);
		}
		.overlay_text4
		{
			LONG(0);
#if defined(A2DP_AAC_ON)
			*:aacdec.o(.text*)
			*:aactabs.o(.text*)
			*:bitstream.o(.text*)
			*:buffers.o(.text*)
			*:dct4.o(.text*)
			*:decelmnt.o(.text*)
			*:dequant.o(.text*)
			*:fft.o(.text*)
			*:filefmt.o(.text*)
			*:huffman.o(.text*)
			*:hufftabs.o(.text*)
			*:imdct.o(.text*)
			*:noiseless.o(.text*)
			*:pns.o(.text*)
			*:stproc.o(.text*)
			*:tns.o(.text*)
			*:trigtabs.o(.text*)
			*:trigtabs_fltgen.o(.text*)
#endif
			*(.overlay_text4)
			. = ALIGN(4);
		}
		.overlay_text5
		{
			LONG(0);
			*:mpa.o(.text*)
			*:bit.o(.text*)
			*:frame.o(.text*)
			*:huffman.o(.text*)
			*:layer12.o(.text*)
			*:layer3.o(.text*)
			*:stream.o(.text*)
			*:synth.o(.text*)
			*(.overlay_text5)
			. = ALIGN(4);
		}
		.overlay_text6
		{
			LONG(0);
			*(.overlay_text6)
			. = ALIGN(4);
		}
		.overlay_text7
		{
			LONG(0);
			*(.overlay_text7)
			. = ALIGN(4);
		}
#endif
		.overlay_text_end
		{
			LONG(0);
			. = ALIGN(4);
		}
	} > FRAMX

	.dummy_overlay_text (.) (NOLOAD) :
	{
		__dummy_overlay_text_end__ = .;
	} > FRAMX

	ASSERT(__dummy_overlay_text_end__ - __fast_sram_text_data_start__ <= FAST_XRAM_SECTION_SIZE, "fast xram sections too large")
	__free_fram = FAST_XRAM_SECTION_SIZE - (__dummy_overlay_text_end__ - __fast_sram_text_data_start__);

	__overlay_data_start__ = __sram_bss_end__;
	OVERLAY (__overlay_data_start__) : NOCROSSREFS AT (__load_stop_overlay_text_end)
	{
#ifndef NO_OVERLAY
		.overlay_data0
		{
			/* Explicitly place 4 bytes at section start to avoid "section type changed to PROGBITS" warnings */
			LONG(0);
			*:preprocess.o(.data* .rodata*)
			*:filterbank.o(.data* .rodata*)
			*:kiss_fft.o(.data* .rodata*)
			*:kiss_fftr.o(.data* .rodata*)
			*:fftwrap.o(.data* .rodata*)
			*:mdf.o(.data* .rodata*)
			*:plc_8000.o(.data* .rodata*)
#if defined(HFP_1_6_ENABLE)
			*:plc_16000.o(.data* .rodata*)
#endif			
#if defined(_CVSD_BYPASS_)
			*:Pcm8k_Cvsd.o(.data* .rodata* .bss*)
#endif
#if defined(SPEECH_TX_2MIC_NS)
			*:dual_mic_denoise.o(.data* .rodata*)
			*:vad_filterbank.o(.data* .rodata*)
			*:energy.o(.data* .rodata*)
			*:get_scaling_square.o(.data* .rodata*)

			*:cross_correlation.o(.data* .rodata*)
			*:min_max_operations.o(.data* .rodata*)
			*:resample_by_2_internal.o(.data* .rodata*)
			*:division_operations.o(.data* .rodata*)
			*:downsample_fast.o(.data* .rodata*)
			*:resample_fractional.o(.data* .rodata*)
			*:resample_48khz.o(.data* .rodata*)
			*:vad_sp.o(.data* .rodata*)
			*:vad_core.o(.data* .rodata*)
			*:webrtc_vad.o(.data* .rodata*)
			*:vad_gmm.o(.data* .rodata*)
#endif
#if defined(SPEECH_TX_NS2)
			*:lc_mmse_ns.o(.data* .rodata*)
#endif
			*:lc_mmse_ns_float.o(.data* .rodata*)
			*:cmsis_fftwrap.o(.data* .rodata*)
			*:arm_rfft_fast_f32.o(.data* .rodata*)
			*:arm_cfft_f32.o(.data* .rodata*)
			*:arm_common_tables.o(.rodata.armBitRevIndexTable256)
			*:arm_common_tables.o(.rodata.twiddleCoef_256)
			*:arm_common_tables.o(.rodata.twiddleCoef_rfft_512)
			*:arm_common_tables.o(.rodata.armBitRevIndexTable128)
			*:arm_common_tables.o(.rodata.twiddleCoef_128)
			*:arm_common_tables.o(.rodata.twiddleCoef_rfft_256)
			*(.overlay_data0 .overlay_rodata0)
			. = ALIGN(4);
		}
		.overlay_data1
		{
			LONG(0);
#if !defined(TWS_RBCODEC_PLAYER) && !defined(TWS_LINEIN_PLAYER)
#if !defined(HFP_1_6_ENABLE)
			*:sbc.o(.data* .rodata*)
			*:sbc_math.o(.data* .rodata*)
#endif
			*:fixedpoint.o(.data* .rodata*)
			*:replaygain.o(.data* .rodata*)
			*:dsp_core.o(.data* .rodata*)
			*:dsp_misc.o(.data* .rodata*)
			*:dsp_filter.o(.data* .rodata*)
			*:dsp_sample_input.o(.data* .rodata*)
			*:dsp_sample_output.o(.data* .rodata*)
			*:eq.o(.data* .rodata*)
			*:pga.o(.data* .rodata*)
#if !defined(A2DP_AAC_ON) && !defined(A2DP_SCALABLE_ON)
			*:fir_process.o(.data* .rodata*)
#endif
#endif
			*(.overlay_data1 .overlay_rodata1)
			. = ALIGN(4);
		}
		.overlay_data2
		{
			LONG(0);
			*(.overlay_data2 .overlay_rodata2)
			. = ALIGN(4);
		}
		.overlay_data3
		{
			LONG(0);
			*:fft128dot.o(.data* .rodata*)
			*(.overlay_data3 .overlay_rodata3)
			. = ALIGN(4);
		}
		.overlay_data4
		{
			LONG(0);
#if defined(A2DP_AAC_ON)
			*:aacdec.o(.data* .bss*. .rodata*)
			*:aactabs.o(.data* .bss*. .rodata*)
			*:bitstream.o(.data* .bss*. .rodata*)
			*:dct4.o(.data* .bss*. .rodata*)
			*:decelmnt.o(.data* .bss*. .rodata*)
			*:dequant.o(.data* .bss*. .rodata*)
			*:fft.o(.data* .bss*. .rodata*)
			*:filefmt.o(.data* .bss*. .rodata*)
			*:huffman.o(.data* .bss*. .rodata*)
			*:hufftabs.o(.data* .bss*. .rodata*)
			*:imdct.o(.data* .bss*. .rodata*)
			*:noiseless.o(.data* .bss*. .rodata*)
			*:pns.o(.data* .bss*. .rodata*)
			*:stproc.o(.data* .bss*. .rodata*)
			*:tns.o(.data* .bss*. .rodata*)
			*:trigtabs.o(.data* .bss*. .rodata*)
			*:trigtabs_fltgen.o(.data* .bss*. .rodata*)
			*:buffers.o(.data* .bss* .rodata*)
			*:a2dpplay.o(.bss.*raw_aac_pcm_data)
#endif
			*(.overlay_data4 .overlay_rodata4)
			. = ALIGN(4);
		}
		.overlay_data5
		{
			LONG(0);
			*:mpa.o(.data* .bss*)
			*:bit.o(.data* .bss*)
			*:frame.o(.data* .bss*)
			*:huffman.o(.data* .bss*)
			*:layer12.o(.data* .bss*)
			*:layer3.o(.data* .bss*)
			*:stream.o(.data* .bss*)
			*:synth.o(.data* .bss*)
			*(.overlay_data5 .overlay_rodata5)
			. = ALIGN(4);
		}
		.overlay_data6
		{
			LONG(0);
			*(.overlay_data6 .overlay_rodata6)
			. = ALIGN(4);
		}
		.overlay_data7
		{
			LONG(0);
			*(.overlay_data7 .overlay_rodata7)
			. = ALIGN(4);
		}
#endif
		.overlay_data_end
		{
			LONG(0);
			. = ALIGN(4);
		}
	} > RAM

	.dummy_overlay_data (.) (NOLOAD) :
	{
		__dummy_overlay_data_end__ = .;
	} > RAM

	ASSERT(__dummy_overlay_data_end__ - __overlay_data_start__ <= OVERLAY_DATA_SECTION_SIZE, "overlay sections too large")

	.text (FLASH_TO_FLASHX(__load_stop_overlay_data_end)) : AT (FLASHX_TO_FLASH(ADDR(.text)))
	{
		*(.text*)
		*(.flash_text*)

		KEEP(*(.init))
		KEEP(*(.fini))

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
		*crtbegin.o(.dtors)
		*crtbegin?.o(.dtors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
		*(SORT(.dtors.*))
		*(.dtors)
	} > FLASHX

	.ARM.extab (.) : AT (FLASHX_TO_FLASH(ADDR(.ARM.extab)))
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASHX

	__exidx_start = .;
	.ARM.exidx (.) : AT (FLASHX_TO_FLASH(ADDR(.ARM.exidx)))
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > FLASHX
	__exidx_end = .;

	. = FLASHX_TO_FLASH(.);

	.rodata (.) :
	{
		*(.rodata*)
		*(.flash_rodata*)
		*(.note.gnu.build-id)

		KEEP(*(.eh_frame*))
	} > FLASH

	/* To copy multiple FLASH to RAM sections,
	 * uncomment .copy.table section and,
	 * define __STARTUP_COPY_MULTIPLE in startup_ARMCMx.S */
	/*
	.copy.table (.) :
	{
		. = ALIGN(4);
		__copy_table_start__ = .;
		LONG (__etext)
		LONG (__data_start__)
		LONG (__data_end__ - __data_start__)
		LONG (__etext2)
		LONG (__data2_start__)
		LONG (__data2_end__ - __data2_start__)
		__copy_table_end__ = .;
	} > FLASH
	*/

	/* To clear multiple BSS sections,
	 * uncomment .zero.table section and,
	 * define __STARTUP_CLEAR_BSS_MULTIPLE in startup_ARMCMx.S */
	/*
	.zero.table (.) :
	{
		. = ALIGN(4);
		__zero_table_start__ = .;
		LONG (__bss_start__)
		LONG (__bss_end__ - __bss_start__)
		LONG (__bss2_start__)
		LONG (__bss2_end__ - __bss2_start__)
		__zero_table_end__ = .;
	} > FLASH
	*/

	__etext = .;

	.data : AT (__etext)
	{
		__data_start__ = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);

		. = ALIGN(4);
		/* finit data */
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);

		KEEP(*(.jcr*))
		. = ALIGN(4);
		/* All data end */
		__data_end__ = .;

	} > RAM

	.bss (.) (NOLOAD) :
	{
		. = ALIGN(4);
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > RAM

	.heap (.) (NOLOAD) :
	{
		. = ALIGN(8);
		__HeapBase = .;
		__end__ = .;
		end = __end__;
		. += HEAP_SECTION_SIZE;
		. = ALIGN(8);
		__HeapLimit = .;
	} > RAM

	/* .stack_dummy section doesn't contains any symbols. It is only
	 * used for linker to calculate size of stack sections, and assign
	 * values to stack symbols later */
	.stack_dummy (.) (COPY) :
	{
		. = STACK_SECTION_SIZE;
		. = ALIGN(8);
	} > RAM

	/* Set stack top to end of RAM, and stack limit move down by
	 * size of stack_dummy section */
	__StackTop = ORIGIN(RAM) + LENGTH(RAM);
	__StackLimit = __StackTop - SIZEOF(.stack_dummy);
	PROVIDE(__stack = __StackTop);

	/* Check if data + heap + stack exceeds RAM limit */
	ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
	__free_ram = __StackLimit - __HeapLimit;

	. = __etext + SIZEOF(.data);
	.build_info (.) :
	{
		KEEP(*(.build_info))
		. = ALIGN(4);
	} > FLASH = 0x00000000

	/* The following section be the last loaded section */
	.code_start_addr (.) :
	{
		LONG(BUILD_INFO_MAGIC);
		LONG(ABSOLUTE(__flash_start));
	} > FLASH

	__flash_end = .;

#ifdef __DUAL_USER_SECTION_BAK__	
	.userdata_backup (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - USERDATA_SECTION_SIZE - AUD_SECTION_SIZE - CRASH_DUMP_SECTION_SIZE - USERDATA_BACKUP_SECTION_SIZE) (NOLOAD) :
	{
		__userdata_start_backup = .;
		. = USERDATA_BACKUP_SECTION_SIZE;
		__userdata_end_backup = .;
	} > FLASH_NC
#endif

	.crash_dump (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - USERDATA_SECTION_SIZE - AUD_SECTION_SIZE - CRASH_DUMP_SECTION_SIZE) (NOLOAD) :
	{
		__crash_dump_start = .;
		. = CRASH_DUMP_SECTION_SIZE;
		__crash_dump_end = .;
	} > FLASH_NC
	
	.audio (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - USERDATA_SECTION_SIZE - AUD_SECTION_SIZE) (NOLOAD) :
	{
		__aud_start = .;
		. = AUD_SECTION_SIZE;
		__aud_end = .;
	} > FLASH_NC

	.userdata (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE - USERDATA_SECTION_SIZE) (NOLOAD) :
	{
		__userdata_start = .;
		. = USERDATA_SECTION_SIZE;
		__userdata_end = .;
	} > FLASH_NC

	.factory (ORIGIN(FLASH_NC) + LENGTH(FLASH_NC) - FACTORY_SECTION_SIZE) (NOLOAD) :
	{
		__factory_start = .;
		. = FACTORY_SECTION_SIZE;
		__factory_end = .;
	} > FLASH_NC

#ifdef __DUAL_USER_SECTION_BAK__
	ASSERT(FLASH_NC_TO_C(__userdata_start_backup) >= __flash_end, "region FLASH overflowed")
	__free_flash = FLASH_NC_TO_C(__userdata_start_backup) - __flash_end;
#else
	ASSERT(FLASH_NC_TO_C(__crash_dump_start) >= __flash_end, "region FLASH overflowed")
	__free_flash = FLASH_NC_TO_C(__crash_dump_start) - __flash_end;
#endif	
}

