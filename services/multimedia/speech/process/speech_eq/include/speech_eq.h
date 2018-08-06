#ifndef SPEECH_EQ_H
#define SPEECH_EQ_H

#include "iirfilt.h"

// use more frame_size * sizeof(float) ram
#define EQ_USE_CMSIS_IIR

#define MAX_VQE_EQ_BAND 5

typedef struct
{
	enum IIR_BIQUARD_TYPE type;
	int32_t f0;
	float gain;
	float q;
} BiquardParam;

typedef struct
{
	float gain;
	int16_t num;
	BiquardParam param[MAX_VQE_EQ_BAND];
} EqConfig;

typedef struct
{
    float a1, a2, b0, b1, b2;
} BiquardParamRaw;

typedef struct
{
    float gain;
    int16_t num;
    BiquardParamRaw param[MAX_VQE_EQ_BAND];
} EqConfigRaw;

typedef struct EqState_ EqState;

#ifdef __cplusplus
extern "C" {
#endif

EqState *eq_init(int32_t sample_rate, int32_t frame_size, const EqConfig *config);

EqState *eq_init_raw(int32_t sample_rate, int32_t frame_size, const EqConfigRaw *config);

void eq_destroy(EqState *st);

void eq_process(EqState *st, int16_t *buf, int32_t frame_size);

#ifdef __cplusplus
}
#endif

#endif