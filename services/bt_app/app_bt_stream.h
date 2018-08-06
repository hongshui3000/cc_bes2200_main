#ifndef __APP_BT_STREAM_H__
#define __APP_BT_STREAM_H__

#include "hal_aud.h"

#if defined(A2DP_AAC_ON)
#define BT_AUDIO_BUFF_SIZE		(2048*2*2)
#else
#define BT_AUDIO_BUFF_SIZE		(512*5*2*2)
#endif

//Modified by ATX : parker.wei_20180310
#define  INVALID_VOL_TYPE  0
#define  A2DP_VOL_TYPE  1
#define  HFP_VOL_TYPE  2



//120:frame size, 2:uint16_t, 2:pingpong; (2: LR dual channel)
#define BT_AUDIO_SCO_BUFF_SIZE      (SPEEH_FRAME_LENGTH*2*2)

#define BT_AUDIO_CACHE_2_UNCACHE(addr) \
    ((unsigned char *)((unsigned int)addr & ~(0x04000000)))

// #if BT_AUDIO_BUFF_SIZE < BT_AUDIO_SCO_BUFF_SIZE * 4
// #error BT_AUDIO_BUFF_SIZE must be at least BT_AUDIO_SCO_BUFF_SIZE * 4
// #endif

enum APP_BT_STREAM_T {
	APP_BT_STREAM_HFP_PCM = 0,
	APP_BT_STREAM_HFP_CVSD,
	APP_BT_STREAM_HFP_VENDOR,
	APP_BT_STREAM_A2DP_SBC,
	APP_BT_STREAM_A2DP_AAC,
	APP_BT_STREAM_A2DP_VENDOR,
	APP_FACTORYMODE_AUDIO_LOOP,
	APP_PLAY_BACK_AUDIO,
        APP_A2DP_SOURCE_LINEIN_AUDIO,

#ifdef TWS_RBCODEC_PLAYER
	APP_BT_STREAM_RBCODEC,
#endif
#ifdef TWS_LINEIN_PLAYER
	APP_BT_STREAM_LINEIN,
#endif

	APP_BT_STREAM_NUM,
};

enum APP_BT_SETTING_T {
    APP_BT_SETTING_OPEN = 0,
    APP_BT_SETTING_SETUP,
    APP_BT_SETTING_CLOSE,
    APP_BT_SETTING_CLOSEALL,
    APP_BT_SETTING_CLOSEMEDIA,
    APP_BT_SETTING_PAUSE,
    APP_BT_SETTING_NUM,
};

typedef enum {
    APP_BT_SBC_PLAYER_NULL = 0,
    APP_BT_SBC_PLAYER_TRIGGER,
    APP_BT_SBC_PLAYER_RESTART,
    APP_BT_SBC_PLAYER_PAUSE,
//    APP_BT_SBC_PLAYER_EXTERN_START,
//    APP_BT_SBC_PLAYER_EXTERN_STOP,
}APP_BT_SBC_PLAYER_OPT_T;

typedef struct {
    uint16_t id;
    uint16_t status;

    uint16_t aud_type;
    uint16_t aud_id;
    uint32_t param;

    uint8_t freq;
}APP_AUDIO_STATUS;

int app_bt_stream_open(APP_AUDIO_STATUS* status);

int app_bt_stream_close(enum APP_BT_STREAM_T player);

int app_bt_stream_setup(enum APP_BT_STREAM_T player, uint32_t param);

int app_bt_stream_restart(APP_AUDIO_STATUS* status);

int app_bt_stream_closeall();

bool app_bt_stream_isrun(enum APP_BT_STREAM_T player);

//Modified by ATX : parker.wei_20180310
#if 0
void app_bt_stream_volumeup(void);

void app_bt_stream_volumedown(void);

#else
uint8_t app_bt_stream_volumeup(void);
uint8_t app_bt_stream_volumedown(void);

#endif

void app_bt_stream_volume_ptr_update(uint8_t *bdAddr);

struct btdevice_volume * app_bt_stream_volume_get_ptr(void);

int app_bt_stream_volumeset(int8_t vol);

//Modified by ATX : parker.wei_20180310
int app_bt_stream_local_volume_get(void);

void app_bt_stream_a2dpvolume_reset(void);

void app_bt_stream_hfpvolume_reset(void);

enum AUD_SAMPRATE_T bt_parse_sbc_sample_rate(uint8_t sbc_samp_rate);

enum AUD_SAMPRATE_T bt_get_sbc_sample_rate(void);

void bt_store_sbc_sample_rate(enum AUD_SAMPRATE_T sample_rate);

enum AUD_SAMPRATE_T bt_parse_store_sbc_sample_rate(uint8_t sbc_samp_rate);

void app_bt_stream_copy_track_one_to_two_16bits(int16_t *dst_buf, int16_t *src_buf, uint32_t src_len);

// =======================================================
// APP RESAMPLE
// =======================================================

#include "hal_aud.h"

typedef int (*APP_RESAMPLE_ITER_CALLBACK)(uint8_t *buf, uint32_t len);

struct APP_RESAMPLE_T {
    void *id;
    APP_RESAMPLE_ITER_CALLBACK cb;
    uint8_t *iter_buf;
    uint32_t iter_len;
    uint32_t offset;
};

struct APP_RESAMPLE_T *app_playback_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
                                                  APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len);
struct APP_RESAMPLE_T *app_playback_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
                                                      APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
                                                      float ratio_step);
int app_playback_resample_close(struct APP_RESAMPLE_T *resamp);
int app_playback_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len);

struct APP_RESAMPLE_T *app_capture_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
                                                 APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len);
struct APP_RESAMPLE_T *app_capture_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
                                                     APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
                                                     float ratio_step);
int app_capture_resample_close(struct APP_RESAMPLE_T *resamp);
int app_capture_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len);

void app_resample_tune(struct APP_RESAMPLE_T *resamp, uint32_t rate, int32_t sample, uint32_t ms);
void app_resample_set_tune_factor(struct APP_RESAMPLE_T *resamp, float factor);
float app_resample_get_tune_factor(void);

#endif
