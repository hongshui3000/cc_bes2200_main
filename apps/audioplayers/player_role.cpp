#include "stdio.h"
#include "cmsis_os.h"
#include "string.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_bootmode.h"
#include "hal_cmu.h"

#include "audioflinger.h"
#include "apps.h"
#include "app_thread.h"
#include "app_key.h"
#include "app_pwl.h"
#include "app_audio.h"
#include "app_overlay.h"
#include "app_battery.h"
#include "app_utils.h"
#include "app_status_ind.h"
#include "besbt.h"
#include "rbplay.h"
#include "pmu.h"
#include "hal_sdmmc.h"

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
#include "btalloc.h"
#include "btapp.h"
#include "app_bt.h"
#include "player_role.h"
#ifdef MEDIA_PLAYER_SUPPORT
#include "resources.h"
#include "app_media_player.h"
#endif
#include "app_bt_media_manager.h"
#include "codec_best2000.h"
#ifdef __TWS__
#include "app_tws.h"
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)

#define ROLE_SWITCH_EVENT (0x01)
static int local_player_role = PLAYER_ROLE_TWS;

typedef enum PLAYER_MODE{
    PLAYER_MODE_SD,
    PLAYER_MODE_LINEIN,
    PLAYER_MODE_BT,
};

extern void app_start_10_second_timer(uint8_t timer_id);
extern void app_bt_accessmode_set(BtAccessibleMode mode);
extern void rb_player_key_stop(void);
extern void app_rbcodec_ctr_play_onoff(bool on );

void app_set_play_role(int role) {
    local_player_role = role ;
}

int app_get_play_role(void) {
    return local_player_role;
}

static osThreadId player_role_switch_tid = NULL;
void app_switch_player_role_key(APP_KEY_STATUS *status, void *param)
{
    if(player_role_switch_tid){
        osSignalSet(player_role_switch_tid, ROLE_SWITCH_EVENT);
    }
    return ;
}

static void player_role_switch_thread(void const *argument);
osThreadDef(player_role_switch_thread, osPriorityHigh, 2048);

static void player_role_switch_thread(void const *argument)
{
#ifdef TWS_RBCODEC_PLAYER   
    static uint8_t  next_mode = PLAYER_MODE_SD;
#else
    static uint8_t  next_mode = PLAYER_MODE_LINEIN;
#endif
    while(1){
        osSignalWait(ROLE_SWITCH_EVENT, osWaitForever);
        {
            TRACE("%s  m:%d",__func__,  next_mode);

            switch(next_mode){
#ifdef TWS_RBCODEC_PLAYER                
                case PLAYER_MODE_SD:
                { //sd
                    app_bt_disconnect_all();
                    osDelay(1500);
                    if(local_player_role == PLAYER_ROLE_LINEIN){
                        app_linein_ctr_play_onoff(false);
                        osDelay(1000);
                    }
#if 0
                   if(app_tws_get_master_stream_state() != TWS_STREAM_START){
                        app_voice_report(APP_STATUS_INDICATION_PLAYER_MODE_SD,  0);
                        osDelay(2000);
                    }
#endif
                    app_rbcodec_ctr_play_onoff(true); 
                    osDelay(2000);
                    app_set_play_role(PLAYER_MODE_SD);
                    next_mode = PLAYER_MODE_LINEIN; 
                }
                break;
#endif
               case PLAYER_MODE_LINEIN:
                { //linein
#ifdef TWS_RBCODEC_PLAYER                     
                    app_rbcodec_ctr_play_onoff(false);
                    osDelay(1000);
#else
                    //app_bt_disconnect_all();
                    //osDelay(2000);
#endif         
                    if(app_tws_get_master_stream_state() != TWS_STREAM_START){
                        app_voice_report(APP_STATUS_INDICATION_PLAYER_MODE_LINEIN,  0);
                        osDelay(2000);
                    }else{
                        //led flash
                    }
                    app_linein_ctr_play_onoff(true);
                    osDelay(2000);
                    app_set_play_role(PLAYER_MODE_LINEIN);

    		    next_mode =PLAYER_MODE_BT;
                }
                break;

                case PLAYER_MODE_BT:
                { //tws

#ifdef TWS_RBCODEC_PLAYER  
    		    next_mode = PLAYER_MODE_SD;
#else
                    next_mode = PLAYER_MODE_LINEIN;
#endif
                    if(bt_media_get_current_media() != BT_STREAM_SBC){
                        app_linein_ctr_play_onoff(false);
                        osDelay(2000);
                        app_voice_report(APP_STATUS_INDICATION_PLAYER_MODE_TWS,  0);
                        osDelay(2000);
                        app_set_play_role(PLAYER_ROLE_TWS);
                        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR,0,0);
                        app_start_10_second_timer(APP_PAIR_TIMER_ID);
                    }else{
                        //led flash
                    }
                } 
                break;
            }
        }
    }
}

void player_role_thread_init(void)
{
    player_role_switch_tid =  osThreadCreate(osThread(player_role_switch_thread), NULL);
}

#endif

