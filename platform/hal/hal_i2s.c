#include "plat_types.h"
#include "plat_addr_map.h"
#include "reg_i2sip.h"
#include "hal_i2sip.h"
#include "hal_i2s.h"
#include "hal_uart.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "hal_iomux.h"
#include "analog.h"

#include "hal_cmu.h"

#define HAL_I2S_TX_FIFO_TRIGGER_LEVEL   (I2SIP_FIFO_DEPTH/2)
#define HAL_I2S_RX_FIFO_TRIGGER_LEVEL   (I2SIP_FIFO_DEPTH/2)

#define HAL_I2S_YES 1
#define HAL_I2S_NO 0

#ifdef CHIP_BEST1000
#define I2S_CMU_DIV                     CODEC_PLL_DIV
#define I2S_CHAN_NUM                    1
#else
#define I2S_CMU_DIV                     CODEC_CMU_DIV
#define I2S_CHAN_NUM                    4
#endif

enum HAL_I2S_STATUS_T {
    HAL_I2S_STATUS_NULL,
    HAL_I2S_STATUS_OPENED,
    HAL_I2S_STATUS_STARTED,
};

struct I2S_SAMPLE_RATE_T{
    enum AUD_SAMPRATE_T sample_rate;
    uint32_t codec_freq;
    uint8_t codec_div;
    uint8_t cmu_div;
};

static const struct I2S_SAMPLE_RATE_T i2s_sample_rate[]={
    {AUD_SAMPRATE_8000,   CODEC_FREQ_48K_SERIES,   CODEC_PLL_DIV, I2S_CMU_DIV},
    {AUD_SAMPRATE_16000,  CODEC_FREQ_48K_SERIES,   CODEC_PLL_DIV, I2S_CMU_DIV},
    {AUD_SAMPRATE_32000,  CODEC_FREQ_48K_SERIES,   CODEC_PLL_DIV, I2S_CMU_DIV},
    {AUD_SAMPRATE_44100,  CODEC_FREQ_44_1K_SERIES, CODEC_PLL_DIV, I2S_CMU_DIV},
    {AUD_SAMPRATE_48000,  CODEC_FREQ_48K_SERIES,   CODEC_PLL_DIV, I2S_CMU_DIV},
    {AUD_SAMPRATE_96000,  CODEC_FREQ_48K_SERIES,   CODEC_PLL_DIV, I2S_CMU_DIV},
};

static const char * const invalid_id = "Invalid I2S ID: %d\n";
//static const char * const invalid_ch = "Invalid I2S CH: %d\n";

static enum HAL_I2S_STATUS_T i2s_status[HAL_I2S_ID_NUM][AUD_STREAM_NUM];
static enum HAL_I2S_MODE_T i2s_mode[HAL_I2S_ID_NUM];
static bool i2s_dma[HAL_I2S_ID_NUM][AUD_STREAM_NUM];

static inline uint32_t _i2s_get_reg_base(enum HAL_I2S_ID_T id)
{
    ASSERT(id < HAL_I2S_ID_NUM, invalid_id, id);
    switch(id) {
        case HAL_I2S_ID_0:
        default:
            return I2S_BASE;
            break;
    }
	return 0;
}

int hal_i2s_open(enum HAL_I2S_ID_T id, enum AUD_STREAM_T stream, enum HAL_I2S_MODE_T mode)
{
    uint32_t reg_base;

    ASSERT(mode == HAL_I2S_MODE_MASTER || mode == HAL_I2S_MODE_SLAVE,
        "Invalid I2S mode for stream %d: %d", stream, mode);
    ASSERT(i2s_mode[id] == HAL_I2S_MODE_NULL || i2s_mode[id] == mode,
        "Incompatible I2S mode for stream %d: prev=%d cur=%d", stream, i2s_mode[id], mode);

    if (i2s_status[id][stream] != HAL_I2S_STATUS_NULL) {
        TRACE("Invalid I2S opening status for stream %d", stream, i2s_status[id][stream]);
        return 1;
    }

    if (i2s_status[id][AUD_STREAM_PLAYBACK] == HAL_I2S_STATUS_NULL &&
            i2s_status[id][AUD_STREAM_CAPTURE] == HAL_I2S_STATUS_NULL) {
        reg_base = _i2s_get_reg_base(id);

#ifndef SIMU
        analog_aud_pll_open(ANA_AUD_PLL_USER_I2S);
#endif

        hal_iomux_set_i2s();
        hal_cmu_i2s_clock_enable();
        hal_cmu_clock_enable(HAL_CMU_MOD_O_I2S);
        hal_cmu_clock_enable(HAL_CMU_MOD_P_I2S);
        hal_cmu_reset_clear(HAL_CMU_MOD_O_I2S);
        hal_cmu_reset_clear(HAL_CMU_MOD_P_I2S);
        i2sip_w_enable_i2sip(reg_base, HAL_I2S_YES);

        for (int i = 0; i < I2S_CHAN_NUM; i++) {
            i2sip_w_enable_rx_channel(reg_base, i, HAL_I2S_NO);
            i2sip_w_enable_tx_channel(reg_base, i, HAL_I2S_NO);
        }

        i2s_mode[id] = mode;
        i2s_dma[id][stream] = false;
    }

    i2s_status[id][stream] = HAL_I2S_STATUS_OPENED;

    return 0;
}

int hal_i2s_close(enum HAL_I2S_ID_T id, enum AUD_STREAM_T stream)
{
    uint32_t reg_base;

    if (i2s_status[id][stream] != HAL_I2S_STATUS_OPENED) {
        TRACE("Invalid I2S closing status for stream %d", stream, i2s_status[id][stream]);
        return 1;
    }

    i2s_status[id][stream] = HAL_I2S_STATUS_NULL;

    if (i2s_status[id][AUD_STREAM_PLAYBACK] == HAL_I2S_STATUS_NULL &&
            i2s_status[id][AUD_STREAM_CAPTURE] == HAL_I2S_STATUS_NULL) {
        reg_base = _i2s_get_reg_base(id);

        i2sip_w_enable_i2sip(reg_base, HAL_I2S_NO);
        hal_cmu_reset_set(HAL_CMU_MOD_P_I2S);
        hal_cmu_reset_set(HAL_CMU_MOD_O_I2S);
        hal_cmu_clock_disable(HAL_CMU_MOD_P_I2S);
        hal_cmu_clock_disable(HAL_CMU_MOD_O_I2S);
        hal_cmu_i2s_clock_disable();

#ifndef SIMU
        analog_aud_pll_close(ANA_AUD_PLL_USER_I2S);
#endif

        i2s_mode[id] = HAL_I2S_MODE_NULL;
    }

    return 0;
}

int hal_i2s_start_stream(enum HAL_I2S_ID_T id, enum AUD_STREAM_T stream)
{
    uint32_t reg_base;

    if (i2s_status[id][stream] != HAL_I2S_STATUS_OPENED) {
        TRACE("Invalid I2S starting status for stream %d", stream, i2s_status[id][stream]);
        return 1;
    }

    reg_base = _i2s_get_reg_base(id);

    if (stream == AUD_STREAM_PLAYBACK)
    {
        i2sip_w_enable_tx_block(reg_base, HAL_I2S_YES);
        i2sip_w_enable_tx_channel(reg_base, 0, HAL_I2S_YES);
        if (i2s_dma[id][stream])
        {
            i2sip_w_enable_tx_dma(reg_base, HAL_I2S_YES);
            if (i2s_mode[id] == HAL_I2S_MODE_MASTER) {
                // Delay a little time for DMA to fill the TX FIFO before sending (enabling clk gen)
                for (volatile int i = 0; i < 50; i++);
            }
        }
    }
    else
    {
        i2sip_w_enable_rx_block(reg_base, HAL_I2S_YES);
        i2sip_w_enable_rx_channel(reg_base, 0, HAL_I2S_YES);
        if (i2s_dma[id][stream])
        {
            i2sip_w_enable_rx_dma(reg_base, HAL_I2S_YES);
        }
    }

    if (i2s_status[id][AUD_STREAM_PLAYBACK] != HAL_I2S_STATUS_STARTED &&
            i2s_status[id][AUD_STREAM_CAPTURE] != HAL_I2S_STATUS_STARTED) {
        if (i2s_mode[id] == HAL_I2S_MODE_MASTER) {
            hal_cmu_i2s_set_master_mode();
            hal_cmu_i2s_clock_out_enable();
            i2sip_w_enable_clk_gen(reg_base, HAL_I2S_YES);
        } else {
            hal_cmu_i2s_set_slave_mode();
        }
#ifndef CHIP_BEST1000
        i2sip_w_enable_slave_mode(reg_base, i2s_mode[id] == HAL_I2S_MODE_MASTER ? HAL_I2S_NO : HAL_I2S_YES);
#endif
    }

    i2s_status[id][stream] = HAL_I2S_STATUS_STARTED;

    return 0;
}

int hal_i2s_stop_stream(enum HAL_I2S_ID_T id, enum AUD_STREAM_T stream)
{
    uint32_t reg_base;

    if (i2s_status[id][stream] != HAL_I2S_STATUS_STARTED) {
        TRACE("Invalid I2S stopping status for stream %d", stream, i2s_status[id][stream]);
        return 1;
    }

    i2s_status[id][stream] = HAL_I2S_STATUS_OPENED;

    reg_base = _i2s_get_reg_base(id);

    if (i2s_status[id][AUD_STREAM_PLAYBACK] != HAL_I2S_STATUS_STARTED &&
            i2s_status[id][AUD_STREAM_CAPTURE] != HAL_I2S_STATUS_STARTED) {
        if (i2s_mode[id] == HAL_I2S_MODE_MASTER) {
            hal_cmu_i2s_clock_out_disable();
            i2sip_w_enable_clk_gen(reg_base, HAL_I2S_NO);
        }
    }

    if (stream == AUD_STREAM_PLAYBACK)
    {
        i2sip_w_enable_tx_block(reg_base, HAL_I2S_NO);
        i2sip_w_enable_tx_channel(reg_base, 0, HAL_I2S_NO);
        i2sip_w_enable_tx_dma(reg_base, HAL_I2S_NO);
        i2sip_w_tx_fifo_reset(reg_base, 0);
    }
    else
    {
        i2sip_w_enable_rx_block(reg_base, HAL_I2S_NO);
        i2sip_w_enable_rx_channel(reg_base, 0, HAL_I2S_NO);
        i2sip_w_enable_rx_dma(reg_base, HAL_I2S_NO);
        i2sip_w_rx_fifo_reset(reg_base, 0);
    }

    return 0;
}

int hal_i2s_setup_stream(enum HAL_I2S_ID_T id, enum AUD_STREAM_T stream, const struct HAL_I2S_CONFIG_T *cfg)
{
    uint32_t reg_base;
    uint32_t div = 0;
    uint8_t resolution = 0, word_sclk = 0;

    if (i2s_status[id][stream] != HAL_I2S_STATUS_OPENED && i2s_status[id][stream] != HAL_I2S_STATUS_STARTED) {
        TRACE("Invalid I2S setup status for stream %d", stream, i2s_status[id][stream]);
        return 1;
    }

    reg_base = _i2s_get_reg_base(id);

    switch (cfg->bits)
    {
        case AUD_BITS_8:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_16CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_IGNORE;
            break;
        case AUD_BITS_12:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_16CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_12BIT;
            break;
        case AUD_BITS_16:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_16CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_16BIT;
            break;
        case AUD_BITS_20:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_16CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_20BIT;
            break;
        case AUD_BITS_24:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_24CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_24BIT;
            break;
        case AUD_BITS_32:
            word_sclk  = I2SIP_CLK_CFG_WSS_VAL_32CYCLE;
            resolution = I2SIP_CFG_WLEN_VAL_32BIT;
            break;
        default:
            return 1;
    }

    /* word clock width : how many sclk work sclk count*/
    /* sclk gate in word clock : how many sclk gate : fixed no gate */
    i2sip_w_clk_cfg_reg(reg_base, (word_sclk<<I2SIP_CLK_CFG_WSS_SHIFT) | (I2SIP_CLK_CFG_SCLK_GATE_VAL_NO_GATE<<I2SIP_CLK_CFG_SCLK_GATE_SHIFT));

    if (i2s_mode[id] == HAL_I2S_MODE_MASTER) {
#ifdef FPGA
        uint32_t sclk;

        sclk = cfg->sample_rate * cfg->bits * cfg->channel_num;

#define I2S_CLOCK_SOURCE 22579200	//44100*512
        div  = I2S_CLOCK_SOURCE/sclk - 1;
#undef I2S_CLOCK_SOURCE

        TRACE("div = %x",div);
#else
        uint8_t i;
        uint32_t i2s_clock;
        uint32_t bit_rate;

        for (i = 0; i < ARRAY_SIZE(i2s_sample_rate); i++)
        {
            if (i2s_sample_rate[i].sample_rate == cfg->sample_rate)
            {
                break;
            }
        }
        ASSERT(i < ARRAY_SIZE(i2s_sample_rate), "%s: Invalid i2s sample rate: %d", __func__, cfg->sample_rate);

        TRACE("[%s] stream=%d sample_rate=%d", __func__, stream, cfg->sample_rate);

        ASSERT(i2s_sample_rate[i].codec_div / i2s_sample_rate[i].cmu_div * i2s_sample_rate[i].cmu_div == i2s_sample_rate[i].codec_div,
            "%s: Invalid codec div for rate %u: codec_div=%u cmu_div=%u", __func__, cfg->sample_rate,
            i2s_sample_rate[i].codec_div, i2s_sample_rate[i].cmu_div);

#ifndef SIMU
        analog_aud_freq_pll_config(i2s_sample_rate[i].codec_freq, i2s_sample_rate[i].codec_div);
#ifndef CHIP_BEST1000
        analog_aud_pll_set_dig_div(i2s_sample_rate[i].codec_div / i2s_sample_rate[i].cmu_div);
#endif
#endif

        ASSERT(cfg->channel_num == AUD_CHANNEL_NUM_2, "[%s] Channel number(%d) error", __func__, cfg->channel_num);

        i2s_clock = i2s_sample_rate[i].codec_freq * i2s_sample_rate[i].cmu_div;
        bit_rate = i2s_sample_rate[i].sample_rate * cfg->channel_num * cfg->bits;
        div = i2s_clock / bit_rate;

        ASSERT(i2s_clock == bit_rate * div, "%s: Bad stream cfg (bad bits?): i2s_clock=%u bit_rate=%u bits=%u",
            __func__, i2s_clock, bit_rate, cfg->bits);
#endif

        hal_cmu_i2s_set_div(div);
    }

	hal_cmu_reset_pulse(HAL_CMU_MOD_O_I2S);

    i2s_dma[id][stream] = cfg->use_dma;

    if (stream == AUD_STREAM_PLAYBACK)
    {
        /* resolution : valid bit width */
        i2sip_w_tx_resolution(reg_base, 0, resolution);

        /* fifo level to trigger empty interrupt or dma req */
        i2sip_w_tx_fifo_threshold(reg_base, 0, HAL_I2S_TX_FIFO_TRIGGER_LEVEL);
    }
    else
    {
        /* resolution : valid bit width */
        i2sip_w_rx_resolution(reg_base, 0, resolution);

        /* fifo level to trigger empty interrupt or dma req */
        i2sip_w_rx_fifo_threshold(reg_base, 0, HAL_I2S_RX_FIFO_TRIGGER_LEVEL);
    }

    return 0;
}

int hal_i2s_send(enum HAL_I2S_ID_T id, const uint8_t *value, uint32_t value_len)
{
    uint32_t i = 0;
    uint32_t reg_base;

    reg_base = _i2s_get_reg_base(id);

    for (i = 0; i < value_len; i += 4) {
        while (!(i2sip_r_int_status(reg_base, 0) & I2SIP_INT_STATUS_TX_FIFO_EMPTY_MASK));

        i2sip_w_tx_left_fifo(reg_base, 0, value[i+1]<<8 | value[i]);
        i2sip_w_tx_right_fifo(reg_base, 0, value[i+3]<<8 | value[i+2]);
    }

    return 0;
}
uint8_t hal_i2s_recv(enum HAL_I2S_ID_T id, uint8_t *value, uint32_t value_len)
{
    //uint32_t reg_base;

    //reg_base = _i2s_get_reg_base(id);

    return 0;
}

//================================================================================
// I2S Packet Mode
//================================================================================

static bool i2s_pkt_opened = false;

int hal_i2s_packet_open(void)
{
    uint32_t reg_base;
    uint8_t resolution = 0, word_sclk = 0;

    if (i2s_pkt_opened) {
        return 1;
    }

    word_sclk  = I2SIP_CLK_CFG_WSS_VAL_32CYCLE;
    resolution = I2SIP_CFG_WLEN_VAL_32BIT;

    reg_base = _i2s_get_reg_base(HAL_I2S_ID_0);

    hal_iomux_set_i2s();
    hal_cmu_i2s_clock_enable();
    hal_cmu_clock_enable(HAL_CMU_MOD_O_I2S);
    hal_cmu_clock_enable(HAL_CMU_MOD_P_I2S);
    hal_cmu_reset_clear(HAL_CMU_MOD_O_I2S);
    hal_cmu_reset_clear(HAL_CMU_MOD_P_I2S);
    i2sip_w_enable_i2sip(reg_base, HAL_I2S_YES);

    for (int i = 0; i < I2S_CHAN_NUM; i++) {
        i2sip_w_enable_rx_channel(reg_base, i, HAL_I2S_NO);
        i2sip_w_enable_tx_channel(reg_base, i, HAL_I2S_NO);
    }

    hal_cmu_i2s_set_slave_mode();
#ifndef CHIP_BEST1000
    i2sip_w_enable_slave_mode(reg_base, HAL_I2S_YES);
#endif

    /* word clock width : how many sclk work sclk count*/
    /* sclk gate in word clock : how many sclk gate : fixed no gate */
    i2sip_w_clk_cfg_reg(reg_base, (word_sclk<<I2SIP_CLK_CFG_WSS_SHIFT) | (I2SIP_CLK_CFG_SCLK_GATE_VAL_NO_GATE<<I2SIP_CLK_CFG_SCLK_GATE_SHIFT));

    i2sip_w_tx_resolution(reg_base, 0, resolution);
    i2sip_w_tx_fifo_threshold(reg_base, 0, HAL_I2S_TX_FIFO_TRIGGER_LEVEL);
    i2sip_w_rx_resolution(reg_base, 0, resolution);
    i2sip_w_rx_fifo_threshold(reg_base, 0, HAL_I2S_RX_FIFO_TRIGGER_LEVEL);

    i2s_pkt_opened = true;

    return 0;
}

int hal_i2s_packet_close(void)
{
    uint32_t reg_base;

    if (!i2s_pkt_opened) {
        return 1;
    }

    reg_base = _i2s_get_reg_base(HAL_I2S_ID_0);

    i2sip_w_enable_i2sip(reg_base, HAL_I2S_NO);
    hal_cmu_reset_set(HAL_CMU_MOD_P_I2S);
    hal_cmu_reset_set(HAL_CMU_MOD_O_I2S);
    hal_cmu_clock_disable(HAL_CMU_MOD_P_I2S);
    hal_cmu_clock_disable(HAL_CMU_MOD_O_I2S);
    hal_cmu_i2s_clock_disable();

    i2s_pkt_opened = false;

    return 0;
}

int hal_i2s_start_transfer(void)
{
    uint32_t reg_base;

    if (!i2s_pkt_opened) {
        return 1;
    }

	hal_cmu_reset_pulse(HAL_CMU_MOD_O_I2S);

    reg_base = _i2s_get_reg_base(HAL_I2S_ID_0);

    i2sip_w_enable_tx_block(reg_base, HAL_I2S_YES);
    i2sip_w_enable_tx_channel(reg_base, 0, HAL_I2S_YES);
    i2sip_w_enable_tx_dma(reg_base, HAL_I2S_YES);

    i2sip_w_enable_rx_block(reg_base, HAL_I2S_YES);
    i2sip_w_enable_rx_channel(reg_base, 0, HAL_I2S_YES);
    i2sip_w_enable_rx_dma(reg_base, HAL_I2S_YES);

    return 0;
}

int hal_i2s_stop_transfer(void)
{
    uint32_t reg_base;

    if (!i2s_pkt_opened) {
        return 1;
    }

    reg_base = _i2s_get_reg_base(HAL_I2S_ID_0);

    i2sip_w_enable_tx_block(reg_base, HAL_I2S_NO);
    i2sip_w_enable_tx_channel(reg_base, 0, HAL_I2S_NO);
    i2sip_w_enable_tx_dma(reg_base, HAL_I2S_NO);
    i2sip_w_tx_fifo_reset(reg_base, 0);

    i2sip_w_enable_rx_block(reg_base, HAL_I2S_NO);
    i2sip_w_enable_rx_channel(reg_base, 0, HAL_I2S_NO);
    i2sip_w_enable_rx_dma(reg_base, HAL_I2S_NO);
    i2sip_w_rx_fifo_reset(reg_base, 0);

    return 0;
}

