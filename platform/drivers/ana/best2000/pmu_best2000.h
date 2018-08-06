#ifndef __PMU_BEST2000_H__
#define __PMU_BEST2000_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_iomux.h"
#include "hal_gpio.h"

void pmu_codec_hppa_enable(int enable);

void pmu_codec_mic_bias_enable(uint32_t map);

void pmu_led_set_direction(enum HAL_GPIO_PIN_T pin, enum HAL_GPIO_DIR_T dir);

enum HAL_GPIO_DIR_T pmu_led_get_direction(enum HAL_GPIO_PIN_T pin);

void pmu_led_set_voltage_domains(enum HAL_IOMUX_PIN_T pin, enum HAL_IOMUX_PIN_VOLTAGE_DOMAINS_T volt);

void pmu_led_set_pull_select(enum HAL_IOMUX_PIN_T pin, enum HAL_IOMUX_PIN_PULL_SELECT_T pull_sel);

#ifdef __cplusplus
}
#endif

#endif

