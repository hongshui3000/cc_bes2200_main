#ifndef __SPEECH_DC_FILTER_H__
#define __SPEECH_DC_FILTER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// typedef struct {
//     // int channel_num;
//     // float gain;
// } SPEECH_DC_FILTER_CFG_T;

typedef void *SPEECH_DC_FILTER_STATE_T;

// Initial speech_dc_filter module/class
int speech_dc_filter_init(void);

// Deinitial speech_dc_filter module/calss
int speech_dc_filter_deinit(void);

// Creat a instance from speech_dc_filter module/class
// Common value include: sample rate, frame size and so on. 
SPEECH_DC_FILTER_STATE_T *speech_dc_filter_create(void);

// Destory a speech dc_filter instance
int speech_dc_filter_destory(SPEECH_DC_FILTER_STATE_T *inst);

// Just use modify instance configure
// int speech_dc_filter_set_config(SPEECH_DC_FILTER_STATE_T *inst, SPEECH_DC_FILTER_CFG_T *cfg);

// Process speech stream
int speech_dc_filter_process(SPEECH_DC_FILTER_STATE_T *inst, int16_t *pcm_buf, int32_t pcm_len);

// Debug speech_dc_filter instance
int speech_dc_filter_dump(SPEECH_DC_FILTER_STATE_T *inst);

#ifdef __cplusplus
}
#endif

#endif
