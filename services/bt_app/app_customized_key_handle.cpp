/*
Copyright (c) ATX ShenZhen, Ltd.
*/
#include "app_customized_key_handle.h"

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
}
#include "btalloc.h"
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
#ifdef __FACTORY_MODE_SUPPORT__
#include "app_factory.h"
#include "app_factory_bt.h"
#endif
#include "ddb.h"
#include "nvrecord.h"
#include "nvrecord_env.h" //@20180303 by parker.wei

//Modified by ATX : parker.wei_20180308
int hfp_volume_get(void);


extern struct BT_DEVICE_T  app_bt_device;
extern  tws_dev_t  tws;

#ifdef __MOBILE_DISCONNECTED_NOT_AUTO_ENTER_PARING_ //Modified by ATX : Parke.Wei_20180328
extern bool	manual_enter_pair_mode;
#endif


//Modified by ATX : Parke.Wei_20180316  for peer slave get the master state;
#ifdef _PROJ_2000IZ_C003__
extern APP_STATUS_INDICATION_T remote_master_state;
#endif
extern void app_otaMode_enter(APP_KEY_STATUS *status, void *param);

#ifdef __ALLOW_CLOSE_SIRI__
bool is_siri_open_flag = 0;
#endif
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
extern int app_hfp_siri_voice(bool en);
#endif

static bool isActiveConnections(void)
{
	 int activeCons;
	 OS_LockStack();
	 activeCons = MEC(activeCons);
	 OS_UnlockStack();
	 return activeCons != 0;
}

static bool reset_paired_device_list(void)
{
    if( isActiveConnections() == true)
    {
    	TRACE("Have activeCons\n");
    	return false;
    }
    TRACE("Reset Paired Device List\r\n");
    nv_record_sector_clear();
    DDB_Open(NULL);
    nv_record_env_init();
    nv_record_flash_flush();
    return true;
}

static void stop_tws_adv(void)
{
	struct nvrecord_env_t *nvrecord_env;
	nv_record_env_get(&nvrecord_env);
	if (nvrecord_env->tws_mode.mode == TWSSLAVE) {
		if (app_tws_ble_reconnect.adv_close_func)
			app_tws_ble_reconnect.adv_close_func(APP_TWS_CLOSE_BLE_ADV);
	}
}

//Modified by ATX : Parke.Wei_20180315
static bool start_pairing_and_disconnect_all(void)
{
	BtAccessibleMode mode;
	app_bt_accessmode_get(&mode);
	TRACE("%s enter",__func__);


	if( mode == BT_DEFAULT_ACCESS_MODE_PAIR )
	{
		TRACE("already in pairing mode\n");
		return false;
	}
	
	if(app_tws_ble_reconnect.isConnectedPhone == true )
	{
		TRACE("phone already connected and disconnect the AG\n");
		
#ifdef __MOBILE_DISCONNECTED_NOT_AUTO_ENTER_PARING_ //Modified by ATX : Parke.Wei_20180328
		manual_enter_pair_mode=true;  //enter pairing mode first ,avoid enter connectable when disconnect all finish
#endif	
		app_bt_disconnect_all();
		osDelay(1000);
	}
	
	if (is_find_tws_peer_device_onprocess())
		find_tws_peer_device_stop();
	stop_tws_adv();
	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);

	return true;  


}

static bool slave_enter_pairing_when_pdl_is_null(void)
{
    struct nvrecord_env_t *nvrecord_env;
    struct tws_mode_t *tws_mode;
    TRACE("%s \r\n",__func__);
    nv_record_env_get(&nvrecord_env);
    tws_mode = &(nvrecord_env->tws_mode);
    if(tws_mode->mode == TWSSLAVE)
    {
        TRACE("TWS PDL is not NULL");        
        return false;
    }
    app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
    return true;
}

static bool start_pairing_without_connection(void)
{
	BtAccessibleMode mode;
	app_bt_accessmode_get(&mode);
	TRACE("%s enter",__func__);


	if( mode == BT_DEFAULT_ACCESS_MODE_PAIR )
	{
		TRACE("already in pairing mode\n");
		return false;
	}

	if(app_tws_ble_reconnect.isConnectedPhone == true )
	{
		TRACE("phone already connected\n");
		return false;
	}
#ifndef __ALLOW_ENTER_PAIRING_WHEN_TWS_CONNECTED__  
#ifdef __TWS_RIGHT_AS_MASTER__
#if (!defined(__TWS_CHANNEL_RIGHT__) && !defined(__TWS_CHANNEL_LEFT__)) || defined(__TWS_CHANNEL_LEFT__)
	if( isActiveConnections() == true)
	{
		TRACE("Have activeCons\n");
		return false;
	}
#endif
#else//__TWS_RIGHT_AS_MASTER__
#if (!defined(__TWS_CHANNEL_LEFT__) && !defined(__TWS_CHANNEL_RIGHT__)) || defined(__TWS_CHANNEL_RIGHT__) 
	if( isActiveConnections() == true)
	{
		TRACE("Have activeCons\n");
		return false;
	}
#endif
#endif//__TWS_RIGHT_AS_MASTER__
#endif//__ALLOW_ENTER_PAIRING_WHEN_TWS_CONNECTED__

	TRACE("enter pairing\n");

	if (is_find_tws_peer_device_onprocess())
		find_tws_peer_device_stop();
	stop_tws_adv();
	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
	return true;
}

static bool start_TWS_pairing_without_connection(void)
{
	TRACE("%s enter",__func__);

	if( isActiveConnections() == true)
	{
		TRACE("Have activeCons\n");
		return false;
	}

	TRACE("enter TWS pairing\n");

	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_TWS_PAIR, 0,0);
	return true;
}

//Modified by ATX : Parke.Wei_20180315
static bool start_tws_searching_and_stop_paring(void)
{
	TRACE("%s enter",__func__);
    BtAccessibleMode mode;
    app_bt_accessmode_get(&mode);

    //when in paring ,stop it then start TWS searching

	if( isActiveConnections() == true)
	{
		TRACE("Have activeCons\n");
		return false;
	}


    if (mode == BT_DEFAULT_ACCESS_MODE_PAIR)
		 app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);


	if (is_find_tws_peer_device_onprocess() == false)
	{
		find_tws_peer_device_start();
#ifdef MEDIA_PLAYER_SUPPORT
		app_voice_report(APP_STATUS_INDICATION_TWS_SEARCH,0);
#endif
		app_status_indication_set(APP_STATUS_INDICATION_TWS_SEARCH);
	}
	return true;



}

static bool start_tws_searching(void)
{
    BtAccessibleMode mode;
    app_bt_accessmode_get(&mode);

	TRACE("%s enter",__func__);

    if (mode != BT_DEFAULT_ACCESS_MODE_PAIR)
        return false;

	if (is_find_tws_peer_device_onprocess())
	{
		find_tws_peer_device_stop();
#ifdef MEDIA_PLAYER_SUPPORT
		app_voice_report(APP_STATUS_INDICATION_TWS_STOPSEARCH,0);
#endif
	}
	else
	{
		find_tws_peer_device_start();
#ifdef MEDIA_PLAYER_SUPPORT
		app_voice_report(APP_STATUS_INDICATION_TWS_SEARCH,0);
#endif
	}
    return true;
}

static bool start_tws_searching_without_connection(void)
{
    BtAccessibleMode mode;
    app_bt_accessmode_get(&mode);

	TRACE("%s enter",__func__);

	if( isActiveConnections() == true)
	{
		TRACE("Have activeCons\n");
		return false;
	}

	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 2,0);
    return true;
}

static bool handle_play_pause(void)
{
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
	{
		if (app_bt_device.a2dp_play_pause_flag == 0)
			a2dp_handleKey(AVRCP_KEY_PLAY);
		else
			a2dp_handleKey(AVRCP_KEY_PAUSE);
		return true;
	}
	return false;
}

//20180304 by parker.wei for seperate play/pause action
static bool handle_a2dp_pause(void)
{
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
	{
		if (app_bt_device.a2dp_play_pause_flag == 1)
			a2dp_handleKey(AVRCP_KEY_PAUSE);
		return true;
	}
	return false;
}
static bool handle_a2dp_play(void)
{
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
	{
		if (app_bt_device.a2dp_play_pause_flag == 0)
			a2dp_handleKey(AVRCP_KEY_PLAY);
		return true;
	}
	return false;
}

//Modified by ATX : Parke.Wei_20180316
#ifdef _PROJ_2000IZ_C003__
static bool handle_master_Weared(void)
{
    BtAccessibleMode mode;
	struct nvrecord_env_t *nvrecord_env;
	app_bt_accessmode_get(&mode);

	set_peer_right_in_state(true);
	nv_record_env_get(&nvrecord_env);
	TRACE("peer R in-ear");
//is AG connected
	if(app_bt_mobile_hasConnected())
	{ 
	   app_voice_report(APP_STATUS_INDICATION_CONNECTED,0); 
	  //tws connected
		if(nvrecord_env->tws_mode.mode == TWSMASTER && app_tws_get_mode() == TWSMASTER) 
		{
			if (get_peer_left_in_state())
			{
				TRACE("auto play music");
			     if (handle_a2dp_play()==true)
					return true;
			}	
		}
		else //only AG connected
		{
			TRACE("NO TWS,auto play music");
			 if (handle_a2dp_play()==true)
			 	return true;   
		}
	}

	if (mode == BT_DEFAULT_ACCESS_MODE_PAIR) //when in paring mode, in-ear auto play the vp
	{
		app_voice_report(APP_STATUS_INDICATION_BOTHSCAN,0); 
		return true;  

    }

	return false;  


}
static bool handle_master_Unweared(void)
{
	struct nvrecord_env_t *nvrecord_env;
	nv_record_env_get(&nvrecord_env);
//is AG connected
	if(app_bt_mobile_hasConnected())
	{			 
	  //tws connected
		if(nvrecord_env->tws_mode.mode == TWSMASTER && app_tws_get_mode() == TWSMASTER) 
		{
			if (get_peer_left_in_state())
			{
				TRACE("peer R out-ear auto pause music");
				   set_peer_right_in_state(false);
				 if (handle_a2dp_pause()==true)
					return true;
			}	
		}
		else //only AG connected
		{
			TRACE("NO TWS,R out-ear auto pause music");
			 if (handle_a2dp_pause()==true)
				return true;   
		}		  
	}
       return false;

}

static bool handle_remote_slave_Weared(void)
{
	set_peer_left_in_state(true);
	TRACE("peer L in-ear");
	if (get_peer_right_in_state())
	{
		TRACE("auto play music");
		 if (handle_a2dp_play()==true)
			return true;
	
	}	

	return false;  
}
static bool handle_remote_slave_Unweared(void)
{
	if (get_peer_right_in_state()) 
	{
		TRACE("peer L out-ear auto pause music");
		 set_peer_left_in_state(false);    
		 if (handle_a2dp_pause()==true)
			return true;		
	}	

	return false;  
}


static bool handle_slave_Weared(void)
{
	if(remote_master_state==APP_STATUS_INDICATION_CONNECTED)	
	{
		app_voice_report(APP_STATUS_INDICATION_CONNECTED,0); 
		return true;  		
	}

	return false;  
}


#endif


static bool handle_a2dp_skipf(void)
{
#ifdef __AUDIO_SKIP_ONLY_WHEN_STREAMING_
	if (app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] == 1)
	{
		a2dp_handleKey(AVRCP_KEY_FORWARD);
		return true;
	}
#else
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
	{
		a2dp_handleKey(AVRCP_KEY_FORWARD);
		return true;
	}
#endif
	return false;
}

static bool handle_a2dp_skipr(void)
{
#ifdef __AUDIO_SKIP_ONLY_WHEN_STREAMING_
	if (app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] == 1)
	{
		a2dp_handleKey(AVRCP_KEY_BACKWARD);
		return true;
	}
#else
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
	{
		a2dp_handleKey(AVRCP_KEY_BACKWARD);
		return true;
	}
#endif
	return false;
}

void master_sync_vol_to_phone_and_slave(uint8_t vol_type)
{
	switch (vol_type) {
	case A2DP_VOL_TYPE:
//TRACE("sync a2dp gain\n");
		if(tws.tws_mode == TWSMASTER)
		{
		    tws_player_a2dp_vol_change(a2dp_volume_get_tws());
		    btapp_a2dp_report_speak_gain();
		}
		else if(tws.tws_mode == TWSON)
            btapp_a2dp_report_speak_gain();
		break;
	case HFP_VOL_TYPE:
//TRACE("sync hfp gain\n");
		if(tws.tws_mode == TWSMASTER)
		{
		    tws_player_hfp_vol_change(hfp_volume_get());
		    btapp_hfp_report_speak_gain();
		}
		else if(tws.tws_mode == TWSON)
            btapp_hfp_report_speak_gain();
		break;
	default:
		break;
	}
}

static bool handle_vol_up(void )
{
    uint8_t vol_type;
	TRACE("%s enter",__func__);
	if( isActiveConnections() == false)
		return false;
	if (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE
			|| app_bt_device.a2dp_play_pause_flag == 1)
	{
		vol_type = app_bt_stream_volumeup();
		switch(vol_type)
		{
			case A2DP_VOL_TYPE:
				if (app_bt_stream_local_volume_get() >= TGT_VOLUME_LEVEL_15)
					app_voice_report(APP_STATUS_INDICATION_VOL_MAX,0);
				break;
			case HFP_VOL_TYPE:
				if (app_bt_stream_local_volume_get() >= TGT_VOLUME_LEVEL_15)
					app_voice_report(APP_STATUS_INDICATION_VOL_MAX,0);
				 break;
			default:
				break;
		}
		master_sync_vol_to_phone_and_slave(vol_type);
	}
	return true;
}

static bool handle_vol_down(void )
{
    uint8_t vol_type;
	TRACE("%s enter",__func__);
	if( isActiveConnections() == false)
		return false;
	if (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE
			|| app_bt_device.a2dp_play_pause_flag == 1)
	{
		vol_type = app_bt_stream_volumedown();
		switch(vol_type)
		{
			case A2DP_VOL_TYPE:
				if (app_bt_stream_local_volume_get()<= TGT_VOLUME_LEVEL_MUTE)
					app_voice_report(APP_STATUS_INDICATION_VOL_MIN,0);
				break;
			case HFP_VOL_TYPE:
				if (app_bt_stream_local_volume_get() <= TGT_VOLUME_LEVEL_0)
					app_voice_report(APP_STATUS_INDICATION_VOL_MIN,0);
				 break;
			default:
				break;
		}
		master_sync_vol_to_phone_and_slave(vol_type);
	}
	return true;
}

#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
static bool handle_threeway_call_end_and_answer(void)
{
	if ((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)
			&& (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN))
	{
		hfp_handle_key(HFP_KEY_THREEWAY_HANGUP_AND_ANSWER);
		return true;
	}
	return false;
}
#endif

static bool handle_hfp_call_answer(void)
{
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE))
	{
		hfp_handle_key(HFP_KEY_ANSWER_CALL);
		return true;
	}
	return false;
}

//Modified by ATX : Parke.Wei_20180319
static bool handle_hfp_trasnfer_audio(void)
{
	TRACE("%s enter",__func__);

	if (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)
	{
	    if(app_bt_device.phone_earphone_mark==0)
	    {
		   	TRACE("  CHANGE_TO_AG"); 
			hfp_handle_key(HFP_KEY_CHANGE_TO_PHONE);
	    }
		else{
			TRACE("  CHANGE_TO_HEADSET"); 
			hfp_handle_key(HFP_KEY_ADD_TO_EARPHONE);

		}
		return true;
	}
	     
	return false;
}



static bool handle_hfp_call_hung_up(void)
{
	if (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)
	{
		hfp_handle_key(HFP_KEY_HANGUP_CALL);
		return true;
	}
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_OUT)
			|| (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_ALERT))
	{
		hfp_handle_key(HFP_KEY_HANGUP_CALL);
		return TRUE;
	}
	return false;
}

#ifdef _SIRI_ENABLED__
static bool handle_hfp_enable_siri(void)
{
#ifndef __OPEN_SIRI_WHEN_CALL__
	if ((app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
			&& (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			&& (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_DISCON))
#endif
	{
#ifdef __ALLOW_CLOSE_SIRI__
	          is_siri_open_flag = 1;
#endif
		 app_hfp_siri_voice(true);
		return true;
	}
	return false;
}
#endif
//Modified by ATX : Parke.Wei_20180322   for all key_handle to call
static bool handle_hfp_call_reject(void)
{
	if (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN)
	{
		TRACE("handle_func_key_longpress:1st call incoming, reject it\n");
		app_voice_report(APP_STATUS_INDICATION_SHORT_1, 0);
		hfp_handle_key(HFP_KEY_HANGUP_CALL);
		return true;
	}
	return false;
}

static bool handle_func_key_click(void)
{
	bool handled = false;

	TRACE("%s enter",__func__);

#ifdef __FUNCTION_KEY_CLICK_FOR_TWS_SEARCHING_
	if (start_tws_searching() == true )
       return true;
#endif

//Modified by ATX : Parke.Wei_20180322
#ifdef __FUNCTION_KEY_CLICK_FOR_TWS_SEARCHING_STOP_PARING_
	if (start_tws_searching_and_stop_paring() == true )
		return true;
#endif

#ifdef __FUNCTION_KEY_CLICK_FOR_TWS_PAIRING_
	if (start_TWS_pairing_without_connection() == true )
		return true;
#endif


#ifdef __FUNCTION_KEY_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif

#ifdef __FUNCTION_KEY_CLICK_FOR_SKIPR_
        if (handle_a2dp_skipr()==true)
            return true;
#endif

//Modified by ATX : Parke.Wei_20180315
#ifdef __FUNCTION_KEY_CLICK_FOR_SKIPF_
		if (handle_a2dp_skipf()== true)
			return true;
#endif

#ifdef __FUNCTION_KEY_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif//__DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif//__FUNCTION_KEY_CLICK_FOR_HF_CALL_

#ifdef __FUNCTION_KEY_CLICK_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
#ifdef __FUNCTION_KEY_CLICK_FOR_VOL_UP_
	handled = handle_vol_up();
#endif

	return handled;
}

static bool handle_func_key_doubleclick(void)
{
	bool handled = false;
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_PAIRING_
	if (start_pairing_without_connection() == true )
        return true;
#endif//__FUNCTION_KEY_DOUBLE_CLICK_FOR_PAIRING_

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif//__DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif//__FUNCTION_KEY_DOUBLE_CLICK_FOR_HF_CALL_

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_SLAVE_ENTER_PAIRING_
	if (slave_enter_pairing_when_pdl_is_null() == true )
	{
	    app_status_indication_set(APP_STATUS_INDICATION_SLAVE_PAIRING);
	    return true;
	}
        
#endif//__FUNCTION_KEY_DOUBLE_CLICK_FOR_SLAVE_ENTER_PAIRING_

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_TWS_PAIRING_
	if (start_TWS_pairing_without_connection() == true )
        return true;
#endif//__FUNCTION_KEY_DOUBLE_CLICK_FOR_TWS_PAIRING_

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_TWS_SEARCHING_
	if (start_tws_searching() == true )
       return true;
#endif
//Modified by ATX : Parke.Wei_20180322
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_HFP_REJECT_CALL_
	if (handle_hfp_call_reject() == true)
		return true;
#endif

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_SKIPF_
#if defined(__DOUBLE_CLICK_FOR_LAST_NUMBER_REDDIAL_) || defined(__FUNCTION_KEY_DOUBLE_CLICK_FOR_VOICE_DIAL_)
	if(app_bt_device.a2dp_play_pause_flag == 1)
#endif
	{
		if (handle_a2dp_skipf()== true)
			return true;
	}
#endif
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_SKIPR_
#if defined(__DOUBLE_CLICK_FOR_LAST_NUMBER_REDDIAL_) || defined(__FUNCTION_KEY_DOUBLE_CLICK_FOR_VOICE_DIAL_)
	if(app_bt_device.a2dp_play_pause_flag == 1)
#endif
	{
		if (handle_a2dp_skipr()== true)
			return true;
	}
#endif
#if defined(__FUNCTION_KEY_DOUBLE_CLICK_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
            if (handle_hfp_enable_siri()== true)
		return true;
#endif
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_LAST_NUMBER_REDDIAL_
		 if((isActiveConnections() == true)
				 && (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_NONE)
				 && (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_NONE)
			 	 && app_bt_device.a2dp_play_pause_flag == 0)
		 {
			 hfp_handle_key(HFP_KEY_REDIAL_LAST_CALL);
			 return true;
		 }
#endif//__DOUBLE_CLICK_FOR_LAST_NUMBER_REDDIAL_
#if defined(__FUNCTION_KEY_DOUBLE_CLICK_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
		if (handle_hfp_enable_siri()== true)
			return true;
#endif

#ifdef __FUNCTION_KEY_DOUBLE_CLICK_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
	return handled;
}

#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
static bool handle_threeway_call_hold_and_answer(void)
{
	if ((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)
			&& (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN))
	{
		TRACE("handle_func_key_longpress:2nd call incoming, hold current and answer it\n");
		app_voice_report(APP_STATUS_INDICATION_SHORT_1, 0);
		hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
		return true;
	}
	return false;
}

//Modified by ATX : Parke.Wei_20180315  reject the threeway incoming
static bool handle_threeway_call_release_held(void)
{
	if ((app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE)
			&& (app_bt_device.hfchan_callSetup[BT_DEVICE_ID_1] == HF_CALL_SETUP_IN))
	{
		TRACE("handle_func_key_longpress:2nd call incoming, hold current and reject it\n");
		app_voice_report(APP_STATUS_INDICATION_SHORT_1, 0);
		hfp_handle_key(HFP_KEY_THREEWAY_HOLD_REL_INCOMING);
		return true;
	}
	return false;
}

static bool handle_threeway_call_switch_to_another(void)
{
	if (app_bt_device.hfchan_call[BT_DEVICE_ID_1] == HF_CALL_ACTIVE &&  app_bt_device.hf_call_held == HF_CALL_HELD_ACTIVE)
	{
		TRACE("handle_func_key_longpress:one call active, another one waiting, switch to another one\n");
		app_voice_report(APP_STATUS_INDICATION_SHORT_1, 0);
		hfp_handle_key(HFP_KEY_THREEWAY_HOLD_AND_ANSWER);
		return true;
	}
	return false;
}
#endif//__DISABLE_THREEWAY_CALL_CONTROL_


static bool handle_func_key_longpress_for_hfp(void)
{
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
//Modified by ATX : Parke.Wei_20180315
#ifdef __THREEWAY_RELEASE_ALL_HELD_
	if (handle_threeway_call_release_held() == true)
		return true;
#endif

	if (handle_threeway_call_hold_and_answer() == true)
		return true;

	if (handle_threeway_call_switch_to_another() == true)
		return true;
#endif//__DISABLE__THREEWAY_CALL_CONTROL_

#ifdef __FUNCTION_KEY_LONG_PRESS_FOR_HFP_REJECT_CALL_
	if (handle_hfp_call_reject() == true)
		return true;
#endif

//Modified by ATX : Parke.Wei_20180319
#ifdef __FUNCTION_KEY_LONG_PRESS_FOR_HFP_TRANSFER_AUDIO_
	 if(handle_hfp_trasnfer_audio()==true)
	 	return true;
#endif	 


	return false;
}

static bool handle_func_key_longpress(void)
{
    TRACE("%s\r\n",__func__);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
	if(app_get_fake_poweroff_flag()== 1)
	{
		app_poweron_from_fake_poweroff();
		return true;
	}
#endif
#ifndef __DISABLE_FUNC_KEY_FOR_HF_CALL_
	if (handle_func_key_longpress_for_hfp() == true)
		return true;
#endif

#if defined(__FUNCTION_KEY_LONG_PRESS_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
	if (handle_hfp_enable_siri()== true)
		return true;
#endif


#ifdef __FUNCTION_KEY_LONG_PRESS_FOR_SKIPF_
	if (handle_a2dp_skipf()== true)
		return true;
#endif

#ifdef __FUNCTION_KEY_LONG_PRESS_FOR_SKIPR_
    if (handle_a2dp_skipr()==true)
        return true;
#endif

	return false;
}

static bool handle_func_key_longlongpress(void)
{
    TRACE("%s\r\n",__func__);
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_HFP_REJECT_CALL_
	if (handle_hfp_call_reject() == true)
		return true;
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_PAIRING_
	if (start_pairing_without_connection() == true )
       return true;
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_TWS_PAIRING_
	if (start_TWS_pairing_without_connection() == true )
       return true;
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_TWS_SEARCHING_
	if (start_tws_searching_without_connection() == true )
       return true;
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_PAIRING_DIS	
	if(start_pairing_and_disconnect_all()==true)
		return true;
#endif

#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_FOR_OTA_//Modified by ATX : Parke.Wei_20180413
	app_otaMode_enter(NULL,NULL);
		return true;
#endif

	return false;
}

static bool handle_func_key_tripleclick(void)
{
    TRACE("enter %s ",__func__);


#ifdef __FUNCTION_KEY_TRIPLE_CLICK_FOR_SKIPR_
    if (handle_a2dp_skipr()==true)
        return true;
#endif
#if defined(__FUNCTION_KEY_TRIPLE_CLICK_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
#ifdef __ALLOW_CLOSE_SIRI__
	if(is_siri_open_flag == 0)
        {
            if (handle_hfp_enable_siri()== true)
		return true;
        }
        else 
        {
            is_siri_open_flag = 0;
            app_hfp_siri_voice(false);
            return true;
        }
#else 
    if (handle_hfp_enable_siri()== true)
        return true;
#endif
#endif
#ifdef __FUNCTION_KEY_TRIPLE_CLICK_CLEAR_PHONE_RECORD_
    if(app_clear_phone_paired_record()==true)
    {
        app_status_indication_set(APP_STATUS_INDICATION_CLEARSUCCEED);
        return true;
    }
        
#endif
//@20180304 by parker.wei for _PROJ_2000IZ_C003__ UI
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
	if(handle_master_Weared()==true)
		return true;  		
#endif

	//@20180316 by parker.wei for _PROJ_2000IZ_C003__ UI
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_LEFT__)
	if(handle_slave_Weared()==true)
		return false;  		//must return fasle,slave need notify the KEY to master
	
#endif

#ifdef __FUNCTION_KEY_TRIPLE_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif//__DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif//__FUNCTION_KEY_TRIPLE_CLICK_FOR_HF_CALL_

#ifdef __FUNCTION_KEY_TRIPLE_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif

#ifdef __FUNCTION_KEY_TRIPLE_CLICK_FOR_TWS_SEARCHING_STOP_PARING_
	if (start_tws_searching_and_stop_paring() == true )
		return true;
#endif

#ifdef __FUNCTION_KEY_TRIPLE_CLICK__FOR_TWS_PAIRING_
	if (start_TWS_pairing_without_connection() == true )
		return true;
#endif


	return false;
}

static bool handle_func_key_ultraclick(void)
{
    TRACE("enter %s ",__func__);
	//@20180304 by parker.wei for _PROJ_2000IZ_C003__ UI
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
	struct nvrecord_env_t *nvrecord_env;
	
#endif

#ifdef __FUNCTION_KEY_ULTRA_CLICK_FOR_PAIRING_
	if (start_pairing_without_connection() == true )
       return true;
#endif

//@20180304 by parker.wei for _PROJ_2000IZ_C003__ UI
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
     if(handle_master_Unweared()==true)
	 	return true;
		
#endif

	return false;
}

//Modified by ATX : Parke.Wei_20180413
static bool handle_func_key_rampageclick(void)
{
    TRACE("enter %s ",__func__);
	BtAccessibleMode mode;
	app_bt_accessmode_get(&mode);

	if( mode == BT_DEFAULT_ACCESS_MODE_PAIR )
	{
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
		tws_mastet_ctrl_slave_enter_ota(0);
#endif
		return true;
	}



	return false;
}




#ifdef _PROJ_2000IZ_C001__
#define VLONGPRESS_INTERVEL                   (3000)
#define VVLONGPRESS_INTERVEL                (5000)
#endif

#ifndef VLONGPRESS_INTERVEL
#define VLONGPRESS_INTERVEL                   (2500)
#endif
#ifndef VVLONGPRESS_INTERVEL
#define VVLONGPRESS_INTERVEL                (5000)
#endif
//Modified by ATX : cc_20180312: REPEAT_CNT=(INTERVEL-SW_KEY_LPRESS_THRESH_MS)/CFG_SW_KEY_REPEAT_THRESH_MS
#define VLONGPRESS_REPEAT_CNT                ((VLONGPRESS_INTERVEL-CFG_SW_KEY_LPRESS_THRESH_MS) / CFG_SW_KEY_REPEAT_THRESH_MS) 
#define VVLONGPRESS_REPEAT_CNT             ((VVLONGPRESS_INTERVEL-CFG_SW_KEY_LPRESS_THRESH_MS) / CFG_SW_KEY_REPEAT_THRESH_MS) 

static bool handle_func_key_repeat(void)
{
    bool handled = false;
    static uint32_t time = 0;
    static uint16_t cnt = 0;
    if (TICKS_TO_MS(hal_sys_timer_get() - time) > CFG_SW_KEY_REPEAT_THRESH_MS)      
        cnt = 0;
    else
        cnt++;
    TRACE("%s enter ,cnt:%d",__func__,cnt);
#ifdef __FUNCTION_KEY_REPEAT_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
#ifdef __FUNCTION_KEY_REPEAT_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
    time = hal_sys_timer_get();
    return handled;
}

static bool handle_up_key_click(void)
{
	bool handled = false;
#ifdef __UP_KEY_FOR_MUSIC_CTRL_
	if (handle_play_pause())
		return true;
#endif
#ifdef __UP_KEY_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
#ifdef __UP_KEY_FOR_VOL_UP_
	handled = handle_vol_up();
#endif

	return handled;
}

static bool handle_up_key_double_click(void)
{
	bool handled = false;
#ifdef __UP_KEY_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
#ifdef __UP_KEY_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
#ifdef __UP_KEY_DOUBLE_CLICK_FOR_PAIRING_
    handled = start_pairing_and_disconnect_all();
#endif
    return handled;
}

static bool handle_up_key_triple_click(void)
{
    bool handled = false;
#ifdef __UP_KEY_TRIPLE_CLICK_FOR_RESET_PAIRED_DEVICE_LIST_
    handled = reset_paired_device_list();
#endif
	return handled;
}

static bool handle_down_key_click(void)
{
	bool handled = false;
#ifdef __DOWN_KEY_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
#ifdef __DOWN_KEY_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
	return handled;
}

static bool handle_down_key_double_click(void)
{
	bool handled = false;
#ifdef __DOWN_KEY_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
#ifdef __DOWN_KEY_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
#ifdef __DOWN_KEY_DOUBLE_CLICK_FOR_PAIRING_
		handled = start_pairing_and_disconnect_all();
#endif
	return handled;
}


static bool handle_down_key_triple_click(void)
{
    bool handled = false;
#ifdef __DOWN_KEY_TRIPLE_CLICK_FOR_RESET_PAIRED_DEVICE_LIST_
    handled = reset_paired_device_list();
#endif
	return handled;
}


static bool handle_up_key_longpress(void)
{

#ifdef __UP_KEY_FOR_SKIPF_
	if (handle_a2dp_skipf()== true)
		return true;
#endif
#ifdef __UP_KEY_FOR_SKIPR_
	if (handle_a2dp_skipr()==true)
		return true;
#endif

//Modified by ATX : Parke.Wei_20180315
#ifdef __LONG_PRESS_UP_KEY_FOR_TWS_SEARCHING_STOP_PARING_
	if (start_tws_searching_and_stop_paring() == true )
		return true;
#endif


#ifdef __LONG_PRESS_UP_KEY_FOR_TWS_SEARCHING_
	if (start_tws_searching() == true )
		return true;
#endif

#ifdef __LONG_PRESS_UP_KEY_FOR_TWS_PAIRING_
	if (start_TWS_pairing_without_connection() == true )
		return true;
#endif
#ifdef __UP_KEY_LONG_PRESS_FOR_ENTER_DUT_
    app_factorymode_enter();
#endif
	return true;
}

static bool handle_down_key_longpress(void)
{
#ifdef __DOWN_KEY_FOR_SKIPF_
	if (handle_a2dp_skipf()== true)
		return true;
#endif
#ifdef __DOWN_KEY_FOR_SKIPR_
    if (handle_a2dp_skipr()==true)
        return true;
#endif
#ifdef __DOWN_KEY_LONG_PRESS_FOR_ENTER_DUT_
		app_factorymode_enter();
#endif
#ifdef __DOWN_KEY_LONG_PRESS_FOR_PAIRING_
	   if(start_pairing_and_disconnect_all()==true)
	   	  return true;
#endif

	return true;
}

#ifdef __DIFF_HANDLE_FOR_REMOTE_KEY_
static bool handle_remote_func_key_click(void)
{
	bool handled = false;
	TRACE("%s enter",__func__);

#ifdef __FUNCTION_REMOTE_KEY_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_CLICK_FOR_SKIPF_
	if (handle_a2dp_skipf()== true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_CLICK_FOR_SKIPR_
	if (handle_a2dp_skipr()==true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif

	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif

#ifdef __FUNCTION_REMOTE_KEY_CLICK_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
	return handled;
}

static bool handle_remote_func_key_doubleclick(void)
{
	bool handled = false;
	TRACE("%s enter",__func__);
#ifdef __FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_SKIPF_
	if (handle_a2dp_skipf()== true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_SKIPR_
	if (handle_a2dp_skipr()== true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif

	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif
#if defined(__FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
	if (handle_hfp_enable_siri()== true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_DOUBLE_CLICK_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
	return handled;
}

static bool handle_remote_func_key_tripleclick(void)
{
	TRACE("%s enter",__func__);
#ifdef __FUNCTION_REMOTE_KEY_TRIPLE_CLICK_FOR_SKIPR_
    if (handle_a2dp_skipr()==true)
        return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_TRIPLE_CLICK_FOR_HF_CALL_
#ifndef __DISABLE_THREEWAY_CALL_CONTROL_
	if (handle_threeway_call_end_and_answer()==true)
		return true;
#endif

	if (handle_hfp_call_answer()==true)
		return true;

	if (handle_hfp_call_hung_up()==true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_TRIPLE_CLICK_FOR_MUSIC_CTRL_
	if (handle_play_pause()==true)
		return true;
#endif
#if defined(__FUNCTION_REMOTE_KEY_TRIPLE_CLICK_FOR_VOICE_DIAL_) && defined(_SIRI_ENABLED__)
#ifdef __ALLOW_CLOSE_SIRI__
        if(is_siri_open_flag == 0)
        {
            if (handle_hfp_enable_siri()== true)
		return true;
        }
        else 
        {
            is_siri_open_flag = 0;
            app_hfp_siri_voice(false);
            return true;
        }
#else 
    if (handle_hfp_enable_siri()== true)
        return true;
#endif

#endif
	//@20180304 by parker.wei for _PROJ_2000IZ_C003__ UI
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
	if(handle_remote_slave_Weared()==true)
		return true;
#endif

	return false;

}

//@20180304 by parker.wei for _PROJ_2000IZ_C003__ UI
static bool handle_remote_func_key_ultraclick(void)
{
	TRACE("%s enter",__func__);
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
	if(handle_remote_slave_Unweared()==true)
		return true;
#endif

	return false;
}


static bool handle_remote_func_key_longpress(void)
{
	TRACE("%s enter",__func__);
#ifdef __FUNCTION_REMOTE_KEY_LONGPRESS_FOR_HF_CALL_
	if (handle_func_key_longpress_for_hfp() == true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_LONG_PRESS_FOR_HFP_REJECT_CALL_
	if (handle_hfp_call_reject() == true)
		return true;
#endif
	return false;
}

//Modified by ATX : Parke.Wei_20180315
static bool handle_remote_func_key_longlongpress(void)
{
	TRACE("%s enter",__func__);
#ifdef __FUNCTION_REMOTE_KEY_LONG_LONG_PRESS_FOR_HFP_REJECT_CALL_
	if (handle_hfp_call_reject() == true)
		return true;
#endif
#ifdef __FUNCTION_REMOTE_KEY_LONG_LONG_PRESS_FOR_PAIRING_DIS	
	if(start_pairing_and_disconnect_all()==true)
		return true;
#endif

	return false;
}

static bool handle_remote_func_key_repeat(void)
{
	bool handled = false;
	TRACE("%s enter",__func__);
#ifdef __FUNCTION_REMOTE_KEY_REPEAT_FOR_VOL_UP_
	handled = handle_vol_up();
#endif
#ifdef __FUNCTION_REMOTE_KEY_REPEAT_FOR_VOL_DOWN_
	handled = handle_vol_down();
#endif
	return false;
}

static void handle_remote_func_key(uint16_t event)
{
	switch (event) {
		case APP_KEY_EVENT_UP:
		case APP_KEY_EVENT_CLICK:
			handle_remote_func_key_click();
			break;
		case APP_KEY_EVENT_DOUBLECLICK:
			handle_remote_func_key_doubleclick();
			break;
		case APP_KEY_EVENT_LONGPRESS:
			handle_remote_func_key_longpress();
			break;
		case APP_KEY_EVENT_LONGLONGPRESS:
			handle_remote_func_key_longlongpress();
			break;
		case APP_KEY_EVENT_TRIPLECLICK:
			handle_remote_func_key_tripleclick();
			break;
		case APP_KEY_EVENT_ULTRACLICK:
			handle_remote_func_key_ultraclick();			
			break;
        case  APP_KEY_EVENT_REPEAT:
        	handle_remote_func_key_repeat();
        	break;
		default:
			;
			break;
	}
}
#endif//__DIFF_HANDLE_FOR_REMOTE_KEY_
static bool handle_local_func_key(uint16_t event)
{
	bool handled = false;

	switch (event) {
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
        	handled = handle_func_key_click();
        	break;
        case  APP_KEY_EVENT_DOUBLECLICK:
        	handled = handle_func_key_doubleclick();
            break;
        case  APP_KEY_EVENT_LONGPRESS:
        	handled = handle_func_key_longpress();
			break;
        case  APP_KEY_EVENT_LONGLONGPRESS:
        	handled = handle_func_key_longlongpress();
        	break;
        case  APP_KEY_EVENT_TRIPLECLICK:
        	handled = handle_func_key_tripleclick();
            break;
        case APP_KEY_EVENT_ULTRACLICK:
        	handled = handle_func_key_ultraclick();
            break;
        case APP_KEY_EVENT_RAMPAGECLICK:
    		handled = handle_func_key_rampageclick();
        break;
        case  APP_KEY_EVENT_REPEAT:
        	handled = handle_func_key_repeat();
        	break;
        default:
            TRACE("unregister func key event=%x",event);
            break;
    }

	return handled;
}

void bt_key_handle_func_key(uint16_t event)
{
#ifdef __DIFF_HANDLE_FOR_REMOTE_KEY_
   	if( (event & BT_REMOTE_KEY_MASK) !=0 )
   	{
   		handle_remote_func_key(event & BT_KEY_MASK);
   		return;
   	}
#endif
	handle_local_func_key(event & BT_KEY_MASK);
}

bool bt_key_handle_local_func_key(uint16_t event)
{
	bool handled = false;
	handled = handle_local_func_key(event & BT_KEY_MASK);
	return handled;
}

void bt_key_handle_up_key(enum APP_KEY_EVENT_T event)
{
    switch(event)
    {
        case  APP_KEY_EVENT_UP:
        case  APP_KEY_EVENT_CLICK:
        	handle_up_key_click();
            break;
        case  APP_KEY_EVENT_DOUBLECLICK:
        	handle_up_key_double_click();
        	break;
        case  APP_KEY_EVENT_TRIPLECLICK:
        	handle_up_key_triple_click();
        	break;
        case  APP_KEY_EVENT_LONGPRESS:
        	handle_up_key_longpress();
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
            handle_down_key_click();
            break;
        case  APP_KEY_EVENT_DOUBLECLICK:
        	handle_down_key_double_click();
        	break;
        case  APP_KEY_EVENT_TRIPLECLICK:
        	handle_down_key_triple_click();
        	break;		
        case  APP_KEY_EVENT_LONGPRESS:
    		handle_down_key_longpress();
            break;
        default:
            TRACE("unregister down key event=%x",event);
            break;
    }
}


