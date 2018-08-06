#include "plat_types.h"
#include "norflash_drv.h"
#include "hal_norflaship.h"
#include "norflash_gd25q80c.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "task_schedule.h"

static uint32_t gd25q80c_read_status_s8_s15(void)
{
    uint8_t val;
    norflash_read_reg(GD25Q80C_CMD_READ_STATUS_S8_S15, &val, 1);
    return val;
}

static uint32_t gd25q80c_read_status_s0_s7(void)
{
    uint8_t val;
    norflash_read_reg(GD25Q80C_CMD_READ_STATUS_S0_S7, &val, 1);
    return val;
}

static void gd25q80c_status_WEL_1_wait(void)
{
    uint32_t status = 0;
    do {
        status = gd25q80c_read_status_s0_s7();
    } while (!(status&GD25Q80C_WEL_BIT_MASK) && TASK_SCHEDULE);
}

static void gd25q80c_status_WIP_1_wait(void)
{
    uint32_t status = 0;
    do {
        status = gd25q80c_read_status_s0_s7();
    } while ((status&GD25Q80C_WIP_BIT_MASK) && TASK_SCHEDULE);
}

static void gd25q80c_write_status(uint16_t status)
{
    norflash_write_reg(GD25Q80C_CMD_WRITE_STATUS, (uint8_t *)&status, 2);
}

static void gd25q80c_status_QE_set(int enable)
{
    uint8_t  status_s0_s7 = 0;
    uint16_t status_s8_s15 = 0;
    status_s0_s7 = gd25q80c_read_status_s0_s7();
    status_s8_s15 = gd25q80c_read_status_s8_s15();
    status_s8_s15 <<= 8;
    if (enable)
        status_s8_s15 |= GD25Q80C_QE_BIT_MASK;
    else
        status_s8_s15 &= ~(GD25Q80C_QE_BIT_MASK);

    gd25q80c_write_status(status_s8_s15|status_s0_s7);
}

const struct NORFLASH_CFG_T gd25q80c_cfg = {
#ifdef SIMU
    .id = { 0xC8, 0x40, 0x13, },
#else
    .id = { 0xC8, 0x40, 0x14, },
#endif
    .hpm_cap = true,
    .erase_in_std_mode = false,
    .crm_en_bits = 0xA0,
    .crm_dis_bits = 0,
    .page_size = GD25Q80C_PAGE_SIZE,
    .sector_size = GD25Q80C_SECTOR_SIZE,
    .block_size = GD25Q80C_BLOCK_SIZE,
    .total_size = GD25Q80C_TOTAL_SIZE,
    .max_speed = 0,
    .norflash_status_WEL_1_wait = gd25q80c_status_WEL_1_wait,
    .norflash_status_WIP_1_wait = gd25q80c_status_WIP_1_wait,
    .norflash_status_QE_set = gd25q80c_status_QE_set,
};

