#ifndef _SPEECH_NOISE_GATE_H_
#define _SPEECH_NOISE_GATE_H_

#include <stdint.h>

typedef struct {
	int data_threshold;
	int data_shift;
	float factor_up;
	float factor_down;
} NoisegateConfig;

struct NoisegateState_;

typedef struct NoisegateState_ NoisegateState;

#ifdef __cplusplus
extern "C" {
#endif

void speech_noise_gate_run(NoisegateState *noise_gate_state,short *buf, uint32_t len);

void speech_noise_gate_deinit(NoisegateState *noise_gate_state);

NoisegateState* speech_noise_gate_init(int sampling_rate, int frame_size, const NoisegateConfig *noise_gate_cfg);

#ifdef __cplusplus
}
#endif

#endif
