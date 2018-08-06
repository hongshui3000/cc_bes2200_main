#ifndef NS3_H
#define NS3_H

#include <stdint.h>

typedef enum
{
    NS3_SUPPRESSION_LOW = 0,
    NS3_SUPPRESSION_MID = 1,
    NS3_SUPPRESSION_HIGH = 2,
    NS3_SUPPRESSION_VERY_HIGH = 3
} NS3_SUPPRESSION_MODE;

typedef struct
{
    NS3_SUPPRESSION_MODE mode;
} Ns3Config;

struct Ns3State_;

typedef struct Ns3State_ Ns3State;

Ns3State *ns3_init(int sample_rate, int frame_size, const Ns3Config *config);

void ns3_destroy(Ns3State *st);

void ns3_process(Ns3State *st, int16_t *buf, int frame_size);

#endif