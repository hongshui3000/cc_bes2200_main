#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hwtimer_list.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "bt_drv.h"
#include "bt_drv_interface.h"
#include "hal_codec.h"
#include "app_audio.h"
#include "audioflinger.h"
#include "nvrecord.h"
#include "app_key.h"
#include "app_utils.h"
#include "app_overlay.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "ddb.h"

extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "btalloc.h"
}
//#ifdef A2DP_AAC_DIRECT_TRANSFER
#include "app_tws_spp.h"
//#endif
#ifdef __TWS__

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#include "app_tws.h"
#include "app_bt.h"
#include "app_tws_if.h"
#include "app_tws_ble.h"
#include "hal_chipid.h"
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#include "player_role.h"
#endif


#include "app_bt_func.h"

extern  tws_dev_t  tws;


//#ifdef A2DP_AAC_DIRECT_TRANSFER
static void tws_ctrl_thread(const void *arg);
 osThreadDef(tws_ctrl_thread, osPriorityHigh, 2048);

#define TWS_CTRL_MAILBOX_MAX (15)
 osMailQDef (tws_ctl_mailbox, TWS_CTRL_MAILBOX_MAX, TWS_MSG_BLOCK);
int hfp_volume_get(void);


uint8_t tws_mail_count=0;

int tws_ctrl_mailbox_put(TWS_MSG_BLOCK* msg_src)
{
    osStatus status;

    TWS_MSG_BLOCK *msg_p = NULL;

    msg_p = (TWS_MSG_BLOCK*)osMailAlloc(tws.tws_mailbox, 0);
    if(!msg_p) {
        TRACE("%s fail, evt:%d,arg=%d \n",__func__,msg_src->evt,msg_src->arg);
        return -1;
    }

    ASSERT(msg_p, "osMailAlloc error");

    msg_p->evt = msg_src->evt;
    msg_p->arg = msg_src->arg;

    status = osMailPut(tws.tws_mailbox, msg_p);
    if(status == osOK)
        tws_mail_count ++;
    return (int)status;
}

int tws_ctrl_mailbox_free(TWS_MSG_BLOCK* msg_p)
{
    osStatus status;

    status = osMailFree(tws.tws_mailbox, msg_p);
    if (osOK == status)
	tws_mail_count--;
    return (int)status;
}

int tws_ctrl_mailbox_get(TWS_MSG_BLOCK** msg_p)
{
    osEvent evt;
    evt = osMailGet(tws.tws_mailbox, osWaitForever);
    if (evt.status == osEventMail) {
        *msg_p = (TWS_MSG_BLOCK *)evt.value.p;
        return 0;
    }
    return -1;
}

static int tws_ctrl_mailbox_init(void)
{
    tws.tws_mailbox = osMailCreate(osMailQ(tws_ctl_mailbox), NULL);
    if (tws.tws_mailbox == NULL)  {
        TRACE("Failed to Create tws_ctl_mailbox\n");
        return -1;
    }
    return 0;
}

void tws_ctrl_thread(const void *arg)
{
    TWS_MSG_BLOCK *msg_p = NULL;
    tws_dev_t *twsd = &tws;
    static AvdtpCodecType old_codectype = AVDTP_CODEC_TYPE_NON_A2DP;
    
    while(1){
        tws_ctrl_mailbox_get(&msg_p);
        TRACE("tws_ctrl_thread evt = %d\n", msg_p->evt);
        switch(msg_p->evt){
            
            case TWS_CTRL_PLAYER_SETCODEC:

#ifdef __AAC_ERROR_RESUME__
                 if(tws.tws_mode == TWSMASTER){      
  //                     tws_spp_set_codec_type(tws.mobile_codectype);
                        app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_CODEC_TYPE,tws.mobile_codectype,NULL,0);
                        twsd->tws_set_codectype_cmp = 0;
                }    
#else                
                 if(tws.tws_mode == TWSMASTER){                 
                    if(old_codectype != tws.mobile_codectype)
                       tws_spp_set_codec_type(tws.mobile_codectype);
                      // app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_CODEC_TYPE,tws.mobile_codectype,NULL,0);
                    else
                        twsd->tws_set_codectype_cmp = 1;
                    old_codectype = tws.mobile_codectype;
                }         
#endif                 
                break;

            case TWS_CTRL_PLAYER_SPPCONNECT:
                {
                    TRACE("sppconnect:%d tws.tws_mode = %d %d \n",msg_p->arg,tws.tws_mode,  twsd->tws_set_codectype_cmp);
                    uint32_t spp_connected = msg_p->arg;
                    if(spp_connected){
                        if(tws.tws_mode == TWSMASTER){
                            if(!twsd->tws_set_codectype_cmp)
                            {
//                                tws_spp_set_codec_type(tws.mobile_codectype);
                                app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_CODEC_TYPE,tws.mobile_codectype,NULL,0);
                                old_codectype = tws.mobile_codectype;
                            }
                        }
                    }else{
                        if(tws.tws_mode != TWSSLAVE){
                            twsd->tws_set_codectype_cmp = 0;
                            old_codectype = AVDTP_CODEC_TYPE_NON_A2DP;
                        }
                    }
                }         
                break;
            case TWS_CTRL_PLAYER_SET_HFP_VOLUME:
                 if(tws.tws_mode == TWSMASTER){                 
                      //  tws_spp_set_hfp_vol(hfp_volume_get());
                      TRACE("HFP_VOLUME SET =%d",hfp_volume_get());
                      app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_HFP_VOLUME,hfp_volume_get(),NULL,0);
                }                         
                break;
		//Modified by ATX : parker.wei_20180308		 
			 case TWS_CTRL_PLAYER_HFP_VOLUME_CHANGE:
				  if(tws.tws_mode == TWSMASTER){				 
					   //  tws_spp_set_hfp_vol(hfp_volume_get());
					   TRACE("HFP_VOLUME CHANGE =%d",hfp_volume_get());
					   app_bt_SPP_Write_Cmd(TWS_SPP_CMD_HFP_VOLUME_CHANGE,hfp_volume_get(),NULL,0);
				 }						   
				 break;
		//Modified by ATX : parker.wei_20180627	   
		   case TWS_CTRL_PLAYER_SET_SYSTEM_VOLUME:
				if(tws.tws_mode == TWSMASTER){				   
					 //  tws_spp_set_hfp_vol(hfp_volume_get());
					 app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_SYSTEM_VOL,msg_p->arg,NULL,0);
			   }						 
			   break;
            case TWS_CTRL_PLAYER_RESTART_PLAYER:
                 if(tws.tws_mode == TWSSLAVE){                 
                       // tws_spp_restart_player(1);
                       app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_TWSPLAYER_RESTART,1,NULL,0);
                }                         
                break;
            case TWS_CTRL_SLAVE_KEY_NOTIFY:
                 if(tws.tws_mode == TWSSLAVE){                 
                      //  tws_spp_notify_key(msg_p->arg);
                      app_bt_SPP_Write_Cmd(TWS_SPP_CMD_NOTIFY_KEY,msg_p->arg,NULL,0);
                }      
                 break;
            case TWS_CTRL_PLAYER_SET_A2DP_VOLUME:
                 if(tws.tws_mode == TWSMASTER){                 
                       // tws_spp_set_a2dp_vol(msg_p->arg);
                       app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SET_A2DP_VOLUME,msg_p->arg,NULL,0);
                }                         
                break;
			//Modified by ATX : parker.wei_20180306	
			 case TWS_CTRL_PLAYER_A2DP_CHANGE:
				  if(tws.tws_mode == TWSMASTER){				 
						// tws_spp_set_a2dp_vol(msg_p->arg);
						app_bt_SPP_Write_Cmd(TWS_SPP_CMD_A2DP_VOLUME_CHANGE,msg_p->arg,NULL,0);
				 }						   
				 break;
			//Modified by ATX : Parke.Wei_20180316
#ifdef _PROJ_2000IZ_C003__
			 case TWS_CTRL_PLAYER_UPDATE_PEER_STATE:
				  if(tws.tws_mode == TWSMASTER){	  
						 app_bt_SPP_Write_Cmd(TWS_SPP_CMD_UPDATE_PEER_STATE,msg_p->arg,NULL,0);
				 }
				 break;
#endif

				//Modified by ATX : Parke.Wei_20180413
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
			  case TWS_MASTER_CTRL_SLAVE_ENTER_OTA:
				   if(tws.tws_mode == TWSMASTER){	   
						  app_bt_SPP_Write_Cmd(TWS_SPP_CMD_SLAVE_ENTER_CMD,msg_p->arg,NULL,0);
				  }
				  break;
#endif

             case TWS_CTRL_PLAYER_SEND_RSP:
                app_bt_SPP_Write_Rsp(msg_p->arg>>16,msg_p->arg&0xff);
                break;
            //Modified by ATX : Haorong.Wu_20180507
             case TWS_CTRL_VOICE_REPORT:
                if(tws.tws_mode == TWSMASTER){
                    app_bt_SPP_Write_Cmd(TWS_SPP_CMD_VOICE_REPORT, msg_p->arg, NULL, 0);
                }
                break;
            case TWS_CTRL_RING_SYNC:
                TRACE("TWS_CTRL_RING_SYNC\n");
#ifdef TWS_RING_SYNC
                if(tws.tws_mode == TWSMASTER){
                    app_bt_SPP_Write_Cmd(TWS_SPP_CMD_RING_SYNC,msg_p->arg,NULL,0);
                }
#endif
                break;

            default:
                break;
        }
        TRACE("tws_ctrl_thread exit\n");
        
        tws_ctrl_mailbox_free(msg_p);
    }
}

void tws_ctrl_thread_init(void)
{
    tws_ctrl_mailbox_init();
    tws.tws_ctrl_tid = osThreadCreate(osThread(tws_ctrl_thread), NULL);    
}



int tws_player_set_codec( AvdtpCodecType codec_type)
{
#ifdef A2DP_AAC_DIRECT_TRANSFER
    TWS_MSG_BLOCK msg;
    msg.arg = 0;
    msg.evt = TWS_CTRL_PLAYER_SETCODEC;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
#endif
    return 0;
}

//Modified by ATX : Parke.Wei_20180316
int tws_player_update_peer_state(APP_STATUS_INDICATION_T state)
{

    TWS_MSG_BLOCK msg;
    msg.arg = state;
    msg.evt = TWS_CTRL_PLAYER_UPDATE_PEER_STATE;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);

    return 0;
}

//Modified by ATX : Parke.Wei_20180316
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
int tws_mastet_ctrl_slave_enter_ota(uint8_t arg)
{

    TWS_MSG_BLOCK msg;
    msg.arg = arg;
    msg.evt = TWS_MASTER_CTRL_SLAVE_ENTER_OTA;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);

    return 0;
}

#endif

int tws_player_set_hfp_vol( int8_t vol)
{
    TWS_MSG_BLOCK msg;
    msg.arg = vol;
    msg.evt = TWS_CTRL_PLAYER_SET_HFP_VOLUME;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

//Modified by ATX : parker.wei_20180627	 
//set hfp&a2dp volume together;
int tws_player_set_system_vol( int8_t hfp_vol,int8_t a2dp_vol)
{
    TWS_MSG_BLOCK msg;
    msg.arg = a2dp_vol|(hfp_vol<<8);
    msg.evt = TWS_CTRL_PLAYER_SET_SYSTEM_VOLUME;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}


//Modified by ATX : parker.wei_20180308
int tws_player_hfp_vol_change( int8_t vol)
{
	TRACE("%s\n",__func__);
	TWS_MSG_BLOCK msg;
    msg.arg = vol;
    msg.evt = TWS_CTRL_PLAYER_HFP_VOLUME_CHANGE;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

int tws_player_pause_player_req( int8_t req)
{
    TWS_MSG_BLOCK msg;
    msg.arg = req;
    msg.evt = TWS_CTRL_PLAYER_RESTART_PLAYER;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}



int tws_player_spp_connected( unsigned char para)
{
#ifdef A2DP_AAC_DIRECT_TRANSFER
    TWS_MSG_BLOCK msg;
    msg.arg = (uint32_t)para;
    msg.evt = TWS_CTRL_PLAYER_SPPCONNECT;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
#endif
    return 0;
}


int tws_player_notify_key( unsigned short key,unsigned short event)
{
    TWS_MSG_BLOCK msg;

    if((tws_mail_count+3) >TWS_CTRL_MAILBOX_MAX)
        return 0;

    //Modified by ATX : Leon.He_20180127:  add 0x10,  avoid 0x0a,0x0d ignored by SPP
    key+=0x10;
    event+=0x10;

    msg.arg = (uint32_t)(key | (event<<16));
    msg.evt = TWS_CTRL_SLAVE_KEY_NOTIFY;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

int tws_player_set_a2dp_vol( int8_t vol)
{
    TWS_MSG_BLOCK msg;
    msg.arg = vol;
    msg.evt = TWS_CTRL_PLAYER_SET_A2DP_VOLUME;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

//Modified by ATX : parker.wei_20180306 vol_up Event deal independently 
int tws_player_a2dp_vol_change( int8_t vol)
{
	TRACE("%s\n",__func__);
	TWS_MSG_BLOCK msg;
    msg.arg = vol;
    msg.evt = TWS_CTRL_PLAYER_A2DP_CHANGE;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}



int tws_player_spp_send_rsp( uint16_t cmdid,uint8_t status)
{
    TWS_MSG_BLOCK msg;
    msg.arg = (uint32_t)(cmdid<<16)|status;
    msg.evt = TWS_CTRL_PLAYER_SEND_RSP;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

//Modified by ATX : Haorong.Wu_20180507
int tws_slave_voice_report(APP_STATUS_INDICATION_T id)
{
    TWS_MSG_BLOCK msg;
    msg.arg = id;
    msg.evt = TWS_CTRL_VOICE_REPORT;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}

#ifdef TWS_RING_SYNC
int tws_player_ring_sync(U16 hf_event)
{
    TWS_MSG_BLOCK msg;
    msg.arg = hf_event;
    msg.evt = TWS_CTRL_RING_SYNC;
    //becase tws_audio_sendrequest will block the evmprocess
    //had better to create a new thread to avoid fall into a dead trap;
    tws_ctrl_mailbox_put(&msg);
    return 0;
}
#endif

#endif
