#include "stdio.h"
#include "cmsis_os.h"
#include "list.h"
#include "string.h"

#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_bootmode.h"

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
#ifdef __FACTORY_MODE_SUPPORT__
#include "app_factory.h"
#include "app_factory_bt.h"
#endif
#include "bt_drv_interface.h"
#include "besbt.h"
#include "ddb.h"
#include "nvrecord.h"
#include "nvrecord_dev.h"
#include "nvrecord_env.h"
#include "app_key_config.h"//Modified by ATX : Leon.He_20171123

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

#ifdef MEDIA_PLAYER_SUPPORT
#include "resources.h"
#include "app_media_player.h"
#endif
#include "app_bt_media_manager.h"
#include "hal_sleep.h"
#if defined(BTUSB_AUDIO_MODE)
extern "C"	bool app_usbaudio_mode_on(void) ;
#endif
#ifdef __BT_ANC__
#include "app_anc.h"
#endif

#ifdef __TWS__
#include "app_tws.h"
#endif

#ifdef __TOUCH_KEY__
#include "app_customized_touch_keyhandle.h"
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
void app_switch_player_role_key(APP_KEY_STATUS *status, void *param);
int rb_ctl_init(void);
void player_role_thread_init(void);
#endif

int app_nvrecord_rebuild(void);

#define APP_BATTERY_LEVEL_LOWPOWERTHRESHOLD (1)

static uint8_t  app_system_status = APP_SYSTEM_STATUS_INVALID;

uint8_t app_poweroff_flag=0;//Modified by ATX : Parke.Wei_20180328 redefine the variable in head of the file for upper function call;

uint8_t app_system_status_set(int flag)
{
    uint8_t old_status = app_system_status;
    app_system_status = flag;
    return old_status;
}

uint8_t app_system_status_get(void)
{
    return app_system_status;
}

#ifdef MEDIA_PLAYER_RBCODEC
extern int rb_ctl_init();
extern bool rb_ctl_is_init_done(void);
extern void app_rbplay_audio_reset_pause_status(void);
#endif

#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
extern bool app_pwr_key_monitor_get_val(void);
static bool anc_single_mode_on = false;
extern "C" bool anc_single_mode_is_on(void)
{
	return anc_single_mode_on;
}
#endif

#ifdef __ANC_STICK_SWITCH_USE_GPIO__
extern bool app_anc_switch_get_val(void);
#endif

static uint8_t app_status_indication_init(void)
{
//Modified by ATX : Leon.He_20180119: remove pwl setup when init¡£ TODO: to test more
//    struct APP_PWL_CFG_T cfg;
//    memset(&cfg, 0, sizeof(struct APP_PWL_CFG_T));
	app_pwl_open();
//    app_pwl_setup(APP_PWL_ID_0, &cfg);
//    app_pwl_setup(APP_PWL_ID_1, &cfg);
    return 0;
}

//Modified by ATX : cc_20180327
static void app_reset_phone_paired_list_during_charge(void)
{
    app_status_indication_set(APP_STATUS_INDICATION_CLEARSUCCEED);
    DDB_Open(NULL);
    nv_record_env_init();
    app_clear_phone_paired_record();
    nv_record_touch_cause_flush();
    nv_record_flash_flush();
#ifdef __DUAL_USER_SECTION_BAK__	
    nv_record_flash_backup();
#endif
}

//Modified by ATX : Haorong.Wu_20190224 add atx factory mode for product line testing
#ifdef _ATX_FACTORY_MODE_DETECT_
static int atx_factory_mode_flag = 0;
#define ATX_FACTORY_MODE_ON 1
#define ATX_FACTORY_MODE_OFF 0

extern "C" int get_atx_factory_mode_flag(void);

int get_atx_factory_mode_flag(void)
{
    TRACE("[%s] atx_factory_mode_flag: %d", __FUNCTION__, atx_factory_mode_flag);
    return atx_factory_mode_flag;
}

int atx_factory_mode_check_init(void)
{
    TRACE("[%s]", __FUNCTION__);
    int counter;
    
    hal_iomux_init(atx_factory_pin, 1);
    hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)atx_factory_pin[0].pin, HAL_GPIO_DIR_IN, 1);
    
    atx_factory_mode_flag = ATX_FACTORY_MODE_OFF;
    counter = 1000;
    while(!hal_gpio_pin_get_val((enum HAL_GPIO_PIN_T)atx_factory_pin[0].pin) && counter--)
    {
        atx_factory_mode_flag = ATX_FACTORY_MODE_ON;
    }

    TRACE("[%s] atx_factory_mode_flag: %d", __FUNCTION__, atx_factory_mode_flag);
    return atx_factory_mode_flag;
}

int atx_factory_mode_proc(void)
{
    TRACE("[%s]", __FUNCTION__);
    
    app_status_indication_set(APP_STATUS_INDICATION_ATX_FACTORY_MODE);

    //ble advertising version
    app_check_version_ble_adv_start();

    //enter bt pairing mode
    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
    
    //todo
    return 0;
}
#endif

#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)

void PairingTransferToConnectable(void);

typedef void (*APP_10_SECOND_TIMER_CB_T)(void);

void app_pair_timerout(void);
void app_poweroff_timerout(void);
void CloseEarphone(void);

//Modified by ATX : Leon.He_20170112: support customized timeout value
#ifdef _PROJ_2000IZ_C001_
#define APP_PAIR_TIMEOUT				30
#define APP_AUTO_POWER_OFF_TIMEOUT		60
#endif

#ifdef _PROJ_2000IZ_C005_
#ifndef APP_PAIR_TIMEOUT
#define APP_PAIR_TIMEOUT				30
#endif
#ifndef APP_AUTO_POWER_OFF_TIMEOUT
#define APP_AUTO_POWER_OFF_TIMEOUT		30
#endif
#endif

//Modified by ATX : Parker.Wei_20180324
#ifdef _PROJ_2000IZ_C002_
#define APP_PAIR_TIMEOUT				13

#ifdef __DEFINE_LONG_LONG_10_SECOND_TIMER_
#define APP_AUTO_POWER_OFF_TIMEOUT		1080//3 hours 6*60*3
#else
#define APP_AUTO_POWER_OFF_TIMEOUT		60
#endif

#endif

//Modified by ATX : Parker.Wei_20180324
#ifdef __MOBILE_CONNECTED_AND_NO_AUDIO_AUDO_POWER_OFF_
#define  APP_NO_AUDIO_TIMEOUT           31
#endif

#ifndef APP_PAIR_TIMEOUT
#define APP_PAIR_TIMEOUT           		18
#endif

#ifndef APP_AUTO_POWER_OFF_TIMEOUT
#define APP_AUTO_POWER_OFF_TIMEOUT      30
#endif

//Modified by ATX : Parker.Wei_20180324
#ifdef __MOBILE_CONNECTED_AND_NO_AUDIO_AUDO_POWER_OFF_
void app_no_audio_timeout_handler(void)
{
   TRACE("%s",__func__);
   app_shutdown();

}
#endif



#ifdef __DEFINE_LONG_LONG_10_SECOND_TIMER_ //Modified by ATX : Parke.Wei_20180418 TIMER for hour scale
typedef struct
{
    uint8_t timer_id;
    uint8_t timer_en;
    uint16_t timer_count;
    uint16_t timer_period;
    APP_10_SECOND_TIMER_CB_T cb;
}APP_10_SECOND_TIMER_STRUCT;

#else

typedef struct
{
    uint8_t timer_id;
    uint8_t timer_en;
    uint8_t timer_count;
    uint8_t timer_period;
    APP_10_SECOND_TIMER_CB_T cb;
}APP_10_SECOND_TIMER_STRUCT;



#endif


APP_10_SECOND_TIMER_STRUCT app_pair_timer =
{
    .timer_id =APP_PAIR_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = APP_PAIR_TIMEOUT,
	.cb =	PairingTimeoutHandler  ////Modified by ATX : Parke.Wei_20180418

#if 0	
//Modified by ATX : Leon.He_20170112: support power off after pair timeout
#ifdef __POWER_OFF_AFTER_PAIR_TIMEOUT__
    .cb =    CloseEarphone
#else
    .cb =    PairingTransferToConnectable
#endif
#endif
};

APP_10_SECOND_TIMER_STRUCT app_poweroff_timer=
{
    .timer_id =APP_POWEROFF_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = APP_AUTO_POWER_OFF_TIMEOUT,
    .cb =    CloseEarphone
};

//Modified by ATX : Parke.Wei_20180327
#ifdef __MOBILE_CONNECTED_AND_NO_AUDIO_AUDO_POWER_OFF_
APP_10_SECOND_TIMER_STRUCT app_no_audio_poweroff_timer=
{
    .timer_id =APP_NO_AUDIO_TIMER_ID,
    .timer_en = 0,
    .timer_count = 0,
    .timer_period = APP_NO_AUDIO_TIMEOUT,
    .cb = app_no_audio_timeout_handler
};

#endif

//Modified by ATX : Parker.Wei_20180324
APP_10_SECOND_TIMER_STRUCT  *app_10_second_array[] =
{
    &app_pair_timer,
    &app_poweroff_timer,
#ifdef __MOBILE_CONNECTED_AND_NO_AUDIO_AUDO_POWER_OFF_
	&app_no_audio_poweroff_timer,
#endif

};



void app_stop_10_second_timer(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];
    timer->timer_en = 0;
    timer->timer_count = 0;
}

void app_start_10_second_timer(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];

    timer->timer_en = 1;
    timer->timer_count = 0;
}

void app_10_second_timerout_handle(uint8_t timer_id)
{
    APP_10_SECOND_TIMER_STRUCT *timer = app_10_second_array[timer_id];

    timer->timer_en = 0;
    timer->cb();
}




void app_10_second_timer_check(void)
{
    uint8_t i;
    for(i=0;i<sizeof(app_10_second_array)/sizeof(app_10_second_array[0]);i++)
    {
        if(app_10_second_array[i]->timer_en)
        {
            app_10_second_array[i]->timer_count++;
            if(app_10_second_array[i]->timer_count >= app_10_second_array[i]->timer_period)
            {
                app_10_second_timerout_handle(i);
            }
        }

    }
}
#endif

#if (HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_BATTERY_REPORT) || (HF_SDK_FEATURES & HF_FEATURE_HF_INDICATORS)
extern int app_hfp_battery_report(uint8_t battery_level);
#endif
void intersys_sleep_checker(void);

int app_status_battery_report(uint8_t level)
{

    intersys_sleep_checker();
#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
	if( anc_single_mode_on )//anc power on,anc only mode
		return 0;
#endif
#if defined(__EARPHONE__)
  //  app_bt_state_checker();
    app_10_second_timer_check();
#endif
    if (level<=APP_BATTERY_LEVEL_LOWPOWERTHRESHOLD){
        //add something
    }
#if 0    
    TRACE("MAX SLOT=%x",*(uint32_t *)0xc0003df8);
     TRACE("MAX len=%x",*(uint32_t *)0xc0003e04);
    uint8_t link_id = tws.tws_conhdl- 0x80;
    uint32_t table = *(uint32_t *)0xc0003df8;
    uint32_t len = *(uint32_t *)0xc0003e04;
     uint16_t table_id;
    if(link_id ==0)
        table_id = table &0xffff;
    else if(link_id==1)
        table_id = table >>16;

    if(table_id == 0x8 || table_id ==0xa)
    {
        TRACE("TX TABLE IS ONLY 1SLOT PACKET");
        if(link_id ==0)
        {
            len &= 0xffff0000;
            len |= 0x2a7;
            table |= 0x110a;
            *(uint32_t *)0xc0003e04) = len;
            *(uint32_t *)0xc0003df8) = table;
        }
        else if(link_id ==1)
        {
            len &= 0xffff;
            len |= 0x2a70000;
            table |= 0x110a0000;
            *(uint32_t *)0xc0003e04) = len;
            *(uint32_t *)0xc0003df8) = table;        
        }

        
    }
    
#endif     

#if (HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_BATTERY_REPORT) || (HF_SDK_FEATURES & HF_FEATURE_HF_INDICATORS)
    app_hfp_battery_report(level);
#else
    TRACE("[%s] Can not enable HF_CUSTOM_FEATURE_BATTERY_REPORT",__func__);
#endif
    return 0;
}

#ifdef MEDIA_PLAYER_SUPPORT

void app_status_set_num(const char* p)
{
    media_Set_IncomingNumber(p);
}



extern "C" int app_voice_report(APP_STATUS_INDICATION_T status,uint8_t device_id)
{
#if defined(BTUSB_AUDIO_MODE)
	if(app_usbaudio_mode_on()) return 0;
#endif
    TRACE("%s %d",__func__, status);
    AUD_ID_ENUM id = MAX_RECORD_NUM;
#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
	if(anc_single_mode_on)
		return 0;
#endif
    switch (status) {
        case APP_STATUS_INDICATION_POWERON:
            id = AUD_ID_POWER_ON;
            break;
        case APP_STATUS_INDICATION_POWEROFF:
            id = AUD_ID_POWER_OFF;
            break;
        case APP_STATUS_INDICATION_CONNECTED:
            id = AUD_ID_BT_CONNECTED;
            break;
        case APP_STATUS_INDICATION_DISCONNECTED:
            id = AUD_ID_BT_DIS_CONNECT;
            break;
        case APP_STATUS_INDICATION_CALLNUMBER:
            id = AUD_ID_BT_CALL_INCOMING_NUMBER;
            break;
        case APP_STATUS_INDICATION_CHARGENEED:
            id = AUD_ID_BT_CHARGE_PLEASE;
            break;
        case APP_STATUS_INDICATION_FULLCHARGE:
            id = AUD_ID_BT_CHARGE_FINISH;
            break;
        case APP_STATUS_INDICATION_PAIRSUCCEED:
            id = AUD_ID_BT_PAIRING_SUC;
            break;
        case APP_STATUS_INDICATION_PAIRFAIL:
            id = AUD_ID_BT_PAIRING_FAIL;
            break;

        case APP_STATUS_INDICATION_HANGUPCALL:
            id = AUD_ID_BT_CALL_HUNG_UP;
            break;

        case APP_STATUS_INDICATION_REFUSECALL:
            id = AUD_ID_BT_CALL_REFUSE;
            break;

        case APP_STATUS_INDICATION_ANSWERCALL:
            id = AUD_ID_BT_CALL_ANSWER;
            break;

        case APP_STATUS_INDICATION_CLEARSUCCEED:
            id = AUD_ID_BT_CLEAR_SUCCESS;
            break;

        case APP_STATUS_INDICATION_CLEARFAIL:
            id = AUD_ID_BT_CLEAR_FAIL;
            break;
        case APP_STATUS_INDICATION_INCOMINGCALL:
            id = AUD_ID_BT_CALL_INCOMING_CALL;
            break;
        case APP_STATUS_INDICATION_BOTHSCAN:
            id = AUD_ID_BT_PAIR_ENABLE;
            break;
        case APP_STATUS_INDICATION_WARNING:
            id = AUD_ID_BT_WARNING;
			break;
//Modified by ATX : cc_20180306       
        case APP_STATUS_INDICATION_VOL_MIN:
            id = AUD_ID_BT_VOL_MIN;
			break;
        case APP_STATUS_INDICATION_VOL_MAX:
            id = AUD_ID_BT_VOL_MAX;
			break;
//Modified by ATX : Leon.He_20180109
        case APP_STATUS_INDICATION_LONG_DOUBLE_LOW:
            id = AUD_ID_BT_LONG_DOUBLE_LOW;
            break;
        case APP_STATUS_INDICATION_SHORT_1:
            id = AUD_ID_BT_SHORT_1;
            break;
//Modified by ATX : Leon.He_20180109
		case APP_STATUS_INDICATION_USER_1:
			id = AUD_ID_BT_USER_1;
			break;
		case APP_STATUS_INDICATION_USER_2:
			id = AUD_ID_BT_USER_2;
			break;

#ifdef __TWS__
        case APP_STATUS_INDICATION_TWS_ISMASTER:            
            id = AUD_ID_BT_TWS_ISMASTER;  
            break;
        case APP_STATUS_INDICATION_TWS_ISSLAVE:         
            id = AUD_ID_BT_TWS_ISSLAVE;  
            break;
        case APP_STATUS_INDICATION_TWS_SEARCH:          
            id = AUD_ID_BT_TWS_SEARCH;
            break;
        case APP_STATUS_INDICATION_TWS_STOPSEARCH:          
            id = AUD_ID_BT_TWS_STOPSEARCH;  
            break;  
        case APP_STATUS_INDICATION_TWS_DISCOVERABLE:            
            id = AUD_ID_BT_TWS_DISCOVERABLE;  
            break;
        case APP_STATUS_INDICATION_TWS_LEFTCHNL:            
            id = AUD_ID_BT_TWS_LEFTCHNL;  
            break;
        case APP_STATUS_INDICATION_TWS_RIGHTCHNL:           
            id = AUD_ID_BT_TWS_RIGHTCHNL;  
            break;
//Modified by ATX : Leon.He_20171123
        case APP_STATUS_INDICATION_TWS_CONNECTED:
            id = AUD_ID_BT_TWS_CONNECTED;
            break;
        case APP_STATUS_INDICATION_TWS_PAIRING:
            id = AUD_ID_BT_TWS_PAIRING;
            break;
#endif              


            
        default:
            break;
    }

#if 0  //Modified by ATX : Parke.Wei_20180328  when during poweroff don't play these VP (APP_SYSTEM_STATUS_POWEROFF_PROC isn't used by applicaiton )
    if ((app_system_status_get() == APP_SYSTEM_STATUS_POWEROFF_PROC)&&
#else	
    if ((app_poweroff_flag ==1)&&
#endif
        (status == APP_STATUS_INDICATION_BOTHSCAN    ||
         status == APP_STATUS_INDICATION_DISCONNECTED||
         status == APP_STATUS_INDICATION_CHARGENEED  ||
         status == APP_STATUS_INDICATION_CONNECTING  ||
         status == APP_STATUS_INDICATION_CONNECTED)){
        //block ring tong
    }else{
        media_PlayAudio(id, device_id);
    }
    return 0;
}
#endif


#define POWERON_PRESSMAXTIME_THRESHOLD_MS  (5000)

enum APP_POWERON_CASE_T {
    APP_POWERON_CASE_NORMAL = 0,
    APP_POWERON_CASE_DITHERING,
    APP_POWERON_CASE_REBOOT,
    APP_POWERON_CASE_ALARM,
    APP_POWERON_CASE_CALIB,
    APP_POWERON_CASE_BOTHSCAN,
    APP_POWERON_CASE_CHARGING,
    APP_POWERON_CASE_FACTORY,
    APP_POWERON_CASE_TEST,
//Modified by ATX : cc_20180413  enter version check bootmode 
    APP_POWERON_CASE_CHECK_VERSION,
    APP_POWERON_CASE_INVALID,

    APP_POWERON_CASE_NUM
};


static osThreadId apps_init_tid = NULL;

static enum APP_POWERON_CASE_T pwron_case = APP_POWERON_CASE_INVALID;

static void app_poweron_normal(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    pwron_case = APP_POWERON_CASE_NORMAL;

    osSignalSet(apps_init_tid, 0x2);
}

static void app_poweron_scan(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    pwron_case = APP_POWERON_CASE_BOTHSCAN;

    osSignalSet(apps_init_tid, 0x2);
}

#ifdef __ENGINEER_MODE_SUPPORT__
static void app_poweron_factorymode(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_factorymode_enter();
}
#endif

static void app_poweron_finished(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    osSignalSet(apps_init_tid, 0x2);
}

void app_poweron_wait_finished(void)
{
    osSignalWait(0x2, osWaitForever);
}

#if  defined(__POWERKEY_CTRL_ONOFF_ONLY__)
void app_bt_key_shutdown(APP_KEY_STATUS *status, void *param);
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},           "power on: shutdown"     , app_bt_key_shutdown, NULL},
};
#elif defined(__ENGINEER_MODE_SUPPORT__)
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITUP},           "power on: normal"     , app_poweron_normal, NULL},
//Modified by ATX : Leon.He_20171123
#ifdef __INIT_LONG_PRESS_FOR_PAIRING_
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: both scan"  , app_poweron_scan  , NULL},
#else
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: normal"     , app_poweron_normal, NULL},
#endif
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGLONGPRESS},"power on: factory mode", app_poweron_factorymode  , NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITFINISHED},     "power on: finished"   , app_poweron_finished  , NULL},
};
#else
const  APP_KEY_HANDLE  pwron_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITUP},           "power on: normal"     , app_poweron_normal, NULL},
//Modified by ATX : Leon.He_20171123	
#ifdef __INIT_LONG_PRESS_FOR_PAIRING_
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: both scan"  , app_poweron_scan  , NULL},
#else
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITLONGPRESS},    "power on: normal"     , app_poweron_normal, NULL},
#endif
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_INITFINISHED},     "power on: finished"   , app_poweron_finished  , NULL},
};
#endif
static void app_poweron_key_init(void)
{
    uint8_t i = 0;
    TRACE("%s",__func__);

    for (i=0; i<(sizeof(pwron_key_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&pwron_key_handle_cfg[i]);
    }
}

static enum APP_POWERON_CASE_T app_poweron_wait_case(void)
{
    uint32_t stime, etime;

#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
    pwron_case = APP_POWERON_CASE_NORMAL;
#else
    pwron_case = APP_POWERON_CASE_INVALID;

    stime = hal_sys_timer_get();
    osSignalWait(0x2, POWERON_PRESSMAXTIME_THRESHOLD_MS);
    etime = hal_sys_timer_get();
    TRACE("powon raw case:%d time:%d", pwron_case, TICKS_TO_MS(etime - stime));
#endif
    return pwron_case;

}

int system_shutdown(void);
int app_shutdown(void)
{
    system_shutdown();
    return 0;
}

int system_reset(void);
int app_reset(void)
{
    system_reset();
    return 0;
}

void app_bt_key_shutdown(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_reset();
#else
    app_shutdown();
#endif
}

void app_bt_key_enter_testmode(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s\n",__FUNCTION__);

//Modified by ATX : Leon.He_20171216, support dut with auto pairing
#if  defined (__AUTO_TWS_SEARCHING_WITH_PDL_EMPTY_)
    if(app_status_indication_get() == APP_STATUS_INDICATION_TWS_SEARCH){
    	stop_tws_searching();
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
        return;
    }
#endif

#if defined (__SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__) || defined (__SLAVE_AUTO_PAIRING_WITH_EMPTY_PDL_)
    if(app_status_indication_get() == APP_STATUS_INDICATION_TWS_PAIRING){
        app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
        osDelay(1000);
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
        return;
    }
#endif

#ifdef __ALWAYS_ALLOW_ENTER_TEST_MODE_
    app_factorymode_bt_signalingtest(status, param);
#else
    if(app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN){
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
        return;
    }
#endif
}

void app_bt_key_enter_nosignal_mode(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s\n",__FUNCTION__);
//Modified by ATX : Leon.He_20171216, support dut with auto pairing
#if defined (__AUTO_TWS_SEARCHING_WITH_PDL_EMPTY_)
    if(app_status_indication_get() == APP_STATUS_INDICATION_TWS_SEARCH){
    	stop_tws_searching();
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
        return;
    }
#endif

#if defined (__SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__) || defined (__SLAVE_AUTO_PAIRING_WITH_EMPTY_PDL_)
    if(app_status_indication_get() == APP_STATUS_INDICATION_TWS_PAIRING){
        app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
        osDelay(1000);
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_signalingtest(status, param);
#endif
        return;
    }
#endif

    if(app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN){
#ifdef __FACTORY_MODE_SUPPORT__
        app_factorymode_bt_nosignalingtest(status, param);
#endif
        return;
    }
}


void app_bt_singleclick(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
}

void app_bt_doubleclick(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
}

void app_power_off(APP_KEY_STATUS *status, void *param)
{
    TRACE("app_power_off\n");
}

extern "C" void OS_NotifyEvm(void);

#define PRESS_KEY_TO_ENTER_OTA_INTERVEL    (15000)          // press key 15s enter to ota
#define PRESS_KEY_TO_ENTER_OTA_REPEAT_CNT    ((PRESS_KEY_TO_ENTER_OTA_INTERVEL - 2000) / 500)
void app_otaMode_enter(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s",__func__);
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_ENTER_HIDE_BOOT);
//#ifdef __KMATE106__//Modified by ATX : Leon.He_20180302: enable UI prompt for OTA by default
    app_status_indication_set(APP_STATUS_INDICATION_OTA);
    app_voice_report(APP_STATUS_INDICATION_SHORT_1, 0);
    osDelay(1200);
//#endif
    hal_cmu_sys_reboot();
}
void app_ota_key_handler(APP_KEY_STATUS *status, void *param)
{
    static uint32_t time = hal_sys_timer_get();
    static uint16_t cnt = 0;

    TRACE("%s %d,%d",__func__, status->code, status->event);

    if (TICKS_TO_MS(hal_sys_timer_get() - time) > 600) // 600 = (repeat key intervel)500 + (margin)100
        cnt = 0;
    else
        cnt++;

    if (cnt == PRESS_KEY_TO_ENTER_OTA_REPEAT_CNT) {
        app_otaMode_enter(NULL, NULL);
    }

    time = hal_sys_timer_get();
}

//Modified by ATX : Leon.He_20180109: support reset PDL
void app_bt_key_handler_when_charging(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    switch(status->event)
    {
    case APP_KEY_EVENT_CLICK:
        TRACE("first blood!");
        break;
    case APP_KEY_EVENT_DOUBLECLICK:
        TRACE("double kill");
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_DURING_CHARGE_FOR_RESET_PDL_
		app_reset_paired_device_list();
#endif
        break;
    case APP_KEY_EVENT_TRIPLECLICK:
        TRACE("triple kill");
        break;
    case APP_KEY_EVENT_ULTRACLICK:
        TRACE("ultra kill");
        break;
    case APP_KEY_EVENT_RAMPAGECLICK:
        TRACE("rampage kill!you are crazy!");
#ifdef __FUNCTION_KEY_RAMPAGE_CLICK_DURING_CHARGE_FOR_OTA_
        app_otaMode_enter(NULL, NULL);
#endif
        break;
    case APP_KEY_EVENT_LONGLONGPRESS:
        TRACE("long_long_press!");
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_DURING_CHARGE_FOR_RESET_PHONE_PDL_
        app_reset_phone_paired_list_during_charge();
#endif
#ifdef __DOWN_KEY_LONG_PRESS_DURING_CHARGE_FOR_OTA_
		app_otaMode_enter(NULL, NULL);
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_DURING_CHARGE_FOR_NV_REBUILD_
		app_status_indication_set(APP_STATUS_INDICATION_CLEARSUCCEED);
		app_nvrecord_rebuild();
#endif
        break;
    }
}

void app_bt_key(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);
    switch(status->event)
    {
        case APP_KEY_EVENT_CLICK:
            TRACE("first blood!");
            break;
        case APP_KEY_EVENT_DOUBLECLICK:
            TRACE("double kill");
            break;
        case APP_KEY_EVENT_TRIPLECLICK:
            TRACE("triple kill");
            break;
        case APP_KEY_EVENT_ULTRACLICK:
            TRACE("ultra kill");
            break;
        case APP_KEY_EVENT_RAMPAGECLICK:
            TRACE("rampage kill!you are crazy!");
            break;
    }
#if defined( __FACTORY_MODE_SUPPORT__) && !defined(__TWS__)
    if (app_status_indication_get() == APP_STATUS_INDICATION_BOTHSCAN && (status->event == APP_KEY_EVENT_DOUBLECLICK)){
        app_factorymode_languageswitch_proc();
    }else
#endif
    {
#ifdef __COWIN_V2_BOARD_ANC_SINGLE_MODE__
   	if(!anc_single_mode_on)
#endif
        bt_key_send(status->code, status->event);
    }
}

//Modified by ATX : Leon.He_20171123, move key config to app_key_config.cpp
#if 0
#ifdef __POWERKEY_CTRL_ONOFF_ONLY__
#if defined(__APP_KEY_FN_STYLE_A__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_UP},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGPRESS},"bt play forward key",app_bt_key, NULL},
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif

};
#else //#elif defined(__APP_KEY_FN_STYLE_B__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_REPEAT},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_REPEAT},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_UP},"bt play forward key",app_bt_key, NULL},
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif

};
#endif
#else
#if defined(__APP_KEY_FN_STYLE_A__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_switch_player_role_key, NULL},
#else
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
#endif
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
#if RAMPAGECLICK_TEST_MODE
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key_enter_nosignal_mode, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
#endif
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt play forward key",app_bt_key, NULL},
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif
#ifdef __BT_ANC_TESTKEY__
     {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"anc test key",app_anc_key, NULL},
#endif
};
#else //#elif defined(__APP_KEY_FN_STYLE_B__)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key_enter_nosignal_mode, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_REPEAT},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_REPEAT},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt play forward key",app_bt_key, NULL},
#if HF_CUSTOM_FEATURE_SUPPORT & HF_CUSTOM_FEATURE_SIRI_REPORT
    {{APP_KEY_CODE_NONE ,APP_KEY_EVENT_NONE},"none function key",app_bt_key, NULL},
#endif
};
#endif
#endif

#endif//end of if 0

void app_key_init(void)
{
    uint8_t i = 0;
    TRACE("%s",__func__);
    for (i=0; i<(sizeof(app_key_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&app_key_handle_cfg[i]);
    }
}

void app_ble_reboot_test_key_handler(APP_KEY_STATUS *status, void *param)
{
    hal_sw_bootmode_set(HAL_SW_BOOTMODE_REBOOT_VERSION_CHECK);
    hal_cmu_sys_reboot();
}

void app_test_key_handler(APP_KEY_STATUS *status, void *param)
{
    app_factorymode_enter();
}

void app_key_init_on_charging(void)
{
    uint8_t i = 0;
	const APP_KEY_HANDLE  key_cfg[] = {
#ifdef __PWR_KEY_LONG_PRESS_DURING_CHARGE_FOR_OTA_
		{{APP_KEY_CODE_PWR,APP_KEY_EVENT_REPEAT},"ota function key", NULL},
#endif
#ifdef __FUNCTION_KEY_DOUBLE_CLICK_DURING_CHARGE_FOR_RESET_PDL_
        {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"function key when charging",app_bt_key_handler_when_charging, NULL},
#endif
#ifdef __FUNCTION_KEY_TRIPLE_CLICK_DURING_CHARGE_FOR_ENTER_TEST_MODE_
        {{APP_KEY_CODE_FN1,APP_KEY_EVENT_TRIPLECLICK},"factory test function key",app_test_key_handler, NULL},
#endif
#ifdef __FUNCTION_KEY_RAMPAGE_CLICK_DURING_CHARGE_FOR_OTA_
		{{APP_KEY_CODE_FN1,APP_KEY_EVENT_RAMPAGECLICK},"ota function key", app_bt_key_handler_when_charging,NULL},
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_DURING_CHARGE_FOR_NV_REBUILD_
		{{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"function key when charging",app_bt_key_handler_when_charging, NULL},
#endif
#ifdef __FUNCTION_KEY_LONG_LONG_PRESS_DURING_CHARGE_FOR_RESET_PHONE_PDL_
        {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"function key when charging",app_bt_key_handler_when_charging, NULL},
#endif
#ifdef __FUNCTION_KEY_SEVEN_CLICK_DURING_CHARGE_FOR_VERSION_CHECK_
        {{APP_KEY_CODE_FN1,APP_KEY_EVENT_SEVENCLICK},"version check key", app_ble_reboot_test_key_handler,NULL},
#endif
#ifdef __DOWN_KEY_LONG_PRESS_DURING_CHARGE_FOR_OTA_
		{{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGLONGPRESS},"ota function key",app_bt_key_handler_when_charging, NULL},
#endif
#if defined(__TOUCH_KEY__)&&defined(__PC_CMD_UART__)//Modified by ATX : Parker.Wei_20180408  
		{{APP_KEY_CODE_FN4,APP_KEY_EVENT_CLICK},"app_touch_key_handler_when_charging", app_touch_key_handler_when_charging,NULL},
#endif
#ifdef __FUNCTION_KEY_UP_DURING_CHARGE_FOR_OTA_
	 	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"function key when charging", app_otaMode_enter, NULL},
#endif

	};

    TRACE("%s",__func__);
    for (i=0; i<(sizeof(key_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&key_cfg[i]);
    }
}

#ifdef __AUTOPOWEROFF__


#ifdef __TWS__
extern  tws_dev_t  tws;
#endif       


void CloseEarphone(void)
{
    int activeCons;
    OS_LockStack();
    activeCons = MEC(activeCons);
    OS_UnlockStack();

#ifdef __BT_ANC__
	if(app_anc_work_status()) {
		app_poweroff_timer.timer_en = 1;
		app_poweroff_timer.timer_period = 30;
		return;
	}
#endif
#ifdef __TWS__
	//Modified by ATX : Leon.He_20180116: tws.mobile_sink.connected to replace tws.tws_source.connected, fix link loss then failed to auto power off
//    if(activeCons == 0 || ((tws.tws_source.connected == false) && (tws.tws_mode == TWSMASTER)))
      if(activeCons == 0 || ((activeCons==1) && (tws.mobile_sink.connected == false) && (tws.tws_mode == TWSMASTER)))
#else
    if(activeCons == 0)
#endif    
    {
      TRACE("!!!CloseEarphone\n");
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
      app_fake_poweroff();
#else
      app_shutdown();
#endif
    }
}
#endif /* __AUTOPOWEROFF__ */


BtStatus LinkDisconnectDirectly(void);
void a2dp_suspend_music_force(void);
extern uint8_t app_tws_auto_poweroff;
///uint8_t app_poweroff_flag=0;//Modified by ATX : Parke.Wei_20180328 redefine the variable in head of the file for upper function call;
static uint8_t app_fake_poweroff_flag=0;
//Modified by ATX : Leon.He_20180119: port app_deinit from 2000
int app_deinit(int deinit_case)
{
	int nRet = 0;
	TRACE("%s case:%d",__func__, deinit_case);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
	if (app_get_fake_poweroff_flag()==1)
	{
		app_set_fake_poweroff_flag(0);
		return nRet;
	}
#endif
#if defined(BTUSB_AUDIO_MODE)
	if(app_usbaudio_mode_on()) return 0;
#endif
	switch (pwron_case) {
		case APP_POWERON_CASE_CHARGING:
			break;
		case APP_POWERON_CASE_NORMAL:
		case APP_POWERON_CASE_DITHERING:
		case APP_POWERON_CASE_REBOOT:
		case APP_POWERON_CASE_ALARM:
		case APP_POWERON_CASE_CALIB:
		case APP_POWERON_CASE_BOTHSCAN:
		case APP_POWERON_CASE_FACTORY:
		case APP_POWERON_CASE_TEST:
		case APP_POWERON_CASE_INVALID:
		default:
			if (!deinit_case){
				app_poweroff_flag =1;
				app_status_indication_filter_set(APP_STATUS_INDICATION_BOTHSCAN);
				app_status_indication_set(APP_STATUS_INDICATION_POWEROFF);
				a2dp_suspend_music_force();
				osDelay(200);
#if FPGA==0
				nv_record_flash_flush();
#endif
				if(MEC(activeCons))
				{
					app_bt_disconnect_all();
					osDelay(1000);
				}
#ifdef __TWS__
				if(app_tws_auto_poweroff == 0){
					app_tws_disconnect_slave();
				}
#endif

#ifdef MEDIA_PLAYER_SUPPORT
				app_voice_report(APP_STATUS_INDICATION_POWEROFF, 0);
#endif
				osDelay(1000);
				af_close();
				app_poweroff_flag = 0;
			}
			break;
	}
	return nRet;
}

#ifdef APP_TEST_MODE
extern void app_test_init(void);
int app_init(void)
{
    int nRet = 0;
//    uint8_t pwron_case = APP_POWERON_CASE_INVALID;//Modified by ATX : Leon.He_20180120: remove duplicated define
    TRACE("%s",__func__);
    app_poweroff_flag = 0;

    apps_init_tid = osThreadGetId();
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_52M);
    list_init();
    af_open();
    app_os_init();
    app_pwl_open();
    app_audio_open();
    app_audio_manager_open();
    app_overlay_open();
    if (app_key_open(true))
    {
        nRet = -1;
        goto exit;
    }

    app_test_init();
exit:
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    return nRet;
}
#else
#define NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS (0xffffaa55)
int app_bt_connect2tester_init(void)
{
    BtDeviceRecord rec;
    BT_BD_ADDR tester_addr;
    uint8_t i;
    bool find_tester = false;
    struct nvrecord_env_t *nvrecord_env;
    btdevice_profile *btdevice_plf_p;
    nv_record_env_get(&nvrecord_env);

    if (nvrecord_env->factory_tester_status.status != NVRAM_ENV_FACTORY_TESTER_STATUS_DEFAULT)
        return 0;

    if (!nvrec_dev_get_dongleaddr(&tester_addr)){
        nv_record_open(section_usrdata_ddbrecord);
        for (i = 0; nv_record_enum_dev_records(i, &rec) == BT_STATUS_SUCCESS; i++) {
            if (!memcmp(rec.bdAddr.addr, tester_addr.addr, BD_ADDR_SIZE)){
                find_tester = true;
            }
        }
        if(i==0 && !find_tester){
            memset(&rec, 0, sizeof(BtDeviceRecord));
            memcpy(rec.bdAddr.addr, tester_addr.addr, BD_ADDR_SIZE);
            nv_record_add(section_usrdata_ddbrecord, &rec);
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(rec.bdAddr.addr);
            btdevice_plf_p->hfp_act = true;
            btdevice_plf_p->a2dp_act = true;
        }
        if (find_tester && i>2){
            nv_record_ddbrec_delete(&tester_addr);
            nvrecord_env->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS;
            nv_record_env_set(nvrecord_env);
        }
    }

    return 0;
}

int app_nvrecord_rebuild(void)
{
    struct nvrecord_env_t *nvrecord_env;
    nv_record_env_get(&nvrecord_env);

    nv_record_sector_clear();
    nv_record_open(section_usrdata_ddbrecord);
    nvrecord_env->factory_tester_status.status = NVRAM_ENV_FACTORY_TESTER_STATUS_TEST_PASS;
    nv_record_env_set(nvrecord_env);
    nv_record_flash_flush();

    return 0;
}

#ifdef __TWS__
extern void app_tws_start_reconnct(struct tws_mode_t *tws_mode);
extern uint16_t app_tws_delay_count;
#endif

void bt_change_to_jlink(APP_KEY_STATUS *status, void *param);
void bt_enable_tports(void);

#if defined(BTUSB_AUDIO_MODE)
#include "app_audio.h"
#include "usb_audio_frm_defs.h"
#include "usb_audio_test.h"

static bool app_usbaudio_mode = false;

extern "C" void btusbaudio_entry(void);
void app_usbaudio_entry(void)
{
	app_usbaudio_mode = true ;

    btusbaudio_entry();
}

bool app_usbaudio_mode_on(void)
{
	return app_usbaudio_mode;
}

void app_usb_key(APP_KEY_STATUS *status, void *param)
{
    TRACE("%s %d,%d",__func__, status->code, status->event);

}

const APP_KEY_HANDLE  app_usb_handle_cfg[] = {
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"USB HID FN1 UP key",app_usb_key, NULL},
	{{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"USB HID FN2 UP key",app_usb_key, NULL},
	{{APP_KEY_CODE_PWR,APP_KEY_EVENT_UP},"USB HID PWR UP key",app_usb_key, NULL},
};

void app_usb_key_init(void)
{
    uint8_t i = 0;
    TRACE("%s",__func__);
    for (i=0; i<(sizeof(app_usb_handle_cfg)/sizeof(APP_KEY_HANDLE)); i++){
        app_key_handle_registration(&app_usb_handle_cfg[i]);
    }
}


#endif
int app_init(void)
{
    int nRet = 0;
    struct nvrecord_env_t *nvrecord_env;
    bool need_check_key = true;
//    uint8_t pwron_case = APP_POWERON_CASE_INVALID;//Modified by ATX : Leon.He_20180120: remove duplicated define
    TRACE("app_init\n");
#ifdef __WATCHER_DOG_RESET__
    app_wdt_open(15);
#endif
#ifdef __BT_ANC__
	app_anc_ios_init();
#endif
    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_52M);
#if defined(MCU_HIGH_PERFORMANCE_MODE)
    TRACE("sys freq calc : %d\n", hal_sys_timer_calc_cpu_freq(0));
#endif
    apps_init_tid = osThreadGetId();
    list_init();
    app_os_init();
    app_status_indication_init();

    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_REBOOT_CRASHDUMP){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT_CRASHDUMP);
        TRACE("CRASHDUMP!!!");
        hal_trace_crashdump_dumptoflash();
    }

    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_REBOOT){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
        pwron_case = APP_POWERON_CASE_REBOOT;
        need_check_key = false;
        TRACE("REBOOT!!!");
    }
//Modified by ATX : cc_20180413  enter version check bootmode 
    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_REBOOT_VERSION_CHECK){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT_VERSION_CHECK);
        pwron_case = APP_POWERON_CASE_CHECK_VERSION;
        need_check_key = false;
        TRACE("BLE_REBOOT!!!");
    }
    
    if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_MODE){
        hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MODE);
        pwron_case = APP_POWERON_CASE_TEST;
        need_check_key = false;
        TRACE("TESTER!!!");
    }

    nRet = app_battery_open();
    TRACE("%d",nRet);
    if (nRet < 0){        
        nRet = -1;
        goto exit;
    }else if (nRet > 0 && 
//Modified by ATX : cc_20180413  enter version check bootmode 
        (pwron_case != APP_POWERON_CASE_TEST)&&(pwron_case != APP_POWERON_CASE_CHECK_VERSION)){
        pwron_case = APP_POWERON_CASE_CHARGING;
        app_status_indication_set(APP_STATUS_INDICATION_CHARGING);
        TRACE("CHARGING!!!");
        app_battery_start();
#if !defined(BTUSB_AUDIO_MODE)
        btdrv_start_bt();
        btdrv_hciopen();
        btdrv_sleep_config(1);
        btdrv_hcioff();
#endif
		app_key_open(false);

//@20180301 by parker.wei touch key is also valid in charging state
#if defined (__TOUCH_KEY__)&&defined(__PC_CMD_UART__)
		touch_init();
#endif

        app_key_init_on_charging();
        nRet = 0;
        goto exit;
    }else{
        nRet = 0;
    }


    if (app_key_open(need_check_key)){
        nRet = -1;
        goto exit;
    }

    hal_sw_bootmode_set(HAL_SW_BOOTMODE_REBOOT);

    app_bt_init();
#ifdef __TWS_RECONNECT_USE_BLE__
    app_tws_ble_reconnect_init();
    app_tws_delay_count=15;
#endif
    
    af_open();
    app_audio_open();
    app_audio_manager_open();
    app_overlay_open();

    nv_record_env_init();
    nvrec_dev_data_open();

#ifdef __SAVE_FW_VERSION_IN_NV_FLASH__
    nvrec_fw_version_init();
#endif
//    app_bt_connect2tester_init();
    nv_record_env_get(&nvrecord_env);
#ifdef __BT_ANC__	
	app_anc_open_module();
#endif
#ifdef MEDIA_PLAYER_SUPPORT
    app_play_audio_set_lang(nvrecord_env->media_language.language);
#endif
    app_bt_stream_volume_ptr_update(NULL);
    if (pwron_case == APP_POWERON_CASE_REBOOT){
        btdrv_start_bt();
        BesbtInit();
        osDelay(600);
        bt_drv_extra_config_after_init();
        TRACE("\n\n\nbt_stack_init_done:%d\n\n\n", pwron_case);
        app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
        app_bt_profile_connect_manager_opening_reconnect();
        app_key_init();
        app_battery_start();
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
        app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
    }
#ifdef __ENGINEER_MODE_SUPPORT__
    else if(pwron_case == APP_POWERON_CASE_TEST){
        app_status_indication_set(APP_STATUS_INDICATION_POWERON);
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_POWERON, 0);
#endif
#ifdef __WATCHER_DOG_RESET__
        app_wdt_close();
#endif
        TRACE("!!!!!ENGINEER_MODE!!!!!\n");
        nRet = 0;
        btdrv_start_bt();
        BesbtInit();
        osDelay(600);
        bt_drv_extra_config_after_init();
  //      app_nvrecord_rebuild();//Modified by ATX : cc_20180413  don't rebuild nvrecord when enter test mode
        if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_SIGNALINGMODE){
            hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MASK);
            app_factorymode_bt_signalingtest(NULL, NULL);
#ifdef __OPEN_AUDIO_LOOP_IN_DUT_MODE_
            app_audio_sendrequest((uint8_t)APP_FACTORYMODE_AUDIO_LOOP, (uint8_t)APP_BT_SETTING_OPEN, 0,0);
#endif
        }
        if (hal_sw_bootmode_get() & HAL_SW_BOOTMODE_TEST_NOSIGNALINGMODE){
            hal_sw_bootmode_clear(HAL_SW_BOOTMODE_TEST_MASK);
            app_factorymode_bt_nosignalingtest(NULL, NULL);
        }
        app_factorymode_key_init();
    }
//Modified by ATX : cc_20180413  enter version check bootmode 
    else if(pwron_case == APP_POWERON_CASE_CHECK_VERSION){
#ifdef __WATCHER_DOG_RESET__
        app_wdt_close();
#endif
        TRACE("!!!!!BLE_MODE!!!!!\n");
        btdrv_start_bt();
        BesbtInit();
        osDelay(600);
        bt_drv_extra_config_after_init();
        app_status_indication_set(APP_STATUS_INDICATION_VERSION_CHECK);
        app_check_version_ble_adv_start();
    }
#endif//end __ENGINEER_MODE_SUPPORT__
    else{
//Modified by ATX : Haorong.Wu_20190224 add atx factory mode for product line testing
#ifdef _ATX_FACTORY_MODE_DETECT_
        atx_factory_mode_check_init();
#endif
        app_status_indication_set(APP_STATUS_INDICATION_POWERON);
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_POWERON, 0);
#endif
        app_poweron_key_init();
        pwron_case = app_poweron_wait_case();
        {
            if (pwron_case != APP_POWERON_CASE_INVALID && pwron_case != APP_POWERON_CASE_DITHERING){
                TRACE("hello world i'm bes1000 hahaha case:%d\n", pwron_case);
                nRet = 0;
#ifndef __POWERKEY_CTRL_ONOFF_ONLY__
                app_status_indication_set(APP_STATUS_INDICATION_INITIAL);
#endif
                btdrv_start_bt();
                BesbtInit();
                osDelay(600);
                bt_drv_extra_config_after_init();
                TRACE("\n\n\nbt_stack_init_done:%d\n\n\n", pwron_case);
                *((volatile uint32_t *)0XD02201B0) &= ~(1 << 15);  ////ble first always
                switch (pwron_case) {
                    case APP_POWERON_CASE_CALIB:
                        break;
                    case APP_POWERON_CASE_BOTHSCAN:
                        app_status_indication_set(APP_STATUS_INDICATION_BOTHSCAN);
#ifdef MEDIA_PLAYER_SUPPORT
                        app_voice_report(APP_STATUS_INDICATION_BOTHSCAN,0);
#endif
#ifdef __EARPHONE__
                        app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
#if defined(__AUTOPOWEROFF__)
                        app_start_10_second_timer(APP_PAIR_TIMER_ID);
#endif
#endif
                        break;
                    case APP_POWERON_CASE_NORMAL:
#if defined( __EARPHONE__ ) && !defined(__EARPHONE_STAY_BOTH_SCAN__)
                        app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
#endif
                    case APP_POWERON_CASE_REBOOT:
                    case APP_POWERON_CASE_ALARM:
                    default:
//Modified by ATX : Haorong.Wu_20190224 add atx factory mode for product line testing
#ifdef _ATX_FACTORY_MODE_DETECT_
                        if(get_atx_factory_mode_flag()){
                            atx_factory_mode_proc();
                            TRACE("ATX_FACTORY_MODE_ON!! skip tws operation!!");
                            break;
                        }
#endif
                        app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);
#ifdef __TWS__

#ifndef __TWS_RECONNECT_USE_BLE__
                        if (nvrecord_env->tws_mode.mode == TWSMASTER){
                            BtStatus status;
                            BtDeviceRecord record;                  
                            app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
                            status = SEC_FindDeviceRecord((BT_BD_ADDR *)&nvrecord_env->tws_mode.record.bdAddr, &record);
                            if (status == BT_STATUS_SUCCESS){
                                app_tws_master_reconnect_slave(&record.bdAddr);
                            }else{
                                app_bt_profile_connect_manager_opening_reconnect();
                            }
                        }else if (nvrecord_env->tws_mode.mode == TWSSLAVE){
                            app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_NCON);
                        }else{
                            app_bt_profile_connect_manager_opening_reconnect();
                        }
#else
                        app_tws_start_reconnct(&nvrecord_env->tws_mode);
#endif

#else               
#if defined( __EARPHONE__) && defined(__BT_RECONNECT__)
#ifndef __APP_A2DP_SOURCE__
                        app_bt_profile_connect_manager_opening_reconnect();
#endif
#endif
#endif
                        break;
                }
#ifndef __POWERKEY_CTRL_ONOFF_ONLY__
                app_poweron_wait_finished();
#endif

#if defined (__TOUCH_KEY__)&&defined(__PC_CMD_UART__)
				touch_init();
#endif

                app_key_init();
                app_battery_start();
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
                app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
				player_role_thread_init();
				rb_ctl_init();
#endif
            }else{
                af_close();
                app_key_close();
                nRet = -1;
            }
        }
    }
exit:
    //bt_change_to_jlink(NULL,NULL);
    //bt_enable_tports();

#ifdef __BT_ANC__
	app_anc_set_init_done();
#endif
#if defined(BTUSB_AUDIO_MODE)
	if(pwron_case == APP_POWERON_CASE_CHARGING) {

#ifdef __WATCHER_DOG_RESET__
		app_wdt_close();
#endif
        af_open();
		app_key_handle_clear();
		app_usb_key_init();

		app_usbaudio_entry();

	}
#endif

    app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    return nRet;
}
#endif

extern bool app_bt_has_connectivitys(void);
void app_auto_power_off_timer(bool turnon)
{
//and no active connection
    if(turnon&&!app_bt_has_connectivitys())
        app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
    else
        app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
}

//Modified by ATX : 
bool app_clear_phone_paired_record(void)
{
    BtDeviceRecord record1;
	BtStatus ret;
    size_t len;
	uint8_t i;

    OS_LockStack();
	len=get_paired_device_nums();

    if(len<=0)
    {
        OS_UnlockStack();
		
        return true;
    }

	for(i=1;i<len+1;i++)
	{
		memset(&record1,0x0,sizeof(BtDeviceRecord));
	    ret = nv_record_enum_dev_records(len-i,&record1);
		if(BT_STATUS_SUCCESS != ret)
		{
		    TRACE("record read fail id[%d]\n",len-i);
			continue;
		}

		if(app_bt_has_profile_record(record1.bdAddr.addr)==true)
        {
            nv_record_ddbrec_delete(&record1.bdAddr);
            TRACE("Clear Pdl id[%d]\n",len-i);
        }
		else
		{
		    TRACE("TWS record  id[%d] not clear\n",len-i);
		}
	}

    OS_UnlockStack();
    return true;

}

//Modified by ATX : Leon.He_20180109: support reset PDL
void app_reset_paired_device_list(void)
{
    app_status_indication_set(APP_STATUS_INDICATION_CLEARSUCCEED);
    nv_record_sector_clear();
    DDB_Open(NULL);
    nv_record_env_init();
    nv_record_flash_flush();
//    osDelay(1000);
//    app_voice_report(APP_STATUS_INDICATION_LONG_DOUBLE_LOW,0);

}


#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
uint8_t app_get_fake_poweroff_flag(void)
{
	return app_fake_poweroff_flag;
}

void app_set_fake_poweroff_flag(uint8_t flag)
{
	app_fake_poweroff_flag = flag;
}

int app_poweron_from_fake_poweroff(void)
{
	pwron_case = APP_POWERON_CASE_NORMAL;
    hal_sw_bootmode_clear(HAL_SW_BOOTMODE_REBOOT);
    app_reset();
    return 0;
}

void app_fake_poweroff(void)
{
	struct nvrecord_env_t *nvrecord_env;
        if(app_poweroff_flag == 1)
            return;
	nv_record_env_get(&nvrecord_env);

	app_status_indication_set(APP_STATUS_INDICATION_POWEROFF);
	app_voice_report(APP_STATUS_INDICATION_POWEROFF, 0);
	if(MEC(activeCons))
	{
		app_bt_disconnect_all();
		osDelay(1000);
	}
#ifdef __TWS__
	if(app_tws_auto_poweroff == 0){
		app_tws_disconnect_slave();
	}
#endif
	if (nvrecord_env->tws_mode.mode == TWSSLAVE) {
		if (app_tws_ble_reconnect.adv_close_func)
			app_tws_ble_reconnect.adv_close_func(APP_TWS_CLOSE_BLE_ADV);
	}
    app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
	app_set_fake_poweroff_flag(1);
	app_stop_10_second_timer(APP_PAIR_TIMER_ID);
	app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
}
#endif
