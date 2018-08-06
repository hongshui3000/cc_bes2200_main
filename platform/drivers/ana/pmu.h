#ifndef __PMU_H__
#define __PMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "hal_analogif.h"
#include "hal_cmu.h"
#include "plat_addr_map.h"
#include CHIP_SPECIFIC_HDR(pmu)

#define pmu_read(reg,val)               hal_analogif_reg_read(reg,val)
#define pmu_write(reg,val)              hal_analogif_reg_write(reg,val)

#define  PMU_MANUAL_MODE                1
#define  PMU_AUTO_MODE                  0
#define  PMU_LDO_ON                     1
#define  PMU_LDO_OFF                    0
#define  PMU_LP_MODE_ON                 1
#define  PMU_LP_MODE_OFF                0
#define  PMU_DSLEEP_MODE_ON             1
#define  PMU_DSLEEP_MODE_OFF            0

union SECURITY_VALUE_T {
    struct {
        unsigned short security_en      :1;
        unsigned short key_id           :3;
        unsigned short key_chksum       :2;
        unsigned short vendor_id        :6;
        unsigned short vendor_chksum    :3;
        unsigned short chksum           :1;
    };
    unsigned short reg;
};

enum PMU_EFUSE_PAGE_T {
    PMU_EFUSE_PAGE_SECURITY     = 0,
    PMU_EFUSE_PAGE_BOOT         = 1,
    PMU_EFUSE_PAGE_FEATURE      = 2,
    PMU_EFUSE_PAGE_BATTER_LV    = 3,

    PMU_EFUSE_PAGE_BATTER_HV    = 4,
    PMU_EFUSE_PAGE_RESERVED_5   = 5,
    PMU_EFUSE_PAGE_RESERVED_6   = 6,
    PMU_EFUSE_PAGE_RESERVED_7   = 7,

    PMU_EFUSE_PAGE_RESERVED_8   = 8,
    PMU_EFUSE_PAGE_DCCALIB2_L   = 9,
    PMU_EFUSE_PAGE_DCCALIB2_R   = 10,
    PMU_EFUSE_PAGE_DCCALIB_L    = 11,

    PMU_EFUSE_PAGE_DCCALIB_R    = 12,
    PMU_EFUSE_PAGE_BONDING      = 13,
    PMU_EFUSE_PAGE_INTERNAL     = 14,
    PMU_EFUSE_PAGE_PROD_TEST    = 15,
};

enum PMU_CHARGER_STATUS_T {
    PMU_CHARGER_PLUGIN,
    PMU_CHARGER_PLUGOUT,
    PMU_CHARGER_UNKNOWN,
};

struct PMU_MODULE_CFG_T {
    unsigned short manual_bit;
    unsigned short ldo_en;
    unsigned short lp_mode;
    unsigned short dsleep_mode;
    unsigned short dsleep_v;
    unsigned short dsleep_v_shift;
    unsigned short normal_v;
    unsigned short normal_v_shift;
};

enum PMU_USB_PIN_CHK_STATUS_T {
    PMU_USB_PIN_CHK_NONE,
    // Chip acts as host
    PMU_USB_PIN_CHK_DEV_CONN,
    // Chip acts as host
    PMU_USB_PIN_CHK_DEV_DISCONN,
    // Chip acts as device
    PMU_USB_PIN_CHK_HOST_RESUME,

    PMU_USB_PIN_CHK_STATUS_QTY
};

enum PMU_USB_CONFIG_TYPE_T {
    PMU_USB_CONFIG_TYPE_NONE,
    PMU_USB_CONFIG_TYPE_DEVICE,
    PMU_USB_CONFIG_TYPE_HOST,
};

enum PMU_POWER_MODE_T {
    PMU_POWER_MODE_NONE,
    PMU_POWER_MODE_LDO,
    PMU_POWER_MODE_ANA_DCDC,
    PMU_POWER_MODE_DIG_DCDC,
};

enum PMU_VIORISE_REQ_USER_T {
   PMU_VIORISE_REQ_USER_PWL0,
   PMU_VIORISE_REQ_USER_PWL1,
   PMU_VIORISE_REQ_USER_FLASH,
   PMU_VIORISE_REQ_USER_CHARGER,

   PMU_VIORISE_REQ_USER_QTY
};

typedef void (*PMU_USB_PIN_CHK_CALLBACK)(enum PMU_USB_PIN_CHK_STATUS_T status);

typedef void (*PMU_RTC_IRQ_HANDLER_T)(uint32_t seconds);

typedef void (*PMU_CHARGER_IRQ_HANDLER_T)(enum PMU_CHARGER_STATUS_T status);

int pmu_open(void);

void pmu_sleep(void);

void pmu_wakeup(void);

void pmu_mode_change(enum PMU_POWER_MODE_T mode);

int pmu_get_security_value(union SECURITY_VALUE_T *val);

void pmu_shutdown(void);

int pmu_get_efuse(enum PMU_EFUSE_PAGE_T page, unsigned short *efuse);

void pmu_codec_config(int enable);

void pmu_anc_config(int enable);

void pmu_usb_config(enum PMU_USB_CONFIG_TYPE_T type);

void pmu_sleep_en(unsigned char sleep_en);

void pmu_flash_write_config(void);

void pmu_flash_read_config(void);

void pmu_flash_freq_config(uint32_t freq);

void pmu_psram_freq_config(uint32_t freq);

void pmu_fir_high_speed_config(int enable);

void pmu_iir_freq_config(uint32_t freq);

void pmu_sys_freq_config(enum HAL_CMU_FREQ_T freq);

void pmu_charger_init(void);

void pmu_charger_set_irq_handler(PMU_CHARGER_IRQ_HANDLER_T handler);

void pmu_charger_plugin_config(void);

void pmu_charger_plugout_config(void);

enum PMU_CHARGER_STATUS_T pmu_charger_get_status(void);

int pmu_usb_config_pin_status_check(enum PMU_USB_PIN_CHK_STATUS_T status, PMU_USB_PIN_CHK_CALLBACK callback, int enable);

void pmu_usb_enable_pin_status_check(void);

void pmu_usb_disable_pin_status_check(void);

void pmu_usb_get_pin_status(int *dp, int *dm);

void pmu_rtc_enable(void);

void pmu_rtc_disable(void);

int pmu_rtc_enabled(void);

void pmu_rtc_set(uint32_t seconds);

uint32_t pmu_rtc_get(void);

void pmu_rtc_set_alarm(uint32_t seconds);

uint32_t pmu_rtc_get_alarm(void);

void pmu_rtc_clear_alarm(void);

int pmu_rtc_alarm_status_set(void);

int pmu_rtc_alarm_alerted();

void pmu_rtc_set_irq_handler(PMU_RTC_IRQ_HANDLER_T handler);

void pmu_viorise_req(enum PMU_VIORISE_REQ_USER_T user, bool rise);

int pmu_debug_config_ana(uint16_t volt);

int pmu_debug_config_codec(uint16_t volt);

int pmu_debug_config_audio_output(bool diff);

void pmu_debug_reliability_test(int stage);

#ifdef __cplusplus
}
#endif

#endif

