#ifndef __HAL_SYSFREQ_H__
#define __HAL_SYSFREQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_cmu.h"

enum HAL_SYSFREQ_USER_T {
    HAL_SYSFREQ_USER_OVERLAY,
    HAL_SYSFREQ_USER_USB,
    HAL_SYSFREQ_USER_BT,

    HAL_SYSFREQ_USER_APP_0,
    HAL_SYSFREQ_USER_APP_1,
    HAL_SYSFREQ_USER_APP_2,
    HAL_SYSFREQ_USER_APP_3,
    HAL_SYSFREQ_USER_APP_COMM,

    HAL_SYSFREQ_USER_QTY
};

int hal_sysfreq_req(enum HAL_SYSFREQ_USER_T user, enum HAL_CMU_FREQ_T freq);

enum HAL_CMU_FREQ_T hal_sysfreq_get(void);

int hal_sysfreq_busy(void);

void hal_sysfreq_print(void);

#ifdef __cplusplus
}
#endif

#endif

