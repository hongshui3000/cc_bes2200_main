#ifndef __SPEECH_DRC_H__
#define __SPEECH_DRC_H__

struct speech_drc_cfg
{
    int ch_num;             //channel number
    int fs;                 //frequency
    int depth;              //audio depth
    int frame_size;         //if change this only, do not need call  drc_setup
    int threshold;
    int makeup_gain;
    int ratio;
    int knee;
    int release_time;
    int attack_time;
};

#ifdef __cplusplus
extern "C" {
#endif

void speech_drc_init(struct speech_drc_cfg *cfg);
void speech_drc_process_fix(short *buf_p_l, short *buf_p_r);


#ifdef __cplusplus
}
#endif

#endif
