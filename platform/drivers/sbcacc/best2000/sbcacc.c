#include "plat_addr_map.h"
#include "reg_hwfir_best2000.h"
#include "hal_hwfir.h"
#include "hal_codec.h"
#include "hal_cmu.h"
#include "hal_psc.h"
#include "hal_aud.h"
#include "hal_trace.h"
#include "hal_dma.h"
#include "analog.h"
#include "tgt_hardware.h"
#include "sbcacc.h"
#include "string.h"

#define SYN1_SAMP_NUM (128)
#define SYN2_SAMP_NUM (128)
#define SYN1_DMA_LINKLIST_NUM_MAX (10)
#define SYN2_DMA_LINKLIST_NUM_MAX (10)

struct HAL_DMA_DESC_T syn1_dma_in_desc[SYN1_DMA_LINKLIST_NUM_MAX];
struct HAL_DMA_DESC_T syn1_dma_ou_desc[SYN1_DMA_LINKLIST_NUM_MAX];
struct HAL_DMA_DESC_T syn2_dma_in_desc[SYN2_DMA_LINKLIST_NUM_MAX];
struct HAL_DMA_DESC_T syn2_dma_ou_desc[SYN2_DMA_LINKLIST_NUM_MAX];
struct HAL_DMA_CH_CFG_T syn1_dma_cfg;
struct HAL_DMA_CH_CFG_T syn2_dma_cfg;

int Synthesis1_channel_num = 0, Synthesis2_channel_num = 0;
int Synthesis2_sample_start = 0;

HAL_HWFIR_CHANNEL_ID ch[4];

int Synthesis_init(const char *coef1, int coef_len1, const char *coef2, int coef_len2, int channel_num)
{
    ch[0] = hal_hwfir_acquire_id(HWFIR_CHANNEL_ID_0);
    ch[1] = hal_hwfir_acquire_id(HWFIR_CHANNEL_ID_1);
    ch[2] = hal_hwfir_acquire_id(HWFIR_CHANNEL_ID_2);
    ch[3] = hal_hwfir_acquire_id(HWFIR_CHANNEL_ID_3);

    hwfir_ch_set_256samp_to_512samp(ch[0], 1);
    hwfir_ch_set_256samp_to_512samp(ch[2], 1);

    Synthesis1_init(coef1, coef_len1, channel_num);
    Synthesis2_init(coef2, coef_len2, channel_num);

    return 0;
}

int Synthesis1_init(const char *coef, int coef_len, int channel_num)
{
    unsigned int coef_addr = 0;
    
    hwfir_ch_set_fir_sample_num(ch[0], SYN1_SAMP_NUM*2-1);

    hwfir_ch_set_fir_sample_start(ch[0], 0);

    hwfir_ch_set_fir_mode(ch[0], 1);
    hwfir_ch_set_stream0_fir1(ch[0], 1);
    hwfir_ch_set_do_remap(ch[0], 0);
    hwfir_ch_set_gain_sel(ch[0], 6);

    hwfir_ch_set_fir_order(ch[0], 8);
    hwfir_ch_set_burst_length(ch[0], 16-1);
    hwfir_ch_set_slide_offset(ch[0], 8);
    hwfir_ch_set_result_base_addr(ch[0], 128);

    hwfir_ch_set_access_offset(ch[0], 0);

    coef_addr = hal_hwfir_get_coef_memory_addr(ch[0]);
    memcpy((char *)coef_addr, coef, coef_len);

    Synthesis1_channel_num = channel_num;

    return 0;
}

int Synthesis2_init(const char *coef, int coef_len, int channel_num)
{
    unsigned int coef_addr = 0;

    hwfir_ch_set_fir_sample_num(ch[2], SYN2_SAMP_NUM-1);

    hwfir_ch_set_fir_mode(ch[2], 2);
    hwfir_ch_set_stream0_fir1(ch[2], 1);
    hwfir_ch_set_do_remap(ch[2], 1);
    hwfir_ch_set_gain_sel(ch[2], 6);

    hwfir_ch_set_fir_order(ch[2], 10);
    hwfir_ch_set_burst_length(ch[2], 8-1);
    hwfir_ch_set_slide_offset(ch[2], 16);
    hwfir_ch_set_result_base_addr(ch[2], 80);

    hwfir_ch_set_access_offset(ch[2], 3);

    coef_addr = hal_hwfir_get_coef_memory_addr(ch[2]);
    memcpy((char *)coef_addr, (char *)coef, coef_len);

    Synthesis2_sample_start = 0;

    Synthesis2_channel_num = channel_num;
    
    return 0;
}

volatile int Synthesis1_lock = 0;
static void Synthesis1_dout_handler(uint32_t remains, uint32_t error, struct HAL_DMA_DESC_T *lli)
{
    TRACE("%s", __func__);
    Synthesis1_lock = 0;
    return;
}

int Synthesis1_process(char *in, int in_len, char *out, int out_len)
{
    unsigned int samp_addr = 0;
    int link_num = 0, loop = 0, i = 0;
    struct HAL_DMA_CH_CFG_T *dma_cfg = &syn1_dma_cfg;

    dma_cfg->ch = hal_audma_get_chan(HAL_AUDMA_DPD_TX, HAL_DMA_HIGH_PRIO);
    dma_cfg->dst = 0; // useless
    dma_cfg->dst_bsize = HAL_DMA_BSIZE_4;
    dma_cfg->dst_periph = HAL_AUDMA_DPD_TX;
    dma_cfg->dst_width = HAL_DMA_WIDTH_WORD;
    dma_cfg->handler = 0; //NULL
    dma_cfg->src_bsize = HAL_DMA_BSIZE_16;
    dma_cfg->src_periph = 0; // useless
    dma_cfg->src_tsize = SYN1_SAMP_NUM;
    dma_cfg->src_width = HAL_DMA_WIDTH_WORD;
    dma_cfg->try_burst = 1;
    dma_cfg->type = HAL_DMA_FLOW_M2P_DI_DMA;

    link_num = 0;
    loop = in_len/SYN1_SAMP_NUM;

    samp_addr = hal_hwfir_get_data_memory_addr(ch[0]);

    for (i = 0; i < loop; i++) {
        dma_cfg->src = (uint32_t)&in[i*SYN1_SAMP_NUM*4*Synthesis1_channel_num];

        if (link_num != (loop - 1))
            hal_audma_init_desc(&syn1_dma_in_desc[link_num], dma_cfg, &syn1_dma_in_desc[link_num + 1], 0);
        else
            hal_audma_init_desc(&syn1_dma_in_desc[link_num], dma_cfg, 0, 0); //int disable

        syn1_dma_in_desc[i].dst = samp_addr;
        link_num++;
    }

    memset(dma_cfg, 0, sizeof(*dma_cfg));
    dma_cfg->ch = hal_audma_get_chan(HAL_AUDMA_DPD_RX, HAL_DMA_HIGH_PRIO);
    dma_cfg->dst_bsize = HAL_DMA_BSIZE_16;
    dma_cfg->dst_periph = 0; //useless
    dma_cfg->dst_width = HAL_DMA_WIDTH_WORD;
    dma_cfg->handler = Synthesis1_dout_handler;
    dma_cfg->src = 0; // useless
    dma_cfg->src_bsize = HAL_DMA_BSIZE_4;
    dma_cfg->src_periph = HAL_AUDMA_DPD_RX;
    dma_cfg->src_tsize = SYN1_SAMP_NUM;
    dma_cfg->src_width = HAL_DMA_WIDTH_WORD;
    dma_cfg->try_burst = 1;
    dma_cfg->type = HAL_DMA_FLOW_P2M_SI_DMA;

    link_num = 0;
    for (i = 0; i < loop; i++) {
        dma_cfg->dst = (uint32_t)&out[i*SYN2_SAMP_NUM*4*Synthesis2_channel_num*2];

        if (link_num != (loop - 1))
            hal_audma_init_desc(&syn1_dma_ou_desc[link_num], dma_cfg, &syn1_dma_ou_desc[link_num + 1], 0);
        else
            hal_audma_init_desc(&syn1_dma_ou_desc[link_num], dma_cfg, 0, 1);

        syn1_dma_ou_desc[i].src = (FFT_BASE + 0xB000 + 128*4);
        link_num++;
    }

    Synthesis1_lock = 1;
    while (Synthesis1_lock);
    TRACE("%s done!", __func__);

    return 0;
}

int Synthesis2_process(char *in, int in_len, char *out, int out_len)
{
    struct HAL_DMA_CH_CFG_T *dma_cfg = &syn2_dma_cfg;

    hwfir_ch_set_fir_sample_start(ch[2], Synthesis2_sample_start);

    return 0;
}

