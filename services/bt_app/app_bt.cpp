#include "cmsis_os.h"
#include "hal_trace.h"
#include "hal_aud.h"
#include "apps.h"
#include "app_thread.h"
#include "app_status_ind.h"

#include "nvrecord.h"

extern "C" {
#include "besbt_cfg.h"
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "sys/mei.h"
}
#include "btalloc.h"
#include "btapp.h"
#include "app_bt.h"
#include "bt_drv_interface.h"
#include "tgt_hardware.h"
#include "nvrecord_env.h"

#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#include "app_bt_media_manager.h"
#endif
#include "app_bt_func.h"
#include "app_dip.h"


#if defined(__EARPHONE_STAY_BCR_SLAVE__) && defined(__BT_ONE_BRING_TWO__)
#error can not defined at the same time.
#endif

#ifdef __TWS__
extern  tws_dev_t  tws;
#endif
#if DIP_DEVICE ==XA_ENABLED
#include "app_dip.h"
//BtRemoteDevice *DIP_test_remDev=NULL;
#endif

extern struct BT_DEVICE_T  app_bt_device;
U16 bt_accessory_feature_feature = HF_CUSTOM_FEATURE_SUPPORT;

#ifdef __BT_ONE_BRING_TWO__
BtDeviceRecord record2_copy;
uint8_t record2_avalible;
#endif
BtAccessibleMode gBT_DEFAULT_ACCESS_MODE = BAM_NOT_ACCESSIBLE;
uint8_t bt_access_mode_set_pending=0;

#ifdef __MOBILE_DISCONNECTED_NOT_AUTO_ENTER_PARING_ //Modified by ATX : Parke.Wei_20180328
bool	manual_enter_pair_mode=false;
#endif



#ifdef __DISCONNECT_TEST_
static uint8_t hf_event_disconnected_reason = 0;
#endif
//Modified by ATX : Leon.He_20180206: postpone to auto pairing when disconnected
static void app_postpone_to_auto_pairing_when_disconnected(void const *param)
{
	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR,0,0);
}

osTimerDef (APP_POSTPONE_TO_AUTO_PAIRING_WHEN_DISCONNECTED, app_postpone_to_auto_pairing_when_disconnected);
static osTimerId POSSIBLY_UNUSED app_postpone_to_auto_pairing_when_disconnected_timer = NULL;
#define APP_POSTPONE_TO_AUTO_PAIRING_WHEN_DISCONNECTED_TIMEOUT	5000
static void app_start_postpone_to_auto_pairing_when_disconnected_timer(void)
{
    TRACE("%s",__func__);
	if (app_postpone_to_auto_pairing_when_disconnected_timer == NULL)
		app_postpone_to_auto_pairing_when_disconnected_timer = osTimerCreate(osTimer(APP_POSTPONE_TO_AUTO_PAIRING_WHEN_DISCONNECTED), osTimerOnce, NULL);
	osTimerStop(app_postpone_to_auto_pairing_when_disconnected_timer);
	osTimerStart(app_postpone_to_auto_pairing_when_disconnected_timer, APP_POSTPONE_TO_AUTO_PAIRING_WHEN_DISCONNECTED_TIMEOUT);
}

static void app_cancel_postpone_to_auto_pairing_when_disconnected_timer(void)
{
    TRACE("%s",__func__);
	if (app_postpone_to_auto_pairing_when_disconnected_timer != NULL)
		osTimerStop(app_postpone_to_auto_pairing_when_disconnected_timer);
    osTimerDelete(app_postpone_to_auto_pairing_when_disconnected_timer);
    app_postpone_to_auto_pairing_when_disconnected_timer = NULL;
}

void app_bt_accessmode_set(BtAccessibleMode mode)
{
    const BtAccessModeInfo info = { BT_DEFAULT_INQ_SCAN_INTERVAL,
                                    BT_DEFAULT_INQ_SCAN_WINDOW,
                                    BT_DEFAULT_PAGE_SCAN_INTERVAL,
                                    BT_DEFAULT_PAGE_SCAN_WINDOW };
    BtStatus status;
//    OS_LockStack();
    gBT_DEFAULT_ACCESS_MODE = mode;
   // status =   ME_SetAccessibleMode(mode, &info);
   app_bt_ME_SetAccessibleMode(mode,&info);
    TRACE("app_bt_accessmode_set status=%x",status);
    if(status == BT_STATUS_IN_PROGRESS)
        bt_access_mode_set_pending = 1;
    else
        bt_access_mode_set_pending = 0;
 //   OS_UnlockStack();
}

void app_bt_accessmode_get(BtAccessibleMode *mode)
{
    OS_LockStack();
	*mode = gBT_DEFAULT_ACCESS_MODE;
    OS_UnlockStack();
}

#ifdef __TWS__
extern  tws_dev_t  tws;
#endif       
void PairingTransferToConnectable(void)
{
    int activeCons;
    OS_LockStack();
    activeCons = MEC(activeCons);
    OS_UnlockStack();

#ifdef __TWS__
    if(activeCons == 0 || ((activeCons==1) && (tws.mobile_sink.connected == false) && (tws.tws_mode ==TWSMASTER)))
#else
    if(activeCons == 0)
#endif       
    {
        TRACE("!!!PairingTransferToConnectable  BAM_CONNECTABLE_ONLY\n");
        app_bt_accessmode_set(BAM_CONNECTABLE_ONLY);
        app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);
    }
}


//Modified by ATX : Parke.Wei_20180418
void PairingTimeoutHandler(void)
{
    int activeCons;
	uint16_t pdl_sizes;
//    OS_LockStack();
//    activeCons = MEC(activeCons);
//    OS_UnlockStack();
	 pdl_sizes=	is_paired_devices_null();
	 TRACE("!!!pdl_sizes =%d\n",pdl_sizes);

#ifdef __POWER_OFF_AFTER_PAIR_TIMEOUT__
      TRACE("!!!power off after pairing timeout\n");  
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
      app_fake_poweroff();
#else
      app_shutdown();              
#endif
#else

		if(pdl_sizes==0)
		{
#ifdef __POWER_OFF_AFTER_PAIR_TIMEOUT_WHEN_PDL_NULL_
			 TRACE("!!!power off after pairing timeout when pdl null\n");  
			 app_shutdown();
#else
			TRACE("!!!PairingTransferToConnectable	BAM_CONNECTABLE_ONLY\n");
			app_bt_accessmode_set(BAM_CONNECTABLE_ONLY);
			app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);

#endif
		}else{

			TRACE("!!!PairingTransferToConnectable	BAM_CONNECTABLE_ONLY\n");
			app_bt_accessmode_set(BAM_CONNECTABLE_ONLY);
			app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);


		}
     


#endif //__POWER_OFF_AFTER_PAIR_TIMEOUT__
}



#define APP_BT_SWITCHROLE_LIMIT (10)

BtHandler  app_bt_handler;
#ifdef __TWS__
MeCommandToken app_name_token;
static char *incoming_device_name = NULL;
uint8_t app_tws_auto_poweroff=0;
#endif                    

static void app_bt_golbal_handle(const BtEvent *Event);

#ifdef __TWS_RECONNECT_USE_BLE__
extern void app_tws_create_acl_to_slave(BT_BD_ADDR *bdAddr);
extern void app_tws_slave_reconnect_master_start(BT_BD_ADDR *bdAddr);
static BtRemoteDevice *app_tws_remDev = NULL;

static void app_tws_reconnect_stop_music(void)
{
    if (app_bt_device.a2dp_streamming[0] == 1) { // playing music now.
         TRACE("%s bleReconnectPending = 1", __func__);
         app_tws_ble_reconnect.bleReconnectPending = 1; 
         tws_player_stop(BT_STREAM_SBC);
    }
}


void app_tws_slave_roleswitch_timeout(void)
{
    uint8_t status;
       TRACE("app_tws_slave_roleswitch_timeout dev=%x",app_tws_remDev);
    
    if (app_tws_remDev)
    {
       status =  ME_SwitchRole(app_tws_remDev);
       TRACE("ROLE SWITCH STATUS=%x",status);
    }    
}


void app_tws_slave_delay_to_roleSwitch(void)
{
    uint8_t status;
    TRACE("app_tws_slave_delay_to_roleSwitch dev=%x",app_tws_remDev);
    
    if (app_tws_remDev)
    {
         app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0, 0, (uint32_t)app_tws_slave_roleswitch_timeout);
  //     status =  ME_SwitchRole(app_tws_remDev);
 //      TRACE("ROLE SWITCH STATUS=%x",status);
    }
}

extern void notify_slave_to_master(void);
extern void notify_master_to_slave(void);
static void app_ble_reconnect_role_change_process(const BtEvent *Event)
{
    static uint8_t roleSwitch_cnt = 0;
	struct nvrecord_env_t *nvrecord_env;
    uint8_t status;
    switch (Event->eType) {
        case BTEVENT_LINK_DISCONNECT:
        case BTEVENT_ROLE_CHANGE:
        case BTEVENT_LINK_CONNECT_CNF:
        case BTEVENT_LINK_CONNECT_IND:            
            break;
        default:            
            return;
    }


    nv_record_env_get(&nvrecord_env);

    if (!((!memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,Event->p.remDev->bdAddr.addr,6)) && 
         (app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE) && (app_tws_ble_reconnect.isConnectedPhone))) {
        return;
    }

    switch (Event->eType) {
        case BTEVENT_LINK_DISCONNECT:            
            TRACE("%s BTEVENT_LINK_DISCONNECT errCode:0x%x role:%d",__func__, Event->errCode, Event->p.remDev->role);
            app_tws_remDev = NULL;
            roleSwitch_cnt = 0;
            break;
        case BTEVENT_ROLE_CHANGE:            
            TRACE("%s BTEVENT_ROLE_CHANGE errCode:0x%x role:%d",__func__, Event->errCode, Event->p.remDev->role);            
            switch (Event->p.roleChange.newRole) {
                case BCR_MASTER:
                    app_tws_reconnect_stop_music();
                    osTimerStop(app_tws_ble_reconnect.slave_delay_role_switch_timerId);
                    app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, (uint32_t)&nvrecord_env->tws_mode.record.bdAddr,0, (uint32_t)app_tws_slave_reconnect_master_start);
                    app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0, 0, (uint32_t)notify_slave_to_master);
                    break;
                case BCR_SLAVE:
                    status = Me_SetLinkPolicy(Event->p.remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
                    TRACE("SET LINKPOLICY STATUS=%x",status);
                    
                    app_tws_reconnect_stop_music();
                    roleSwitch_cnt++; 
                    //osDelay(500);
                    if (roleSwitch_cnt <= APP_BT_SWITCHROLE_LIMIT)
                    {
                      //  app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0, 0, (uint32_t)app_tws_slave_delay_to_roleSwitch);
                        osTimerStart(app_tws_ble_reconnect.slave_delay_role_switch_timerId, 100);
                    }
                        //
                    //ME_SwitchRole(Event->p.remDev);
                    break;
                case BCR_ANY:               
                case BCR_UNKNOWN:
                default:
                    break;
            }

            if (roleSwitch_cnt > APP_BT_SWITCHROLE_LIMIT) {
                osTimerStop(app_tws_ble_reconnect.slave_delay_role_switch_timerId);
                MeDisconnectLink(Event->p.remDev);		  
                roleSwitch_cnt = 0;
            }

            break;
        case BTEVENT_LINK_CONNECT_CNF:
            app_tws_remDev = Event->p.remDev;
            break;
        case BTEVENT_LINK_CONNECT_IND:            
            TRACE("%s LINK_CONNECT_CNF/LINK_CONNECT_IND errCode:%x role:%d\n",__func__, Event->errCode, Event->p.remDev->role);            
            app_tws_remDev = Event->p.remDev;
            if (Event->p.remDev->role == BCR_SLAVE ) {                
                roleSwitch_cnt = 0;
                status = ME_SwitchRole(Event->p.remDev);
                TRACE("role switch status = %x",status);
            } else if (Event->p.remDev->role == BCR_MASTER) {
                app_tws_reconnect_stop_music();
                osTimerStop(app_tws_ble_reconnect.slave_delay_role_switch_timerId);
                app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, (uint32_t)&nvrecord_env->tws_mode.record.bdAddr,0, (uint32_t)app_tws_slave_reconnect_master_start);
                app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0, 0, (uint32_t)notify_slave_to_master);
            }
            break;
        default:
            break;
    }
}
#endif

#if defined(__TWS__)

void app_tws_stop_sniff(void)
{
	A2dpStream *tws_source;    
    BtRemoteDevice *remDev;

    tws_source = app_tws_get_tws_source_stream();
    remDev = A2DP_GetRemoteDevice(tws_source);
    if (ME_GetRemDevState(remDev) == BDS_CONNECTED){
        if (ME_GetCurrentMode(remDev) == BLM_SNIFF_MODE){
            TRACE("!!! stop tws sniff currmode:%d\n", ME_GetCurrentMode(remDev));
            ME_StopSniff(remDev);
        }
    }

}
#endif

static BtRemoteDevice *app_earphone_remDev = NULL;
static void app_bt_earphone_role_process(const BtEvent *Event)
{
    static uint8_t switchrole_cnt = 0;  
    uint8_t status;
//    const BT_BD_ADDR BDADDR_NV_DEFAULT = {0,0,0,0,0,0};
	struct nvrecord_env_t *nvrecord_env;
    switch (Event->eType) {
        case BTEVENT_LINK_DISCONNECT:
        case BTEVENT_ROLE_CHANGE:
        case BTEVENT_LINK_CONNECT_CNF:
        case BTEVENT_LINK_CONNECT_IND:            
            break;
        default:            
            return;
    }
#if defined(__TWS__)
    nv_record_env_get(&nvrecord_env);
    //skip TWSSLAVE/TWSMASTER
    if (((nvrecord_env->tws_mode.mode == TWSSLAVE)||(nvrecord_env->tws_mode.mode == TWSMASTER)) &&
        (!memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,Event->p.remDev->bdAddr.addr,6))){
        return;
    }
#endif    
    //when tws connecting
    if((Event->p.remDev->bdAddr.addr[5] == bt_addr[5]) && 
       (Event->p.remDev->bdAddr.addr[4] == bt_addr[4]) &&
       (Event->p.remDev->bdAddr.addr[3] == bt_addr[3])){
        return;
    }
    //on phone connecting
    switch (Event->eType) {
        case BTEVENT_LINK_DISCONNECT:
            TRACE("%s BTEVENT_LINK_DISCONNECT errCode:0x%x role:%d",__func__, Event->errCode, Event->p.remDev->role);
            app_earphone_remDev = NULL;
            switchrole_cnt = 0;

#ifdef __TWS_RECONNECT_USE_BLE__
            app_tws_ble_reconnect.isConnectedPhone = 0;

      //      app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_NCON);
            
    //        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
            if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                if (app_tws_ble_reconnect.scan_func)
                    app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);

            } else if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                if (app_tws_ble_reconnect.adv_close_func)
                    app_tws_ble_reconnect.adv_close_func(APP_TWS_CLOSE_BLE_ADV);                
                if (app_tws_ble_reconnect.adv_func)
                    app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
            }
#endif

            break;
        case BTEVENT_ROLE_CHANGE:
            TRACE("%s BTEVENT_ROLE_CHANGE errCode:0x%x role:%d,newrole=%x",__func__, Event->errCode, Event->p.remDev->role,Event->p.roleChange.newRole);
            switch (Event->p.roleChange.newRole) {
                case BCR_MASTER:                    
                    if (++switchrole_cnt<=APP_BT_SWITCHROLE_LIMIT)
                    {
                      status =  ME_SwitchRole(Event->p.remDev);
                       TRACE("SET ME_SwitchRole STATUS=%x",status);

                    }
                    break;
                case BCR_SLAVE:                    
                    Me_SetLinkPolicy(Event->p.remDev,BLP_SNIFF_MODE);
                    break;
                case BCR_ANY:
                    break;                    
                case BCR_UNKNOWN:
                    break;
                default:
                    break;
            }
            break;
        case BTEVENT_LINK_CONNECT_CNF:
        case BTEVENT_LINK_CONNECT_IND:            
            TRACE("%s BTEVENT_LINK_CONNECT_CNF/BTEVENT_LINK_CONNECT_IND errCode:0x%x role:%d",__func__, Event->errCode, Event->p.remDev->role);
            app_earphone_remDev = Event->p.remDev;
            if(Event->errCode == 0){
                switchrole_cnt = 0;
                switch (Event->p.remDev->role) {
                    case BCR_MASTER:
                    case BCR_PMASTER:
                        status = Me_SetLinkPolicy(Event->p.remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
                        TRACE("SET LINKPOLICY STATUS=%x",status);
                        if (++switchrole_cnt<=APP_BT_SWITCHROLE_LIMIT)
                        {
                            status = ME_SwitchRole(Event->p.remDev);
                            TRACE("SET ME_SwitchRole STATUS=%x,Event->p.remDev=%x",status,Event->p.remDev);
                        }
                        break;
                    case BCR_SLAVE:      
                    case BCR_PSLAVE:          
                        Me_SetLinkPolicy(Event->p.remDev,BLP_SNIFF_MODE);
                        break;
                    case BCR_ANY:               
                    case BCR_UNKNOWN:
                    default:
                        break;
                }
#ifdef __TWS_RECONNECT_USE_BLE__
                app_tws_ble_reconnect.isConnectedPhone = 1;
                app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_2C,0,0);
                if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_NCON,0,0);
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
                } else if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                    if (app_tws_ble_reconnect.adv_close_func)
                        app_tws_ble_reconnect.adv_close_func(APP_TWS_CLOSE_BLE_ADV);                
                    if (app_tws_ble_reconnect.adv_func)
                        app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
                    app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_NCON,0,0);                
                    
                }
#endif
            }

            break;
        default:
           break;
        }
}


#ifdef __TWS_CALL_DUAL_CHANNEL__
extern uint8_t bt_addr[6];
#endif

#if DIP_DEVICE ==XA_ENABLED
extern int app_bt_dip_QuryService(BtRemoteDevice* rem);
#endif
static uint8_t peer_device_connecting_mobile=0;

static void app_bt_golbal_handle(const BtEvent *Event)
{
   // TRACE("app_bt_golbal_handle evt = %d",Event->eType);
#ifdef __TWS_RECONNECT_USE_BLE__
    BT_BD_ADDR slave_addr;
    struct tws_mode_t *tws_mode;
#endif

    CmgrHandler *cmgrHandler;
    BtSniffInfo   sniffInfo;                

#if defined(__TWS__) //&& defined(__BT_SNIFF__)
	struct nvrecord_env_t *nvrecord_env;
	A2dpStream *tws_source;    
    BtRemoteDevice *remDev;
#endif
    app_bt_earphone_role_process(Event);

#ifdef __TWS_RECONNECT_USE_BLE__
    app_ble_reconnect_role_change_process(Event);
#endif
    switch (Event->eType) {
        case BTEVENT_LINK_DISCONNECT:
            TRACE("BTEVENT_LINK_DISCONNECT eType=%d,reason=%x",Event->eType, Event->p.disconnect.remDev->discReason);
            btdrv_detach_reset_status();
#if defined(__TWS__)
#if 1
            if(TWSSLAVE==app_tws_get_mode())
            {
                TRACE("clean_for_disconnected");
                btdrv_clean_slave_afh();
            }
#endif
            TRACE("tws mode=%x",app_tws_get_mode());
            DUMP8("%02x ",app_tws_get_peer_bdaddr(),6);
            DUMP8("%02x ",Event->p.disconnect.remDev->bdAddr.addr,6);
            if(app_tws_get_mode()== TWSON || app_tws_get_mode() == TWSINVD)
            {
                if(!memcmp(app_tws_get_peer_bdaddr(),Event->p.disconnect.remDev->bdAddr.addr,6) &&  Event->p.disconnect.remDev->discReason==0x13)
                {
				//@20180705 by parker.wei Fixed single master has no voice when in call active
                 	app_tws_set_tws_conhdl(0);	
					
#if defined(_PROJ_2000IZ_C003__) && defined(__TWS_CHANNEL_RIGHT__)
  				set_peer_left_in_state(false);//@20180304 by parker.wei tws disconnected,the in-ear state clear;              
#endif

				
#ifdef __TWS_AUTO_POWER_OFF_WHEN_DISCONNECTED__
                    app_tws_auto_poweroff = 1;
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
                    app_fake_poweroff();
#else
#if  FPGA==0
                    app_shutdown();
#endif
#endif
                    return;
#endif
                }
            }
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
		if( app_get_fake_poweroff_flag())
			return;
#endif
#ifdef __TWS_RECONNECT_USE_BLE__
#if FPGA==0
            if ((app_system_status_get() != APP_SYSTEM_STATUS_POWEROFF_PROC) && (!memcmp(app_tws_get_peer_bdaddr(),Event->p.disconnect.remDev->bdAddr.addr,6))){
                nv_record_env_get(&nvrecord_env);
                tws_mode = &(nvrecord_env->tws_mode);

                if (tws_mode->mode == TWSSLAVE) {
                    if (app_tws_ble_reconnect.adv_func)
                        app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
                } else if (tws_mode->mode == TWSMASTER) {
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
                }
            }
#endif
#endif

#endif

#if 0//def __EARPHONE_STAY_BOTH_SCAN__//Modified by ATX : Leon.He_20171218: fix led state not match with state for tws
            if(MEC(activeCons) == 0)
            {


#ifdef __BT_ONE_BRING_TWO__
                if(app_bt_device.bt_open_recon_flag[BT_DEVICE_ID_2] == 0)
#endif
                {
                    app_status_indication_set(APP_STATUS_INDICATION_BOTHSCAN);
#if  FPGA==0
                    app_start_10_second_timer(APP_PAIR_TIMER_ID);
#endif
                }
            }
#endif

#if defined(__TWS__)
#if defined(__EARPHONE_STAY_BOTH_SCAN__)&& !defined(__MOBILE_DISCONNECTED_NOT_AUTO_ENTER_PARING_)
			nv_record_env_get(&nvrecord_env);
			if(nvrecord_env->tws_mode.mode == TWSSLAVE){
				app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
	            app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);
			}else if((nvrecord_env->tws_mode.mode == TWSMASTER && app_tws_get_mode() == TWSMASTER) && (MEC(activeCons) == 1)
					|| MEC(activeCons) == 0){
#ifdef __LINK_LOSE_TIMEOUT_THEN_ENTER_PAIRING__//Modified by ATX : Leon.He_20170317: reconnect only during link loss, enter pairing after timeout
#ifdef __DISCONNECT_TEST_ 
                        if((Event->p.disconnect.remDev->discReason !=0x8)||(hf_event_disconnected_reason !=0x8))
#else 
                        if(Event->p.disconnect.remDev->discReason !=0x8)
#endif
#endif

					app_start_postpone_to_auto_pairing_when_disconnected_timer();//Modified by ATX : Leon.He_20180206: postpone to auto pairing when disconnected


			}
#else
#ifdef __MOBILE_DISCONNECTED_NOT_AUTO_ENTER_PARING_ //Modified by ATX : Parke.Wei_20180328
		   if(manual_enter_pair_mode==true)  //when manual enter pair mode ,dont enter connectable again
			{
			   manual_enter_pair_mode=false;
			   break;
			}		   
			TRACE("Disconnect and go connectable\n");
		    if(app_bt_mobile_hasConnected()==false)
#else
            if(app_tws_get_mode() == TWSMASTER)
#endif
            {
                TRACE("STREAM close  MASTER\n");
                app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
            }            
#endif
#endif   

            break;
#ifdef __TWS__
        case BTEVENT_LINK_CONNECT_IND:
#if (DIP_DEVICE==XA_ENABLED)           
            app_bt_dip_QuryService(Event->p.remDev);
#endif    
			BtAccessibleMode mode;
			struct nvrecord_env_t *nvrecord_env;
			app_bt_accessmode_get(&mode);
			nv_record_env_get(&nvrecord_env);
            TRACE("GLOBAL HANDLE BTEVENT_LINK_CONNECT_IND remDev.role:%d\n",Event->p.remDev->role );
            if ( mode == BAM_CONNECTABLE_ONLY &&
            	(app_tws_get_mode() == TWSSLAVE)&&
            	(nvrecord_env->tws_mode.mode == TWSSLAVE)&&
            	(memcmp((void *)&Event->p.remDev->bdAddr, (void *)&nvrecord_env->tws_mode.record.bdAddr, BD_ADDR_SIZE))){
                TRACE("slave don't connect to mobile!disconnect");
                MeDisconnectLink(Event->p.remDev);		  
            }else{
                app_name_token.callback = app_bt_golbal_handle;
                app_name_token.p.name.io.in.psi.psRepMode =1;
                app_name_token.p.name.io.in.psi.psMode=0;
                memcpy(&app_name_token.p.name.bdAddr,Event->p.remDev->bdAddr.addr,sizeof(BT_BD_ADDR));
                ME_GetRemoteDeviceName(&app_name_token);
            }

            //skip TWSSLAVE/TWSMASTER

            break;            
        case BTEVENT_LINK_CONNECT_CNF:
#if DIP_DEVICE ==XA_ENABLED
            //DIP_test_remDev = Event->p.remDev;
            app_bt_dip_QuryService(Event->p.remDev);            
#endif    
            TRACE("%s BTEVENT_LINK_CONNECT_CNF errCode:0x%x role:%d",__func__, Event->errCode, Event->p.remDev->role);
            nv_record_env_get(&nvrecord_env);
            if(!memcmp((void *)&Event->p.remDev->bdAddr, (void *)&nvrecord_env->tws_mode.record.bdAddr, BD_ADDR_SIZE))
            {
                if(Event->errCode !=0)
                {
                    TRACE("TWS CONNECT FAIL");
                    peer_device_connecting_mobile = 0;
                    if(app_tws_ble_reconnect.bleReconnectPending==1)
                        app_tws_ble_reconnect.bleReconnectPending = 0;
                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);                    
                }
                else
                {       
                        uint8_t status;
                        TRACE("TWS CONNECT SUCC");
                        status = Me_SetLinkPolicy(Event->p.remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
                        TRACE("SET LINKPOLICY STATUS=%x",status);
                        memcpy(app_tws_get_peer_bdaddr(), Event->p.remDev->bdAddr.addr, sizeof(BT_BD_ADDR));
                        app_tws_set_have_peer(true);            
                        if(peer_device_connecting_mobile ==1)
                        {
#ifdef __TWS_CALL_DUAL_CHANNEL__
                            TRACE("SET SNIFFER ROLE");
                            DUMP8("%02x ",bt_addr,6);
                            Me_SetSnifferEnv(1,SNIFFER_ROLE,bt_addr,bt_addr);
                            peer_device_connecting_mobile = 0;
#endif
                        }
                }
            }
            break;
        case BTEVENT_MODE_CHANGE:
            TRACE("GLOBAL HANDLE BTEVENT_MODE_CHANGE remDev:%x mode:%d interval:%d\n",Event->p.modeChange.remDev, Event->p.modeChange.curMode, Event->p.modeChange.interval);
            break;            
        case BTEVENT_NAME_RESULT:
            nv_record_env_get(&nvrecord_env);
	     	incoming_device_name = (char *)(Event->p.meToken->p.name.io.out.name);
            TRACE("name result =%s\n",incoming_device_name);
            if (!strcmp(incoming_device_name,BT_LOCAL_NAME)  ){ //find peer device
                TRACE("set tws to slave");
                DUMP8("%02x ",Event->p.meToken->p.name.bdAddr.addr,6);
				memcpy(app_tws_get_peer_bdaddr(),&(Event->p.meToken->p.name.bdAddr.addr), sizeof(BT_BD_ADDR));
				app_tws_set_have_peer(true);

#ifdef __TWS_CALL_DUAL_CHANNEL__
                if(!(app_tws_get_mode() == TWSON && (nvrecord_env->tws_mode.mode == TWSSLAVE)))
                {
                    TRACE("SET SNIFFER ROLE");
                    DUMP8("%02x ",bt_addr,6);
#if 1                
                    btdrv_set_slave_afh();              
#endif                
                    Me_SetSnifferEnv(1,SNIFFER_ROLE,bt_addr,bt_addr);
                }
#endif
                /* Slave connected by Master*/
#ifdef __TWS_RECONNECT_USE_BLE__
                if (app_tws_ble_reconnect.adv_func)
                    app_tws_ble_reconnect.adv_func(APP_TWS_CLOSE_BLE_ADV);
#endif
			}

            break;
#ifdef __TWS_RECONNECT_USE_BLE__
            /* Master or Slave Receive ble adv from slave */
        case BTEVENT_TWS_BLE_ADV_REPORT_EVENT:
            nv_record_env_get(&nvrecord_env);
            TRACE("%s ::BTEVENT_TWS_BLE_ADV_REPORT_EVENT", __func__);
            memcpy(slave_addr.addr, Event->p.twsBleReport.data + 3, BD_ADDR_SIZE);
            DUMP8("%02x ", Event->p.twsBleReport.data, Event->p.twsBleReport.len);
            DUMP8("%02x ", slave_addr.addr, BD_ADDR_SIZE);
            peer_device_connecting_mobile == 0;
            //DUMP8("%02x ", (void *)&nvrecord_env->tws_mode.record.bdAddr, BD_ADDR_SIZE);
            if (0 == memcmp(slave_addr.addr, (void *)&nvrecord_env->tws_mode.record.bdAddr, BD_ADDR_SIZE)) {

                TRACE("slave isConnectedPhone:%x,curr twsmode=%x,local isConnectedPhone=%x",Event->p.twsBleReport.data[Event->p.twsBleReport.len - 2],app_tws_get_mode(), app_tws_ble_reconnect.isConnectedPhone);
                if(Event->p.twsBleReport.data[Event->p.twsBleReport.len - 2] == 0 || (Event->p.twsBleReport.data[Event->p.twsBleReport.len - 2] == 1 && app_tws_get_mode() !=TWSON))
                {
                    if (app_bt_device.a2dp_streamming[0] == 1) { // playing music now.
                        TRACE("%s bleReconnectPending = 1", __func__);
                        app_tws_ble_reconnect.bleReconnectPending = 1;
                        tws_player_stop(BT_STREAM_SBC);
                    } 


                    if (app_tws_ble_reconnect.scan_func)
                        app_tws_ble_reconnect.scan_func(APP_TWS_CLOSE_BLE_SCAN);

                    osTimerStop(app_tws_ble_reconnect.master_ble_scan_timerId);
                    if (Event->p.twsBleReport.data[Event->p.twsBleReport.len - 2] == 1 && app_tws_get_mode() !=TWSON) { // Slave Connected Phone.

                        // Master Create Acl connection with Slave.
                        app_tws_create_acl_to_slave(&slave_addr);

                        app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0, 0, (uint32_t)notify_master_to_slave);
                        peer_device_connecting_mobile  = 1;

                    } else if(Event->p.twsBleReport.data[Event->p.twsBleReport.len - 2] == 0){ // Slave no connected phone.
                        app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, (uint32_t)&nvrecord_env->tws_mode.record.bdAddr,0, (uint32_t)app_tws_master_reconnect_slave);
                    
                    }

  //                  app_tws_notipy();
                }

            }

            break;
#endif

#ifdef __BT_SNIFF__
        case BTEVENT_ACL_DATA_NOT_ACTIVE:            
            TRACE("BTEVENT_ACL_DATA_NOT_ACTIVE state:%d mode:%d\n", Event->p.remDev->state, Event->p.remDev->mode);
            tws_source = app_tws_get_tws_source_stream();
            remDev = A2DP_GetRemoteDevice(tws_source);
            if(Event->p.remDev == remDev){
                if (ME_GetRemDevState(remDev) == BDS_CONNECTED){
                    if (ME_GetCurrentMode(remDev) == BLM_SNIFF_MODE){
                        TRACE("!!!acl_not_act-->tws_source stop sniff currmode:%d\n", ME_GetCurrentMode(remDev));
                        ME_StopSniff(remDev);
                    }
                }
            }
            break;
#endif
		case BTEVENT_SCO_CONNECT_IND:
		case BTEVENT_SCO_CONNECT_CNF:
			Me_SetLinkPolicy(Event->p.scoConnect.remDev, BLP_DISABLE_ALL);
#ifdef __TWS__

#ifdef __TWS_RECONNECT_USE_BLE__
#ifndef __TWS_RECONNECT_IN_CALL_STATE_ //Modified by ATX : Parker.Wei_20180625 allow TWS reconnect when in call state;

            nv_record_env_get(&nvrecord_env);
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
            if (besbt_cfg.sniff){
                if (app_earphone_remDev == Event->p.remDev){
                    cmgrHandler = CMGR_GetFirstHandler(Event->p.remDev);
                     if (cmgrHandler)
                    {
                        CMGR_ClearSniffTimer(cmgrHandler);
                        CMGR_DisableSniffTimer(cmgrHandler);
                    }
                }
                
                if (app_tws_get_mode() == TWSMASTER){
                    tws_source = app_tws_get_tws_source_stream();
                    remDev = A2DP_GetRemoteDevice(tws_source);
                    if (remDev && ME_GetRemDevState(remDev) == BDS_CONNECTED){
                        if (ME_GetCurrentMode(remDev) == BLM_SNIFF_MODE){
                            TRACE("!!!sco-->tws_source stop sniff currmode:%d\n", ME_GetCurrentMode(remDev));
                            ME_StopSniff(remDev);
                        }
                        cmgrHandler = CMGR_GetFirstHandler(remDev);
                        if (cmgrHandler){
                            CMGR_ClearSniffTimer(cmgrHandler);
                            CMGR_DisableSniffTimer(cmgrHandler);
                        }
                    }
                }
            }
#endif
			break;
		case BTEVENT_SCO_DISCONNECT:
#ifdef __BT_ONE_BRING_TWO__
			Me_SetLinkPolicy(Event->p.scoConnect.remDev,  BLP_SNIFF_MODE);
#else			
			Me_SetLinkPolicy(Event->p.scoConnect.remDev,  BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
#endif
#ifdef __TWS__

#ifdef __TWS_RECONNECT_USE_BLE__
            nv_record_env_get(&nvrecord_env);
            if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER)) {
                if (app_tws_ble_reconnect.scan_func)
                    app_tws_ble_reconnect.scan_func(APP_TWS_OPEN_BLE_SCAN);
            }

            if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE)) {
                if (app_tws_ble_reconnect.adv_func)
                    app_tws_ble_reconnect.adv_func(APP_TWS_OPEN_BLE_ADV);
            }
#endif

            if (besbt_cfg.sniff){                
                if (app_earphone_remDev == Event->p.remDev){
                    /* Start the sniff timer */
                    sniffInfo.minInterval = CMGR_SNIFF_MIN_INTERVAL;
                    sniffInfo.maxInterval = CMGR_SNIFF_MAX_INTERVAL;
                    sniffInfo.attempt = CMGR_SNIFF_ATTEMPT;
                    sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                    cmgrHandler = CMGR_GetFirstHandler(Event->p.remDev);
                    if (cmgrHandler){
                        CMGR_SetSniffTimer(cmgrHandler, &sniffInfo, CMGR_SNIFF_TIMER);
                    }
                }
                
                if (app_tws_get_mode() == TWSMASTER){
                    tws_source = app_tws_get_tws_source_stream();
                    remDev = A2DP_GetRemoteDevice(tws_source);
                    if (ME_GetRemDevState(remDev) == BDS_CONNECTED){
                        /* Start the sniff timer */
                        sniffInfo.minInterval = CMGR_SNIFF_MIN_INTERVAL-40;
                        sniffInfo.maxInterval = CMGR_SNIFF_MAX_INTERVAL-40;
                        sniffInfo.attempt = CMGR_SNIFF_ATTEMPT;
                        sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                        cmgrHandler = CMGR_GetFirstHandler(remDev);
                        if (cmgrHandler){                        
                            CMGR_SetSniffTimer(cmgrHandler, &sniffInfo, CMGR_SNIFF_TIMER);
                        }
                    }
                }
            }
#endif            
			break;

#endif     
		case BTEVENT_ACL_DATA_ACTIVE:
#if 1            
#ifdef __TWS__
            if (besbt_cfg.sniff){
                if (app_earphone_remDev == Event->p.remDev){
                    cmgrHandler = CMGR_GetFirstHandler(Event->p.remDev);
                    if (app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_CON){
                        if (cmgrHandler){                        
                            CMGR_ClearSniffTimer(cmgrHandler);
                            CMGR_DisableSniffTimer(cmgrHandler);
                        }
                    }else{
                        /* Start the sniff timer */
                        sniffInfo.minInterval = CMGR_SNIFF_MIN_INTERVAL;
                        sniffInfo.maxInterval = CMGR_SNIFF_MAX_INTERVAL;
                        sniffInfo.attempt = CMGR_SNIFF_ATTEMPT;
                        sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                        if (cmgrHandler){                        
                            CMGR_SetSniffTimer(cmgrHandler, &sniffInfo, CMGR_SNIFF_TIMER);
                        }
                    }
                }
                
                if (app_tws_get_mode() == TWSMASTER){                    
                    tws_source = app_tws_get_tws_source_stream();
                    remDev = A2DP_GetRemoteDevice(tws_source);
                    if (ME_GetRemDevState(remDev) == BDS_CONNECTED){                     
                        cmgrHandler = CMGR_GetFirstHandler(remDev);
                        switch (app_bt_device.hf_audio_state[BT_DEVICE_ID_1]) {
                            case HF_AUDIO_CON:
                                if (cmgrHandler){                                    
                                    CMGR_ClearSniffTimer(cmgrHandler);
                                    CMGR_DisableSniffTimer(cmgrHandler);
            					}
                                break;
                            case HF_AUDIO_DISCON:
                                /* Start the sniff timer */
                                sniffInfo.minInterval = CMGR_SNIFF_MIN_INTERVAL-40;
                                sniffInfo.maxInterval = CMGR_SNIFF_MAX_INTERVAL-40;
                                sniffInfo.attempt = CMGR_SNIFF_ATTEMPT;
                                sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                                if (cmgrHandler){
                                    CMGR_SetSniffTimer(cmgrHandler, &sniffInfo, CMGR_SNIFF_TIMER);
            					}
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
#else            
            if (besbt_cfg.sniff){
                /* Start the sniff timer */
                BtSniffInfo   sniffInfo;                
                CmgrHandler    *cmgrHandler;
                sniffInfo.minInterval = CMGR_SNIFF_MIN_INTERVAL;
                sniffInfo.maxInterval = CMGR_SNIFF_MAX_INTERVAL;
                sniffInfo.attempt = CMGR_SNIFF_ATTEMPT;
                sniffInfo.timeout = CMGR_SNIFF_TIMEOUT;
                cmgrHandler = CMGR_GetFirstHandler(Event->p.remDev);
                if (cmgrHandler)
                        CMGR_SetSniffTimer(cmgrHandler, &sniffInfo, CMGR_SNIFF_TIMER);
            }
#endif
#endif
            break;
        case BTEVENT_ACCESSIBLE_CHANGE:
            TRACE("ACCESSIBLE_CHANGE evt:%d errCode:0x%0x aMode=0x%0x",Event->eType, Event->errCode,Event->p.aMode);
            if(bt_access_mode_set_pending ==1){
                TRACE("BEM_ACCESSIBLE_CHANGE PENDING");
                bt_access_mode_set_pending = 0;
                app_bt_accessmode_set(gBT_DEFAULT_ACCESS_MODE);
            }
            break;

        }
}

void app_bt_snifftimeout_handler(EvmTimer * timer, BOOL *skipInternalHandler)
{   
    CmgrHandler *handler = (CmgrHandler *)timer->context;
    if ((app_bt_device.hf_audio_state[BT_DEVICE_ID_1] == HF_AUDIO_CON)||
        (app_bt_device.a2dp_streamming[BT_DEVICE_ID_1])){        
        TRACE("%s restart sniff timer", __func__);
        *skipInternalHandler = TRUE;        
        EVM_StartTimer(timer, handler->sniffTimeout);        
    }else{
        TRACE("%s use internal sniff timer", __func__);
        *skipInternalHandler = FALSE;        
    }
}
#if DIP_DEVICE ==XA_ENABLED

const DipPnpInfo* PnpInfo;

//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
static void set_a2dp_to_aac_limited(void)
{
	if (false == app_bt_device.app_aac_limited)
		return;
	A2DP_Deregister(&app_bt_device.a2dp_aac_stream);
	a2dp_aac_avdtpcodec.elements = (U8*) (&a2dp_codec_aac_elements_limited);

#ifdef __A2DP_AVDTP_CP__
    a2dp_avdtpCp[0].cpType = AVDTP_CP_TYPE_SCMS_T;
    a2dp_avdtpCp[0].data = (U8 *)&a2dp_avdtpCp_securityData;
    a2dp_avdtpCp[0].dataLen = 1;
    A2DP_Register(&app_bt_device.a2dp_aac_stream, &a2dp_aac_avdtpcodec, &a2dp_avdtpCp[0], a2dp_callback);
    A2DP_AddContentProtection(&app_bt_device.a2dp_aac_stream, &a2dp_avdtpCp[0]);
#else
	A2DP_Register(&app_bt_device.a2dp_aac_stream, &a2dp_aac_avdtpcodec, NULL, a2dp_callback);
#endif

}

//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
static bool is_iphone6_series(const DipPnpInfo* info)
{
	uint16_t pid = (uint16_t) (info->PID & IPHONE_PID_MASK);
	return SRT_BT_PID_IPHONE6 == pid || SRT_BT_PID_IPHONE6S == pid;
}

//Modified by ATX : Parker.Wei_20180719: 
static bool misc_phone_series(const DipPnpInfo* info)
{
	if(SRT_BT_PID_MISC_ONE == info->PID)
		return true;
	if(SRT_BT_PID_MISC_TWO == info->PID)
		return true;

	return false;

}


void getRemotePhoneInfo(const DipPnpInfo* info)
{
//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
#ifdef _ANDROID_AAC_DIFFRENT_BITRATE_
	app_bt_device.app_aac_limited = true;
#else
    app_bt_device.app_aac_limited = false;
#endif
	app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_NOT_SURE;

    switch (info->VIDSource)
    {
        case SRC_BT:
        {
            switch (info->VID)
            {
                case SRC_BT_SANSUMG:
                    app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_SAMSUNG;
                    TRACE("GET sumsung phone");
                    break;
                case SRC_BT_APPLE:
#ifdef _ANDROID_AAC_DIFFRENT_BITRATE_
                    app_bt_device.app_aac_limited = false;
#endif
                    app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_IPHONE;
//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
#ifdef _IPHONE_6_SERIES_AAC_DIFFRENT_BITRATE_
                    if (is_iphone6_series(info))
                    {
						app_bt_device.app_aac_limited = true;
						TRACE("get apple phone 6 series");
                    }
                    else
#endif
                    TRACE("get apple phone");
                break;	          
				case SRC_BT_MEIZU:
					if(SRT_BT_PID_MEIZU == info->PID)
					{
						app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_MEIZU;
						TRACE("get meizu phones with AAC");
					}
				break;              
				case SRC_BT_MISC_ONE:
				case SRC_BT_MISC_TWO:
				case SRC_BT_MISC_THREE:
                     if(misc_phone_series(info))
                     {
						 app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_MISC;
						 TRACE("get misc phones with AAC");

					 }
				break;
                default:
                   app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_NOT_SURE;
                   TRACE("don't sure phone");                   
                break;
            }
        }
        break;

        case SRC_USB:
        {
            switch (info->VID)
            {
                case SRC_USB_APPLE:
                    app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_IPHONE;
#ifdef _ANDROID_AAC_DIFFRENT_BITRATE_
                    app_bt_device.app_aac_limited = false;
#endif
                    TRACE("get apple phone");
                break;
//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
			case SRC_BT_BRCM:
					app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_BRCM_CHIP_SERIES;
					TRACE("get BRCM chip series phone, such as Pixel");
                break;
                default:
                    app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_NOT_SURE;
                    TRACE("don't sure phone");
                break;
             }
        }
        break;
        default:
		   app_bt_device.app_vid_of_phone=APP_VID_IN_AAC_WHITE_LIST_NOT_SURE;
           TRACE("not test this case yet.it may get PNP error"); 
        break;
    }

	set_a2dp_to_aac_limited();//Modified by ATX : Leon.He_20180625: add app_acc_limited flag
}
#endif
void app_bt_golbal_handle_init(void)
{
#if DIP_DEVICE ==XA_ENABLED
    app_DIP_init(getRemotePhoneInfo);
#endif
    BtEventMask mask = BEM_NO_EVENTS;
    ME_InitHandler(&app_bt_handler);
    app_bt_handler.callback = app_bt_golbal_handle;
    ME_RegisterGlobalHandler(&app_bt_handler);
    mask |= BEM_ROLE_CHANGE | BEM_SCO_CONNECT_CNF | BEM_SCO_DISCONNECT | BEM_SCO_CONNECT_IND;
#ifdef __EARPHONE_STAY_BOTH_SCAN__
    mask |= BEM_LINK_DISCONNECT;
#endif
#ifdef __EARPHONE_STAY_BCR_SLAVE__
    mask |= BEM_LINK_CONNECT_CNF;
    ME_SetConnectionRole(BCR_ANY);
#endif

#ifdef __TWS__
    mask |= BEM_LINK_CONNECT_IND  | BEM_LINK_DISCONNECT | BEM_MODE_CHANGE;
#endif
    mask |= BEM_ACCESSIBLE_CHANGE;
    ME_SetEventMask(&app_bt_handler, mask);

    ME_SetConnectionRole(BCR_ANY);
    for (uint8_t i=0; i<BT_DEVICE_NUM; i++){
        A2DP_SetMasterRole(&app_bt_device.a2dp_stream[i], FALSE);
        HF_SetMasterRole(&app_bt_device.hf_channel[i], FALSE);
    }
#ifdef __TWS__
    CMGR_SetSniffTimeoutHandlerExt(app_bt_snifftimeout_handler);
#endif
}

void app_bt_send_request(uint32_t message_id, uint32_t param0, uint32_t param1, uint32_t ptr)
{
    APP_MESSAGE_BLOCK msg;
    
    TRACE("app_bt_send_request: %d\n", message_id);
    msg.mod_id = APP_MODUAL_BT;
    msg.msg_body.message_id = message_id;
    msg.msg_body.message_Param0 = param0;
    msg.msg_body.message_Param1 = param1;
    msg.msg_body.message_ptr = ptr;
    app_mailbox_put(&msg);
}

//Modified by ATX : Leon.He_20171216, support auto pairing
bool auto_tws_searching(void)
{
    TRACE("%s enter",__func__);
    if (is_find_tws_peer_device_onprocess() == false)
    {
        find_tws_peer_device_start();
		app_voice_report(APP_STATUS_INDICATION_TWS_SEARCH,0);
        app_status_indication_set(APP_STATUS_INDICATION_TWS_SEARCH);
    }
    return true;
}

bool stop_tws_searching(void)
{
    TRACE("%s enter",__func__);
    if (is_find_tws_peer_device_onprocess() == true)
    	find_tws_peer_device_stop();
    return true;
}

static int app_bt_handle_process(APP_MESSAGE_BODY *msg_body)
{
    TRACE("app_bt_handle_process: %d\n", msg_body->message_id);

    switch (msg_body->message_id) {
        case APP_BT_REQ_ACCESS_MODE_SET:
            if (msg_body->message_Param0 == BT_DEFAULT_ACCESS_MODE_PAIR){
                if (msg_body->message_Param1 !=2)//Modified by ATX : Leon.He_20171123
                {
        			BtAccessibleMode mode;
        			app_bt_accessmode_get(&mode);
        			if(mode == BT_DEFAULT_ACCESS_MODE_PAIR)//@20180108 by parker.wei when been BOTHSCAN dont enter pairing
        				break;
					app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_PAIR);
	                app_voice_report(APP_STATUS_INDICATION_BOTHSCAN, 0);
					app_status_indication_set(APP_STATUS_INDICATION_BOTHSCAN);
					app_start_10_second_timer(APP_PAIR_TIMER_ID);
                }
                else if (msg_body->message_Param1 == 2)//Modified by ATX : Leon.He_20171130: for tws searching
                {
                	app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
                	auto_tws_searching();
                }
            }
//Modified by ATX : Leon.He_20171123
            else if (msg_body->message_Param0 == BT_DEFAULT_ACCESS_MODE_TWS_PAIR){
                app_status_indication_set(APP_STATUS_INDICATION_TWS_PAIRING);
                app_voice_report(APP_STATUS_INDICATION_TWS_PAIRING, 0);
                app_bt_accessmode_set(BT_DEFAULT_ACCESS_MODE_TWS_PAIR);
                app_start_10_second_timer(APP_PAIR_TIMER_ID);
            }
            else if (msg_body->message_Param0 == BAM_CONNECTABLE_ONLY){
                app_bt_accessmode_set(BAM_CONNECTABLE_ONLY);	
				app_status_indication_set(APP_STATUS_INDICATION_PAGESCAN);//Modified by ATX : Parke.Wei_20180328
            }else if (msg_body->message_Param0 == BAM_NOT_ACCESSIBLE){
                app_bt_accessmode_set(BAM_NOT_ACCESSIBLE);
            }
            break;
		case APP_BT_REQ_CUSTOMER_CALL:
			if (msg_body->message_ptr){
				//((APP_BT_REQ_CUSTOMER_CALl_CB_T)(msg_body->message_ptr))((void *)msg_body->message_Param0, (void *)msg_body->message_Param1);
				app_bt_generic_req(msg_body->message_Param0,msg_body->message_Param1,msg_body->message_ptr);
			}
			break;
         default:
            break;
    }

    return 0;
}

void *app_bt_profile_active_store_ptr_get(uint8_t *bdAddr)
{
    static btdevice_profile device_profile = {false, false, false,0};
    btdevice_profile *ptr;
    nvrec_btdevicerecord *record = NULL;

#if  FPGA==0
    if (!nv_record_btdevicerecord_find((BT_BD_ADDR *)bdAddr,&record)){
        ptr = &(record->device_plf);
        DUMP8("0x%02x ", bdAddr, BD_ADDR_SIZE);
        TRACE("%s hfp_act:%d hsp_act:%d a2dp_act:0x%x,codec_type=%x", __func__, ptr->hfp_act, ptr->hsp_act, ptr->a2dp_act,ptr->a2dp_codectype);
    }else
#endif
    {
        ptr = &device_profile;
        TRACE("%s default", __func__);
    }
    return (void *)ptr;
}

enum bt_profile_reconnect_mode{
    bt_profile_reconnect_null,
    bt_profile_reconnect_openreconnecting,
    bt_profile_reconnect_reconnecting,
};

enum bt_profile_connect_status{
    bt_profile_connect_status_unknow,
    bt_profile_connect_status_success,
    bt_profile_connect_status_failure,
};

struct app_bt_profile_manager{
    bool has_connected;
    enum bt_profile_connect_status hfp_connect;
    enum bt_profile_connect_status hsp_connect;
    enum bt_profile_connect_status a2dp_connect;
    BT_BD_ADDR rmt_addr;
    bt_profile_reconnect_mode reconnect_mode;
    A2dpStream *stream;
    HfChannel *chan;
    uint16_t reconnect_cnt;    
    osTimerId connect_timer;    
    void (* connect_timer_cb)(void const *);
};

#ifdef _PROJ_2000IZ_C001__
//reconnect = (INTERVAL+PAGETO)*CNT = (3000ms+5000ms)*36 = 388s, near to 5mins
#define APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT (36)
#endif

#ifdef _PROJ_2000IZ_C005__
//reconnect = (INTERVAL+PAGETO)*CNT = (3000ms+5000ms)*36 = 388s, near to 5mins
#define APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT (36)
#endif

//reconnect = (INTERVAL+PAGETO)*CNT = (3000ms+5000ms)*15 = 120s
#define APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS (3000)
#ifndef APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT
#define APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT (15)
#endif
#define APP_BT_PROFILE_CONNECT_RETRY_MS (10000)

static struct app_bt_profile_manager bt_profile_manager[BT_DEVICE_NUM];
static void app_bt_profile_reconnect_timehandler(void const *param);
 
osTimerDef (BT_PROFILE_CONNECT_TIMER0, app_bt_profile_reconnect_timehandler);                      // define timers
#ifdef __BT_ONE_BRING_TWO__
osTimerDef (BT_PROFILE_CONNECT_TIMER1, app_bt_profile_reconnect_timehandler);
#endif

#ifdef __AUTO_CONNECT_OTHER_PROFILE__
static void app_bt_profile_connect_hf_retry_timehandler(void const *param)
{
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;    
    HF_CreateServiceLink(bt_profile_manager_p->chan, &bt_profile_manager_p->rmt_addr);
}


#endif

static void app_bt_profile_connect_a2dp_retry_timehandler(void const *param)
{
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;    
    A2DP_OpenStream(bt_profile_manager_p->stream, &bt_profile_manager_p->rmt_addr);
}

static void app_bt_profile_reconnect_timehandler(void const *param)
{
    struct app_bt_profile_manager *bt_profile_manager_p = (struct app_bt_profile_manager *)param;
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bt_profile_manager_p->rmt_addr.addr);
    if (bt_profile_manager_p->connect_timer_cb){
        bt_profile_manager_p->connect_timer_cb(param);
        bt_profile_manager_p->connect_timer_cb = NULL;
    }else{
        if (btdevice_plf_p->hfp_act){
            TRACE("try connect hf");
            HF_CreateServiceLink(bt_profile_manager_p->chan, &bt_profile_manager_p->rmt_addr);
        }else if(btdevice_plf_p->a2dp_act){
            TRACE("try connect a2dp");
            A2DP_OpenStream(bt_profile_manager_p->stream, &bt_profile_manager_p->rmt_addr);
        }
    }
}

void app_bt_profile_connect_manager_open(void)
{
    uint8_t i=0;
    for (i=0;i<BT_DEVICE_NUM;i++){
        bt_profile_manager[i].has_connected = false;
        bt_profile_manager[i].hfp_connect = bt_profile_connect_status_unknow;
        bt_profile_manager[i].hsp_connect = bt_profile_connect_status_unknow;
        bt_profile_manager[i].a2dp_connect = bt_profile_connect_status_unknow;
        memset(bt_profile_manager[i].rmt_addr.addr,0 ,0);
        bt_profile_manager[i].reconnect_mode = bt_profile_reconnect_null;
        bt_profile_manager[i].stream = NULL;
        bt_profile_manager[i].chan = NULL;
        bt_profile_manager[i].reconnect_cnt = 0;
        bt_profile_manager[i].connect_timer_cb = NULL;
    }
    
    bt_profile_manager[BT_DEVICE_ID_1].connect_timer = osTimerCreate (osTimer(BT_PROFILE_CONNECT_TIMER0), osTimerOnce, &bt_profile_manager[BT_DEVICE_ID_1]);
#ifdef __BT_ONE_BRING_TWO__        
    bt_profile_manager[BT_DEVICE_ID_2].connect_timer = osTimerCreate (osTimer(BT_PROFILE_CONNECT_TIMER1), osTimerOnce, &bt_profile_manager[BT_DEVICE_ID_2]);
#endif
}

BOOL app_bt_profile_connect_openreconnecting(void *ptr)
{
    bool nRet = false;
    uint8_t i;

    for (i=0;i<BT_DEVICE_NUM;i++){
        nRet |= bt_profile_manager[i].reconnect_mode == bt_profile_reconnect_openreconnecting ? true : false;
    }

    return nRet;
}

void app_bt_profile_connect_manager_opening_reconnect(void)
{
    int ret;
    BtDeviceRecord record1;
    BtDeviceRecord record2;
    struct nvrecord_env_t *nvrecord_env;
    btdevice_profile *btdevice_plf_p;
    int find_invalid_record_cnt;

    OS_LockStack();
    nv_record_env_get(&nvrecord_env);

    do{
        find_invalid_record_cnt = 0;
        ret = nv_record_enum_latest_two_paired_dev(&record1,&record2);
#ifdef __TWS__
        
        if(ret == 1){
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(record1.bdAddr.addr);
            if (!(btdevice_plf_p->hfp_act)&&
                !(btdevice_plf_p->a2dp_act)&&
                memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record1.bdAddr.addr,BD_ADDR_SIZE)){
                nv_record_ddbrec_delete(&record1.bdAddr);
                find_invalid_record_cnt++;
            }
        }else if(ret == 2){
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(record1.bdAddr.addr);
            if (!(btdevice_plf_p->hfp_act)&&
                !(btdevice_plf_p->a2dp_act)&&
                memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record1.bdAddr.addr,BD_ADDR_SIZE)){
                nv_record_ddbrec_delete(&record1.bdAddr);
                find_invalid_record_cnt++;
            }
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(record2.bdAddr.addr);
            if (!(btdevice_plf_p->hfp_act)&&
                !(btdevice_plf_p->a2dp_act)&&
                memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record2.bdAddr.addr,BD_ADDR_SIZE)){
                nv_record_ddbrec_delete(&record2.bdAddr);
                find_invalid_record_cnt++;
            }
        }
#endif

        
    }while(find_invalid_record_cnt);

    TRACE("!!!app_bt_opening_reconnect:\n");
    DUMP8("%02x ", &record1.bdAddr, 6);
    DUMP8("%02x ", &record2.bdAddr, 6);

#ifdef __TWS__  

//@20180117 by Parker.Wei should also deal TWS PDL record 
    if (ret == 0) {
		
		TRACE("!!!PDL is 0\n");    
#ifdef __EARPHONE_STAY_BOTH_SCAN__
#if defined (__MASTER_AUTO_TWS_SEARCHING_WITH_EMPTY_PDL__)
//Modified by ATX : Leon.He_20171130: for pending tws searching
		TRACE("!!!go to tws searching\n");
		app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 2,0);
#elif defined (__SLAVE_AUTO_PAIRING_WITH_EMPTY_PDL_)
		TRACE("!!!go to tws pairing\n");
		app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_TWS_PAIR, 0,0);
#else
		TRACE("!!!go to pairing\n");
		app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
#endif
#else
		app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
#endif
        OS_UnlockStack();

		return ;

    }

	if (ret == 1) {
		if(!memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record1.bdAddr.addr,BD_ADDR_SIZE)){
           ret = 0; 
		}
    }else if (ret == 2) {
        do {
            if(memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record1.bdAddr.addr,BD_ADDR_SIZE)){
                ret = 1;
                break;
            }else if(memcmp(nvrecord_env->tws_mode.record.bdAddr.addr,record2.bdAddr.addr,BD_ADDR_SIZE)){
                record1 = record2;
                ret = 1;
                break;
            }
        }while(0);
    }
#endif

    if(ret > 0){
#ifndef __DISABLE_SLAVE_OPENING_RECONNECT_PHONE_
        TRACE("!!!start reconnect first device\n");

        if(MEC(pendCons) == 0){
            bt_profile_manager[BT_DEVICE_ID_1].reconnect_mode = bt_profile_reconnect_openreconnecting;
            memcpy(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.addr, record1.bdAddr.addr, BD_ADDR_SIZE);
            btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(bt_profile_manager[BT_DEVICE_ID_1].rmt_addr.addr);
#if defined(A2DP_AAC_ON)
            if(btdevice_plf_p->a2dp_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
                bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_aac_stream;
            else
                bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_stream[BT_DEVICE_ID_1];
                
#else
            bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_stream[BT_DEVICE_ID_1];
#endif
            bt_profile_manager[BT_DEVICE_ID_1].chan = &app_bt_device.hf_channel[BT_DEVICE_ID_1];

            if (btdevice_plf_p->hfp_act){
                TRACE("try connect hf");
                HF_CreateServiceLink(bt_profile_manager[BT_DEVICE_ID_1].chan, &bt_profile_manager[BT_DEVICE_ID_1].rmt_addr);
            }else if(btdevice_plf_p->a2dp_act) {            
                TRACE("try connect a2dp");
                A2DP_OpenStream(bt_profile_manager[BT_DEVICE_ID_1].stream, &bt_profile_manager[BT_DEVICE_ID_1].rmt_addr);
            }
        }
#ifdef __BT_ONE_BRING_TWO__        
        if(ret > 1){            
            TRACE("!!!need reconnect second device\n");
            bt_profile_manager[BT_DEVICE_ID_2].reconnect_mode = bt_profile_reconnect_openreconnecting;
            memcpy(bt_profile_manager[BT_DEVICE_ID_2].rmt_addr.addr, record2.bdAddr.addr, BD_ADDR_SIZE);            
            bt_profile_manager[BT_DEVICE_ID_2].stream = &app_bt_device.a2dp_stream[BT_DEVICE_ID_2];
            bt_profile_manager[BT_DEVICE_ID_2].chan = &app_bt_device.hf_channel[BT_DEVICE_ID_2];
        }
#endif        
#endif  //__DISABLE_SLAVE_OPENING_RECONNECT_PHONE_
    }else{
#ifdef __EARPHONE_STAY_BOTH_SCAN__
        TRACE("!!!go to pairing\n");
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);

#else
        app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
#endif
    }
    OS_UnlockStack();
}

void app_bt_profile_connect_manager_hf(enum BT_DEVICE_ID_T id, HfChannel *Chan, HfCallbackParms *Info)
{
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get((uint8_t *)Info->p.remDev->bdAddr.addr);
    
    osTimerStop(bt_profile_manager[id].connect_timer);
    bt_profile_manager[id].connect_timer_cb = NULL;
    if (Chan&&Info){
        switch(Info->event)
        {
            case HF_EVENT_SERVICE_CONNECTED:                
                TRACE("%s HF_EVENT_SERVICE_CONNECTED",__func__);
                btdevice_plf_p->hfp_act = true;
#if  FPGA==0
                nv_record_touch_cause_flush();
#endif

			  //Modified by ATX : parker.wei_20180309 
#ifdef __HF_ATCMD_SEND_ 
				app_hfp_send_accessory_cmd();
#endif

                bt_profile_manager[id].hfp_connect = bt_profile_connect_status_success;                 
                bt_profile_manager[id].reconnect_cnt = 0;
                bt_profile_manager[id].chan = &app_bt_device.hf_channel[id];
                memcpy(bt_profile_manager[id].rmt_addr.addr, Info->p.remDev->bdAddr.addr, BD_ADDR_SIZE);
                if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
                    //do nothing
                }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){
                    if (btdevice_plf_p->a2dp_act && bt_profile_manager[id].a2dp_connect != bt_profile_connect_status_success){                
#if defined(A2DP_AAC_ON)
                        if(btdevice_plf_p->a2dp_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
                            bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_aac_stream;
                        else
                            bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_stream[BT_DEVICE_ID_1];
                            
#else
                        bt_profile_manager[BT_DEVICE_ID_1].stream = &app_bt_device.a2dp_stream[BT_DEVICE_ID_1];
#endif
                        TRACE("!!!continue connect a2dp\n");
                        A2DP_OpenStream(bt_profile_manager[id].stream, &bt_profile_manager[id].rmt_addr);
                    }
                }
#ifdef __AUTO_CONNECT_OTHER_PROFILE__
                else{
                    bt_profile_manager[id].connect_timer_cb = app_bt_profile_connect_a2dp_retry_timehandler;
                    osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_CONNECT_RETRY_MS);
                }
#endif
                break;            
            case HF_EVENT_SERVICE_DISCONNECTED:             
#ifdef __DISCONNECT_TEST_
                hf_event_disconnected_reason = Info->p.remDev->discReason;
#endif
                TRACE("%s HF_EVENT_SERVICE_DISCONNECTED discReason:%d/%d",__func__, Info->p.remDev->discReason, Info->p.remDev->discReason_saved);
                bt_profile_manager[id].hfp_connect = bt_profile_connect_status_failure;                
                if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
                    //do nothing
                }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){
                    if (++bt_profile_manager[id].reconnect_cnt < APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT){                        
                        osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS);
                    }else{
#ifdef __LINK_LOSE_TIMEOUT_THEN_ENTER_PAIRING__//Modified by ATX : Leon.He_20170317: reconnect only during link loss, enter pairing after timeout
                    	struct nvrecord_env_t *nvrecord_env;
                    	nv_record_env_get(&nvrecord_env);
                    	if(nvrecord_env->tws_mode.mode == TWSMASTER && app_tws_get_mode() == TWSMASTER)
                        	app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
#endif
                        bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;
                    }                    
                    TRACE("%s try to reconnect cnt:%d",__func__, bt_profile_manager[id].reconnect_cnt);
                    }else if ((Info->p.remDev->discReason == 0x8)||
                              (Info->p.remDev->discReason_saved == 0x8)){
                    bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_reconnecting;                    
                    TRACE("%s try to reconnect",__func__);
                    osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS);
                }else{
                    bt_profile_manager[id].hfp_connect = bt_profile_connect_status_unknow;
                }
                break;
            default:
                break;                
        }
    }
    
    if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){        
        bool reconnect_hfp_proc_final = true;
        bool reconnect_a2dp_proc_final = true;
        if (bt_profile_manager[id].hfp_connect == bt_profile_connect_status_failure){
            reconnect_hfp_proc_final = false;
        }
        if (bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_failure){
            reconnect_a2dp_proc_final = false;
        }
        if (reconnect_hfp_proc_final && reconnect_a2dp_proc_final){
            TRACE("!!!reconnect success1 %d/%d/%d\n", bt_profile_manager[id].hfp_connect, bt_profile_manager[id].hsp_connect, bt_profile_manager[id].a2dp_connect);
            bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;            
        }            
    }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
        bool opening_hfp_proc_final = false;
        bool opening_a2dp_proc_final = false;

        if (btdevice_plf_p->hfp_act && bt_profile_manager[id].hfp_connect == bt_profile_connect_status_unknow){
            opening_hfp_proc_final = false;
        }else{
            opening_hfp_proc_final = true;
        }

        if (btdevice_plf_p->a2dp_act && bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_unknow){
            opening_a2dp_proc_final = false;
        }else{
            opening_a2dp_proc_final = true;
        }

        if ((opening_hfp_proc_final && opening_a2dp_proc_final) ||
            (bt_profile_manager[id].hfp_connect == bt_profile_connect_status_failure)){        
            TRACE("!!!reconnect success2 %d/%d/%d\n", bt_profile_manager[id].hfp_connect, bt_profile_manager[id].hsp_connect, bt_profile_manager[id].a2dp_connect);
            bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;            
#ifdef __EARPHONE_STAY_BOTH_SCAN__
//Modified by ATX : Leon.He_20180109: only master auto enter pairing when reconnect hfp and a2dp fail.
			if((bt_profile_manager[id].hfp_connect == bt_profile_connect_status_failure))
			{
				TRACE("hfp_connect failure,force to pairing\n");
#ifdef __SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
			app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_TWS_PAIR, 0,0);
#endif//__SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
#ifdef __MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
			app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
#endif//__MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
			}
#else
            app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_CONNECTABLE_ONLY,0,0);
#endif            
            
        }

        if (btdevice_plf_p->hfp_act && bt_profile_manager[id].hfp_connect == bt_profile_connect_status_success){
            if (btdevice_plf_p->a2dp_act && !opening_a2dp_proc_final){                
                TRACE("!!!continue connect a2dp\n");
              //  A2DP_OpenStream(bt_profile_manager[id].stream, &bt_profile_manager[id].rmt_addr);
                    bt_profile_manager[id].connect_timer_cb = app_bt_profile_connect_a2dp_retry_timehandler;
                    osTimerStart(bt_profile_manager[id].connect_timer, 300);      
              
            }
        }

        if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_null){
            for (uint8_t i=0; i<BT_DEVICE_NUM; i++){
                if (bt_profile_manager[i].reconnect_mode == bt_profile_reconnect_openreconnecting){
                    TRACE("!!!hf->start reconnect second device\n");
                    if (btdevice_plf_p->hfp_act){
                        TRACE("try connect hf");
                        HF_CreateServiceLink(bt_profile_manager[i].chan, &bt_profile_manager[i].rmt_addr);
                    }else if(btdevice_plf_p->a2dp_act) {            
                        TRACE("try connect a2dp");
                        A2DP_OpenStream(bt_profile_manager[i].stream, &bt_profile_manager[i].rmt_addr);
                    }
                    break;
                }
            }
        }
    }

    if (!bt_profile_manager[id].has_connected &&
        (bt_profile_manager[id].hfp_connect == bt_profile_connect_status_success ||
         bt_profile_manager[id].hsp_connect == bt_profile_connect_status_success||
         bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_success)){

        bt_profile_manager[id].has_connected = true;
        app_cancel_postpone_to_auto_pairing_when_disconnected_timer();//Modified by ATX : Leon.He_20180206: postpone to auto pairing when disconnected
        app_audio_switch_flash_flush_req();
		

#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_CONNECTED, id);
        app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
#ifndef FPGA //@20180113 by Parker.Wei
         app_stop_10_second_timer(APP_PAIR_TIMER_ID);
#endif
#endif
		//Modified by ATX : Parke.Wei_20180316
		tws_player_update_peer_state(APP_STATUS_INDICATION_CONNECTED);

    }

    if (bt_profile_manager[id].has_connected &&
        (bt_profile_manager[id].hfp_connect != bt_profile_connect_status_success &&
         bt_profile_manager[id].hsp_connect != bt_profile_connect_status_success &&
         bt_profile_manager[id].a2dp_connect != bt_profile_connect_status_success)){

        bt_profile_manager[id].has_connected = false;
#ifdef __LINK_LOSE_TIMEOUT_THEN_ENTER_PAIRING__//Modified by ATX : Leon.He_20170317: reconnect only during link loss, enter pairing after timeout
        if(Info->p.remDev->discReason == 0x8)
            app_voice_report(APP_STATUS_INDICATION_WARNING, id);
        else
#endif		
#ifdef MEDIA_PLAYER_SUPPORT
        	app_voice_report(APP_STATUS_INDICATION_DISCONNECTED, id);
#endif
		//Modified by ATX : Parke.Wei_20180316
		tws_player_update_peer_state(APP_STATUS_INDICATION_DISCONNECTED);

    }
}

void app_bt_profile_connect_manager_a2dp(enum BT_DEVICE_ID_T id, A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    btdevice_profile *btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get((uint8_t *)Stream->stream.conn.remDev->bdAddr.addr);

    osTimerStop(bt_profile_manager[id].connect_timer);
    bt_profile_manager[id].connect_timer_cb = NULL;
    if (Stream&&Info){
        switch(Info->event)
        {
            case A2DP_EVENT_STREAM_OPEN:                
                TRACE("%s A2DP_EVENT_STREAM_OPEN,codec type=%x",__func__,Info->p.configReq->codec.codecType);
                btdevice_plf_p->a2dp_act = true;                
                btdevice_plf_p->a2dp_codectype = Info->p.configReq->codec.codecType;                
#if  FPGA==0
                nv_record_touch_cause_flush();
#endif
                bt_profile_manager[id].a2dp_connect = bt_profile_connect_status_success;
                bt_profile_manager[id].reconnect_cnt = 0;
                bt_profile_manager[id].stream = &app_bt_device.a2dp_stream[id];
                memcpy(bt_profile_manager[id].rmt_addr.addr, Stream->stream.conn.remDev->bdAddr.addr, BD_ADDR_SIZE);
                if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
                    //do nothing
                }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){
                    if (btdevice_plf_p->hfp_act&& bt_profile_manager[id].hfp_connect != bt_profile_connect_status_success){                
                        TRACE("!!!continue connect hfp\n");
                        HF_CreateServiceLink(bt_profile_manager[id].chan, &bt_profile_manager[id].rmt_addr);
                    }
                }
#ifdef __AUTO_CONNECT_OTHER_PROFILE__
                else{
                    bt_profile_manager[id].connect_timer_cb = app_bt_profile_connect_hf_retry_timehandler;
                    osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_CONNECT_RETRY_MS);
                }
#endif
                break;            
            case A2DP_EVENT_STREAM_CLOSED:                
                TRACE("%s A2DP_EVENT_STREAM_CLOSED discReason:%d/%d",__func__, Info->discReason, A2DP_GetRemoteDevice(Stream)->discReason_saved);
                bt_profile_manager[id].a2dp_connect = bt_profile_connect_status_failure;
                
                if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
                   //do nothing
                }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){
                   if (++bt_profile_manager[id].reconnect_cnt < APP_BT_PROFILE_RECONNECT_RETRY_LIMIT_CNT){
                       osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS);
                   }else{
                       bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;                    
                   }
                   TRACE("%s try to reconnect cnt:%d",__func__, bt_profile_manager[id].reconnect_cnt);
                }else if(((Info->discReason == 0x8)||
                           (A2DP_GetRemoteDevice(Stream)->discReason_saved == 0x8)) && 
                        (btdevice_plf_p->a2dp_act)&&
                        (!btdevice_plf_p->hfp_act) &&
                        (!btdevice_plf_p->hsp_act)){
                    bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_reconnecting;                    
                    TRACE("%s try to reconnect cnt:%d",__func__, bt_profile_manager[id].reconnect_cnt);
                    osTimerStart(bt_profile_manager[id].connect_timer, APP_BT_PROFILE_RECONNECT_RETRY_INTERVAL_MS);
               }else{
                    bt_profile_manager[id].a2dp_connect = bt_profile_connect_status_unknow;
               }
               break;
            default:
                break;                
        }
    }

    if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_reconnecting){        
        bool reconnect_hfp_proc_final = true;
        bool reconnect_a2dp_proc_final = true;
        if (bt_profile_manager[id].hfp_connect == bt_profile_connect_status_failure){
            reconnect_hfp_proc_final = false;
        }
        if (bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_failure){
            reconnect_a2dp_proc_final = false;
        }
        if (reconnect_hfp_proc_final && reconnect_a2dp_proc_final){
            TRACE("!!!reconnect success %d/%d/%d\n", bt_profile_manager[id].hfp_connect, bt_profile_manager[id].hsp_connect, bt_profile_manager[id].a2dp_connect);
            bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;            
        }            
    }else if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_openreconnecting){
        bool opening_hfp_proc_final = false;
        bool opening_a2dp_proc_final = false;

        if (btdevice_plf_p->hfp_act && bt_profile_manager[id].hfp_connect == bt_profile_connect_status_unknow){
            opening_hfp_proc_final = false;
        }else{
            opening_hfp_proc_final = true;
        }

        if (btdevice_plf_p->a2dp_act && bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_unknow){
            opening_a2dp_proc_final = false;
        }else{
            opening_a2dp_proc_final = true;
        }

        if ((opening_hfp_proc_final && opening_a2dp_proc_final) ||
            (bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_failure)){        
            TRACE("!!!reconnect success %d/%d/%d\n", bt_profile_manager[id].hfp_connect, bt_profile_manager[id].hsp_connect, bt_profile_manager[id].a2dp_connect);
            bt_profile_manager[id].reconnect_mode = bt_profile_reconnect_null;

//Modified by ATX : Leon.He_20180109: diff master and slave pairing mode
            if(bt_profile_manager[id].hfp_connect == bt_profile_connect_status_unknow && bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_failure)
			{
				TRACE("a2dp_connect failure,force to pairing\n");
#ifdef __SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
				app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_TWS_PAIR, 0,0);
#endif//__SLAVE_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
#ifdef __MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
				app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BT_DEFAULT_ACCESS_MODE_PAIR, 0,0);
#endif//__MASTER_AUTO_PAIRING_WHEN_FAILED_TO_RECONNECT__
			}

        }

        if (btdevice_plf_p->a2dp_act && bt_profile_manager[id].a2dp_connect== bt_profile_connect_status_success){
            if (btdevice_plf_p->hfp_act && !opening_hfp_proc_final){                
                TRACE("!!!continue connect hf\n");
                HF_CreateServiceLink(bt_profile_manager[id].chan, &bt_profile_manager[id].rmt_addr);
            }
        }

        if (bt_profile_manager[id].reconnect_mode == bt_profile_reconnect_null){
            for (uint8_t i=0; i<BT_DEVICE_NUM; i++){
                if (bt_profile_manager[i].reconnect_mode == bt_profile_reconnect_openreconnecting){
                    TRACE("!!!a2dp->start reconnect second device\n");
                    if (btdevice_plf_p->hfp_act){
                        TRACE("try connect hf");
                        HF_CreateServiceLink(bt_profile_manager[i].chan, &bt_profile_manager[i].rmt_addr);
                    }else if(btdevice_plf_p->a2dp_act) {            
                        TRACE("try connect a2dp");
                        A2DP_OpenStream(bt_profile_manager[i].stream, &bt_profile_manager[i].rmt_addr);
                    }
                    break;
                }
            }
        }
    }
    
    if (!bt_profile_manager[id].has_connected &&
        (bt_profile_manager[id].hfp_connect == bt_profile_connect_status_success ||
         bt_profile_manager[id].hsp_connect == bt_profile_connect_status_success||
         bt_profile_manager[id].a2dp_connect == bt_profile_connect_status_success)){

        bt_profile_manager[id].has_connected = true;
        app_cancel_postpone_to_auto_pairing_when_disconnected_timer();//Modified by ATX : Leon.He_20180206: postpone to auto pairing when disconnected
        app_audio_switch_flash_flush_req();
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_CONNECTED, id);
        app_status_indication_set(APP_STATUS_INDICATION_CONNECTED);
#ifndef FPGA //@20180113 by Parker.Wei
		app_stop_10_second_timer(APP_PAIR_TIMER_ID);
#endif

#endif

		//Modified by ATX : Parke.Wei_20180316	
		 tws_player_update_peer_state(APP_STATUS_INDICATION_CONNECTED);  

    }

    if (bt_profile_manager[id].has_connected &&
        (bt_profile_manager[id].hfp_connect != bt_profile_connect_status_success &&
         bt_profile_manager[id].hsp_connect != bt_profile_connect_status_success &&
         bt_profile_manager[id].a2dp_connect != bt_profile_connect_status_success)){

        bt_profile_manager[id].has_connected = false;
#ifdef __LINK_LOSE_TIMEOUT_THEN_ENTER_PAIRING__//Modified by ATX : Leon.He_20170317: reconnect only during link loss, enter pairing after timeout
        if(Info->discReason == 0x8)
            app_voice_report(APP_STATUS_INDICATION_WARNING, id);
        else
#endif
#ifdef MEDIA_PLAYER_SUPPORT
        app_voice_report(APP_STATUS_INDICATION_DISCONNECTED, id);
#endif
		//Modified by ATX : Parke.Wei_20180316
		tws_player_update_peer_state(APP_STATUS_INDICATION_DISCONNECTED);	

    }

}


void app_bt_init(void)
{
    app_bt_mail_init();

    app_set_threadhandle(APP_MODUAL_BT, app_bt_handle_process);
	
	//Modified by ATX : Parker.Wei_20180615  
	/*
	fixed when hs reconnect the AG ,won't have" pairing request" popout
	if we ignored the connected bt device on AG before;*/
    SecSetIoCapRspRejectExt(app_bt_profile_connect_openreconnecting);

}

#if HID_DEVICE == XA_ENABLED
void hid_exit_shutter_mode(void);
#endif

void app_bt_disconnect_all(void)
{
    OS_LockStack();

#if HID_DEVICE == XA_ENABLED
    hid_exit_shutter_mode();
#endif
    
    if(app_bt_device.hf_channel[BT_DEVICE_ID_1].state == HF_STATE_OPEN)
    {
        if(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev)
            MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev);        
//        HF_DisconnectServiceLink(&app_bt_device.hf_channel[BT_DEVICE_ID_1]);
    }
    else if(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].stream.state == AVDTP_STRM_STATE_STREAMING ||
        app_bt_device.a2dp_stream[BT_DEVICE_ID_1].stream.state == AVDTP_STRM_STATE_OPEN)
    {
        if(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].device->cmgrHandler.remDev)
            MeDisconnectLink(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].device->cmgrHandler.remDev);        
//        A2DP_CloseStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_1]);
//        AVRCP_Disconnect(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1]);
    }    
    OS_UnlockStack();
#ifdef __BT_ONE_BRING_TWO__

#if 0
    if(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_OPEN)
    {
        MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev);        
    }
#endif
    OS_LockStack();
    if(app_bt_device.hf_channel[BT_DEVICE_ID_2].state == HF_STATE_OPEN)
    {
        if(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev)
            MeDisconnectLink(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev);        
//        HF_DisconnectServiceLink(&app_bt_device.hf_channel[BT_DEVICE_ID_2]);
    }
    else if(app_bt_device.a2dp_stream[BT_DEVICE_ID_2].stream.state == AVDTP_STRM_STATE_STREAMING ||
        app_bt_device.a2dp_stream[BT_DEVICE_ID_2].stream.state == AVDTP_STRM_STATE_OPEN)
    {
        if(app_bt_device.a2dp_stream[BT_DEVICE_ID_2].device->cmgrHandler.remDev)
            MeDisconnectLink(app_bt_device.a2dp_stream[BT_DEVICE_ID_2].device->cmgrHandler.remDev);        
//        A2DP_CloseStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_2]);
 //       AVRCP_Disconnect(&app_bt_device.avrcp_channel[BT_DEVICE_ID_2]);
    }
    OS_UnlockStack();
#endif
}

BOOL app_bt_bypass_hcireceiveddata_cb(void)
{
    ListEntry        *tmpRxBuffList;            
    HciBuffer        *hciRxBuffer;            
    HciBuffer        *next;
    BOOL              handled = FALSE;
    BOOL              nRet =  TRUE;
    
    tmpRxBuffList = &HCC(rxBuffList);
    
    OS_StopHardware();
    if (!IsListEmpty(tmpRxBuffList)) {
        IterateListSafe(HCC(rxBuffList), hciRxBuffer, next, HciBuffer *) {
            if (hciRxBuffer->flags & HCI_BUFTYPE_EVENT) {
                handled = TRUE;
                RemoveEntryList(&hciRxBuffer->node);
                break;
            }
        }
    }            
    if (handled){        
//        TRACE("InsertHeadList hciRxBuffer(event)");
        InsertHeadList(tmpRxBuffList, &(hciRxBuffer->node));
        OS_NotifyEvm();
        nRet =  FALSE;
    }

    OS_ResumeHardware();
#if defined(__TWS__)
    if (app_tws_get_a2dpbuff_available() > 1024){        
//        TRACE("!!!Resume HciBypassProcessReceivedData");
        HciBypassProcessReceivedDataExt(NULL);
        nRet =  FALSE;
    }    
#endif
    return nRet;
}



void app_bt_hcireceived_data_clear(uint16_t conhdl)
{
    ListEntry        *tmpRxBuffList;            
    HciBuffer        *hciRxBuffer;            
    HciBuffer        *next;
    uint16_t   buff_handle;
    tmpRxBuffList = &HCC(rxBuffList);
    OS_StopHardware();
    if (!IsListEmpty(tmpRxBuffList)) {
        IterateListSafe(HCC(rxBuffList), hciRxBuffer, next, HciBuffer *) {
            if (hciRxBuffer->flags & HCI_BUFTYPE_ACL_DATA) {
                 buff_handle = LEtoHost16(hciRxBuffer->buffer);
                 buff_handle &= 0x0FFF;
                 TRACE("hciRxBuffer BUFF HANDLE=%x",buff_handle);
                if(buff_handle == conhdl)
                {
                     RemoveEntryList(&hciRxBuffer->node);
                    TRACE("free hciRxBuffer(event)");
                    RXBUFF_Free(hciRxBuffer);
                }
            }
        }
    }            
    OS_ResumeHardware();
    
}

//Modified by ATX : cc_20180317: Has Profile Record?
bool app_bt_has_profile_record(U8 *addr)
{
    btdevice_profile *btdevice_plf_p;
    btdevice_plf_p = (btdevice_profile *)app_bt_profile_active_store_ptr_get(addr);
    if ((btdevice_plf_p->hfp_act)||(btdevice_plf_p->a2dp_act))
        return true;
    
    return false;
}

//Modified by ATX : cc_20180413  ble adv for check version 
void app_check_version_ble_adv_start(void)
{
    adv_para_struct adv_para;
    uint8_t adv_all_data_len = 0;
#ifndef BLE_DEFAULT_NAME
#ifdef __TWS_CHANNEL_LEFT__
    uint8_t adv_data[] = {
        0x13,       //len
        0x09,       //type name
        'B', 'E', 'S','-','V', 'E', 'R',  'S','I', 'O', 'N','-','0',  '0', '0', '7','-','L',  // name
        0x05,       //len
        0xff,       //type manufacturer
        0xff ,0xff,0x00,0x01// manufacturer data
    };
#else 
    uint8_t adv_data[] = {
        0x13,       //len
        0x09,       //type name
        'B', 'E', 'S','-','V', 'E', 'R',  'S','I', 'O', 'N','-','0',  '0', '0', '7','-','R',  // name
        0x05,       //len
        0xff,       //type manufacturer
        0xff ,0xff,0x00,0x01// manufacturer data
    };
    adv_all_data_len = sizeof(adv_data);
#endif  //__TWS_CHANNEL_LEFT__
#else   //else BLE_DEFAULT_NAME
    uint8_t adv_data[GAP_ADV_DATA_LEN];
    adv_data[adv_all_data_len++] = sizeof(BLE_DEFAULT_NAME);
    adv_data[adv_all_data_len++] = 0x09; //type name
    memcpy(&adv_data[adv_all_data_len],BLE_DEFAULT_NAME,sizeof(BLE_DEFAULT_NAME)-1);
    adv_all_data_len += sizeof(BLE_DEFAULT_NAME)-1;
    adv_data[adv_all_data_len++] = BLE_MANU_DATA_LEN;
    memcpy(&adv_data[adv_all_data_len],BLE_MANU_DATA,BLE_MANU_DATA_LEN);    
    adv_all_data_len += BLE_MANU_DATA_LEN;
    adv_data[adv_all_data_len] = '\0';
//    TRACE("adv_all_data_len:%d adv_data:",adv_all_data_len);
//    DUMP8("%02x ", adv_data, adv_all_data_len);
#endif //endif BLE_DEFAULT_NAME
    struct nvrecord_env_t *nvrecord_env;
    TRACE("%s \r\n",__func__);
    if ((app_tws_get_mode() != TWSMASTER) && (nvrecord_env->tws_mode.mode == TWSMASTER))
    {
        if (app_tws_ble_reconnect.scan_func)
            app_tws_ble_reconnect.scan_func(APP_TWS_CLOSE_BLE_SCAN);
    }
    else if ((app_tws_get_mode() != TWSSLAVE) && (nvrecord_env->tws_mode.mode == TWSSLAVE))
    {
        if (app_tws_ble_reconnect.adv_close_func)
            app_tws_ble_reconnect.adv_close_func(APP_TWS_CLOSE_BLE_ADV); 
    }
    
    adv_para.interval_min =             0x0320; // 1s
    adv_para.interval_max =             0x0320; // 1s
    adv_para.adv_type =                     0x03;
    adv_para.own_addr_type =            0x01;
    adv_para.peer_addr_type =           0x01;
    adv_para.adv_chanmap =              0x07;
    adv_para.adv_filter_policy =        0x00;
    ME_Ble_Set_Private_Address((BT_BD_ADDR *)&bt_addr[0]);
    ME_Ble_SetAdv_parameters(&adv_para);
    ME_Ble_SetAdv_data(adv_all_data_len, adv_data);
    ME_Ble_SetAdv_en(true);
}

bool app_bt_has_connectivitys(void)
{
    int activeCons;
    OS_LockStack();
    activeCons = MEC(activeCons);
    OS_UnlockStack();

	if(activeCons > 0)
		return true;

	return false;
#if 0
	if(app_bt_device.hf_channel[BT_DEVICE_ID_1].cmgrHandler.remDev)
		return true;
	if(app_bt_device.a2dp_stream[BT_DEVICE_ID_1].device->cmgrHandler.remDev)
		return true;
#ifdef __BT_ONE_BRING_TWO__
	if(app_bt_device.hf_channel[BT_DEVICE_ID_2].cmgrHandler.remDev)
		return true;
	if(app_bt_device.a2dp_stream[BT_DEVICE_ID_2].device->cmgrHandler.remDev)
		return true;
#endif
	return false;
#endif
}
//@20180304 by parker.wei  Is mobile conneted?
bool app_bt_mobile_hasConnected(void)
{
   
	if(bt_profile_manager[BT_DEVICE_ID_1].has_connected)
		return true;

#ifdef __BT_ONE_BRING_TWO__
	if(bt_profile_manager[BT_DEVICE_ID_2].has_connected)
		return true;

#endif
	return false;

}
