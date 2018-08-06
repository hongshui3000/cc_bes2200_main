#include "plat_types.h"
#include "aud_section.h"
#include "aud_section_inc.h"
#include "tgt_hardware.h"
#include "hal_trace.h"
#include "anc_process.h"

#ifdef ANC_COEF_LIST_NUM
#if (ANC_COEF_LIST_NUM < 1)
#error "Invalid ANC_COEF_LIST_NUM configuration"
#endif
#else
#define ANC_COEF_LIST_NUM                   (1)
#endif

extern const struct_anc_cfg * anc_coef_list_50p7k[ANC_COEF_LIST_NUM];
extern const struct_anc_cfg * anc_coef_list_48k[ANC_COEF_LIST_NUM];
extern const struct_anc_cfg * anc_coef_list_44p1k[ANC_COEF_LIST_NUM];

const struct_anc_cfg * WEAK anc_coef_list_50p7k[ANC_COEF_LIST_NUM] = { };
const struct_anc_cfg * WEAK anc_coef_list_48k[ANC_COEF_LIST_NUM] = { };
const struct_anc_cfg * WEAK anc_coef_list_44p1k[ANC_COEF_LIST_NUM] = { };

static enum ANC_INDEX cur_coef_idx;
static bool cur_coef_44p1k;

int anc_load_cfg(void)
{
    int res = 0;
    const struct_anc_cfg **list;

#ifdef __AUDIO_RESAMPLE__
    res = anccfg_loadfrom_audsec(anc_coef_list_50p7k, anc_coef_list_44p1k, ANC_COEF_LIST_NUM);
    list=anc_coef_list_50p7k;
#else
    res = anccfg_loadfrom_audsec(anc_coef_list_48k, anc_coef_list_44p1k, ANC_COEF_LIST_NUM);
    list=anc_coef_list_48k;
#endif
    
    if(res)
    {
        TRACE("[%s] WARNING(%d): Can not load anc coefficient from audio section!!!", __func__, res);
    }
    else
    {
        TRACE("[%s] Load anc coefficient from audio section.", __func__);
#ifdef CHIP_BEST2000
        TRACE("best2000 ANC load cfg data:");
        TRACE("appmode1,FEEDFORWARD,L:gain %d,R:gain %d",list[0]->anc_cfg[0].total_gain,list[0]->anc_cfg[1].total_gain);
        TRACE("appmode1,FEEDBACK,L:gain %d,R:gain %d",list[0]->anc_cfg[2].total_gain,list[0]->anc_cfg[3].total_gain);
        TRACE("appmode1,FEEDFORWARD,L,iir coef:0x%x, 0x%x, 0x%x...", list[0]->anc_cfg[0].iir_coef[0].coef_b[0],\
                                                                    list[0]->anc_cfg[0].iir_coef[0].coef_b[1],\
                                                                    list[0]->anc_cfg[0].iir_coef[0].coef_b[2]);
#elif defined(CHIP_BEST1000)
        TRACE("[%s] L: gain = %d, len = %d, dac = %d, adc = %d", __func__, list[0]->anc_cfg[0].total_gain, list[0]->anc_cfg[0].fir_len, list[0]->anc_cfg[0].dac_gain_offset, list[0]->anc_cfg[0].adc_gain_offset);
        TRACE("[%s] R: gain = %d, len = %d, dac = %d, adc = %d", __func__, list[0]->anc_cfg[1].total_gain, list[0]->anc_cfg[1].fir_len, list[0]->anc_cfg[1].dac_gain_offset, list[0]->anc_cfg[1].adc_gain_offset);
#endif
    }

    return res;
}

const struct_anc_cfg * anc_get_default_coef(void)
{
#ifdef __AUDIO_RESAMPLE__
    return anc_coef_list_50p7k[0];
#else
    return anc_coef_list_48k[0];
#endif


}

int anc_select_coef(enum ANC_INDEX index,enum ANC_TYPE_T anc_type,ANC_GAIN_TIME anc_gain_delay)
{
    int ret;
    const struct_anc_cfg **list, **other_list;

    if (index >= ANC_COEF_LIST_NUM) {
        return 1;
    }

    if (cur_coef_44p1k) {
        list = anc_coef_list_44p1k;
        other_list = anc_coef_list_48k;
    } else {
#ifdef __AUDIO_RESAMPLE__
        list = anc_coef_list_50p7k;
        other_list = anc_coef_list_44p1k;
#else
        list = anc_coef_list_48k;
        other_list = anc_coef_list_44p1k;
#endif
        
    }

    if (list[index] == NULL) {
        TRACE("%s: No coef for index=%d", __func__, index);
        if (other_list[index] == NULL) {
            return 2;
        }
        TRACE("%s: Using other coef type: %s", __func__, cur_coef_44p1k ? "48K" : "44.1K");
        list = other_list;
        ret = -1;
    } else {
        ret = 0;
    }

    cur_coef_idx = index;

    anc_set_cfg(list[index],anc_type,anc_gain_delay);

    return ret;
}

int anc_set_coef_type(bool coef_44p1k)
{
#ifndef CHIP_BEST1000
    if (cur_coef_44p1k == coef_44p1k) {
        return 0;
    }

    cur_coef_44p1k = coef_44p1k;

    if (anc_opened(ANC_FEEDFORWARD)) {
        anc_select_coef(cur_coef_idx, ANC_FEEDFORWARD, ANC_GAIN_NO_DELAY);
    }

    if (anc_opened(ANC_FEEDBACK)) {
        anc_select_coef(cur_coef_idx, ANC_FEEDBACK, ANC_GAIN_NO_DELAY);
    }
#endif

    return 0;
}

enum ANC_INDEX anc_get_current_coef(void)
{
    return cur_coef_idx;
}

