#include "hal_cmu.h"
#include "hal_bootmode.h"
#include "hal_location.h"
#include "cmsis.h"

// BOOTMODE
#define CMU_BOOTMODE_WATCHDOG       (1 << 0)
#define CMU_BOOTMODE_GLOBAL         (1 << 1)
#define CMU_BOOTMODE_RTC            (1 << 2)
#define CMU_BOOTMODE_CHARGER        (1 << 3)
#define CMU_BOOTMODE_HW_MASK        (0xF << 0)
#define CMU_BOOTMODE_SW(n)          (((n) & 0x0FFFFFFF) << 4)
#define CMU_BOOTMODE_SW_MASK        (0x0FFFFFFF << 4)
#define CMU_BOOTMODE_SW_SHIFT       (4)

static union HAL_HW_BOOTMODE_T BOOT_BSS_LOC hw_bm;

union HAL_HW_BOOTMODE_T hal_hw_bootmode_get(void)
{
    return hw_bm;
}

union HAL_HW_BOOTMODE_T hal_rom_hw_bootmode_get(void)
{
    union HAL_HW_BOOTMODE_T bm;

    bm.reg = (*hal_cmu_get_bootmode_addr()) & CMU_BOOTMODE_HW_MASK;
    return bm;
}

void hal_hw_bootmode_init(void)
{
    hw_bm.reg = (*hal_cmu_get_bootmode_addr()) & CMU_BOOTMODE_HW_MASK;
    (*hal_cmu_get_bootmode_addr()) |= CMU_BOOTMODE_HW_MASK;
}

uint32_t hal_sw_bootmode_get(void)
{
    return (*hal_cmu_get_bootmode_addr()) & CMU_BOOTMODE_SW_MASK;
}

void hal_sw_bootmode_set(uint32_t bm)
{
    uint32_t lock;
    volatile uint32_t *addr;

    addr = hal_cmu_get_bootmode_addr();

    lock = int_lock();
    *addr = (*addr | bm) & CMU_BOOTMODE_SW_MASK;
    int_unlock(lock);
}

void hal_sw_bootmode_clear(uint32_t bm)
{
    uint32_t lock;
    volatile uint32_t *addr;

    addr = hal_cmu_get_bootmode_addr();

    lock = int_lock();
    *addr = (*addr & ~bm) & CMU_BOOTMODE_SW_MASK;
    int_unlock(lock);
}

