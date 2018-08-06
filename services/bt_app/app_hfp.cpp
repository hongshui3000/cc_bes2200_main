//#include "mbed.h"
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
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "nvrecord_dev.h"
#include "bt_drv_interface.h"


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


#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"

#include "nvrecord.h"

#include "apps.h"
#include "resources.h"

#include "app_bt_media_manager.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_spp.h"
#endif

/* hfp */
//HfChannel hf_channel;
int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len);
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len);

int store_voicepcm_buffer(unsigned char *buf, unsigned int len);
int store_voicecvsd_buffer(unsigned char *buf, unsigned int len);
int store_voicemsbc_buffer(unsigned char *buf, unsigned int len);

#ifdef __BT_ONE_BRING_TWO__
extern BtDeviceRecord record2_copy;
extern uint8_t record2_avalible;
#endif

#ifndef _SCO_BTPCM_CHANNEL_
struct HF_SENDBUFF_CONTROL_T  hf_sendbuff_ctrl;
#endif

#if defined(SCO_LOOP)
#define HF_LOOP_CNT (20)
#define HF_LOOP_SIZE (360)

uint8_t hf_loop_buffer[HF_LOOP_CNT*HF_LOOP_SIZE];
uint32_t hf_loop_buffer_len[HF_LOOP_CNT];
uint32_t hf_loop_buffer_valid = 1;
uint32_t hf_loop_buffer_size = 0;
char hf_loop_buffer_w_idx = 0;
#endif

#if defined(HFP_1_6_ENABLE)
void bt_sco_set_current_codecid(uint16_t id);
#endif

typedef void (*btapp_sniffer_sco_status_callback)(uint16_t conhdl,uint8_t sco_status,uint8_t airmode,uint32_t bandwidth);

btapp_sniffer_sco_status_callback sniffer_sco_status_callback;


#ifdef __TWS_CALL_DUAL_CHANNEL__


extern uint8_t app_poweroff_flag;
uint8_t slave_sco_active=0;
void btapp_sniffer_sco_start(uint16_t conhdl,uint8_t sco_status,uint8_t airmode,uint32_t bandwidth)
{
    TRACE("conhdl=%x,scostatus=%x,airmode=%x,samplerate=%x ct=%d",conhdl,sco_status,airmode,bandwidth,hal_sys_timer_get());

#if defined(HFP_1_6_ENABLE)
    if(airmode == 3)    
    {        
        bt_sco_set_current_codecid(HF_SCO_CODEC_MSBC);         
    }
    else if(airmode == 2)
    {
        bt_sco_set_current_codecid(HF_SCO_CODEC_CVSD);       
    }
#endif   
    if(app_poweroff_flag ==1)
        return;
    
    if((sco_status == 1 || sco_status == 3) && slave_sco_active ==0)
    {
        TRACE("CHANMAP=:");
        DUMP8("%02x",(unsigned char *)0xc0004c10,10);
        TRACE("\n instant=%x,mode=%x",*(unsigned int *)0xc0004c1c,*(unsigned int *)0xc0004c20);
        slave_sco_active = 1;
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,0);
    }
    else if(sco_status == 0 || sco_status == 2)
    {
        if(sco_status ==0)
        {
            btdrv_dynamic_patch_sco_status_clear();

        }
        slave_sco_active = 0;        
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
    }
        
}
#endif




extern struct BT_DEVICE_T  app_bt_device;

osPoolId   app_hfp_hfcommand_mempool = NULL;
osPoolDef (app_hfp_hfcommand_mempool, 4, HfCommand);

int app_hfp_hfcommand_mempool_init(void)
{
    if (app_hfp_hfcommand_mempool == NULL)
        app_hfp_hfcommand_mempool = osPoolCreate(osPool(app_hfp_hfcommand_mempool));
    return 0;
}

int app_hfp_hfcommand_mempool_calloc(HfCommand **hf_cmd_p)
{
    *hf_cmd_p = (HfCommand *)osPoolCAlloc(app_hfp_hfcommand_mempool);
    return 0;
}

int app_hfp_hfcommand_mempool_free(HfCommand *hf_cmd_p)
{
    if (hf_cmd_p) {
        osPoolFree(app_hfp_hfcommand_mempool, (void *)hf_cmd_p);
    }
    return 0;    
}

void app_hfp_init(void)
{
    app_hfp_hfcommand_mempool_init();
    app_bt_device.curr_hf_channel_id = BT_DEVICE_ID_1;
    app_bt_device.hf_mute_flag = 0;

    for(uint8_t i=0; i<BT_DEVICE_NUM; i++)
    {
        app_bt_device.hfchan_call[i] = 0;
        app_bt_device.hfchan_callSetup[i] = 0;
        app_bt_device.hf_audio_state[i] = 0;
        app_bt_device.hf_conn_flag[i] = 0;
        app_bt_device.hf_voice_en[i] = 0;
    }

#ifdef __TWS_CALL_DUAL_CHANNEL__
    sniffer_sco_status_callback = btapp_sniffer_sco_start;
#else
    sniffer_sco_status_callback = NULL;
#endif   
    
}

//Modified by ATX : parker.wei_20180309 
#ifdef __HF_ATCMD_SEND_ 
//*************************************************************************************************************************************

const static uint32_t SEED[4] = {0x3A699E0D, 0x05A5AA75,0x3B9AC995,0x04C4B445};
const static uint8_t PRIME[12] = {3, 7, 11, 13, 17,19,23,29, 191, 193, 197, 199};
 
int get_sign(uint8_t *bt_addr /*6 bytes space*/,uint8_t *out_sign /*33 bytes space*/){
    uint64_t* signTemp = (uint64_t*)out_sign;
    uint16_t addrTemp[4];
    uint8_t i = 0;
    uint8_t j = 0;
    
    if (bt_addr == NULL || out_sign == NULL) {
        return 0;
    }
 
    for (j = 0; j < 4; j++) {
        signTemp[j] = SEED[j];
        addrTemp[j] = *((uint16_t*)bt_addr + j % 3)
            + ((bt_addr[3] << 5) + (bt_addr[4] << 7) + (bt_addr[5] << 9)) * PRIME[8 + j];
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            signTemp[j] ^= (signTemp[j] >> PRIME[0 + j]) + (signTemp[j] << PRIME[4 + j]) + addrTemp[j];
        }
    }
 
  
  for (i= 0; i < 4; i++ ) {
    sprintf((char*)(signTemp + i), "%08x", signTemp[i]);
  }
  out_sign[31] = 0;   //[20180307]Change the visible length of the sign string equals to 31
  
  return -1;
}

bool app_hfp_send_accessory_cmd()
{
	uint8_t i;
	BtStatus res = BT_STATUS_LAST_CODE;
	char at[20+33+8]="AT+XAMZ=4D01-0001-";
	char at_end[]="-0001,10";
	
    uint8_t sign_key[33]={0};
	//uint8_t *local_dev=NULL;
	BT_BD_ADDR local_dev;
    
	ME_ReadLocalBdAddr(&local_dev);
		
	//strcat((char*)at,(char*)e);
	//TRACE("%s", __func__);
   // DUMP8("0x%x ",local_dev.addr,6);
	
	if(!get_sign(local_dev.addr,sign_key))
		 return 0;

    strcat(at,(const char*)sign_key);
	strcat(at,(const char*)at_end);
	for(i=0; i<BT_DEVICE_NUM; i++)
	{
		if((app_bt_device.hf_channel[i].state == HF_STATE_OPEN))
		{			 
			HfCommand *hf_cmd_p;
			app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);		
			if (hf_cmd_p){
				res = HF_SendAtCommand(&app_bt_device.hf_channel[i],(const char*)at, hf_cmd_p);
				if (res == BT_STATUS_PENDING){
					TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);
				}else{
					TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res); 				   
					app_hfp_hfcommand_mempool_free(hf_cmd_p);
				}
			}
		}
	}

	return 1;


}

#endif


#if (HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_BATTERY_REPORT) || (HF_SDK_FEATURES & HF_FEATURE_HF_INDICATORS)
#ifdef __BT_ONE_BRING_TWO__
    static uint8_t battery_level[BT_DEVICE_NUM] = {0xff, 0xff};
#else
    static uint8_t battery_level[BT_DEVICE_NUM] = {0xff};
#endif

int app_hfp_battery_report_reset(uint8_t bt_device_id)
{
    ASSERT(bt_device_id < BT_DEVICE_NUM, "bt_device_id error");
    battery_level[bt_device_id] = 0xff;
    return 0;
}

int app_hfp_battery_report(uint8_t level)
{
    // Care: BT_DEVICE_NUM<-->{0xff, 0xff, ...}
    BtStatus status = BT_STATUS_LAST_CODE;

    uint8_t i;
    int nRet = 0;
    HfCommand *hf_cmd_p = NULL;

    if (level>9)
        return -1;

    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if(app_bt_device.hf_channel[i].state == HF_STATE_OPEN){
            if ((app_bt_device.hf_channel[i].gwFeatures & HF_GW_FEATURE_HF_INDICATORS) && (HF_SDK_FEATURES & HF_FEATURE_HF_INDICATORS)){
                if (battery_level[i] != level){
                    app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);
                    if (hf_cmd_p){
                        TRACE("app_hfp_battery_report  HF_Update_Hf_Indicators_Battery_Level\n");
                        status = HF_Update_Hf_Indicators_Battery_Level(&app_bt_device.hf_channel[i], hf_cmd_p, level);
                    }
                }
            }else if(app_bt_device.hf_channel[i].bt_accessory_feature & HF_CUSTOM_FEATURE_BATTERY_REPORT){
                if (battery_level[i] != level){
                    app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);
                    if (hf_cmd_p){
                        status = HF_Battery_Report(&app_bt_device.hf_channel[i], hf_cmd_p, level);
                    }
                }
            }
            if (BT_STATUS_PENDING == status){
                battery_level[i] = level;
            }else{                        
                app_hfp_hfcommand_mempool_free(hf_cmd_p);
                nRet = -1;
            }
        }else{
             battery_level[i] = 0xff;
             nRet = -1;
        }
    }
    return nRet;
}
#endif


#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
int app_hfp_siri_report()
{
    uint8_t i;
    BtStatus res = BT_STATUS_LAST_CODE;
    
    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if((app_bt_device.hf_channel[i].state == HF_STATE_OPEN))
        {            
            HfCommand *hf_cmd_p;
            app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);        
            if (hf_cmd_p){
                res = HF_Siri_Report(&app_bt_device.hf_channel[i], hf_cmd_p);
                if (res == BT_STATUS_PENDING){
                    TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);
                }else{
                    TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);                    
                    app_hfp_hfcommand_mempool_free(hf_cmd_p);
                }
            }
        }
    }
    return 0;
}

//Modified by ATX : Leon.He_20180108: Sync siri code from bes_tws
int app_hfp_siri_voice(bool en)
{

    uint8_t i;
    BtStatus res;
    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if((app_bt_device.hf_channel[i].state == HF_STATE_OPEN))
        {
            res = HF_EnableVoiceRecognition(&app_bt_device.hf_channel[i], en, &app_bt_device.hf_command[i]);
            if (res == BT_STATUS_PENDING)
            {
                TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);
            }
            else
            {
                TRACE("[%s] Line = %d, res = %d", __func__, __LINE__, res);
            }
        }
    }
    return 0;
}
#endif

//Modified by ATX : Leon.He_20171123
#ifdef __ONLY_REPORT_IMCOMING_CALL_ONCE_
static uint8_t incoming_call_report_count = 0;
#endif

#if !defined(FPGA) && defined(__EARPHONE__)
void hfp_app_status_indication(enum BT_DEVICE_ID_T chan_id,HfCallbackParms *Info)
{
#ifdef __TWS_RECONNECT_USE_BLE__
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
#endif
    switch(Info->event)
    {
        case HF_EVENT_SERVICE_CONNECTED:
            break;
        case HF_EVENT_SERVICE_DISCONNECTED:
            break;
        case HF_EVENT_CURRENT_CALL_STATE:
            TRACE("!!!HF_EVENT_CURRENT_CALL_STATE  chan_id:%d, call_number:%s\n", chan_id,Info->p.callListParms->number);
            if(app_bt_device.hfchan_callSetup[chan_id] == HF_CALL_SETUP_IN){
                //////report incoming call number

//#ifndef __TWS_CALL_DUAL_CHANNEL__
#ifdef __ENABLE_CALL_NUMBER_REPORT_//Modified by ATX : Parker.Wei_2018328
                app_status_set_num(Info->p.callListParms->number);
                app_voice_report(APP_STATUS_INDICATION_CALLNUMBER,chan_id);
#endif
#ifdef __TWS_RECONNECT_USE_BLE__

#ifndef __TWS_RECONNECT_IN_CALL_STATE_ //Modified by ATX : Parker.Wei_20180625 allow TWS reconnect when in call state;
                if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_CLOSE_BLE_SCAN);
                }

                if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                    if (app_tws_ble_reconnect.adv_func)
                        app_tws_ble_reconnect.adv_func(APP_TWS_CLOSE_BLE_ADV);
                }
#endif

#endif

                
            }
            break;
        case HF_EVENT_CALL_IND:
            if(Info->p.call == HF_CALL_NONE && app_bt_device.hfchan_call[chan_id] == HF_CALL_ACTIVE){
                //////report call hangup voice
                TRACE("!!!HF_EVENT_CALL_IND  APP_STATUS_INDICATION_HANGUPCALL  chan_id:%d\n",chan_id);
                app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP_MEDIA,BT_STREAM_VOICE,chan_id,0,0,0);
                ///disable media prompt                
                if(app_bt_device.hf_endcall_dis[chan_id] == false)               
                {
                    TRACE("HANGUPCALL PROMPT");
                    app_voice_report(APP_STATUS_INDICATION_HANGUPCALL,chan_id);
                }
#ifdef __TWS_RECONNECT_USE_BLE__
                if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
                }

                if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                    if (app_tws_ble_reconnect.adv_func)
                        app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
                }
#endif

                
            }
            break;
        case HF_EVENT_CALLSETUP_IND:
            if(Info->p.callSetup == HF_CALL_SETUP_NONE &&
                (app_bt_device.hfchan_call[chan_id] != HF_CALL_ACTIVE) &&
                (app_bt_device.hfchan_callSetup[chan_id] != HF_CALL_SETUP_NONE)){
                ////check the call refuse and stop media of (ring and call number)
                TRACE("!!!HF_EVENT_CALLSETUP_IND  APP_STATUS_INDICATION_REFUSECALL  chan_id:%d\n",chan_id);
//Modified by ATX : Leon.He_20171123
#ifdef __ONLY_REPORT_IMCOMING_CALL_ONCE_
                incoming_call_report_count = 0;
#endif
                app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP_MEDIA,BT_STREAM_VOICE,chan_id,0,0,0);
                app_voice_report(APP_STATUS_INDICATION_REFUSECALL,chan_id);/////////////duÁ½Éù
#ifdef __TWS_RECONNECT_USE_BLE__
                if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
                }

                if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                    if (app_tws_ble_reconnect.adv_func)
                        app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
                }
#endif


                
            }
            break;
        case HF_EVENT_AUDIO_CONNECTED:
            TRACE("!!!HF_EVENT_AUDIO_CONNECTED  APP_STATUS_INDICATION_ANSWERCALL  chan_id:%d\n",chan_id);
//Modified by ATX : Leon.He_20171123
#ifdef __ONLY_REPORT_IMCOMING_CALL_ONCE_
            incoming_call_report_count = 0;
#endif
          //  app_voice_report(APP_STATUS_INDICATION_ANSWERCALL,chan_id);//////////////duÒ»Éù
            break;
        case HF_EVENT_RING_IND:
			 TRACE("!!!HF_EVENT_RING_IND   chan_id:%d\n",chan_id);
//Modified by ATX : Leon.He_20171123
#ifdef __ONLY_REPORT_IMCOMING_CALL_ONCE_
        	if( incoming_call_report_count++ == 0)
        		app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,chan_id);
        	else
        	    app_voice_report(APP_STATUS_INDICATION_CALLNUMBER,chan_id);
#else
    			app_voice_report(APP_STATUS_INDICATION_INCOMINGCALL,chan_id);
#endif

            break;
        default:
            break;
    }
}
#endif


static struct BT_DEVICE_ID_DIFF chan_id_flag;
#ifdef __BT_ONE_BRING_TWO__
void hfp_chan_id_distinguish(HfChannel *Chan)
{
    if(Chan == &app_bt_device.hf_channel[BT_DEVICE_ID_1]){
        chan_id_flag.id = BT_DEVICE_ID_1;
        chan_id_flag.id_other = BT_DEVICE_ID_2;
    }else if(Chan == &app_bt_device.hf_channel[BT_DEVICE_ID_2]){
        chan_id_flag.id = BT_DEVICE_ID_2;
        chan_id_flag.id_other = BT_DEVICE_ID_1;
    }
}
#endif

int hfp_volume_get(void)
{
    int vol = app_bt_stream_volume_get_ptr()->hfp_vol - 2;

    if (vol > 15)
        vol = 15;
    if (vol < 0)
        vol = 0;
    
    return (vol);
}

void hfp_volume_local_set(int8_t vol)
{
    app_bt_stream_volume_get_ptr()->hfp_vol = vol;
    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif
}

int hfp_volume_set(int vol)
{
    if (vol > 15)
        vol = 15;
    if (vol < 0)
        vol = 0;
    
    hfp_volume_local_set(vol+2);
    return (app_bt_stream_volumeset(vol+2));
}

extern void app_bt_profile_connect_manager_hf(enum BT_DEVICE_ID_T id, HfChannel *Chan, HfCallbackParms *Info);

#if HF_VERSION_1_6 == XA_ENABLED
HfCommand hf_codec_sel_command;
#endif


void app_tws_stop_sniff(void);

//Modified by ATX : Leon.He_20180308: postpone to sync vol change to peer, avoid mail box overflow
struct HFP_VOL_CHANGE_T {
	int8_t vol;
};
static struct HFP_VOL_CHANGE_T hfp_vol_change;
static void hfp_postpone_to_sync_vol_change_handler(void const *param)
{
    struct HFP_VOL_CHANGE_T *_p = (struct HFP_VOL_CHANGE_T *)param;
    int8_t vol = _p->vol;
    tws_player_hfp_vol_change(vol);
}
osTimerDef (hfp_postpone_to_sync_vol_change, hfp_postpone_to_sync_vol_change_handler);
static osTimerId POSSIBLY_UNUSED hfp_postpone_to_sync_vol_change_timer = NULL;
#define HFP_POSTPONE_TO_SYNC_VOL_TIMEOUT	700
static void hfp_postpone_to_sync_vol_change(int8_t vol)
{
    TRACE("%s",__func__);
    hfp_vol_change.vol=vol;
	if (hfp_postpone_to_sync_vol_change_timer == NULL)
		hfp_postpone_to_sync_vol_change_timer = osTimerCreate(osTimer(hfp_postpone_to_sync_vol_change), osTimerOnce, &hfp_vol_change);
	osTimerStop(hfp_postpone_to_sync_vol_change_timer);
	osTimerStart(hfp_postpone_to_sync_vol_change_timer, HFP_POSTPONE_TO_SYNC_VOL_TIMEOUT);
}

void hfp_callback(HfChannel *Chan, HfCallbackParms *Info)
{
    HfCommand *hf_cmd_p;
    struct nvrecord_env_t *nvrecord_env;
    struct tws_mode_t *tws_mode;
#ifdef __BT_ONE_BRING_TWO__
    hfp_chan_id_distinguish(Chan);
#else
    chan_id_flag.id = BT_DEVICE_ID_1;
#endif

    switch(Info->event)
    {
    case HF_EVENT_SERVICE_CONNECTED:
        TRACE("::HF_EVENT_SERVICE_CONNECTED  Chan_id:%d\n", chan_id_flag.id);   
#if defined(HFP_1_6_ENABLE)
        app_bt_device.hf_channel[chan_id_flag.id].negotiated_codec = HF_SCO_CODEC_CVSD;
#endif        
        app_bt_profile_connect_manager_hf(chan_id_flag.id, Chan, Info);
        app_bt_device.phone_earphone_mark = 1;
#if HF_VERSION_1_6 == XA_ENABLED
        HF_Codec_Sel_msbc(Chan,&hf_codec_sel_command);
#endif

#if defined( __TWS_RECONNECT_USE_BLE__) && defined(__TWS__)
        nv_record_env_get(&nvrecord_env);
        tws_mode = &(nvrecord_env->tws_mode);
        if ((app_tws_get_mode() != TWSSLAVE) && (tws_mode->mode == TWSSLAVE))
            app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_NCON,0,0);
#endif



#if !defined(FPGA) && defined(__EARPHONE__)
        if(Chan->state == HF_STATE_OPEN){
            ////report connected voice
            app_bt_device.hf_conn_flag[chan_id_flag.id] = 1;
            hfp_app_status_indication(chan_id_flag.id,Info);
        }
#endif
#if (HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_BATTERY_REPORT) || ((HF_SDK_FEATURES & HF_FEATURE_HF_INDICATORS))
        app_hfp_battery_report_reset(chan_id_flag.id);
#endif
        app_bt_stream_volume_ptr_update((uint8_t *)Info->p.remDev->bdAddr.addr);
//        app_bt_stream_hfpvolume_reset();
        app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);
        if (hf_cmd_p){
            if(HF_ReportSpeakerVolume(Chan,hfp_volume_get(), hf_cmd_p) != BT_STATUS_PENDING)
                app_hfp_hfcommand_mempool_free(hf_cmd_p);
        }

#if defined(SPEECH_TX_NS) || defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2FLOAT)
        app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);        
        if (hf_cmd_p){
            if(HF_DisableNREC(Chan,hf_cmd_p) != BT_STATUS_PENDING)                
                app_hfp_hfcommand_mempool_free(hf_cmd_p);
        }
#endif

#ifdef __BT_ONE_BRING_TWO__
        ////if a call is active and start bt open reconnect procedure, process the curr_hf_channel_id
        if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
            app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
        }else{
            app_bt_device.curr_hf_channel_id = chan_id_flag.id;
        }
#endif
        break;
    case HF_EVENT_AUDIO_DATA_SENT:
//      TRACE("::HF_EVENT_AUDIO_DATA_SENT %d\n", Info->event);
#if defined(SCO_LOOP)
        hf_loop_buffer_valid = 1;
#endif
        break;
    case HF_EVENT_AUDIO_DATA:
    {
#ifdef __BT_ONE_BRING_TWO__
    if(app_bt_device.hf_voice_en[chan_id_flag.id])
    {
#endif

#ifndef _SCO_BTPCM_CHANNEL_
        uint32_t idx = 0;
        if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)){
            store_voicebtpcm_m2p_buffer(Info->p.audioData->data, Info->p.audioData->len);

            idx = hf_sendbuff_ctrl.index%HF_SENDBUFF_MEMPOOL_NUM;
            get_voicebtpcm_p2m_frame(&(hf_sendbuff_ctrl.mempool[idx].buffer[0]), Info->p.audioData->len);
            hf_sendbuff_ctrl.mempool[idx].packet.data = &(hf_sendbuff_ctrl.mempool[idx].buffer[0]);
            hf_sendbuff_ctrl.mempool[idx].packet.dataLen = Info->p.audioData->len;
            hf_sendbuff_ctrl.mempool[idx].packet.flags = BTP_FLAG_NONE;
            if(!app_bt_device.hf_mute_flag){
                HF_SendAudioData(Chan, &hf_sendbuff_ctrl.mempool[idx].packet);
            }
            hf_sendbuff_ctrl.index++;
        }
#endif

#ifdef __BT_ONE_BRING_TWO__
    }
#endif
    }

#if defined(SCO_LOOP)
    memcpy(hf_loop_buffer + hf_loop_buffer_w_idx*HF_LOOP_SIZE, Info->p.audioData->data, Info->p.audioData->len);
    hf_loop_buffer_len[hf_loop_buffer_w_idx] = Info->p.audioData->len;
    hf_loop_buffer_w_idx = (hf_loop_buffer_w_idx+1)%HF_LOOP_CNT;
    ++hf_loop_buffer_size;

    if (hf_loop_buffer_size >= 18 && hf_loop_buffer_valid == 1) {
        hf_loop_buffer_valid = 0;
        idx = hf_loop_buffer_w_idx-17<0?(HF_LOOP_CNT-(17-hf_loop_buffer_w_idx)):hf_loop_buffer_w_idx-17;
        pkt.flags = BTP_FLAG_NONE;
        pkt.dataLen = hf_loop_buffer_len[idx];
        pkt.data = hf_loop_buffer + idx*HF_LOOP_SIZE;
        HF_SendAudioData(Chan, &pkt);
    }
#endif
        break;
    case HF_EVENT_SERVICE_DISCONNECTED:
        TRACE("::HF_EVENT_SERVICE_DISCONNECTED Chan_id:%d, reason=%x\n", chan_id_flag.id,Info->p.remDev->discReason);
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM,0,0);
#if defined(HFP_1_6_ENABLE)
        app_bt_device.hf_channel[chan_id_flag.id].negotiated_codec = HF_SCO_CODEC_CVSD;
#endif
#if !defined(FPGA) && defined(__EARPHONE__)
        if(app_bt_device.hf_conn_flag[chan_id_flag.id] ){
            ////report device disconnected voice
            app_bt_device.hf_conn_flag[chan_id_flag.id] = 0;
            hfp_app_status_indication(chan_id_flag.id,Info);
        }
#endif

#ifdef __BT_ONE_BRING_TWO__
        if(app_bt_device.hf_conn_flag[chan_id_flag.id]){
            app_bt_stream_volume_ptr_update(app_bt_device.hf_channel[chan_id_flag.id].cmgrHandler.remDev->bdAddr.addr);
        }else if(app_bt_device.hf_conn_flag[chan_id_flag.id_other]){
            app_bt_stream_volume_ptr_update(app_bt_device.hf_channel[chan_id_flag.id_other].cmgrHandler.remDev->bdAddr.addr);
        }else{
            app_bt_stream_volume_ptr_update(NULL);
        }
#else
        app_bt_stream_volume_ptr_update(NULL);
#endif
        app_bt_profile_connect_manager_hf(chan_id_flag.id, Chan, Info);
        for (uint8_t i=0; i<BT_DEVICE_NUM; i++){
            if (Chan == &(app_bt_device.hf_channel[i])){
                app_bt_device.hfchan_call[i] = 0;
                app_bt_device.hfchan_callSetup[i] = 0;
                app_bt_device.hf_audio_state[i] = 0;
                app_bt_device.hf_conn_flag[i] = 0;
                app_bt_device.hf_voice_en[i] = 0;
            }
        }
        break;
    case HF_EVENT_CALL_IND:
        TRACE("::HF_EVENT_CALL_IND  chan_id:%d, call:%d\n",chan_id_flag.id,Info->p.call);
        if(Info->p.call == HF_CALL_ACTIVE)
        {
            ///call is active so check if it's a outgoing call
            if(app_bt_device.hfchan_callSetup[chan_id_flag.id] == HF_CALL_SETUP_ALERT)
            {
                TRACE("HF CALLACTIVE TIME=%d",hal_sys_timer_get());
                if(TICKS_TO_MS(hal_sys_timer_get()-app_bt_device.hf_callsetup_time[chan_id_flag.id])<1000)
                {
                    TRACE("DISABLE HANGUPCALL PROMPT");
                    app_bt_device.hf_endcall_dis[chan_id_flag.id] = true;
                }
            }
            /////stop media of (ring and call number) and switch to sco
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_SWITCHTO_SCO,BT_STREAM_VOICE,chan_id_flag.id,0,0,0);              
            
        }

#if !defined(FPGA) && defined(__EARPHONE__)
        hfp_app_status_indication(chan_id_flag.id,Info);
#endif


        if(Info->p.call == HF_CALL_ACTIVE){
            app_bt_device.curr_hf_channel_id = chan_id_flag.id;
        }
#ifdef __BT_ONE_BRING_TWO__
        else if((Info->p.call == HF_CALL_NONE)&&(app_bt_device.hfchan_call[chan_id_flag.id_other] == HF_CALL_ACTIVE)){
            app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
        }
#endif
//        TRACE("!!!HF_EVENT_CALL_IND  curr_hf_channel_id:%d\n",app_bt_device.curr_hf_channel_id);
        app_bt_device.hfchan_call[chan_id_flag.id] = Info->p.call;
        if(Info->p.call == HF_CALL_NONE)
        {
            app_bt_device.hf_endcall_dis[chan_id_flag.id] = false;
        }
#if defined( __BT_ONE_BRING_TWO__)
#if !defined(HFP_1_6_ENABLE)    
#ifdef CHIP_BEST1000
        if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
#endif
#ifdef CHIP_BEST2000
        if(1)
#endif
        {
            ////a call is active:
            if(app_bt_device.hfchan_call[chan_id_flag.id] == HF_CALL_ACTIVE){
                if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
                    app_bt_device.curr_hf_channel_id = chan_id_flag.id;
                    Me_switch_sco(app_bt_device.hf_channel[chan_id_flag.id].cmgrHandler.scoConnect->scoHciHandle);
                }
                app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
                app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
            }else{
                ////a call is hung up:
                ///if one device  setup a sco connect so get the other device's sco state, if both connect mute the earlier one
                if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
                    app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
                    app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
                }
            }
        }
#endif
#endif
        break;
    case HF_EVENT_CALLSETUP_IND:
        TRACE("::HF_EVENT_CALLSETUP_IND chan_id:%d, callSetup = %d\n", chan_id_flag.id,Info->p.callSetup);
#if !defined(FPGA) && defined(__EARPHONE__)
        hfp_app_status_indication(chan_id_flag.id,Info);
#endif

#ifdef __BT_ONE_BRING_TWO__
        if(Info->p.callSetup == 0){
            //do nothing
        }else{
	        if((app_bt_device.hfchan_call[chan_id_flag.id_other] == HF_CALL_ACTIVE)||((app_bt_device.hfchan_callSetup[chan_id_flag.id_other] == HF_CALL_SETUP_IN)&&(app_bt_device.hfchan_call[chan_id_flag.id] != HF_CALL_ACTIVE))){
	            app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
	        }else{
	            app_bt_device.curr_hf_channel_id = chan_id_flag.id;
	        }
        }
        TRACE("!!!HF_EVENT_CALLSETUP_IND curr_hf_channel_id:%d\n",app_bt_device.curr_hf_channel_id);
#endif
        app_bt_device.hfchan_callSetup[chan_id_flag.id] = Info->p.callSetup;
        /////call is alert so remember this time 
        if(app_bt_device.hfchan_callSetup[chan_id_flag.id] ==HF_CALL_SETUP_ALERT )
        {
             TRACE("HF CALLSETUP TIME=%d",hal_sys_timer_get());
            app_bt_device.hf_callsetup_time[chan_id_flag.id] = hal_sys_timer_get();
        }
        if(app_bt_device.hfchan_callSetup[chan_id_flag.id]== HF_CALL_SETUP_IN){
            app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);            
            if (hf_cmd_p){
                if (HF_ListCurrentCalls(Chan, hf_cmd_p) != BT_STATUS_PENDING)
                    app_hfp_hfcommand_mempool_free(hf_cmd_p);
            }
        }

        app_tws_stop_sniff();

        break;
    case HF_EVENT_CURRENT_CALL_STATE:
        TRACE("::HF_EVENT_CURRENT_CALL_STATE  chan_id:%d\n", chan_id_flag.id);
#if !defined(FPGA) && defined(__EARPHONE__)
        hfp_app_status_indication(chan_id_flag.id,Info);
#endif
        break;
    case HF_EVENT_AUDIO_CONNECTED:
         if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_2)
         {
            *(volatile uint16_t*)(0xc0003bec) = 0xffff;         //rssi low
         }		
        if(Info->status == BT_STATUS_SUCCESS){	
#if defined(HFP_1_6_ENABLE)            
            TRACE("::HF_EVENT_AUDIO_CONNECTED  chan_id:%d, codec_id %d\n", chan_id_flag.id, app_bt_device.hf_channel[chan_id_flag.id].negotiated_codec);

            bt_sco_set_current_codecid(app_bt_device.hf_channel[chan_id_flag.id].negotiated_codec);
#else
		 TRACE("::HF_EVENT_AUDIO_CONNECTED  chan_id:%d\n", chan_id_flag.id);
#endif
#if !defined(FPGA) && defined(__EARPHONE__)
	        hfp_app_status_indication(chan_id_flag.id,Info);
#endif

	        app_bt_device.phone_earphone_mark = 0;
	        app_bt_device.hf_mute_flag = 0;

	        app_bt_device.hf_audio_state[chan_id_flag.id] = HF_AUDIO_CON;
            
#if defined(__FORCE_REPORTVOLUME_SOCON__)            
            app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);
            if (hf_cmd_p){
                if(HF_ReportSpeakerVolume(Chan,hfp_volume_get(), hf_cmd_p) != BT_STATUS_PENDING)
                    app_hfp_hfcommand_mempool_free(hf_cmd_p);
            }
#endif

#ifdef __BT_ONE_BRING_TWO__

#if !defined(HFP_1_6_ENABLE) 
#ifdef CHIP_BEST1000
	        if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
#endif
#ifdef CHIP_BEST2000
              if(1)
#endif
	        {
	            if(app_bt_device.hfchan_call[chan_id_flag.id] == HF_CALL_ACTIVE){
	                Me_switch_sco(app_bt_device.hf_channel[chan_id_flag.id].cmgrHandler.scoConnect->scoHciHandle);
	                app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
	                app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
	            }else if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
	                app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
	                app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
	            }
	        }
	        else
#endif
	        {
	            ///if one device  setup a sco connect so get the other device's sco state, if both connect mute the earlier one
	            if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
	                app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_DISABLE;
	            }
	            app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_ENABLE;
	        }
            
	        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM);

	        if(MEC(activeCons) != 2){
	            ////A call is active, set BAM_NOT_ACCESSIBLE mode.
	            //ME_SetAccessibleMode(BAM_NOT_ACCESSIBLE, NULL);
	           // app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_NOT_ACCESSIBLE);
	        }
#else
#ifdef __TWS_CALL_DUAL_CHANNEL__
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,0);
            
            if(app_tws_get_mode() == TWSMASTER)
            {
                tws_player_set_hfp_vol(hfp_volume_get());            
            }

#else
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
#endif



//	        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
#endif
        }
        break;
    case HF_EVENT_AUDIO_DISCONNECTED:
        TRACE("::HF_EVENT_AUDIO_DISCONNECTED  chan_id:%d\n", chan_id_flag.id);
        if(app_bt_device.hfchan_call[chan_id_flag.id] == HF_CALL_ACTIVE){
            app_bt_device.phone_earphone_mark = 1;
        }

        app_bt_device.hf_audio_state[chan_id_flag.id] = HF_AUDIO_DISCON;

#ifdef __TWS__
        btdrv_dynamic_patch_sco_status_clear();
#ifdef __TWS_RECONNECT_USE_BLE__
            nv_record_env_get(&nvrecord_env);
            TRACE("audio disconnect so start ble scan for tws master ,app_tws_get_mode=%x,nvrecord_env->tws_mode.mode=%x,app_tws_ble_reconnect.scan_func=%x",
                app_tws_get_mode(),nvrecord_env->tws_mode.mode,app_tws_ble_reconnect.scan_func);

            if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                if (app_tws_ble_reconnect.scan_func)
                    app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
            }

            if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                if (app_tws_ble_reconnect.adv_func)
                    app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
            }
#endif
#endif

        
#ifdef __BT_ONE_BRING_TWO__
        if(app_bt_device.hf_channel[chan_id_flag.id_other].state != HF_STATE_OPEN)
            ////one device disconnected, set accessible mode to BAM_CONNECTABLE_ONLY
            //ME_SetAccessibleMode(BAM_CONNECTABLE_ONLY, NULL);
           // app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY);

        TRACE("!!!HF_EVENT_AUDIO_DISCONNECTED  hfchan_call[chan_id_flag.id_other]:%d\n",app_bt_device.hfchan_call[chan_id_flag.id_other]);
        if((app_bt_device.hfchan_call[chan_id_flag.id_other] == HF_CALL_ACTIVE)||(app_bt_device.hfchan_callSetup[chan_id_flag.id_other] == HF_CALL_SETUP_IN)){
            app_bt_device.curr_hf_channel_id = chan_id_flag.id_other;
            TRACE("!!!HF_EVENT_AUDIO_DISCONNECTED  app_bt_device.curr_hf_channel_id:%d\n",app_bt_device.curr_hf_channel_id);
        }else{
            app_bt_device.curr_hf_channel_id = chan_id_flag.id;
        }

        app_bt_device.hf_voice_en[chan_id_flag.id] = HF_VOICE_DISABLE;
        if(app_bt_device.hf_audio_state[chan_id_flag.id_other] == HF_AUDIO_CON){
            app_bt_device.hf_voice_en[chan_id_flag.id_other] = HF_VOICE_ENABLE;
            TRACE("chan_id:%d AUDIO_DISCONNECTED, then enable id_other voice",chan_id_flag.id);
        }
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,chan_id_flag.id,MAX_RECORD_NUM,0,0);
#else

        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
#endif
        break;
    case HF_EVENT_RING_IND:
        TRACE("::HF_EVENT_RING_IND  chan_id:%d\n", chan_id_flag.id);
#if !defined(FPGA) && defined(__EARPHONE__)
        if(app_bt_device.hf_audio_state[chan_id_flag.id] != HF_AUDIO_CON){
#if defined(TWS_RING_SYNC) && defined(__TWS__)
            tws_player_ring_sync(Info->event);
            hfp_app_status_indication(chan_id_flag.id,Info);
#else
            hfp_app_status_indication(chan_id_flag.id,Info);
#endif
        }
#endif
        break;
    case HF_EVENT_SPEAKER_VOLUME:
        TRACE("::HF_EVENT_SPEAKER_VOLUME  chan_id:%d,speaker gain = %x\n", chan_id_flag.id,Info->p.ptr);
        hfp_volume_set((int)(uint32_t)Info->p.ptr);
#ifdef __TWS_CALL_DUAL_CHANNEL__
#ifdef __TWS__
        //    tws_player_set_hfp_vol(hfp_volume_get());  
//Modified by ATX : parker.wei_20180308
        hfp_postpone_to_sync_vol_change(hfp_volume_get());
        if(hfp_volume_get()>=15)
	   		app_voice_report(APP_STATUS_INDICATION_VOL_MAX,0);
        else
        if(hfp_volume_get()<=0)
   	   		app_voice_report(APP_STATUS_INDICATION_VOL_MIN,0);
#endif
#endif
    
        break;

#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
    case HF_EVENT_SIRI_STATUS:
        TRACE("[%s] siri status = %d", __func__, Info->p.ptr);
        break;
#endif

	case HF_EVENT_BES_TEST:
		TRACE("HF_EVENT_BES_TEST content = %d", Info->p.ptr);
		break;

    case HF_EVENT_READ_AG_INDICATORS_STATUS:
        TRACE("HF_EVENT_READ_AG_INDICATORS_STATUS  assigned_num:%x, status:%x", ((AgIndicatorsStatus *)Info->p.ptr)->assigned_num,((AgIndicatorsStatus *)Info->p.ptr)->status);
        break;
    case HF_EVENT_COMMAND_COMPLETE:
        TRACE("::HF_EVENT_COMMAND_COMPLETE  chan_id:%d %x\n", chan_id_flag.id, (HfCommand *)Info->p.ptr);
        if (Info->p.ptr)
            app_hfp_hfcommand_mempool_free((HfCommand *)Info->p.ptr);
        break;
//Modified by ATX : Leon.He_20171123
	case HF_EVENT_CALLHELD_IND:
		 TRACE("::HF_EVENT_CALLHELD_IND  chan_id:%d %x HELD_STATUS = %d \n",chan_id_flag.id, (HfCommand *)Info->p.ptr,Info->p.callHeld);
		 if(Info->p.callHeld == HF_CALL_HELD_ACTIVE){
			 app_bt_device.hf_call_held = HF_CALL_HELD_ACTIVE;
		 }else if(Info->p.callHeld == HF_CALL_HELD_NONE){
			 app_bt_device.hf_call_held = HF_CALL_HELD_NONE;
		 }else if(Info->p.callHeld == HF_CALL_HELD_NO_ACTIVE){
			 app_bt_device.hf_call_held = HF_CALL_HELD_NO_ACTIVE;
		 }else{
			TRACE("UNKNOWN CMD.IGNORE");
		 } 
		 break;
    default:
        break;

    }
}

#ifdef __EARPHONE__
/////////profile safely exit
BtStatus LinkDisconnectDirectly(void)
{
    OS_LockStack();

    if(app_bt_device.hf_channel[BT_DEVICE_ID_1].state == HF_STATE_OPEN)
    {
//        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev);
        HF_DisconnectServiceLink(&app_bt_device.hf_channel[BT_DEVICE_ID_1]);
    }
    if(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].stream.state == AVDTP_STRM_STATE_STREAMING ||
        app_bt_device.a2dp_stream[BT_DEVICE_ID_1].stream.state == AVDTP_STRM_STATE_OPEN)
    {
//        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev);
        A2DP_CloseStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_1]);
    }

#ifdef __BT_ONE_BRING_TWO__

#if 0
    if(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_OPEN)
    {
        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev);
    }
#endif
    if(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_OPEN)
    {
//        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev);
        HF_DisconnectServiceLink(&app_bt_device.hf_channel[BT_DEVICE_ID_2]);
    }
    if(app_bt_device.a2dp_stream[BT_DEVICE_ID_2].stream.state == AVDTP_STRM_STATE_STREAMING ||
        app_bt_device.a2dp_stream[BT_DEVICE_ID_2].stream.state == AVDTP_STRM_STATE_OPEN)
    {
//        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev);
        A2DP_CloseStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_2]);
    }

#endif
    OS_UnlockStack();

    return BT_STATUS_SUCCESS;
}
#endif

uint8_t btapp_hfp_get_call_state(void)
{
    uint8_t i;
    for(i=0;i<BT_DEVICE_NUM;i++)
   {
        if(app_bt_device.hfchan_call[i] == HF_CALL_ACTIVE)
            return 1;
    }
    return 0;
}


void btapp_hfp_report_speak_gain(void)
{

    uint8_t i;


    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if((app_bt_device.hf_channel[i].state == HF_STATE_OPEN)){
            HfCommand *hf_cmd_p;
            app_hfp_hfcommand_mempool_calloc(&hf_cmd_p);        
            if (hf_cmd_p){
                if (HF_ReportSpeakerVolume(&app_bt_device.hf_channel[i],hfp_volume_get(), hf_cmd_p) != BT_STATUS_PENDING)
                    app_hfp_hfcommand_mempool_free(hf_cmd_p);
            }
        }
    }


}

uint8_t btapp_hfp_need_mute(void)
{
    return app_bt_device.hf_mute_flag;
}



#if HF_VERSION_1_6 == XA_ENABLED

uint8_t btapp_hfp_check_msbc_status(HfChannel *Chan)
{
#ifdef CHIP_BEST1000
    BtRemoteDevice* remDev = CMGR_GetRemoteDevice(&Chan->cmgrHandler);
    TRACE("feature=%x,accessory_feature=%x,version=%x",Chan->gwFeatures,Chan->bt_accessory_feature,remDev->remoteVersion[1]);
    if((Chan->gwFeatures & HF_GW_FEATURE_WIND_BAND_SPEECH) && (Chan->bt_accessory_feature & HF_CUSTOM_FEATURE_DOCK) && (remDev->remoteVersion[1] == 0x0f) && (remDev->remoteVersion[2] == 0))
    {
        return 1;
    }
    else
    {
        return 0;
    }
#else
    return 1;
#endif
}


#endif
