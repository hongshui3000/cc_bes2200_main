#ifndef __APP_BT_MEDIA_MANAGER_H__
#define __APP_BT_MEDIA_MANAGER_H__

#include "resources.h"
#ifdef __cplusplus
extern "C" {
#endif

#define  BT_STREAM_NULL         0x00
#define  BT_STREAM_SBC          0x01
#define  BT_STREAM_MEDIA        0x02
#define  BT_STREAM_VOICE        0x04

#ifdef TWS_RBCODEC_PLAYER
#define  BT_STREAM_RBCODEC      0x08    //from rockbox decoder
#define  BT_STREAM_LINEIN       0x10    //from rockbox decoder
#endif


#ifdef __EARPHONE_AUDIO_MONITOR__
#define  BT_STREAM_MONITOR        0x08
#define BT_STREAM_TYPE_MASK     (BT_STREAM_SBC | BT_STREAM_MEDIA | BT_STREAM_VOICE | BT_STREAM_MONITOR)
#else
#define BT_STREAM_TYPE_MASK     (BT_STREAM_SBC | BT_STREAM_MEDIA | BT_STREAM_VOICE)
#endif


enum APP_BT_MEDIA_MANAGER_ID_T {
	APP_BT_STREAM_MANAGER_START = 0,
    APP_BT_STREAM_MANAGER_SETUP,
	APP_BT_STREAM_MANAGER_STOP,
	APP_BT_STREAM_MANAGER_SWITCHTO_SCO,
	APP_BT_STREAM_MANAGER_STOP_MEDIA,
	APP_BT_STREAM_MANAGER_NUM,
};

typedef void (*APP_AUDIO_MANAGER_CALLBACK)(uint32_t param);

#define APP_AUDIO_MANAGER_SET_MESSAGE(appevt, id, stream_type) (appevt = (((uint32_t)id&0xffff)<<16)|(stream_type&0xffff))
#define APP_AUDIO_MANAGER_SET_MESSAGE0(appmsg, device_id,aud_id) (appmsg = (((uint32_t)device_id&0xffff)<<16)|(aud_id&0xffff))
#define APP_AUDIO_MANAGER_SET_PARAM(appparam, param) (appparam = param)

#define APP_AUDIO_MANAGER_GET_ID(appevt, id) (id = (appevt>>16)&0xffff)
#define APP_AUDIO_MANAGER_GET_STREAM_TYPE(appevt, stream_type) (stream_type = appevt&0xffff)
#define APP_AUDIO_MANAGER_GET_DEVICE_ID(appmsg, device_id) (device_id = (appmsg>>16)&0xffff)
#define APP_AUDIO_MANAGER_GET_AUD_ID(appmsg, aud_id) (aud_id = appmsg&0xffff)
#define APP_AUDIO_MANAGER_GET_PTR(appparam, ptr) (ptr = appparam)
#define APP_AUDIO_MANAGER_GET_PARAM(appparam, param) (param = appparam)

int app_audio_manager_sendrequest(uint8_t massage_id,uint8_t stream_type, uint8_t device_id, uint8_t aud_id, uint32_t param, uint32_t ptr);
void app_audio_manager_open(void);
bool app_audio_manager_bt_stream_voice_request_exist(void);
uint8_t app_audio_manager_get_current_media_type(void);
uint8_t bt_media_get_current_media(void);


void  bt_media_start(uint8_t stream_type,enum BT_DEVICE_ID_T device_id,AUD_ID_ENUM media_id, uint32_t param, uint32_t ptr);
void bt_media_stop(uint8_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr);
void bt_media_switch_to_voice(uint8_t stream_type,enum BT_DEVICE_ID_T device_id, uint32_t ptr);
uint8_t bt_media_is_media_active_by_type(uint8_t media_type);

void app_audio_set_key_priority(void);
void app_audio_reset_priority(void);
void app_linein_ctr_play_onoff(bool on );
int app_audio_manager_sco_status_checker(void);

#ifdef __cplusplus
	}
#endif

#endif
