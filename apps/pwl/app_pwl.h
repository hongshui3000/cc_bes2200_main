#ifndef __APP_PWL_H__
#define __APP_PWL_H__

#include "tgt_hardware.h"
#include "hal_cmu.h"//Modified by ATX 

#ifdef __cplusplus
extern "C" {
#endif

//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
#if (CFG_HW_PLW_NUM == 1)
enum APP_PWL_ID_T {
    APP_PWL_ID_0 = HAL_PWM_ID_2,
    APP_PWL_ID_1,

    APP_PWL_ID_QTY = APP_PWL_ID_1
};
#elif (CFG_HW_PLW_NUM == 2)
enum APP_PWL_ID_T {
    APP_PWL_ID_0 = 0,
    APP_PWL_ID_1,

    APP_PWL_ID_QTY
};
#else
#error "NOT SUPPORT"
#endif
#else
#if (CFG_HW_PLW_NUM == 1)
enum APP_PWL_ID_T {
    APP_PWL_ID_0 = 0,
    APP_PWL_ID_1 = 1,

    APP_PWL_ID_QTY = 1
};
#elif (CFG_HW_PLW_NUM == 2)
enum APP_PWL_ID_T {
    APP_PWL_ID_0 = 0,
    APP_PWL_ID_1,

    APP_PWL_ID_QTY
};
	//Modified by ATX : Parke.Wei_20180322
#elif (CFG_HW_PLW_NUM == 3)
enum APP_PWL_ID_T {
	APP_PWL_ID_0 = 0,
	APP_PWL_ID_1,
	APP_PWL_ID_2,

	APP_PWL_ID_QTY
};
#else
#error "NOT SUPPORT"
#endif
#endif
//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
struct PWL_CYCLE_ONCE_T {
    uint8_t  level;
    uint32_t time;
};

struct APP_PWL_CFG_T {
    struct PWL_CYCLE_ONCE_T *part;
    uint8_t parttotal;
    uint8_t startlevel;
    bool periodic;    
    bool dynamic_light;
};
#else
struct APP_PWL_CFG_T {
    struct PWL_CYCLE_ONCE {
        uint8_t  level;
        uint32_t time;
    }part[10];
    uint8_t parttotal;
    uint8_t startlevel;
    bool periodic;
};
#endif
int app_pwl_open(void);

int app_pwl_start(enum APP_PWL_ID_T id);

int app_pwl_setup(enum APP_PWL_ID_T id, struct APP_PWL_CFG_T *cfg);

int app_pwl_stop(enum APP_PWL_ID_T id);

int app_pwl_close(void);

#ifdef __cplusplus
}
#endif

#endif

