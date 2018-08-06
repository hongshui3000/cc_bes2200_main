#ifndef __SPEECH_MEMORY_H__
#define __SPEECH_MEMORY_H__

#include "med_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define speech_heap_init(a, b)          med_system_heap_init(a, b)
#define speech_malloc(a)                med_malloc(a)
#define speech_realloc(a, b)            med_realloc(a, b)
#define speech_calloc(a, b)             med_calloc(a, b)
#define speech_free(a)                  med_free(a)
#define speech_memory_info(a, b, c)     med_memory_info(a, b, c)

#ifdef __cplusplus
}
#endif

#endif