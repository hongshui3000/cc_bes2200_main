#include "plat_addr_map.h"
#include "hal_cmu.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_iomux.h"
#include "hal_dma.h"
#include "hal_key.h"
#include "hal_gpadc.h"
#include "hal_sleep.h"
#include "hal_sysfreq.h"
#include "cmsis.h"
#include "pmu.h"
#include "analog.h"
#include "string.h"
#include "hwtimer_list.h"
#include "audioflinger.h"
#if defined(ANC_TEST)
#include "anc_test.h"
#endif
#include "usb_audio_test.h"
#include "dualadc_audio_test.h"
#include "usb_audio_frm_defs.h"
#include "tgt_hardware.h"
#include "task_schedule.h"
#include "audio_eq.h"

#ifdef RTOS
#include "cmsis_os.h"
#endif
#ifdef __PC_CMD_UART__
#include "hal_cmd.h"
#endif

#define CODEC_BUFF_FRAME_NUM            4
#define USB_BUFF_FRAME_NUM              8
#if (CODEC_BUFF_FRAME_NUM >= USB_BUFF_FRAME_NUM)
#error "Codec buffer frame num should be less than usb buffer frame num (on the requirement of conflict ctrl)"
#endif

#ifdef USB_AUDIO_DYN_CFG
#define USB_AUDIO_PLAYBACK_BUFF_SIZE    NON_EXP_ALIGN(MAX_FRAME_SIZE_PLAYBACK * CODEC_BUFF_FRAME_NUM, DAC_BUFF_ALIGN)
#define USB_AUDIO_CAPTURE_BUFF_SIZE     NON_EXP_ALIGN(MAX_FRAME_SIZE_CAPTURE * CODEC_BUFF_FRAME_NUM, ADC_BUFF_ALIGN)

#define USB_AUDIO_RECV_BUFF_SIZE        ALIGN(MAX_FRAME_SIZE_RECV * USB_BUFF_FRAME_NUM, USB_BUFF_ALIGN)
#define USB_AUDIO_SEND_BUFF_SIZE        ALIGN(MAX_FRAME_SIZE_SEND * USB_BUFF_FRAME_NUM, USB_BUFF_ALIGN)

#if defined(CHIP_BEST1000)
// FIR EQ is working on 16-bit
// FIR_EQ_buffer_size = max_playback_symbol_number_in_buffer * sizeof(int16_t)
#define USB_AUDIO_FIR_EQ_BUFF_SIZE      USB_AUDIO_PLAYBACK_BUFF_SIZE
#elif defined(CHIP_BEST2000)
// FIR EQ is working on 32-bit
// FIR_EQ_buffer_size = max_playback_symbol_number_in_buffer * sizeof(int32_t)
#define USB_AUDIO_FIR_EQ_BUFF_SIZE      (USB_AUDIO_PLAYBACK_BUFF_SIZE*2)
#endif

#else
#define USB_AUDIO_PLAYBACK_BUFF_SIZE    NON_EXP_ALIGN(FRAME_SIZE_PLAYBACK * CODEC_BUFF_FRAME_NUM, DAC_BUFF_ALIGN)
#define USB_AUDIO_CAPTURE_BUFF_SIZE     NON_EXP_ALIGN(FRAME_SIZE_CAPTURE * CODEC_BUFF_FRAME_NUM, ADC_BUFF_ALIGN)

#define USB_AUDIO_RECV_BUFF_SIZE        ALIGN(FRAME_SIZE_RECV * USB_BUFF_FRAME_NUM, USB_BUFF_ALIGN)
#define USB_AUDIO_SEND_BUFF_SIZE        ALIGN(FRAME_SIZE_SEND * USB_BUFF_FRAME_NUM, USB_BUFF_ALIGN)

#if defined(CHIP_BEST1000)
// FIR EQ is working on 16-bit
#define USB_AUDIO_FIR_EQ_BUFF_SIZE      (USB_AUDIO_PLAYBACK_BUFF_SIZE * sizeof(int16_t) / SAMPLE_SIZE_PLAYBACK)
#elif defined(CHIP_BEST2000)
// FIR EQ is working on 16-bit
#define USB_AUDIO_FIR_EQ_BUFF_SIZE      (USB_AUDIO_PLAYBACK_BUFF_SIZE * sizeof(int32_t) / SAMPLE_SIZE_PLAYBACK)
#endif

#endif

#if (defined(CHIP_BEST1000) && (defined(ANC_TEST) || defined(_DUAL_AUX_MIC_))) && (CHAN_NUM_CAPTURE == CHAN_NUM_SEND)
// Resample input buffer size should be (half_of_max_sample_num * SAMPLE_SIZE_CAPTURE * CHAN_NUM_CAPTURE).
// half_of_max_sample_num = 48000 / 1000 * CODEC_BUFF_FRAME_NUM / 2 * 48 / 44
#define RESAMPLE_INPUT_BUFF_SIZE        ALIGN(48000 / 1000 * SAMPLE_SIZE_CAPTURE * CHAN_NUM_CAPTURE * CODEC_BUFF_FRAME_NUM / 2 * 48 / 44, 4)
#else
#define RESAMPLE_INPUT_BUFF_SIZE        0
#endif
// Resample history buffer size should be
// sizeof(struct RESAMPLE_CTRL_T) + ((SAMPLE_NUM + phase_coef_num) * SAMPLE_SIZE_CAPTURE * CHAN_NUM_CAPTURE)
#define RESAMPLE_HISTORY_BUFF_SIZE      (50 + (256 * SAMPLE_SIZE_CAPTURE * CHAN_NUM_CAPTURE))
#define USB_AUDIO_RESAMPLE_BUFF_SIZE    (RESAMPLE_INPUT_BUFF_SIZE + RESAMPLE_HISTORY_BUFF_SIZE)

#define ALIGNED4                        ALIGNED(4)

#if defined(USB_AUDIO_TEST) || defined(DUALADC_AUDIO_TEST)

#ifdef AUDIO_ANC_FB_MC
static uint8_t ALIGNED4 test_playback_buff[USB_AUDIO_PLAYBACK_BUFF_SIZE * 9];//max 48->384 or 44.1->44.1*8;
#else
static uint8_t ALIGNED4 test_playback_buff[USB_AUDIO_PLAYBACK_BUFF_SIZE];
#endif

static uint8_t ALIGNED4 test_capture_buff[USB_AUDIO_CAPTURE_BUFF_SIZE];

#endif

#ifdef USB_AUDIO_TEST
#ifdef __HW_FIR_EQ_PROCESS__
static uint8_t ALIGNED4 test_fir_eq_buff[USB_AUDIO_FIR_EQ_BUFF_SIZE];
#else
static uint8_t ALIGNED4 test_fir_eq_buff[0];
#endif

#if defined(USB_AUDIO_DYN_CFG) || defined(__AUDIO_RESAMPLE__)
static uint8_t ALIGNED4 test_resample_buff[USB_AUDIO_RESAMPLE_BUFF_SIZE];
#endif

static uint8_t ALIGNED4 test_recv_buff[USB_AUDIO_RECV_BUFF_SIZE];
static uint8_t ALIGNED4 test_send_buff[USB_AUDIO_SEND_BUFF_SIZE];
#endif

#ifdef CFG_HW_KEY_LED_PIN
const struct HAL_IOMUX_PIN_FUNCTION_MAP pinmux_key_led[1] = {
    {CFG_HW_KEY_LED_PIN, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_NOPULL},
};
#endif

#ifdef CFG_MIC_KEY
extern void mic_key_open (void);
#endif

static void uart_i2c_switch(void)
{
    static int flag = 0;

    flag ^= 1;

    if (flag) {
        hal_iomux_set_analog_i2c();
    } else {
        hal_iomux_set_uart0();
    }
}

static int key_event_process(uint32_t key_code, uint8_t key_event)
{
    TRACE("%s: code=0x%X, event=%u", __FUNCTION__, key_code, key_event);

#ifdef CFG_HW_KEY_LED_PIN
    if (key_event == HAL_KEY_EVENT_DOWN) {
        hal_gpio_pin_set(CFG_HW_KEY_LED_PIN);
    } else if (key_event == HAL_KEY_EVENT_UP) {
        hal_gpio_pin_clr(CFG_HW_KEY_LED_PIN);
    }
#endif

#ifdef USB_AUDIO_TEST
    if (usb_audio_tester_key(key_code, key_event) == 0) {
        return 0;
    }
#endif

#ifdef ANC_TEST
    if (anc_tester_key(key_code, key_event) == 0) {
        return 0;
    }
#endif

    if (key_event == HAL_KEY_EVENT_CLICK) {
        if (key_code == HAL_KEY_CODE_FN9) {
            uart_i2c_switch();
        }
    }

    return 0;
}

#if __ANC_EQ_TYPE__ == 1
extern uint32_t usb_audio_set_eq_iir(uint8_t index);
#elif __ANC_EQ_TYPE__ == 2
extern uint32_t usb_audio_set_eq_fir(uint8_t index);
#endif

void anc_usb_test(void)
{
#ifdef ANC_TEST
#if __ANC_EQ_TYPE__ == 1
    uint8_t anc_status = 0;
    uint8_t anc_status_record = 0xff;
    //uint32_t anc_status_test_cnt = 0;
#endif
#endif

    TRACE("%s", __FUNCTION__);

#ifdef __AUDIO_RESAMPLE__
    hal_cmu_audio_resample_enable();
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)
    audio_eq_init();
    // TODO: Get EQ store parameter
    // audio_eq_fir_update_cfg(int16_t *coef, int16_t num);
    // audio_eq_iir_update_cfg(int *gain, int num);
#endif

#ifdef USB_AUDIO_TEST

    usb_audio_tester_init(test_playback_buff,
#ifdef AUDIO_ANC_FB_MC
        sizeof(test_playback_buff) / 9,
#else
        sizeof(test_playback_buff),
#endif
        test_capture_buff, sizeof(test_capture_buff),
        test_recv_buff, sizeof(test_recv_buff),
        test_send_buff, sizeof(test_send_buff),
        test_fir_eq_buff, sizeof(test_fir_eq_buff),
#if defined(USB_AUDIO_DYN_CFG) || defined(__AUDIO_RESAMPLE__)
        test_resample_buff, sizeof(test_resample_buff)
#else
        NULL, 0
#endif
        );
    usb_audio_tester(1);
#endif

#ifdef ANC_TEST
    anc_tester_init(AUD_INPUT_PATH_MAINMIC, SAMPLE_RATE_PLAYBACK, SAMPLE_RATE_CAPTURE);
#endif

#ifdef DUALADC_AUDIO_TEST
    dualadc_audio_tester_init(test_playback_buff, USB_AUDIO_PLAYBACK_BUFF_SIZE,
        test_capture_buff, USB_AUDIO_CAPTURE_BUFF_SIZE);
    dualadc_audio_tester(1);
#endif

#if defined(CFG_MIC_KEY)
    mic_key_open();
#endif

    // Allow sleep
    hal_sysfreq_req(HAL_SYSFREQ_USER_APP_0, HAL_CMU_FREQ_32K);

    while (1) {
#ifdef USB_AUDIO_TEST
        usb_audio_tester_loop();
#endif

#ifdef ANC_TEST
        anc_tester_loop();
#endif

#ifdef RTOS
        // Let the task sleep
        osDelay(20);
#else // !RTOS

#ifdef ANC_TEST
// anc on/off use different filters
// TODO: define some MACRO
#if __ANC_EQ_TYPE__ == 1
#if 0
        // test
        if(anc_status_test_cnt++ == 600000)
        {
            anc_status_test_cnt = 0;
            anc_status_record = !anc_status_record;
            TRACE("[%s] anc_status = %d", __func__, anc_status_record);

            if(anc_status_record)
            {
                audio_eq_iir_set_cfg(audio_eq_iir_cfg_list[1]);
            }
            else
            {
                audio_eq_iir_set_cfg(audio_eq_iir_cfg_list[0]);
            }
        }
#else
// #if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)
        anc_status = anc_tester_get_status();
        if(anc_status_record != anc_status)
        {
            anc_status_record = anc_status;
            TRACE("[%s] anc_status = %d", __func__, anc_status);
            if(anc_status)
            {
                usb_audio_set_eq_iir(1);
            }
            else
            {
                usb_audio_set_eq_iir(0);
            }
            // TRACE("[%s] anc_status = %d", __func__, anc_status_record);
        }
#endif
#endif
#endif

#ifdef __PC_CMD_UART__
        hal_cmd_run();
#endif

        hal_sleep_enter_sleep();

#endif // !RTOS
    }
}

#ifdef CFG_HW_GPADCKEY
void gpadc_key_handler(uint16_t irq_val, HAL_GPADC_MV_T volt)
{
    static uint16_t stable_cnt = 0;
    static uint16_t click_cnt = 0;
    static uint32_t click_time;
    uint32_t time;
    enum HAL_KEY_EVENT_T event;
    bool send_event = false;

    time = hal_sys_timer_get();

    if (volt < 100) {
        stable_cnt++;
        //TRACE("adc_key down: volt=%u stable=%u click_cnt=%u click_time=%u time=%u", volt, stable_cnt, click_cnt, click_time, time);
    } else {
        if (stable_cnt > 1) {
            //TRACE("adc_key up: volt=%u stable=%u click_cnt=%u click_time=%u time=%u", volt, stable_cnt, click_cnt, click_time, time);
            if (click_cnt == 0 || (time - click_time) < MS_TO_TICKS(500)) {
                click_time = time;
                click_cnt++;
                if (click_cnt >= 3) {
                    send_event = true;
                }
            } else {
                send_event = true;
            }
        }
        stable_cnt = 0;

        if (click_cnt > 0 && (time - click_time) >= MS_TO_TICKS(500)) {
            send_event = true;
        }

        if (send_event) {
            //TRACE("adc_key click: volt=%u stable=%u click_cnt=%u click_time=%u time=%u", volt, stable_cnt, click_cnt, click_time, time);
            if (click_cnt == 1) {
                event = HAL_KEY_EVENT_CLICK;
            } else if (click_cnt == 2) {
                event = HAL_KEY_EVENT_DOUBLECLICK;
            } else {
                event = HAL_KEY_EVENT_TRIPLECLICK;
            }
            key_event_process(CFG_HW_GPADCKEY, event);
            click_cnt = 0;
        }
    }
}
#endif

// GDB can set a breakpoint on the main function only if it is
// declared as below, when linking with STD libraries.
int btusbaudio_entry(int argc, char *argv[])
{
    anc_usb_test();

    return 0;
}
