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
    bool ns_enabled;
    bool cng_enabled;
    // af config
    int blocks;
    int delay;
    // nlp config
    float min_ovrd;
    float target_supp;
    // ns config
    float noise_supp;
    // cng config
    int cng_type; // 0 - white noise, 1 - pink noise
    float cng_level;
} EcConfig;

EcState *ec_init(int sample_rate, int frame_size, const EcConfig *config);

void ec_destroy(EcState *st);

void ec_process(EcState *st, int16_t *in, int16_t *ref, int16_t *out);

#endif