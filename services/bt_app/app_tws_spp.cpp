
#include <stdio.h>
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "app_audio.h"
#include "app_status_ind.h"
#include "app_bt_stream.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "nvrecord_dev.h"
extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "spp.h"
#include "btalloc.h"
}

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"

#include "nvrecord.h"

#include "apps.h"
#include "resources.h"


#ifdef MEDIA_PLAYER_SUPPORT  //Modified by ATX : parker.wei_20180305
#include "app_media_player.h"
#endif


#include "app_bt_media_manager.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif
#include "bt_drv_interface.h"
#include "app_tws_spp.h"
#include "app_key.h"

#include "app_bt_func.h"
//#if defined(A2DP_AAC_ON) && defined(A2DP_AAC_DIRECT_TRANSFER)

#define CMD_BUFFER_LEN 96

#define PAYLOAD_LEN_WIDTH (1)
#define CMD_RSP_TAG_LEN 6   //6 + 1 payload_len
#define CMD_TAG "T_CMD:"
#define RSP_TAG "T_RSP:"

int a2dp_volume_set(U8 vol);

//Modified by ATX : parker.wei_20180305
extern int get_a2dp_volume_level(U8 vol);

//Modified by ATX : Parke.Wei_20180316  for peer slave get the master state;
#ifdef _PROJ_2000IZ_C003_
APP_STATUS_INDICATION_T remote_master_state;
#endif

extern struct BT_DEVICE_T  app_bt_device;
void tws_spp_close_read_thread(void);
void tws_spp_create_read_thread(void);
void tws_spp_callback(SppDev *locDev, SppCallbackParms *Info);
static void tws_spp_read_thread(const void *arg);
 osThreadDef(tws_spp_read_thread, osPriorityHigh, 1024*2);
osSemaphoreDef(spp_wait_rsp);
osSemaphoreId spp_wait_rsp = NULL;
extern  tws_dev_t  tws;
extern int hfp_volume_set(int vol);
int hfp_volume_get(void);


void tws_create_spp_client(A2dpStream *Stream)
{
    BtRemoteDevice *dev = NULL;

    if(tws.tws_spp_dev){
        tws_spp_close_read_thread();
        if(spp_wait_rsp)
            osSemaphoreDelete(spp_wait_rsp);
        spp_wait_rsp = NULL;
        SPP_Close(tws.tws_spp_dev);
    }
    
    TRACE("enter %s \n", __func__);
    dev = Stream->stream.chnl->conn.remDev;
    tws.tws_spp_dev = &app_bt_device.spp_dev[0];
    tws.tws_spp_dev->portType = SPP_CLIENT_PORT;
    tws.tws_spp_connect = false;
    SPP_InitDevice(tws.tws_spp_dev, &app_bt_device.txPacket, app_bt_device.numPackets);
    SPP_Open(tws.tws_spp_dev, dev, tws_spp_callback);
    spp_wait_rsp = osSemaphoreCreate(osSemaphore(spp_wait_rsp), 0);
    TRACE("leave %s \n", __func__);
}

void tws_create_spp_server(A2dpStream *Stream)
{
    TRACE("%s:%d\n", __func__, __LINE__);
    if(tws.tws_spp_dev){
        TRACE("%s:%d\n", __func__, __LINE__);
        tws_spp_close_read_thread();
        if(spp_wait_rsp)
            osSemaphoreDelete(spp_wait_rsp);
        spp_wait_rsp = NULL;
        
        SPP_Close(tws.tws_spp_dev);
    }

    TRACE("enter %s \n", __func__);
    tws.tws_spp_dev = &app_bt_device.spp_dev[0];
    tws.tws_spp_dev->portType = SPP_SERVER_PORT;
    tws.tws_spp_connect = false;
    SPP_InitDevice(tws.tws_spp_dev, &app_bt_device.txPacket, app_bt_device.numPackets);
    SPP_Open(tws.tws_spp_dev, NULL, tws_spp_callback);

    spp_wait_rsp = osSemaphoreCreate(osSemaphore(spp_wait_rsp), 0);
    
    TRACE("leave %s \n", __func__);
}

 static uint8_t spp_rx_buffer[CMD_BUFFER_LEN];
 static uint8_t spp_tx_buffer[CMD_BUFFER_LEN];


void tws_spp_wait_send_done(void)
{
    if(spp_wait_rsp){
        tws.tws_spp_wait_event = true;
        osSemaphoreWait(spp_wait_rsp, osWaitForever);
        tws.tws_spp_wait_event = false;
    }
}
 
void tws_spp_send_cmd(uint8_t* cmd, uint8_t len)
{
    uint8_t *payload = NULL;
    U16  cmd_len = CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH + len;
    
    ASSERT(cmd_len <= (CMD_BUFFER_LEN + PAYLOAD_LEN_WIDTH), "tws_spp_build_cmd \n");

    if(!tws.tws_spp_connect)
    {
        if(tws.tws_spp_wait_event ){
            TRACE("RELEASE SPP WAIT RSP");
            if(spp_wait_rsp)
                osSemaphoreRelease(spp_wait_rsp);
      //      tws.tws_spp_wait_event = false;
        }           
        return;
    }

    TRACE("Enter %s spp_wait_rsp= 0x%x \n", __func__, spp_wait_rsp);
    payload = spp_tx_buffer;
    memcpy(payload, CMD_TAG, CMD_RSP_TAG_LEN);
    spp_tx_buffer[CMD_RSP_TAG_LEN] = len;
    payload += CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH;
    memcpy(payload, cmd, len);
    SPP_Write(tws.tws_spp_dev, (char *)spp_tx_buffer, &cmd_len);
#if 0    
    if(spp_wait_rsp){
        tws.tws_spp_wait_event = true;
        osSemaphoreWait(spp_wait_rsp, osWaitForever);
        tws.tws_spp_wait_event = false;
    }
#endif    
    TRACE("Leave %s \n", __func__);
}

void tws_spp_send_rsp(uint8_t* rsp, uint8_t len)
{
    uint8_t *payload = NULL;
    U16 rsp_len = CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH + len;
    
    ASSERT(rsp_len <= CMD_BUFFER_LEN, "tws_spp_build_cmd \n");
    
    if(!tws.tws_spp_connect)
    {
        if(tws.tws_spp_wait_event ){
            TRACE("RELEASE SPP WAIT RSP");
            if(spp_wait_rsp)
                osSemaphoreRelease(spp_wait_rsp);
      //      tws.tws_spp_wait_event = false;
        }              
        return;
    }
    payload = spp_tx_buffer;
    memcpy(payload, RSP_TAG, CMD_RSP_TAG_LEN);
    spp_tx_buffer[CMD_RSP_TAG_LEN] = len;
    payload += CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH;
    memcpy(payload, rsp, len);
    SPP_Write(tws.tws_spp_dev, (char *)spp_tx_buffer, &rsp_len);
    TRACE("Leave %s rsp_len = %d\n", __func__, rsp_len);
}

//server
void tws_spp_parse_cmd(uint8_t* cmd, uint8_t len)
{
    uint16_t cmd_id = 0;
    TWS_SPP_CMD_RSP rsp;
    
    cmd_id = cmd[0] | (cmd[1] << 8);
    
    TRACE("%s id:0x%x \n", __func__, cmd_id);
    switch(cmd_id){
        case TWS_SPP_CMD_SET_CODEC_TYPE:
        {
            TWS_SPP_SET_TWS_CODEC_TYPE codec_type;
            memcpy(&codec_type, cmd, sizeof(codec_type));
            tws.mobile_codectype = codec_type.codec_type;
            TRACE("TWS_SPP_CMD_SET_CODEC_TYPE %d \n",  tws.mobile_codectype);
        }
        break;
        case TWS_SPP_CMD_SET_HFP_VOLUME:
        {
            TWS_SPP_SET_TWS_HFP_VOLUME hfp_volume;
            memcpy(&hfp_volume, cmd, sizeof(hfp_volume));
            tws.hfp_volume = hfp_volume.vol-0x10;
            TRACE("TWS_SPP_CMD_SET_HFP_VOLUME %d \n", tws.hfp_volume);
            hfp_volume_set(tws.hfp_volume);
        }
        break;
	//Modified by ATX : parker.wei_20180308
        case TWS_SPP_CMD_HFP_VOLUME_CHANGE:
        {
            TWS_SPP_SET_TWS_HFP_VOLUME hfp_volume;
            memcpy(&hfp_volume, cmd, sizeof(hfp_volume));
            tws.hfp_volume = hfp_volume.vol-0x10;
            TRACE("TWS_SPP_CMD_HFP_VOLUME_CHANGE %d \n", tws.hfp_volume);
            hfp_volume_set(tws.hfp_volume);
			
			//Modified by ATX : parker.wei_20180308
			if(tws.hfp_volume>=15)
#ifdef MEDIA_PLAYER_SUPPORT
				app_voice_report(APP_STATUS_INDICATION_VOL_MAX,0);
#endif
			 if(tws.hfp_volume<=0)
#ifdef MEDIA_PLAYER_SUPPORT
				 app_voice_report(APP_STATUS_INDICATION_VOL_MIN,0);


#endif		
        }
        break;

        case TWS_SPP_CMD_SET_TWSPLAYER_RESTART:
        {
            TWS_SPP_SET_TWS_PLATER_RESTART player_restart;
            memcpy(&player_restart, cmd, sizeof(player_restart));
            if(tws.tws_mode == TWSMASTER && (bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()) > tws.player.last_trigger_time))
                tws.tws_player_restart = player_restart.restart;
            TRACE("TWS_SPP_CMD_SET_TWSPLAYER_RESTART %d \n", tws.tws_player_restart);
            
        }
        break;
        case TWS_SPP_CMD_NOTIFY_KEY:
        {
            TWS_SPP_SET_TWS_NOTIFY_KEY key_notify;
            memcpy(&key_notify, cmd, sizeof(key_notify));
            if(tws.tws_mode == TWSMASTER)
            {
//Modified by ATX : Leon.He_20180127: different handle for local key and remote key, add avoid 0x0a,0x0d ignored by SPP
            	uint16_t code = (uint16_t)(key_notify.key & 0xffff) - 0x10;
            	uint16_t key = (uint16_t)(key_notify.key >> 16) - 0x10;

#ifdef __DIFF_HANDLE_FOR_REMOTE_KEY_
            	bt_remote_key_send(code,key);
#else
            	bt_key_send(code,key);
#endif
            }
            TRACE("TWS_SPP_CMD_NOTIFY_KEY %d \n", key_notify.key);
            
        }
        break;
//Modified by ATX : parker.wei_20180627
		case TWS_SPP_CMD_SET_SYSTEM_VOL:
		{   uint8_t hfp_vol;
			uint8_t a2dp_vol;
			TWS_SPP_SET_TWS_SYSTEM_VOLUME  sys_volume;
			memcpy(&sys_volume, cmd, sizeof(sys_volume));
			tws.hfp_volume =(sys_volume.vol&0xff00)>>8;
			a2dp_vol=sys_volume.vol&0x00ff;
			//tws.hfp_volume = a2dp_volume.vol-0x10;
			TRACE("TWS_SPP_CMD_SET_SYSTEM_VOL a2dp=%d,hfp=%d \n",a2dp_vol,tws.hfp_volume);
			a2dp_volume_set(a2dp_vol);
			hfp_volume_set(tws.hfp_volume);
		}
		break;	  
        case TWS_SPP_CMD_SET_A2DP_VOLUME:
        {
            TWS_SPP_SET_TWS_A2DP_VOLUME  a2dp_volume;
            memcpy(&a2dp_volume, cmd, sizeof(a2dp_volume));
      //      tws.hfp_volume = a2dp_volume.vol-0x10;
            TRACE("TWS_SPP_CMD_SET_A2DP_VOLUME %d \n", a2dp_volume.vol-0x10);
	  		a2dp_volume_set(a2dp_volume.vol-0x10);
        }
        break;  		
		//Modified by ATX : parker.wei_20180305 vol change
        case TWS_SPP_CMD_A2DP_VOLUME_CHANGE:
        {
            TWS_SPP_SET_TWS_A2DP_VOLUME  a2dp_volume;
            memcpy(&a2dp_volume, cmd, sizeof(a2dp_volume));
      //      tws.hfp_volume = a2dp_volume.vol-0x10;
            TRACE("TWS_SPP_CMD_A2DP_VOLUME_CHANGE %d \n", a2dp_volume.vol);
	  		a2dp_volume_set(a2dp_volume.vol-0x10);
            if(get_a2dp_volume_level(a2dp_volume.vol-0x10)==TGT_VOLUME_LEVEL_15)
#ifdef MEDIA_PLAYER_SUPPORT
				 app_voice_report(APP_STATUS_INDICATION_VOL_MAX,0);

 #endif
             if(get_a2dp_volume_level(a2dp_volume.vol-0x10)==TGT_VOLUME_LEVEL_MUTE)
#ifdef MEDIA_PLAYER_SUPPORT
				 app_voice_report(APP_STATUS_INDICATION_VOL_MIN,0);


 #endif
        }
        break;  
        //Modified by ATX : Haorong.Wu_20180507
        case TWS_SPP_CMD_VOICE_REPORT:
        {
            TWS_SPP_SEND_VOICE_REPORT voice_cmd;
            memcpy(&voice_cmd, cmd, sizeof(voice_cmd));
            app_voice_report((APP_STATUS_INDICATION_T)(voice_cmd.voice_id), 0);
        }
            break;
		//Modified by ATX : Parke.Wei_20180316
#ifdef _PROJ_2000IZ_C003_
		case TWS_SPP_CMD_UPDATE_PEER_STATE:
        {
            TWS_SPP_UPDATE_PEER_STATE  peer_state;
            memcpy(&peer_state, cmd, sizeof(peer_state));
			uint8_t update_state=peer_state.state&0x7f;
      //      tws.hfp_volume = a2dp_volume.vol-0x10;
            TRACE("TWS_SPP_CMD_UPDATE_PEER_STATE [%d] \n",update_state);
	  		 remote_master_state=(APP_STATUS_INDICATION_T)update_state;
            if(remote_master_state==APP_STATUS_INDICATION_CONNECTED)	
				app_voice_report(APP_STATUS_INDICATION_CONNECTED,0); 
        }
        break; 
#endif
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__ //Modified by ATX : Parke.Wei_20180413
		case TWS_SPP_CMD_SLAVE_ENTER_CMD:
		{
			TWS_SPP_UPDATE_PEER_CTRL  ctrl_cmd;
			memcpy(&ctrl_cmd, cmd, sizeof(ctrl_cmd));
			TRACE("TWS_SPP_CMD_SLAVE_ENTER_CMD [%d] \n",ctrl_cmd.reserved);
		    bt_key_send(APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS);	//FOR SLAVE ENTER OTA

		}
		break; 
#endif
        case TWS_SPP_CMD_RING_SYNC:
        {
            TWS_SPP_SET_RING_SYNC ring_sync;
            memcpy(&ring_sync, cmd, sizeof(ring_sync));
            TRACE("TWS_SPP_CMD_RING_SYNC ring_sync.hf_event:%d\n", ring_sync.hf_event);
            if(ring_sync.hf_event == HF_EVENT_RING_IND){
                app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,0);
            }
        }
        break;



        default:
            return;
    }

    rsp.cmd_id = cmd_id;
    rsp.status = 0;
//    tws_spp_send_rsp((uint8_t *)&rsp, sizeof(rsp));
    //app_bt_SPP_Write_Rsp(rsp.cmd_id,rsp.status);
    tws_player_spp_send_rsp(cmd_id,0);
}

//client
void tws_spp_parse_rsp(uint8_t* rsp, uint8_t len)
{
    TWS_SPP_CMD_RSP s_rsp;
    
    memcpy(&s_rsp, rsp, sizeof(s_rsp));

    TRACE("%s id:0x%x \n", __func__, s_rsp.cmd_id);
    
    switch(s_rsp.cmd_id){
        case TWS_SPP_CMD_SET_CODEC_TYPE:
            if(s_rsp.status == 0){
#if 0                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                    tws.tws_set_codectype_cmp = 1;
                }
#endif                
                tws.tws_set_codectype_cmp = 1;

            }
            break;
        case TWS_SPP_CMD_SET_HFP_VOLUME:
            if(s_rsp.status == 0){
#if 0                
                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                }
#endif                
            }
            break;
	//Modified by ATX : parker.wei_20180308
		case TWS_SPP_CMD_HFP_VOLUME_CHANGE:
			if(s_rsp.status == 0){
#if 0                
				
				if(tws.tws_spp_wait_event ){
					if(spp_wait_rsp)
						osSemaphoreRelease(spp_wait_rsp);
					tws.tws_spp_wait_event = false;
				}
#endif                
			}
			break;

			case TWS_SPP_CMD_SET_TWSPLAYER_RESTART:
            if(s_rsp.status == 0){
#if 0                
                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                }
#endif                
            }
            break;

        case TWS_SPP_CMD_NOTIFY_KEY:
            if(s_rsp.status == 0){
#if 0                    
                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                }
#endif                
            }
            break;
		case TWS_SPP_CMD_SET_SYSTEM_VOL:
			if(s_rsp.status == 0){
#if 0                
				if(tws.tws_spp_wait_event ){
					if(spp_wait_rsp)
						osSemaphoreRelease(spp_wait_rsp);
					tws.tws_spp_wait_event = false;
				}
#endif                
			}
			break;
        case TWS_SPP_CMD_SET_A2DP_VOLUME:
            if(s_rsp.status == 0){
#if 0                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                }
#endif                
            }
            break;
			//Modified by ATX : parker.wei_20180306
        case TWS_SPP_CMD_A2DP_VOLUME_CHANGE:
            if(s_rsp.status == 0){
#if 0                
                if(tws.tws_spp_wait_event ){
                    if(spp_wait_rsp)
                        osSemaphoreRelease(spp_wait_rsp);
                    tws.tws_spp_wait_event = false;
                }
#endif                
            }
            break;    

			//Modified by ATX : parker.wei_20180316
#ifdef _PROJ_2000IZ_C003_
		case TWS_SPP_CMD_UPDATE_PEER_STATE:
			if(s_rsp.status == 0){
#if 0                
				if(tws.tws_spp_wait_event ){
					if(spp_wait_rsp)
						osSemaphoreRelease(spp_wait_rsp);
					tws.tws_spp_wait_event = false;
				}
#endif                
			}
			break;	 
#endif
		//Modified by ATX : parker.wei_20180413
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
		case TWS_SPP_CMD_SLAVE_ENTER_CMD:
			if(s_rsp.status == 0){
#if 0                
				if(tws.tws_spp_wait_event ){
					if(spp_wait_rsp)
						osSemaphoreRelease(spp_wait_rsp);
					tws.tws_spp_wait_event = false;
				}
#endif                
			}
			break;	 
#endif




        default:
            break;
    }
}

static void tws_spp_read_thread(const void *arg)
{
    BtStatus status;
    uint8_t* t_buffer;
    uint8_t payload_len = 0;
    uint8_t recv_len = 0;
    U16 maxBytes = 0;
    
    while (1)
    {
    	//Modified by ATX : parker.wei_20180310 redefine trace
        TRACE("\nSPP: tws_spp_read_thread -> reading...... \n");
        memset(spp_rx_buffer, 0, CMD_BUFFER_LEN);
        maxBytes = CMD_BUFFER_LEN;

        t_buffer = spp_rx_buffer;
        recv_len = 0;
        while(1){
            status = SPP_Read(tws.tws_spp_dev, (char  *)t_buffer, &maxBytes);
            if(status == BT_STATUS_SUCCESS){
                recv_len += maxBytes;
                TRACE("spp recv_len = %d \n", recv_len);
                if(recv_len < (CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH)){                  
                    maxBytes = CMD_BUFFER_LEN - recv_len;          
                }else{
                    payload_len = spp_rx_buffer[CMD_RSP_TAG_LEN];
                    TRACE("spp payload_len = %d \n", payload_len);
                    if(recv_len < payload_len + CMD_RSP_TAG_LEN){
                         maxBytes = payload_len + CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH - recv_len;      
                    }else{
                        break;
                    }
                }
                t_buffer =spp_rx_buffer + recv_len;
             }else{
                payload_len = 0;
                break;
             }
        }

        TRACE("%s %d\n", payload_len);

        if(payload_len == 0)
            continue;
        
        if(!memcmp(spp_rx_buffer, CMD_TAG, CMD_RSP_TAG_LEN)){
            tws_spp_parse_cmd(spp_rx_buffer + CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH, payload_len);
        }else if(!memcmp(spp_rx_buffer, RSP_TAG, CMD_RSP_TAG_LEN)){
            tws_spp_parse_rsp(spp_rx_buffer + CMD_RSP_TAG_LEN + PAYLOAD_LEN_WIDTH, payload_len);
        }
    }
}

void tws_spp_create_read_thread(void)
{
    TRACE("%s %d\n", __func__, __LINE__);
    tws.tws_spp_tid = osThreadCreate(osThread(tws_spp_read_thread), NULL);
    TRACE("%s %d\n", __func__, __LINE__);
}

void tws_spp_close_read_thread(void)
{
    TRACE("%s %d\n", __func__, __LINE__);
    if(tws.tws_spp_tid ){
        if(tws.tws_spp_wait_event){
            if(spp_wait_rsp)
            {
                    osSemaphoreRelease(spp_wait_rsp);
            }
        }
        osThreadTerminate(tws.tws_spp_tid);
        tws.tws_spp_tid = NULL;
    }
    TRACE("%s %d\n", __func__, __LINE__);
}




void tws_spp_callback(SppDev *locDev, SppCallbackParms *Info)
{
    TRACE("spp_callback : locDev %p, Info %p\n", locDev, Info);
    TRACE("::spp Info->event %d,wait_event=%d\n", Info->event,tws.tws_spp_wait_event);

    if (Info->event == SPP_EVENT_REMDEV_CONNECTED)
    {
        TRACE("::SPP_EVENT_REMDEV_CONNECTED %d\n", Info->event);
        tws.tws_spp_connect = true;
        tws_spp_create_read_thread();
        tws_player_spp_connected(1);
        //Modified by ATX : Parker.Wei_20180627  sync a2dp&hfp volume together
#ifdef __TWS_CALL_DUAL_CHANNEL__
		tws_player_set_system_vol(hfp_volume_get(),a2dp_volume_get_tws());	  
#else		
		tws_player_set_a2dp_vol(a2dp_volume_get_tws());
		tws_player_set_hfp_vol(hfp_volume_get());//Modified by ATX : Leon.He_20170626: fix tws reconnect during call, slave vol 2
#endif	
    }
    else if (Info->event == SPP_EVENT_REMDEV_DISCONNECTED)
    {
        TRACE("::SPP_EVENT_REMDEV_DISCONNECTED %d\n", Info->event);
        tws.tws_spp_connect = false;
        tws_spp_close_read_thread();
        if(tws.tws_spp_wait_event ){
            TRACE("RELEASE SPP WAIT RSP");
            if(spp_wait_rsp)
                osSemaphoreRelease(spp_wait_rsp);
      //      tws.tws_spp_wait_event = false;
        }          
        
        if(spp_wait_rsp)
            osSemaphoreDelete(spp_wait_rsp);
        spp_wait_rsp = NULL;
        SPP_Close(tws.tws_spp_dev);
        tws.tws_spp_dev = NULL;
        tws_player_spp_connected(0);
    }
    else if(Info->event ==RFEVENT_PACKET_HANDLED)
    {
        if(tws.tws_spp_wait_event ){
            TRACE("RELEASE SPP WAIT RSP");
            if(spp_wait_rsp)
                osSemaphoreRelease(spp_wait_rsp);
      //      tws.tws_spp_wait_event = false;
        }    
    }
    else
    {
        TRACE("::unknown event %d\n", Info->event);
    }
}

void tws_spp_set_codec_type(uint8_t codec_type)
{
    TWS_SPP_SET_TWS_CODEC_TYPE set_tws_codec_type;

    set_tws_codec_type.cmd_id = TWS_SPP_CMD_SET_CODEC_TYPE;
    set_tws_codec_type.codec_type = codec_type;

    tws_spp_send_cmd((uint8_t *)&set_tws_codec_type, sizeof(set_tws_codec_type));
}




void tws_spp_set_hfp_vol(int8_t vol)
{
    TWS_SPP_SET_TWS_HFP_VOLUME set_tws_hfp_volume;
    TRACE("tws_spp_set_hfp_vol VOL=%d",vol);
    set_tws_hfp_volume.cmd_id = TWS_SPP_CMD_SET_HFP_VOLUME;
    set_tws_hfp_volume.vol= vol+0x10;

    tws_spp_send_cmd((uint8_t *)&set_tws_hfp_volume, sizeof(set_tws_hfp_volume));
}

//Modified by ATX : parker.wei_20180308
void tws_spp_hfp_vol_change(int8_t vol)
{
    TWS_SPP_SET_TWS_HFP_VOLUME set_tws_hfp_volume;
    TRACE("tws_spp_hfp_vol_change VOL=%d",vol);
    set_tws_hfp_volume.cmd_id = TWS_SPP_CMD_HFP_VOLUME_CHANGE;
    set_tws_hfp_volume.vol= vol+0x10;

    tws_spp_send_cmd((uint8_t *)&set_tws_hfp_volume, sizeof(set_tws_hfp_volume));
}


void tws_spp_restart_player(int8_t restart)
{
    TWS_SPP_SET_TWS_PLATER_RESTART set_tws_player_restart;
    TRACE("tws_spp_set_hfp_vol restart=%d",restart);
    set_tws_player_restart.cmd_id = TWS_SPP_CMD_SET_TWSPLAYER_RESTART;
    set_tws_player_restart.restart = 1;

    tws_spp_send_cmd((uint8_t *)&set_tws_player_restart, sizeof(set_tws_player_restart));
}



void tws_spp_notify_key(uint32_t key)
{
    _TWS_SPP_SET_TWS_NOTIFY_KEY tws_player_notify_key;
    TRACE("tws_spp_notify_key key=%d",key);
    tws_player_notify_key.cmd_id = TWS_SPP_CMD_NOTIFY_KEY;
    tws_player_notify_key.key = key;

    tws_spp_send_cmd((uint8_t *)&tws_player_notify_key, sizeof(tws_player_notify_key));
}

void tws_spp_set_system_vol(uint16_t vol)
{
    TWS_SPP_SET_TWS_SYSTEM_VOLUME sys_volume;
    TRACE("tws_spp_set sys_vol=%d hfp_vol=%d",(vol&0x00ff),(vol&0xff00)>>8);
    sys_volume.cmd_id = TWS_SPP_CMD_SET_SYSTEM_VOL;
    sys_volume.vol= vol;

    tws_spp_send_cmd((uint8_t *)&sys_volume, sizeof(sys_volume));
}

void tws_spp_set_a2dp_vol(uint8_t vol)
{
    TWS_SPP_SET_TWS_A2DP_VOLUME set_tws_a2dp_volume;
    TRACE("tws_spp_set_a2dp_vol VOL=%d",vol);
    set_tws_a2dp_volume.cmd_id = TWS_SPP_CMD_SET_A2DP_VOLUME;
    set_tws_a2dp_volume.vol= vol+0x10;

    tws_spp_send_cmd((uint8_t *)&set_tws_a2dp_volume, sizeof(set_tws_a2dp_volume));
}

#ifdef TWS_RING_SYNC
void tws_spp_ring_sync(U16 hf_event)
{
    TWS_SPP_SET_RING_SYNC tws_ring_sync;
    TRACE("tws_spp_ring_sync  hf_event=%d",hf_event);
    tws_ring_sync.cmd_id = TWS_SPP_CMD_RING_SYNC;
    tws_ring_sync.hf_event = hf_event;

    tws_spp_send_cmd((uint8_t *)&tws_ring_sync, sizeof(tws_ring_sync));
}
#endif

//Modified by ATX : parker.wei_20180306
void tws_spp_a2dp_vol_change(uint8_t vol)
{
    TWS_SPP_SET_TWS_A2DP_VOLUME set_tws_a2dp_volume;
    TRACE("%s VOL=%d",__func__,vol);
    set_tws_a2dp_volume.cmd_id = TWS_SPP_CMD_A2DP_VOLUME_CHANGE;
    set_tws_a2dp_volume.vol= vol+0x10;

    tws_spp_send_cmd((uint8_t *)&set_tws_a2dp_volume, sizeof(set_tws_a2dp_volume));
}

//Modified by ATX : Haorong.Wu_20180507
void tws_spp_voice_report(APP_STATUS_INDICATION_T id)
{
    TWS_SPP_SEND_VOICE_REPORT set_tws_voice_report;
    TRACE("%s voice id =%d",__func__,id);
    set_tws_voice_report.cmd_id = TWS_SPP_CMD_VOICE_REPORT;
    set_tws_voice_report.voice_id = id;

    tws_spp_send_cmd((uint8_t *)&set_tws_voice_report, sizeof(set_tws_voice_report));
}


#ifdef _PROJ_2000IZ_C003_
void tws_spp_update_peer_state(uint8_t state)
{
    TWS_SPP_UPDATE_PEER_STATE peer_state;
    TRACE("%s UPDATE PEER STATE[%d]",__func__,state);
    peer_state.cmd_id = TWS_SPP_CMD_UPDATE_PEER_STATE;
    peer_state.state= state|0x80;

    tws_spp_send_cmd((uint8_t *)&peer_state, sizeof(peer_state));
}
#endif

#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__ //Modified by ATX : Parke.Wei_20180413
void tws_spp_master_ctrl_slave_enter_ota(uint8_t arg)
{
    TWS_SPP_UPDATE_PEER_CTRL ctrl_cmd;
    TRACE("%s ]",__func__);
    ctrl_cmd.cmd_id = TWS_SPP_CMD_SLAVE_ENTER_CMD;
    ctrl_cmd.reserved= NULL;

    tws_spp_send_cmd((uint8_t *)&ctrl_cmd, sizeof(ctrl_cmd));
}
#endif



void  btapp_process_spp_write(uint16_t cmdid,uint32_t param,uint8_t *ptr,uint32_t len)
{
    TRACE("%s",__FUNCTION__);
    switch(cmdid)
    {
        case TWS_SPP_CMD_SET_CODEC_TYPE:
            tws_spp_set_codec_type(param);
            break;
        case TWS_SPP_CMD_SET_HFP_VOLUME:
            tws_spp_set_hfp_vol(param);
            break;
	//Modified by ATX : parker.wei_20180308
        case TWS_SPP_CMD_HFP_VOLUME_CHANGE:
        	tws_spp_hfp_vol_change(param);
        	break;
        case TWS_SPP_CMD_SET_TWSPLAYER_RESTART:
            tws_spp_restart_player(param);
            break;
        case TWS_SPP_CMD_NOTIFY_KEY:
            tws_spp_notify_key(param);
            break;
        case TWS_SPP_CMD_SET_A2DP_VOLUME:
            tws_spp_set_a2dp_vol(param);
            break;
#ifdef TWS_RING_SYNC
        case TWS_SPP_CMD_RING_SYNC:
            tws_spp_ring_sync(param);
            break;
#endif
		case TWS_SPP_CMD_SET_SYSTEM_VOL:
			tws_spp_set_system_vol(param);
			break;
//Modified by ATX : parker.wei_20180306			
		case TWS_SPP_CMD_A2DP_VOLUME_CHANGE:
			tws_spp_a2dp_vol_change(param);	
            break;		
            //Modified by ATX : Haorong.Wu_20180507
            case TWS_SPP_CMD_VOICE_REPORT:
			tws_spp_voice_report((APP_STATUS_INDICATION_T)param);	
            break;
//Modified by ATX : Parke.Wei_20180316	
#ifdef _PROJ_2000IZ_C003_
		case TWS_SPP_CMD_UPDATE_PEER_STATE:
			tws_spp_update_peer_state(param);
            break;
#endif
	//Modified by ATX : Parke.Wei_20180413
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
		case TWS_SPP_CMD_SLAVE_ENTER_CMD:
			tws_spp_master_ctrl_slave_enter_ota(param);
			break;
#endif

    }
}



//#endif

