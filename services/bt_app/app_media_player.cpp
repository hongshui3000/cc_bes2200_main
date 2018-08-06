
#ifdef MEDIA_PLAYER_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cmsis_os.h"
#include "tgt_hardware.h"

#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "app_bt_stream.h"
#include "app_overlay.h"
#include "app_audio.h"
#include "app_utils.h"

#include "res_audio_data.h"
#include "res_audio_data_cn.h"
#include "resources.h"
#include "app_media_player.h"

extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
}

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt_media_manager.h"

#include "app_tws.h"

static char need_init_decoder = 1;
static SbcDecoder *media_sbc_decoder = NULL;

#define SBC_TEMP_BUFFER_SIZE 64
#define SBC_QUEUE_SIZE (SBC_TEMP_BUFFER_SIZE*16)
CQueue media_sbc_queue;

static float media_sbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS];

#ifdef __BT_ANC__
#define APP_AUDIO_PLAYBACK_BUFF_SIZE		(1024*3)
#else
#define APP_AUDIO_PLAYBACK_BUFF_SIZE		(1024)
#endif
#define  SBC_FRAME_LEN  64 //0x5c   /* pcm 512 bytes*/
static U8* g_app_audio_data = NULL;
static uint32_t g_app_audio_length = 0;
static uint32_t g_app_audio_read = 0;

static uint32_t g_play_continue_mark = 0;

static uint8_t app_play_sbc_stop_proc_cnt = 0;

//for continue play     

#define MAX_SOUND_NUMBER 10 

typedef struct tMediaSoundMap
{
    U8* data;  //total files
    uint32_t fsize; //file index
    
}_tMediaSoundMap;

tMediaSoundMap*  media_sound_map;
    
tMediaSoundMap media_sound_map_cn[MAX_SOUND_NUMBER] =
{
    {(U8*)CN_SOUND_ZERO,    sizeof(CN_SOUND_ZERO) },
    {(U8*)CN_SOUND_ONE,    sizeof(CN_SOUND_ONE) },
    {(U8*)CN_SOUND_TWO,     sizeof(CN_SOUND_TWO) },
    {(U8*)CN_SOUND_THREE,     sizeof(CN_SOUND_THREE) },
    {(U8*)CN_SOUND_FOUR,     sizeof(CN_SOUND_FOUR) },
    {(U8*)CN_SOUND_FIVE,     sizeof(CN_SOUND_FIVE) },
    {(U8*)CN_SOUND_SIX,     sizeof(CN_SOUND_SIX) },
    {(U8*)CN_SOUND_SEVEN,     sizeof(CN_SOUND_SEVEN) },
    {(U8*)CN_SOUND_EIGHT,     sizeof(CN_SOUND_EIGHT) },
    {(U8*)CN_SOUND_NINE,    sizeof(CN_SOUND_NINE) }, 
};

tMediaSoundMap media_sound_map_en[MAX_SOUND_NUMBER] =
{
    {(U8*)EN_SOUND_ZERO,    sizeof(EN_SOUND_ZERO) },
    {(U8*)EN_SOUND_ONE,    sizeof(EN_SOUND_ONE) },
    {(U8*)EN_SOUND_TWO,     sizeof(EN_SOUND_TWO) },
    {(U8*)EN_SOUND_THREE,     sizeof(EN_SOUND_THREE) },
    {(U8*)EN_SOUND_FOUR,     sizeof(EN_SOUND_FOUR) },
    {(U8*)EN_SOUND_FIVE,     sizeof(EN_SOUND_FIVE) },
    {(U8*)EN_SOUND_SIX,     sizeof(EN_SOUND_SIX) },
    {(U8*)EN_SOUND_SEVEN,     sizeof(EN_SOUND_SEVEN) },
    {(U8*)EN_SOUND_EIGHT,     sizeof(EN_SOUND_EIGHT) },
    {(U8*)EN_SOUND_NINE,    sizeof(EN_SOUND_NINE) }, 
};

char Media_player_number[MAX_PHB_NUMBER];

typedef struct tPlayContContext
{
    uint32_t g_play_continue_total; //total files
    uint32_t g_play_continue_n; //file index
    
    uint32_t g_play_continue_fread; //per file have readed
    
    U8 g_play_continue_array[MAX_PHB_NUMBER];
    
}_tPlayContContext;

tPlayContContext pCont_context;
    
APP_AUDIO_STATUS MSG_PLAYBACK_STATUS;
APP_AUDIO_STATUS*  ptr_msg_playback = &MSG_PLAYBACK_STATUS;

static int g_language = MEDIA_DEFAULT_LANGUAGE;

#define PREFIX_AUDIO(name)  ((g_language==MEDIA_DEFAULT_LANGUAGE) ? EN_##name : CN_##name)

int media_audio_init(void)
{
    const float EQLevel[25] = {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107 , 0.5011872,
        0.6309573,  0.794328 , 1,         1.258925 , 1.584893 ,
        1.995262 ,  2.5118864, 3.1622776, 3.9810717, 5.011872 ,
        6.309573 ,  7.943282 , 10       , 12.589254, 15.848932
    };//-12~12
    uint8_t *buff = NULL;
    uint8_t i;

    for (i=0; i<sizeof(media_sbc_eq_band_gain)/sizeof(float); i++){
        media_sbc_eq_band_gain[i] = EQLevel[12];
    }

    app_audio_mempool_get_buff(&buff, SBC_QUEUE_SIZE);
    memset(buff, 0, SBC_QUEUE_SIZE);

    LOCK_APP_AUDIO_QUEUE();
    APP_AUDIO_InitCQueue(&media_sbc_queue, SBC_QUEUE_SIZE, buff);
    UNLOCK_APP_AUDIO_QUEUE();
    
    app_audio_mempool_get_buff((uint8_t **)&media_sbc_decoder, sizeof(SbcDecoder) + 4);

    need_init_decoder = 1;

    app_play_sbc_stop_proc_cnt = 0;

    return 0;
}
static int decode_sbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    uint8_t underflow = 0;

    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    
    static SbcPcmData pcm_data;
    XaStatus ret = XA_STATUS_SUCCESS;
    unsigned short byte_decode = 0;

    pcm_data.data = (unsigned char*)pcm_buffer;

    LOCK_APP_AUDIO_QUEUE();
again:
    if(need_init_decoder) {
        pcm_data.data = (unsigned char*)pcm_buffer;
        pcm_data.dataLen = 0;
#ifdef __SBC_FUNC_IN_ROM__
        SBC_ROM_FUNC.SBC_InitDecoder(media_sbc_decoder);
#else        
        SBC_InitDecoder(media_sbc_decoder);
#endif
    }

get_again:
    len1 = len2 = 0;
    r = APP_AUDIO_PeekCQueue(&media_sbc_queue, SBC_TEMP_BUFFER_SIZE, &e1, &len1, &e2, &len2);

    if(r == CQ_ERR) {
            need_init_decoder = 1;
            underflow = 1;
            r = pcm_data.dataLen;
            TRACE("cache underflow");
            goto exit;
    }
    if (!len1){
        TRACE("len1 underflow %d/%d\n", len1, len2);
        goto get_again;
    }
#ifdef __SBC_FUNC_IN_ROM__                
        ret =  SBC_ROM_FUNC.SBC_DecodeFrames(media_sbc_decoder, (unsigned char *)e1, len1, &byte_decode,
        &pcm_data, pcm_len, media_sbc_eq_band_gain);  
#else    
    ret = SBC_DecodeFrames(media_sbc_decoder, (unsigned char *)e1, len1, &byte_decode,
        &pcm_data, pcm_len, media_sbc_eq_band_gain);
#endif
    if(ret == XA_STATUS_CONTINUE) {
        need_init_decoder = 0;
        APP_AUDIO_DeCQueue(&media_sbc_queue, 0, len1);
        goto again;

        /* back again */
    }
    else if(ret == XA_STATUS_SUCCESS) {
        need_init_decoder = 0;
        r = pcm_data.dataLen;
        pcm_data.dataLen = 0;

        APP_AUDIO_DeCQueue(&media_sbc_queue, 0, byte_decode);

        //TRACE("p %d\n", pcm_data.sampleFreq);

        /* leave */
    }
    else if(ret == XA_STATUS_FAILED) {
        need_init_decoder = 1;
        r = pcm_data.dataLen;
        TRACE("err\n");

        APP_AUDIO_DeCQueue(&media_sbc_queue, 0, byte_decode);

        /* leave */
    }
    else if(ret == XA_STATUS_NO_RESOURCES) {
        need_init_decoder = 0;

        TRACE("no\n");

        /* leav */
        r = 0;
    }

exit:
    if (underflow){
        TRACE( "media_sbc_decoder underflow len:%d\n ", pcm_len);
    }
    UNLOCK_APP_AUDIO_QUEUE();
    return r;
}

static int store_sbc_buffer(unsigned char *buf, unsigned int len)
{
    int nRet;

    LOCK_APP_AUDIO_QUEUE();
    nRet = APP_AUDIO_EnCQueue(&media_sbc_queue, buf, len);
    UNLOCK_APP_AUDIO_QUEUE();

    return nRet;
}


uint32_t media_PlayAudio(AUD_ID_ENUM id,uint8_t device_id)
{
 //   uint32_t aud_id = (uint32_t)id;
 //   TRACE("=========media_PlayAudio, aud_id = %d", aud_id );
   // app_audio_sendrequest((uint8_t)APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, aud_id, 0);    
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_MEDIA,device_id,id,0,0);
 
    return 0;
}


void media_Set_IncomingNumber(const char* pNumber)
{
    memcpy(Media_player_number, pNumber, MAX_PHB_NUMBER);
}

void media_Play_init_audio(APP_AUDIO_STATUS* data)
{

    APP_AUDIO_STATUS* ptr = (APP_AUDIO_STATUS* )data;


    if (ptr->aud_id == AUD_ID_BT_CALL_INCOMING_NUMBER)  
    {
          g_play_continue_mark = 1; 

        if (g_language == MEDIA_DEFAULT_LANGUAGE)      
            media_sound_map = media_sound_map_en;
        else
            media_sound_map = media_sound_map_cn;

        memset(&pCont_context, 0x0, sizeof(pCont_context)); 
        
        pCont_context.g_play_continue_total =  strlen((const char*)Media_player_number);

        for (uint32_t i=0;( i<pCont_context.g_play_continue_total) && (i< MAX_PHB_NUMBER); i++)
        {
        pCont_context.g_play_continue_array[i] = Media_player_number[i] - '0';

        TRACE("media_PlayNumber, pCont_context.g_play_continue_array[%d] = %d, total =%d",i,  pCont_context.g_play_continue_array[i], pCont_context.g_play_continue_total);
        
        }
    }   else{
        g_app_audio_read = 0;
        g_play_continue_mark = 0;
            
        switch( ptr->aud_id)
        {
        case AUD_ID_POWER_ON:   
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_POWER_ON:  (U8*)CN_POWER_ON; //aud_get_reouce((AUD_ID_ENUM)id, &g_app_audio_length, &type);
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_POWER_ON): sizeof(CN_POWER_ON);
            break;
        case AUD_ID_POWER_OFF:   
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_POWER_OFF: (U8*)CN_POWER_OFF;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_POWER_OFF): sizeof(CN_POWER_OFF);
            break;
        case AUD_ID_BT_PAIR_ENABLE:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_PAIR_ENABLE: (U8*)CN_BT_PAIR_ENABLE;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_PAIR_ENABLE): sizeof(CN_BT_PAIR_ENABLE);
            break;
        case AUD_ID_BT_PAIRING: 
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_PAIRING: (U8*)CN_BT_PAIRING;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_PAIRING): sizeof(CN_BT_PAIRING);     
            break;
        case AUD_ID_BT_PAIRING_SUC:              
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_PAIRING_SUCCESS: (U8*)CN_BT_PAIRING_SUCCESS;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_PAIRING_SUCCESS): sizeof(CN_BT_PAIRING_SUCCESS);     
            break;
        case AUD_ID_BT_PAIRING_FAIL:            
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_PAIRING_FAIL: (U8*)CN_BT_PAIRING_FAIL;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_PAIRING_FAIL): sizeof(CN_BT_PAIRING_FAIL);        
            break;
        case AUD_ID_BT_CALL_REFUSE:                 
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_REFUSE: (U8*)CN_BT_REFUSE;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_REFUSE): sizeof(CN_BT_REFUSE);      
            break;
        case AUD_ID_BT_CALL_OVER:                   
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_OVER: (U8*)CN_BT_OVER;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_OVER): sizeof(CN_BT_OVER);        
            break;
        case AUD_ID_BT_CALL_ANSWER: 
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_ANSWER: (U8*)CN_BT_ANSWER;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_ANSWER): sizeof(CN_BT_ANSWER);      
            break;
        case AUD_ID_BT_CALL_HUNG_UP:        
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_HUNG_UP: (U8*)CN_BT_HUNG_UP;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_HUNG_UP): sizeof(CN_BT_HUNG_UP);     
            break;
        case AUD_ID_BT_CALL_INCOMING_CALL:      
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_INCOMING_CALL: (U8*)CN_BT_INCOMING_CALL;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_INCOMING_CALL): sizeof(CN_BT_INCOMING_CALL);       
            break;
        case AUD_ID_BT_CHARGE_PLEASE:   
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_CHARGE_PLEASE: (U8*)CN_CHARGE_PLEASE;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_CHARGE_PLEASE): sizeof(CN_CHARGE_PLEASE);      
            break;
        case AUD_ID_BT_CHARGE_FINISH:       
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_CHARGE_FINISH: (U8*)CN_CHARGE_FINISH;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_CHARGE_FINISH): sizeof(CN_CHARGE_FINISH);      
            break;
        case AUD_ID_BT_CONNECTED:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_CONNECTED: (U8*)CN_BT_CONNECTED;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_CONNECTED): sizeof(CN_BT_CONNECTED);           
            break;
        case AUD_ID_BT_DIS_CONNECT:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_DIS_CONNECT: (U8*)CN_BT_DIS_CONNECT;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_DIS_CONNECT): sizeof(CN_BT_DIS_CONNECT);         
            break;     
        case AUD_ID_BT_WARNING:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_WARNING: (U8*)CN_BT_WARNING;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_WARNING): sizeof(CN_BT_WARNING);         
            break;
//Modified by ATX : cc_20180306            
        case AUD_ID_BT_VOL_MIN:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_VOL_MIN: (U8*)CN_BT_VOL_MIN;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_VOL_MIN): sizeof(CN_BT_VOL_MIN);
            break;
        case AUD_ID_BT_VOL_MAX:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_VOL_MAX: (U8*)CN_BT_VOL_MAX;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_VOL_MAX): sizeof(CN_BT_VOL_MAX);
            break;
//Modified by ATX : Leon.He_20180109
        case AUD_ID_BT_LONG_DOUBLE_LOW:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_LONG_DOUBLE_LOW: (U8*)CN_BT_LONG_DOUBLE_LOW;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_LONG_DOUBLE_LOW): sizeof(CN_BT_LONG_DOUBLE_LOW);
            break;
        case AUD_ID_BT_SHORT_1:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_SHORT_1: (U8*)CN_BT_SHORT_1;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_SHORT_1): sizeof(CN_BT_SHORT_1);
            break;
//Modified by ATX : Parker.Wei_2018328
		case AUD_ID_BT_USER_1:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_USER_1: (U8*)CN_BT_USER_1;
			g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_USER_1): sizeof(CN_BT_USER_1);
			break;
		case AUD_ID_BT_USER_2:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_BT_USER_2: (U8*)CN_BT_USER_2;
			g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_BT_USER_2): sizeof(CN_BT_USER_2);
			break;

        case AUD_ID_LANGUAGE_SWITCH:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)?(U8*)EN_LANGUAGE_SWITCH: (U8*)CN_LANGUAGE_SWITCH;
            g_app_audio_length = (g_language==MEDIA_DEFAULT_LANGUAGE)?sizeof(EN_LANGUAGE_SWITCH): sizeof(CN_LANGUAGE_SWITCH);         
            break;
        case AUD_ID_NUM_0:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_ZERO:  (U8*)CN_SOUND_ZERO;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_ZERO): sizeof(CN_SOUND_ZERO);
            break;
        case AUD_ID_NUM_1:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_ONE:  (U8*)CN_SOUND_ONE;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_ONE): sizeof(CN_SOUND_ONE);
            break;
        case AUD_ID_NUM_2: 
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_TWO:  (U8*)CN_SOUND_TWO;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_TWO): sizeof(CN_SOUND_TWO);
            break;
        case AUD_ID_NUM_3:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_THREE:  (U8*)CN_SOUND_THREE;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_THREE): sizeof(CN_SOUND_THREE);
            break;
        case AUD_ID_NUM_4:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_FOUR:  (U8*)CN_SOUND_FOUR;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_FOUR): sizeof(CN_SOUND_FOUR);
            break;
        case AUD_ID_NUM_5:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_FIVE:  (U8*)CN_SOUND_FIVE;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_FIVE): sizeof(CN_SOUND_FIVE);
            break;
        case AUD_ID_NUM_6:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_SIX:  (U8*)CN_SOUND_SIX;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_SIX): sizeof(CN_SOUND_SIX);
            break;
        case AUD_ID_NUM_7:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_SEVEN:  (U8*)CN_SOUND_SEVEN;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_SEVEN): sizeof(CN_SOUND_SEVEN);
            break;
        case AUD_ID_NUM_8:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_EIGHT:  (U8*)CN_SOUND_EIGHT;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_EIGHT): sizeof(CN_SOUND_EIGHT);
            break;
        case AUD_ID_NUM_9:
            g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_SOUND_NINE:  (U8*)CN_SOUND_NINE;
            g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_SOUND_NINE): sizeof(CN_SOUND_NINE);
            break;
#ifdef __TWS__			
		case AUD_ID_BT_TWS_ISMASTER:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_ISMASTER:  (U8*)CN_BT_TWS_ISMASTER;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_ISMASTER): sizeof(CN_BT_TWS_ISMASTER);
			break;
		case AUD_ID_BT_TWS_ISSLAVE:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_ISSLAVE:  (U8*)CN_BT_TWS_ISSLAVE;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_ISSLAVE): sizeof(CN_BT_TWS_ISSLAVE);
			break;
		case AUD_ID_BT_TWS_SEARCH:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_SEARCH:  (U8*)CN_BT_TWS_SEARCH;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_SEARCH): sizeof(CN_BT_TWS_SEARCH);
			break;
		case AUD_ID_BT_TWS_STOPSEARCH:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_STOPSEARCH:  (U8*)CN_BT_TWS_STOPSEARCH;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_STOPSEARCH): sizeof(CN_BT_TWS_STOPSEARCH);
			break;
		case AUD_ID_BT_TWS_DISCOVERABLE:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_DISCOVERABLE:  (U8*)CN_BT_TWS_DISCOVERABLE;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_DISCOVERABLE): sizeof(CN_BT_TWS_DISCOVERABLE);
			break;			
		case AUD_ID_BT_TWS_LEFTCHNL:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_LEFTCHNL:  (U8*)CN_BT_TWS_LEFTCHNL;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_LEFTCHNL): sizeof(CN_BT_TWS_LEFTCHNL);
			break;
		case AUD_ID_BT_TWS_RIGHTCHNL:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_RIGHTCHNL:  (U8*)CN_BT_TWS_RIGHTCHNL;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_RIGHTCHNL): sizeof(CN_BT_TWS_RIGHTCHNL);
			break;
//Modified by ATX : Leon.He_20171123			
		case AUD_ID_BT_TWS_CONNECTED:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_CONNECTED:  (U8*)CN_BT_TWS_CONNECTED;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_CONNECTED): sizeof(CN_BT_TWS_CONNECTED);
			break;
		case AUD_ID_BT_TWS_PAIRING:
			g_app_audio_data = (g_language==MEDIA_DEFAULT_LANGUAGE)? (U8*)EN_BT_TWS_PAIRING:  (U8*)CN_BT_TWS_PAIRING;
			g_app_audio_length =  (g_language==MEDIA_DEFAULT_LANGUAGE)? sizeof(EN_BT_TWS_PAIRING): sizeof(CN_BT_TWS_PAIRING);
			break;
#endif
            
        default:
            g_app_audio_length = 0;
            break;
        }
    }   
}
uint32_t app_play_sbc_more_data_fadeout(int16_t *buf, uint32_t len)
{
    uint32_t i;
    uint32_t j = 0;

    for (i = len; i > 0; i--){
        *(buf+j) = *(buf+j)*i/len;
        j++;
    }
    
    return len;
}

static uint32_t need_fadein_len = 0;
static uint32_t need_fadein_len_processed = 0;

int app_play_sbc_more_data_fadein_config(uint32_t len)
{
    TRACE("fadein_config l:%d", len);
    need_fadein_len = len;
    need_fadein_len_processed = 0;
    return 0;
}
uint32_t app_play_sbc_more_data_fadein(int16_t *buf, uint32_t len)
{
    uint32_t i;
    uint32_t j = 0;
    uint32_t base;
    uint32_t dest;

    base = need_fadein_len_processed;
    dest = need_fadein_len_processed + len < need_fadein_len ? 
           need_fadein_len_processed + len :
           need_fadein_len_processed;

    if (base >= dest){
//        TRACE("skip fadein");
        return len;
    }
//    TRACE("fadein l:%d base:%d dest:%d", len, base, dest);
//    DUMP16("%5d ", buf, 20);    
//    DUMP16("%5d ", buf+len-19, 20);
    
    for (i = base; i < dest; i++){
        *(buf+j) = *(buf+j)*i/need_fadein_len;
        j++;
    }
    
    need_fadein_len_processed += j;
//    DUMP16("%05d ", buf, 20);
//    DUMP16("%5d ", buf+len-19, 20);
    return len;
}

uint32_t app_play_single_sbc_more_data(uint8_t *buf, uint32_t len)
{
    //int32_t stime, etime;
    //U16 byte_decode;
    uint32_t l = 0;
    
     //TRACE( "app_play_sbc_more_data : %d, %d", g_app_audio_read, g_app_audio_length);

    if (g_app_audio_read < g_app_audio_length){
        unsigned int available_len = 0;
        unsigned int store_len = 0;

        available_len = AvailableOfCQueue(&media_sbc_queue);
        store_len = (g_app_audio_length - g_app_audio_read) > available_len ? available_len :(g_app_audio_length - g_app_audio_read);
        store_sbc_buffer((unsigned char *)(g_app_audio_data + g_app_audio_read), store_len);
        g_app_audio_read += store_len;
    }


        l = decode_sbc_frame(buf, len);

        if ( l != len)
        {
            g_app_audio_read = g_app_audio_length;        
            //af_stream_stop(AUD_STREAM_PLAYBACK);
            //af_stream_close(AUD_STREAM_PLAYBACK);
            TRACE( "app_play_sbc_more_data-->need close, length:%d len:%d l:%d", g_app_audio_length, len, l); 
        }
    
    return l;
}


/* play continue sound */
uint32_t app_play_continue_sbc_more_data(uint8_t *buf, uint32_t len)
{
    
    uint32_t l, n, fsize = 0;

    U8*  pdata; 

// store data
    unsigned int available_len = 0;
    unsigned int store_len = 0;
    
    if (pCont_context.g_play_continue_n < pCont_context.g_play_continue_total){
        do {
            n = pCont_context.g_play_continue_n;
            pdata = media_sound_map[pCont_context.g_play_continue_array[n]].data;
            fsize = media_sound_map[pCont_context.g_play_continue_array[n]].fsize;

            available_len = AvailableOfCQueue(&media_sbc_queue);
            if (!available_len)
                break;
                
            store_len = (fsize - pCont_context.g_play_continue_fread) > available_len ? available_len :(fsize - pCont_context.g_play_continue_fread);
            store_sbc_buffer((unsigned char *)(pdata + pCont_context.g_play_continue_fread), store_len);
            pCont_context.g_play_continue_fread += store_len;
            if (pCont_context.g_play_continue_fread == fsize){
                pCont_context.g_play_continue_n++;
                pCont_context.g_play_continue_fread = 0;
            }
        }while(pCont_context.g_play_continue_n < pCont_context.g_play_continue_total);
    }

    l = decode_sbc_frame(buf, len);
    
    if (l !=len){
        TRACE( "continue sbc decode ---> APP_BT_SETTING_CLOSE"); 
    }

    return l;
}

#ifdef __BT_ANC__
extern uint8_t bt_sco_samplerate_ratio ;
extern void us_fir_init (void);
extern U32 us_fir_run (short* src_buf, short* dst_buf, U32 in_samp_num);
void app_anc_mono_to_stereo(int16_t *inbuf,int16_t *outbuf, uint32_t sample_cnt)
{
	for(uint32_t i = sample_cnt-1 ; i > 0; i--) {
		*(outbuf + i*2 ) = *(inbuf+i);
		*(outbuf + i*2 + 1) = *(inbuf+i);
	}
}

#endif

static int16_t *app_play_sbc_cache = NULL;
uint32_t app_play_sbc_more_data(uint8_t *buf, uint32_t len)
{
    uint32_t l = 0;

#ifdef __BT_ANC__
	uint32_t dec_len = len/bt_sco_samplerate_ratio;
#endif
    if (app_play_sbc_stop_proc_cnt) {
        memset(buf, 0, len);
        if (app_play_sbc_stop_proc_cnt == 1) {
            app_play_sbc_stop_proc_cnt = 2;
        } else if (app_play_sbc_stop_proc_cnt == 2) {
            app_play_sbc_stop_proc_cnt = 3;

            // For 8K sample rate data, it takes about 4ms (or 12ms if h/w resample in use) from codec to DAC PA.
            // The playback stream should be stopped after the last data arrives at DAC PA, otherwise there
            // might be some pop sound.
            app_audio_sendrequest((uint8_t)APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);
        }
    } else {
        if (app_play_sbc_cache) {
#ifdef __BT_ANC__
			len = dec_len;
#endif
            if (g_play_continue_mark) {
                l = app_play_continue_sbc_more_data((uint8_t *)app_play_sbc_cache, len/2);
            } else {
                l = app_play_single_sbc_more_data((uint8_t *)app_play_sbc_cache, len/2);
            }
            if (l != len / 2) {
#ifdef __BT_ANC__
				len = dec_len*3;
#endif
                memset(app_play_sbc_cache+l, 0, len/2-l);
                app_play_sbc_stop_proc_cnt = 1;
            }
#ifdef __BT_ANC__
			len = dec_len*3;
			l = l*3;
			us_fir_run((short*)app_play_sbc_cache,(short*)buf,dec_len/2/2);
			app_anc_mono_to_stereo((short*)buf, (short*)buf,len/2/2);
#else
            app_bt_stream_copy_track_one_to_two_16bits((int16_t *)buf, app_play_sbc_cache, len/2/2);
#endif
        } else {
            memset(buf, 0, len);
        }
    }

    return l;
}

int app_play_audio_onoff(bool onoff, APP_AUDIO_STATUS* status)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    uint8_t* bt_audio_buff = NULL;

    static bool isRun =  false;

    TRACE("app_play_audio_onoff work:%d op:%d aud_id:%d", isRun, onoff, status->aud_id);
//  osDelay(1);

    if (isRun == onoff)
        return 0;
    
    if (onoff ) {
        media_Play_init_audio(status);
        if (!g_app_audio_length){
            app_audio_sendrequest((uint8_t)APP_PLAY_BACK_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0,0);
            return 0;
        }
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_52M);
        af_set_priority(osPriorityHigh);
        app_overlay_select(APP_OVERLAY_A2DP); 
        
        app_audio_mempool_init();
        app_audio_mempool_get_buff(&bt_audio_buff, BT_AUDIO_BUFF_SIZE);
        app_audio_mempool_get_buff((uint8_t **)&app_play_sbc_cache, BT_AUDIO_BUFF_SIZE/2/2);
        media_audio_init();
        
        memset(&stream_cfg, 0, sizeof(stream_cfg));
        
        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
#ifdef __BT_ANC__
        stream_cfg.sample_rate =  AUD_SAMPRATE_48000 ;//AUD_SAMPRATE_44100;
#else
		stream_cfg.sample_rate =  AUD_SAMPRATE_16000 ;//AUD_SAMPRATE_44100;
#endif
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;

        stream_cfg.vol = TGT_VOLUME_LEVEL_WARNINGTONE;

        stream_cfg.handler = app_play_sbc_more_data; //app_play_audio_more_data; //

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        stream_cfg.data_size = APP_AUDIO_PLAYBACK_BUFF_SIZE>BT_AUDIO_BUFF_SIZE ? BT_AUDIO_BUFF_SIZE : APP_AUDIO_PLAYBACK_BUFF_SIZE;
#ifdef __BT_ANC__
		bt_sco_samplerate_ratio = 3;
		us_fir_init();
#endif
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);

#ifdef TWS_RING_SYNC
        TRACE("status->id:%d, aud_id:%d\n",status->id,status->aud_id);
        if((status->id == APP_PLAY_BACK_AUDIO)&&(status->aud_id == AUD_ID_BT_CALL_INCOMING_CALL)){
            uint32_t trig_ticks = 0;
            trig_ticks = tws_media_play_calc_ring_sync_trigger_time();
            TRACE("trig_ticks:%d\n",trig_ticks);

            app_tws_set_trigger_time(trig_ticks);
        }
#endif

    } else {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        
        app_play_sbc_cache = NULL;
        app_overlay_unloadall();        
        af_set_priority(osPriorityAboveNormal);
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun = onoff;

    return 0;
}


void app_play_audio_set_lang(int L)
{
    g_language = L;
}

int app_play_audio_get_lang()
{
    return g_language;
}
#endif

