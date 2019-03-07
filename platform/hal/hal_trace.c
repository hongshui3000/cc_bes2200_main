#include "hal_trace.h"
#include "hal_uart.h"
#include "hal_dma.h"
#include "hal_cmu.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "cmsis_nvic.h"
#include "hal_iomux.h"
#include "hal_timer.h"
#include "hal_chipid.h"
#include "hal_norflash.h"
#include "hal_bootmode.h"
#include "pmu.h"

//Kevin
#include "hal_timer.h"


extern const char sys_build_info[];
#define CRASH_REBOOT   1

#ifndef DEBUG 
#define CRASH_REBOOT (1)
#endif

#ifndef DEBUG 
#define DEBUG (1)
#endif

#define TRACE_IDLE_OUTPUT               0

#ifndef TRACE_BUF_SIZE
#define TRACE_BUF_SIZE                  (4 * 1024)
#endif

#define HAL_TRACE_ASSERT_ID             0xBE57AAAA
#define HAL_TRACE_EXCEPTION_ID          0xBE57EEEE

#define CACHE_2_UNCACHE(addr) \
        ((unsigned char *)((unsigned int)addr & ~(0x04000000)))

struct HAL_TRACE_ASSERT_INFO_T {
    uint32_t ID;
    uint32_t PC;
    const char *FILE;
    const char *FUNC;
    uint32_t LINE;
    const char *FMT;
    uint32_t R[4];
    uint32_t SP;
    uint32_t MSP;
    uint32_t PSP;
    uint32_t CONTROL;
};

struct HAL_TRACE_EXCEPTION_INFO_T {
    uint32_t ID;
    const uint32_t* REGS;
    uint32_t MSP;
    uint32_t PSP;
    uint8_t PRIMASK;
    uint8_t FAULTMASK;
    uint8_t BASEPRI;
    uint8_t CONTROL;
    uint32_t ICSR;
    uint32_t AIRCR;
    uint32_t SCR;
    uint32_t CCR;
    uint32_t SHCSR;
    uint32_t CFSR;
    uint32_t HFSR;
    uint32_t AFSR;
    uint32_t MMFAR;
    uint32_t BFAR;
};

#ifdef DEBUG

struct HAL_TRACE_BUF_T {
    unsigned char buf[TRACE_BUF_SIZE];
    unsigned short wptr;
    unsigned short rptr;
#if (TRACE_IDLE_OUTPUT == 0)
    unsigned short sends[2];
#endif
    unsigned short discards;
    bool sending;
    bool in_trace;
};

static const struct HAL_UART_CFG_T uart_cfg = {
    .parity = HAL_UART_PARITY_NONE,
    .stop = HAL_UART_STOP_BITS_1,
    .data = HAL_UART_DATA_BITS_8,
    .flow = HAL_UART_FLOW_CONTROL_NONE,//HAL_UART_FLOW_CONTROL_RTSCTS,
    .tx_level = HAL_UART_FIFO_LEVEL_1_2,
    .rx_level = HAL_UART_FIFO_LEVEL_1_2,
    .baud = 921600,
    .dma_rx = false,
#if (TRACE_IDLE_OUTPUT == 0)
    .dma_tx = true,
#else
    .dma_tx = false,
#endif
    .dma_rx_stop_on_err = false,
};

extern uint32_t __crash_dump_start[];
extern uint32_t __crash_dump_end[];

#if (TRACE_IDLE_OUTPUT == 0)
static struct HAL_DMA_CH_CFG_T dma_cfg;

static struct HAL_DMA_DESC_T dma_desc[2];
#endif

static enum HAL_TRACE_TRANSPORT_T trace_transport = HAL_TRACE_TRANSPORT_QTY;
static enum HAL_UART_ID_T trace_uart;

static struct HAL_TRACE_BUF_T trace;

static const char POSSIBLY_UNUSED newline[] = "\r\n";

static const char discards_prefix[] = "\r\nLOST ";
static const uint32_t max_discards = 99999;
// 5 digits + "\r\n" = 7 chars
static char discards_buf[sizeof(discards_prefix) - 1 + 7];
static const unsigned char discards_digit_start = sizeof(discards_prefix) - 1;

static char crash_buf[100];

static HAL_TRACE_CRASH_DUMP_CB_T crash_dump_cb_list[3];

#ifdef FAULT_DUMP
static void hal_trace_fault_handler(void);
#endif

#ifdef HAL_TRACE_RX_ENABLE
static int hal_trace_rx_open();
#endif
#if defined(TRACE_DUMP2FLASH)
int hal_trace_dumper_output(const unsigned char *buf, unsigned int buf_len);
#endif
int hal_trace_output_phy(const unsigned char *buf, unsigned int buf_len);

static int (*hal_trace_output_fn)(const unsigned char *, unsigned int ) = NULL;
static uint32_t trace_level_mask = 0xffffffff; 

#if (TRACE_IDLE_OUTPUT == 0)

static void hal_trace_uart_send(void)
{
    uint32_t sends;
    uint32_t size;
    uint32_t int_state;

    int_state = int_lock();

    // There is a race condition if we do not check s/w flag, but only check the h/w status.
    // [e.g., hal_gpdma_chan_busy(dma_cfg.ch)]
    // When the DMA is done, but DMA IRQ handler is still pending due to interrupt lock
    // or higher priority IRQ, it will have a chance to send the same content twice.
    if (trace.wptr != trace.rptr && !trace.sending) {
        trace.sending = true;

        if (trace.wptr > trace.rptr) {
            sends = trace.wptr - trace.rptr;
        } else {
            sends = TRACE_BUF_SIZE - (trace.rptr - trace.wptr);
        }

        size = TRACE_BUF_SIZE - trace.rptr;
        dma_cfg.src = (uint32_t)&trace.buf[trace.rptr];
        if (size >= sends) {
            size = sends;
            dma_cfg.src_tsize = size;
            hal_gpdma_init_desc(&dma_desc[0], &dma_cfg, NULL, 1);
            trace.sends[1] = dma_cfg.src_tsize;
        } else {
            dma_cfg.src_tsize = size;
            hal_gpdma_init_desc(&dma_desc[0], &dma_cfg, &dma_desc[1], 0);
            trace.sends[0] = dma_cfg.src_tsize;

            dma_cfg.src = (uint32_t)&trace.buf[0];
            dma_cfg.src_tsize = sends - size;
            hal_gpdma_init_desc(&dma_desc[1], &dma_cfg, NULL, 1);
            trace.sends[1] = dma_cfg.src_tsize;
        }

        hal_gpdma_sg_start(&dma_desc[0], &dma_cfg);
    }

    int_unlock(int_state);
}

static void hal_trace_uart_xfer_done(uint8_t chan, uint32_t remain_tsize, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    if (error) {
        if (lli) {
            if (trace.sends[0] > remain_tsize) {
                trace.sends[0] -= remain_tsize;
            } else {
                trace.sends[0] = 0;
            }
            trace.sends[1] = 0;
        } else {
            if (trace.sends[1] > remain_tsize) {
                trace.sends[1] -= remain_tsize;
            } else {
                trace.sends[1] = 0;
            }
        }
    }

    trace.rptr += trace.sends[0] + trace.sends[1];
    if (trace.rptr >= TRACE_BUF_SIZE) {
        trace.rptr -= TRACE_BUF_SIZE;
    }
    trace.sends[0] = 0;
    trace.sends[1] = 0;
    trace.sending = false;

    if (trace.wptr != trace.rptr) {
        hal_trace_uart_send();
    }
}

static void hal_trace_send(void)
{
    if (trace_transport == HAL_TRACE_TRANSPORT_QTY) {
        return;
    }

    hal_trace_uart_send();
}

#else // TRACE_IDLE_OUTPUT

static void hal_trace_uart_idle_send(void)
{
    int i;
    uint32_t lock;
    unsigned short wptr, rptr;

    lock = int_lock();
    wptr = trace.wptr;
    rptr = trace.rptr;
    int_unlock(lock);

    if (wptr == rptr) {
        return;
    }

    if (wptr < rptr) {
        for (i = rptr; i < TRACE_BUF_SIZE; i++) {
            hal_uart_blocked_putc(trace_uart, trace.buf[i]);
        }
        rptr = 0;
    }

    for (i = rptr; i < wptr; i++) {
        hal_uart_blocked_putc(trace_uart, trace.buf[i]);
    }

    trace.rptr = wptr;
    if (trace.rptr >= TRACE_BUF_SIZE) {
        trace.rptr -= TRACE_BUF_SIZE;
    }
}

void hal_trace_idle_send(void)
{
    if (trace_transport == HAL_TRACE_TRANSPORT_QTY) {
        return;
    }

    hal_trace_uart_idle_send();
}

#endif // TRACE_IDLE_OUTPUT

int hal_trace_switch(enum HAL_TRACE_TRANSPORT_T transport)
{
    uint32_t POSSIBLY_UNUSED lock;
    int ret;

#if (CHIP_HAS_UART > 1)
#ifdef FORCE_TRACE_UART1
    transport = HAL_TRACE_TRANSPORT_UART1;
#endif
#endif

#ifdef CHIP_HAS_USB
    if (transport == HAL_TRACE_TRANSPORT_USB) {
        return 1;
    }
#endif
    if (transport >= HAL_TRACE_TRANSPORT_QTY) {
        return 1;
    }
    if (trace_transport >= HAL_TRACE_TRANSPORT_QTY) {
        return 1;
    }
    if (trace_transport == transport) {
        return 0;
    }

    ret = 0;

#if (CHIP_HAS_UART > 1)

    lock = int_lock();

    if (trace_transport == HAL_TRACE_TRANSPORT_UART0 || trace_transport == HAL_TRACE_TRANSPORT_UART1) {
#if (TRACE_IDLE_OUTPUT == 0)
        if (dma_cfg.ch != HAL_DMA_CHAN_NONE) {
            hal_gpdma_cancel(dma_cfg.ch);
        }
#endif
        hal_uart_close(trace_uart);
    }

    if (transport == HAL_TRACE_TRANSPORT_UART0 || transport == HAL_TRACE_TRANSPORT_UART1) {
        if (transport == HAL_TRACE_TRANSPORT_UART0) {
            trace_uart = HAL_UART_ID_0;
        } else if (transport == HAL_TRACE_TRANSPORT_UART1) {
            trace_uart = HAL_UART_ID_1;
        }
#if (TRACE_IDLE_OUTPUT == 0)
        dma_cfg.dst_periph = (trace_uart == HAL_UART_ID_0) ? HAL_GPDMA_UART0_TX : HAL_GPDMA_UART1_TX;
        trace.sends[0] = 0;
        trace.sends[1] = 0;
#endif
        ret = hal_uart_open(trace_uart, &uart_cfg);
        if (ret) {
#if (TRACE_IDLE_OUTPUT == 0)
            hal_gpdma_free_chan(dma_cfg.ch);
            dma_cfg.ch = HAL_DMA_CHAN_NONE;
#endif
            trace_transport = HAL_TRACE_TRANSPORT_QTY;
            goto _exit;
        }
    }

    trace.sending = false;

    trace_transport = transport;

_exit:
    int_unlock(lock);

#endif // CHIP_HAS_UART > 1

    return ret;
}

#if defined(TRACE_DUMP2FLASH)
#include "cqueue.h"
CQueue trace_buff_queue;

int hal_trace_dumper_open(void)
{
    uint32_t lock;

    hal_sw_bootmode_set(HAL_SW_BOOTMODE_REBOOT_DUMPERENABLE);

    lock = int_lock();
    InitCQueue(&trace_buff_queue, TRACE_BUF_SIZE, trace.buf);
    int_unlock(lock);

    hal_trace_output_fn = hal_trace_dumper_output;

    // Show build info
    static const char dbl_new_line[] = "\n\n";
    hal_trace_output((unsigned char *)dbl_new_line, sizeof(dbl_new_line) - 1);
    hal_trace_output((unsigned char *)sys_build_info, strlen(sys_build_info));
    hal_trace_printf(HAL_TRACE_LEVEL_0, "\n\n------\nCHIP_ID: %d\n------\n\n", hal_get_chip_metal_id());

    return 0;
}
#endif

int hal_trace_open(enum HAL_TRACE_TRANSPORT_T transport)
{
    int ret;

    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT_DUMPERENABLE);

#if (CHIP_HAS_UART > 1)
#ifdef FORCE_TRACE_UART1
    transport = HAL_TRACE_TRANSPORT_UART1;
#endif
#endif

#ifdef CHIP_HAS_USB
    if (transport == HAL_TRACE_TRANSPORT_USB) {
        return 1;
    }
#endif
    if (transport >= HAL_TRACE_TRANSPORT_QTY) {
        return 1;
    }

    if (trace_transport != HAL_TRACE_TRANSPORT_QTY) {
        return hal_trace_switch(transport);
    }

    if (transport == HAL_TRACE_TRANSPORT_UART0) {
        trace_uart = HAL_UART_ID_0;
#if (CHIP_HAS_UART > 1)
    } else if (transport == HAL_TRACE_TRANSPORT_UART1) {
        trace_uart = HAL_UART_ID_1;
#endif
    }

    memcpy(discards_buf, discards_prefix, discards_digit_start);

    trace.wptr = 0;
    trace.rptr = 0;
    trace.discards = 0;
    trace.sending = false;
    trace.in_trace = false;

#if (TRACE_IDLE_OUTPUT == 0)
    trace.sends[0] = 0;
    trace.sends[1] = 0;

    if (transport == HAL_TRACE_TRANSPORT_UART0
#if (CHIP_HAS_UART > 1)
            || transport == HAL_TRACE_TRANSPORT_UART1
#endif
            ) {
        memset(&dma_cfg, 0, sizeof(dma_cfg));
        dma_cfg.dst = 0; // useless
        dma_cfg.dst_bsize = HAL_DMA_BSIZE_8;
#if (CHIP_HAS_UART > 1)
        dma_cfg.dst_periph = (trace_uart == HAL_UART_ID_0) ? HAL_GPDMA_UART0_TX : HAL_GPDMA_UART1_TX;
#else
        dma_cfg.dst_periph = HAL_GPDMA_UART0_TX;
#endif
        dma_cfg.dst_width = HAL_DMA_WIDTH_BYTE;
        dma_cfg.handler = hal_trace_uart_xfer_done;
        dma_cfg.src_bsize = HAL_DMA_BSIZE_32;
        dma_cfg.src_periph = 0; // useless
        dma_cfg.src_width = HAL_DMA_WIDTH_BYTE;
        dma_cfg.type = HAL_DMA_FLOW_M2P_DMA;
        dma_cfg.try_burst = 0;
        dma_cfg.ch = hal_gpdma_get_chan(dma_cfg.dst_periph, HAL_DMA_HIGH_PRIO);

        ASSERT(dma_cfg.ch != HAL_DMA_CHAN_NONE, "Failed to get DMA channel");
        ASSERT(trace_uart == HAL_UART_ID_0
#if (CHIP_HAS_UART > 1)
                || trace_uart == HAL_UART_ID_1
#endif
                ,
            "BAD trace uart ID: %d", trace_uart);
    }
#endif

    ret = hal_uart_open(trace_uart, &uart_cfg);
    if (ret) {
#if (TRACE_IDLE_OUTPUT == 0)
        hal_gpdma_free_chan(dma_cfg.ch);
        dma_cfg.ch = HAL_DMA_CHAN_NONE;
#endif
        return ret;
    }

#ifdef FAULT_DUMP
    NVIC_SetDefaultFaultHandler(hal_trace_fault_handler);
#endif

#ifdef HAL_TRACE_RX_ENABLE
    hal_trace_rx_open();
#endif

    trace_transport = transport;

    hal_trace_output_fn = hal_trace_output_phy;
    // Show build info
    static const char dbl_new_line[] = "\n\n";
    hal_trace_output((unsigned char *)dbl_new_line, sizeof(dbl_new_line) - 1);
    hal_trace_output((unsigned char *)sys_build_info, strlen(sys_build_info));
    hal_trace_printf(HAL_TRACE_LEVEL_0, "\n\n------\nCHIP_ID: %d\n------\n\n", hal_get_chip_metal_id());

    return 0;
}

int hal_trace_close(void)
{
    if (trace_transport >= HAL_TRACE_TRANSPORT_QTY) {
        goto _exit;
    }
#ifdef CHIP_HAS_USB
    if (trace_transport == HAL_TRACE_TRANSPORT_USB) {
        goto _exit;
    }
#endif

    if (trace_transport == HAL_TRACE_TRANSPORT_UART0
#if (CHIP_HAS_UART > 1)
            || trace_transport == HAL_TRACE_TRANSPORT_UART1
#endif
            ) {
#if (TRACE_IDLE_OUTPUT == 0)
        if (dma_cfg.ch != HAL_DMA_CHAN_NONE) {
            hal_gpdma_cancel(dma_cfg.ch);
            hal_gpdma_free_chan(dma_cfg.ch);
            dma_cfg.ch = HAL_DMA_CHAN_NONE;
        }
#endif
        hal_uart_close(trace_uart);
    }

_exit:
    trace_transport = HAL_TRACE_TRANSPORT_QTY;

    return 0;
}

static void hal_trace_print_discards(uint32_t discards)
{
    static const uint8_t base = 10;
    char digit[5], *d, *out;
    int len;
    uint16_t size;

    if (discards > max_discards) {
        discards = max_discards;
    }

    d = &digit[0];
    do {
        *d++ = (discards % base) + '0';
    } while (discards /= base);

    out = &discards_buf[discards_digit_start];
    do {
        *out++ = *--d;
    } while (d > &digit[0]);
    *out++ = '\r';
    *out++ = '\n';
    len = out - &discards_buf[0];

    size = TRACE_BUF_SIZE - trace.wptr;
    if (size >= len) {
        size = len;
    }
    memcpy(&trace.buf[trace.wptr], &discards_buf[0], size);
    if (size < len) {
        memcpy(&trace.buf[0], &discards_buf[size], len - size);
    }
    trace.wptr += len;
    if (trace.wptr >= TRACE_BUF_SIZE) {
        trace.wptr -= TRACE_BUF_SIZE;
    }
}

#if defined(TRACE_DUMP2FLASH)
int hal_trace_dumper_output(const unsigned char *buf, unsigned int buf_len)
{
    uint32_t lock;
    uint32_t available_len;

    lock = int_lock();
    available_len = AvailableOfCQueue(&trace_buff_queue);
    if (available_len < buf_len){
        DeCQueue(&trace_buff_queue, 0, buf_len - available_len);
    }
    EnCQueue(&trace_buff_queue, (unsigned char *)buf, buf_len);
    int_unlock(lock);

    return 0;
}
#endif

int hal_trace_output_phy(const unsigned char *buf, unsigned int buf_len)
{
    int ret;
    uint32_t lock;
    uint32_t avail;
    uint16_t size;

    ret = 0;

    lock = int_lock();

    // Avoid troubles when NMI occurs during trace
    if (!trace.in_trace) {
        trace.in_trace = true;

        if (trace.wptr >= trace.rptr) {
            avail = TRACE_BUF_SIZE - (trace.wptr - trace.rptr) - 1;
        } else {
            avail = (trace.rptr - trace.wptr) - 1;
        }

        if (avail < buf_len + (trace.discards ? sizeof(discards_buf) : 0)) {
            if (trace.discards < (1 << (sizeof(trace.discards) * 8)) - 1) {
                trace.discards++;
            }
            ret = 1;
        } else {
            if (trace.discards) {
                hal_trace_print_discards(trace.discards);
                trace.discards = 0;
            }

            size = TRACE_BUF_SIZE - trace.wptr;
            if (size >= buf_len) {
                size = buf_len;
            }
            memcpy(&trace.buf[trace.wptr], &buf[0], size);
            if (size < buf_len) {
                memcpy(&trace.buf[0], &buf[size], buf_len - size);
            }
            trace.wptr += buf_len;
            if (trace.wptr >= TRACE_BUF_SIZE) {
                trace.wptr -= TRACE_BUF_SIZE;
            }
#if (TRACE_IDLE_OUTPUT == 0)
            hal_trace_send();
#endif
        }

        trace.in_trace = false;
    }

    int_unlock(lock);

    return ret ? 0 : buf_len;
}

int hal_trace_output(const unsigned char *buf, unsigned int buf_len)
{
    return hal_trace_output_fn(buf, buf_len);
}

static inline int hal_trace_format_va(char *buf, unsigned int size, int crlf, const char *fmt, va_list ap)
{
    int len;

    len = vsnprintf(&buf[0], size, fmt, ap);
    if (crlf) {
        if (len + 2 < size) {
            buf[len++] = '\r';
        }
        if (len + 1 < size) {
            buf[len++] = '\n';
        }
    }
    //if (len < size) buf[len] = 0;

    return len;
}

int hal_trace_printf(uint32_t lvl, const char *fmt, ...)
{
    char buf[100];
    int len;
    va_list ap;

    if (!(lvl & trace_level_mask)){
        return 0;
    }

    sprintf(buf, "[%08d] ", TICKS_TO_MS(hal_sys_timer_get()));
    
    va_start(ap, fmt);
    len =  strlen(buf) + hal_trace_format_va(buf + strlen(buf), sizeof(buf) - strlen(buf), true, fmt, ap);
    va_end(ap);

    return hal_trace_output((unsigned char *)buf, len);
}

int hal_trace_color_printf(TRACE_COLOR color, uint32_t lvl, const char *fmt, ...){
    char buf[110];
    int len;
    va_list ap;
    
    if(color < BLACK || color > TOTAL_COLOR_NUM){
        hal_trace_output("hal_trace_color_printf!! invail color\r\n", 40);
        return -1;
    }

    sprintf(buf, "\x1b[0;%dm[%08d] ", color, TICKS_TO_MS(hal_sys_timer_get()));
    
    va_start(ap, fmt);
    len = strlen(buf) + hal_trace_format_va(buf + strlen(buf), sizeof(buf) - strlen(buf) - 10, false, fmt, ap);
    va_end(ap);
    
    strcat(buf, "\x1b[0m\r\n");
    
    return hal_trace_output((unsigned char *)buf, len + 6);

}

int hal_trace_printf_without_crlf_fix_arg(uint32_t lvl, const char *fmt)
{
    char buf[100];
    int len,slen;

    if (!(lvl & trace_level_mask)){
        return 0;
    }

    if (trace.discards > 0) {
        len = snprintf(buf, sizeof(buf), "\r\nLOST %d\r\n", trace.discards);
    } else {
        len = 0;
    }

    slen = strlen(fmt);
    strcpy(&buf[len], fmt);
    len += slen;

    return hal_trace_output((unsigned char *)buf, len);
}

int hal_trace_printf_without_crlf(uint32_t lvl, const char *fmt, ...)
{
    char buf[100];
    int len;
    va_list ap;

    if (!(lvl& trace_level_mask)){
        return 0;
    }

    va_start(ap, fmt);
    len = hal_trace_format_va(buf, sizeof(buf), false, fmt, ap);
    va_end(ap);

    return hal_trace_output((unsigned char *)buf, len);
}

int hal_trace_printf_with_tag(uint32_t lvl, const char *tag, const char *fmt, ...)
{
#ifdef AUDIO_DEBUG
    return 0;
#else
    char buf[100];
    int len = 0;
    va_list ap;

    if (!(lvl& trace_level_mask)) {
        return 0;
    }

    if (tag != NULL)
        len += snprintf(&buf[len], sizeof(buf) - len, "[%s]: ", tag);

    va_start(ap, fmt);
    len += hal_trace_format_va(&buf[len], sizeof(buf) - len, true, fmt, ap);
    va_end(ap);

    return hal_trace_output((unsigned char *)buf, len);
#endif
}

int hal_trace_dump(uint32_t lvl, const char *fmt, unsigned int size,  unsigned int count, const void *buffer)
{
    char buf[255];
    int len=0,n=0,i=0;

    if (!(lvl & trace_level_mask)){
        return 0;
    }

    switch( size )
    {
        case sizeof(uint32_t):
            while(i<count && len<sizeof(buf))
            {
                len += snprintf(&buf[len], sizeof(buf) - len, fmt, *(uint32_t *)((uint32_t *)buffer+i));
                i++;
            }
            break;
        case sizeof(uint16_t):
                while(i<count && len<sizeof(buf))
                {
                    len += snprintf(&buf[len], sizeof(buf) - len, fmt, *(uint16_t *)((uint16_t *)buffer+i));
                    i++;
                }
                break;
        case sizeof(uint8_t):
        default:
            while(i<count && len<sizeof(buf))
            {
                len += snprintf(&buf[len], sizeof(buf) - len, fmt, *(uint8_t *)((uint8_t *)buffer+i));
                i++;
            }
            break;
    }


    if (len + 2 < sizeof(buf)) {
        buf[len++] = '\r';
    }
    if (len + 1 < sizeof(buf)) {
        buf[len++] = '\n';
    }

    n = hal_trace_output((unsigned char *)buf, len);

    return n;
}

int hal_trace_busy(void)
{
    union HAL_UART_FLAG_T flag;

    if (trace_transport == HAL_TRACE_TRANSPORT_UART0 ||
            trace_transport == HAL_TRACE_TRANSPORT_UART1) {
        if (hal_uart_opened(trace_uart)) {
            flag = hal_uart_get_flag(trace_uart);
            return flag.BUSY;
        }
    }
    return 0;
}

static void hal_trace_flush_buffer(void)
{
    uint32_t lock;

    lock = int_lock();

#if (TRACE_IDLE_OUTPUT == 0)
    while (trace.wptr != trace.rptr) {
        while (hal_gpdma_chan_busy(dma_cfg.ch));
        hal_gpdma_irq_run_chan(dma_cfg.ch);
    }
#else
    while (trace.wptr != trace.rptr) {
        hal_trace_idle_send();
    }
#endif

    int_unlock(lock);
}

void hal_trace_printf_imm(uint32_t lvl, const char *fmt, ...)
{
    char buf[100];
    int len;
    va_list ap;

    if (!(lvl & trace_level_mask)){
        return;
    }

    hal_trace_flush_buffer();

    va_start(ap, fmt);
    len = hal_trace_format_va(buf, sizeof(buf), true, fmt, ap);
    va_end(ap);

    hal_trace_output((unsigned char *)buf, len);

    hal_trace_flush_buffer();
}

int hal_trace_crash_dump_register(HAL_TRACE_CRASH_DUMP_CB_T cb)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(crash_dump_cb_list); i++) {
        if (crash_dump_cb_list[i] == NULL) {
            crash_dump_cb_list[i] = cb;
            return 0;
        }
    }

    return 1;
}

static void hal_trace_crash_dump_callback(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(crash_dump_cb_list); i++) {
        if (crash_dump_cb_list[i]) {
            crash_dump_cb_list[i]();
        }
    }
}

#endif // DEBUG

#define CRASH_DUMP_FRAME_SIZE (1024*12)
#define CRASH_DUMP_FRAME_NUM  (((uint32_t)__crash_dump_end - (uint32_t)__crash_dump_start)/CRASH_DUMP_FRAME_SIZE)

#define SIZEOFSTRUCT(TYPE, MEMBER)          (sizeof((TYPE *)0)->MEMBER)

struct CRASH_OUTPUT_BUF_T {
    uint32_t seqnum;
    uint32_t partnum;    
    uint32_t reserved;    
    uint32_t bufpos;
    uint8_t buf[CRASH_DUMP_FRAME_SIZE-16];
};

struct CRASH_OUTPUT_BUF_T *crash_dump_section_p = NULL;
struct CRASH_OUTPUT_BUF_T *crash_output_p = NULL;
bool crash_dump_init = false;

extern int app_audio_mempool_init(void);
extern int app_audio_mempool_noclr_init();
extern int app_audio_mempool_get_buff(uint8_t **buff, uint32_t size);

int hal_trace_crash_dump_output(const unsigned char *buf, unsigned int buf_len)
{
    if (crash_dump_init){
        if ((crash_output_p->bufpos+buf_len) <= SIZEOFSTRUCT(struct CRASH_OUTPUT_BUF_T, buf)){
            memcpy(crash_output_p->buf+crash_output_p->bufpos, buf, buf_len);
            crash_output_p->bufpos += buf_len;
        }else{
            memcpy(crash_output_p->buf+crash_output_p->bufpos, buf, SIZEOFSTRUCT(struct CRASH_OUTPUT_BUF_T, buf)-crash_output_p->bufpos);
            crash_output_p->bufpos += SIZEOFSTRUCT(struct CRASH_OUTPUT_BUF_T, buf)-crash_output_p->bufpos;
        }
    }else{    
        hal_trace_flush_buffer();
        hal_trace_output(buf, buf_len);
        hal_trace_flush_buffer();
    }
    return 0;
}

int hal_trace_crash_printf_without_crlf(const char *fmt, ...)
{
    char buf[100];
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = hal_trace_format_va(buf, sizeof(buf), false, fmt, ap);
    va_end(ap);

    return hal_trace_crash_dump_output((unsigned char *)buf, len);
}

int hal_trace_crash_printf(const char *fmt, ...)
{
    char buf[100];
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = hal_trace_format_va(buf, sizeof(buf), true, fmt, ap);
    va_end(ap);

    return hal_trace_crash_dump_output((unsigned char *)buf, len);
}
    
int hal_trace_crashdump_get_empty_section(uint32_t *seqnum, struct CRASH_OUTPUT_BUF_T **crash_dump_addr)
{
    struct CRASH_OUTPUT_BUF_T *p;
    uint32_t max = 0;    

    for (p = (struct CRASH_OUTPUT_BUF_T *)__crash_dump_start; p < (struct CRASH_OUTPUT_BUF_T *)__crash_dump_end; p++){        
        if (max < p->seqnum && p->seqnum!=0xffffffff){
            max = p->seqnum;
        }
    }
    *seqnum = max+1;
    *crash_dump_addr = (struct CRASH_OUTPUT_BUF_T *)__crash_dump_start+((*seqnum)%CRASH_DUMP_FRAME_NUM);
    
    return 0;
}

static void hal_trace_crashdump_flush_buffer(void)
{
    uint32_t time;
    uint32_t sent_cnt;
    uint32_t send_frame_size;
    if (trace_transport != HAL_TRACE_TRANSPORT_QTY){    
        sent_cnt = 0;
        do{
            send_frame_size = (crash_output_p->bufpos-sent_cnt) < (TRACE_BUF_SIZE-4)?
                               crash_output_p->bufpos-sent_cnt:
                               TRACE_BUF_SIZE-4;

            hal_trace_flush_buffer();
            hal_trace_output(crash_output_p->buf + sent_cnt, send_frame_size);
            hal_trace_flush_buffer();
            sent_cnt += send_frame_size;
        }while(sent_cnt < crash_output_p->bufpos);

        time = hal_sys_timer_get();
        while (hal_trace_busy() && hal_sys_timer_get() - time < MS_TO_TICKS(100));
    }
}

int hal_trace_crashdump_init(void)
{
    const char separate_line[] = "\r\n----------------------------------------\r\n";
    uint32_t dump_section_idx;    
    int len;

    if (crash_dump_init)
        return 0;

    crash_dump_init = true;

    if (trace_transport == HAL_TRACE_TRANSPORT_UART0) {
        hal_iomux_set_uart0();
#if (CHIP_HAS_UART > 1)
    } else if (trace_transport == HAL_TRACE_TRANSPORT_UART1) {
        hal_iomux_set_uart1();
#endif
    }

    hal_trace_crashdump_get_empty_section(&dump_section_idx, &crash_dump_section_p);

    app_audio_mempool_init();
    app_audio_mempool_get_buff((uint8_t **)&crash_output_p, sizeof(struct CRASH_OUTPUT_BUF_T));
    crash_output_p->bufpos = 0;

    crash_output_p->seqnum = dump_section_idx;
    crash_output_p->partnum = CRASH_DUMP_FRAME_NUM;
    hal_trace_crash_dump_output((unsigned char *)separate_line, sizeof(separate_line) - 1);
    len = snprintf(crash_buf, sizeof(crash_buf), "%08lX/%08lX\r\n", crash_output_p->seqnum, crash_output_p->partnum);
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    hal_trace_crash_dump_output((unsigned char *)sys_build_info, strlen(sys_build_info));
    hal_trace_crash_dump_output((unsigned char *)separate_line, sizeof(separate_line) - 1);

#if defined(TRACE_DUMP2FLASH)
    {
        unsigned char *e1 = NULL, *e2 = NULL;
        unsigned int len1 = 0, len2 = 0;
    
        PeekCQueue(&trace_buff_queue, LengthOfCQueue(&trace_buff_queue), &e1, &len1, &e2, &len2);

        hal_trace_crash_dump_output(e1, len1);
        hal_trace_crash_dump_output(e2, len2);
    }
#endif
    hal_trace_crash_dump_output((unsigned char *)separate_line, sizeof(separate_line) - 1);

    return 0;
}

void hal_trace_flash_flush(void)
{
	hal_trace_crash_dump_callback();
	hal_trace_crashdump_flush_buffer();
	hal_trace_crashdump_dumptoflash();
}

void nv_record_flash_flush(void);
static void NORETURN hal_trace_crash_end(void)
{
    hal_trace_crashdump_flush_buffer();
    // Tag failure for simulation environment
    hal_cmu_simu_fail();
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_REBOOT_CRASHDUMP);
#ifdef CRASH_REBOOT
    hal_cmu_sys_reboot();
#else
    hal_iomux_set_analog_i2c();
    hal_iomux_set_jtag();
    hal_cmu_jtag_clock_enable();
#endif

    SAFE_PROGRAM_STOP();
}

void hal_trace_crashdump_dumptoflash(void)
{
    uint32_t lock;
    uint32_t dump_section_idx;    

    app_audio_mempool_noclr_init();
    app_audio_mempool_get_buff((uint8_t **)&crash_output_p, sizeof(struct CRASH_OUTPUT_BUF_T));

    hal_trace_crashdump_get_empty_section(&dump_section_idx, &crash_dump_section_p);
    lock = int_lock();  
    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)CACHE_2_UNCACHE(crash_dump_section_p),sizeof(struct CRASH_OUTPUT_BUF_T));
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)CACHE_2_UNCACHE(crash_dump_section_p),(uint8_t *)crash_output_p, sizeof(struct CRASH_OUTPUT_BUF_T)); 
    pmu_flash_read_config();
    int_unlock(lock);   
}

void hal_trace_assert_dump(ASSERT_DUMP_ARGS)
{
    struct HAL_TRACE_ASSERT_INFO_T info;
    register uint32_t r0 asm("r0");
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    register uint32_t r3 asm("r3");
    register uint32_t sp asm("sp");
    register uint32_t lr asm("lr");

    // MUST SAVE REGISTERS FIRST!
    info.PC = lr;
    info.R[0] = r0;
    info.R[1] = r1;
    info.R[2] = r2;
    info.R[3] = r3;
    info.SP = sp;

    int_lock();

    info.ID = HAL_TRACE_ASSERT_ID;
#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
    info.FILE = file;
#elif defined(DEBUG) && defined(ASSERT_SHOW_FILE)
    info.FILE = scope;
#else
    info.FILE = NULL;
#endif
#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
    info.FUNC = func;
#elif defined(DEBUG) && defined(ASSERT_SHOW_FUNC)
    info.FUNC = scope;
#else
    info.FUNC = NULL;
#endif
#if defined(ASSERT_SHOW_FILE_FUNC) || defined(ASSERT_SHOW_FILE) || defined(ASSERT_SHOW_FUNC)
    info.LINE = line;
#else
    info.LINE = 0;
#endif
    info.FMT = fmt;
    info.MSP = __get_MSP();
    info.PSP = __get_PSP();
    info.CONTROL = __get_CONTROL();

    *(volatile uint32_t *)RAM_BASE = (uint32_t)&info;

#ifdef DEBUG
    static const char title[] = "\r\n### ASSERT ###\r\n";
    static const char POSSIBLY_UNUSED desc_file[] = "FILE    : ";
    static const char POSSIBLY_UNUSED desc_func[] = "FUNCTION: ";
    static const char POSSIBLY_UNUSED desc_line[] = "LINE    : ";
    int len;
    va_list ap;
    uint32_t time;

    hal_trace_crashdump_init();

    hal_trace_flush_buffer();

    hal_trace_crash_dump_output((unsigned char *)title, sizeof(title) - 1);

#if defined(ASSERT_SHOW_FILE_FUNC) || defined(ASSERT_SHOW_FILE) || defined(ASSERT_SHOW_FUNC)
    const char separate_line[] = "\r\n----------------------------------------\r\n";
#ifdef ASSERT_SHOW_FILE
    const char *file = scope;
#elif defined(ASSERT_SHOW_FUNC)
    const char *func = scope;
#endif

#if defined(ASSERT_SHOW_FILE_FUNC) || defined(ASSERT_SHOW_FILE)
    hal_trace_crash_dump_output((const unsigned char *)desc_file, sizeof(desc_file) - 1);
    hal_trace_crash_dump_output((const unsigned char *)file, strlen(file));
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);
#endif

#if defined(ASSERT_SHOW_FILE_FUNC) || defined(ASSERT_SHOW_FUNC)
    hal_trace_crash_dump_output((const unsigned char *)desc_func, sizeof(desc_func) - 1);
    hal_trace_crash_dump_output((const unsigned char *)func, strlen(func));
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);
#endif

    hal_trace_crash_dump_output((const unsigned char *)desc_line, sizeof(desc_func) - 1);
    len = snprintf(crash_buf, sizeof(crash_buf), "%u", line);
    hal_trace_crash_dump_output((const unsigned char *)crash_buf, len);
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    hal_trace_crash_dump_output((unsigned char *)separate_line, sizeof(separate_line) - 1);

    hal_trace_flush_buffer();
#endif

    va_start(ap, fmt);
    len = hal_trace_format_va(crash_buf, sizeof(crash_buf), true, fmt, ap);
    va_end(ap);

    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    hal_trace_flush_buffer();

    hal_trace_crash_dump_callback();

    time = hal_sys_timer_get();
    while (hal_trace_busy() && hal_sys_timer_get() - time < MS_TO_TICKS(100));
#endif

    hal_trace_crash_end();
}

#ifdef FAULT_DUMP
static void hal_trace_fault_dump(const uint32_t *regs, const uint32_t *fpu_regs)
{
    struct HAL_TRACE_EXCEPTION_INFO_T info;
    uint32_t primask;

    primask = __get_PRIMASK();

    int_lock();

    info.ID = HAL_TRACE_EXCEPTION_ID;
    info.REGS = regs;
    info.MSP = __get_MSP();
    info.PSP = __get_PSP();
    info.PRIMASK = primask;
    info.FAULTMASK = __get_FAULTMASK();
    info.BASEPRI = __get_BASEPRI();
    info.CONTROL = __get_CONTROL();
    info.ICSR = SCB->ICSR;
    info.AIRCR = SCB->AIRCR;
    info.SCR = SCB->SCR;
    info.CCR = SCB->CCR;
    info.SHCSR = SCB->SHCSR;
    info.CFSR = SCB->CFSR;
    info.HFSR = SCB->HFSR;
    info.AFSR = SCB->AFSR;
    info.MMFAR = SCB->MMFAR;
    info.BFAR = SCB->BFAR;

    *(volatile uint32_t *)RAM_BASE = (uint32_t)&info;

#ifdef DEBUG
    static const char title[] = "\r\n### EXCEPTION ###\r\n";
    int len;
    uint32_t time;
    int i;
    int index;
    uint32_t val;
    uint32_t *stack;

    hal_trace_crashdump_init();

    hal_trace_flush_buffer();

    hal_trace_crash_dump_output((unsigned char *)title, sizeof(title) - 1);

    len = snprintf(crash_buf, sizeof(crash_buf), "PC =%08lX", regs[15]);
    val = __get_IPSR();
    if (val == 0) {
        len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, ", ThreadMode\r\n");
    } else {
        len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, ", ExceptionNumber=%d\r\n", (int)val - 16);
    }
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    for (i = 0; i < 3; i++) {
        index = i * 4;
        len = snprintf(crash_buf, sizeof(crash_buf), "R%-2d=%08lX, R%-2d=%08lX, R%-2d=%08lX, R%-2d=%08lX\r\n",
            index, regs[index], index + 1, regs[index + 1], index + 2, regs[index + 2], index + 3, regs[index + 3]);
        hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    }
    len = snprintf(crash_buf, sizeof(crash_buf), "R12=%08lX, SP =%08lX, LR =%08lX\r\n", regs[12], regs[13], regs[14]);
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    len = snprintf(crash_buf, sizeof(crash_buf), "MSP=%08lX, PSP=%08lX\r\n", __get_MSP(), __get_PSP());
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    len = snprintf(crash_buf, sizeof(crash_buf), "PRIMASK=%02lX, FAULTMASK=%02lX, BASEPRI=%02lX, CONTROL=%02lX\r\n",
        primask, __get_FAULTMASK(), __get_BASEPRI(), __get_CONTROL());
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    len = snprintf(crash_buf, sizeof(crash_buf), "XPSR=%08lX", regs[16]);
    len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, ", APSR=%c%c%c%c%c",
        (regs[16] & (1 << 31)) ? 'N' : 'n',
        (regs[16] & (1 << 30)) ? 'Z' : 'z',
        (regs[16] & (1 << 29)) ? 'C' : 'c',
        (regs[16] & (1 << 28)) ? 'V' : 'v',
        (regs[16] & (1 << 27)) ? 'Q' : 'q'
        );
    val = regs[16] & 0xFF;
    len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, ", EPSR=%08lX, IPSR=%02lX", regs[16] & 0x0700FE00, val);
    if (val == 0) {
        len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (NoException)");
    }
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    hal_trace_flush_buffer();

    len = snprintf(crash_buf, sizeof(crash_buf), "ICSR =%08lX, AIRCR=%08lX, SCR  =%08lX, CCR  =%08lX\r\n", SCB->ICSR, SCB->AIRCR, SCB->SCR, SCB->CCR);
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    len = snprintf(crash_buf, sizeof(crash_buf), "SHCSR=%08lX, CFSR =%08lX, HFSR =%08lX, AFSR =%08lX\r\n", SCB->SHCSR, SCB->CFSR, SCB->HFSR, SCB->AFSR);
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    len = snprintf(crash_buf, sizeof(crash_buf), "MMFAR=%08lX, BFAR =%08lX\r\n", SCB->MMFAR, SCB->BFAR);
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);

    len = snprintf(crash_buf, sizeof(crash_buf), "FaultInfo :");
    if ((SCB->SHCSR & 0xF) == 0) {
        len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (None)");
    } else {
        if (SCB->SHCSR & (1 << 0)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (MemFault)");
        }
        if (SCB->SHCSR & (1 << 1)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (BusFault)");
        }
        if (SCB->SHCSR & (1 << 3)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (UsageFault)");
        }
    }
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    len = snprintf(crash_buf, sizeof(crash_buf), "FaultCause:");
    if (SCB->CFSR == 0) {
        len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (None)");
    } else {
        if (SCB->CFSR & (1 << 0)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Instruction access violation)");
        }
        if (SCB->CFSR & (1 << 1)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Data access violation)");
        }
        if (SCB->CFSR & (1 << 3)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (MemFault on unstacking for a return from exception)");
        }
        if (SCB->CFSR & (1 << 4)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (MemFault on stacking for exception entry)");
        }
        if (SCB->CFSR & (1 << 5)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (MemFault during floating-point lazy state preservation)");
        }
        if (SCB->CFSR & (1 << 7)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (MMFAR valid)");
        }
        if (len) {
            hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
            hal_trace_flush_buffer();
            len = 0;
        }
        if (SCB->CFSR & (1 << 8)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Instruction bus error)");
        }
        if (SCB->CFSR & (1 << 9)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Precise data bus error)");
        }
        if (SCB->CFSR & (1 << 10)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Imprecise data bus error)");
        }
        if (SCB->CFSR & (1 << 11)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (BusFault on unstacking for a return from exception)");
        }
        if (SCB->CFSR & (1 << 12)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (BusFault on stacking for exception entry)");
        }
        if (SCB->CFSR & (1 << 13)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (BusFault during floating-point lazy state preservation)");
        }
        if (SCB->CFSR & (1 << 15)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (BFAR valid)");
        }
        if (len) {
            hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
            hal_trace_flush_buffer();
            len = 0;
        }
        if (SCB->CFSR & (1 << 16)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Undefined instruction UsageFault)");
        }
        if (SCB->CFSR & (1 << 17)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Invalid state UsageFault)");
        }
        if (SCB->CFSR & (1 << 18)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Invalid PC load by EXC_RETURN UsageFault)");
        }
        if (SCB->CFSR & (1 << 19)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (No coprocessor UsageFault)");
        }
        if (SCB->CFSR & (1 << 24)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Unaligned access UsageFault)");
        }
        if (SCB->CFSR & (1 << 25)) {
            len += snprintf(&crash_buf[len], sizeof(crash_buf) - len, " (Divide by zero UsageFault)");
        }
    }
    hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    hal_trace_flush_buffer();

    hal_trace_crash_dump_output((const unsigned char *)newline, sizeof(newline) - 1);

    stack = (uint32_t *)(regs[13] & ~3);
    for (i = 0; i < 256; i += 4) {
        len = snprintf(crash_buf, sizeof(crash_buf), "%08lX: %08lX %08lX %08lX %08lX\r\n",
            (uint32_t)&stack[i], stack[i], stack[i + 1], stack[i + 2], stack[i + 3]);
        hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    }

    stack =  (uint32_t *)(__get_MSP() & ~3);
    for (i = 0; i < 32; i += 4) {
        len = snprintf(crash_buf, sizeof(crash_buf), "%08lX: %08lX %08lX %08lX %08lX\r\n",
            (uint32_t)&stack[i], stack[i], stack[i + 1], stack[i + 2], stack[i + 3]);
        hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    }

    stack =  (uint32_t *)(__get_PSP() & ~3);
    for (i = 0; i < 32; i += 4) {
        len = snprintf(crash_buf, sizeof(crash_buf), "%08lX: %08lX %08lX %08lX %08lX\r\n",
            (uint32_t)&stack[i], stack[i], stack[i + 1], stack[i + 2], stack[i + 3]);
        hal_trace_crash_dump_output((unsigned char *)crash_buf, len);
    }
    hal_trace_flush_buffer();

    hal_trace_crash_dump_callback();

    time = hal_sys_timer_get();
    while (hal_trace_busy() && hal_sys_timer_get() - time < MS_TO_TICKS(100));
#endif

    hal_trace_crash_end();
}

static void NAKED hal_trace_fault_handler(void)
{
    register uint32_t dump_func asm("r2") = (uint32_t)hal_trace_fault_dump;

    asm volatile (
        "tst lr, #0x04 \n"
        "ite eq \n"
        // Using MSP
        "mrseq r3, msp \n"
        // Using PSP
        "mrsne r3, psp \n"
        "tst lr, #0x10 \n"
        "ite eq \n"
        // FPU context saved
        "addeq r1, r3, #8*4 \n"
        // No FPU context
        "movne r1, #0 \n"
        // Make room for r0-r15,psr
        "sub sp, #17*4 \n"
        // Save r4-r11
        "add r0, sp, #4*4 \n"
        "stm r0, {r4-r11} \n"
        // Save r0-r3
        "ldm r3, {r4-r7} \n"
        "stm sp, {r4-r7} \n"
        // Save r12
        "ldr r0, [r3, #4*4] \n"
        "str r0, [sp, #12*4] \n"
        // Save sp
        "teq r1, 0 \n"
        "ite eq \n"
        "addeq r0, r3, #8*4 \n"
        "addne r0, r1, #17*4 \n"
        // -- Check CCR.STKALIGN
        "ldr r4, =0xE000ED14 \n"
        "ldr r4, [r4] \n"
        "tst r4, #(1 << 9) \n"
        "itt ne \n"
        "addne r0, #4 \n"
        "bfcne r0, 0, #3 \n"
        "str r0, [sp, #13*4] \n"
        // Save lr
        "ldr r0, [r3, #5*4] \n"
        "str r0, [sp, #14*4] \n"
        // Save pc
        "ldr r0, [r3, #6*4] \n"
        "str r0, [sp, #15*4] \n"
        // Save PSR
        "ldr r0, [r3, #7*4] \n"
        "str r0, [sp, #16*4] \n"
        // Save current exception lr
        "mov r4, lr \n"
        // Invoke the fault handler
        "mov r0, sp \n"
        "blx %0 \n"
        // Restore current exception lr
        "mov lr, r4 \n"
        // Restore r4-r7
        "add r0, sp, #4*4 \n"
        "ldm r0, {r4-r7} \n"
        "mov r0, r3 \n"
        // Restore sp
        "add sp, #17*4 \n"
        ""
        :
        : "r"(dump_func)
        );
}
#endif

void hal_trace_crashdump_print(void)
{
    struct CRASH_OUTPUT_BUF_T *p;
    uint32_t send_frame_size;   
    uint32_t sent_cnt;

    crash_dump_init = true;
    for (p = (struct CRASH_OUTPUT_BUF_T *)__crash_dump_start; p < (struct CRASH_OUTPUT_BUF_T *)__crash_dump_end; p++){
        if (p->seqnum != 0xffffffff){           
            app_audio_mempool_init();           
            app_audio_mempool_get_buff((uint8_t **)&crash_output_p, sizeof(struct CRASH_OUTPUT_BUF_T));
            hal_trace_crash_dump_output(p->buf, p->bufpos);
            sent_cnt = 0;
            do{             
                send_frame_size = (crash_output_p->bufpos-sent_cnt) < (TRACE_BUF_SIZE-4)?
                                   crash_output_p->bufpos-sent_cnt:
                                   TRACE_BUF_SIZE-4;

                hal_trace_flush_buffer();
                hal_trace_output(crash_output_p->buf + sent_cnt, send_frame_size);
                hal_trace_flush_buffer();
                sent_cnt += send_frame_size;
            }while(sent_cnt < crash_output_p->bufpos);
        }
    }
    crash_dump_init = false;
}

void os_error_str (const char *fmt, ...) __attribute__((alias("hal_trace_assert_dump")));


const struct HAL_IOMUX_PIN_FUNCTION_MAP pinmux_tport[] = {
    {HAL_IOMUX_PIN_P3_4, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    {HAL_IOMUX_PIN_P3_6, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
};

int hal_trace_tportopen(void)
{
    int i;

    for (i=0;i<sizeof(pinmux_tport)/sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP);i++){
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&pinmux_tport[i], 1);
        hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)pinmux_tport[i].pin, HAL_GPIO_DIR_OUT, 0);
    }
    return 0;
}

int hal_trace_tportset(int port)
{
    hal_gpio_pin_set((enum HAL_GPIO_PIN_T)pinmux_tport[port].pin);
    return 0;
}

int hal_trace_tportclr(int port)
{
    hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)pinmux_tport[port].pin);
    return 0;
}




#ifdef HAL_TRACE_RX_ENABLE
#define HAL_TRACE_RX_HEAD_SIZE   4
#define HAL_TRACE_RX_NAME_SIZE   12
#define HAL_TRACE_RX_BUF_SIZE    1024
#define HAL_TRACE_RX_ROLE_NUM    6

#define HAL_TRACE_RX_HEAD       '['
#define HAL_TRACE_RX_END        ']'
#define HAL_TRACE_RX_SEPARATOR  ','

typedef struct{
    char *name;
    uint32_t len;
    uint8_t  *buf;
}HAL_TRACE_RX_CFG_T;

typedef struct{
    // uint32_t    used;
    char name[HAL_TRACE_RX_NAME_SIZE];
    HAL_TRACE_RX_CALLBACK_T callback;
}HAL_TRACE_RX_LIST_T;

typedef struct{
    uint32_t list_num;
    HAL_TRACE_RX_LIST_T list[HAL_TRACE_RX_ROLE_NUM];

    uint32_t rx_enable;
    uint32_t pos;
    uint8_t  buf[HAL_TRACE_RX_BUF_SIZE];
}HAL_TRACE_RX_T;

HAL_TRACE_RX_T hal_trace_rx;

int hal_trace_rx_dump_list(void)
{
    for(int i=0;i<HAL_TRACE_RX_ROLE_NUM;i++)
    {
        TRACE("%d: %s", i, hal_trace_rx.list[i].name);
    }
    return 0;
}

int hal_trace_rx_is_in_list(char *name)
{
    for(int i=0;i<HAL_TRACE_RX_ROLE_NUM;i++)
    {
        if(!strcmp(hal_trace_rx.list[i].name, name))
        {
            return i;
        }
    }
    hal_trace_rx_dump_list();
    // TRACE("%s", hal_trace_rx.list[0].name);
    // TRACE("%s", name);
    // TRACE("%d", strlen(hal_trace_rx.list[0].name));
    // TRACE("%d", strlen(name));
    // TRACE("%d", strcmp(hal_trace_rx.list[0].name, name));
    return -1;
}

int hal_trace_rx_add_item_to_list(char *name, HAL_TRACE_RX_CALLBACK_T callback)
{
    for(int i=0;i<HAL_TRACE_RX_ROLE_NUM;i++)
    {
        if(hal_trace_rx.list[i].name[0] == 0)
        {
            memcpy(hal_trace_rx.list[i].name, name, strlen(name));
            hal_trace_rx.list[i].callback = callback;
            hal_trace_rx.list_num++;
            return 0;
        }
    }

    return 1;
}

int hal_trace_rx_del_item_to_list(int id)
{
    memset(hal_trace_rx.list[id].name, 0, sizeof(hal_trace_rx.list[id].name));
    hal_trace_rx.list[id].callback = NULL;
    hal_trace_rx.list_num--;

    return 0;
}

int hal_trace_rx_register(char *name, HAL_TRACE_RX_CALLBACK_T callback)
{
    if(hal_trace_rx_is_in_list(name) == -1)
    {
        hal_trace_rx_add_item_to_list(name, callback);
        return 0;
    }
    else
    {
        return 0;
    }
}

int hal_trace_rx_deregister(char *name)
{
    int id = 0;

    id = hal_trace_rx_is_in_list(name);

    if(id != -1)
    {
        hal_trace_rx_del_item_to_list(id);
        return 0;
    }
    else
    {
        return 1;
    }
}

static int hal_trace_rx_reset(void)
{

    hal_trace_rx.pos = 0;
    memset(hal_trace_rx.buf, 0, HAL_TRACE_RX_BUF_SIZE);

    return 0;
}

// [test,12,102.99]
static int hal_trace_rx_parse(int8_t *buf, HAL_TRACE_RX_CFG_T *cfg)
{
    int pos = 0;

    for(; pos<HAL_TRACE_RX_HEAD_SIZE; pos++)
    {
        if(buf[pos] == HAL_TRACE_RX_HEAD)
        {
            buf[pos] = NULL;
            break;
        }
    }

    if(pos == HAL_TRACE_RX_HEAD_SIZE)
    {
        return 3;
    }

    pos++;

    cfg->name = (char *)(buf+pos);
    for(; pos<HAL_TRACE_RX_NAME_SIZE+HAL_TRACE_RX_HEAD_SIZE; pos++)
    {
        if(buf[pos] == HAL_TRACE_RX_SEPARATOR)
        {
            buf[pos] = 0;
            break;
        }
    }

    TRACE("%d", strlen(cfg->name));

    if(pos == HAL_TRACE_RX_NAME_SIZE)
    {
        return 1;
    }

    pos++;

    cfg->buf = (char *)(buf+pos);
    for(; pos<HAL_TRACE_RX_BUF_SIZE; pos++)
    {
        if(buf[pos] == HAL_TRACE_RX_END)
        {
            buf[pos] = 0;
            break;
        }
    }

    cfg->len = strlen(cfg->buf);

    if(pos == HAL_TRACE_RX_BUF_SIZE)
    {
        return 2;
    }

    return 0;
}

int hal_trace_rx_process(uint8_t *buf)
{
    HAL_TRACE_RX_CFG_T cfg;
    int id = 0;
    int res = 0;

    res = hal_trace_rx_parse(buf, &cfg);

    if(res)
    {
        TRACE("ERROR: hal_trace_rx_parse %d", res);
        return 1;
    }
    else
    {
        TRACE("%s rx OK", cfg.name);
    }

    id = hal_trace_rx_is_in_list(cfg.name);

    if(id == -1)
    {
        TRACE("%s is invalid", cfg.name);
        return -1;
    }

    if(hal_trace_rx.list[id].callback)
    {
        hal_trace_rx.list[id].callback(cfg.buf, cfg.len);
    }
    else
    {
        TRACE("%s has not callback", hal_trace_rx.list[id].name);
    }

    return 0;
}

void hal_trace_rx_rx_irq_handler(enum HAL_UART_ID_T id, union HAL_UART_IRQ_T status)
{
    // status.RT
    if(status.RX)
    {
        while(hal_uart_readable(trace_uart))
        {
            hal_trace_rx.buf[hal_trace_rx.pos] = hal_uart_getc(trace_uart);
            if(hal_trace_rx.buf[hal_trace_rx.pos] == HAL_TRACE_RX_END)
            {
                hal_trace_rx.buf[hal_trace_rx.pos + 1] = 0;
                TRACE("%s", hal_trace_rx.buf);
                hal_trace_rx_process(hal_trace_rx.buf);
                hal_trace_rx_reset();
                hal_trace_flush_buffer();
            }
            else
            {
                hal_trace_rx.pos++;
            }
        }
    }
    else
    {
        //trace_rx_flag = false;
    }
}

uint32_t app_test_callback(unsigned char *buf, uint32_t len)
{
    TRACE("[%s] len = %d", __func__, len);

    // Process string
    int num_int = 0;
    float num_float = 0.0;
    hal_trace_rx_parser(buf, "%d,%f", &num_int, &num_float);

    TRACE("[%s] %d:%f", __func__, num_int, num_float);


    return 0;
}

int hal_trace_rx_open()
{
    union HAL_UART_IRQ_T mask;

    // init parameter
    memset(&hal_trace_rx, 0, sizeof(hal_trace_rx));

    // init hw
    mask.reg = 0;
    mask.BE = 1;
    mask.FE = 1;
    mask.OE = 1;
    mask.PE = 1;
    mask.RT = 1;
    mask.RX = 1;
    hal_uart_irq_set_mask(trace_uart, mask);
    hal_uart_irq_set_handler(trace_uart, hal_trace_rx_rx_irq_handler);
    hal_trace_rx_register("test", (HAL_TRACE_RX_CALLBACK_T)app_test_callback);
//    hal_uart_irq_set_dma_handler(trace_uart, trace_rx, hal_trace_uart_xfer_done, NULL);
//    hal_uart_dma_recv(trace_uart, trace_rx_data, 4, NULL, NULL);
    return 0;
}
#endif
