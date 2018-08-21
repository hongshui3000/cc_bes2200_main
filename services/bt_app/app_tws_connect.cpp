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


#define __TWS_FOLLOW_MOBILE__

#define TWS_CONNECT_DEBUG
#ifdef TWS_CONNECT_DEBUG
#define TWSCON_DBLOG TRACE        
#else
#define TWSCON_DBLOG(...)
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#define TWS_SET_STREAM_STATE(x) \
    {\
    tws.tws_source.stream_state  = x; \
    }

#define TWS_SET_M_STREAM_STATE(x) \
    {\
    tws.mobile_sink.stream_state  = x; \
    }
#endif
//#define __TWS_MASTER_STAY_MASTER__

static AvdtpCodec setconfig_codec;
extern AvdtpMediaHeader media_header;
extern U8 get_valid_bit(U8 elements, U8 mask);

extern  tws_dev_t  tws;
extern struct BT_DEVICE_T  app_bt_device;

MeCommandToken me_cmd_tkn;

uint8_t tws_inq_addr_used;

typedef struct {
    uint8_t used;
    BT_BD_ADDR bdaddr;
}TWS_INQ_ADDR_STRUCT;

TWS_INQ_ADDR_STRUCT tws_inq_addr[5];


static void app_tws_inquiry_timeout_handler(void const *param);

void btdrv_rf_set_conidx(uint32_t conidx);

osTimerDef (APP_TWS_INQ, app_tws_inquiry_timeout_handler);

static osTimerId app_tws_timer = NULL;

static uint8_t tws_find_process=0;
static uint8_t tws_inquiry_count=0;

//Modified by ATX : Leon.He_20170206: support customized TWS times value
#ifdef _PROJ_2000IZ_C001__
#define MAX_TWS_INQUIRY_TIMES   	10 //Approximate 1Mins
#endif

//Modified by ATX : Parker.Wei_20180324
#ifdef _PROJ_2000IZ_C002__
#define MAX_TWS_INQUIRY_TIMES   	21 //Approximate 2Mins
#endif


#ifndef MAX_TWS_INQUIRY_TIMES
#define MAX_TWS_INQUIRY_TIMES        10
#endif

//Modified by ATX : Leon.He_20171227: Macro for adjusting inquire timeout
#define PEER_INQUIRE_TIMEOUT	3

static BtHandler me_handler;

static bool tws_reconnect_process = 0;
static uint8_t tws_reconnect_count = 0;
static uint8_t tws_reconnect_mobile = 0;

#define MAX_TWS_RECONNECT_TIMES   (1)


BT_BD_ADDR test_addr = {0x01,0x02,0x03,0x04,0x05,0x06}; 

extern uint8_t app_poweroff_flag;
int hfp_volume_get(void);



#ifdef __SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
//Modified by ATX : Leon.He_20180131: tws slave reconnect timeout for auto enter tws pairing
static void tws_slave_reconnect_timer_callback(void const *param)
{
    TRACE("%s",__func__);
    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_TWS_PAIR, 0,0);
}
osTimerDef (APP_TWS_SLAVE_RECONNECT_TIMER, tws_slave_reconnect_timer_callback);
static osTimerId POSSIBLY_UNUSED app_tws_slave_reconnect_timer = NULL;
#define APP_TWS_RECONNECT_TIMEOUT	12000

static void app_start_tws_slave_reconnect_timer(void)
{
    TRACE("%s",__func__);
	if (app_tws_slave_reconnect_timer == NULL)
		app_tws_slave_reconnect_timer = osTimerCreate(osTimer(APP_TWS_SLAVE_RECONNECT_TIMER), osTimerOnce, NULL);
	osTimerStop(app_tws_slave_reconnect_timer);
	osTimerStart(app_tws_slave_reconnect_timer, APP_TWS_RECONNECT_TIMEOUT);
}

static void app_cancel_tws_slave_reconnect_timer(void)
{
    TRACE("%s",__func__);
	if (app_tws_slave_reconnect_timer != NULL)
		osTimerStop(app_tws_slave_reconnect_timer);
    osTimerDelete(app_tws_slave_reconnect_timer);
    app_tws_slave_reconnect_timer = NULL;
}
#endif
#define APP_TWS_SET_CURRENT_OP(op) do { tws.current_operation = (op); TRACE("Set TWS current op to %d", op); } while (0);
#define APP_TWS_SET_NEXT_OP(op) do { tws.next_operation = (op); TRACE("Set TWS next op to %d", op); } while (0);
#ifdef __TWS_RECONNECT_USE_BLE__
extern "C" {
    BtStatus ME_Ble_Clear_Whitelist(void);
    BtStatus ME_Ble_Set_Private_Address(BT_BD_ADDR *addr);
    BtStatus ME_Ble_Add_Dev_To_Whitelist(U8 addr_type,BT_BD_ADDR *addr);
    BtStatus ME_Ble_SetAdv_data(U8 len, U8 *data);
    BtStatus ME_Ble_SetAdv_parameters(adv_para_struct *para);
    BtStatus ME_Ble_SetAdv_en(U8 en);
    BtStatus ME_Ble_Setscan_parameter(scan_para_struct *para);
    BtStatus ME_Ble_Setscan_en(U8 scan_en,  U8 filter_duplicate);
}

app_tws_ble_reconnect_t app_tws_ble_reconnect;

void app_tws_set_slave_adv_data(void)
{
#if !defined(IAG_BLE_INCLUDE)
#ifdef __RECONNECT_BLE_ADV_INCLUDE_FW_VERSION_
    uint8_t adv_data[] = {
        0x04, 0x09, 'V', '0', '5',  // FW VERSION
        0x02, 0xff, 0x00  // manufacturer data
    };
#else
    uint8_t adv_data[] = {
        0x04, 0x09, 'T', 'W', 'S',  // Ble Name
        0x02, 0xff, 0x00  // manufacturer data
    };
#endif


    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        adv_data[7] = 1; 
    }

    ME_Ble_SetAdv_data(sizeof(adv_data), adv_data);
#else
    tws_reconnect_ble_set_adv_data(app_tws_ble_reconnect.isConnectedPhone);
#endif
}

void app_tws_set_slave_adv_para(void)
{
#if !defined(IAG_BLE_INCLUDE)
    uint8_t  peer_addr[BD_ADDR_SIZE] = {0};
    adv_para_struct para; 

    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        para.interval_min =             0x0320; // 1s
        para.interval_max =             0x0320; // 1s
    
    } else {
        para.interval_min =             0x0040; // 20ms
        para.interval_max =             0x0040; // 20ms
    }
    para.adv_type =                 0x03;
    para.own_addr_type =            0x01;
    para.peer_addr_type =           0x01;
    para.adv_chanmap =              0x07;
    para.adv_filter_policy =        0x00;
    memcpy(para.bd_addr.addr, peer_addr, BD_ADDR_SIZE);

    ME_Ble_SetAdv_parameters(&para);
#else
    tws_reconnect_ble_set_adv_para(app_tws_ble_reconnect.isConnectedPhone);
#endif
}

void app_tws_set_slave_adv_en(BOOL listen)
{
#if !defined(IAG_BLE_INCLUDE)
    TRACE("%s,adv en=%x",__func__,listen);

    ME_Ble_SetAdv_en(listen);
#else
    tws_ble_listen(listen);
#endif
}


BtStatus app_tws_set_slave_adv_disen(void)
{
    BtStatus  status;
#if !defined(IAG_BLE_INCLUDE)
    TRACE("%s",__func__);

    status = ME_Ble_SetAdv_disen();
    return status;
#else
    tws_ble_listen(0);
#endif
}



void app_tws_set_master_scan_para(void)
{
    scan_para_struct para; 

    para.scan_type =            0x00;
    if (app_tws_ble_reconnect.isConnectedPhone)
    {
        TRACE("%s ConnectedPhone", __func__);
        para.scan_interval =        0x0160; // 1 s
        para.scan_window =          0x008; // 10 ms
    } else {
        TRACE("%s Not ConnectedPhone", __func__);
        para.scan_interval =        0xa0; // 100 ms
        para.scan_window =          0x50; //50 ms
    }
    para.own_addr_type =        0x00;
    para.scan_filter_policy =   0x01;

    TRACE("%s", __func__);

    ME_Ble_Setscan_parameter(&para);
}


void app_tws_set_master_scan_en(BOOL enable)
{
    uint8_t le_scan_enable = enable ? 0x01: 0x00;
    uint8_t filter_duplicates = 0x01;
    TRACE("%s,scan en=%x",__func__,le_scan_enable);
    ME_Ble_Setscan_en(le_scan_enable, filter_duplicates);
}


void app_tws_master_add_whitelist(BT_BD_ADDR *addr)
{
    ME_Ble_Add_Dev_To_Whitelist(1,addr);
}

static void POSSIBLY_UNUSED app_tws_clear_whitelist(void)
{
    ME_Ble_Clear_Whitelist();
}

void app_tws_set_private_address(void)
{
#if !defined(IAG_BLE_INCLUDE)
    ME_Ble_Set_Private_Address((BT_BD_ADDR *)&bt_addr[0]);
#endif
}


static uint8_t ble_scan_status=0;
BtStatus app_tws_master_ble_scan_process(uint8_t enable)
{
    BtStatus status; 
    BtDeviceRecord record;	
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    status = SEC_FindDeviceRecord((BT_BD_ADDR *)&nvrecord_env->tws_mode.record.bdAddr, &record);

    if(status!=BT_STATUS_SUCCESS)
    {
        TRACE("tws info was removed,need repair !");
        memcpy(&record.bdAddr, &nvrecord_env->tws_mode.record.bdAddr, BD_ADDR_SIZE);
        status=BT_STATUS_SUCCESS;
    }
#ifdef _ALLOW_RECONNECT_DURING_CALL_
    if (enable == 1 && ble_scan_status ==0 )
#else
    if (enable == 1 && app_bt_device.hf_audio_state[0] == HF_AUDIO_DISCON && ble_scan_status ==0 )
#endif
    {
        if (status == BT_STATUS_SUCCESS){
            app_tws_master_add_whitelist(&record.bdAddr);
            app_tws_set_master_scan_para();
            ble_scan_status = 1;
            TRACE("%s open_ble_scan", __func__);
            app_tws_set_master_scan_en(true);
        }
    } else if (enable == 0) {
        TRACE("%s close_ble_scan", __func__);
        ble_scan_status = 0;
        app_tws_set_master_scan_en(false);
    } else {
        return  XA_STATUS_FAILED;
    }
    return status;
}


static uint8_t ble_adv_status=0;
BtStatus app_tws_slave_ble_adv_process(uint8_t enable)
{
    BtStatus status = BT_STATUS_SUCCESS; 

#ifdef _ALLOW_RECONNECT_DURING_CALL_
    if (enable == 1 && ble_adv_status ==0 )
#else
    if (enable == 1 && app_bt_device.hf_audio_state[0] == HF_AUDIO_DISCON && ble_adv_status ==0 )
#endif
    {
        app_tws_set_private_address();
        app_tws_set_slave_adv_para();
        app_tws_set_slave_adv_data();
        ble_adv_status = 1;
        TRACE("%s open_ble_adv",__func__);
        app_tws_set_slave_adv_en(true);
    } else if (enable == 0) {
        ble_adv_status = 0;
        TRACE("%s close_ble_adv",__func__);
        app_tws_set_slave_adv_en(false);
    } else {
        return  XA_STATUS_FAILED;
    }

    return status;
}


BtStatus app_tws_slave_ble_close_adv_process(uint8_t enable)
{
    BtStatus status = BT_STATUS_SUCCESS; 

    ble_adv_status = 0;
    status = app_tws_set_slave_adv_disen();

    return status;
}



void app_tws_reconnect_phone_start(void* arg);
void master_ble_scan_timer_callback(void const *n) {
    TRACE("%s master_ble_scan_timer fired", __func__);
    if (app_tws_ble_reconnect.scan_func)
        app_tws_ble_reconnect.scan_func(APP_TWS_CLOSE_BLE_SCAN);
    
    app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)app_tws_reconnect_phone_start);
}
osTimerDef(master_ble_scan_timer, master_ble_scan_timer_callback);

void app_tws_slave_delay_to_roleSwitch(void);
void slave_delay_role_switch_timer_callback(void const *n) {
    TRACE("%s slave_role_switch_timer_callback", __func__);
    app_tws_slave_delay_to_roleSwitch();
}

osTimerDef(slave_delay_role_switch_timer, slave_delay_role_switch_timer_callback);

void tws_ble_init(void);
void app_tws_ble_reconnect_init(void)
{
    app_tws_ble_reconnect.adv_func = app_tws_slave_ble_adv_process;
    app_tws_ble_reconnect.scan_func = app_tws_master_ble_scan_process;
    app_tws_ble_reconnect.adv_close_func = app_tws_slave_ble_close_adv_process;
    app_tws_ble_reconnect.bleReconnectPending = 0;
    app_tws_ble_reconnect.tws_start_stream_pending = 0;
#ifdef _TWS_MASTER_DROP_DATA_
    app_tws_ble_reconnect.master_filter = 0;
#endif

    app_tws_ble_reconnect.master_ble_scan_timerId = osTimerCreate(osTimer(master_ble_scan_timer), osTimerOnce, (void *)0); 
    app_tws_ble_reconnect.slave_delay_role_switch_timerId = osTimerCreate(osTimer(slave_delay_role_switch_timer), osTimerOnce, (void *)0); 
    app_tws_ble_reconnect.isConnectedPhone = 0;

    tws_ble_init();
}
#endif



AvdtpCodec app_tws_get_set_config_codec_info(void)
{
    return setconfig_codec;
}

void set_a2dp_reconfig_sample_rate(A2dpStream *Stream)
{
    tws_dev_t *twsd = &tws;
    enum AUD_SAMPRATE_T sample_rate;

    sample_rate = bt_parse_store_sbc_sample_rate(Stream->stream.codecCfg.elements[0]);

    twsd->audout.sample_rate = (uint32_t)sample_rate;

//    if((Stream->stream.codecCfg.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK) == 0x10){
//        a2dp_sample_rate = AUD_SAMPRATE_48000;
//        twsd->audout.sample_rate = 48000;
//    }else if((Stream->stream.codecCfg.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK) == 0x20){
//        a2dp_sample_rate = AUD_SAMPRATE_44100;
//        twsd->audout.sample_rate = 44100;
//    }
    TWSCON_DBLOG("[%s] sample_rate = %d", __func__, sample_rate);
}

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC) ||defined(A2DP_AAC_ON) ) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
void app_tws_split_channel_select(uint8_t *locaddr,uint8_t *peeraddr)
{
        uint32_t local_uap = (locaddr[2]<<16) | (locaddr[1]<<8) | locaddr[0];
        uint32_t peer_uap = (peeraddr[2]<<16) | (peeraddr[1]<<8) | peeraddr[0];
#ifdef __TWS_CHANNEL_LEFT__
        tws.decode_channel= SBC_DEC_LEFT;
        tws.tran_channel = SBC_SPLIT_RIGHT;

#elif defined(__TWS_CHANNEL_RIGHT__)
        tws.decode_channel = SBC_DEC_RIGHT;
        tws.tran_channel = SBC_SPLIT_LEFT;        

#else
        TWSCON_DBLOG("LUAP=%x,PUAP=%x",local_uap,peer_uap);
        if(local_uap >= peer_uap)
        {
            if(local_uap == peer_uap)
            {
                if(tws.tws_mode==TWSMASTER)
                {
                    tws.decode_channel= SBC_DEC_LEFT;
                    tws.tran_channel = SBC_SPLIT_RIGHT;                    
                }
                else
                {
                    tws.decode_channel= SBC_DEC_RIGHT;
                    tws.tran_channel = SBC_SPLIT_LEFT;                 
                }
            }
            else
            {
                tws.decode_channel= SBC_DEC_LEFT;
                tws.tran_channel = SBC_SPLIT_RIGHT;
            }
        }
        else
        {
            tws.decode_channel = SBC_DEC_RIGHT;
            tws.tran_channel = SBC_SPLIT_LEFT;        
        }
#endif  
}

#endif


void app_tws_channel_report_timeout_handler(void const *param)
{
    app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);    
}

osTimerDef (APP_TWS_CHANNEL_REPORT, app_tws_channel_report_timeout_handler);
static osTimerId POSSIBLY_UNUSED app_tws_channel_report_timer = NULL;

void app_tws_Indicate_connection(void)
{
#ifdef MEDIA_PLAYER_SUPPORT
    app_voice_report(APP_STATUS_INDICATION_TWS_CONNECTED,0);//Modified by ATX : Leon.He_20171123
#endif
    //      app_voice_report(APP_STATUS_INDICATION_TWS_ISSLAVE,0);
    app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
#ifdef __ENABLE_LEFT_RIGHT_PROMPT__//Modified by ATX : Leon.He_20171123
#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_ON)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
#ifdef __TWS_CHANNEL_LEFT__
    app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
#elif defined(__TWS_CHANNEL_RIGHT__)
    if (app_tws_channel_report_timer == NULL)
        app_tws_channel_report_timer = osTimerCreate(osTimer(APP_TWS_CHANNEL_REPORT), osTimerOnce, NULL);  
    osTimerStart(app_tws_channel_report_timer, 2000);
//    app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
#else
    tws_dev_t *twsd = &tws;
    if(twsd->decode_channel == SBC_DEC_RIGHT)
    {
     //   app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
        if (app_tws_channel_report_timer == NULL)
            app_tws_channel_report_timer = osTimerCreate(osTimer(APP_TWS_CHANNEL_REPORT), osTimerOnce, NULL);  
        osTimerStart(app_tws_channel_report_timer, 2000);     
    }
    else
    {
        app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
    }
#endif
#endif
#endif//__ENABLE_LEFT_RIGHT_PROMPT__
}



void app_tws_store_info_to_nv(DeviceMode mode,uint8_t *addr)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
    //Modified by ATX : cc_20180402: avoid store info every time
    if(!memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,addr,6))
    {
        TRACE("%s==========>return\r\n",__func__);
        return;
    }
    nvrecord_env->tws_mode.mode = mode;
    memcpy(nvrecord_env->tws_mode.record.bdAddr.addr, addr, 6);
    nv_record_env_set(nvrecord_env);    
}

static void tws_retrigger_hfp_voice_callback(uint32_t status, uint32_t param)
{
  TRACE("tws_retrigger_hfp_voice_callback audio state=%d",app_bt_device.hf_audio_state[0]);
  if(app_bt_device.hf_audio_state[0] != HF_AUDIO_CON)
  {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);  
  }
}


static void tws_stop_hfp_voice_callback(uint32_t status, uint32_t param)
{
  TRACE("tws_retrigger_hfp_voice_callback audio state=%d",app_bt_device.hf_audio_state[0]);
  if(app_bt_device.hf_audio_state[0] == HF_AUDIO_CON)
  {
//      osDelay(30);
      app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,(uint32_t)tws_retrigger_hfp_voice_callback);
  }
}
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
extern void rb_play_reconfig_stream(uint32_t arg );
#endif
extern void a2dp_set_config_codec(AvdtpCodec *config_codec,const A2dpCallbackParms *Info);
void a2dp_callback_tws_source(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    tws_dev_t *twsd = &tws;
    bool tran_blocked;
#ifdef __TWS_RECONNECT_USE_BLE__
    //BtStatus status;
#endif
    //    TWS_DBLOG("enter %s  e:%d\n",__FUNCTION__, Info->event);
  //  TWSCON_DBLOG("[%s] event=%d ", __func__,Info->event);
    
    switch(Info->event)
    {
        case A2DP_EVENT_STREAM_OPEN:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_OPEN,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);            
            twsd->tws_source.connected = true;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
            TWS_SET_STREAM_STATE(TWS_STREAM_OPEN)
#endif
            twsd->tws_mode = TWSMASTER;
            app_tws_set_tws_conhdl(Stream->device->cmgrHandler.remDev->hciHandle);
#ifdef __TWS_OUTPUT_POWER_FIX_SEPARATE__            
            *(volatile uint32_t *)(0xd02101a4+btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle)*96) =6;
#endif
            btdrv_rf_bit_offset_track_enable(true);
#if 0
            btdrv_rf_set_conidx(Stream->device->cmgrHandler.remDev->hciHandle-0x80);
#endif
            if(twsd->mobile_sink.connected == false) {
                btdrv_rf_trig_patch_enable(0);
            }
#ifdef __TWS_FOLLOW_MOBILE__            
    *(uint32_t *)0xc00062d0 = btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle);  //tws idx
#endif
            memcpy(tws.peer_addr.addr,Stream->device->cmgrHandler.remDev->bdAddr.addr,6);       
            app_tws_store_info_to_nv(TWSMASTER,Stream->device->cmgrHandler.remDev->bdAddr.addr);
            btdrv_set_tws_role_triggler(TWSMASTER);
            app_audio_switch_flash_flush_req();
            APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
            APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
#ifdef __TWS_CALL_DUAL_CHANNEL__
            if(twsd->mobile_sink.connected == true)
            {
                TRACE("SET FORWARD ROLE");
                DUMP8("%02x ",tws.mobile_addr.addr,6);                  
                DUMP8("%02x ",tws.peer_addr.addr,6);             
#if 0                
                *(uint32_t *)(0xc0004c54) = 0x06400640;
                *(uint32_t *)(0xc0004c58) = 0;
                *(unsigned int *)0xc0004c1c = 0;
                *(unsigned int *)0xc0004c20 |= 1;
                *(unsigned int *)0xc0004c10 = 0xffffffff;
                *(unsigned int *)0xc0004c14 = 0xffffffff;
                *(unsigned int *)0xc0004c18 = 0x00007fff;     
#endif
                if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_2)
                {
                    A2dpStream *stream =  NULL;
                    uint8_t role;
                    stream = twsd->mobile_sink.stream;                
                    struct ld_afh_bak *afh_bak = (struct ld_afh_bak *)0xc0003fb8;
                    struct  ld_afh_bak *afh_bak_id = &afh_bak[btdrv_conhdl_to_linkid(tws.mobile_conhdl)];
                    struct ld_sniffer_env *ld_sniffer = (struct ld_sniffer_env *)0xc0004bf0;
                    TRACE("AFH0:");
                    DUMP8("%02x ",(uint8_t *)0xc0004067,10);
                    TRACE("\n");
                    TRACE("AFH1:");
                    DUMP8("%02x ",(uint8_t *)0xc0004071,10);      
                    TRACE("\n");                        
                    if(stream->device && stream->device->cmgrHandler.remDev)
                    {
                        if(stream->device->cmgrHandler.remDev->role == 1) //SLAVE ROLE
                        {

                            #if 0
                            ld_sniffer->peer_dev.afh_instant = afh_bak_id->instant;
                            ld_sniffer->peer_dev.afh_mode = afh_bak_id->mode;
                            if( ld_sniffer->peer_dev.afh_mode == 0)
                            {
                                uint8_t default_map[]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
                                memcpy(ld_sniffer->peer_dev.chan_map,default_map,10);
                            }
                            else
                            {
                                memcpy( ld_sniffer->peer_dev.chan_map,afh_bak_id->map.map,10);
                            }
                            #else
                            ld_sniffer->peer_dev.afh_mode = 1;
                            memcpy( ld_sniffer->peer_dev.chan_map,(uint8_t *)0xc0004071,10);
                            #endif
           //                 ld_sniffer->peer_dev.chan_map = afh_bak_id->map;
                             TRACE("SET SLAVE AFH");

                        }
                        else 
                        {
                             ld_sniffer->peer_dev.afh_instant = 0;
                             ld_sniffer->peer_dev.afh_mode = 1;
                             memcpy( ld_sniffer->peer_dev.chan_map,(uint8_t *)0xc0004067,10);
                             TRACE("SET MASTER AFH");
                             
                        }
                        TRACE("CHANMAP=:");
                        DUMP8("%02x",ld_sniffer->peer_dev.chan_map,10);
                        TRACE("\n instant=%x,mode=%x",ld_sniffer->peer_dev.afh_instant,ld_sniffer->peer_dev.afh_mode);           
                        
                    }
                    else
                    {
                        TRACE("ROLE ERROR ,PLEASE CHECK THE MOBILE LINK STATUS!");
                    }
                }
                Me_SetSnifferEnv(1,FORWARD_ROLE,tws.mobile_addr.addr,tws.peer_addr.addr);
                tws_player_set_hfp_vol(hfp_volume_get());            

                
            }
#endif

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
            app_tws_split_channel_select(bt_addr,tws.peer_addr.addr);
#endif            
            twsd->notify(twsd); //notify link task        
            twsd->unlock_mode(&tws);
            if(twsd->stream_idle == 1)
            {
                twsd->stream_idle = 0;
            }
            else
            {
                app_tws_Indicate_connection();
//#ifdef A2DP_AAC_DIRECT_TRANSFER            
                tws_create_spp_client(Stream);   
//#endif                
         //       AVRCP_Connect(twsd->rcp, &Stream->device->cmgrHandler.remDev->bdAddr);///TG

            }
#ifndef FPGA
            app_stop_10_second_timer(APP_PAIR_TIMER_ID);
#endif
            if (!app_tws_reconnect_slave_process(A2DP_EVENT_STREAM_OPEN,0)){
                if(twsd->mobile_sink.connected == false  && tws_reconnect_mobile == 0)
                {
                    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR,0,0);
#ifdef __TWS_RECONNECT_USE_BLE__
                    TRACE("%s start_pare_timer", __func__);
#if 0//Modified by ATX : Parker.Wei£¬ handle pairing time in app_bt_handle_process	
#ifndef FPGA
                    app_start_10_second_timer(APP_PAIR_TIMER_ID);
#endif
#endif//#if 0
#endif
                }
            }


            ///if phone has been connected
            if(app_bt_device.a2dp_state[0]){
                TWSCON_DBLOG("a2dp_callback_tws_source  tws samplerate:%x,mobile samplerate:%x\n",Info->p.codec->elements[0],app_bt_device.a2dp_codec_elements[0][0]);
                if((app_bt_device.a2dp_codec_elements[0][0] & 0xf0) != (Info->p.codec->elements[0]&0xf0)){
#ifdef A2DP_AAC_ON
                    TWSCON_DBLOG("%s A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream code = %d\n",__FUNCTION__, twsd->mobile_codectype);
#else
                    TWSCON_DBLOG("%s A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream\n",__FUNCTION__);
#endif
#ifdef A2DP_AAC_DIRECT_TRANSFER
                    if(twsd->mobile_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                        A2dpStream *stream =  NULL;
                        stream = twsd->tws_sink.stream;
                        if(twsd->audout.sample_rate == AUD_SAMPRATE_44100){
                            stream->stream.codecCfg.elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                            stream->stream.codecCfg.elements[0] |= A2D_SBC_IE_SAMP_FREQ_44;
                        }else if(twsd->audout.sample_rate == AUD_SAMPRATE_48000){
                            stream->stream.codecCfg.elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                            stream->stream.codecCfg.elements[0] |= A2D_SBC_IE_SAMP_FREQ_48;
                        }
                        A2DP_ReconfigStream(twsd->tws_source.stream, &stream->stream.codecCfg, NULL);
                    }else
#endif
                    {
                        A2DP_ReconfigStream(twsd->tws_source.stream, &twsd->tws_sink.stream->stream.codecCfg,NULL);
                    }
                }
#ifdef __TWS_RECONNECT_USE_BLE__
                else {
                    if (app_bt_device.a2dp_streamming[0] == 1) {
                        uint8_t status;
                        //tws_start_stream_pending = 1; 
                        app_tws_ble_reconnect.tws_start_stream_pending = 1;
#ifdef _TWS_MASTER_DROP_DATA_
                        app_tws_ble_reconnect.master_filter=1;
#endif
                        TRACE("1,start tws stream!");
                        app_tws_check_max_slot_setting();
                        status = A2DP_StartStream(tws.tws_source.stream);
                        if(status == BT_STATUS_PENDING)
                        {
                            APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                        }
                        else
                        {
                            TRACE("%s,start stream error %x",__FUNCTION__,status);
                        }
                    }
                }
#endif
            }
    if(twsd->mobile_sink.connected == true && app_bt_device.hf_audio_state[0] == HF_AUDIO_CON && app_audio_manager_get_current_media_type() ==BT_STREAM_VOICE )
    {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_hfp_voice_callback);

    }
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
            tws_local_player_set_tran_2_slave_flag(1);
#endif
            break;
        case A2DP_EVENT_STREAM_SBC_PACKET_SENT:
            //TWS_DBLOG("ssbc sent curr t =%d",bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
            twsd->tws_source.outpackets += 1;
            tws.lock_prop(&tws);
            tran_blocked = tws.tran_blocked ;
            if(tws.tran_blocked == true)
            {
                tws.tran_blocked = false;
            }
            tws.unlock_prop(&tws);
            if (!tran_blocked){
                // master has send first packet to salve
                if(twsd->tws_source.outpackets==0)
                {
                    TWSCON_DBLOG("Master send first packet...OK!!!");
                    BTCLK_STATUS_PRINT();
                }       
                
            }
            twsd->tws_source.notify_send(&(twsd->tws_source));
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_CLOSED,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            
            twsd->stream_idle = 0;
            tws.media_suspend = true;
            APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
            APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
#ifdef __TWS_FOLLOW_MOBILE__
            *(uint32_t *)0xc00062d0 = 0xff;
#endif
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)   
            TWS_SET_STREAM_STATE(TWS_STREAM_CLOSE)
#endif
            
#ifdef __TWS_RECONNECT_USE_BLE__
            if (app_tws_ble_reconnect.bleReconnectPending) {
                app_tws_reset_tws_data();                
                app_tws_ble_reconnect.bleReconnectPending = 0;
                //bleRconFlag = 0; 
            } 
#endif
            if( (Info->p.capability) && Info->len ){
                TWSCON_DBLOG("%s  e:%d t:%d A2DP_EVENT_STREAM_CLOSED\n",__FUNCTION__, Info->event, Info->p.capability->type);
                //Info.p.capability->p.
            }

   //              if(tws.start_media_waiting == true)
    //             {
    //                A2DP_StartStreamRsp(tws.mobile_sink.stream,A2DP_ERR_NO_ERROR);           
    //               tws.start_media_waiting = false;


 //                }
            
        //  if(Info->discReason == BEC_USER_TERMINATED){
              // app_shutdown();
//          }else 
            if (!app_tws_reconnect_slave_process(A2DP_EVENT_STREAM_CLOSED,0)){
                twsd->tws_source.connected = false;
                twsd->tran_blocked =false;
                if(twsd->mobile_sink.connected == true){
                    tws.tws_mode = TWSON;
                    restore_tws_channels();
                }else{
                    tws.tws_mode = TWSINVD;
                    restore_tws_channels();   
                    restore_mobile_channels();
                }
            }
#ifdef A2DP_AAC_DIRECT_TRANSFER            
            twsd->tws_set_codectype_cmp = 0;
#endif
            break;
        case A2DP_EVENT_AVDTP_CONNECT:
            break;
        case A2DP_EVENT_AVDTP_DISCONNECT:
#if 0            
            twsd->tws_source.connected = false;
            twsd->tran_blocked =false;
            if(twsd->mobile_sink.connected ==  true)
            {
                
                tws.tws_mode = TWSON;
                restore_tws_channels();
            }
            else
            {
                tws.tws_mode = TWSINVD;
                restore_tws_channels();   
                restore_mobile_channels();
            }            
#endif      
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
            rb_sink_stream_stared();
            if(tws.local_player_wait_reconfig){
                osSemaphoreRelease(tws.wait_stream_reconfig._osSemaphoreId);
                tws.local_player_wait_reconfig = 0;
            }
#endif      
            break;
        case A2DP_EVENT_CODEC_INFO:
            TWSCON_DBLOG("%s %d A2DP_EVENT_CODEC_INFO,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);            
            a2dp_set_config_codec(&setconfig_codec,Info);
            break;
        case A2DP_EVENT_GET_CONFIG_IND:
            TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);            
            setconfig_codec.elements[0] &=0xcf;
            if (twsd->audout.sample_rate == 48000){
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_48000\n",__FUNCTION__,__LINE__);
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_48;  
            }else if (twsd->audout.sample_rate == 44100){
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_44100\n",__FUNCTION__,__LINE__);
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_44;  
            }else if (twsd->audout.sample_rate == 32000){
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_32;
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_32000\n",__FUNCTION__,__LINE__);
            }else if (twsd->audout.sample_rate == 16000){
                setconfig_codec.elements[0] |=A2D_SBC_IE_SAMP_FREQ_16;
                TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_16000\n",__FUNCTION__,__LINE__);
            }
            A2DP_SetStreamConfig(Stream, &setconfig_codec, NULL);            
            break;
        case A2DP_EVENT_STREAM_START_IND:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_IDLE,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            tws.media_suspend = false;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            TWS_SET_STREAM_STATE(TWS_STREAM_START)
#endif
            break;
        case A2DP_EVENT_STREAM_STARTED:
            TWSCON_DBLOG("%s  e:%d  eror=%d,status=%d \n",__FUNCTION__, Info->event,Info->error,Info->status);
#if defined(SLAVE_USE_ENC_SINGLE_SBC)
#if defined(ADJUST_BITPOOL_BETWEEN_TWS)
            app_tws_reset_cadun_count();
#endif
#endif
            if(Info->error == 0)
            {
#ifdef __TWS_RECONNECT_USE_BLE__
                 if(app_tws_ble_reconnect.tws_start_stream_pending == 1)
                 {
                     app_tws_ble_reconnect.tws_start_stream_pending = 0;
                     app_tws_reset_tws_data();                
                 }
                 app_tws_ble_reconnect.bleReconnectPending = 0;
                 
#endif
#ifdef A2DP_AAC_DIRECT_TRANSFER
                 tws_player_set_codec(tws.mobile_codectype);                 
#endif
                 tws.media_suspend = false;
        //         tws.start_media_waiting = false;
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
                 TWS_SET_STREAM_STATE(TWS_STREAM_START)
                 rb_sink_stream_stared();
#endif                 
    //             A2DP_StartStreamRsp(tws.mobile_sink.stream,A2DP_ERR_NO_ERROR);           
            }
            else if(Info->status == XA_STATUS_TIMEOUT && Info->error == AVDTP_ERR_UNKNOWN_ERROR)
            {
                //uint8_t status;
              //  tws.media_suspend = true;
           //     tws.start_media_waiting = false;
               // A2DP_StartStream(tws.tws_source.stream);
 //               A2DP_StartStreamRsp(tws.mobile_sink.stream,A2DP_ERR_NO_ERROR);    
              //  tws.stream_reset =1;
              //  status = A2DP_IdleStream(tws.tws_source.stream);
                tws.tws_source.stream->stream.state = AVDTP_STRM_STATE_STREAMING;
                tws.media_suspend = false;              
#ifdef A2DP_AAC_DIRECT_TRANSFER
                 tws_player_set_codec(tws.mobile_codectype);                 
#endif				
                TRACE("A2DP IDLE STREAM stream=%x",tws.tws_source.stream);
               
            }
            else if(Info->error == AVDTP_ERR_BAD_STATE)
            {
                tws.tws_source.stream->stream.state = AVDTP_STRM_STATE_STREAMING;
                tws.media_suspend = false;
#ifdef A2DP_AAC_DIRECT_TRANSFER
                 tws_player_set_codec(tws.mobile_codectype);                 
#endif				
            }
            else
            {
                TWSCON_DBLOG("%s,expect error occured here~",__FUNCTION__);
                
            }

            TRACE("%s,A2DP_EVENT_STREAM_STARTED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
            if(tws.current_operation == APP_TWS_START_STREAM)
            {
                if(tws.next_operation == APP_TWS_START_STREAM)
                {
                    TRACE("next operation is start stream");
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);
                }
                else if(tws.next_operation == APP_TWS_SUSPEND_STREAM)
                {
                    uint8_t status;
                    TRACE("next is suspend stream");
                    status = A2DP_SuspendStream(tws.tws_source.stream);
                    if(status == BT_STATUS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                        APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);                    
                    }
                    else
                    {
                        TRACE("%s,suspend stream error %x",__FUNCTION__,status);
                    }
                }
                else
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    TRACE("ELSE curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
                }
            }
            else
            {
                TRACE("^^^^^ERROR!!!START OP MISMATCH!!!");
            }
            
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            TWS_SET_STREAM_STATE(TWS_STREAM_SUSPEND) 
#endif
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
            if(Info->status == BT_STATUS_TIMEOUT){
                tws.tws_source.stream->stream.state = AVDTP_STRM_STATE_OPEN;
            }

 //           tws.suspend_media_pending = false;
            if(tws.current_operation == APP_TWS_SUSPEND_STREAM)
            {
                if(tws.next_operation == APP_TWS_START_STREAM)
                {
                    uint8_t status;
                    TRACE("next operation is start stream");
                    app_tws_check_max_slot_setting();
                    status = A2DP_StartStream(tws.tws_source.stream);
                    if(status == BT_STATUS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                        APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);                    
                    }
                    else
                    {
                        TRACE("%s,start stream error %x",__FUNCTION__,status);
                    }
                    
                }
                else if(tws.next_operation == APP_TWS_SUSPEND_STREAM)
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);                        

                }
                else
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_IDLE_OP);
                    TRACE("ELSE curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
                }
            }
            else
            {
                TRACE("^^^^^ERROR!!!SUSUPEND OP MISMATCH!!!");
            }
 #if 0
            if(tws.start_media_pending == true)
            {
                tws.start_media_pending = false;
                if(tws.tws_source.stream->stream.state == AVDTP_STRM_STATE_OPEN)
                {
                    BtStatus state;
                    state = A2DP_StartStream(tws.tws_source.stream);
                    TWSCON_DBLOG("send start to slave state = %x",state);
                }            
                else
                {
                    TRACE("START SLAVE STREAM ERROR!!! CHECK THE STATE MECHINE");
                }
            }
#endif            
            
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)  
            tws.media_suspend = true;
            if(tws.tws_source.stream_need_reconfig){
                rb_play_reconfig_stream(tws.tws_source.stream_need_reconfig);
                tws.tws_source.stream_need_reconfig = 0;
            }
#endif

            break;
        case A2DP_EVENT_STREAM_RECONFIG_IND:
            A2DP_ReconfigStreamRsp(Stream,A2DP_ERR_NO_ERROR,0);
            break;
        case A2DP_EVENT_STREAM_RECONFIG_CNF:

            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_IDLE,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);

#ifdef __TWS_RECONNECT_USE_BLE__
            if (app_bt_device.a2dp_streamming[0] == 1) {
                uint8_t status;
                app_tws_ble_reconnect.tws_start_stream_pending = 1;
#ifdef _TWS_MASTER_DROP_DATA_
                app_tws_ble_reconnect.master_filter=1;
#endif
                app_tws_check_max_slot_setting();
                status = A2DP_StartStream(tws.tws_source.stream);
                if(status == BT_STATUS_PENDING)
                {
                    APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                }
                else
                {
                    TRACE("%s,start stream error2 %x",__FUNCTION__,status);
                }
            }
#endif
#if defined(A2DP_AAC_ON)
            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                
            }else
#endif
            {
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
                if(tws.local_player_wait_reconfig){
                    osSemaphoreRelease(tws.wait_stream_reconfig._osSemaphoreId);
                    tws.local_player_wait_reconfig = 0;
                }
#endif                
                set_a2dp_reconfig_sample_rate(Stream);
            }
            break;
        case A2DP_EVENT_STREAM_IDLE:
            TWSCON_DBLOG("%s %d A2DP_EVENT_STREAM_IDLE,error=%d,status=%d,discReason:0x%02x,stream=%x\n",
                                               __FUNCTION__,__LINE__,Info->error,Info->status,Info->discReason,Stream);
            twsd->stream_idle = 1;
            if(twsd->stream_reset == 1 && Stream->device && Stream->device->cmgrHandler.remDev)
            {
                uint32_t status;
                setconfig_codec.elements[0] &=0xcf;
                if (twsd->audout.sample_rate == 48000){
                    TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_48000\n",__FUNCTION__,__LINE__);
                    setconfig_codec.elements[0] |=0x10;  
                }else{
                    TWSCON_DBLOG("%s %d A2DP_EVENT_GET_CONFIG_IND AUD_SAMPRATE_44100\n",__FUNCTION__,__LINE__);
                    setconfig_codec.elements[0] |=0x20;  
                }
                status = A2DP_SetStreamConfig(Stream, &setconfig_codec, NULL);
                TRACE("A2DP_EVENT_STREAM_IDLE REOPEN STREAM STATUS=%x",status);
                twsd->stream_reset = 0;
            }            
            break;
    }
}


bool is_tws_peer_device(void *tws, A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    bool is_tws_peer = FALSE;
    tws_dev_t *twsd = (tws_dev_t *)tws;

    if(twsd->tws_mode == TWSSLAVE){
        return true;
    }
//    TWS_DBLOG("is_tws_peer_device");
    if(Info->event == A2DP_EVENT_AVDTP_CONNECT)
    {
//        DUMP8("%02x ", twsd->peer_addr.addr,6);
//        DUMP8("%02x ", Info->p.device->cmgrHandler.remDev->bdAddr.addr,6);    
        if (!memcmp((const char *)&(twsd->peer_addr), (const char *)&(Info->p.device->cmgrHandler.remDev->bdAddr.addr),6)){
            is_tws_peer = true;
        }        
    }
    else
    {
        //get remote device addr 
        BtRemoteDevice * addr = A2DP_GetRemoteDevice(Stream);
//        DUMP8("%02x ", twsd->peer_addr.addr,6);
//        DUMP8("%02x ", addr->bdAddr.addr,6);
        if (!memcmp((const char *)&(twsd->peer_addr), (const char *)&(addr->bdAddr),6)){
            is_tws_peer = true;
        }
    }
    return is_tws_peer;
}



bool is_tws_active(void)
{
    bool res = false;
    if (tws.tws_mode == TWSMASTER || tws.tws_mode == TWSSLAVE){
        if (tws.pcm_playing){
            res = true;
        }
    }
    return res;
}

static bool is_tws_sink(void *tws, A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    bool res = false;
    tws_dev_t *twsd = (tws_dev_t *)tws;
 //   TWS_DBLOG(" is_tws_sink Stream = %x,sink stream=%x",Stream,twsd->tws_sink.stream);
    bool is_tws_peer = is_tws_peer_device(tws,Stream,Info);
    if (((uint32_t)Stream == (uint32_t)(twsd->tws_sink.stream)) && is_tws_peer){
        res = true;
    }
    return res;
}

#if 0
static bool is_link_request_from_tws(void *tws, A2dpStream *stream, A2dpCallbackParms *Info)
{
    bool res = false;
    tws_dev_t *twsd = (tws_dev_t *)tws;
    if(Info->event == A2DP_EVENT_STREAM_OPEN){
        struct nvrecord_env_t *nvrecord_env;
        twsd->tws_mode = TWSSLAVE;
        app_tws_set_tws_conhdl(stream->device->cmgrHandler.remDev->hciHandle);
        nv_record_env_get(&nvrecord_env);
        nvrecord_env->tws_mode.mode = TWSSLAVE;
        memcpy(nvrecord_env->tws_mode.record.bdAddr.addr, stream->device->cmgrHandler.remDev->bdAddr.addr, 6);
        nv_record_env_set(nvrecord_env);
#ifdef SLAVE_USE_ENC_SINGLE_SBC        
        //app_bt_stream_volumeset(11);
#endif
        twsd->unlock_mode(twsd);
        res = true;
        app_voice_report(APP_STATUS_INDICATION_CONNECTED,0);
        //      app_voice_report(APP_STATUS_INDICATION_TWS_ISSLAVE,0);
        app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
#ifdef __TWS_CHANNEL_LEFT__
            app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
#elif defined(__TWS_CHANNEL_RIGHT__)
            app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
#else
            if(twsd->decode_channel == SBC_DEC_RIGHT)
                app_voice_report(APP_STATUS_INDICATION_TWS_RIGHTCHNL,0);
            else
                app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
#endif
#endif

//      app_voice_report(APP_STATUS_INDICATION_TWS_LEFTCHNL,0);
    }
    if(Info->event == A2DP_EVENT_STREAM_CLOSED){
        twsd->tws_mode = TWSINVD;
        tws_player_stop();
        restore_tws_channels();   
        res = true;
    }
    return res;
}
#endif

static bool is_no_mobile_connect(void *tws)
{
    bool res = false;
    tws_dev_t *twsd = (tws_dev_t *)tws;
    res = twsd->mobile_sink.connected;
    return res;
}


void app_bt_hcireceived_data_clear(uint16_t conhdl);


#ifdef __TWS_CALL_DUAL_CHANNEL__
extern uint8_t slave_sco_active;
#endif

int app_tws_slave_a2dp_callback(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    tws_dev_t *twsd = &tws;
    int res=0;
    if(is_no_mobile_connect(twsd) == true)
    {
        TWSCON_DBLOG("%s,TWS ERROR!slave can't connect to the mobile");
    }
 //   TWSCON_DBLOG("[%s] event=%d ", __func__,Info->event);
    switch(Info->event)
    {
        case A2DP_EVENT_STREAM_OPEN:
            TRACE("%s,A2DP_EVENT_STREAM_OPEN,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            
            twsd->tws_mode = TWSSLAVE;
            app_tws_set_tws_conhdl(Stream->device->cmgrHandler.remDev->hciHandle);
            btdrv_rf_bit_offset_track_enable(true);
            btdrv_rf_set_conidx(btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle));
#ifdef __TWS_OUTPUT_POWER_FIX_SEPARATE__            
            *(volatile uint32_t *)(0xd02101a4+btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle)*96) =7;
#endif            
            btdrv_rf_trig_patch_enable(0);
            btdrv_set_tws_role_triggler(TWSSLAVE);

#ifdef __TWS_CALL_DUAL_CHANNEL__
#endif
            app_tws_store_info_to_nv(TWSSLAVE,Stream->device->cmgrHandler.remDev->bdAddr.addr);

#if (defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined( SLAVE_USE_ENC_SINGLE_SBC)) && defined(__TWS_OUTPUT_CHANNEL_SELECT__)
            app_tws_split_channel_select(bt_addr,tws.peer_addr.addr);
#endif                      
            twsd->unlock_mode(twsd);
            app_audio_switch_flash_flush_req();

#ifndef FPGA
            app_stop_10_second_timer(APP_PAIR_TIMER_ID);//Modified by ATX : Leon.He_20180202: move it ahead, since it might be conflict with slave auto pairing
#endif
            if(twsd->stream_idle ==1)
            {
                twsd->stream_idle = 0;
            }
            else
            {
#ifdef __SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
//Modified by ATX : Leon.He_20180131: tws slave reconnect timeout for auto enter tws pairing
            	app_cancel_tws_slave_reconnect_timer();
#endif
                app_tws_Indicate_connection();
            }

            set_a2dp_reconfig_sample_rate(Stream);
   //         twsd->suspend_media_pending = false;
  //          twsd->start_media_pending = false;
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            TRACE("%s,A2DP_EVENT_STREAM_CLOSED,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            TRACE("conhdl=%x\n",tws.tws_conhdl);
            app_bt_hcireceived_data_clear(tws.tws_conhdl);
            HciBypassProcessReceivedDataExt(NULL);
            twsd->tws_mode = TWSINVD;
            twsd->stream_idle = 0;
            tws_player_stop(BT_STREAM_SBC);
            restore_tws_channels(); 
   //         twsd->suspend_media_pending = false;
  //          twsd->start_media_pending = false;            
            if(Info->discReason == BEC_USER_TERMINATED)
            {
             //   app_shutdown();
            }            
#ifdef __TWS_CALL_DUAL_CHANNEL__
            slave_sco_active = 0;    
            bt_drv_reg_op_ld_sniffer_env_monitored_dev_state_set(0);
            if(bt_media_is_media_active_by_type(BT_STREAM_VOICE))
            {
                app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);
            }
#endif            
            break;
        case A2DP_EVENT_AVDTP_CONNECT:
//#ifdef A2DP_AAC_DIRECT_TRANSFER
            if(twsd->tws_mode != TWSON)
                tws_create_spp_server(Stream);
//#endif
            *(volatile uint32_t *)0xd02200f0 &= ~0x01;
            break;
        case A2DP_EVENT_STREAM_START_IND:
            TRACE("%s,A2DP_EVENT_STREAM_START_IND,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            app_tws_reset_tws_data();
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            tws_player_stop(BT_STREAM_SBC);
            break;
         case A2DP_EVENT_STREAM_DATA_IND:
            if (!app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)&&
#ifdef MEDIA_PLAYER_SUPPORT
                !app_bt_stream_isrun(APP_PLAY_BACK_AUDIO)&&
#endif
                !app_audio_manager_bt_stream_voice_request_exist() &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_VOICE ) &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_MEDIA ) &&
                 tws_reconnect_process == false &&
                 app_poweroff_flag == 0 
#ifdef __TWS_RECONNECT_USE_BLE__
                 && (app_tws_ble_reconnect.bleReconnectPending == 0)
#endif
                 )
            {
            
            if(app_tws_media_frame_analysis(twsd,Stream,Info))
                res = 1;
            }
            else
            {
                 res = 1;
                TRACE("SLAVE SKIP SBC PACKET");
            }
            break;
        case A2DP_EVENT_STREAM_IDLE:
            TRACE("%s,A2DP_EVENT_STREAM_IDLE,state=%d,error=%d",__FUNCTION__,Info->status,Info->error);
            twsd->stream_idle = 1;
            break;
         default:
            break;
    }
    return res;
#if 0    
    if (is_link_request_from_tws(twsd, Stream, (A2dpCallbackParms *)Info) ){
        // set device to slave mode
        TWSCON_DBLOG("\n%s %d req from tws\n",__FUNCTION__,__LINE__);
    }    
    if (is_invalid_frame_in(twsd,Stream,(A2dpCallbackParms *)Info)){ ////A2DP_EVENT_STREAM_DATA_IND
        TWSCON_DBLOG("\n%s %d tws sink invlid data in\n",__FUNCTION__,__LINE__);
        return 1;
    }
#endif
}

//#ifdef A2DP_AAC_ON
void tws_set_mobile_sink_stream(A2dpStream *stream)
{
    tws.mobile_sink.stream = stream;
}
//#endif

extern TWS_SBC_FRAME_INFO sbc_frame_info;

int app_tws_master_a2dp_callback_from_mobile(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    int nRet = 0;
    BtRemoteDevice *remDev = 0;

    tws_dev_t *twsd = &tws;
//    TWSCON_DBLOG("[%s] event=%d ", __func__,Info->event);
    switch(Info->event)
    {
        case A2DP_EVENT_STREAM_OPEN:
            twsd->mobile_sink.connected = true;   
 //           tws.start_media_waiting = false;
            
            ///tws is not connect so set the mode to twson for only mobile connect
            if(twsd->tws_mode == TWSINVD){
                TRACE("set slave to twson");
                app_tws_set_tws_mode(TWSON);

            }
            twsd->unlock_mode(twsd);            
  //          twsd->tws_sink = Stream;
            tws_set_mobile_sink_stream(Stream);

#ifdef A2DP_AAC_ON
            twsd->mobile_codectype = Info->p.configReq->codec.codecType;             
            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_SBC){
                twsd->audout.sample_rate = bt_parse_sbc_sample_rate(Info->p.configReq->codec.elements[0]);
                twsd->pcmbuff.sample_rate = twsd->audout.sample_rate;             
            }else if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                if (Info->p.configReq->codec.elements[1] & A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100){
                    twsd->audout.sample_rate = AUD_SAMPRATE_44100;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_44100;
                }else if (Info->p.configReq->codec.elements[2] & A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000){
                    twsd->audout.sample_rate = AUD_SAMPRATE_48000;
                    twsd->pcmbuff.sample_rate = AUD_SAMPRATE_48000;
                }           
            }           
#else
            twsd->audout.sample_rate = bt_parse_sbc_sample_rate(Info->p.configReq->codec.elements[0]);
            twsd->pcmbuff.sample_rate = twsd->audout.sample_rate;   
#endif
            if(twsd->tws_mode == TWSON)
                btdrv_rf_bit_offset_track_enable(false);
            else
                btdrv_rf_bit_offset_track_enable(true);
            btdrv_rf_set_conidx(btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle));
            tws.mobile_conhdl = Stream->device->cmgrHandler.remDev->hciHandle;
            remDev = A2DP_GetRemoteDevice(Stream);

            if (ME_GetCurrentRole(remDev) == BCR_MASTER) {
                btdrv_rf_trig_patch_enable(0);
                TRACE("%s : as bt master codec = %d\n", __func__, Info->p.configReq->codec.codecType);
            }
            else {
                btdrv_rf_trig_patch_enable(1);
//                if(hal_get_chip_metal_id()==HAL_CHIP_METAL_ID_4)
//                {
//                    btdrv_tws_trig_role(BCR_SLAVE);
//                }
                TRACE("%s : as bt slave codec = %d\n", __func__, Info->p.configReq->codec.codecType);
            }
#ifdef __TWS_FOLLOW_MOBILE__
    *(uint32_t *)0xc00062d4 = btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle);  //tws idx
#endif            
#ifdef __TWS_CALL_DUAL_CHANNEL__
            memcpy(twsd->mobile_addr.addr,Stream->device->cmgrHandler.remDev->bdAddr.addr,6);    
            
            if(twsd->tws_mode == TWSMASTER)
            {
                TRACE("SET FORWARD ROLE");
                DUMP8("%02x ",tws.mobile_addr.addr,6);                  
                DUMP8("%02x ",tws.peer_addr.addr,6); 

                if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_2)
                {
                    struct ld_afh_bak *afh_bak = (struct ld_afh_bak *)0xc0003fb8;
                    struct  ld_afh_bak *afh_bak_id = &afh_bak[btdrv_conhdl_to_linkid(Stream->device->cmgrHandler.remDev->hciHandle)];
                    struct ld_sniffer_env *ld_sniffer = (struct ld_sniffer_env *)0xc0004bf0;
                    TRACE("AFH0:");
                    DUMP8("%02x ",(uint8_t *)0xc0004067,10);
                    TRACE("\n");
                    TRACE("AFH1:");
                    DUMP8("%02x ",(uint8_t *)0xc0004071,10);      
                    TRACE("\n");                    
                    if(Stream->device && Stream->device->cmgrHandler.remDev)
                    {
                        if(Stream->device->cmgrHandler.remDev->role == 1) //SLAVE ROLE
                        {
                            #if 0
                            ld_sniffer->peer_dev.afh_instant = afh_bak_id->instant;
                            ld_sniffer->peer_dev.afh_mode = afh_bak_id->mode;
                            if( ld_sniffer->peer_dev.afh_mode == 0)
                            {
                                uint8_t default_map[]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
                                memcpy(ld_sniffer->peer_dev.chan_map,default_map,10);
                            }
                            else
                            {
                                memcpy( ld_sniffer->peer_dev.chan_map,afh_bak_id->map.map,10);
                            }
                            #else
                            ld_sniffer->peer_dev.afh_mode = 1;
                            memcpy( ld_sniffer->peer_dev.chan_map,(uint8_t *)0xc0004071,10);
                            #endif
                            
           //               ld_sniffer->peer_dev.chan_map = afh_bak_id->map;
                            TRACE("SET SLAVE AFH");
                        }
                        else 
                        {
                             ld_sniffer->peer_dev.afh_instant = 0;
                             ld_sniffer->peer_dev.afh_mode = 1;
                             memcpy( ld_sniffer->peer_dev.chan_map,(uint8_t *)0xc0004067,10);
                            TRACE("SET MASTER AFH"); 
                        }
                        
                        TRACE("CHAN ADDR=%x",&ld_sniffer->peer_dev.chan_map);                        
                        TRACE("CHANMAP=:");
                        DUMP8("%02x",ld_sniffer->peer_dev.chan_map,10);
                        TRACE("\n instant=%x,mode=%x",ld_sniffer->peer_dev.afh_instant,ld_sniffer->peer_dev.afh_mode);           
                        
                    }
                    else
                    {
                        TRACE("ROLE ERROR ,PLEASE CHECK THE MOBILE LINK STATUS!");
                    }
                }

                
#if 0                
                *(unsigned int *)0xc0004c1c = 0;
                *(unsigned int *)0xc0004c20 |= 1;
                *(unsigned int *)0xc0004c10 = 0xffffffff;
                *(unsigned int *)0xc0004c14 = 0xffffffff;
                *(unsigned int *)0xc0004c18 = 0x00007fff;                   
#endif                
                Me_SetSnifferEnv(1,FORWARD_ROLE,tws.mobile_addr.addr,tws.peer_addr.addr);
 //s               tws_player_set_hfp_vol(hfp_volume_get());            
         
                
            }
#endif

#ifdef FPGA            
            TRACE("FPGA MODE CONNECT TWS SLAVE");
            if(twsd->tws_mode == TWSON){
                uint8_t addr[6] = {0x55, 0x33, 0x22, 0x11, 0xB1, 0x1A};
                A2DP_OpenStream(twsd->tws_source.stream, (BT_BD_ADDR *)addr);
            }
#endif            
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
                 TWS_SET_M_STREAM_STATE(TWS_STREAM_OPEN);
#endif
            break;
        case A2DP_EVENT_STREAM_OPEN_IND:
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
                 TWS_SET_M_STREAM_STATE(TWS_STREAM_OPEN)
#endif
#ifdef A2DP_AAC_ON
            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_SBC){
                twsd->audout.sample_rate = bt_parse_sbc_sample_rate(Info->p.configReq->codec.elements[0]);
                twsd->pcmbuff.sample_rate = twsd->audout.sample_rate;     
            }else if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                if (Info->p.configReq->codec.elements[1] & A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100){
                    twsd->audout.sample_rate = AUD_SAMPRATE_44100;
                    twsd->pcmbuff.sample_rate  = AUD_SAMPRATE_44100;
                }else if (Info->p.configReq->codec.elements[2] & A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000){
                    twsd->audout.sample_rate = AUD_SAMPRATE_48000;
                    twsd->pcmbuff.sample_rate = AUD_SAMPRATE_48000;
                }         
            }
#else
            twsd->audout.sample_rate = bt_parse_sbc_sample_rate(Info->p.configReq->codec.elements[0]);
            twsd->pcmbuff.sample_rate = twsd->audout.sample_rate;
#endif            
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            //close tws sink channel
            TWSCON_DBLOG("\n%s %d mobile stream close tws_mode=%d\n",__FUNCTION__,__LINE__,twsd->tws_mode);
            TRACE("conhdl=%x\n",tws.mobile_conhdl);
            app_bt_hcireceived_data_clear(tws.mobile_conhdl);
            HciBypassProcessReceivedDataExt(NULL);
            btdrv_set_powerctrl_rssi_low(0);
            
            // A2DP_CloseStream(twsd->tws_sink.stream);
            restore_mobile_channels();
            app_tws_reset_tws_data();
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
            TWS_SET_M_STREAM_STATE(TWS_STREAM_CLOSE)
#endif            
            ///if tws is active so check the slave is playing or not 
            ///if slave playing,so stop sbc play 
            if(twsd->tws_mode == TWSMASTER && tws.media_suspend == false)
            {
                tws.media_suspend = true;
                if(tws.current_operation == APP_TWS_IDLE_OP)
                {
                    uint8_t status;
                    status = A2DP_SuspendStream(tws.tws_source.stream);
                    if(status == BT_STATUS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                        APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);                    
                    }
                    else
                    {
                        TRACE("%s,suspend stream=%x",__FUNCTION__,status);
                    }
                }
                else
                {
                    APP_TWS_SET_NEXT_OP(APP_TWS_SUSPEND_STREAM);  
                }     

              
            }
#ifdef __TWS_FOLLOW_MOBILE__
            *(uint32_t *)0xc00062d4 = 0xff;
#endif
            if(twsd->tws_mode == TWSON)
                app_tws_set_tws_mode(TWSINVD);
            //should be in stream closed end, or the trans thread may send frame to slave in wrong codec type
#ifdef A2DP_AAC_DIRECT_TRANSFER
            twsd->mobile_codectype = AVDTP_CODEC_TYPE_NON_A2DP;   
#endif            
            break;
        case A2DP_EVENT_STREAM_START_IND:
            TRACE("%s,A2DP_EVENT_STREAM_START_IND twsmode = %d",__FUNCTION__,tws.tws_mode);
            btdrv_set_powerctrl_rssi_low(0xffff);
            
            if(tws.tws_mode == TWSMASTER ||tws.tws_mode == TWSON)
            {
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)            
                if(app_get_play_role() == PLAYER_ROLE_SD)
                    tws_player_stop(BT_STREAM_RBCODEC);
                else if(app_get_play_role() == PLAYER_ROLE_LINEIN)
                    tws_player_stop(BT_STREAM_LINEIN);
#endif                
                app_tws_reset_tws_data();
                if(tws.tws_mode == TWSMASTER)
                {
                    //tws.media_suspend = false;
                    BtStackState state=0;
          //          TWSCON_DBLOG("send start to slave start stream state= %d,tws.suspend_media_pending=%d",
         //               tws.tws_source.stream->stream.state,tws.suspend_media_pending);
          //          tws.start_media_waiting = true;
                    A2DP_StartStreamRsp(Stream,A2DP_ERR_NO_ERROR);  
                    TRACE("A2DP_EVENT_STREAM_START_IND curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
              //      if(tws.tws_source.stream->stream.state == AVDTP_STRM_STATE_OPEN)
             //       {
                        if(tws.current_operation == APP_TWS_IDLE_OP)
                        {
                            app_tws_check_max_slot_setting();
                            state = A2DP_StartStream(tws.tws_source.stream);
                            if(state == BT_STATUS_PENDING)
                            {
                                APP_TWS_SET_CURRENT_OP(APP_TWS_START_STREAM);
                            }
                            else
                            {
                                TRACE("%s,start stream=%x",__FUNCTION__,state);
                            }
                        }
                        else
                        {
                            APP_TWS_SET_NEXT_OP(APP_TWS_START_STREAM);    
                            state = BT_STATUS_PENDING;
                        }
                        if(state != BT_STATUS_PENDING)
                        {
                  //          A2DP_StartStreamRsp(Stream,A2DP_ERR_NO_ERROR);  
                            tws.stream_reset =1;
                            state = A2DP_IdleStream(tws.tws_source.stream);
                            //reset tws a2dp connect
                        }
                        TWSCON_DBLOG("send start to slave state = %x",state);
          //          }
                  //  else if(tws.tws_source.stream->stream.state == AVDTP_STRM_STATE_STREAMING  && tws.suspend_media_pending != true)
       //             {
                        ////tws is already streaming,so just start rsp to the mobile device
               //         A2DP_StartStreamRsp(Stream,A2DP_ERR_NO_ERROR);  
     //               }
       //             else if(tws.suspend_media_pending == true)
       //             {
      //                  TRACE("can't send start to slave so wait ");
      //                  tws.start_media_pending = true;
     //               }
           //         else
         //           {
         //               TRACE("error state!!! please check the tws slave state");
        //            }
                    
                }
            } 
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
            TWS_SET_M_STREAM_STATE(TWS_STREAM_START)
#endif            
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED twsmode = %d,error=%x,status=%x,stream=%x",__FUNCTION__,tws.tws_mode,Info->error,Info->status,Stream);
            if(tws.tws_mode == TWSMASTER)
            {
                uint16_t status;
                tws.lock_prop(&tws);
                tws.media_suspend = true;
      //          tws.suspend_media_pending = true;
        //        tws.start_media_waiting = false;         
                tws.unlock_prop(&tws);
                TRACE("%s,A2DP_EVENT_STREAM_SUSPENDED curr op=%d,next op=%d",tws.current_operation,tws.next_operation);
                if(tws.current_operation == APP_TWS_IDLE_OP)
                {
                    status = A2DP_SuspendStream(tws.tws_source.stream);
                    if(status == BT_STATUS_PENDING)
                    {
                        APP_TWS_SET_CURRENT_OP(APP_TWS_SUSPEND_STREAM);
                        APP_TWS_SET_NEXT_OP(APP_TWS_IDLE_OP);                    
                    }
                    else
                    {
                        TRACE("%s,suspend stream=%x",__FUNCTION__,status);                    
                    }
                }
                else
                {
                    APP_TWS_SET_NEXT_OP(APP_TWS_SUSPEND_STREAM);  
                    status = BT_STATUS_PENDING;
                }                
            //    status = A2DP_SuspendStream(tws.tws_source.stream);
                TWSCON_DBLOG("send suspend to slave status=%x,source stream=%x",status,tws.tws_source.stream);

                
            }            
            
            if (app_bt_device.hf_audio_state[0] == HF_AUDIO_DISCON){
                 remDev = A2DP_GetRemoteDevice(Stream);
                Me_SetLinkPolicy(remDev, BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
            }
            
            tws_player_stop(BT_STREAM_SBC);
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER) 
            TWS_SET_M_STREAM_STATE(TWS_STREAM_SUSPEND)
#endif             
            break;
        case A2DP_EVENT_STREAM_STARTED:
             remDev = A2DP_GetRemoteDevice(Stream);
            Me_SetLinkPolicy(remDev, 0);
            break;
        case A2DP_EVENT_STREAM_DATA_IND:
            if (!app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM)&&
#ifdef MEDIA_PLAYER_SUPPORT
                !app_bt_stream_isrun(APP_PLAY_BACK_AUDIO)&&
#endif
                !app_audio_manager_bt_stream_voice_request_exist() &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_VOICE ) &&
                (app_audio_manager_get_current_media_type() !=BT_STREAM_MEDIA ) &&
                 tws_reconnect_process == false &&
                 app_poweroff_flag == 0 
#ifdef __TWS_RECONNECT_USE_BLE__
                 && (app_tws_ble_reconnect.bleReconnectPending == 0)
#endif
                &&((tws.tws_mode == TWSMASTER) ? 
                            (tws.tws_source.stream->stream.state == AVDTP_STRM_STATE_STREAMING) : 1)
                && tws.current_operation == APP_TWS_IDLE_OP//tws.start_media_pending == false

                 ){
                if(app_tws_analysis_media_data_from_mobile(twsd, Stream, (A2dpCallbackParms *)Info))
                {
                    int header_len = 0;
                    u32 sbc_packet_len = 0;
                    U8 frameNum = 0;
                    unsigned char * sbc_buffer = NULL;
                    
                    TWSCON_DBLOG("\n%s %d  is first data from mobile\n",__FUNCTION__,__LINE__);
#ifdef __A2DP_AVDTP_CP__
                    header_len = AVDTP_ParseMediaHeader(&media_header, Info->p.data,app_bt_device.avdtp_cp);        
#else
                    header_len = AVDTP_ParseMediaHeader(&media_header, Info->p.data,0);  
#endif
                    app_tws_start_master_player(Stream);
                    tws.pause_packets = 0;
                    sbc_buffer = (unsigned char *)Info->p.data + header_len;
                    sbc_packet_len = Info->len - header_len - 1;
                    frameNum = *(sbc_buffer);
                    //TRACE("@%x %x %x %x",*(sbc_buffer),*(sbc_buffer+1),*(sbc_buffer+2),*(sbc_buffer+3));
                    if(0==frameNum)
                    {
                        TRACE("@@@##########framenum:%d header_len:%d",frameNum,header_len);
                        nRet=1;
                        return nRet;
                    }
                    sbc_frame_info.frameLen = sbc_packet_len/frameNum;
                    sbc_frame_info.frameNum = frameNum;
                }
            }else{
                if(app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
                    TRACE("PCM FILTER");
                if(app_bt_stream_isrun(APP_PLAY_BACK_AUDIO))
                    TRACE("AUDIO FILTER");
                if(app_audio_manager_bt_stream_voice_request_exist())
                    TRACE("VOICE REQ ESIST");
                if(app_audio_manager_get_current_media_type() ==BT_STREAM_VOICE)
                    TRACE("VOICE FILTER");
                if(app_audio_manager_get_current_media_type() ==BT_STREAM_MEDIA)
                    TRACE("MEDIA FILTER");
                if(tws_reconnect_process !=false)
                    TRACE("reconn flter");
                if(app_poweroff_flag)
                    TRACE("power off filter");
                if(app_tws_ble_reconnect.bleReconnectPending)
                    TRACE("ble reconn filter");
                if(tws.tws_mode == TWSMASTER && tws.tws_source.stream->stream.state != AVDTP_STRM_STATE_STREAMING)
                    TRACE("stream filter");
                if(tws.current_operation != APP_TWS_IDLE_OP)
                    TRACE("CURR OP FILTER");
                    
                nRet = 1;
            }
            break;
    }
    return nRet;
}



/*
 * return 0: continue pass to a2dp_callback;1: end of a2dp event process 
 */
int tws_a2dp_callback(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    tws_dev_t *twsd = &tws;
    //TWS_DBLOG("\nenter: %s %d e:%d\n",__FUNCTION__,__LINE__,Info->event);

    if(is_tws_sink(twsd, Stream,Info)){
        //TWS_DBLOG("\n%s %d is tws sink\n",__FUNCTION__,__LINE__);
        return app_tws_slave_a2dp_callback(Stream,Info);

    }
    else{  
        return  app_tws_master_a2dp_callback_from_mobile(Stream,Info);
            //TWS_DBLOG("\n%s %d  from mobile\n",__FUNCTION__,__LINE__);
    }
}


int app_tws_fill_addr_to_array(const BT_BD_ADDR *addr)
{
    uint8_t i;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 0)
        {
            tws_inq_addr[i].used =1;
            memcpy(tws_inq_addr[i].bdaddr.addr,addr->addr,BD_ADDR_SIZE);
            return 0;
        }
    }

    return -1;
}


uint8_t app_tws_get_tws_addr_inq_num(void)
{
    uint8_t i,count=0;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 1)
        {
            count++;
        }
    }    
    return count;
}



bool app_tws_is_addr_in_tws_inq_array(const BT_BD_ADDR *addr)
{
    uint8_t i;
    for(i=0;i<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]);i++)
    {
        if(tws_inq_addr[i].used == 1)
        {
            if(!memcmp(tws_inq_addr[i].bdaddr.addr,addr->addr,BD_ADDR_SIZE))
                return true;
        }
    }    
    return false;
}

void app_tws_clear_tws_inq_array(void)
{
    memset(&tws_inq_addr,0,sizeof(tws_inq_addr));
}


static void app_tws_inquiry_timeout_handler(void const *param)
{

    TWSCON_DBLOG("app_tws_inquiry_timeout_handler\n");

//Modified by ATX : Leon.He_20171227: support liac tws pairing
#ifdef __LIAC_FOR_TWS_PAIRING__
    ME_Inquiry(BT_IAC_LIAC, PEER_INQUIRE_TIMEOUT, 0);
#else
    ME_Inquiry(BT_IAC_GIAC, PEER_INQUIRE_TIMEOUT, 0);
#endif
}




void tws_app_stop_find(void)
{
    tws_find_process=0;
    ME_UnregisterGlobalHandler(&me_handler);

}


static void bt_call_back(const BtEvent* event)
{
    //   TWS_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    uint8_t device_name[64];
    uint8_t device_name_len;
    switch(event->eType){
        case BTEVENT_NAME_RESULT:
            TWSCON_DBLOG("\n%s %d BTEVENT_NAME_RESULT\n",__FUNCTION__,__LINE__);
            break;
        case BTEVENT_INQUIRY_RESULT: 
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_RESULT\n",__FUNCTION__,__LINE__);
            DUMP8("%02x ", event->p.inqResult.bdAddr.addr, 6);
            TWSCON_DBLOG("inqmode = %x",event->p.inqResult.inqMode);
            DUMP8("%02x ", event->p.inqResult.extInqResp, 20);
            ///check the uap and nap if equal ,get the name for tws slave

            if((event->p.inqResult.bdAddr.addr[5]== bt_addr[5]) && (event->p.inqResult.bdAddr.addr[4]== bt_addr[4]) &&
                    (event->p.inqResult.bdAddr.addr[3]== bt_addr[3]))
            {
                ///check the device is already checked
                if(app_tws_is_addr_in_tws_inq_array(&event->p.inqResult.bdAddr))
                {
                    break;
                }            
                ////if rssi event is eir,so find name derictly
                if(event->p.inqResult.inqMode == INQ_MODE_EXTENDED)
                {
                    uint8_t *eir = (uint8_t *)event->p.inqResult.extInqResp;
                    device_name_len = ME_GetExtInqData(eir,0x09,device_name,sizeof(device_name));
                    if(device_name_len>0)
                    {
                        ////if name is the same as the local name so we think the device is the tws slave
                        if(!memcmp(device_name,BT_LOCAL_NAME,device_name_len))
                        {
                            BtStatus status;
                            BtDeviceRecord record;
                            tws_dev_t *twsd = &tws;
                            A2dpStream *src = twsd->tws_source.stream;
                            ME_CancelInquiry(); 
                            osTimerStop(app_tws_timer);
                            tws.create_conn_pending = true;
                            memcpy(tws.connecting_addr.addr,event->p.inqResult.bdAddr.addr,6);
                            status = SEC_FindDeviceRecord(&(tws.connecting_addr), &record);
                            if (status == BT_STATUS_SUCCESS){
                                SEC_DeleteDeviceRecord(&(tws.connecting_addr));
                            }
                            A2DP_OpenStream(src, (BT_BD_ADDR *)&event->p.inqResult.bdAddr);
//                            tws.notify(&tws);
                        }

                        else
                        {
                            if(app_tws_get_tws_addr_inq_num()<sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]))
                            {
                                app_tws_fill_addr_to_array(&event->p.inqResult.bdAddr);
                                if(app_tws_get_tws_addr_inq_num()==sizeof(tws_inq_addr)/sizeof(tws_inq_addr[0]))
                                {
                                    ///fail to find a tws slave
                                    ME_CancelInquiry(); 
                                    tws_app_stop_find();
                                }
                            }
                            else
                            {
                                ///fail to find a tws slave
                                ME_CancelInquiry();     
                                tws_app_stop_find();
                            }
                        }
                        break;
                    }
                    /////have no name so just wait for next device
                    //////we can do remote name req for tws slave if eir can't received correctly

                }
            }
            break;
        case BTEVENT_INQUIRY_COMPLETE: 
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_COMPLETE\n",__FUNCTION__,__LINE__);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
			if(app_get_fake_poweroff_flag()== 1)
			{
				TWSCON_DBLOG("exit from tws inquire when fake power off\n");
				tws_app_stop_find();
				return;
			}
#endif
           if(tws_inquiry_count>=MAX_TWS_INQUIRY_TIMES)
            {
                tws_app_stop_find();
                //Modified by ATX :Parker.Wei TWS inquiry timeout and enter pair mode
				app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);					
                return;
            }                

            if(tws.create_conn_pending ==false)
            {
                ////inquiry complete if bt don't find any slave ,so do inquiry again

                uint8_t rand_delay = rand() % 5;
                 tws_inquiry_count++;
                
                if(rand_delay == 0)
                {
//Modified by ATX : Leon.He_20171227: support liac tws pairing
#ifdef __LIAC_FOR_TWS_PAIRING__
                	ME_Inquiry(BT_IAC_LIAC, PEER_INQUIRE_TIMEOUT, 0);
#else
                	ME_Inquiry(BT_IAC_GIAC, PEER_INQUIRE_TIMEOUT, 0);
#endif
                }
                else
                {
                    osTimerStart(app_tws_timer, rand_delay*1000);
                }
            }

            break;
            /** The Inquiry process is canceled. */
        case BTEVENT_INQUIRY_CANCELED:
            TWSCON_DBLOG("\n%s %d BTEVENT_INQUIRY_CANCELED\n",__FUNCTION__,__LINE__);
            // tws.notify(&tws);
            break;
        case BTEVENT_LINK_CONNECT_CNF:
            TWSCON_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_CNF stats=%x\n",__FUNCTION__,__LINE__,event->errCode);
            tws.create_conn_pending = false;
            //connect fail start inquiry again
            if(event->errCode ==4 && tws_find_process == 1)
            {
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
				if(app_get_fake_poweroff_flag()== 1)
				{
		            TWSCON_DBLOG("exit from tws inquire when fake power off\n");
				    tws_app_stop_find();
					return;
				}
#endif
               if(tws_inquiry_count>=MAX_TWS_INQUIRY_TIMES)
                {
                    tws_app_stop_find();
                    //Modified by ATX :Parker.Wei TWS inquiry timeout and enter pair mode
				    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);	
                    return;
                }             
                uint8_t rand_delay = rand() % 5;
                 tws_inquiry_count++;
                if(rand_delay == 0)
                {
//Modified by ATX : Leon.He_20171227: support liac tws pairing
#ifdef __LIAC_FOR_TWS_PAIRING__
                	ME_Inquiry(BT_IAC_LIAC, PEER_INQUIRE_TIMEOUT, 0);
#else
                	ME_Inquiry(BT_IAC_GIAC, PEER_INQUIRE_TIMEOUT, 0);
#endif
                }
                else
                {
                    osTimerStart(app_tws_timer, rand_delay*1000);
                }            
            }
            ///connect succ,so stop the finding tws procedure
            else if(event->errCode ==0)
            {
                tws_app_stop_find();
            }
            break;
        case BTEVENT_LINK_CONNECT_IND:
            TWSCON_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_IND stats=%x\n",__FUNCTION__,__LINE__,event->errCode);
            ////there is a incoming connect so cancel the inquiry and the timer and the  connect creating
            ME_CancelInquiry();
            osTimerStop(app_tws_timer);
            if(tws.create_conn_pending == true)
            {
                tws_dev_t *twsd = &tws;
                A2dpStream *src = twsd->tws_source.stream;
                tws.create_conn_pending = false;
                if(src->device && src->device->cmgrHandler.remDev)
                {
                    ME_CancelCreateLink(&src->device->cmgrHandler.btHandler, src->device->cmgrHandler.remDev);
                }
            }

        default:
            //TWS_DBLOG("\n%s %d etype:%d\n",__FUNCTION__,__LINE__,event->eType);
            break;


    }

    //TWS_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);

}

uint8_t is_find_tws_peer_device_onprocess(void)
{
    return tws_find_process;
}

void find_tws_peer_device_start(void)
{
    TWSCON_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    BtStatus stat;
    if(tws_find_process ==0)
    {
        tws_find_process = 1;
        tws_inquiry_count = 0;
        if (app_tws_timer == NULL)
            app_tws_timer = osTimerCreate(osTimer(APP_TWS_INQ), osTimerOnce, NULL);
        app_tws_clear_tws_inq_array();
        me_handler.callback = bt_call_back;
        ME_RegisterGlobalHandler(&me_handler);
        ME_SetEventMask(&me_handler, BEM_LINK_DISCONNECT|BEM_ROLE_CHANGE|BEM_INQUIRY_RESULT|
                BEM_INQUIRY_COMPLETE|BEM_INQUIRY_CANCELED|BEM_LINK_CONNECT_CNF|BEM_LINK_CONNECT_IND);
#ifdef __TWS_MASTER_STAY_MASTER__        
        HF_SetMasterRole(&app_bt_device.hf_channel[BT_DEVICE_ID_1], true);
#endif
    again:  
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
//Modified by ATX : Leon.He_20171227: support liac tws pairing
#ifdef __LIAC_FOR_TWS_PAIRING__
        stat = ME_Inquiry(BT_IAC_LIAC, PEER_INQUIRE_TIMEOUT, 0);
#else
        stat = ME_Inquiry(BT_IAC_GIAC, PEER_INQUIRE_TIMEOUT, 0);
#endif
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
        if (stat != BT_STATUS_PENDING){
            osDelay(500);
            goto again;
        }
        TWSCON_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
    }
}

void find_tws_peer_device_stop(void)
{
    ME_CancelInquiry();     
    tws_app_stop_find();
}



void app_tws_slave_reconnect_master_start(BT_BD_ADDR *bdAddr)
{
    BtStatus status;
    tws_dev_t *twsd = &tws; 

    A2dpStream *src = twsd->tws_source.stream;

    TWSCON_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    memcpy(tws.connecting_addr.addr,bdAddr->addr,6);
    status = A2DP_OpenStream(src, bdAddr);        
    TWSCON_DBLOG("\nexit: %s %d %d\n",__FUNCTION__,__LINE__, status);   
}


void app_tws_master_reconnect_slave(BT_BD_ADDR *bdAddr)
{
    tws_dev_t *twsd = &tws; 
    BtStatus status;
    A2dpStream *src = twsd->tws_source.stream;

    TWSCON_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    tws.create_conn_pending = true;
    tws_reconnect_process = true;
    memcpy(tws.connecting_addr.addr,bdAddr->addr,6);
    status = A2DP_OpenStream(src, bdAddr);        
//    tws.notify(&tws);

    TWSCON_DBLOG("\nexit: %s %d %d\n",__FUNCTION__,__LINE__, status);   
}





static BtDeviceRecord record; 
static uint8_t device_id = 0;
static void POSSIBLY_UNUSED hfp_reconnect(void *arg)
{
    HF_CreateServiceLink(&app_bt_device.hf_channel[device_id], &record.bdAddr);
}
void reconnect_timer_callback(void const *n) {
    //app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)hfp_reconnect);
    app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)app_bt_profile_connect_manager_opening_reconnect);
}
static osTimerId timerId = NULL;
osTimerDef(master_reconnect_phone_timer, reconnect_timer_callback);

bool app_tws_reconnect_slave_process(uint8_t a2dp_event, uint8_t avctp_event)
{
    static struct _SlaveReconnectParms {
        A2dpEvent   a2dp_event;
        AvctpEvent  avctp_event;
    }slave_conn_parms = {
        .a2dp_event = 0,
        .avctp_event = 0
    };

    //int ret;
    BtStatus status;
    BtDeviceRecord record1;
    //BtDeviceRecord record2;             
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);
    //int need_connect_player = 0;

    TWSCON_DBLOG("%s %d\n",__FUNCTION__, tws_reconnect_process);
    tws_reconnect_mobile = 0;

	if (tws_reconnect_process){
		if (a2dp_event == A2DP_EVENT_STREAM_OPEN ||
			a2dp_event == A2DP_EVENT_STREAM_CLOSED){
			slave_conn_parms.a2dp_event = a2dp_event;
		}
#if 0        
		if (avctp_event == AVCTP_EVENT_CONNECT ||
			avctp_event == AVCTP_EVENT_DISCONNECT){
			slave_conn_parms.avctp_event = avctp_event;
		}
#endif        
		if (slave_conn_parms.a2dp_event == A2DP_EVENT_STREAM_OPEN){

			//add try connect to remote player
			goto connect_to_player;
		}
		if (a2dp_event == A2DP_EVENT_STREAM_CLOSED){
			if (++tws_reconnect_count<MAX_TWS_RECONNECT_TIMES){
				status = SEC_FindDeviceRecord((BT_BD_ADDR *)&nvrecord_env->tws_mode.record.bdAddr, &record1);
				if (status == BT_STATUS_SUCCESS){
					//app_tws_reconnect_slave_start(&record1.bdAddr);					
	                app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, (uint32_t)&nvrecord_env->tws_mode.record.bdAddr,0, (uint32_t)app_tws_master_reconnect_slave);
				}else{
					goto connect_to_player;

				}
			}else{
				goto connect_to_player;
			}
		}
#if 0
		if(avctp_event == AVCTP_EVENT_DISCONNECT){
			//fix add more...
		}
#endif        
	}
	return tws_reconnect_process;

connect_to_player:	
	tws_reconnect_process = false;
	tws_reconnect_count = 0;
      tws_reconnect_mobile = 1;

    if (!timerId) {
        timerId = osTimerCreate(osTimer(master_reconnect_phone_timer), osTimerOnce, (void *)0); 
    }
    if(MEC(pendCons) == 0  && (tws.mobile_sink.connected == false)){
        osTimerStart(timerId, 2000);
        //HF_CreateServiceLink(&app_bt_device.hf_channel[BT_DEVICE_ID_1], &record2.bdAddr);
    }

    return tws_reconnect_process;
}


void app_tws_disconnect_slave(void)
{
    A2dpStream *src;
    //AvrcpChannel *rcp;

    if (app_tws_get_source_stream_connect_status()){
        src = app_tws_get_tws_source_stream();
        if(src->device->cmgrHandler.remDev)
            MeDisconnectLink(src->device->cmgrHandler.remDev);
/*        
        A2DP_CloseStream(src);      
        rcp = app_tws_get_avrcp_TGchannel();
        AVRCP_Disconnect(rcp);
*/        
    }
}

int app_tws_nvrecord_rebuild(void)
{
    BtStatus status;
    struct nvrecord_env_t *nvrecord_env;
    struct tws_mode_t tws_mode;
    nv_record_env_get(&nvrecord_env);
    BtDeviceRecord record;
    bool needrebuild = false;

    TWSCON_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    if (nvrecord_env->tws_mode.mode == TWSMASTER || nvrecord_env->tws_mode.mode == TWSSLAVE){
        status = SEC_FindDeviceRecord((BT_BD_ADDR *)&nvrecord_env->tws_mode.record.bdAddr, &record);
        if (status == BT_STATUS_SUCCESS){
            memcpy(&tws_mode, &nvrecord_env->tws_mode, sizeof(struct tws_mode_t));
            needrebuild = true;
        }
    }
    nv_record_sector_clear();
    DDB_Open(NULL);
    nv_record_env_init();
    if (needrebuild){
        memcpy(&nvrecord_env->tws_mode, &tws_mode, sizeof(struct tws_mode_t));
        nv_record_env_set(nvrecord_env);
        DDB_AddRecord(&record);
    }
    nv_record_flash_flush();
    TWSCON_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);  

    return 0;
}

#ifdef __TWS_RECONNECT_USE_BLE__
void app_tws_reconnect_phone_start(void* arg)
{
    TRACE("%s", __func__);
    tws_reconnect_count = MAX_TWS_RECONNECT_TIMES;
    tws_reconnect_process = true;
    app_tws_reconnect_slave_process(A2DP_EVENT_STREAM_CLOSED, 0);
}

static CmgrHandler app_tws_cmgrHandler;

static void app_tws_CmgrCallback(CmgrHandler *cHandler, 
                              CmgrEvent    Event, 
                              BtStatus     Status)
{
    TRACE("%s cHandler:%x Event:%d status:%d", __func__, cHandler, Event, Status);
    TRACE("addr=");
    DUMP8("%02x ", app_tws_cmgrHandler.remDev->bdAddr.addr, 6);    
}

void app_tws_create_acl_to_slave(BT_BD_ADDR *bdAddr)
{
    BtStatus status;

    //tws_dev_t *twsd = &tws; 

    memcpy(tws.connecting_addr.addr,bdAddr->addr,6);

    status = CMGR_RegisterHandler(&app_tws_cmgrHandler, app_tws_CmgrCallback);

    status = CMGR_CreateDataLink(&app_tws_cmgrHandler, bdAddr);
//     tws.notify(&tws);

    TWSCON_DBLOG("\nexit: %s %d %d\n",__FUNCTION__,__LINE__, status);   
}


//#define __TWS_SLAVE_RECONNECT_MOBILE__




struct TWS_SLAVE_CON_PHONE_TIMER_T{
    u8  Times;
    osTimerId TimerID;
    OsTimerNotify TimerNotifyFunc;
    BtStatus con_status;
    uint8_t no_phone_info;
};

struct TWS_SLAVE_CON_PHONE_TIMER_T tws_slave_con_phone;
void cancel_tws_slave_con_phone_timer(void)
{
    TRACE("cancel_tws_slave_con_phone_timer\n");
    if(tws_slave_con_phone.TimerNotifyFunc) {
        osTimerStop(tws_slave_con_phone.TimerID);
        osTimerDelete(tws_slave_con_phone.TimerID);
        tws_slave_con_phone.TimerNotifyFunc= 0;
    }
}

void app_tws_slave_con_phone(void)
{    
    TRACE("app_tws_slave_con_phone\n");
    cancel_tws_slave_con_phone_timer();
    if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_2)
   {
        *(volatile uint32_t *)0xc000566c = 0x80;
   }
#ifdef __TWS_SLAVE_RECONNECT_MOBILE__
    
    if(MEC(activeCons) ==0)
    {
        app_tws_set_slave_adv_disen();
        app_bt_profile_connect_manager_opening_reconnect();
        if((tws_slave_con_phone.con_status != BT_STATUS_PENDING) || tws_slave_con_phone.no_phone_info){
            app_tws_set_private_address();
            app_tws_set_slave_adv_para();
            app_tws_set_slave_adv_data();

            TRACE("%s open_ble_adv",__func__);
            app_tws_set_slave_adv_en(true);
        }
    }
#endif

    
}

void tws_slave_con_phone_timer_callback(void const *n) {
    if(tws_slave_con_phone.TimerNotifyFunc)
        tws_slave_con_phone.TimerNotifyFunc();
}
osTimerDef(tws_slave_con_phone_timer, tws_slave_con_phone_timer_callback);

#define APP_START_TWS_SLAVE_CON_PHONE_MS (5000)

void app_start_tws_slave_con_phone_timer(void)
{
    tws_slave_con_phone.con_status = 0xff;
    tws_slave_con_phone.no_phone_info = 0;
    
    TRACE("app_start_tws_slave_con_phone_timer\n");
    if(tws_slave_con_phone.TimerNotifyFunc == 0){
        tws_slave_con_phone.TimerID= osTimerCreate(osTimer(tws_slave_con_phone_timer), osTimerOnce, NULL);
    }
    tws_slave_con_phone.TimerNotifyFunc = app_tws_slave_con_phone;

    osTimerStart(tws_slave_con_phone.TimerID, APP_START_TWS_SLAVE_CON_PHONE_MS);
}



extern "C" void MeWriteBtPagescan_Type(uint8_t scan_type);

void app_tws_start_reconnct(struct tws_mode_t *tws_mode)
{
    //BtDeviceRecord record;	
    BtStatus status = XA_STATUS_FAILED;

    if(tws_mode->mode == TWSMASTER)
    {
        //if last status is twsmaster so just try to connect twsslave
        app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);

        if (app_tws_ble_reconnect.scan_func) {
            if (app_tws_ble_reconnect.master_ble_scan_timerId) {
                status = app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
				osTimerStart(app_tws_ble_reconnect.master_ble_scan_timerId, 6000); //Modified by ATX : Paker.Wei_20180103: force to start reconnect timer,avoid sometimes not auto reconnect phone
                TRACE("app_tws_ble_reconnect scan_func , status: %d", status);
            }
#if 0//Modified by ATX : Paker.Wei_20180103: force to start reconnect timer,avoid sometimes not auto reconnect phone
            if (status == BT_STATUS_SUCCESS) {
                osTimerStart(app_tws_ble_reconnect.master_ble_scan_timerId, 6000);
            }
#endif
        }
    }
#if  !defined (__MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__) || !defined (__MASTER_AUTO_TWS_SEARCHING_WITH_EMPTY_PDL__)
//Modified by ATX : Leon.He_20171219:master shouldn't be slave, workaround, to force reconnect.
    else if(tws_mode->mode == TWSSLAVE)
    {
        if (app_tws_ble_reconnect.adv_func) {
            status = app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
            TRACE("app_tws_ble_reconnect adv_func, status: %d", status);
            
            if(hal_get_chip_metal_id()>=HAL_CHIP_METAL_ID_2)
            {
                TRACE("scan interval = %x",*(volatile uint32_t *)0xc000566c);
                *(volatile uint32_t *)0xc000566c = 0x20;
            }
             MeWriteBtPagescan_Type(1);
//            *((volatile uint32_t *)0XD02201B0) |= (1 << 15);  ////ble first always
            app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_NCON);

//Modified by ATX : Leon.He_20170323: prohibit slave connect phone timer.
#ifdef __TWS_SLAVE_RECONNECT_MOBILE__
            app_start_tws_slave_con_phone_timer();
#endif

#ifdef __SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
            app_start_tws_slave_reconnect_timer();
#endif
        }
    }
#endif//__MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
    else
    {
        app_bt_profile_connect_manager_opening_reconnect();
    }
}

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
uint16_t app_tws_get_master_stream_state(void)
{
    return tws.mobile_sink.stream_state;
}
#endif
#endif

#endif

