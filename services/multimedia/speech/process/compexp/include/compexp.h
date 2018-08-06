#ifndef COMPEXP_H
#define COMPEXP_H

#include <stdint.h>

typedef struct
{
    float comp_threshold;
    float comp_slope;
    float expand_threshold;
    float expand_slope;
    float attack_time;
    float release_time;
    float makeup_gain;
    int delay;
} CompexpConfig;

typedef struct CompexpState_ CompexpState;

#ifdef __cplusplus
extern "C" {
#endif

CompexpState *compexp_init(int sampling_rate, const CompexpConfig *config);

void compexp_destroy(CompexpState *st);

void compexp_process(CompexpState *st, int16_t *buf, int frame_size);

#ifdef __cplusplus
}
#endif

#endif