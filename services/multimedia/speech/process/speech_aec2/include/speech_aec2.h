#ifndef __SPEECH_AEC2_H__
#define __SPEECH_AEC2_H__

#include "plat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int     enEAecEnable;
    int     enHpfEnable;
    int     enAfEnable;
    int     enNsEnable;
    int     enNlpEnable;
    int     enCngEnable;
    int     shwDelayLength;
    int     shwNlpRefCnt;
    int     shwNlpRefAmp1;
    int     shwNlpExOverdrive;
    int     shwReserve4;
    int     shwNlpResdPowAlph;
    int     shwNlpResdPowThd;
    int     shwNlpSmoothGainDod;
    float   shwNlpBandSortIdx;
    float   shwNlpBandSortIdxLow;
    int     shwNlpTargetSupp;
    int     shwNlpMinOvrd;
    int     shwNlpOvrdHigh;
    int     shwNlpOvrdLow;
} SPEECH_AEC2_CFG_T;

typedef void SPEECH_AEC2_STATE_T;

SPEECH_AEC2_STATE_T *speech_aec2_create(const SPEECH_AEC2_CFG_T *cfg, int sample_rate, int frame_size);

int speech_aec2_destroy(SPEECH_AEC2_STATE_T *inst);

int speech_aec2_process(SPEECH_AEC2_STATE_T *inst, short *pcm_in_buf, short *pcm_echo_buf, int pcm_len);


#ifdef __cplusplus
}
#endif

#endif//__BT_SCO_CHAIN_WEBRTC_H__