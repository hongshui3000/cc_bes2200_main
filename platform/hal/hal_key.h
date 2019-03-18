#ifndef __HAL_KEY_H__
#define __HAL_KEY_H__

#include "hal_gpio.h"
#include "hal_gpadc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _PROJ_2000IZ_C001_
#define CFG_SW_KEY_REPEAT_THRESH_MS         700
#define CFG_SW_KEY_LPRESS_THRESH_MS         500
#define CFG_SW_KEY_LLPRESS_THRESH_MS        3000
#define CFG_SW_KEY_DBLCLICK_THRESH_MS       450
#endif

#ifdef _PROJ_2000IZ_C005_
#define CFG_SW_KEY_REPEAT_THRESH_MS         700
#define CFG_SW_KEY_LPRESS_THRESH_MS         1000
#define CFG_SW_KEY_LLPRESS_THRESH_MS        5000
#define CFG_SW_KEY_DBLCLICK_THRESH_MS       450
#endif

#ifndef CFG_SW_KEY_LLPRESS_THRESH_MS
#define CFG_SW_KEY_LLPRESS_THRESH_MS        2500//Modified by ATX : Leon.He_20180110:change long long press to 2.5s
#endif
#ifndef CFG_SW_KEY_LPRESS_THRESH_MS
#define CFG_SW_KEY_LPRESS_THRESH_MS         1000//Modified by ATX : Leon.He_20180110:change long press to 1s
#endif
#ifndef CFG_SW_KEY_REPEAT_THRESH_MS
#define CFG_SW_KEY_REPEAT_THRESH_MS         500
#endif
#ifndef CFG_SW_KEY_DBLCLICK_THRESH_MS
#define CFG_SW_KEY_DBLCLICK_THRESH_MS       400
#endif
#ifndef CFG_SW_KEY_INIT_DOWN_THRESH_MS
#define CFG_SW_KEY_INIT_DOWN_THRESH_MS      200
#endif
#ifndef CFG_SW_KEY_INIT_LPRESS_THRESH_MS
#define CFG_SW_KEY_INIT_LPRESS_THRESH_MS    3000
#endif
#ifndef CFG_SW_KEY_INIT_LLPRESS_THRESH_MS
#define CFG_SW_KEY_INIT_LLPRESS_THRESH_MS   10000
#endif
#ifndef CFG_SW_KEY_CHECK_INTERVAL_MS
#define CFG_SW_KEY_CHECK_INTERVAL_MS        20//Modified by ATX : Leon.He_20190315: reduce the key check interval for reduce double, triple click failed rate
#endif

enum HAL_KEY_CODE_T {
	HAL_KEY_CODE_NONE = 0,
	HAL_KEY_CODE_PWR = (1 << 0),
	HAL_KEY_CODE_FN1 = (1 << 1),
	HAL_KEY_CODE_FN2 = (1 << 2),
	HAL_KEY_CODE_FN3 = (1 << 3),
	HAL_KEY_CODE_FN4 = (1 << 4),
	HAL_KEY_CODE_FN5 = (1 << 5),
	HAL_KEY_CODE_FN6 = (1 << 6),
	HAL_KEY_CODE_FN7 = (1 << 7),
	HAL_KEY_CODE_FN8 = (1 << 8),
	HAL_KEY_CODE_FN9 = (1 << 9),
	HAL_KEY_CODE_FN10 = (1 << 10),
	HAL_KEY_CODE_FN11 = (1 << 11),
	HAL_KEY_CODE_FN12 = (1 << 12),
	HAL_KEY_CODE_FN13 = (1 << 13),
	HAL_KEY_CODE_FN14 = (1 << 14),
	HAL_KEY_CODE_FN15 = (1 << 15),
};

enum HAL_KEY_EVENT_T {
	HAL_KEY_EVENT_NONE = 0,
	HAL_KEY_EVENT_DOWN,
	HAL_KEY_EVENT_UP,
	HAL_KEY_EVENT_LONGPRESS,
	HAL_KEY_EVENT_LONGLONGPRESS,
	HAL_KEY_EVENT_CLICK,
	HAL_KEY_EVENT_DOUBLECLICK,
	HAL_KEY_EVENT_TRIPLECLICK,
	HAL_KEY_EVENT_ULTRACLICK,
	HAL_KEY_EVENT_RAMPAGECLICK,
//Modified by ATX : cc_20180414: support seven click
	HAL_KEY_EVENT_SIXCLICK,
	HAL_KEY_EVENT_SEVENCLICK,
	HAL_KEY_EVENT_REPEAT,
	HAL_KEY_EVENT_GROUPKEY_DOWN,
	HAL_KEY_EVENT_GROUPKEY_REPEAT,
	HAL_KEY_EVENT_INITDOWN,
	HAL_KEY_EVENT_INITUP,
	HAL_KEY_EVENT_INITLONGPRESS,
	HAL_KEY_EVENT_INITLONGLONGPRESS,
	HAL_KEY_EVENT_INITFINISHED,

	HAL_KEY_EVENT_NUM,
};

#define KEY_EVENT_SET(a)                (1 << HAL_KEY_EVENT_ ## a)
#define KEY_EVENT_SET2(a, b)            (KEY_EVENT_SET(a) | KEY_EVENT_SET(b))
#define KEY_EVENT_SET3(a, b, c)         (KEY_EVENT_SET2(a, b) | KEY_EVENT_SET(c))
#define KEY_EVENT_SET4(a, b, c, d)      (KEY_EVENT_SET3(a, b, c) | KEY_EVENT_SET(d))

enum HAL_KEY_GPIOKEY_VAL_T {
    HAL_KEY_GPIOKEY_VAL_LOW = 0,
    HAL_KEY_GPIOKEY_VAL_HIGH,
};

struct HAL_KEY_GPIOKEY_CFG_T {
    enum HAL_KEY_CODE_T key_code;
    struct HAL_IOMUX_PIN_FUNCTION_MAP key_config;
    enum HAL_KEY_GPIOKEY_VAL_T key_down;
};

int hal_key_open(int checkPwrKey, int (* cb)(uint32_t, uint8_t));

enum HAL_KEY_EVENT_T hal_key_read_status(enum HAL_KEY_CODE_T code);

int hal_key_close(void);

#ifdef __cplusplus
	}
#endif

#endif//__FMDEC_H__
