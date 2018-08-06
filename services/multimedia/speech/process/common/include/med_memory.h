#ifndef __MED_MEMORY_H__
#define __MED_MEMORY_H__

#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"

/* #define RT_MEM_DEBUG */
#define RT_MEM_STATS

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GFP_KERNEL = 0,
    GFP_ATOMIC,
    __GFP_HIGHMEM,
    __GFP_HIGH
} gfp_t;   

struct mem_element{
    unsigned int size;
    unsigned char data[0];
};

extern void med_system_heap_init(void *begin_addr, void *end_addr);
extern void *med_malloc(uint32_t size);
extern void *med_realloc(void *rmem, uint32_t newsize);
extern void *med_calloc(uint32_t count, uint32_t size);
extern void med_free(void *rmem);

#ifdef RT_MEM_STATS
void med_memory_info(uint32_t *total,
                    uint32_t *used,
                    uint32_t *max_used);
#endif

#ifdef __cplusplus
}
#endif

#endif /*__OPUS_MEMORY_H__ */
