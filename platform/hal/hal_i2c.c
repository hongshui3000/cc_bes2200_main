#include "plat_types.h"
#include "plat_addr_map.h"
#include "string.h"
#include "hal_i2cip.h"
#include "hal_i2c.h"
#include "hal_timer.h"
#include "hal_dma.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "cmsis_nvic.h"

//#define HAL_I2C_TASK_MODE_USE_DMA 1

/* i2cip cmd_data use 16bit and read action is driven by write action */
/* when use dma to read from i2c, we need to use another dma to write cmd/stop/restart */
/* cmd/stop/restart + data use 16bit width, so dma buffer are 2 times of orgin data buffer */
/* available value : 2 and 4 , 2 means 8bit -> 16bit , 4 means 8bit -> 32bit (use low 16bit) */
#define HAL_I2C_DMA_BUFFER_WIDTH_MULTI (2)

typedef void (*HAL_I2C_IRQ_HANDLER_T)(void);

#define HAL_I2C_TX_TL I2CIP_TX_TL_QUARTER
#define HAL_I2C_RX_TL I2CIP_RX_TL_THREE_QUARTER
#define HAL_I2C_DMA_TX_TL I2CIP_DMA_TX_TL_1_BYTE
#define HAL_I2C_DMA_RX_TL (I2CIP_DMA_RX_TL_1_BYTE-1)

#define HAL_I2C_DMA_TX 1
#define HAL_I2C_DMA_RX 0
#define HAL_I2C_RESTART 1
#define HAL_I2C_NO_RESTART 0
#define HAL_I2C_YES 1
#define HAL_I2C_NO 0

/* state machine */
#define HAL_I2C_SM_TASK_NUM_MAX 2
#define HAL_I2C_SM_DMA_BUF_LEN_MAX 1024

enum HAL_I2C_SM_STATE_T {
    HAL_I2C_SM_IDLE = 0,
    HAL_I2C_SM_RUNNING,
};

enum HAL_I2C_SM_TASK_ACTION_T {
    HAL_I2C_SM_TASK_ACTION_NONE = 0,
    HAL_I2C_SM_TASK_ACTION_M_SEND,
    HAL_I2C_SM_TASK_ACTION_M_RECV,
};

enum HAL_I2C_SM_TASK_STATE_T {
    HAL_I2C_SM_TASK_STATE_START   = 0x1,
    HAL_I2C_SM_TASK_STATE_STOP    = 0x2,
    HAL_I2C_SM_TASK_STATE_TX_ABRT = 0x4,
    HAL_I2C_SM_TASK_STATE_ACT     = 0x8,
};

struct HAL_I2C_SM_TASK_T {
    /* send or recv buffer */
    uint8_t  *buf;
    uint32_t tx_len;
    uint32_t rx_len;
    uint32_t rx_idx;
    uint32_t tx_idx;

    /* device control */
    uint32_t stop;
    uint32_t restart_after_write;
    uint32_t target_addr;

    /* task control */
    uint32_t state;
    uint32_t action;

    /* system control */
    /* TODO : os and non os version */
    volatile uint32_t lock;
    uint32_t transfer_id;
    uint32_t errcode;
    HAL_I2C_TRANSFER_HANDLER_T handler;
};

struct HAL_I2C_SM_T {
    /* state machine related */
    uint32_t in_task;
    uint32_t out_task;
    uint32_t task_count;
    enum HAL_I2C_SM_STATE_T state;
    struct HAL_I2C_SM_TASK_T task[HAL_I2C_SM_TASK_NUM_MAX];

    /* dma related */
#ifdef HAL_I2C_TASK_MODE_USE_DMA
    struct HAL_DMA_CH_CFG_T tx_dma_cfg;
    struct HAL_DMA_CH_CFG_T rx_dma_cfg;
    uint8_t dma_tx_buf[HAL_I2C_SM_DMA_BUF_LEN_MAX*HAL_I2C_DMA_BUFFER_WIDTH_MULTI];
#endif

    /* device related */
    struct HAL_I2C_CONFIG_T cfg;
};
/* state machine end */

static const char * const invalid_id = "Invalid I2C ID: %d\n";
/* simple mode */
void hal_i2c0_simple_mode_irq_handler(void);
static HAL_I2C_INT_HANDLER_T hal_i2c_int_handlers[HAL_I2C_ID_NUM];
static HAL_I2C_IRQ_HANDLER_T hal_i2c_simple_mode_irq_handlers[HAL_I2C_ID_NUM] = {
    hal_i2c0_simple_mode_irq_handler,
};
/* simple mode end */

/* task mode */
void hal_i2c0_tx_dma_handler(uint8_t chan, uint32_t remain_tsize, uint32_t error, struct HAL_DMA_DESC_T *lli);
void hal_i2c0_rx_dma_handler(uint8_t chan, uint32_t remain_tsize, uint32_t error, struct HAL_DMA_DESC_T *lli);
void hal_i2c0_irq_handler(void);
static struct HAL_I2C_SM_T hal_i2c_sm[HAL_I2C_ID_NUM];
static HAL_I2C_IRQ_HANDLER_T hal_i2c_task_mode_irq_handlers[HAL_I2C_ID_NUM] = {
    hal_i2c0_irq_handler,
};
#ifdef HAL_I2C_TASK_MODE_USE_DMA
/* offset +0 tx dma handler, offset +1 rx dma handler */
static HAL_DMA_IRQ_HANDLER_T hal_i2c_dma_handler[HAL_I2C_ID_NUM*2] = {
    hal_i2c0_tx_dma_handler, hal_i2c0_rx_dma_handler,
};
#endif
/* task mode end */

static uint32_t _i2c_get_base(enum HAL_I2C_ID_T id)
{
    switch(id) {
        case HAL_I2C_ID_0:
            return I2C_BASE;
            break;
        default:
            return 0;
            break;
    }
    return 0;
}

static void _i2c_set_speed(enum HAL_I2C_ID_T id, int i2c_spd)
{
    uint32_t reg_base = _i2c_get_base(id);
    unsigned int hcnt, lcnt;

    switch (i2c_spd) {
    case IC_SPEED_MODE_MAX:
        hcnt = (IC_CLK * MIN_HS_SCL_HIGHTIME) / NANO_TO_MICRO;
        i2cip_w_high_speed_hcnt(reg_base, hcnt);
        lcnt = (IC_CLK * MIN_HS_SCL_LOWTIME) / NANO_TO_MICRO;
        i2cip_w_high_speed_lcnt(reg_base, lcnt);

        i2cip_w_speed(reg_base, I2CIP_HIGH_SPEED_MASK);
        break;

    case IC_SPEED_MODE_STANDARD:
        hcnt = (IC_CLK * MIN_SS_SCL_HIGHTIME) / NANO_TO_MICRO;
        i2cip_w_standard_speed_hcnt(reg_base, hcnt);
        lcnt = (IC_CLK * MIN_SS_SCL_LOWTIME) / NANO_TO_MICRO;
        i2cip_w_standard_speed_lcnt(reg_base, lcnt);

        i2cip_w_speed(reg_base, I2CIP_STANDARD_SPEED_MASK);
        break;

    case IC_SPEED_MODE_FAST:
    default:
        hcnt = (IC_CLK * MIN_FS_SCL_HIGHTIME) / NANO_TO_MICRO;
        i2cip_w_fast_speed_hcnt(reg_base, hcnt);
        lcnt = (IC_CLK * MIN_FS_SCL_LOWTIME) / NANO_TO_MICRO;
        i2cip_w_fast_speed_lcnt(reg_base, lcnt);

        i2cip_w_speed(reg_base, I2CIP_FAST_SPEED_MASK);
        break;
    }
}

static uint8_t _i2c_set_bus_speed(enum HAL_I2C_ID_T id,
                     unsigned int speed)
{
    uint32_t i2c_spd;

    if (speed >= I2C_MAX_SPEED)
        i2c_spd = IC_SPEED_MODE_MAX;
    else if (speed >= I2C_FAST_SPEED)
        i2c_spd = IC_SPEED_MODE_FAST;
    else
        i2c_spd = IC_SPEED_MODE_STANDARD;

    _i2c_set_speed(id, i2c_spd);

    return 0;
}

static uint32_t _i2c_check_error(enum HAL_I2C_ID_T id)
{
    uint32_t reg_base = _i2c_get_base(id);

    if (i2cip_r_tx_abrt_source(reg_base) & I2CIP_RAW_INT_STATUS_TX_ABRT_MASK)
        return i2cip_r_tx_abrt_source(reg_base);

    return 0;
}

#if 0
static uint32_t _i2c_wait_timeout(enum HAL_I2C_ID_T id)
{
    return 0;
}
#endif

/* state machine related */
static void hal_i2c_sm_init(enum HAL_I2C_ID_T id, struct HAL_I2C_CONFIG_T *cfg)
{
    hal_i2c_sm[id].in_task    = 0;
    hal_i2c_sm[id].out_task   = 0;
    hal_i2c_sm[id].task_count = 0;
    memcpy(&(hal_i2c_sm[id].cfg), cfg, sizeof(struct HAL_I2C_CONFIG_T));
}

static uint32_t hal_i2c_sm_commit(enum HAL_I2C_ID_T id, uint8_t *buf, uint32_t tx_len, uint32_t rx_len, uint32_t action,
        uint32_t target_addr, uint32_t restart_after_write, uint32_t transfer_id, HAL_I2C_TRANSFER_HANDLER_T handler)
{
    uint32_t cur = 0;

    cur = hal_i2c_sm[id].in_task;

    hal_i2c_sm[id].task[cur].buf                 = buf;
    hal_i2c_sm[id].task[cur].stop                = 1;
    hal_i2c_sm[id].task[cur].lock                = 1;
    hal_i2c_sm[id].task[cur].state               = 0;
    hal_i2c_sm[id].task[cur].rx_idx              = tx_len;
    hal_i2c_sm[id].task[cur].tx_idx              = 0;
    hal_i2c_sm[id].task[cur].rx_len              = rx_len;
    hal_i2c_sm[id].task[cur].tx_len              = tx_len;
    hal_i2c_sm[id].task[cur].errcode             = 0;
    hal_i2c_sm[id].task[cur].action              = action;
    hal_i2c_sm[id].task[cur].handler             = handler;
    hal_i2c_sm[id].task[cur].restart_after_write = restart_after_write;
    hal_i2c_sm[id].task[cur].target_addr         = target_addr;
    hal_i2c_sm[id].task[cur].transfer_id         = transfer_id;

    hal_i2c_sm[id].in_task = (hal_i2c_sm[id].in_task+1)%HAL_I2C_SM_TASK_NUM_MAX;
    ++hal_i2c_sm[id].task_count;

    return cur;
}
static void hal_i2c_sm_done_task(enum HAL_I2C_ID_T id)
{
    struct HAL_I2C_SM_TASK_T *task;
    uint32_t reg_base = _i2c_get_base(id);
#ifdef HAL_I2C_TASK_MODE_USE_DMA
    struct HAL_DMA_CH_CFG_T *tx_dma_cfg;
    struct HAL_DMA_CH_CFG_T *rx_dma_cfg;
#endif

    task = &(hal_i2c_sm[id].task[hal_i2c_sm[id].out_task]);

    if (task->errcode) {
        TRACE("i2c err : 0x%x\n", task->errcode);
#ifdef DEBUG
        if (task->errcode & HAL_I2C_ERRCODE_SLVRD_INTX)
            TRACE("i2c err : HAL_I2C_ERRCODE_SLVRD_INTX\n");
        if (task->errcode & HAL_I2C_ERRCODE_SLV_ARBLOST)
            TRACE("i2c err : HAL_I2C_ERRCODE_SLV_ARBLOST\n");
        if (task->errcode & HAL_I2C_ERRCODE_SLVFLUSH_TXFIFO)
            TRACE("i2c err : HAL_I2C_ERRCODE_SLVFLUSH_TXFIFO\n");
        if (task->errcode & HAL_I2C_ERRCODE_MASTER_DIS)
            TRACE("i2c err : HAL_I2C_ERRCODE_MASTER_DIS\n");
        if (task->errcode & HAL_I2C_ERRCODE_10B_RD_NORSTRT)
            TRACE("i2c err : HAL_I2C_ERRCODE_10B_RD_NORSTRT\n");
        if (task->errcode & HAL_I2C_ERRCODE_SBYTE_NORSTRT)
            TRACE("i2c err : HAL_I2C_ERRCODE_SBYTE_NORSTRT\n");
        if (task->errcode & HAL_I2C_ERRCODE_HS_NORSTRT)
            TRACE("i2c err : HAL_I2C_ERRCODE_HS_NORSTRT\n");
        if (task->errcode & HAL_I2C_ERRCODE_SBYTE_ACKDET)
            TRACE("i2c err : HAL_I2C_ERRCODE_SBYTE_ACKDET\n");
        if (task->errcode & HAL_I2C_ERRCODE_HS_ACKDET)
            TRACE("i2c err : HAL_I2C_ERRCODE_HS_ACKDET\n");
        if (task->errcode & HAL_I2C_ERRCODE_GCALL_READ)
            TRACE("i2c err : HAL_I2C_ERRCODE_GCALL_READ\n");
        if (task->errcode & HAL_I2C_ERRCODE_GCALL_NOACK)
            TRACE("i2c err : HAL_I2C_ERRCODE_GCALL_NOACK\n");
        if (task->errcode & HAL_I2C_ERRCODE_TXDATA_NOACK)
            TRACE("i2c err : HAL_I2C_ERRCODE_TXDATA_NOACK\n");
        if (task->errcode & HAL_I2C_ERRCODE_10ADDR2_NOACK)
            TRACE("i2c err : HAL_I2C_ERRCODE_10ADDR2_NOACK\n");
        if (task->errcode & HAL_I2C_ERRCODE_10ADDR1_NOACK)
            TRACE("i2c err : HAL_I2C_ERRCODE_10ADDR1_NOACK\n");
        if (task->errcode & HAL_I2C_ERRCODE_7B_ADDR_NOACK)
            TRACE("i2c err : HAL_I2C_ERRCODE_7B_ADDR_NOACK\n");
#endif
    }

    if (task->stop || task->errcode) {
        TRACE("disable i2c\n");
        i2cip_w_enable(reg_base, HAL_I2C_NO);
    }

#ifdef HAL_I2C_TASK_MODE_USE_DMA
    tx_dma_cfg = &hal_i2c_sm[id].tx_dma_cfg;
    rx_dma_cfg = &hal_i2c_sm[id].rx_dma_cfg;
    if (hal_i2c_sm[id].cfg.use_dma) {
        if (task->action == HAL_I2C_SM_TASK_ACTION_M_SEND) {
            TRACE("i2c tx free dma ch %d\n", tx_dma_cfg->ch);
            hal_gpdma_free_chan(tx_dma_cfg->ch);
        }
        else if (task->action == HAL_I2C_SM_TASK_ACTION_M_RECV) {
            TRACE("i2c tx free dma ch %d\n", tx_dma_cfg->ch);
            TRACE("i2c rx free dma ch %d\n", rx_dma_cfg->ch);
            hal_gpdma_free_chan(tx_dma_cfg->ch);
            hal_gpdma_free_chan(rx_dma_cfg->ch);
        }
    }
#endif

    if (hal_i2c_sm[id].cfg.use_sync) {
        /* FIXME : os and non-os - different proc */
        task->lock = 0;
    }
    else {
        if (task->handler) {
            task->handler(id, task->transfer_id, task->buf, task->tx_len + task->rx_len, task->errcode);
        }
    }

    hal_i2c_sm[id].out_task = (hal_i2c_sm[id].out_task+1)%HAL_I2C_SM_TASK_NUM_MAX;
    --hal_i2c_sm[id].task_count;
}
static void hal_i2c_sm_done(enum HAL_I2C_ID_T id)
{
    TRACE("sm done\n");
    hal_i2c_sm[id].state      = HAL_I2C_SM_IDLE;
    hal_i2c_sm[id].in_task    = 0;
    hal_i2c_sm[id].out_task   = 0;
    hal_i2c_sm[id].task_count = 0;
}

static void hal_i2c_sm_next_task(enum HAL_I2C_ID_T id)
{
    uint32_t out_task_index;
    uint32_t reg_base = 0, reinit = 0;
    enum HAL_I2C_SM_TASK_ACTION_T action;
    struct HAL_I2C_SM_TASK_T *out_task = 0;
#ifdef HAL_I2C_TASK_MODE_USE_DMA
    uint32_t i = 0;
    struct HAL_DMA_CH_CFG_T *tx_dma_cfg = 0, *rx_dma_cfg = 0;
#endif

    reg_base = _i2c_get_base(id);

    if (hal_i2c_sm[id].task_count <= 0) {
        hal_i2c_sm_done(id);
        return;
    }

    out_task_index = hal_i2c_sm[id].out_task;
    out_task = &(hal_i2c_sm[id].task[out_task_index]);
    action = out_task->action;

    if (action == HAL_I2C_SM_TASK_ACTION_M_SEND) {
        /* tx : quarter trigger TX EMPTY INT */
        i2cip_w_tx_threshold(reg_base, HAL_I2C_TX_TL);
    }
    else if (action == HAL_I2C_SM_TASK_ACTION_M_RECV) {
        /* tx : quarter trigger TX EMPTY INT */
        i2cip_w_tx_threshold(reg_base, HAL_I2C_TX_TL);
        /* rx : three quarter trigger RX FULL INT */
        i2cip_w_rx_threshold(reg_base, HAL_I2C_RX_TL-1);
    }

    if (!(hal_i2c_sm[id].cfg.use_dma)) {
        /* open all interrupt */
        i2cip_w_int_mask(reg_base, I2CIP_INT_MASK_ALL);
    }
    else {
        i2cip_w_int_mask(reg_base, I2CIP_INT_MASK_STOP_DET_MASK|I2CIP_INT_MASK_TX_ABRT_MASK);

#ifdef HAL_I2C_TASK_MODE_USE_DMA
        /* prepare for dma operation */
        tx_dma_cfg = &hal_i2c_sm[id].tx_dma_cfg;
        rx_dma_cfg = &hal_i2c_sm[id].rx_dma_cfg;
        memset(tx_dma_cfg, 0, sizeof(*tx_dma_cfg));
        memset(rx_dma_cfg, 0, sizeof(*rx_dma_cfg));
        memset(&(hal_i2c_sm[id].dma_tx_buf[0]), 0, sizeof(&(hal_i2c_sm[id].dma_tx_buf)));

        if (action == HAL_I2C_SM_TASK_ACTION_M_SEND) {
            i2cip_w_tx_dma_enable(reg_base, HAL_I2C_YES);
            i2cip_w_tx_dma_tl(reg_base, HAL_I2C_DMA_TX_TL);

            tx_dma_cfg->dst = 0; // useless
            tx_dma_cfg->dst_bsize = HAL_DMA_BSIZE_1;
            tx_dma_cfg->dst_periph = HAL_GPDMA_I2C_TX;
            tx_dma_cfg->dst_width = HAL_DMA_WIDTH_HALFWORD;
            tx_dma_cfg->handler = hal_i2c_dma_handler[id+0];
            tx_dma_cfg->src_bsize = HAL_DMA_BSIZE_1;
            tx_dma_cfg->src_periph = 0; // useless
            tx_dma_cfg->src_tsize = out_task->tx_len;
            tx_dma_cfg->src_width = HAL_DMA_WIDTH_HALFWORD;
            tx_dma_cfg->try_burst = 1;
            tx_dma_cfg->type = HAL_DMA_FLOW_M2P_DMA;
            tx_dma_cfg->src = (uint32_t)hal_i2c_sm[id].dma_tx_buf;
            tx_dma_cfg->ch = hal_gpdma_get_chan(tx_dma_cfg->dst_periph, HAL_DMA_HIGH_PRIO);
            TRACE("i2c tx get dma ch %d\n", tx_dma_cfg->ch);

            TRACE("transfer size %d, HAL_I2C_DMA_BUFFER_WIDTH_MULTI %d\n", out_task->tx_len, HAL_I2C_DMA_BUFFER_WIDTH_MULTI);

            for (i = 0; i < out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI; i += HAL_I2C_DMA_BUFFER_WIDTH_MULTI) {
                // lo byte of short : for data
                // hi byte of short : for cmd/stop/restart
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2) {
                    *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = out_task->buf[i/HAL_I2C_DMA_BUFFER_WIDTH_MULTI];
                }
                else {
                    *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = out_task->buf[i/HAL_I2C_DMA_BUFFER_WIDTH_MULTI];
                }
            }
            if (out_task->restart_after_write) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[0]))) |= I2CIP_CMD_DATA_RESTART_MASK;
                else
                    *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[0]))) |= I2CIP_CMD_DATA_RESTART_MASK;
            }
            if (out_task->stop) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI-HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_STOP_MASK;
                else
                    *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI-HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_STOP_MASK;
            }

#ifdef DEBUG
            for (i = 0; i < out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI; i += HAL_I2C_DMA_BUFFER_WIDTH_MULTI) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    TRACE("buf %d - 0x%x\n", i, *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))));
                else
                    TRACE("32bit buf %d - 0x%x\n", i, *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))));
            }
#endif
        }
        else if (action == HAL_I2C_SM_TASK_ACTION_M_RECV) {
            i2cip_w_tx_dma_enable(reg_base, HAL_I2C_YES);
            i2cip_w_rx_dma_enable(reg_base, HAL_I2C_YES);
            i2cip_w_tx_dma_tl(reg_base, HAL_I2C_DMA_TX_TL);
            i2cip_w_rx_dma_tl(reg_base, HAL_I2C_DMA_RX_TL);

            tx_dma_cfg->src = (uint32_t)hal_i2c_sm[id].dma_tx_buf;
            tx_dma_cfg->dst = 0; // useless
            tx_dma_cfg->dst_bsize = HAL_DMA_BSIZE_1;
            tx_dma_cfg->dst_periph = HAL_GPDMA_I2C_TX;
            tx_dma_cfg->dst_width = HAL_DMA_WIDTH_HALFWORD;
            tx_dma_cfg->handler = hal_i2c_dma_handler[id+0];
            tx_dma_cfg->src_bsize = HAL_DMA_BSIZE_1;
            tx_dma_cfg->src_periph = 0; // useless
            tx_dma_cfg->src_tsize = out_task->tx_len + out_task->rx_len;
            tx_dma_cfg->src_width = HAL_DMA_WIDTH_HALFWORD;
            tx_dma_cfg->try_burst = 1;
            tx_dma_cfg->type = HAL_DMA_FLOW_M2P_DMA;
            tx_dma_cfg->ch = hal_gpdma_get_chan(tx_dma_cfg->dst_periph, HAL_DMA_HIGH_PRIO);
            TRACE("i2c tx get dma ch %d\n", tx_dma_cfg->ch);

            for (i = 0; i < (out_task->tx_len+out_task->rx_len)*HAL_I2C_DMA_BUFFER_WIDTH_MULTI; i += HAL_I2C_DMA_BUFFER_WIDTH_MULTI) {
                // lo byte of short : for data
                // hi byte of short : for cmd/stop/restart
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2) {
                    if (i < out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI)
                        *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = out_task->buf[i/HAL_I2C_DMA_BUFFER_WIDTH_MULTI];
                    else
                        *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = I2CIP_CMD_DATA_CMD_READ_MASK;
                }
                else {
                    if (i < out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI)
                        *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = out_task->buf[i/HAL_I2C_DMA_BUFFER_WIDTH_MULTI];
                    else
                        *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))) = I2CIP_CMD_DATA_CMD_READ_MASK;
                }
            }
            if (out_task->restart_after_write) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_RESTART_MASK;
                else
                    *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[out_task->tx_len*HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_RESTART_MASK;
            }
            if (out_task->stop) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[(out_task->tx_len+out_task->rx_len)*HAL_I2C_DMA_BUFFER_WIDTH_MULTI-HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_STOP_MASK;
                else
                    *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[(out_task->tx_len+out_task->rx_len)*HAL_I2C_DMA_BUFFER_WIDTH_MULTI-HAL_I2C_DMA_BUFFER_WIDTH_MULTI]))) |= I2CIP_CMD_DATA_STOP_MASK;
            }
#ifdef DEBUG
            for (i = 0; i < (out_task->tx_len+out_task->rx_len)*HAL_I2C_DMA_BUFFER_WIDTH_MULTI; i += HAL_I2C_DMA_BUFFER_WIDTH_MULTI) {
                if (HAL_I2C_DMA_BUFFER_WIDTH_MULTI == 2)
                    TRACE("buf %d - 0x%x\n", i, *((uint16_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))));
                else
                    TRACE("32bit buf %d - 0x%x\n", i, *((uint32_t *)(&(hal_i2c_sm[id].dma_tx_buf[i]))));
            }
#endif

            TRACE("transfer size %d, HAL_I2C_DMA_BUFFER_WIDTH_MULTI %d\n", out_task->tx_len + out_task->rx_len, HAL_I2C_DMA_BUFFER_WIDTH_MULTI);
            rx_dma_cfg->dst = (uint32_t)(out_task->buf + out_task->tx_len);
            rx_dma_cfg->dst_bsize = HAL_DMA_BSIZE_1;
            rx_dma_cfg->dst_periph = 0; // useless
            rx_dma_cfg->dst_width = HAL_DMA_WIDTH_BYTE;
            rx_dma_cfg->handler = hal_i2c_dma_handler[id+1];
            rx_dma_cfg->src_bsize = HAL_DMA_BSIZE_1;
            rx_dma_cfg->src_periph = HAL_GPDMA_I2C_RX;
            rx_dma_cfg->src_tsize = out_task->rx_len;
            rx_dma_cfg->src_width = HAL_DMA_WIDTH_BYTE;
            rx_dma_cfg->try_burst = 1;
            rx_dma_cfg->src = 0; // useless
            rx_dma_cfg->type = HAL_DMA_FLOW_P2M_DMA;
            rx_dma_cfg->ch = hal_gpdma_get_chan(rx_dma_cfg->src_periph, HAL_DMA_HIGH_PRIO);
            TRACE("i2c rx get dma ch %d\n", rx_dma_cfg->ch);
        }
#endif
    }

    reinit = i2cip_r_enable_status(reg_base);
    /* not enable : reconfig i2cip with new-task params */
    /* enable : same operation with pre task */
    if (!(reinit&I2CIP_ENABLE_STATUS_ENABLE_MASK)) {
        TRACE("enable i2c\n");
        i2cip_w_restart(reg_base, HAL_I2C_YES);
        i2cip_w_target_address(reg_base, out_task->target_addr);

        i2cip_w_enable(reg_base, HAL_I2C_YES);
    }

#ifdef HAL_I2C_TASK_MODE_USE_DMA
    if (hal_i2c_sm[id].cfg.use_dma) {
        if (action == HAL_I2C_SM_TASK_ACTION_M_SEND) {
            TRACE("enable tx dma\n");
            hal_gpdma_start(tx_dma_cfg);
        }
        else if (action == HAL_I2C_SM_TASK_ACTION_M_RECV) {
            TRACE("enable tx rx dma\n");
            hal_gpdma_start(rx_dma_cfg);
            hal_gpdma_start(tx_dma_cfg);
        }
    }
#endif
}
static uint32_t hal_i2c_sm_wait_task_if_need(enum HAL_I2C_ID_T id, uint32_t task_idx)
{
    struct HAL_I2C_SM_TASK_T *task = 0;

    /* FIXME : task_id maybe invalid cause so-fast device operation */
    task = &(hal_i2c_sm[id].task[task_idx]);

    if (!(hal_i2c_sm[id].cfg.use_sync))
        return 0;

    /* FIXME : os and non-os - different proc */
    while(task->lock);

    TRACE("wait %d done\n", task_idx);

    return task->errcode;
}
static void hal_i2c_sm_kickoff(enum HAL_I2C_ID_T id)
{
    if (hal_i2c_sm[id].state == HAL_I2C_SM_IDLE) {
        hal_i2c_sm[id].state = HAL_I2C_SM_RUNNING;
        hal_i2c_sm_next_task(id);
    }
}
static void hal_i2c_sm_proc(enum HAL_I2C_ID_T id)
{
    uint32_t reg_base = 0;
    struct HAL_I2C_SM_TASK_T *task;
    enum HAL_I2C_SM_STATE_T state = 0;
    uint32_t i = 0, restart = 0, stop = 0, data = 0;
    uint32_t ip_int_status = 0, tx_abrt_source = 0, tmp1 = 0;
    uint32_t rx_limit = 0, tx_limit = 0, depth = 0;

    reg_base = _i2c_get_base(id);
    state = hal_i2c_sm[id].state;
    task = &(hal_i2c_sm[id].task[hal_i2c_sm[id].out_task]);

    ip_int_status = i2cip_r_int_status(reg_base);
    tx_abrt_source = i2cip_r_tx_abrt_source(reg_base);

 //   TRACE("int status 0x%x, tx_abrt_source 0x%x\n", ip_int_status, tx_abrt_source);

    switch (state) {
        case HAL_I2C_SM_IDLE:
            break;
        case HAL_I2C_SM_RUNNING:
            {
        //        TRACE("int status 0x%x, tx_abrt_source 0x%x\n", ip_int_status, tx_abrt_source);
                if (ip_int_status & I2CIP_INT_STATUS_TX_ABRT_MASK) {
                    task->state |= HAL_I2C_SM_TASK_STATE_TX_ABRT;

                    /* sbyte_norstrt is special to clear : restart disable but user want to send a restart */
                    /* to fix this bit : enable restart , clear speical , clear gc_or_start bit temporary */
                    tmp1 = i2cip_r_target_address_reg(reg_base);
                    if (tx_abrt_source & I2CIP_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_MASK) {
                        i2cip_w_restart(reg_base, HAL_I2C_YES);
                        i2cip_w_special_bit(reg_base, HAL_I2C_NO);
                        i2cip_w_gc_or_start_bit(reg_base, HAL_I2C_NO);
                    }
                    i2cip_r_clr_tx_abrt(reg_base);
                    /* restore register after clear */
                    if (tx_abrt_source & I2CIP_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_MASK) {
                        i2cip_w_target_address_reg(reg_base, tmp1);
                    }

                    task->errcode = tx_abrt_source;

                    /* done task on any error */
                    hal_i2c_sm_done_task(id);
                    hal_i2c_sm_next_task(id);

                    return;
                }
                if (ip_int_status & I2CIP_INT_STATUS_STOP_DET_MASK) {
                    task->state |= HAL_I2C_SM_TASK_STATE_STOP;
                    i2cip_r_clr_stop_det(reg_base);
                }
                if (ip_int_status & I2CIP_INT_STATUS_START_DET_MASK) {
                    task->state |= HAL_I2C_SM_TASK_STATE_START;
                    i2cip_r_clr_start_det(reg_base);
                }
                if (ip_int_status & I2CIP_INT_STATUS_ACTIVITY_MASK) {
                    task->state |= HAL_I2C_SM_TASK_STATE_ACT;
                    i2cip_r_clr_activity(reg_base);
                }

                switch (task->action) {
                    case HAL_I2C_SM_TASK_ACTION_M_SEND:
                        {
                            /* tx empty : means tx fifo is at or below IC_TX_TL : need to write more data : we can NOT clear this bit, cleared by hw */
                            if (ip_int_status & I2CIP_INT_STATUS_TX_EMPTY_MASK) {
                           //     TRACE("m_send : tx empty\n");
                                for (i = task->tx_idx, tx_limit = 0; (i < task->tx_len) && (tx_limit < (I2CIP_TX_TL_DEPTH-HAL_I2C_TX_TL)); ++i, ++tx_limit) {
                                    /* last byte : we need to decide stop */
                                    if (i == task->tx_len - 1)
                                        stop = task->stop?I2CIP_CMD_DATA_STOP_MASK:0;
                                    /* first byte : need to decide restart */
                                    if (i == 0)
                                        restart = task->restart_after_write?I2CIP_CMD_DATA_RESTART_MASK:0;
                                    i2cip_w_cmd_data(reg_base, task->buf[i] | I2CIP_CMD_DATA_CMD_WRITE_MASK | restart | stop);
                                }
                                task->tx_idx = i;
                                /* all write action done : do NOT need tx empty int */
                                if (task->tx_idx == task->tx_len) {
                                    i2cip_w_int_mask(reg_base, ~I2CIP_INT_MASK_TX_EMPTY_MASK);
                                }
                            }

                            /* stop condition : done task */
                            if (task->state & HAL_I2C_SM_TASK_STATE_STOP) {
                                hal_i2c_sm_done_task(id);
                                hal_i2c_sm_next_task(id);
                            }
                        }
                        break;
                    case HAL_I2C_SM_TASK_ACTION_M_RECV:
                        {
                            /* rx full : need to read */
                            if (ip_int_status & I2CIP_INT_STATUS_RX_FULL_MASK) {
                                TRACE("m_recv : rx full\n");
                                depth = i2cip_r_rx_fifo_level(reg_base);
                                for (i = task->rx_idx, tx_limit = 0; (rx_limit < depth) && (i < (task->rx_len + task->rx_idx)); ++i, ++rx_limit) {
                                    task->buf[i] = i2cip_r_cmd_data(reg_base);
                                }
                                task->rx_idx = i;
                            }
                            /* tx empty : means tx fifo is at or below IC_TX_TL : need to write more data : we can NOT clear this bit, cleared by hw */
                            if (ip_int_status & I2CIP_INT_STATUS_TX_EMPTY_MASK) {
                                TRACE("m_recv : tx empty\n");
                                depth = i2cip_r_rx_fifo_level(reg_base);
                                for (i = task->tx_idx, tx_limit = 0, rx_limit = 0;
                                        (i < (task->rx_len + task->tx_len)) && (tx_limit < (I2CIP_TX_TL_DEPTH-HAL_I2C_TX_TL)) && (rx_limit < (I2CIP_RX_TL_DEPTH-depth));
                                        ++i, ++tx_limit, ++rx_limit) {
                                    /* last byte : we need to decide stop */
                                    if (i == (task->rx_len + task->tx_len - 1))
                                        stop = task->stop?I2CIP_CMD_DATA_STOP_MASK:0;
                                    /* first byte : need to decide restart */
                                    if (i == task->tx_len)
                                        restart = task->restart_after_write?I2CIP_CMD_DATA_RESTART_MASK:0;
                                    else
                                        restart = 0;
                                    /* real write data */
                                    if (i < task->tx_len) {
                                        data = task->buf[i];
                                    }
                                    /* no data */
                                    else {
                                        data = 0;
                                        data |= I2CIP_CMD_DATA_CMD_READ_MASK;
                                    }

                                    i2cip_w_cmd_data(reg_base, data | restart | stop);
                                }
                                task->tx_idx = i;
                                /* all write action done */
                                if (task->tx_idx == (task->rx_len + task->tx_len) ) {
                                    i2cip_w_int_mask(reg_base, ~I2CIP_INT_MASK_TX_EMPTY_MASK);
                                }
                            }
                            /* stop condition : need to read out all rx fifo */
                            if (task->state & HAL_I2C_SM_TASK_STATE_STOP) {
                                depth = i2cip_r_rx_fifo_level(reg_base);
                                for (i = task->rx_idx, rx_limit = 0; (rx_limit < depth) && (i < (task->rx_idx + task->rx_len)); ++i, ++rx_limit) {
                                    task->buf[i] = i2cip_r_cmd_data(reg_base);
                                    TRACE("task->buf[i] 0x%x\n", task->buf[i]);
                                }
                                TRACE("m_recv : rx stop depth %d\n", depth);
                                task->rx_idx = i;
                                hal_i2c_sm_done_task(id);
                                hal_i2c_sm_next_task(id);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
    }
}
#ifdef HAL_I2C_TASK_MODE_USE_DMA
#if 0
static void hal_i2c_dma_sm_proc(enum HAL_I2C_ID_T id, uint32_t tx, uint32_t remain_tsize, uint32_t error)
{
    uint32_t reg_base = 0;
    struct HAL_I2C_SM_TASK_T *task;
    enum HAL_I2C_SM_STATE_T state = 0;
    uint32_t ip_raw_int_status = 0, tx_abrt_source = 0, tmp1 = 0;

    reg_base = _i2c_get_base(id);
    state = hal_i2c_sm[id].state;
    task = &(hal_i2c_sm[id].task[hal_i2c_sm[id].out_task]);

    ip_raw_int_status = i2cip_r_raw_int_status(reg_base);
    tx_abrt_source = i2cip_r_tx_abrt_source(reg_base);

    switch (state) {
        case HAL_I2C_SM_IDLE:
            break;
        case HAL_I2C_SM_RUNNING:
            {
                /* TODO : maye need to see 'error' param from dma callback */
                if (ip_raw_int_status & I2CIP_INT_STATUS_TX_ABRT_MASK) {
                    task->state |= HAL_I2C_SM_TASK_STATE_TX_ABRT;

                    /* sbyte_norstrt is special to clear : restart disable but user want to send a restart */
                    /* to fix this bit : enable restart , clear speical , clear gc_or_start bit temporary */
                    tmp1 = i2cip_r_target_address_reg(reg_base);
                    if (tx_abrt_source & I2CIP_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_MASK) {
                        i2cip_w_restart(reg_base, HAL_I2C_YES);
                        i2cip_w_special_bit(reg_base, HAL_I2C_NO);
                        i2cip_w_gc_or_start_bit(reg_base, HAL_I2C_NO);
                    }
                    i2cip_r_clr_tx_abrt(reg_base);
                    /* restore register after clear */
                    if (tx_abrt_source & I2CIP_TX_ABRT_SOURCE_ABRT_SBYTE_NORSTRT_MASK) {
                        i2cip_w_target_address_reg(reg_base, tmp1);
                    }

                    task->errcode = tx_abrt_source;
                }
                TRACE("i2c dma ip_raw_int_status 0x%x, tx_abrt_source 0x%x\n", ip_raw_int_status, tx_abrt_source);
                ip_raw_int_status = i2cip_r_raw_int_status(reg_base);
                tx_abrt_source = i2cip_r_tx_abrt_source(reg_base);
                switch (task->action) {
                    case HAL_I2C_SM_TASK_ACTION_M_SEND:
                        {
                            TRACE("i2c dma m_send done\n");
                            hal_i2c_sm_done_task(id);
                            hal_i2c_sm_next_task(id);
                        }
                        break;
                    case HAL_I2C_SM_TASK_ACTION_M_RECV:
                        {
                            TRACE("i2c dma m_recv tx %d, dma err %d, task->errcode %d\n", tx, error, task->errcode);
                            /* rx dma done or erro issue */
                            if (tx == HAL_I2C_DMA_RX || task->errcode) {
                                hal_i2c_sm_done_task(id);
                                hal_i2c_sm_next_task(id);
                            }
                        }
                        break;
                }
            }
            break;
    }
}
#endif
#endif
/* state machine related end */
#ifdef HAL_I2C_TASK_MODE_USE_DMA
void hal_i2c0_tx_dma_handler(uint8_t chan, uint32_t remain_tsize, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    /*
    hal_i2c_dma_sm_proc(HAL_I2C_ID_0, HAL_I2C_DMA_TX, remain_tsize, error);
    hal_i2c_sm_proc(HAL_I2C_ID_0);
    */
}
void hal_i2c0_rx_dma_handler(uint8_t chan, uint32_t remain_tsize, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    /*
    hal_i2c_dma_sm_proc(HAL_I2C_ID_0, HAL_I2C_DMA_RX, remain_tsize, error);
    hal_i2c_sm_proc(HAL_I2C_ID_0);
    */
}
#endif
void hal_i2c0_irq_handler(void)
{
    hal_i2c_sm_proc(HAL_I2C_ID_0);
}

uint32_t hal_i2c_open(enum HAL_I2C_ID_T id, struct HAL_I2C_CONFIG_T *cfg)
{
    uint32_t reg_base = 0;
    ASSERT(id < HAL_I2C_ID_NUM, invalid_id, id);

    hal_cmu_clock_enable(HAL_CMU_MOD_O_I2C);
    hal_cmu_clock_enable(HAL_CMU_MOD_P_I2C);
    hal_cmu_reset_clear(HAL_CMU_MOD_O_I2C);
    hal_cmu_reset_clear(HAL_CMU_MOD_P_I2C);

    reg_base = _i2c_get_base(id);

    i2cip_w_enable(reg_base, HAL_I2C_NO);

    i2cip_w_target_address(reg_base, 0x18);

    /* clear */
    i2cip_w_clear_ctrl(reg_base);

    /* as master */
    if (cfg->as_master) {
        i2cip_w_disable_slave(reg_base, HAL_I2C_YES);
        i2cip_w_master_mode(reg_base, HAL_I2C_YES);
    }
    else {
        i2cip_w_disable_slave(reg_base, HAL_I2C_NO);
        i2cip_w_master_mode(reg_base, HAL_I2C_NO);

        /* address as slave */
        i2cip_w_address_as_slave(reg_base, cfg->addr_as_slave);
    }

    /* speed */
    _i2c_set_bus_speed(id, cfg->speed);

    /* only use task irq handler when master mode */
    if (cfg->mode == HAL_I2C_API_MODE_TASK) {

        if (!cfg->as_master) {
            TRACE("task mode but slave\n");
            return 1;
        }

#ifndef HAL_I2C_TASK_MODE_USE_DMA
        cfg->use_dma = 0;
#endif
        NVIC_SetVector(I2C_IRQn, (uint32_t)hal_i2c_task_mode_irq_handlers[id]);
        hal_i2c_sm_init(id, cfg);
    }
    else if (cfg->mode ==HAL_I2C_API_MODE_SIMPLE) {
        TRACE("simple mode\n");
        NVIC_SetVector(I2C_IRQn, (uint32_t)hal_i2c_simple_mode_irq_handlers[id]);

        /* only open slave related int, polling in master */
        /*                         read req (master read us), rx full (master write us) */
        if (!cfg->as_master)
            i2cip_w_int_mask(reg_base, I2CIP_INT_MASK_RD_REQ_MASK|I2CIP_INT_MASK_RX_FULL_MASK);
        else
            i2cip_w_int_mask(reg_base, I2CIP_INT_UNMASK_ALL);

        /* rx tl half trigger rx full */
        i2cip_w_rx_threshold(reg_base, I2CIP_RX_TL_1_BYTE-1);

        /* enable restart */
        i2cip_w_restart(reg_base, HAL_I2C_YES);

        /* enable i2c */
        i2cip_w_enable(reg_base, HAL_I2C_YES);
    }

    NVIC_SetPriority(I2C_IRQn, IRQ_PRIORITY_NORMAL);
    NVIC_ClearPendingIRQ(I2C_IRQn);
    NVIC_EnableIRQ(I2C_IRQn);

    return 0;
}
uint32_t hal_i2c_close(enum HAL_I2C_ID_T id)
{
    uint32_t reg_base = 0;
    ASSERT(id < HAL_I2C_ID_NUM, invalid_id, id);

    NVIC_DisableIRQ(I2C_IRQn);

    reg_base = _i2c_get_base(id);

    i2cip_w_enable(reg_base, HAL_I2C_NO);

    hal_cmu_reset_set(HAL_CMU_MOD_P_I2C);
    hal_cmu_reset_set(HAL_CMU_MOD_O_I2C);
    hal_cmu_clock_disable(HAL_CMU_MOD_P_I2C);
    hal_cmu_clock_disable(HAL_CMU_MOD_O_I2C);

    return 0;
}
/* task mode */
uint32_t hal_i2c_send(enum HAL_I2C_ID_T id, uint32_t device_addr, uint8_t *buf, uint32_t reg_len, uint32_t value_len,
        uint32_t transfer_id, HAL_I2C_TRANSFER_HANDLER_T handler)
{
    uint32_t task_idx = 0, ret = 0;
    ASSERT(id < HAL_I2C_ID_NUM, invalid_id, id);
    if (hal_i2c_sm[id].cfg.as_master) {
        /* id, buffer, tx len, rx len, action, device address, restart_after_write, handler */
        task_idx = hal_i2c_sm_commit(id, buf, reg_len + value_len, 0, HAL_I2C_SM_TASK_ACTION_M_SEND, device_addr,
                HAL_I2C_NO_RESTART, transfer_id, handler);

        hal_i2c_sm_kickoff(id);

        ret = hal_i2c_sm_wait_task_if_need(id, task_idx);
    }
    return ret;
}
uint32_t hal_i2c_recv(enum HAL_I2C_ID_T id, uint32_t device_addr, uint8_t *buf, uint32_t reg_len, uint32_t value_len,
        uint8_t restart_after_write, uint32_t transfer_id, HAL_I2C_TRANSFER_HANDLER_T handler)
{
    uint32_t task_idx = 0, ret = 0;
    ASSERT(id < HAL_I2C_ID_NUM, invalid_id, id);

    if (hal_i2c_sm[id].cfg.as_master) {
        /* id, buffer, tx len, rx len, action, device address, restart_after_write, handler */
        task_idx = hal_i2c_sm_commit(id, buf, reg_len, value_len, HAL_I2C_SM_TASK_ACTION_M_RECV, device_addr,
                restart_after_write?HAL_I2C_RESTART:HAL_I2C_NO_RESTART, transfer_id, handler);

        hal_i2c_sm_kickoff(id);

        ret = hal_i2c_sm_wait_task_if_need(id, task_idx);
    }
    return ret;
}
/* task mode end */

/* simple mode */
void hal_i2c0_simple_mode_irq_handler(void)
{
    uint32_t reg_base = 0, ip_raw_int_status = 0, tx_abrt_source = 0;

    reg_base = _i2c_get_base(HAL_I2C_ID_0);
    ip_raw_int_status = i2cip_r_raw_int_status(reg_base);
    tx_abrt_source    = i2cip_r_tx_abrt_source(reg_base);

    i2cip_r_clr_all_intr(reg_base);

    hal_i2c_int_handlers[HAL_I2C_ID_0](ip_raw_int_status, tx_abrt_source);

    i2cip_r_clr_all_intr(reg_base);

    TRACE("irq:i2c status 0x%x, raw int statux0x%x\n", i2cip_r_status(reg_base), i2cip_r_raw_int_status(reg_base));
}
uint32_t hal_i2c_slv_write(enum HAL_I2C_ID_T id, uint8_t *buf, uint32_t len, uint32_t *act_write)
{
    uint32_t reg_base = 0, i = 0, ret = 0;

    reg_base = _i2c_get_base(id);

    *act_write = 0;

    for (i = 0; i < len;) {
        /* Tx Fifo Not Full */
        if (i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK) {
            i2cip_w_cmd_data(reg_base, buf[i]);
            ++(*act_write);
            ++i;
        }
        else {
            /* wait for TFNF & not rx done & no error */
            while(!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK)
                    && !(i2cip_r_raw_int_status(reg_base) & I2CIP_RAW_INT_STATUS_RX_DONE_MASK)
                        && !(ret = _i2c_check_error(reg_base)));

            if (ret || (i2cip_r_raw_int_status(reg_base) & I2CIP_RAW_INT_STATUS_RX_DONE_MASK))
                break;
        }
    }

    /* FIXME : tx empty is not end of transmit, i2cip may transmit the last byte */
    while(!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFE_SHIFT)
            && !(ret = _i2c_check_error(reg_base)));

    /* wait to idle */
    /* FIXME : time out */
    while(i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK);

    return ret;
}
uint32_t hal_i2c_slv_read(enum HAL_I2C_ID_T id, uint8_t *buf, uint32_t len, uint32_t *act_read)
{
    uint32_t reg_base = 0, i = 0, ret = 0, depth = 0;

    reg_base = _i2c_get_base(id);

    *act_read = 0;
    /* slave mode : just read */
    for (i = 0; i < len;) {
        /* Rx Fifo Not Empty */
        if (i2cip_r_status(reg_base) & I2CIP_STATUS_RFNE_MASK) {
            buf[i] = i2cip_r_cmd_data(reg_base);
            ++(*act_read);
            ++i;
        }
        else {
            /* wait for RFNE & no stop & no error */
            while(!(i2cip_r_status(reg_base) & I2CIP_STATUS_RFNE_MASK)
                    && !(i2cip_r_raw_int_status(reg_base) & I2CIP_RAW_INT_STATUS_STOP_DET_MASK)
                    && !(ret = _i2c_check_error(reg_base))) {
            }

            if (ret || (i2cip_r_raw_int_status(reg_base) & I2CIP_RAW_INT_STATUS_STOP_DET_MASK)) {
                TRACE("drv:i2c slave read ret 0x%x, raw status 0x%x\n", ret, i2cip_r_raw_int_status(reg_base));
                break;
            }
        }
    }

    /* may left some bytes in rx fifo */
    depth = i2cip_r_rx_fifo_level(reg_base);
    TRACE("drv:i2c slave read depth %d\n", depth);
    while (depth > 0 && i < len) {
        buf[i] = i2cip_r_cmd_data(reg_base);
        ++(*act_read);
        ++i;
        --depth;
    }

    /* wait to idle */
    while(i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK);

    return 0;
}
uint32_t hal_i2c_mst_write(enum HAL_I2C_ID_T id, uint32_t device_addr, uint8_t *buf, uint32_t len, uint32_t *act_write, uint32_t restart, uint32_t stop)
{
    uint32_t reg_base = 0, restart_cmd = 0, stop_cmd = 0, i = 0, ret = 0;

    reg_base = _i2c_get_base(id);

    i2cip_w_target_address(reg_base, device_addr);

    *act_write = 0;

    for (i = 0; i < len;) {
        /* Tx Fifo Not Full */
        if (i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK) {
            if (i == 0)
                restart_cmd = restart?I2CIP_CMD_DATA_RESTART_MASK:0;
            if (i == len - 1)
                stop_cmd = stop?I2CIP_CMD_DATA_STOP_MASK:0;
            i2cip_w_cmd_data(reg_base, buf[i] | restart_cmd | stop_cmd);

            ++(*act_write);
            ++i;
        }
        else {
            /* wait for TFNF & no error */
            while (!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK)
                    && !(ret = _i2c_check_error(reg_base)));

            if (ret)
                break;
        }
    }

    if (stop) {
        /* wait to idle */
        while (i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK);
    }
    else {
        /* FIXME : tx fifo empty is not transmit end, i2cip may transmit last byte */
        /* we may can wait a most long time out (lowest speed) */
    }

    i2cip_r_clr_all_intr(reg_base);

    return ret;
}
uint32_t hal_i2c_mst_read(enum HAL_I2C_ID_T id, uint32_t device_addr, uint8_t *buf, uint32_t len, uint32_t *act_read, uint32_t restart, uint32_t stop)
{
    uint32_t reg_base = 0, i = 0, j = 0, depth = 0, restart_cmd = 0, stop_cmd = 0, ret = 0;
    reg_base = _i2c_get_base(id);

    i2cip_w_target_address(reg_base, device_addr);

    for (i = 0; i < len; ++i) {
        if (i2cip_r_status(reg_base) & I2CIP_STATUS_TFE_SHIFT) {
            if (i == 0)
                restart_cmd = restart?I2CIP_CMD_DATA_RESTART_MASK:0;
            else
                restart_cmd = 0;
            if (i == len - 1)
                stop_cmd = stop?I2CIP_CMD_DATA_STOP_MASK:0;

            i2cip_w_cmd_data(reg_base, I2CIP_CMD_DATA_CMD_READ_MASK|restart_cmd|stop_cmd);

            while (!(i2cip_r_status(reg_base) & I2CIP_STATUS_RFNE_MASK)
                    && !(ret = _i2c_check_error(reg_base)));

            if (ret)
                break;

            ++j;
            ++(*act_read);
            buf[j] = i2cip_r_cmd_data(reg_base);
        }
        else {
            /* wait for TFNF & no error */
            while(!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK)
                    && !(ret = _i2c_check_error(reg_base)));

            if (ret)
                break;
        }
    }

    /* may left some bytes in rx fifo */
    depth = i2cip_r_rx_fifo_level(reg_base);
    TRACE("drv:i2c mst read depth %d\n", depth);
    while (depth > 0 && j < len) {
        buf[i] = i2cip_r_cmd_data(reg_base);
        ++(*act_read);
        ++j;
        --depth;
    }

    if (stop) {
        /* wait to idle */
        while (i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK);
    }
    else {
        /* FIXME : tx fifo empty is not transmit end, i2cip may transmit last byte */
        /* we may can wait a most long time out (lowest speed) */
    }

    i2cip_r_clr_all_intr(reg_base);

    return ret;
}
uint32_t hal_i2c_set_interrupt_handler(enum HAL_I2C_ID_T id, HAL_I2C_INT_HANDLER_T handler)
{
    hal_i2c_int_handlers[id] = handler;
    return 0;
}
/* simple mode end */
