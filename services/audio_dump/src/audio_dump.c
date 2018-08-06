#include "hal_trace.h"
#include "audio_dump.h"
#include "string.h"

#ifdef AUDIO_DUMP
#define AUDIO_DUMP_MAX_FRAME_LEN      (512)
#define AUDIO_DUMP_MAX_CHANNEL_NUM    (8)
#define AUDIO_DUMP_BUFFER_SIZE        (AUDIO_DUMP_MAX_FRAME_LEN * AUDIO_DUMP_MAX_CHANNEL_NUM)

static int audio_dump_frame_len = 256;
static int audio_dump_channel_num = 2;
static short audio_dump_dump_buf[AUDIO_DUMP_BUFFER_SIZE];
#endif

void audio_dump_clear_up(void)
{
#ifdef AUDIO_DUMP
    memset(audio_dump_dump_buf, 0, sizeof(audio_dump_dump_buf));
#endif
}

void audio_dump_init(int frame_len, int channel_num)
{
#ifdef AUDIO_DUMP
    ASSERT(frame_len <= AUDIO_DUMP_MAX_FRAME_LEN, "[%s] frame_len(%d) is invalid", __func__, frame_len);
    ASSERT(channel_num <= AUDIO_DUMP_MAX_CHANNEL_NUM, "[%s] channel_num(%d) is invalid", __func__, channel_num);

    audio_dump_frame_len = frame_len;
    audio_dump_channel_num = channel_num;
    audio_dump_clear_up();
#endif
}

void audio_dump_add_channel_data(int channel_id, short *pcm_buf, int pcm_len)
{
#ifdef AUDIO_DUMP
    ASSERT(audio_dump_frame_len == pcm_len, "[%s] pcm_len(%d) is not valid", __func__, pcm_len);

    if (channel_id >= audio_dump_channel_num)
    {
        return;
    }

    for(int i=0; i<pcm_len; i++)
    {
        audio_dump_dump_buf[audio_dump_channel_num * i + channel_id] = pcm_buf[i];
    }
#endif
}

void audio_dump_run(void)
{
#ifdef AUDIO_DUMP
    AUDIO_DEBUG_DUMP((const unsigned char *)audio_dump_dump_buf, audio_dump_frame_len * audio_dump_channel_num * sizeof(short));
#endif
}