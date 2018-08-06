#include "plat_addr_map.h"
#include "hal_timer.h"
#include "hal_timer_raw.h"
#include "reg_timer.h"
#include "hal_location.h"
#include "hal_cmu.h"
#include "cmsis_nvic.h"

//#define ELAPSED_TIMER_ENABLED

static struct DUAL_TIMER_T * const BOOT_DATA_LOC dual_timer = (struct DUAL_TIMER_T *)TIMER_BASE;

static HAL_TIMER_IRQ_HANDLER_T irq_handler = NULL;

//static uint32_t load_value = 0;
static uint32_t start_time;

static void hal_timer1_irq_handler(void);
static void hal_timer2_irq_handler(void);

void hal_sys_timer_open(void)
{
    dual_timer->timer[0].Control = TIMER_CTRL_EN | TIMER_CTRL_PRESCALE_DIV_1 | TIMER_CTRL_SIZE_32_BIT;
    NVIC_SetVector(TIMER1_IRQn, (uint32_t)hal_timer1_irq_handler);
}

uint32_t BOOT_TEXT_SRAM_LOC hal_sys_timer_get(void)
{
    return -dual_timer->timer[0].Value;
}

uint32_t hal_sys_timer_get_max(void)
{
    return 0xFFFFFFFF;
}

void BOOT_TEXT_SRAM_LOC hal_sys_timer_delay(uint32_t ticks)
{
    uint32_t start = dual_timer->timer[0].Value;

    while (start - dual_timer->timer[0].Value < ticks);
}

void SRAM_TEXT_LOC hal_sys_timer_delay_us(uint32_t us)
{
    enum HAL_CMU_FREQ_T freq = hal_cmu_sys_get_freq();
    uint32_t loop;
    uint32_t i;

    // Assuming:
    // 1) system clock uses audio PLL
    // 2) audio PLL is configured as 48K series, 196.608M

    if (freq == HAL_CMU_FREQ_208M) {
        loop = 197;
    } else if (freq == HAL_CMU_FREQ_104M) {
        loop = 197 / 2;
    } else if (freq == HAL_CMU_FREQ_78M) {
        loop = 197 / 3;
    } else if (freq == HAL_CMU_FREQ_52M) {
        loop = 52;
    } else {
        loop = 26;
    }

    loop = loop * us / 5;
    for (i = 0; i < loop; i++) {
        asm volatile("nop");
    }
}

static uint32_t NOINLINE SRAM_TEXT_DEF(measure_cpu_freq_interval)(uint32_t cnt)
{
    uint32_t start;

    start = dual_timer->timer[0].Value;

    asm volatile(
        "_loop:;"
        "subs %0, #1;"
        "cmp %0, #0;"
        "bne _loop;"
        : : "r"(cnt));

    return start - dual_timer->timer[0].Value;
}

uint32_t hal_sys_timer_calc_cpu_freq(int high_res)
{
    static const uint32_t ref_freq = 26000000;
    static const uint32_t cnt = ref_freq / 4;
    uint32_t one_sec;
    uint32_t lock;
    uint32_t interval;
    uint32_t freq;

    if (high_res) {
        one_sec = 6500000;
    } else {
        one_sec = 16000;
    }

    lock = int_lock();

    if (high_res) {
        hal_cmu_timer_select_fast();
    }

    interval = measure_cpu_freq_interval(cnt);

    if (high_res) {
        hal_cmu_timer_select_slow();
    }

    int_unlock(lock);

    freq = (uint32_t)((uint64_t)ref_freq * one_sec / interval);
    if (high_res == 0) {
        freq = (freq + 500000) / 1000000 * 1000000;
    }
    return freq;
}

#ifndef RTOS
int osDelay(uint32_t ms)
{
    hal_sys_timer_delay(MS_TO_TICKS(ms));
    return 0;
}
#endif

static void hal_timer1_irq_handler(void)
{
    dual_timer->timer[0].IntClr = 1;
    dual_timer->timer[0].Control &= ~TIMER_CTRL_INTEN;
}

void hal_timer_setup(enum HAL_TIMER_TYPE_T type, HAL_TIMER_IRQ_HANDLER_T handler)
{
    uint32_t mode;

    if (type == HAL_TIMER_TYPE_ONESHOT) {
        mode = TIMER_CTRL_ONESHOT;
    } else if (type == HAL_TIMER_TYPE_PERIODIC) {
        mode = TIMER_CTRL_MODE_PERIODIC;
    } else {
        mode = 0;
    }

    irq_handler = handler;

    dual_timer->timer[1].IntClr = 1;
#ifdef ELAPSED_TIMER_ENABLED
    dual_timer->elapsed_timer[1].ElapsedCtrl = TIMER_ELAPSED_CTRL_CLR;
#endif

    if (handler) {
        NVIC_SetVector(TIMER2_IRQn, (uint32_t)hal_timer2_irq_handler);
        NVIC_SetPriority(TIMER2_IRQn, IRQ_PRIORITY_NORMAL);
        NVIC_ClearPendingIRQ(TIMER2_IRQn);
        NVIC_EnableIRQ(TIMER2_IRQn);
    }

    dual_timer->timer[1].Control = mode |
                                   (handler ? TIMER_CTRL_INTEN : 0) |
                                   TIMER_CTRL_PRESCALE_DIV_1 |
                                   TIMER_CTRL_SIZE_32_BIT;
}

void hal_timer_start(uint32_t load)
{
    start_time = hal_sys_timer_get();
    hal_timer_reload(load);
    hal_timer_continue();
}

void hal_timer_stop(void)
{
    dual_timer->timer[1].Control &= ~TIMER_CTRL_EN;
#ifdef ELAPSED_TIMER_ENABLED
    dual_timer->elapsed_timer[1].ElapsedCtrl = TIMER_ELAPSED_CTRL_CLR;
#endif
    dual_timer->timer[1].IntClr = 1;
    NVIC_ClearPendingIRQ(TIMER2_IRQn);
}

void hal_timer_continue(void)
{
#ifdef ELAPSED_TIMER_ENABLED
    dual_timer->elapsed_timer[1].ElapsedCtrl = TIMER_ELAPSED_CTRL_EN | TIMER_ELAPSED_CTRL_CLR;
#endif
    dual_timer->timer[1].Control |= TIMER_CTRL_EN;
}

int hal_timer_is_enabled(void)
{
    return !!(dual_timer->timer[1].Control & TIMER_CTRL_EN);
}

void hal_timer_reload(uint32_t load)
{
    if (load > HAL_TIMER_LOAD_DELTA) {
        //load_value = load;
        load -= HAL_TIMER_LOAD_DELTA;
    } else {
        //load_value = HAL_TIMER_LOAD_DELTA + 1;
        load = 1;
    }
    dual_timer->timer[1].Load = load;
}

uint32_t hal_timer_get(void)
{
    return dual_timer->timer[1].Value;
}

int hal_timer_irq_active(void)
{
    return NVIC_GetActive(TIMER2_IRQn);
}

int hal_timer_irq_pending(void)
{
    // Or NVIC_GetPendingIRQ(TIMER2_IRQn) ?
    return (dual_timer->timer[1].MIS & TIMER_MIS_MIS);
}

uint32_t hal_timer_get_overrun_time(void)
{
#ifdef ELAPSED_TIMER_ENABLED
    uint32_t extra;

    if (dual_timer->elapsed_timer[1].ElapsedCtrl & TIMER_ELAPSED_CTRL_EN) {
        extra = dual_timer->elapsed_timer[1].ElapsedVal;
    } else {
        extra = 0;
    }

    return extra;
#else
    return 0;
#endif
}

uint32_t hal_timer_get_elapsed_time(void)
{
    //return load_value + hal_timer_get_overrun_time();
    return hal_sys_timer_get() - start_time;
}

static void hal_timer2_irq_handler(void)
{
    uint32_t elapsed;

    dual_timer->timer[1].IntClr = 1;
    if (irq_handler) {
        elapsed = hal_timer_get_elapsed_time();
        irq_handler(elapsed);
    } else {
        dual_timer->timer[1].Control &= ~TIMER_CTRL_INTEN;
    }
}

