#include "plat_types.h"
#include "hal_sysfreq.h"
#include "hal_location.h"
#include "hal_trace.h"
#include "cmsis.h"
#ifndef ROM_BUILD
#include "pmu.h"
#endif

static uint32_t BOOT_BSS_LOC sysfreq_bundle[(HAL_SYSFREQ_USER_QTY + 3) / 4];

static uint8_t * const sysfreq_per_user = (uint8_t *)&sysfreq_bundle[0];

static enum HAL_SYSFREQ_USER_T BOOT_DATA_LOC top_user = HAL_SYSFREQ_USER_QTY;

static enum HAL_CMU_FREQ_T hal_sysfreq_revise_freq(enum HAL_CMU_FREQ_T freq)
{
    return freq >= HAL_CMU_FREQ_26M ? freq : HAL_CMU_FREQ_26M;
}

int hal_sysfreq_req(enum HAL_SYSFREQ_USER_T user, enum HAL_CMU_FREQ_T freq)
{
    uint32_t lock;
    enum HAL_CMU_FREQ_T cur_sys_freq;
    int i;

    if (user >= HAL_SYSFREQ_USER_QTY) {
        return 1;
    }
    if (freq >= HAL_CMU_FREQ_QTY) {
        return 2;
    }

//#ifdef CMU_FREQ_52M_IN_ACTIVE_MOODE
    if (freq < HAL_CMU_FREQ_52M &&
        freq > HAL_CMU_FREQ_32K){
        freq = HAL_CMU_FREQ_52M;
    }
//#endif

    lock = int_lock();

    cur_sys_freq = hal_sysfreq_get();

    sysfreq_per_user[user] = freq;

    if (freq == cur_sys_freq) {
        top_user = user;
    } else if (freq > cur_sys_freq) {
        top_user = user;
        freq = hal_sysfreq_revise_freq(freq);
#ifndef ROM_BUILD
        pmu_sys_freq_config(freq);
#endif
        hal_cmu_sys_set_freq(freq);
    } else /* if (freq < cur_sys_freq) */ {
        // When top_user is HAL_SYSFREQ_USER_QTY, not to update top_user
        if (top_user == user) {
            freq = sysfreq_per_user[0];
            user = 0;
            for (i = 1; i < HAL_SYSFREQ_USER_QTY; i++) {
                if (freq < sysfreq_per_user[i]) {
                    freq = sysfreq_per_user[i];
                    user = i;
                }
            }
            top_user = user;
            if (freq != cur_sys_freq) {
                freq = hal_sysfreq_revise_freq(freq);
                hal_cmu_sys_set_freq(freq);
#ifndef ROM_BUILD
                pmu_sys_freq_config(freq);
#endif
            }
        }
    }

    int_unlock(lock);

    return 0;
}

enum HAL_CMU_FREQ_T hal_sysfreq_get(void)
{
    if (top_user < HAL_SYSFREQ_USER_QTY) {
        return sysfreq_per_user[top_user];
    } else {
        return hal_cmu_sys_get_freq();
    }
}

int hal_sysfreq_busy(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(sysfreq_bundle); i++) {
        if (sysfreq_bundle[i] != 0) {
            return 1;
        }
    }

    return 0;
}

void hal_sysfreq_print(void)
{
    int i;

    for (i = 0; i < HAL_SYSFREQ_USER_QTY; i++) {
        if (sysfreq_per_user[i] != 0) {
            TRACE("*** SYSFREQ user=%d freq=%d", i, sysfreq_per_user[i]);
        }
    }
    TRACE("*** SYSFREQ top_user=%d", top_user);
}

