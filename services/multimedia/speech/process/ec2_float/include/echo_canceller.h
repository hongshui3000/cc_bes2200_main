#ifndef ECHO_CANCELLER_H
#define ECHO_CANCELLER_H

#include <stdint.h>
#include <stdbool.h>

struct Ecstate_;

typedef struct EcState_ EcState;

typedef struct
{
    bool hpf_enabled;
    bool af_enabled;
    bool nlp_enabled;
    int blocks;
    int delay;
    // nlp config
    float min_ovrd;
    float target_supp;
} EcConfig;

EcState *ec_init(int sample_rate, int frame_size, EcConfig *config);

void ec_destroy(EcState *st);

void ec_process(EcState *st, int16_t *in, int16_t *ref, int16_t *out);

#endif