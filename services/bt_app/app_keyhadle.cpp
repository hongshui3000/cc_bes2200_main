//#include "mbed.h"
#include <stdio.h>
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "app_bt_stream.h"

extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "hid.h"
#include "mei.h"
}

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_key.h"
#include "app_audio.h"

#include "apps.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_bt.h"
#include "app_tws_if.h"
#endif

#include "app_customized_key_handle.h"//Modified by ATX : Leon.He_20171123

extern struct BT_DEVICE_T  app_bt_device;

//@20180304 by parkwe.wei for _PROJ_2000IZ_C003__ UI
#ifdef _PROJ_2000IZ_C003__
static bool peer_right_in=false;
static bool peer_left_in=false;
#endif





//HfCommand hf_command;
//bool hf_mute_flag = 0;
void hfp_handle_key(uint8_t hfp_key)
{
    HfChannel *hf_channel_tmp = NULL;

#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T another_device_id =  (app_bt_device.curr_hf_channel_id == BT_DEVICE_ID_1) ? BT_DEVICE_ID_2 : BT_DEVICE_ID_1;
    TRACE("!!!hfp_handle_key curr_hf_channel=%d\n",app_bt_device.curr_hf_channel_id);
    hf_channel_tmp = (app_bt_device.curr_hf_channel_id == BT_DEVICE_ID_1) ? &(app_bt_device.hf_channel[BT_DEVICE_ID_1]) : &(app_bt_device.hf_channel[BT_DEVICE_ID_2]);

#else
    hf_channel_tmp = &(app_bt_device.hf_channel[BT_DEVICE_ID_1]);
#endif
    switch(hfp_key)
    {
        case HFP_KEY_ANSWER_CALL:
            ///answer a incomming call
            TRACE("avrcp_key = HFP_KEY_ANSWER_CALL\n");
            HF_AnswerCall(hf_channel_tmp,&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;
        case HFP_KEY_HANGUP_CALL:
            TRACE("avrcp_key = HFP_KEY_HANGUP_CALL\n");
            HF_Hangup(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;
        case HFP_KEY_REDIAL_LAST_CALL:
            ///redail the last call
            TRACE("avrcp_key = HFP_KEY_REDIAL_LAST_CALL\n");
            HF_Redial(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;
        case HFP_KEY_CHANGE_TO_PHONE:
            ///remove sco and voice change to phone
            TRACE("avrcp_key = HFP_KEY_CHANGE_TO_PHONE\n");
            HF_DisconnectAudioLink(hf_channel_tmp);
            break;
        case HFP_KEY_ADD_TO_EARPHONE:
            ///add a sco and voice change to earphone
            TRACE("avrcp_key = HFP_KEY_ADD_TO_EARPHONE\n");
            HF_CreateAudioLink(hf_channel_tmp);
            break;
        case HFP_KEY_MUTE:
            TRACE("avrcp_key = HFP_KEY_MUTE\n");
            app_bt_device.hf_mute_flag = 1;
            break;
        case HFP_KEY_CLEAR_MUTE:
            TRACE("avrcp_key = HFP_KEY_CLEAR_MUTE\n");
            app_bt_device.hf_mute_flag = 0;
            break;
        case HFP_KEY_THREEWAY_HOLD_AND_ANSWER:
            TRACE("avrcp_key = HFP_KEY_THREEWAY_HOLD_AND_ANSWER\n");
            HF_CallHold(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],HF_HOLD_HOLD_ACTIVE_CALLS,0,&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;
        case HFP_KEY_THREEWAY_HANGUP_AND_ANSWER:
            TRACE("avrcp_key = HFP_KEY_THREEWAY_HOLD_SWAP_ANSWER\n");
            HF_CallHold(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],HF_HOLD_RELEASE_ACTIVE_CALLS,0,&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;
        case HFP_KEY_THREEWAY_HOLD_REL_INCOMING:
            TRACE("avrcp_key = HFP_KEY_THREEWAY_HOLD_REL_INCOMING\n");
            HF_CallHold(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],HF_HOLD_RELEASE_HELD_CALLS,0,&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            break;            
#ifdef __BT_ONE_BRING_TWO__
        case HFP_KEY_DUAL_HF_HANGUP_ANOTHER:////////double click
            TRACE("avrcp_key = HFP_KEY_DUAL_HF_HANGUP_ANOTHER\n");
            HF_Hangup(&app_bt_device.hf_channel[another_device_id],&app_bt_device.hf_command[another_device_id]);
            break;
        case HFP_KEY_DUAL_HF_HANGUP_CURR_ANSWER_ANOTHER://////////click
            TRACE("avrcp_key = HFP_KEY_DUAL_HF_HANGUP_CURR_ANSWER_ANOTHER\n");
//            HF_Hangup(&app_bt_device.hf_channel[another_device_id],&app_bt_device.hf_command[another_device_id]);
//            HF_AnswerCall(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],&app_bt_device.hf_command[app_bt_device.curr_hf_channel_id]);
            HF_Hangup(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],&app_bt_device.hf_command[app_bt_device.curr_hf_channel_id]);
            HF_AnswerCall(&app_bt_device.hf_channel[another_device_id],&app_bt_device.hf_command[another_device_id]);
            break;
        case HFP_KEY_DUAL_HF_HOLD_CURR_ANSWER_ANOTHER:///////////long press
            TRACE("avrcp_key = HFP_KEY_DUAL_HF_HOLD_CURR_ANSWER_ANOTHER\n");
            //HF_AnswerCall(&app_bt_device.hf_channel[app_bt_device.curr_hf_channel_id],&app_bt_device.hf_command[BT_DEVICE_ID_1]);
            HF_AnswerCall(&app_bt_device.hf_channel[another_device_id],&app_bt_device.hf_command[another_device_id]);
            break;
#endif

        default :
            break;
    }
}

//bool a2dp_play_pause_flag = 0;
void a2dp_handleKey(uint8_t a2dp_key)
{
    AvrcpChannel *avrcp_channel_tmp = NULL;
    if(app_bt_device.a2dp_state[app_bt_device.curr_a2dp_stream_id] == 0)
        return;
    if(app_bt_device.avrcp_channel[app_bt_device.curr_a2dp_stream_id].avrcpState == AVRCP_STATE_DISCONNECTED)
    {
        TRACE("keyhandle avrcp don't connected ");
        return;
    }
#ifdef __BT_ONE_BRING_TWO__
    TRACE("!!!a2dp_handleKey curr_a2dp_stream_id=%d\n",app_bt_device.curr_a2dp_stream_id);
    avrcp_channel_tmp = (app_bt_device.curr_a2dp_stream_id == BT_DEVICE_ID_1) ? &(app_bt_device.avrcp_channel[BT_DEVICE_ID_1]) : &(app_bt_device.avrcp_channel[BT_DEVICE_ID_2]);
#else
    avrcp_channel_tmp = &app_bt_device.avrcp_channel[BT_DEVICE_ID_1];
#endif

#if 0//def __TWS__
    if(app_tws_get_mode() == TWSMASTER){
        avrcp_channel_tmp = app_tws_get_avrcp_TGchannel();
    }
#endif

    switch(a2dp_key)
    {
        case AVRCP_KEY_STOP:
            TRACE("avrcp_key = AVRCP_KEY_STOP");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_STOP,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_STOP,FALSE);
            app_bt_device.a2dp_play_pause_flag = 0;
            break;
        case AVRCP_KEY_PLAY:
            TRACE("avrcp_key = AVRCP_KEY_PLAY");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_PLAY,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_PLAY,FALSE);
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case AVRCP_KEY_PAUSE:
            TRACE("avrcp_key = AVRCP_KEY_PAUSE");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_PAUSE,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_PAUSE,FALSE);
            app_bt_device.a2dp_play_pause_flag = 0;
            break;
        case AVRCP_KEY_FORWARD:
            TRACE("avrcp_key = AVRCP_KEY_FORWARD");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_FORWARD,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_FORWARD,FALSE);
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case AVRCP_KEY_BACKWARD:
            TRACE("avrcp_key = AVRCP_KEY_BACKWARD");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_BACKWARD,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_BACKWARD,FALSE);
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case AVRCP_KEY_VOLUME_UP:
            TRACE("avrcp_key = AVRCP_KEY_VOLUME_UP");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_VOLUME_UP,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_VOLUME_UP,FALSE);
            break;
        case AVRCP_KEY_VOLUME_DOWN:
            TRACE("avrcp_key = AVRCP_KEY_VOLUME_DOWN");
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_VOLUME_DOWN,TRUE);
            AVRCP_SetPanelKey(avrcp_channel_tmp,AVRCP_POP_VOLUME_DOWN,FALSE);
            break;
        default :
            break;
    }
}

//uint8_t phone_earphone_mark = 0;

#if 0
//uint8_t phone_earphone_mark = 0;
uint8_t iic_mask=0;
extern "C" void pmu_sleep_en(unsigned char sleep_en);

void btapp_change_to_iic(void)
{
    const struct HAL_IOMUX_PIN_FUNCTION_MAP pinmux_iic[] = {
        {HAL_IOMUX_PIN_P3_0, HAL_IOMUX_FUNC_I2C_SCL, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
        {HAL_IOMUX_PIN_P3_1, HAL_IOMUX_FUNC_I2C_SDA, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    };

    hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)pinmux_iic, sizeof(pinmux_iic)/sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP));
}

void btapp_change_to_uart0(void)
{
    const struct HAL_IOMUX_PIN_FUNCTION_MAP pinmux_uart0[] = {
        {HAL_IOMUX_PIN_P3_0, HAL_IOMUX_FUNC_UART0_RX, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
        {HAL_IOMUX_PIN_P3_1, HAL_IOMUX_FUNC_UART0_TX, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    };


    hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)pinmux_uart0, sizeof(pinmux_uart0)/sizeof(struct HAL_IOMUX_PIN_FUNCTION_MAP));
}
#endif


#ifdef __EARPHONE_AUDIO_MONITOR__
#include "app_bt_media_manager.h"
uint8_t bt_monitor=0;
#endif

//Modified by ATX : Leon.He_20171123,move func, up and down key handle into customized key handle
#if 0
void bt_key_handle_func_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
#ifndef __BT_ONE_BRING_TWO__
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
#if 0
            if(iic_mask ==0)
            {
                iic_mask=1;
                btapp_change_to_iic();
                
            }
            else if(iic_mask == 1)
            {
                pmu_sleep_en(0);
                iic_mask = 2;
            }
            else
            {
                iic_mask=0;
                
                btapp_change_to_uart0();
                pmu_sleep_en(1);
            }        
            return;
#endif  
            
            if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON)){
                if(app_bt_device.a2dp_play_pause_flag == 0){   /////if no connect?
                    a2dp_handleKey(AVRCP_KEY_PLAY);
                }else{
                    a2dp_handleKey(AVRCP_KEY_PAUSE);
                }
            } else if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE) && (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)){
				TRACE("#### click: Now is three call !!!");
				hfp_handle_key(HFP_KEY_THREEWAY_HANGUP_AND_ANSWER);
			} else if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)){
				TRACE("#### click: only call is incoming !!!");
				hfp_handle_key(HFP_KEY_ANSWER_CALL);
            } else if(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE){
            	TRACE("#### click: only call is incoming !!!");
                hfp_handle_key(HFP_KEY_HANGUP_CALL);
            } else if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_OUT)||(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_ALERT)){
				TRACE("#### click: only call is outcoming !!!");
				hfp_handle_key(HFP_KEY_HANGUP_CALL);
            }
            break;
        case  APP_KEY_EVENT_DOUBLECLICK:
            TRACE("!!!APP_KEY_EVENT_DOUBLECLICK\n");


            
#ifdef __TWS__
            BtAccessibleMode mode;
            app_bt_accessmode_get(&mode);
            if (mode == BT_DEFAULT_ACCESS_MODE_PAIR){
                if (is_find_tws_peer_device_onprocess()){
                    find_tws_peer_device_stop();
#ifdef MEDIA_PLAYER_SUPPORT
                    app_voice_report(APP_STATUS_INDICATION_TWS_STOPSEARCH,0);    
#endif
                }else{          
                    find_tws_peer_device_start();
#ifdef MEDIA_PLAYER_SUPPORT
                    app_voice_report(APP_STATUS_INDICATION_TWS_SEARCH,0);
#endif
                }
            }
            if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)){
                hfp_handle_key(HFP_KEY_REDIAL_LAST_CALL);
            } else if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE) && (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)){
				TRACE("#### click: Now is three call !!!");
				hfp_handle_key(HFP_KEY_THREEWAY_HOLD_REL_INCOMING);
			} 
            if(app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_CON){
                if(app_bt_device.hf_mute_flag == 0){
                    hfp_handle_key(HFP_KEY_MUTE);
                    app_bt_device.hf_mute_flag = 1;
                }else{
                    hfp_handle_key(HFP_KEY_CLEAR_MUTE);
                    app_bt_device.hf_mute_flag = 0;
                }
            }
#endif            
            break;
        case  APP_KEY_EVENT_LONGPRESS:
			if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE) && (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)){
				TRACE("#### long press: Now is three call !!!");
#ifdef MEDIA_PLAYER_SUPPORT
				app_voice_report(APP_STATUS_INDICATION_WARNING, 0);
#endif
				hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
			} else if(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE){
				TRACE("#### long press: two call is active !!!");
#ifdef MEDIA_PLAYER_SUPPORT
				app_voice_report(APP_STATUS_INDICATION_WARNING, 0);
#endif
				hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
#if 0
                if(app_bt_device.phone_earphone_mark == 0){
                    //call is active, switch from earphone to phone
                    hfp_handle_key(HFP_KEY_CHANGE_TO_PHONE);
                }else if(app_bt_device.phone_earphone_mark == 1){
                    //call is active, switch from phone to earphone
                    hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);
                }
#endif
            } else if(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN){
            	TRACE("#### long press: only call is incoming call !!!");
#ifdef MEDIA_PLAYER_SUPPORT
				app_voice_report(APP_STATUS_INDICATION_WARNING, 0);
#endif
                hfp_handle_key(HFP_KEY_HANGUP_CALL);
            }

#if HID_DEVICE == XA_ENABLED
            else if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)){
                Hid_Send_capture(&app_bt_device.hid_channel[BT_DEVICE_ID_1]);
            }
#endif
            break;
        case  APP_KEY_EVENT_TRIPLECLICK:
             if(app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] == 1)
             {
                a2dp_handleKey(AVRCP_KEY_FORWARD);
             }
            
            break;
        case APP_KEY_EVENT_ULTRACLICK:
            if(app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] == 1)
            {
                a2dp_handleKey(AVRCP_KEY_BACKWARD);
            }
            else
            {
#ifdef __EARPHONE_AUDIO_MONITOR__
                if(bt_monitor == 0)
                {
                    bt_monitor = 1;
                    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_MONITOR,0,0,0,0);
                }
                else
                {
                    bt_monitor = 0;
                    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_MONITOR,0,0,0,0);
                }
#endif                
            }
            break;


#else
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
            TRACE("!!!APP_KEY_EVENT_CLICK  callSetup1&2:%d,%d, call1&2:%d,%d, audio_state1&2:%d,%d\n",app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1],app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2],app_bt_device.hfchan_call[BT_DEVICE_ID_1],app_bt_device.hfchan_call[BT_DEVICE_ID_2],app_bt_device.hf_audio_state[BT_DEVICE_ID_1],app_bt_device.hf_audio_state[BT_DEVICE_ID_2]);
            if((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON)&&
                (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE)&&(app_bt_device.hf_audio_state[BT_DEVICE_ID_2] == HF_AUDIO_DISCON)){
                if(app_bt_device.a2dp_play_pause_flag == 0){
                    a2dp_handleKey(AVRCP_KEY_PLAY);
                }else{
                    a2dp_handleKey(AVRCP_KEY_PAUSE);
                }
            }

            if(((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] != HF_CALL_SETUP_ALERT))||
                ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] != HF_CALL_SETUP_ALERT))){
                hfp_handle_key(HFP_KEY_ANSWER_CALL);
            }

            if((((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_OUT)||(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_ALERT))&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE))||
                (((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_OUT)||(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_ALERT))&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE))){
                TRACE("!!!!call out hangup\n");
                hfp_handle_key(HFP_KEY_HANGUP_CALL);
            }

            if(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE){
                if((app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE)){
                    hfp_handle_key(HFP_KEY_HANGUP_CALL);
                }else if(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_IN){
                    hfp_handle_key(HFP_KEY_DUAL_HF_HANGUP_CURR_ANSWER_ANOTHER);
                }
            }else if(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_ACTIVE){
                if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)){
                    hfp_handle_key(HFP_KEY_HANGUP_CALL);
                }else if(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN){
                    hfp_handle_key(HFP_KEY_DUAL_HF_HANGUP_CURR_ANSWER_ANOTHER);
                }
            }

            if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_ACTIVE)){
                TRACE("!!!two call both active\n");
                hfp_handle_key(HFP_KEY_HANGUP_CALL);
            }
            break;
        case  APP_KEY_EVENT_DOUBLECLICK:
            if(((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_CLOSED))||
                ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE))){
                hfp_handle_key(HFP_KEY_REDIAL_LAST_CALL);
            }

            if(((app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_CON)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE))||
                ((app_bt_device.hf_audio_state[BT_DEVICE_ID_2] == HF_AUDIO_CON)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE))){
                if(app_bt_device.hf_mute_flag == 0){
                    hfp_handle_key(HFP_KEY_MUTE);
                }else{
                    hfp_handle_key(HFP_KEY_CLEAR_MUTE);
                }
            }

            if(((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_IN))||
                ((app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN))){
                hfp_handle_key(HFP_KEY_DUAL_HF_HANGUP_ANOTHER);
            }
            break;
        case  APP_KEY_EVENT_LONGPRESS:
            if(((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE))||
                ((app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE))){
                if(app_bt_device.phone_earphone_mark == 0){
                    hfp_handle_key(HFP_KEY_CHANGE_TO_PHONE);
                }else if(app_bt_device.phone_earphone_mark == 1){
                    hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);
                }
            }

            if(((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE))||
                ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE))){
                hfp_handle_key(HFP_KEY_HANGUP_CALL);
            }
#if HID_DEVICE == XA_ENABLED
            if(((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_CLOSED))||
                ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_NONE)&&(app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_NONE))){

                Hid_Send_capture(&app_bt_device.hid_channel[BT_DEVICE_ID_1]);
                Hid_Send_capture(&app_bt_device.hid_channel[BT_DEVICE_ID_2]);
            }
#endif
            
 #if 0
            if((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_2] == HF_CALL_SETUP_IN)){
                hfp_handle_key(HFP_KEY_DUAL_HF_HOLD_CURR_ANSWER_ANOTHER);
            }else if((app_bt_device.hfchan_call[BT_DEVICE_ID_2] == HF_CALL_ACTIVE)&&(app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)){
                hfp_handle_key(HFP_KEY_DUAL_HF_HOLD_CURR_ANSWER_ANOTHER);
            }
#endif
            break;
#endif/* __BT_ONE_BRING_TWO__ */
        default:
            TRACE("unregister func key event=%x",event);
            break;
    }
}

#if defined(__APP_KEY_FN_STYLE_A__)
void bt_key_handle_up_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
#ifdef __TWS__
            a2dp_handleKey(AVRCP_KEY_VOLUME_UP);/* open for press volume up key on slave */
#endif
            app_bt_stream_volumeup();
            btapp_hfp_report_speak_gain();
            btapp_a2dp_report_speak_gain();
            break;
        case  APP_KEY_EVENT_LONGPRESS:
            a2dp_handleKey(AVRCP_KEY_FORWARD);
            break;
        default:
            TRACE("unregister up key event=%x",event);
            break;
    }
}


void bt_key_handle_down_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
#ifdef __TWS__
            a2dp_handleKey(AVRCP_KEY_VOLUME_DOWN);/* open for press volume down key on slave */
#endif
            app_bt_stream_volumedown();
            btapp_hfp_report_speak_gain();
            btapp_a2dp_report_speak_gain();
            break;
        case  APP_KEY_EVENT_LONGPRESS:
            a2dp_handleKey(AVRCP_KEY_BACKWARD);

            break;
        default:
            TRACE("unregister down key event=%x",event);
            break;
    }
}
#else //#elif defined(__APP_KEY_FN_STYLE_B__)
void bt_key_handle_up_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_REPEAT:
//            a2dp_handleKey(AVRCP_KEY_VOLUME_UP);
            app_bt_stream_volumeup();
            btapp_hfp_report_speak_gain();
            btapp_a2dp_report_speak_gain();
#ifdef __TWS__
            //avrcp_set_slave_volume(0,a2dp_volume_get_tws());        
            tws_player_set_a2dp_vol(a2dp_volume_get_tws());
#endif
            break;
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
            a2dp_handleKey(AVRCP_KEY_FORWARD);
            break;
        default:
            TRACE("unregister up key event=%x",event);
            break;
    }
}


void bt_key_handle_down_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_REPEAT:
 //           a2dp_handleKey(AVRCP_KEY_VOLUME_DOWN);
            app_bt_stream_volumedown();
            btapp_hfp_report_speak_gain();
            btapp_a2dp_report_speak_gain();
#ifdef __TWS__
//            avrcp_set_slave_volume(0,a2dp_volume_get_tws());            
            tws_player_set_a2dp_vol(a2dp_volume_get_tws());
#endif
            break;
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
            a2dp_handleKey(AVRCP_KEY_BACKWARD);
            break;
        default:
            TRACE("unregister down key event=%x",event);
            break;
    }
}
#endif
#endif//end of if 0

#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
extern int app_hfp_siri_report();
extern int app_hfp_siri_voice(bool en);

void bt_key_handle_siri_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_UP:
            app_hfp_siri_voice(true);
            break;
        case  APP_KEY_EVENT_LONGPRESS:
            app_hfp_siri_report();
            break;
        case  APP_KEY_EVENT_LONGLONGPRESS:
            app_hfp_siri_voice(false);
            break;
        default:
            TRACE("unregister down key event=%x",event);
            break;
    }
}
#endif


#ifdef __APP_A2DP_SOURCE__

void bt_key_handle_source_func_key(enum APP_KEY_EVENT_T event)
{
    TRACE("%s,%d",__FUNCTION__,event);
    static bool onaudioloop = false;

    switch(event)
    {
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
            app_a2dp_source_find_sink();
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
             onaudioloop = onaudioloop?false:true;
            if (onaudioloop)
                app_audio_sendrequest((uint8_t)APP_A2DP_SOURCE_LINEIN_AUDIO, (uint8_t)APP_BT_SETTING_OPEN, 0, 0);
            else
                app_audio_sendrequest((uint8_t)APP_A2DP_SOURCE_LINEIN_AUDIO, (uint8_t)APP_BT_SETTING_CLOSE, 0, 0);
            break;
        case  APP_KEY_EVENT_LONGPRESS:
            app_a2dp_start_stream();
            break;
        case  APP_KEY_EVENT_LONGLONGPRESS:
            break;
        default:
            TRACE("unregister down key event=%x",event);
            break;
    }    
}
#endif

APP_KEY_STATUS bt_key;

void bt_key_init(void)
{
    bt_key.code = 0xffff;
    bt_key.event = 0xff;
}

void bt_key_send(uint32_t code, uint16_t event)
{
    OS_LockStack();
    if(bt_key.code == 0xffff){
        TRACE("%s code:%d evt:%d",__func__, code, event);
        bt_key.code = code;
        bt_key.event = event;
        OS_NotifyEvm();
    }else{
        TRACE("%s busy",__func__);
    }
    OS_UnlockStack();
}

// @20180228 by parker.wei rename the funciton
#if defined (__TOUCH_KEY__) 
void touch_bt_key_send(uint16_t code, uint16_t event)
{
    OS_LockStack();
    if(bt_key.code == 0xffff){
       // TRACE("%s code:%d evt:%d",__func__, code, event);
        bt_key.code = code;
        bt_key.event = event;
        OS_NotifyEvm();
    }else{
        TRACE("%s busy",__func__);
    }
    OS_UnlockStack();
}

#endif

//@20180304 by parkwe.wei for _PROJ_2000IZ_C003__ UI
#ifdef _PROJ_2000IZ_C003__
bool get_peer_right_in_state(void)
{
	return peer_right_in;
}
bool get_peer_left_in_state(void)
{
	return peer_left_in;
}
void set_peer_right_in_state(bool state)
{
	 	peer_right_in=state;

}
void set_peer_left_in_state(bool state)
{
	    peer_left_in=state;
}



#endif





#ifdef __DIFF_HANDLE_FOR_REMOTE_KEY_
//Modified by ATX : Leon.He_20180103: different handle for local key and remote key
void bt_remote_key_send(uint32_t code, uint16_t event)
{
	bt_key_send(code,event|BT_REMOTE_KEY_MASK);
}
#endif//__DIFF_HANDLE_FOR_REMOTE_KEY_
void bt_key_handle(void)
{
	bool allowed = true;
    OS_LockStack();
    if(bt_key.code != 0xffff)
    {
        TRACE("%s code:%d evt:%d",__func__, bt_key.code, bt_key.event);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
		if((app_get_fake_poweroff_flag()== 1)
				&& (bt_key.code != BTAPP_FUNC_KEY || (bt_key.event & BT_KEY_MASK) != APP_KEY_EVENT_LONGPRESS))
		{
			allowed = false;
		}
#endif
		if(allowed)
		{
			switch(bt_key.code)
			{
				case BTAPP_FUNC_KEY:
#ifdef __APP_A2DP_SOURCE__
					bt_key_handle_source_func_key((enum APP_KEY_EVENT_T)bt_key.event);
#else
					if(app_tws_mode_is_slave())
					{
#ifndef __DISALE_SLAVE_HANDLE_LOCAL_KEY_	//modify by ATX
						if( bt_key_handle_local_func_key(bt_key.event) == false)
#endif
							tws_player_notify_key(bt_key.code, bt_key.event);
					}
					else
						bt_key_handle_func_key(bt_key.event);//Modified by ATX : Leon.He_20180110: change to uint16
#endif
					break;
				case BTAPP_VOLUME_UP_KEY:		
				    if(app_tws_mode_is_slave())   //Modified by ATX : parker.wei_20180305
                        tws_player_notify_key(bt_key.code, bt_key.event);
					else
					 	bt_key_handle_up_key((enum APP_KEY_EVENT_T)bt_key.event);
//Modified by ATX : Leon.He_20171123
#if 0
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
					bt_key_handle_siri_key((enum APP_KEY_EVENT_T)bt_key.event);
#endif
#endif
					break;
				case BTAPP_VOLUME_DOWN_KEY:
				    if(app_tws_mode_is_slave())  //Modified by ATX : parker.wei_20180305
                        tws_player_notify_key(bt_key.code, bt_key.event);
					else
						bt_key_handle_down_key((enum APP_KEY_EVENT_T)bt_key.event);
					break;
				default:
					TRACE("bt_key_handle  undefined key");
					break;
			}
		}

		bt_key.code = 0xffff;
    }    

    OS_UnlockStack();
}
