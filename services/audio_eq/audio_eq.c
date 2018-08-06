/*******************************************************************************
** namer：	Audio_eq
** description:	fir and iir eq manager
** version：V0.9
** author： Yunjie Huo
** modify：	2016.12.26
** todo: 	1.
*******************************************************************************/
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "string.h"
#include "audio_eq.h"
#include "stdbool.h"
#include "hal_location.h"
#include "hw_codec_iir_process.h"

#ifdef __PC_CMD_UART__
#include "hal_cmd.h"

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
#ifndef __TOUCH_KEY__     //@20180228 by parker.wei  when uart1 for touch key ,the EQ DYNAMIC closed
#define AUDIO_EQ_IIR_DYNAMIC    1
#endif
#endif

#ifdef __HW_FIR_EQ_PROCESS__
#ifndef __TOUCH_KEY__     //@20180228 by parker.wei  when uart1 for touch key ,the EQ DYNAMIC closed
#define AUDIO_EQ_FIR_DYNAMIC    1
#endif
#endif

#endif

#if defined(AUDIO_EQ_IIR_DYNAMIC) || defined(AUDIO_EQ_FIR_DYNAMIC)
#define AUDIO_EQ_DYNAMIC        1
#endif

typedef signed int          audio_eq_sample_24bits_t;
typedef signed short int    audio_eq_sample_16bits_t;

typedef struct{
    enum AUD_BITS_T     sample_bits;
    enum AUD_SAMPRATE_T sample_rate;
    enum AUD_CHANNEL_NUM_T ch_num; 

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
    bool iir_enable;
#endif

#ifdef __HW_FIR_EQ_PROCESS__
    bool fir_enable;
#endif

#ifdef AUDIO_EQ_DYNAMIC
    bool update_cfg;
#endif

#ifdef AUDIO_EQ_IIR_DYNAMIC
    IIR_CFG_T iir_cfg;
#endif

#ifdef AUDIO_EQ_FIR_DYNAMIC
    FIR_CFG_T fir_cfg;
#endif
} AUDIO_EQ_T;

extern const IIR_CFG_T audio_eq_iir_cfg;
extern const FIR_CFG_T audio_eq_fir_cfg;

static AUDIO_EQ_T audio_eq = {
    .sample_bits = AUD_BITS_NULL,
    .sample_rate = AUD_SAMPRATE_NULL,
    .ch_num = AUD_CHANNEL_NUM_NULL,
#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)    
    .iir_enable =  false,
#endif
#ifdef AUDIO_EQ_DYNAMIC
    .update_cfg = false,
#endif
#ifdef AUDIO_EQ_IIR_DYNAMIC
    .iir_cfg = {.num = 0},
#endif
};

#ifdef AUDIO_EQ_DYNAMIC
static int audio_eq_update_cfg_enable(bool en)
{
    audio_eq.update_cfg = en;

    return 0;
}

static int audio_eq_update_cfg(void)
{
#ifdef AUDIO_EQ_IIR_DYNAMIC
    iir_set_cfg(&audio_eq.iir_cfg);
#endif

#ifdef AUDIO_EQ_FIR_DYNAMIC
    fir_set_cfg(&audio_eq.fir_cfg);
#endif

    audio_eq_update_cfg_enable(false);

    return 0;
}
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
int audio_eq_iir_set_cfg(const IIR_CFG_T *cfg)
{
    if(cfg)
    {
        iir_set_cfg(cfg);
        audio_eq.iir_enable = true;
    }
    else
    {
        audio_eq.iir_enable = false;
    }

    return 0;
}
#endif

#ifdef __HW_FIR_EQ_PROCESS__
int audio_eq_fir_set_cfg(const FIR_CFG_T *cfg)
{
    if(cfg)
    {
#ifdef AUDIO_EQ_FIR_DYNAMIC
        memcpy(&audio_eq.fir_cfg, cfg, sizeof(audio_eq.fir_cfg));
        audio_eq_update_cfg_enable(true);
#else
        fir_set_cfg(cfg);
#endif
        audio_eq.fir_enable = true;
    }
    else
    {
        audio_eq.fir_enable = false;
    }

    return 0;
}
#endif

int SRAM_TEXT_LOC audio_eq_run(uint8_t *buf, uint32_t len)
{
    int eq_len = 0;

    if(audio_eq.sample_bits == AUD_BITS_16)
    {
        eq_len = len/sizeof(audio_eq_sample_16bits_t);
    }
    else if(audio_eq.sample_bits == AUD_BITS_24)
    {
        eq_len = len/sizeof(audio_eq_sample_24bits_t);
    }
    else
    {
        ASSERT(0, "[%s] bits(%d) is invalid", __func__, audio_eq.sample_bits);
    }


#if defined( __PC_CMD_UART__)&&defined (AUDIO_EQ_DYNAMIC)//@20180228 by parker.wei  when uart1 for touch key ,the EQ DYNAMIC closed
    hal_cmd_run();
#endif



//    int32_t s_time,e_time;
//    s_time = hal_sys_timer_get();

#ifdef __SW_IIR_EQ_PROCESS__
    if(audio_eq.iir_enable)
    {
        iir_run(buf, eq_len);
    }
#endif

#ifdef __HW_FIR_EQ_PROCESS__
    if(audio_eq.fir_enable)
    {
        fir_run(buf, eq_len);
    }
#endif

    if(audio_eq.sample_bits == AUD_BITS_24)
    {
        audio_eq_sample_24bits_t *eq_buf = (audio_eq_sample_24bits_t *)buf;
        for (int i = 0; i < eq_len; i++)
        {
            eq_buf[i] = eq_buf[i] >> (24 - CODEC_PLAYBACK_BIT_DEPTH);
        }
    }

//    e_time = hal_sys_timer_get();
//    TRACE("[%s] Sample len = %d, used time = %d ticks", __func__, eq_len, (e_time - s_time));
    return 0;
}

int audio_eq_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_BITS_T sample_bits, enum AUD_CHANNEL_NUM_T ch_num,void *eq_buf, uint32_t len)
{
    TRACE("[%s] sample_rate = %d, sample_bits = %d", __func__, sample_rate, sample_bits);

    audio_eq.sample_rate = sample_rate;
    audio_eq.sample_bits = sample_bits;
    audio_eq.ch_num = ch_num;

#ifdef __SW_IIR_EQ_PROCESS__
    iir_open(sample_rate, sample_bits);
#ifdef AUDIO_EQ_IIR_DYNAMIC
    if (audio_eq.iir_cfg.num)
    {
        audio_eq_iir_set_cfg((IIR_CFG_T *)&(audio_eq.iir_cfg));
    }
    else
#endif
    {
        audio_eq_iir_set_cfg((IIR_CFG_T *)&audio_eq_iir_cfg);
    }
    audio_eq.iir_enable = false;
#endif

#ifdef __HW_FIR_EQ_PROCESS__
#if defined(CHIP_BEST1000) && defined(FIR_HIGHSPEED)
    hal_cmu_fir_high_speed_enable(HAL_CMU_FIR_USER_EQ);
#endif
    fir_open(sample_rate, sample_bits,ch_num, eq_buf, len);
    audio_eq_fir_set_cfg((FIR_CFG_T *)&audio_eq_fir_cfg);
    audio_eq.fir_enable = false;
#endif

#ifdef __HW_CODEC_IIR_EQ_PROCESS__
    {
        HW_CODEC_IIR_CFG_T *hw_iir_cfg=NULL;
        
#ifdef __AUDIO_RESAMPLE__
        enum AUD_SAMPRATE_T sample_rate_hw_iir=AUD_SAMPRATE_50781;
#else
        enum AUD_SAMPRATE_T sample_rate_hw_iir=sample_rate;
#endif

        hw_codec_iir_open(sample_rate_hw_iir,HW_CODEC_IIR_DAC);
 #ifdef AUDIO_EQ_IIR_DYNAMIC
    if (audio_eq.iir_cfg.num)
    {
        hw_iir_cfg = hw_codec_iir_get_cfg(sample_rate_hw_iir, HW_CODEC_IIR_DAC, 0, &(audio_eq.iir_cfg));        
    }
    else
#endif
    {
        hw_iir_cfg = hw_codec_iir_get_cfg(sample_rate_hw_iir, HW_CODEC_IIR_DAC, 0, NULL);        
    }
        ASSERT(hw_iir_cfg != NULL, "[%s] %d codec IIR parameter error!", __func__, hw_iir_cfg);         
        hw_codec_iir_set_cfg(hw_iir_cfg, sample_rate_hw_iir, HW_CODEC_IIR_DAC);            
    }
#endif

#if defined( __PC_CMD_UART__)&&defined (AUDIO_EQ_DYNAMIC)//@20180228 by parker.wei  when uart1 for touch key ,the EQ DYNAMIC closed
    hal_cmd_open();
#endif

	return 0;
}

int audio_eq_close(void)
{
#ifdef __SW_IIR_EQ_PROCESS__
    audio_eq.iir_enable = false;
    iir_close();
#endif

#ifdef __HW_FIR_EQ_PROCESS__
    audio_eq.fir_enable = false;
    fir_close();
#if defined(CHIP_BEST1000) && defined(FIR_HIGHSPEED)
    hal_cmu_fir_high_speed_disable(HAL_CMU_FIR_USER_EQ);
#endif
#endif

#ifdef __HW_CODEC_IIR_EQ_PROCESS__
    hw_codec_iir_close(HW_CODEC_IIR_DAC);
#endif



#if defined( __PC_CMD_UART__)&&defined (AUDIO_EQ_DYNAMIC)//@20180228 by parker.wei  when uart1 for touch key ,the EQ DYNAMIC closed
    hal_cmd_close();

#endif

    return 0;
}

#ifdef __PC_CMD_UART__
#ifdef AUDIO_EQ_IIR_DYNAMIC

#ifndef USB_AUDIO_TEST
int audio_eq_iir_callback(uint8_t *buf, uint32_t  len)
{    
    memcpy(&audio_eq.iir_cfg, buf, sizeof(IIR_CFG_T));
    TRACE("band num:%d gain0:%d, gain1:%d", 
                                                (int32_t)audio_eq.iir_cfg.num,
                                                (int32_t)(audio_eq.iir_cfg.gain0*10),      
                                                (int32_t)(audio_eq.iir_cfg.gain1*10));
    for (uint8_t i = 0; i<audio_eq.iir_cfg.num; i++){
        TRACE("band num:%d type %d gain:%d fc:%d q:%d", i,
			                           (int32_t)(audio_eq.iir_cfg.param[i].type), 
                                                (int32_t)(audio_eq.iir_cfg.param[i].gain*10), 
                                                (int32_t)(audio_eq.iir_cfg.param[i].fc*10), 
                                                (int32_t)(audio_eq.iir_cfg.param[i].Q*10));
    }

#ifdef __SW_IIR_EQ_PROCESS__
    {
        audio_eq_iir_set_cfg(&audio_eq.iir_cfg);
    }
#endif

#ifdef __HW_CODEC_IIR_EQ_PROCESS__
    {
        HW_CODEC_IIR_CFG_T *hw_iir_cfg=NULL;
        
#ifdef __AUDIO_RESAMPLE__
        enum AUD_SAMPRATE_T sample_rate_hw_iir=AUD_SAMPRATE_50781;
#else
        enum AUD_SAMPRATE_T sample_rate_hw_iir=sample_rate;
#endif

        hw_codec_iir_open(sample_rate_hw_iir,HW_CODEC_IIR_DAC);
        
        hw_iir_cfg = hw_codec_iir_get_cfg(sample_rate_hw_iir, HW_CODEC_IIR_DAC, 0, &audio_eq.iir_cfg);        
        ASSERT(hw_iir_cfg != NULL, "[%s] %d codec IIR parameter error!", __func__, hw_iir_cfg);         
        hw_codec_iir_set_cfg(hw_iir_cfg, sample_rate_hw_iir, HW_CODEC_IIR_DAC);            
    }
#endif
    
    return 0;
}
#else
int audio_eq_iir_callback(uint8_t *buf, uint32_t  len)
{
    // IIR_CFG_T *rx_iir_cfg = NULL;

    // rx_iir_cfg = (IIR_CFG_T *)buf;

    // TRACE("[%s] left gain = %f, right gain = %f", __func__, rx_iir_cfg->gain0, rx_iir_cfg->gain1);

    // for(int i=0; i<rx_iir_cfg->num; i++)
    // {
    //     TRACE("[%s] iir[%d] gain = %f, f = %f, Q = %f", __func__, i, rx_iir_cfg->param[i].gain, rx_iir_cfg->param[i].fc, rx_iir_cfg->param[i].Q);
    // }

    // audio_eq_iir_set_cfg((const IIR_CFG_T *)rx_iir_cfg);

    update_iir_cfg_tbl(buf, len);

    return 0;
}
#endif
#endif

#ifdef __PC_CMD_UART__
int audio_tool_ping_callback(uint8_t *buf, uint32_t len)
{
    //TRACE("");
    return 0;
}
#endif

#ifdef AUDIO_EQ_FIR_DYNAMIC
int audio_eq_fir_callback(uint8_t *buf, uint32_t  len)
{
    FIR_CFG_T *rx_fir_cfg = NULL;

    rx_fir_cfg = (FIR_CFG_T *)buf;

    TRACE("[%s] left gain = %d, right gain = %d", __func__, rx_fir_cfg->gain0, rx_fir_cfg->gain1);

    TRACE("[%s] len = %d, coef: %d, %d......%d, %d", __func__, rx_fir_cfg->len, rx_fir_cfg->coef[0], rx_fir_cfg->coef[1], rx_fir_cfg->coef[rx_fir_cfg->len-2], rx_fir_cfg->coef[rx_fir_cfg->len-1]);

    rx_fir_cfg->gain0 = 6;
    rx_fir_cfg->gain1 = 6;
    audio_eq_fir_set_cfg((const FIR_CFG_T *)rx_fir_cfg);

    return 0;
}
#endif
#endif

typedef struct {
    uint8_t type;
    uint8_t maxEqBandNum;
    uint16_t sample_rate_num;
    uint8_t sample_rate[50];
} query_eq_info_t;


extern int getMaxEqBand(void);
extern int getSampleArray(uint8_t* buf, uint16_t *num);

extern void hal_cmd_set_res_playload(uint8_t* data, int len);
#define CMD_TYPE_QUERY_DUT_EQ_INFO  0x00
int audio_cmd_callback(uint8_t *buf, uint32_t len)
{
    uint8_t type;
    //uint32_t* sample_rate;
    //uint8_t* p;
    query_eq_info_t  info;

    type = buf[0];
    //p = buf + 1;

    TRACE("%s type: %d", __func__, type);
    switch (type) {
        case CMD_TYPE_QUERY_DUT_EQ_INFO:
            info.type = CMD_TYPE_QUERY_DUT_EQ_INFO;
            info.maxEqBandNum = getMaxEqBand();
            getSampleArray(info.sample_rate, &info.sample_rate_num);

            hal_cmd_set_res_playload((uint8_t*)&info, 4 + info.sample_rate_num * 4);
            break;
        default:
            break;
    }

    return 0;
}

int audio_eq_init(void)
{
#ifdef __SW_IIR_EQ_PROCESS__
    iir_open(AUD_SAMPRATE_96000, AUD_BITS_16);
#endif

#ifdef __PC_CMD_UART__
    hal_cmd_init();
#ifdef AUDIO_EQ_IIR_DYNAMIC
    hal_cmd_register("iir_eq", audio_eq_iir_callback);
#endif

#ifdef AUDIO_EQ_FIR_DYNAMIC
    hal_cmd_register("fir_eq", audio_eq_fir_callback);
#endif
    hal_cmd_register("cmd", audio_cmd_callback);

    hal_cmd_register("ping", audio_tool_ping_callback);
#endif

    return 0;
}
