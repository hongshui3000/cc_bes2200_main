#ifndef __HAL_TRACE_H__
#define __HAL_TRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "plat_types.h"

//#define AUDIO_DEBUG
//#define HAL_TRACE_RX_ENABLE

enum HAL_TRACE_MODUAL {
    HAL_TRACE_LEVEL_0  = 1<<0,
    HAL_TRACE_LEVEL_1  = 1<<1,
    HAL_TRACE_LEVEL_2  = 1<<2,
    HAL_TRACE_LEVEL_3  = 1<<3,
    HAL_TRACE_LEVEL_4  = 1<<4,
    HAL_TRACE_LEVEL_5  = 1<<5,
    HAL_TRACE_LEVEL_6  = 1<<6,
    HAL_TRACE_LEVEL_7  = 1<<7,
    HAL_TRACE_LEVEL_8  = 1<<8,
    HAL_TRACE_LEVEL_9  = 1<<9,
    HAL_TRACE_LEVEL_10 = 1<<10,
    HAL_TRACE_LEVEL_11 = 1<<11,
    HAL_TRACE_LEVEL_12 = 1<<12,
    HAL_TRACE_LEVEL_13 = 1<<13,
    HAL_TRACE_LEVEL_14 = 1<<14,
    HAL_TRACE_LEVEL_15 = 1<<15,
    HAL_TRACE_LEVEL_16 = 1<<16,
    HAL_TRACE_LEVEL_17 = 1<<17,
    HAL_TRACE_LEVEL_18 = 1<<18,
    HAL_TRACE_LEVEL_19 = 1<<19,
    HAL_TRACE_LEVEL_20 = 1<<20,
    HAL_TRACE_LEVEL_21 = 1<<21,
    HAL_TRACE_LEVEL_22 = 1<<22,
    HAL_TRACE_LEVEL_23 = 1<<23,
    HAL_TRACE_LEVEL_24 = 1<<24,
    HAL_TRACE_LEVEL_25 = 1<<25,
    HAL_TRACE_LEVEL_26 = 1<<26,
    HAL_TRACE_LEVEL_27 = 1<<27,
    HAL_TRACE_LEVEL_28 = 1<<28,
    HAL_TRACE_LEVEL_29 = 1<<29,
    HAL_TRACE_LEVEL_30 = 1<<30,
    HAL_TRACE_LEVEL_31 = 1<<31,
    HAL_TRACE_LEVEL_ALL = 0XFFFFFFFF,
};

#if defined(__PRODUCT_TEST__)
#define HAL_TRACE_RX_ENABLE
int hal_product_test_trace_printf(const char *fmt, ...);
int hal_product_test_trace_dump (const char *fmt, unsigned int size,  unsigned int count, const void *buffer);
#endif //__PRODUCT_TEST__

#define TRACE_HIGHPRIO(str, ...)    hal_trace_crash_printf(str, ##__VA_ARGS__)
#define TRACE_HIGHPRIO_IMM(str, ...)    hal_trace_crash_printf(str, ##__VA_ARGS__)
#define TRACE_HIGHPRIO_NOCRLF(str, ...)      hal_trace_crash_printf_without_crlf(str, ##__VA_ARGS__)

#if defined(DEBUG) && !defined(AUDIO_DEBUG)
#define TRACE(str, ...)             hal_trace_printf(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_NOCRLF(str, ...)      hal_trace_printf_without_crlf(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_IMM(str, ...)         hal_trace_printf_imm(HAL_TRACE_LEVEL_0, str, ##__VA_ARGS__)
#define TRACE_OUTPUT(str, len)      hal_trace_output(str, len)
#define FUNC_ENTRY_TRACE()          hal_trace_printf(HAL_TRACE_LEVEL_0, __FUNCTION__)
#define DUMP8(str, buf, cnt)        hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint8_t), cnt, buf)
#define DUMP16(str, buf, cnt)       hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint16_t), cnt, buf)
#define DUMP32(str, buf, cnt)       hal_trace_dump(HAL_TRACE_LEVEL_0, str, sizeof(uint32_t), cnt, buf)
#else
// To avoid warnings on unused variables
#define TRACE(str, ...)             hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_NOCRLF(str, ...)      hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_IMM(str, ...)         hal_trace_dummy(str, ##__VA_ARGS__)
#define TRACE_OUTPUT(str, len)      hal_trace_dummy(str, len)
#define FUNC_ENTRY_TRACE()          hal_trace_dummy(NULL)
#define DUMP8(str, buf, cnt)        hal_trace_dummy(str, buf, cnt)
#define DUMP16(str, buf, cnt)       hal_trace_dummy(str, buf, cnt)
#define DUMP32(str, buf, cnt)       hal_trace_dummy(str, buf, cnt)
#endif

#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FILE__, __FUNCTION__, __LINE__, str, ##__VA_ARGS__); } }
#elif defined(DEBUG) && defined(ASSERT_SHOW_FILE)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FILE__, __LINE__, str, ##__VA_ARGS__); } }
#elif defined(DEBUG) && defined(ASSERT_SHOW_FUNC)
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(__FUNCTION__, __LINE__, str, ##__VA_ARGS__); } }
#else
#define ASSERT(cond, str, ...)      { if (!(cond)) { hal_trace_assert_dump(str, ##__VA_ARGS__); } }
#endif

// Avoid CPU instruction fetch blocking the system bus on BEST1000
#define SAFE_PROGRAM_STOP()         do { asm volatile("nop \n nop \n nop \n nop"); } while (1)

enum HAL_TRACE_TRANSPORT_T {
#ifdef CHIP_HAS_USB
    HAL_TRACE_TRANSPORT_USB,
#endif
    HAL_TRACE_TRANSPORT_UART0,
#if (CHIP_HAS_UART > 1)
    HAL_TRACE_TRANSPORT_UART1,
#endif

    HAL_TRACE_TRANSPORT_QTY
};

typedef void (*HAL_TRACE_CRASH_DUMP_CB_T)(void);

int hal_trace_switch(enum HAL_TRACE_TRANSPORT_T transport);
#if defined(TRACE_DUMP2FLASH)
int hal_trace_dumper_open(void);
#endif

int hal_trace_open(enum HAL_TRACE_TRANSPORT_T transport);

int hal_trace_close(void);

int hal_trace_output(const unsigned char *buf, unsigned int buf_len);

int hal_trace_printf(uint32_t lvl, const char *fmt, ...);

int hal_trace_printf_without_crlf(uint32_t lvl, const char *fmt, ...);

void hal_trace_printf_imm(uint32_t lvl, const char *fmt, ...);

int hal_trace_printf_without_crlf_fix_arg(uint32_t lvl, const char *fmt);

int hal_trace_printf_with_tag(uint32_t lvl, const char *tag, const char *fmt, ...);

int hal_trace_dump(uint32_t lvl, const char *fmt, unsigned int size,  unsigned int count, const void *buffer);

int hal_trace_busy(void);

void hal_trace_idle_send(void);

int hal_trace_crash_dump_register(HAL_TRACE_CRASH_DUMP_CB_T cb);

int hal_trace_crash_printf(const char *fmt, ...);

int hal_trace_crash_printf_without_crlf(const char *fmt, ...);

void hal_trace_crashdump_dumptoflash(void);

void hal_trace_flash_flush(void);

static inline void hal_trace_dummy(const char *fmt, ...) { }


int hal_trace_tportopen(void);

int hal_trace_tportset(int port);

int hal_trace_tportclr(int port);



#if defined(DEBUG) && defined(ASSERT_SHOW_FILE_FUNC)
#define ASSERT_DUMP_ARGS    const char *file, const char *func, unsigned int line, const char *fmt, ...
#elif defined(DEBUG) && (defined(ASSERT_SHOW_FILE) || defined(ASSERT_SHOW_FUNC))
#define ASSERT_DUMP_ARGS    const char *scope, unsigned int line, const char *fmt, ...
#else
#define ASSERT_DUMP_ARGS    const char *fmt, ...
#endif
void NORETURN hal_trace_assert_dump(ASSERT_DUMP_ARGS);


//==============================================================================
// AUDIO_DEBUG
//==============================================================================

#ifdef AUDIO_DEBUG
#define AUDIO_DEBUG_TRACE(str, ...)         hal_trace_printf(str, ##__VA_ARGS__)
#define AUDIO_DEBUG_DUMP(buf, cnt)          hal_trace_output(buf, cnt)
#endif


//==============================================================================
// TRACE RX
//==============================================================================

#ifdef HAL_TRACE_RX_ENABLE

#include "stdio.h"

#define hal_trace_rx_parser(buf, str, ...)  sscanf(buf, str, ##__VA_ARGS__)

typedef unsigned int (*HAL_TRACE_RX_CALLBACK_T)(unsigned char *buf, unsigned int  len);

int hal_trace_rx_register(char *name, HAL_TRACE_RX_CALLBACK_T callback);
int hal_trace_rx_deregister(char *name);

#endif

#ifdef __cplusplus
}
#endif

#endif
