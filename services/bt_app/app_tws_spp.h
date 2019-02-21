#ifndef __APP_TWS_SPP_H__
#define __APP_TWS_SPP_H__
#include "a2dp.h"
#include "avrcp.h"
#include "avdtp.h"
#include "cqueue.h"
#include "app_audio.h"
#include "spp.h"


#define TWS_SPP_CMD_SET_CODEC_TYPE  (0x100)
#define TWS_SPP_CMD_SET_HFP_VOLUME  (0x101)
#define TWS_SPP_CMD_SET_TWSPLAYER_RESTART  (0x102)
#define TWS_SPP_CMD_NOTIFY_KEY  (0x103)
#define TWS_SPP_CMD_SET_A2DP_VOLUME  (0x104)
#define TWS_SPP_CMD_RING_SYNC  (0x105)
//@20180304 by parker.wei 
#ifdef _PROJ_2000IZ_C003_
#define TWS_SPP_CMD_UPDATE_PEER_STATE (0X110)// by parker.wei NOTIFY THE SLAVE ABOUT THE AG state
#endif
//Modified by ATX : parker.wei_20180306
#define TWS_SPP_CMD_HFP_VOLUME_CHANGE  (0X111)
#define TWS_SPP_CMD_A2DP_VOLUME_CHANGE  (0X112)

#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__ //Modified by ATX : Parke.Wei_20180413
#define TWS_SPP_CMD_SLAVE_ENTER_CMD (0X113)//
#endif

//Modified by ATX : Haorong.Wu_20180507
#define TWS_SPP_CMD_VOICE_REPORT  (0X114)

#define TWS_SPP_CMD_SET_SYSTEM_VOL (0X115)//


   
typedef struct _TWS_SPP_SET_TWS_CODEC_TYPE{
    uint16_t cmd_id;
    uint8_t codec_type;
}__attribute__((packed)) TWS_SPP_SET_TWS_CODEC_TYPE;



typedef struct _TWS_SPP_SET_TWS_HFP_VOLUME{
    uint16_t cmd_id;
    int8_t vol;
}__attribute__((packed)) TWS_SPP_SET_TWS_HFP_VOLUME;

typedef struct _TWS_SPP_SET_TWS_PLAYER_RESTART{
    uint16_t cmd_id;
    int8_t restart;
}__attribute__((packed)) TWS_SPP_SET_TWS_PLATER_RESTART;

typedef struct _TWS_SPP_SET_TWS_NOTIFY_KEY{
    uint16_t cmd_id;
    int32_t key;
}__attribute__((packed)) TWS_SPP_SET_TWS_NOTIFY_KEY;


typedef struct _TWS_SPP_SET_TWS_A2DP_VOLUME{
    uint16_t cmd_id;
    int8_t vol;
}__attribute__((packed)) TWS_SPP_SET_TWS_A2DP_VOLUME;

typedef struct _TWS_SPP_SET_TWS_SYSTEM_VOLUME{
    uint16_t cmd_id;
    uint16_t vol;
}__attribute__((packed)) TWS_SPP_SET_TWS_SYSTEM_VOLUME;

//Modified by ATX : Parke.Wei_20180316
#ifdef _PROJ_2000IZ_C003_
typedef struct _TWS_SPP_UPDATE_PEER_STATE{
    uint16_t cmd_id;
    uint8_t state;
}__attribute__((packed)) TWS_SPP_UPDATE_PEER_STATE;
#endif


//Modified by ATX : Parke.Wei_20180413
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
typedef struct _TWS_SPP_UPDATE_PEER_CTRL{
    uint16_t cmd_id;
    uint8_t reserved;
}__attribute__((packed)) TWS_SPP_UPDATE_PEER_CTRL;
#endif

//Modified by ATX : Haorong.Wu_20180507
typedef struct _TWS_SPP_SEND_VOICE_REPORT{
    uint16_t cmd_id;
    uint16_t voice_id;
}__attribute__((packed)) TWS_SPP_SEND_VOICE_REPORT;

typedef struct __TWS_SPP_CMD_RSP{
    uint16_t cmd_id;
    int8_t status;
} __attribute__((packed)) TWS_SPP_CMD_RSP;

typedef struct _TWS_SPP_SET_RING_SYNC{
    uint16_t cmd_id;
    U16 hf_event;
}__attribute__((packed)) TWS_SPP_SET_RING_SYNC;

void tws_create_spp_client(A2dpStream *Stream);
void tws_create_spp_server(A2dpStream *Stream);
void tws_spp_send_cmd(uint8_t* cmd, uint8_t len);
void tws_spp_send_rsp(uint8_t* rsp, uint8_t len);
void tws_spp_set_codec_type(uint8_t codec_type);

void tws_spp_set_hfp_vol(int8_t vol);

void tws_spp_restart_player(int8_t restart);

void tws_spp_notify_key(uint32_t key);

void tws_spp_set_a2dp_vol(uint8_t vol);
void  btapp_process_spp_write(uint16_t cmdid,uint32_t param,uint8_t *ptr,uint32_t len);
#ifdef _PROJ_2000IZ_C003_
void tws_spp_update_peer_state(uint8_t state);
#endif

#endif
