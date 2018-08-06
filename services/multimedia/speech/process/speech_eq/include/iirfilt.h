#ifndef IIRFILT_H
#define IIRFILT_H

#include <stdint.h>

enum IIR_BIQUARD_TYPE
{
    // pass through
    IIR_BIQUARD_PASS = 0,
    // low pass filter
    IIR_BIQUARD_LPF,
    // high pass filter
    IIR_BIQUARD_HPF,
    // band pass filter, constant skirt gain, peak gain = Q
    IIR_BIQUARD_BPF0,
    // band pass filter, const 0 dB peak gain
    IIR_BIQUARD_BPF1,
    // notch filter
    IIR_BIQUARD_NOTCH,
    // allpass filter
    IIR_BIQUARD_APF,
    // peakingEQ
    IIR_BIQUARD_PEAKINGEQ,
    // low shelf filter
    IIR_BIQUARD_LOWSHELF,
    // high shelf filter
    IIR_BIQUARD_HIGHSHELF,
    IIR_BIQUARD_QTY
};

struct IirBiquardState
{
    float a1, a2, b0, b1, b2;
    float s0, s1, s2;
};

typedef struct IirBiquardState IirBiquardState;

void iirfilt_design(IirBiquardState *st, int fs, int f0, float gain, float q, enum IIR_BIQUARD_TYPE type);

void iirfilt_reset(IirBiquardState *st, int stages);

void iirfilt_process(IirBiquardState *st, int stages, int16_t *buf, int frame_size);

void iirfilt_process_float(IirBiquardState *st, int stages, float *buf, int frame_size);

/* Deal with master gain in iir */
void iirfilt_process2(IirBiquardState *st, int stages, float master_gain, int16_t *buf, int frame_size);

void iirfilt_process2_float(IirBiquardState *st, int stages, float master_gain, float *buf, int frame_size);

#endif