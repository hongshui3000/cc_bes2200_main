#include "plat_addr_map.h"
#include "hal_pwm.h"
#include "reg_pwm.h"
#include "hal_trace.h"
#include "cmsis.h"

#define PWM_SLOW_CLOCK                  32000
#define PWM_FAST_CLOCK                  13000000
#define PWM_MAX_VALUE                   0xFFFF

static struct PWM_T * const pwm[] = {
    (struct PWM_T *)PWM_BASE,
#ifdef CHIP_BEST2000
    (struct PWM_T *)AON_PWM_BASE,
#endif
};

static const enum HAL_CMU_MOD_ID_T pwm_o_mod[] = {
    HAL_CMU_MOD_O_PWM0,
#ifdef CHIP_BEST2000
    HAL_CMU_AON_O_PWM0,
#endif
};

static const enum HAL_CMU_MOD_ID_T pwm_p_mod[] = {
    HAL_CMU_MOD_P_PWM,
#ifdef CHIP_BEST2000
    HAL_CMU_AON_A_PWM,
#endif
};

int hal_pwm_enable(enum HAL_PWM_ID_T id, const struct HAL_PWM_CFG_T *cfg)
{
    uint32_t mod_freq;
    uint32_t load;
    uint32_t toggle;
    uint32_t lock;
    uint8_t ratio;
    uint8_t index;

    if (id >= HAL_PWM_ID_QTY) {
        return 1;
    }
    if (cfg->ratio > 100) {
        return 2;
    }

    if (cfg->inv && (cfg->ratio == 0 || cfg->ratio == 100)) {
        ratio = 100 - cfg->ratio;
    } else {
        ratio = cfg->ratio;
    }

    mod_freq = PWM_SLOW_CLOCK;
    if (ratio == 100) {
        load = PWM_MAX_VALUE;
        toggle = PWM_MAX_VALUE;
    } else if (ratio == 0) {
        load = 0;
        toggle = 0;
    } else {
        load = mod_freq / cfg->freq;
        toggle = load * ratio / 100;
        if (toggle == 0) {
            toggle = 1;
        }
        // 10% error is allowed
        if (!cfg->sleep_on && ABS((int)(toggle * 100 - load * ratio)) > load * 10) {
            mod_freq = PWM_FAST_CLOCK;
            load = mod_freq / cfg->freq;
            toggle = load * ratio / 100;
        }
        load = PWM_MAX_VALUE + 1 - load;
        toggle = PWM_MAX_VALUE - toggle;
    }

#ifdef CHIP_BEST2000
    index = (id < HAL_PWM2_ID_0) ? 0 : 1;
#else
    index = 0;
#endif

    if (hal_cmu_reset_get_status(pwm_o_mod[index] + id) == HAL_CMU_RST_SET) {
        hal_cmu_clock_enable(pwm_o_mod[index] + id);
        hal_cmu_clock_enable(pwm_p_mod[index]);
        hal_cmu_reset_clear(pwm_o_mod[index] + id);
        hal_cmu_reset_clear(pwm_p_mod[index]);
    } else {
        pwm[index]->EN &= ~(1 << id);
    }

    if (ratio == 0) {
        // Output 0 when disabled
        return 0;
    }

    hal_cmu_pwm_set_freq(id, mod_freq);

    lock = int_lock();

    if (id == HAL_PWM_ID_0) {
        pwm[index]->LOAD01 = SET_BITFIELD(pwm[index]->LOAD01, PWM_LOAD01_0, load);
        pwm[index]->TOGGLE01 = SET_BITFIELD(pwm[index]->TOGGLE01, PWM_TOGGLE01_0, toggle);
    } else if (id == HAL_PWM_ID_1) {
        pwm[index]->LOAD01 = SET_BITFIELD(pwm[index]->LOAD01, PWM_LOAD01_1, load);
        pwm[index]->TOGGLE01 = SET_BITFIELD(pwm[index]->TOGGLE01, PWM_TOGGLE01_1, toggle);
    } else if (id == HAL_PWM_ID_2) {
        pwm[index]->LOAD23 = SET_BITFIELD(pwm[index]->LOAD23, PWM_LOAD23_2, load);
        pwm[index]->TOGGLE23 = SET_BITFIELD(pwm[index]->TOGGLE23, PWM_TOGGLE23_2, toggle);
    } else {
        pwm[index]->LOAD23 = SET_BITFIELD(pwm[index]->LOAD23, PWM_LOAD23_3, load);
        pwm[index]->TOGGLE23 = SET_BITFIELD(pwm[index]->TOGGLE23, PWM_TOGGLE23_3, toggle);
    }

    if (cfg->inv) {
        pwm[index]->INV |= (1 << id);
    } else {
        pwm[index]->INV &= ~(1 << id);
    }

    pwm[index]->EN |= (1 << id);

    int_unlock(lock);

    return 0;
}

int hal_pwm_disable(enum HAL_PWM_ID_T id)
{
    uint8_t index;

    if (id >= HAL_PWM_ID_QTY) {
        return 1;
    }

#ifdef CHIP_BEST2000
    index = (id < HAL_PWM2_ID_0) ? 0 : 1;
#else
    index = 0;
#endif

    if (hal_cmu_reset_get_status(pwm_o_mod[index] + id) == HAL_CMU_RST_SET) {
        return 0;
    }

    pwm[index]->EN &= ~(1 << id);
    hal_cmu_reset_set(pwm_o_mod[index] + id);
    hal_cmu_clock_disable(pwm_o_mod[index] + id);
    if (pwm[index]->EN == 0) {
        hal_cmu_reset_set(pwm_p_mod[index]);
        hal_cmu_clock_disable(pwm_p_mod[index]);
    }

    return 0;
}

