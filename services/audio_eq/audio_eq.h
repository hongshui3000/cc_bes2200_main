#ifndef __AUDIO_EQ_H__
#define __AUDIO_EQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "iir_process.h"
#include "fir_process.h"

int audio_eq_init(void);
int audio_eq_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_BITS_T sample_bits,enum AUD_CHANNEL_NUM_T ch_num, void *eq_buf, uint32_t len);
int audio_eq_run(uint8_t *buf, uint32_t len);
int audio_eq_close(void);

int audio_eq_iir_set_cfg(const IIR_CFG_T *cfg);
int audio_eq_fir_set_cfg(const FIR_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif