#include "plat_addr_map.h"
#include "hal_spi.h"
#include "reg_spi.h"
#include "hal_trace.h"
#include "hal_dma.h"
#include "hal_cmu.h"
#include "hal_location.h"
#include "string.h"
#include "cmsis.h"

// TODO:
// 1) Add transfer timeout control

#ifndef SPI_FREQ
#define SPI_FREQ                        26000000
#endif

enum HAL_SPI_ID_T {
    HAL_SPI_ID_INTERNAL,
#ifdef CHIP_HAS_SPI
    HAL_SPI_ID_0,
#endif
#ifdef CHIP_HAS_SPILCD
    HAL_SPI_ID_SLCD,
#endif
#ifdef CHIP_HAS_SPIPHY
    HAL_SPI_ID_USBPHY,
#endif

    HAL_SPI_ID_QTY
};

struct HAL_SPI_MOD_NAME_T {
    enum HAL_CMU_MOD_ID_T mod;
    enum HAL_CMU_MOD_ID_T apb;
};

static struct SPI_T * const spi[HAL_SPI_ID_QTY] = {
#ifdef CHIP_HAS_SPI
    (struct SPI_T *)ISPI_BASE,
    (struct SPI_T *)SPI_BASE,
#else
    (struct SPI_T *)SPI_BASE,
#endif
#ifdef CHIP_HAS_SPILCD
    (struct SPI_T *)SPILCD_BASE,
#endif
#ifdef CHIP_HAS_SPIPHY
    (struct SPI_T *)SPIPHY_BASE,
#endif
};

static const struct HAL_SPI_MOD_NAME_T spi_mod[HAL_SPI_ID_QTY] = {
#ifdef CHIP_HAS_SPI
    {
        .mod = HAL_CMU_MOD_O_SPI_ITN,
        .apb = HAL_CMU_MOD_P_SPI_ITN,
    },
    {
        .mod = HAL_CMU_MOD_O_SPI,
        .apb = HAL_CMU_MOD_P_SPI,
    },
#else
    {
        .mod = HAL_CMU_MOD_O_SPI,
        .apb = HAL_CMU_MOD_P_SPI,
    },
#endif
#ifdef CHIP_HAS_SPILCD
    {
        .mod = HAL_CMU_MOD_O_SLCD,
        .apb = HAL_CMU_MOD_P_SLCD,
    },
#endif
#ifdef CHIP_HAS_SPIPHY
    {
        .mod = HAL_CMU_MOD_O_SPI_PHY,
        .apb = HAL_CMU_MOD_P_SPI_PHY,
    },
#endif
};

static struct HAL_SPI_CTRL_T BOOT_BSS_LOC spi_ctrl[HAL_SPI_ID_QTY];

static bool BOOT_BSS_LOC in_use[HAL_SPI_ID_QTY] = { false, };

static HAL_SPI_DMA_HANDLER_T BOOT_BSS_LOC spi_txdma_handler[HAL_SPI_ID_QTY];
static HAL_SPI_DMA_HANDLER_T BOOT_BSS_LOC spi_rxdma_handler[HAL_SPI_ID_QTY];
static uint8_t BOOT_BSS_LOC spi_txdma_chan[HAL_SPI_ID_QTY];
static uint8_t BOOT_BSS_LOC spi_rxdma_chan[HAL_SPI_ID_QTY];
static bool BOOT_BSS_LOC spi_init_done = false;

//static const char *invalid_id = "Invalid SPI ID: %d";

static inline uint8_t get_frame_bytes(enum HAL_SPI_ID_T id)
{
    uint8_t bits, cnt;

    bits = GET_BITFIELD(spi[id]->SSPCR0, SPI_SSPCR0_DSS) + 1;
    if (bits <= 8) {
        cnt = 1;
    } else if (bits <= 16) {
        cnt = 2;
    } else {
        cnt = 4;
    }

    return cnt;
}

int hal_spi_init_ctrl(const struct HAL_SPI_CFG_T *cfg, struct HAL_SPI_CTRL_T *ctrl)
{
    uint32_t div;
    uint16_t cpsdvsr, scr;

    ASSERT(cfg->rate <= SPI_FREQ / (MIN_CPSDVSR * (1 + MIN_SCR)), "SPI rate too large: %u", cfg->rate);
    ASSERT(cfg->rate >= SPI_FREQ / (MAX_CPSDVSR * (1 + MAX_SCR)), "SPI rate too small: %u", cfg->rate);
    ASSERT(cfg->tx_bits <= MAX_DATA_BITS && cfg->tx_bits >= MIN_DATA_BITS, "Invalid SPI TX bits: %d", cfg->tx_bits);
    ASSERT(cfg->rx_bits <= MAX_DATA_BITS && cfg->rx_bits >= MIN_DATA_BITS, "Invalid SPI RX bits: %d", cfg->rx_bits);
    ASSERT(cfg->rx_frame_bits <= MAX_DATA_BITS && (cfg->rx_frame_bits == 0 || cfg->rx_frame_bits > cfg->rx_bits),
        "Invalid SPI RX FRAME bits: %d", cfg->rx_frame_bits);

    div = SPI_FREQ / cfg->rate;
    cpsdvsr = div / (1 + MAX_SCR);
    if (cpsdvsr < 2) {
        cpsdvsr = 2;
    } else {
        cpsdvsr = (cpsdvsr + 2) & ~1;
        if (cpsdvsr > MAX_CPSDVSR) {
            cpsdvsr = MAX_CPSDVSR;
        }
    }
    scr = div / cpsdvsr;
    if (scr > 0) {
        scr -= 1;
    }
    if (scr > MAX_SCR) {
        scr = MAX_SCR;
    }

    ctrl->sspcr0[HAL_SPI_XFER_TYPE_SEND] = SPI_SSPCR0_SCR(scr) |
                     (cfg->clk_delay_half ? SPI_SSPCR0_SPH : 0) |
                     (cfg->clk_polarity ? SPI_SSPCR0_SPO : 0) |
                     SPI_SSPCR0_FRF(0) | // Only support Motorola SPI frame format
                     SPI_SSPCR0_DSS(cfg->tx_bits - 1);
    ctrl->sspcr1 = SPI_SSPCR1_SOD |
                   (cfg->slave? SPI_SSPCR1_MS : 0) |
                   SPI_SSPCR1_SSE;
    ctrl->sspcpsr = SPI_SSPCPSR_CPSDVSR(cpsdvsr);
    ctrl->sspdmacr = (cfg->dma_tx ? SPI_SSPDMACR_TXDMAE : 0) |
                     (cfg->dma_rx ? SPI_SSPDMACR_RXDMAE : 0);
    if (cfg->rx_frame_bits > 0) {
        ctrl->sspcr0[HAL_SPI_XFER_TYPE_RECV] = (ctrl->sspcr0[0] & ~SPI_SSPCR0_DSS_MASK) |
                                               SPI_SSPCR0_DSS(cfg->rx_frame_bits - 1);
        ctrl->ssprxcr = SPI_SSPRXCR_EN | SPI_SSPRXCR_OEN_POLARITY | SPI_SSPRXCR_RXBITS(cfg->rx_bits - 1);
    } else {
        ctrl->sspcr0[HAL_SPI_XFER_TYPE_RECV] = (ctrl->sspcr0[0] & ~SPI_SSPCR0_DSS_MASK) |
                                               SPI_SSPCR0_DSS(cfg->rx_bits - 1);
        ctrl->ssprxcr = 0;
    }

    return 0;
}

static void hal_spi_enable_id(enum HAL_SPI_ID_T id, const struct HAL_SPI_CTRL_T *ctrl, enum HAL_SPI_XFER_TYPE_T type)
{
    spi[id]->SSPCR0 = ctrl->sspcr0[(type == HAL_SPI_XFER_TYPE_SEND) ? HAL_SPI_XFER_TYPE_SEND : HAL_SPI_XFER_TYPE_RECV];
    spi[id]->SSPCR1 = ctrl->sspcr1;
    spi[id]->SSPCPSR = ctrl->sspcpsr;
    spi[id]->SSPDMACR = ctrl->sspdmacr;
    spi[id]->SSPRXCR = (type == HAL_SPI_XFER_TYPE_SEND) ? 0 : ctrl->ssprxcr;
}

static void POSSIBLY_UNUSED hal_spi_disable_id(enum HAL_SPI_ID_T id)
{
    spi[id]->SSPCR1 &= ~SPI_SSPCR1_SSE;
}

static void POSSIBLY_UNUSED hal_spi_get_ctrl_id(enum HAL_SPI_ID_T id, struct HAL_SPI_CTRL_T *ctrl)
{
    ctrl->sspcr0[HAL_SPI_XFER_TYPE_SEND] = spi[id]->SSPCR0;
    ctrl->sspcr1 = spi[id]->SSPCR1;
    ctrl->sspcpsr = spi[id]->SSPCPSR;
    ctrl->sspdmacr = spi[id]->SSPDMACR;
}

static void POSSIBLY_UNUSED hal_spi_restore_ctrl_id(enum HAL_SPI_ID_T id, const struct HAL_SPI_CTRL_T *ctrl)
{
    hal_spi_enable_id(id, ctrl, HAL_SPI_XFER_TYPE_SEND);
}

static int hal_spi_open_id(enum HAL_SPI_ID_T id, const struct HAL_SPI_CFG_T *cfg)
{
    int i;
    int ret;

    //ASSERT(id < HAL_SPI_ID_QTY, invalid_id, id);

    if (!spi_init_done) {
        spi_init_done = true;
        for (i = HAL_SPI_ID_INTERNAL; i < HAL_SPI_ID_QTY; i++) {
            spi_txdma_chan[i] = HAL_DMA_CHAN_NONE;
            spi_rxdma_chan[i] = HAL_DMA_CHAN_NONE;
        }
    }

    ret = hal_spi_init_ctrl(cfg, &spi_ctrl[id]);
    if (ret) {
        return ret;
    }

    hal_cmu_clock_enable(spi_mod[id].mod);
    hal_cmu_clock_enable(spi_mod[id].apb);
    hal_cmu_reset_clear(spi_mod[id].mod);
    hal_cmu_reset_clear(spi_mod[id].apb);

    hal_spi_enable_id(id, &spi_ctrl[id], HAL_SPI_XFER_TYPE_SEND);

    return 0;
}

static void POSSIBLY_UNUSED hal_spi_close_id(enum HAL_SPI_ID_T id)
{
    hal_spi_disable_id(id);

    hal_cmu_reset_set(spi_mod[id].apb);
    hal_cmu_reset_set(spi_mod[id].mod);
    hal_cmu_clock_disable(spi_mod[id].apb);
    hal_cmu_clock_disable(spi_mod[id].mod);
}

static bool hal_spi_busy_id(enum HAL_SPI_ID_T id)
{
    return ((spi[id]->SSPCR1 & SPI_SSPCR1_SSE) && (spi[id]->SSPSR & SPI_SSPSR_BSY));
}

static void hal_spi_enable_slave_output_id(enum HAL_SPI_ID_T id)
{
    if (spi[id]->SSPCR1 & SPI_SSPCR1_MS) {
        spi[id]->SSPCR1 &= ~SPI_SSPCR1_SOD;
    }
}

static void hal_spi_disable_slave_output_id(enum HAL_SPI_ID_T id)
{
    if (spi[id]->SSPCR1 & SPI_SSPCR1_MS) {
        spi[id]->SSPCR1 |= SPI_SSPCR1_SOD;
    }
}

static int hal_spi_send_id(enum HAL_SPI_ID_T id, const void *data, uint32_t len)
{
    uint8_t cnt;
    uint32_t sent, value;
    int ret;

    //ASSERT(id < HAL_SPI_ID_QTY, invalid_id, id);
    ASSERT((spi[id]->SSPDMACR & SPI_SSPDMACR_TXDMAE) == 0, "TX-DMA configured on SPI %d", id);

    cnt = get_frame_bytes(id);

    if (len == 0 || (len & (cnt - 1)) != 0) {
        return -1;
    }

    sent = 0;

    hal_spi_enable_slave_output_id(id);

    while (sent < len) {
        if ((spi[id]->SSPCR1 & SPI_SSPCR1_SSE) == 0) {
            break;
        }
        if (spi[id]->SSPSR & SPI_SSPSR_TNF) {
            value = 0;
            memcpy(&value, (uint8_t *)data + sent, cnt);
            spi[id]->SSPDR = value;
            sent += cnt;
        }
    }

    if (sent >= len) {
        ret = 0;
    } else {
        ret = 1;
    }

    while (hal_spi_busy_id(id));
    hal_spi_disable_slave_output_id(id);

    return ret;
}

static int hal_spi_recv_id(enum HAL_SPI_ID_T id, const void *cmd, void *data, uint32_t len)
{
    uint8_t cnt;
    uint32_t sent, recv, value;
    int ret;

    //ASSERT(id < HAL_SPI_ID_QTY, invalid_id, id);
    ASSERT((spi[id]->SSPDMACR & (SPI_SSPDMACR_TXDMAE | SPI_SSPDMACR_RXDMAE)) == 0, "RX/TX-DMA configured on SPI %d", id);

    cnt = get_frame_bytes(id);

    if (len == 0 || (len & (cnt - 1)) != 0) {
        return -1;
    }

    // Rx transaction should start from idle state
    if (spi[id]->SSPSR & SPI_SSPSR_BSY) {
        return -11;
    }

    sent = 0;
    recv = 0;

    // Flush the RX FIFO by reset or CPU read
    while (spi[id]->SSPSR & SPI_SSPSR_RNE) {
        spi[id]->SSPDR;
    }
    spi[id]->SSPICR = ~0;

    hal_spi_enable_slave_output_id(id);

    while (recv < len || sent < len) {
        if ((spi[id]->SSPCR1 & SPI_SSPCR1_SSE) == 0) {
            break;
        }
        if (sent < len && spi[id]->SSPSR & SPI_SSPSR_TNF) {
            value = 0;
            memcpy(&value, (uint8_t *)cmd + sent, cnt);
            spi[id]->SSPDR = value;
            sent += cnt;
        }
        if (recv < len && spi[id]->SSPSR & SPI_SSPSR_RNE) {
            value = spi[id]->SSPDR;
            memcpy((uint8_t *)data + recv, &value, cnt);
            recv += cnt;
        }
    }

    if (recv >= len && sent >= len) {
        ret = 0;
    } else {
        ret = 1;
    }

    while (hal_spi_busy_id(id));
    hal_spi_disable_slave_output_id(id);

    return ret;
}

static void hal_spi_txdma_handler(uint8_t chan, uint32_t remains, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    enum HAL_SPI_ID_T id;
    uint32_t lock;

    lock = int_lock();
    for (id = HAL_SPI_ID_INTERNAL; id < HAL_SPI_ID_QTY; id++) {
        if (spi_txdma_chan[id] == chan) {
            spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
            break;
        }
    }
    int_unlock(lock);

    if (id == HAL_SPI_ID_QTY) {
        return;
    }

    hal_gpdma_free_chan(chan);

    clear_bool_flag(&in_use[id]);

    if (spi_txdma_handler[id]) {
        spi_txdma_handler[id](error);
    }
}

static int hal_spi_dma_send_id(enum HAL_SPI_ID_T id, const void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    uint8_t cnt;
    enum HAL_DMA_RET_T ret;
    struct HAL_DMA_CH_CFG_T dma_cfg;
    enum HAL_DMA_WDITH_T dma_width;
    uint32_t lock;
    enum HAL_DMA_PERIPH_T dst_periph;

    //ASSERT(id < HAL_SPI_ID_QTY, invalid_id, id);
    ASSERT((spi[id]->SSPDMACR & SPI_SSPDMACR_TXDMAE), "TX-DMA not configured on SPI %d", id);

    cnt = get_frame_bytes(id);

    if ((len & (cnt - 1)) != 0) {
        return -1;
    }
    if (((uint32_t)data & (cnt - 1)) != 0) {
        return -2;
    }

    if (id == HAL_SPI_ID_INTERNAL) {
        dst_periph = HAL_GPDMA_ISPI_TX;
    } else {
        dst_periph = HAL_GPDMA_SPI_TX;
    }

    lock = int_lock();
    if (spi_txdma_chan[id] != HAL_DMA_CHAN_NONE) {
        int_unlock(lock);
        return -3;
    }
    spi_txdma_chan[id] = hal_gpdma_get_chan(dst_periph, HAL_DMA_HIGH_PRIO);
    if (spi_txdma_chan[id] == HAL_DMA_CHAN_NONE) {
        int_unlock(lock);
        return -4;
    }
    int_unlock(lock);

    if (cnt == 1) {
        dma_width = HAL_DMA_WIDTH_BYTE;
    } else if (cnt == 2) {
        dma_width = HAL_DMA_WIDTH_HALFWORD;
    } else {
        dma_width = HAL_DMA_WIDTH_WORD;
    }

    memset(&dma_cfg, 0, sizeof(dma_cfg));
    dma_cfg.ch = spi_txdma_chan[id];
    dma_cfg.dst = 0; // useless
    dma_cfg.dst_bsize = HAL_DMA_BSIZE_4;
    dma_cfg.dst_periph = dst_periph;
    dma_cfg.dst_width = dma_width;
    dma_cfg.handler = handler ? hal_spi_txdma_handler : NULL;
    dma_cfg.src = (uint32_t)data;
    dma_cfg.src_bsize = HAL_DMA_BSIZE_16;
    //dma_cfg.src_periph = HAL_GPDMA_PERIPH_QTY; // useless
    dma_cfg.src_tsize = len / cnt;
    dma_cfg.src_width = dma_width;
    dma_cfg.try_burst = 0;
    dma_cfg.type = HAL_DMA_FLOW_M2P_DMA;

    hal_spi_enable_slave_output_id(id);

    ret = hal_gpdma_start(&dma_cfg);
    if (ret != HAL_DMA_OK) {
        hal_spi_disable_slave_output_id(id);
        return -5;
    }

    if (handler == NULL) {
        while ((spi[id]->SSPCR1 & SPI_SSPCR1_SSE) && hal_gpdma_chan_busy(spi_txdma_chan[id]));
        hal_gpdma_free_chan(spi_txdma_chan[id]);
        spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
        while (hal_spi_busy_id(id));
        hal_spi_disable_slave_output_id(id);
    }

    return 0;
}

void hal_spi_stop_dma_send_id(enum HAL_SPI_ID_T id)
{
    uint32_t lock;
    uint8_t tx_chan;

    lock = int_lock();
    tx_chan = spi_txdma_chan[id];
    spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
    int_unlock(lock);

    if (tx_chan != HAL_DMA_CHAN_NONE) {
        hal_gpdma_cancel(tx_chan);
        hal_gpdma_free_chan(tx_chan);
    }

    clear_bool_flag(&in_use[id]);
}

static void hal_spi_rxdma_handler(uint8_t chan, uint32_t remains, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    enum HAL_SPI_ID_T id;
    uint32_t lock;
    uint8_t tx_chan = HAL_DMA_CHAN_NONE;

    lock = int_lock();
    for (id = HAL_SPI_ID_INTERNAL; id < HAL_SPI_ID_QTY; id++) {
        if (spi_rxdma_chan[id] == chan) {
            tx_chan = spi_txdma_chan[id];
            spi_rxdma_chan[id] = HAL_DMA_CHAN_NONE;
            spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
            break;
        }
    }
    int_unlock(lock);

    if (id == HAL_SPI_ID_QTY) {
        return;
    }

    hal_gpdma_free_chan(chan);
    hal_gpdma_cancel(tx_chan);
    hal_gpdma_free_chan(tx_chan);

    hal_spi_enable_id(id, &spi_ctrl[id], HAL_SPI_XFER_TYPE_SEND);
    clear_bool_flag(&in_use[id]);

    if (spi_rxdma_handler[id]) {
        spi_rxdma_handler[id](error);
    }
}

static int hal_spi_dma_recv_id(enum HAL_SPI_ID_T id, const void *cmd, void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    uint8_t cnt;
    enum HAL_DMA_RET_T ret;
    struct HAL_DMA_CH_CFG_T dma_cfg;
    enum HAL_DMA_WDITH_T dma_width;
    uint32_t lock;
    int result;
    enum HAL_DMA_PERIPH_T dst_periph, src_periph;

    //ASSERT(id < HAL_SPI_ID_QTY, invalid_id, id);
    ASSERT((spi[id]->SSPDMACR & (SPI_SSPDMACR_TXDMAE | SPI_SSPDMACR_RXDMAE)) ==
        (SPI_SSPDMACR_TXDMAE | SPI_SSPDMACR_RXDMAE), "RX/TX-DMA not configured on SPI %d", id);

    result = 0;

    cnt = get_frame_bytes(id);

    if ((len & (cnt - 1)) != 0) {
        return -1;
    }
    if (((uint32_t)data & (cnt - 1)) != 0) {
        return -2;
    }

    // Rx transaction should start from idle state
    if (spi[id]->SSPSR & SPI_SSPSR_BSY) {
        return -11;
    }

    if (id == HAL_SPI_ID_INTERNAL) {
        src_periph = HAL_GPDMA_ISPI_RX;
        dst_periph = HAL_GPDMA_ISPI_TX;
    } else {
        src_periph = HAL_GPDMA_SPI_RX;
        dst_periph = HAL_GPDMA_SPI_TX;
    }

    lock = int_lock();
    if (spi_txdma_chan[id] != HAL_DMA_CHAN_NONE || spi_rxdma_chan[id] != HAL_DMA_CHAN_NONE) {
        int_unlock(lock);
        return -3;
    }
    spi_txdma_chan[id] = hal_gpdma_get_chan(dst_periph, HAL_DMA_HIGH_PRIO);
    if (spi_txdma_chan[id] == HAL_DMA_CHAN_NONE) {
        int_unlock(lock);
        return -4;
    }
    spi_rxdma_chan[id] = hal_gpdma_get_chan(src_periph, HAL_DMA_HIGH_PRIO);
    if (spi_rxdma_chan[id] == HAL_DMA_CHAN_NONE) {
        hal_gpdma_free_chan(spi_txdma_chan[id]);
        spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
        int_unlock(lock);
        return -5;
    }
    int_unlock(lock);

    if (cnt == 1) {
        dma_width = HAL_DMA_WIDTH_BYTE;
    } else if (cnt == 2) {
        dma_width = HAL_DMA_WIDTH_HALFWORD;
    } else {
        dma_width = HAL_DMA_WIDTH_WORD;
    }

    memset(&dma_cfg, 0, sizeof(dma_cfg));
    dma_cfg.ch = spi_rxdma_chan[id];
    dma_cfg.dst = (uint32_t)data;
    dma_cfg.dst_bsize = HAL_DMA_BSIZE_16;
    //dma_cfg.dst_periph = HAL_GPDMA_PERIPH_QTY; // useless
    dma_cfg.dst_width = dma_width;
    dma_cfg.handler = handler ? hal_spi_rxdma_handler : NULL;
    dma_cfg.src = 0; // useless
    dma_cfg.src_periph = src_periph;
    dma_cfg.src_bsize = HAL_DMA_BSIZE_4;
    dma_cfg.src_tsize = len / cnt;
    dma_cfg.src_width = dma_width;
    dma_cfg.try_burst = 0;
    dma_cfg.type = HAL_DMA_FLOW_P2M_DMA;

    // Flush the RX FIFO by reset or DMA read (CPU read is forbidden when DMA is enabled)
    if (spi[id]->SSPSR & SPI_SSPSR_RNE) {
        // Reset SPI MODULE might cause the increment of the FIFO pointer
        hal_cmu_reset_pulse(spi_mod[id].mod);
        // Reset SPI APB will reset the FIFO pointer
        hal_cmu_reset_pulse(spi_mod[id].apb);
        hal_spi_enable_id(id, &spi_ctrl[id], HAL_SPI_XFER_TYPE_RECV);
    }
    spi[id]->SSPICR = ~0;

    ret = hal_gpdma_start(&dma_cfg);
    if (ret != HAL_DMA_OK) {
        result = -8;
        goto _exit;
    }

    dma_cfg.ch = spi_txdma_chan[id];
    dma_cfg.dst = 0; // useless
    dma_cfg.dst_bsize = HAL_DMA_BSIZE_4;
    dma_cfg.dst_periph = dst_periph;
    dma_cfg.dst_width = dma_width;
    dma_cfg.handler = NULL;
    dma_cfg.src = (uint32_t)cmd;
    dma_cfg.src_bsize = HAL_DMA_BSIZE_16;
    //dma_cfg.src_periph = HAL_GPDMA_PERIPH_QTY; // useless
    dma_cfg.src_tsize = len / cnt;
    dma_cfg.src_width = dma_width;
    dma_cfg.try_burst = 0;
    dma_cfg.type = HAL_DMA_FLOW_M2P_DMA;

    hal_spi_enable_slave_output_id(id);

    ret = hal_gpdma_start(&dma_cfg);
    if (ret != HAL_DMA_OK) {
        result = -9;
        goto _exit;
    }

    if (handler == NULL) {
        while ((spi[id]->SSPCR1 & SPI_SSPCR1_SSE) && hal_gpdma_chan_busy(spi_rxdma_chan[id]));
    }

_exit:
    if (result || handler == NULL) {
        hal_gpdma_cancel(spi_txdma_chan[id]);
        hal_gpdma_free_chan(spi_txdma_chan[id]);
        spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;

        while (hal_spi_busy_id(id));
        hal_spi_disable_slave_output_id(id);

        hal_gpdma_cancel(spi_rxdma_chan[id]);
        hal_gpdma_free_chan(spi_rxdma_chan[id]);
        spi_rxdma_chan[id] = HAL_DMA_CHAN_NONE;
    }

    return 0;
}

void hal_spi_stop_dma_recv_id(enum HAL_SPI_ID_T id)
{
    uint32_t lock;
    uint8_t rx_chan, tx_chan;

    lock = int_lock();
    rx_chan = spi_rxdma_chan[id];
    spi_rxdma_chan[id] = HAL_DMA_CHAN_NONE;
    tx_chan = spi_txdma_chan[id];
    spi_txdma_chan[id] = HAL_DMA_CHAN_NONE;
    int_unlock(lock);

    if (rx_chan == HAL_DMA_CHAN_NONE && tx_chan == HAL_DMA_CHAN_NONE) {
        return;
    }

    if (rx_chan != HAL_DMA_CHAN_NONE) {
        hal_gpdma_cancel(rx_chan);
        hal_gpdma_free_chan(rx_chan);
    }
    if (tx_chan != HAL_DMA_CHAN_NONE) {
        hal_gpdma_cancel(tx_chan);
        hal_gpdma_free_chan(tx_chan);
    }

    hal_spi_enable_id(id, &spi_ctrl[id], HAL_SPI_XFER_TYPE_SEND);
    clear_bool_flag(&in_use[id]);
}

//------------------------------------------------------------
// ISPI functions
//------------------------------------------------------------

int hal_ispi_open(const struct HAL_SPI_CFG_T *cfg)
{
    return hal_spi_open_id(HAL_SPI_ID_INTERNAL, cfg);
}

int hal_ispi_busy(void)
{
    return hal_spi_busy_id(HAL_SPI_ID_INTERNAL);
}

int hal_ispi_send(const void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_INTERNAL])) {
        return -31;
    }

    ret = hal_spi_send_id(HAL_SPI_ID_INTERNAL, data, len);

    clear_bool_flag(&in_use[HAL_SPI_ID_INTERNAL]);

    return ret;
}

int hal_ispi_recv(const void *cmd, void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_INTERNAL])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_INTERNAL, &spi_ctrl[HAL_SPI_ID_INTERNAL], HAL_SPI_XFER_TYPE_RECV);
    ret = hal_spi_recv_id(HAL_SPI_ID_INTERNAL, cmd, data, len);
    hal_spi_enable_id(HAL_SPI_ID_INTERNAL, &spi_ctrl[HAL_SPI_ID_INTERNAL], HAL_SPI_XFER_TYPE_SEND);

    clear_bool_flag(&in_use[HAL_SPI_ID_INTERNAL]);

    return ret;
}

int hal_ispi_dma_send(const void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_INTERNAL])) {
        return -31;
    }

    ret = hal_spi_dma_send_id(HAL_SPI_ID_INTERNAL, data, len, handler);

    if (ret || handler == NULL) {
        clear_bool_flag(&in_use[HAL_SPI_ID_INTERNAL]);
    }

    return ret;
}

int hal_ispi_dma_recv(const void *cmd, void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_INTERNAL])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_INTERNAL, &spi_ctrl[HAL_SPI_ID_INTERNAL], HAL_SPI_XFER_TYPE_RECV);

    ret = hal_spi_dma_recv_id(HAL_SPI_ID_INTERNAL, cmd, data, len, handler);

    if (ret || handler == NULL) {
        hal_spi_enable_id(HAL_SPI_ID_INTERNAL, &spi_ctrl[HAL_SPI_ID_INTERNAL], HAL_SPI_XFER_TYPE_SEND);

        clear_bool_flag(&in_use[HAL_SPI_ID_INTERNAL]);
    }

    return ret;
}

void hal_ispi_stop_dma_send(void)
{
    hal_spi_stop_dma_send_id(HAL_SPI_ID_INTERNAL);
}

void hal_ispi_stop_dma_recv(void)
{
    hal_spi_stop_dma_recv_id(HAL_SPI_ID_INTERNAL);
}

#ifdef CHIP_HAS_SPI
//------------------------------------------------------------
// SPI peripheral functions
//------------------------------------------------------------

int hal_spi_open(const struct HAL_SPI_CFG_T *cfg)
{
    return hal_spi_open_id(HAL_SPI_ID_0, cfg);
}

void hal_spi_close(void)
{
    hal_spi_close_id(HAL_SPI_ID_0);
}

int hal_spi_busy(void)
{
    return hal_spi_busy_id(HAL_SPI_ID_0);
}

int hal_spi_send(const void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    ret =  hal_spi_send_id(HAL_SPI_ID_0, data, len);

    clear_bool_flag(&in_use[HAL_SPI_ID_0]);

    return ret;
}

int hal_spi_recv(const void *cmd, void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_0, &spi_ctrl[HAL_SPI_ID_0], HAL_SPI_XFER_TYPE_RECV);
    ret = hal_spi_recv_id(HAL_SPI_ID_0, cmd, data, len);
    hal_spi_enable_id(HAL_SPI_ID_0, &spi_ctrl[HAL_SPI_ID_0], HAL_SPI_XFER_TYPE_SEND);

    clear_bool_flag(&in_use[HAL_SPI_ID_0]);

    return ret;
}

int hal_spi_dma_send(const void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    ret = hal_spi_dma_send_id(HAL_SPI_ID_0, data, len, handler);

    if (ret || handler == NULL) {
        clear_bool_flag(&in_use[HAL_SPI_ID_0]);
    }

    return ret;
}

int hal_spi_dma_recv(const void *cmd, void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_0, &spi_ctrl[HAL_SPI_ID_0], HAL_SPI_XFER_TYPE_RECV);

    ret = hal_spi_dma_recv_id(HAL_SPI_ID_0, cmd, data, len, handler);

    if (ret || handler == NULL) {
        hal_spi_enable_id(HAL_SPI_ID_0, &spi_ctrl[HAL_SPI_ID_0], HAL_SPI_XFER_TYPE_SEND);

        clear_bool_flag(&in_use[HAL_SPI_ID_0]);
    }

    return ret;
}

void hal_spi_stop_dma_send(void)
{
    hal_spi_stop_dma_send_id(HAL_SPI_ID_0);
}

void hal_spi_stop_dma_recv(void)
{
    hal_spi_stop_dma_recv_id(HAL_SPI_ID_0);
}

int hal_spi_enable_and_send(const struct HAL_SPI_CTRL_T *ctrl, const void *data, uint32_t len)
{
    int ret;
    struct HAL_SPI_CTRL_T saved;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    hal_spi_get_ctrl_id(HAL_SPI_ID_0, &saved);
    hal_spi_enable_id(HAL_SPI_ID_0, ctrl, HAL_SPI_XFER_TYPE_SEND);
    ret = hal_spi_send_id(HAL_SPI_ID_0, data, len);
    hal_spi_restore_ctrl_id(HAL_SPI_ID_0, &saved);

    clear_bool_flag(&in_use[HAL_SPI_ID_0]);

    return ret;
}

int hal_spi_enable_and_recv(const struct HAL_SPI_CTRL_T *ctrl, const void *cmd, void *data, uint32_t len)
{
    int ret;
    struct HAL_SPI_CTRL_T saved;

    if (set_bool_flag(&in_use[HAL_SPI_ID_0])) {
        return -31;
    }

    hal_spi_get_ctrl_id(HAL_SPI_ID_0, &saved);
    hal_spi_enable_id(HAL_SPI_ID_0, ctrl, HAL_SPI_XFER_TYPE_RECV);
    ret = hal_spi_recv_id(HAL_SPI_ID_0, cmd, data, len);
    hal_spi_restore_ctrl_id(HAL_SPI_ID_0, &saved);

    clear_bool_flag(&in_use[HAL_SPI_ID_0]);

    return ret;
}
#endif // CHIP_HAS_SPI

#ifdef CHIP_HAS_SPILCD
//------------------------------------------------------------
// SPI LCD functions
//------------------------------------------------------------

int hal_spilcd_open(const struct HAL_SPI_CFG_T *cfg)
{
    return hal_spi_open_id(HAL_SPI_ID_SLCD, cfg);
}

void hal_spilcd_close(void)
{
    hal_spi_close_id(HAL_SPI_ID_SLCD);
}

int hal_spilcd_busy(void)
{
    return hal_spi_busy_id(HAL_SPI_ID_SLCD);
}

int hal_spilcd_send(const void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    ret =  hal_spi_send_id(HAL_SPI_ID_SLCD, data, len);

    clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);

    return ret;
}

int hal_spilcd_recv(const void *cmd, void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_SLCD, &spi_ctrl[HAL_SPI_ID_SLCD], HAL_SPI_XFER_TYPE_RECV);
    ret = hal_spi_recv_id(HAL_SPI_ID_SLCD, cmd, data, len);
    hal_spi_enable_id(HAL_SPI_ID_SLCD, &spi_ctrl[HAL_SPI_ID_SLCD], HAL_SPI_XFER_TYPE_SEND);

    clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);

    return ret;
}

int hal_spilcd_dma_send(const void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    ret = hal_spi_dma_send_id(HAL_SPI_ID_SLCD, data, len, handler);

    if (ret || handler == NULL) {
        clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);
    }

    return ret;
}

int hal_spilcd_dma_recv(const void *cmd, void *data, uint32_t len, HAL_SPI_DMA_HANDLER_T handler)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_SLCD, &spi_ctrl[HAL_SPI_ID_SLCD], HAL_SPI_XFER_TYPE_RECV);

    ret = hal_spi_dma_recv_id(HAL_SPI_ID_SLCD, cmd, data, len, handler);

    if (ret || handler == NULL) {
        hal_spi_enable_id(HAL_SPI_ID_SLCD, &spi_ctrl[HAL_SPI_ID_SLCD], HAL_SPI_XFER_TYPE_SEND);

        clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);
    }

    return ret;
}

void hal_spilcd_stop_dma_send(void)
{
    hal_spi_stop_dma_send_id(HAL_SPI_ID_SLCD);
}

void hal_spilcd_stop_dma_recv(void)
{
    hal_spi_stop_dma_recv_id(HAL_SPI_ID_SLCD);
}

int hal_spilcd_set_data_mode(void)
{
    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    spi_ctrl[HAL_SPI_ID_SLCD].sspcr1 |= SPI_LCD_DC_DATA;
    spi[HAL_SPI_ID_SLCD]->SSPCR1 = spi_ctrl[HAL_SPI_ID_SLCD].sspcr1;

    clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);
    return 0;
}

int hal_spilcd_set_cmd_mode(void)
{
    if (set_bool_flag(&in_use[HAL_SPI_ID_SLCD])) {
        return -31;
    }

    spi_ctrl[HAL_SPI_ID_SLCD].sspcr1 &= ~SPI_LCD_DC_DATA;
    spi[HAL_SPI_ID_SLCD]->SSPCR1 = spi_ctrl[HAL_SPI_ID_SLCD].sspcr1;

    clear_bool_flag(&in_use[HAL_SPI_ID_SLCD]);
    return 0;
}
#endif // CHIP_HAS_SPILCD

#ifdef CHIP_HAS_SPIPHY
//------------------------------------------------------------
// SPI USB PHY functions
//------------------------------------------------------------

int hal_spiphy_open(const struct HAL_SPI_CFG_T *cfg)
{
    return hal_spi_open_id(HAL_SPI_ID_USBPHY, cfg);
}

void hal_spiphy_close(void)
{
    hal_spi_close_id(HAL_SPI_ID_USBPHY);
}

int hal_spiphy_busy(void)
{
    return hal_spi_busy_id(HAL_SPI_ID_USBPHY);
}

int hal_spiphy_send(const void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_USBPHY])) {
        return -31;
    }

    ret =  hal_spi_send_id(HAL_SPI_ID_USBPHY, data, len);

    clear_bool_flag(&in_use[HAL_SPI_ID_USBPHY]);

    return ret;
}

int hal_spiphy_recv(const void *cmd, void *data, uint32_t len)
{
    int ret;

    if (set_bool_flag(&in_use[HAL_SPI_ID_USBPHY])) {
        return -31;
    }

    hal_spi_enable_id(HAL_SPI_ID_USBPHY, &spi_ctrl[HAL_SPI_ID_USBPHY], HAL_SPI_XFER_TYPE_RECV);
    ret = hal_spi_recv_id(HAL_SPI_ID_USBPHY, cmd, data, len);
    hal_spi_enable_id(HAL_SPI_ID_USBPHY, &spi_ctrl[HAL_SPI_ID_USBPHY], HAL_SPI_XFER_TYPE_SEND);

    clear_bool_flag(&in_use[HAL_SPI_ID_USBPHY]);

    return ret;
}
#endif // CHIP_HAS_SPIPHY

