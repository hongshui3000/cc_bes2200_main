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
#include "apps.h"
#include "app_status_ind.h"
#include "app_utils.h"
#include "app_bt_stream.h"
#include "nvrecord_dev.h"
#include "tgt_hardware.h"
#include "besbt_cfg.h"

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
#include "hid.h"


#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif

#if IAG_BLE_INCLUDE == XA_ENABLED
extern void bridge_ble_stack_init(void);
extern void bridge_process_ble_event(void);
extern void bridge_gatt_client_proc_thread();
#endif
void BESHCI_Open(void);
void BESHCI_Poll(void);
void BESHCI_SCO_Data_Start(void);
void BESHCI_SCO_Data_Stop(void);
void BESHCI_LockBuffer(void);
void BESHCI_UNLockBuffer(void);
}
#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"

struct besbt_cfg_t besbt_cfg = {
#ifdef __BT_SNIFF__
    .sniff = true,
#else
    .sniff = false,
#endif
#ifdef __BT_ONE_BRING_TWO__
    .one_bring_two = true,
#else
    .one_bring_two = false,
#endif
};


SbcEncoder sbc_encoder;
SbcDecoder sbc_decoder;



osMessageQDef(evm_queue, 128, message_t);
osMessageQId  evm_queue_id;

/* besbt thread */
#if IAG_BLE_INCLUDE == XA_ENABLED
#define BESBT_STACK_SIZE 1024*5
#else
#ifdef __TWS__
#define BESBT_STACK_SIZE (1024*4) 
#else
#define BESBT_STACK_SIZE (1024*2+512)
#endif


#endif

static osThreadId besbt_tid;
uint32_t os_thread_def_stack_Besbt[BESBT_STACK_SIZE / sizeof(uint32_t)];
osThreadDef_t os_thread_def_BesbtThread = { (BesbtThread), (osPriorityAboveNormal), (BESBT_STACK_SIZE), (os_thread_def_stack_Besbt)};



unsigned char BT_INQ_EXT_RSP[INQ_EXT_RSP_LEN];


#ifdef __TWS__
//#define __TWS_FIX_AFH_CHANNEL__
#ifdef __TWS_FIX_AFH_CHANNEL__
BtChannelClass host_chanmap = {
    .map = {0xff,0x03,0,0,0,0,0,0,0x0f,0x7f}
};
extern uint8_t local_feature[];
#endif
#endif


static BESBT_HOOK_HANDLER bt_hook_handler[BESBT_HOOK_USER_QTY] = {0};

int Besbt_hook_handler_set(enum BESBT_HOOK_USER_T user, BESBT_HOOK_HANDLER handler)
{
    bt_hook_handler[user]= handler;
    return 0;
}

static void Besbt_hook_proc(void)
{
    uint8_t i;
    for (i=0; i<BESBT_HOOK_USER_QTY; i++){
        if (bt_hook_handler[i]){
            bt_hook_handler[i]();
        }
    }
}

void bes_refresh_device_name(void)
{
    ME_SetLocalDeviceName((const unsigned char*)BT_LOCAL_NAME, strlen(BT_LOCAL_NAME) + 1);
}


extern struct BT_DEVICE_T  app_bt_device;

extern void a2dp_init(void);
extern void app_hfp_init(void);

extern struct BT_DEVICE_T  app_bt_device;
#if (SPP_CLIENT == XA_ENABLED) || (SPP_SERVER == XA_ENABLED)
extern void app_spp_init(void);
#endif
#if (SPP_SERVER == XA_ENABLED)
void spp_callback(SppDev *locDev, SppCallbackParms *Info);
void spp_create_read_thread(void);
#endif

unsigned char *randaddrgen_get_bt_addr(void)
{
    return bt_addr;
}

unsigned char *randaddrgen_get_ble_addr(void)
{
    return ble_addr;
}

const char *randaddrgen_get_btd_localname(void)
{
    return BT_LOCAL_NAME;
}

#if HID_DEVICE == XA_ENABLED
static bool shutter_mode = false;
void hid_callback(HidChannel *Chan, HidCallbackParms *Info)
{
    TRACE("hid_callback Info->event=%x\n",Info->event);

    if (Info->event == HID_EVENT_INTERRUPT_CONNECTED)
    {
        TRACE("HID_EVENT_INTERRUPT_CONNECTED \n");
    }
    else if (Info->event == HID_EVENT_CONTROL_CONNECTED)
    {
        TRACE("HID_EVENT_CONTROL_CONNECTED \n");
    }
    else if (Info->event == HID_EVENT_INTERRUPT_DISCONNECTED)
    {
        TRACE("HID_EVENT_INTERRUPT_DISCONNECTED \n");        
        shutter_mode = false;
    }
    else if (Info->event == HID_EVENT_CONTROL_DISCONNECTED)
    {
        TRACE("HID_EVENT_CONTROL_DISCONNECTED \n");        
        shutter_mode = false;
    }
}

void hid_enter_shutter_mode(void)
{
    BtRemoteDevice *RemDev = app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev;

    TRACE("hid_enter_shutter_mode \n");
    OS_LockStack();
	if (!shutter_mode){
        if (RemDev){
            Hid_Connect(&app_bt_device.hid_channel[BT_DEVICE_ID_1], RemDev);
            shutter_mode = true;
        }
	}    
    OS_UnlockStack();
}

void hid_exit_shutter_mode(void)
{
    BtRemoteDevice *RemDev = app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev;
    
    TRACE("hid_exit_shutter_mode \n");    
    OS_LockStack();
    if (shutter_mode){
        if (RemDev){
            Hid_Disconnect(&app_bt_device.hid_channel[BT_DEVICE_ID_1]);
        }
        shutter_mode = false;
    }    
    OS_UnlockStack();
}
#endif

#ifdef HCI_DEBUG
extern "C" uint32_t thread_index;
extern "C" uint32_t thread_count;
#endif

uint8_t sco_num=0;
#ifdef __TWS_CALL_DUAL_CHANNEL__
void bt_sco_check_pcm(void);
#endif
int besmain(void)
{
#ifdef RANDOM_BTADDR_GEN
    dev_addr_name devinfo;
#endif

    OS_Init();

    BESHCI_Open();

    for(uint8_t i=0;i<BT_DEVICE_NUM;i++)
    {
        memset(&(app_bt_device.a2dp_stream[i]), 0, sizeof(app_bt_device.a2dp_stream[i]));
        memset(&(app_bt_device.avrcp_channel[i]), 0, sizeof(app_bt_device.avrcp_channel[i]));
        memset(&(app_bt_device.hf_command[i]), 0, sizeof(app_bt_device.hf_command[i]));
        memset(&(app_bt_device.hf_channel[i]), 0, sizeof(app_bt_device.hf_channel[i]));
        memset(&(app_bt_device.spp_dev[i]), 0, sizeof(app_bt_device.spp_dev[i]));
    }
#ifdef CHIP_BEST1000
    
#if !defined(HFP_1_6_ENABLE)  
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
    {
        sco_num = 2;
    }
    else
#endif
    {
        sco_num = 1;
    }
#endif

#ifdef CHIP_BEST2000
    sco_num = 2;
#endif


#if IAG_BLE_INCLUDE == XA_ENABLED
    bridge_ble_stack_init();
#endif
    /* bes stack init */
    EVM_Init();


    a2dp_init();
    app_hfp_init();
#if (SPP_CLIENT == XA_ENABLED) || (SPP_SERVER == XA_ENABLED)
	app_spp_init();
#endif
    /* pair callback init */
    pair_handler.callback = pair_handler_func;
    SEC_RegisterPairingHandler(&pair_handler);
#if BT_SECURITY == XA_ENABLED
    auth_handler.callback = auth_handler_func;
    SEC_RegisterAuthorizeHandler(&auth_handler);
#endif

    /* a2dp register */
    a2dp_avdtpcodec.codecType = AVDTP_CODEC_TYPE_SBC;
    a2dp_avdtpcodec.discoverable = 1;
    a2dp_avdtpcodec.elements = (U8 *)&a2dp_codec_elements;
    a2dp_avdtpcodec.elemLen  = 4;

#if defined(A2DP_AAC_ON)
    a2dp_aac_avdtpcodec.codecType = AVDTP_CODEC_TYPE_MPEG2_4_AAC;
    a2dp_aac_avdtpcodec.discoverable = 1;
    a2dp_aac_avdtpcodec.elements = (U8 *)&a2dp_codec_aac_elements;
    a2dp_aac_avdtpcodec.elemLen  = sizeof(a2dp_codec_aac_elements);
    app_bt_device.a2dp_aac_stream.type = A2DP_STREAM_TYPE_SINK;
#ifndef __BT_ONE_BRING_TWO__
#ifdef __A2DP_AVDTP_CP__
    //TRACE("##a2dp_avdtpCp:%x",a2dp_avdtpCp);
    a2dp_avdtpCp_aac[0].cpType = AVDTP_CP_TYPE_SCMS_T;
    a2dp_avdtpCp_aac[0].data = (U8 *)&a2dp_avdtpCp_securityData_aac;
    a2dp_avdtpCp_aac[0].dataLen = 1;
    A2DP_Register(&app_bt_device.a2dp_aac_stream, &a2dp_aac_avdtpcodec, &a2dp_avdtpCp_aac[0], a2dp_callback);
    A2DP_AddContentProtection(&app_bt_device.a2dp_aac_stream, &a2dp_avdtpCp_aac[0]);
#else
     A2DP_Register(&app_bt_device.a2dp_aac_stream, &a2dp_aac_avdtpcodec, NULL, a2dp_callback);
#endif 
    avdtp_get_aacenable_callback = avdtp_Get_aacEnable_Flag;
#endif
    //TRACE("##@a2dp_aac_stream = 0x%x \n", &app_bt_device.a2dp_aac_stream.stream);
    //TRACE("##@AACa2dp_stream cp= 0x%x  %x \n", app_bt_device.a2dp_aac_stream.stream.cpList.Flink,app_bt_device.a2dp_aac_stream.stream.cpList.Blink);
    //TRACE("##@&AACa2dp_stream cp= 0x%x  %x \n", &app_bt_device.a2dp_aac_stream.stream.cpList.Flink,&app_bt_device.a2dp_aac_stream.stream.cpList.Blink);
#endif

    for(uint8_t i=0;i<BT_DEVICE_NUM;i++)
    {
        app_bt_device.a2dp_stream[i].type = A2DP_STREAM_TYPE_SINK;
#if defined(A2DP_AAC_ON)        
#ifdef __BT_ONE_BRING_TWO__
        if(i == 0)
             A2DP_Register(&app_bt_device.a2dp_aac_stream, &a2dp_aac_avdtpcodec, a2dp_callback);
#endif
#endif

#ifdef __A2DP_AVDTP_CP__
        a2dp_avdtpCp[i].cpType = AVDTP_CP_TYPE_SCMS_T;
        a2dp_avdtpCp[i].data = (U8 *)&a2dp_avdtpCp_securityData;
        a2dp_avdtpCp[i].dataLen = 1;
        A2DP_Register(&app_bt_device.a2dp_stream[i], &a2dp_avdtpcodec, &a2dp_avdtpCp[i], a2dp_callback);
        A2DP_AddContentProtection(&app_bt_device.a2dp_stream[i], &a2dp_avdtpCp[i]);
        //TRACE("##@a2dp_stream = 0x%x \n", &app_bt_device.a2dp_stream[0].stream);
        //TRACE("##@a2dp_stream cp= 0x%x  %x \n", app_bt_device.a2dp_stream[0].stream.cpList.Flink,app_bt_device.a2dp_stream[0].stream.cpList.Blink);
        //TRACE("##@a2dp_stream cp= 0x%x  %x \n", &app_bt_device.a2dp_stream[0].stream.cpList.Flink,&app_bt_device.a2dp_stream[0].stream.cpList.Blink);

        
#else
        A2DP_Register(&app_bt_device.a2dp_stream[i], &a2dp_avdtpcodec, NULL, a2dp_callback); 
#endif
        /* avrcp register */        
#ifdef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
        AVRCP_Register(&app_bt_device.avrcp_channel[i], avrcp_callback_CT, AVRCP_CT_CATEGORY_2 );
#else
        AVRCP_Register(&app_bt_device.avrcp_channel[i], avrcp_callback_CT, AVRCP_CT_CATEGORY_1 | AVRCP_CT_CATEGORY_2 | AVRCP_TG_CATEGORY_2);
#endif
        /* hfp register */
        HF_Register(&app_bt_device.hf_channel[i], hfp_callback);
#if HF_VERSION_1_6 == XA_ENABLED
        app_bt_device.hf_channel[i].hf_msbc_callback = btapp_hfp_check_msbc_status;
#endif

#if HID_DEVICE == XA_ENABLED
        Hid_Register(&app_bt_device.hid_channel[i],hid_callback);
#endif
#if 0//ndef A2DP_AAC_DIRECT_TRANSFER
#if (SPP_CLIENT == XA_ENABLED) || (SPP_SERVER == XA_ENABLED)
		SPP_InitDevice(&app_bt_device.spp_dev[i], &app_bt_device.txPacket, app_bt_device.numPackets);
#endif
#if (SPP_SERVER == XA_ENABLED)
		SPP_Open(&app_bt_device.spp_dev[i], NULL, spp_callback);
		spp_create_read_thread();
#endif
#endif
    }

#ifdef __TWS__
     static tws_env_cfg_t tws_env_cfg;
    app_tws_env_init(&tws_env_cfg);
    init_tws(&tws_env_cfg);
#endif
    /* bt local name */
#ifdef RANDOM_BTADDR_GEN
    devinfo.btd_addr = randaddrgen_get_bt_addr();
    devinfo.ble_addr = randaddrgen_get_ble_addr();
    devinfo.localname = randaddrgen_get_btd_localname();
    nvrec_dev_localname_addr_init(&devinfo);
    BT_LOCAL_NAME = devinfo.localname;
    ME_SetLocalDeviceName((const unsigned char*)devinfo.localname, strlen(devinfo.localname) + 1);
#else
    ME_SetLocalDeviceName((const unsigned char*)BT_LOCAL_NAME, strlen(BT_LOCAL_NAME) + 1);
#endif

#if BT_BEST_SYNC_CONFIG == XA_ENABLED
    /* sync config */
    ME_SetSyncConfig(SYNC_CONFIG_PATH, SYNC_CONFIG_MAX_BUFFER, SYNC_CONFIG_CVSD_BYPASS);
#endif

    /* err report enable */
    CMGR_ScoSetEDR(EDR_ENABLED);

#if HID_DEVICE == XA_ENABLED
//            ME_SetClassOfDevice(COD_MAJOR_PERIPHERAL|COD_MINOR_PERIPH_KEYBOARD);
    ME_SetClassOfDevice(COD_MAJOR_AUDIO|COD_MAJOR_PERIPHERAL|COD_MINOR_AUDIO_HEADSET|COD_MINOR_PERIPH_KEYBOARD |COD_AUDIO|COD_RENDERING);
#else
    ME_SetClassOfDevice(COD_MAJOR_AUDIO|COD_MINOR_AUDIO_HEADSET|COD_AUDIO|COD_RENDERING);
#endif
#if (FPGA ==1) || defined(__EARPHONE_STAY_BOTH_SCAN__)
    MEC(accModeNCON) = BT_DEFAULT_ACCESS_MODE_PAIR;
#else
    MEC(accModeNCON) = BT_DEFAULT_ACCESS_MODE_NCON;
#endif

    app_bt_golbal_handle_init();
    app_bt_profile_connect_manager_open();

    /* ext inquiry response */
    ME_AutoCreateExtInquiryRsp(BT_INQ_EXT_RSP, INQ_EXT_RSP_LEN);
    ME_SetExtInquiryRsp(1,BT_INQ_EXT_RSP, INQ_EXT_RSP_LEN);
    ME_SetInquiryMode(INQ_MODE_EXTENDED);

#if defined(__TWS__) && defined(__TWS_FIX_AFH_CHANNEL__)
    OS_MemCopy(MEC(btFeatures), local_feature, 8);
    ME_SetChannelClassification(&host_chanmap,false);
#endif    

    
#if defined( __EARPHONE__)
#if (FPGA==1) || defined(APP_TEST_MODE)
    app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
#endif
#endif
    SEC_SetIoCapabilities(IO_NO_IO);
    ///init bt key
    bt_key_init();

    OS_NotifyEvm();
#ifdef __TWS__
	notify_tws_btenv_ready();
#endif	
    
    while(1)
    {
#ifdef __WATCHER_DOG_RESET__
        app_wdt_ping();
#endif
    app_sysfreq_req(APP_SYSFREQ_USER_APP_1, APP_SYSFREQ_32K);
#ifdef HCI_DEBUG	
    if(thread_index)
        thread_index = 0;
    else
        thread_index = 1;
    thread_count ++;
#endif	
    osMessageGet(evm_queue_id, osWaitForever);
    app_sysfreq_req(APP_SYSFREQ_USER_APP_1, APP_SYSFREQ_52M);
//    BESHCI_LockBuffer();
#ifdef __LOCK_AUDIO_THREAD__
    bool stream_a2dp_sbc_isrun = app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC);
    if (stream_a2dp_sbc_isrun)
    {
        af_lock_thread();
    }
#endif
#ifdef HCI_DEBUG
    thread_index = 2;
#endif	
    EVM_Process();
#ifdef HCI_DEBUG	
    thread_index = 3;
#endif	
    Besbt_hook_proc();

    //avrcp_handleKey();
    bt_key_handle();

#if IAG_BLE_INCLUDE == XA_ENABLED
        bridge_process_ble_event();
#endif
#ifdef HCI_DEBUG
    thread_index = 4;
#endif	
#ifdef __LOCK_AUDIO_THREAD__
    if (stream_a2dp_sbc_isrun)
    {
        af_unlock_thread();
    }
#endif
#ifdef HCI_DEBUG
    thread_index = 5;
#endif
//    BESHCI_UNLockBuffer();
    BESHCI_Poll();
#ifdef __TWS__
    app_tws_check_pcm_signal();
#ifdef __TWS_CALL_DUAL_CHANNEL__
     bt_sco_check_pcm();
#endif
#endif
    }

    return 0;
}

void BesbtThread(void const *argument)
{
    besmain();
}

void BesbtInit(void)
{
    evm_queue_id = osMessageCreate(osMessageQ(evm_queue), NULL);
    /* bt */
    besbt_tid = osThreadCreate(osThread(BesbtThread), NULL);
    TRACE("BesbtThread: %x\n", besbt_tid);
}



void bt_thread_set_priority(osPriority priority)
{
    osThreadSetPriority(besbt_tid, priority);
}

