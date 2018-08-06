#ifndef __APP_UTILS_H__
#define __APP_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_sysfreq.h"

enum APP_SYSFREQ_USER_T {
    APP_SYSFREQ_USER_APP_0 = HAL_SYSFREQ_USER_APP_0,
    APP_SYSFREQ_USER_APP_1 = HAL_SYSFREQ_USER_APP_1,
    APP_SYSFREQ_USER_APP_2 = HAL_SYSFREQ_USER_APP_2,
    APP_SYSFREQ_USER_APP_3 = HAL_SYSFREQ_USER_APP_3,
//#ifdef CMU_FREQ_52M_IN_ACTIVE_MOODE
    APP_SYSFREQ_APP_COMM = HAL_SYSFREQ_USER_APP_COMM,
//#endif

    APP_SYSFREQ_USER_QTY
};

enum APP_SYSFREQ_FREQ_T {
    APP_SYSFREQ_32K =  HAL_CMU_FREQ_32K,
    APP_SYSFREQ_26M =  HAL_CMU_FREQ_26M,
    APP_SYSFREQ_52M =  HAL_CMU_FREQ_52M,
    APP_SYSFREQ_78M =  HAL_CMU_FREQ_78M,
    APP_SYSFREQ_104M =  HAL_CMU_FREQ_104M,
    APP_SYSFREQ_208M =  HAL_CMU_FREQ_208M,

    APP_SYSFREQ_FREQ_QTY =  HAL_CMU_FREQ_QTY
};

enum APP_WDT_THREAD_CHECK_USER_T {
    APP_WDT_THREAD_CHECK_USER_APP,
    APP_WDT_THREAD_CHECK_USER_AF,
    APP_WDT_THREAD_CHECK_USER_BT,
    APP_WDT_THREAD_CHECK_USER_3,
    APP_WDT_THREAD_CHECK_USER_4,
    APP_WDT_THREAD_CHECK_USER_5,
    APP_WDT_THREAD_CHECK_USER_6,
    APP_WDT_THREAD_CHECK_USER_7,
    APP_WDT_THREAD_CHECK_USER_8,
    APP_WDT_THREAD_CHECK_USER_9,
    APP_WDT_THREAD_CHECK_USER_10,
    
    APP_WDT_THREAD_CHECK_USER_QTY
};

int app_sysfreq_req(enum APP_SYSFREQ_USER_T user, enum APP_SYSFREQ_FREQ_T freq);

void app_wdt_ping(void);

int app_wdt_open(int seconds);

int app_wdt_close(void);

int app_wdt_thread_check_open(void);

int app_wdt_thread_check_enable(enum APP_WDT_THREAD_CHECK_USER_T user, int seconds);

int app_wdt_thread_check_disable(enum APP_WDT_THREAD_CHECK_USER_T user);

int app_wdt_thread_check_ping(enum APP_WDT_THREAD_CHECK_USER_T user);

int app_wdt_thread_check_handle(void);

#ifdef __cplusplus
}
#endif

#endif//__FMDEC_H__
