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
#include "bt_drv.h"
#include "app_audio.h"
#include "bt_drv_interface.h"
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
#include "btalloc.h"
#include "hid.h"
#include "spp.h"
}


#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "app_bt.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_bt.h"
#include "app_tws_if.h"
#endif
#if defined(A2DP_AAC_ON)
#include "btapp.h"
#endif

#if (SPP_CLIENT == XA_ENABLED)
extern BtRemoteDevice *sppBtDevice;
#endif
#ifdef __A2DP_AVDTP_CP__
AvdtpContentProt a2dp_avdtpCp[BT_DEVICE_NUM];
U8 a2dp_avdtpCp_securityData[AVDTP_MAX_CP_VALUE_SIZE]=
{
};
#endif

extern void spp_callback(SppDev *locDev, SppCallbackParms *Info);


int a2dp_volume_set(U8 vol);

AvdtpCodec a2dp_avdtpcodec;
#if defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
AvdtpCodec a2dp_avdtpcodec_opus;
#endif
#if defined(A2DP_AAC_ON)
AvdtpCodec a2dp_avdtpcodec_aac;
#endif
//A2dpStream a2dp_stream;
/* avrcp */
//AvrcpChannel avrcp_channel;
#if AVRCP_ADVANCED_CONTROLLER == XA_ENABLED
typedef struct {
    AvrcpAdvancedPdu pdu;
    uint8_t para_buf[40];
}APP_A2DP_AVRCPADVANCEDPDU;
osPoolId   app_a2dp_avrcpadvancedpdu_mempool = NULL;

osPoolDef (app_a2dp_avrcpadvancedpdu_mempool, 10, APP_A2DP_AVRCPADVANCEDPDU);

#define app_a2dp_avrcpadvancedpdu_mempool_init() do{ \
                                                    if (app_a2dp_avrcpadvancedpdu_mempool == NULL) \
                                                        app_a2dp_avrcpadvancedpdu_mempool = osPoolCreate(osPool(app_a2dp_avrcpadvancedpdu_mempool)); \
                                                  }while(0);

#define app_a2dp_avrcpadvancedpdu_mempool_calloc(buf)  do{ \
                                                        APP_A2DP_AVRCPADVANCEDPDU * avrcpadvancedpdu; \
                                                        avrcpadvancedpdu = (APP_A2DP_AVRCPADVANCEDPDU *)osPoolCAlloc(app_a2dp_avrcpadvancedpdu_mempool); \
                                                        buf = &(avrcpadvancedpdu->pdu); \
                                                        buf->parms = avrcpadvancedpdu->para_buf; \
                                                     }while(0);

#define app_a2dp_avrcpadvancedpdu_mempool_free(buf)  do{ \
                                                        osPoolFree(app_a2dp_avrcpadvancedpdu_mempool, buf); \
                                                     }while(0);
#endif

void get_value1_pos(U8 mask,U8 *start_pos, U8 *end_pos)
{
    U8 num = 0;

    for(U8 i=0;i<8;i++){
        if((0x01<<i) & mask){
            *start_pos = i;//start_pos,end_pos stands for the start and end position of value 1 in mask
            break;
        }
    }
    for(U8 i=0;i<8;i++){
        if((0x01<<i) & mask)
            num++;//number of value1 in mask
    }
    *end_pos = *start_pos + num - 1;
}
U8 get_valid_bit(U8 elements, U8 mask)
{
    U8 start_pos,end_pos;

    get_value1_pos(mask,&start_pos,&end_pos);
//    TRACE("!!!start_pos:%d,end_pos:%d\n",start_pos,end_pos);
    for(U8 i = start_pos; i <= end_pos; i++){
        if((0x01<<i) & elements){
            elements = ((0x01<<i) | (elements & (~mask)));
            break;
        }
    }
    return elements;
}


struct BT_DEVICE_T  app_bt_device;

#if AVRCP_ADVANCED_CONTROLLER == XA_ENABLED
void a2dp_init(void)
{
    app_a2dp_avrcpadvancedpdu_mempool_init();

    for(uint8_t i=0; i<BT_DEVICE_NUM; i++){
        app_bt_device.a2dp_state[i]=0;
        app_bt_device.a2dp_streamming[i] = 0;
        app_bt_device.avrcp_get_capabilities_rsp[i] = NULL;
        app_bt_device.avrcp_control_rsp[i] = NULL;
        app_bt_device.avrcp_notify_rsp[i] = NULL;
        app_bt_device.avrcp_cmd1[i] = NULL;
        app_bt_device.avrcp_cmd2[i] = NULL;
        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_play_status_cmd[i]);;
    }

    app_bt_device.a2dp_state[BT_DEVICE_ID_1]=0;
    app_bt_device.a2dp_play_pause_flag = 0;
    app_bt_device.curr_a2dp_stream_id= BT_DEVICE_ID_1;
}



#ifdef __TWS__
typedef uint8_t tx_done_flag;
#define TX_DONE_FLAG_INIT           0
#define TX_DONE_FLAG_SUCCESS    1
#define TX_DONE_FLAG_FAIL           2
#define TX_DONE_FLAG_TXING        3
tx_done_flag TG_tx_done_flag = TX_DONE_FLAG_INIT;

#if 0
void avrcp_set_slave_volume(uint8_t transid,int8_t volume)
{
    AvrcpChannel *tws_TG = NULL;
    BtStatus get_volume_rsp_status;

    if((TG_tx_done_flag == TX_DONE_FLAG_INIT)||(TG_tx_done_flag == TX_DONE_FLAG_SUCCESS)){
        if(app_tws_get_mode() == TWSMASTER)
        {
            TRACE("!!!avrcp_set_slave_volume transid:%x\n",transid);
            tws_TG = app_tws_get_avrcp_TGchannel();
            if(tws_TG->adv.cmdInUse == FALSE){
                if (app_bt_device.avrcp_volume_cmd[0] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_volume_cmd[0]);
                app_bt_device.avrcp_volume_cmd[0]->transId = transid;
                app_bt_device.avrcp_volume_cmd[0]->ctype = AVCTP_RESPONSE_INTERIM;
                app_bt_device.volume_report[0] = AVCTP_RESPONSE_INTERIM;
                get_volume_rsp_status = AVRCP_CtGetAbsoluteVolume_Rsp(tws_TG, app_bt_device.avrcp_volume_cmd[0], volume);
                TG_tx_done_flag = TX_DONE_FLAG_TXING;
                TRACE("!!!avrcp_set_slave_volume  get_volume_rsp_status:%x\n",get_volume_rsp_status);
                if(get_volume_rsp_status != BT_STATUS_PENDING){
                     //don't know reason why the status != pending yet
                    TG_tx_done_flag = TX_DONE_FLAG_FAIL;
                }
            }
        }
    }
}


void avrcp_callback_TG(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    TRACE("avrcp_callback_TG : chnl %p, Parms %p\n", chnl, Parms);
    TRACE("::avrcp_callback_TG Parms->event %d\n", Parms->event);
#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T device_id = (chnl == &app_bt_device.avrcp_channel[0])?BT_DEVICE_ID_1:BT_DEVICE_ID_2;
#else
    enum BT_DEVICE_ID_T device_id = BT_DEVICE_ID_1;
#endif
    switch(Parms->event)
    {
        case AVRCP_EVENT_CONNECT:
            if(0)//(chnl->avrcpVersion >=0x103)
            {
                TRACE("::avrcp_callback_TG AVRCP_GET_CAPABILITY\n");
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd1[device_id]);
                AVRCP_CtGetCapabilities(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED);
            }

            app_bt_device.avrcp_channel[device_id].avrcpState = AVRCP_STATE_CONNECTED;
#ifdef __TWS__
            if(app_tws_get_mode() == TWSMASTER)
            {
                //avrcp_set_slave_volume(0,a2dp_volume_get_tws());
                tws_player_set_a2dp_vol(a2dp_volume_get_tws());
            }
#endif

#ifdef __TWS__
            app_tws_reconnect_slave_process(0,AVRCP_EVENT_CONNECT);
#endif
            TRACE("::avrcp_callback_TG AVRCP_EVENT_CONNECT %x,device_id=%d\n", chnl->avrcpVersion,device_id);
            break;
        case AVRCP_EVENT_DISCONNECT:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_DISCONNECT");
            app_bt_device.avrcp_channel[device_id].avrcpState = AVRCP_STATE_DISCONNECTED;
            if (app_bt_device.avrcp_get_capabilities_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_control_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_control_rsp[device_id]);
                app_bt_device.avrcp_control_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_notify_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_notify_rsp[device_id]);
                app_bt_device.avrcp_notify_rsp[device_id] = NULL;
            }

            if (app_bt_device.avrcp_cmd1[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd1[device_id]);
                app_bt_device.avrcp_cmd1[device_id] = NULL;
            }
            if (app_bt_device.avrcp_cmd2[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd2[device_id]);
                app_bt_device.avrcp_cmd2[device_id] = NULL;
            }
            app_bt_device.volume_report[device_id] = 0;
#ifdef __TWS__
            app_tws_reconnect_slave_process(0,AVRCP_EVENT_DISCONNECT);
#endif
            break;
        case AVRCP_EVENT_RESPONSE:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_RESPONSE op=%x,status=%x\n", Parms->advOp,Parms->status);

            break;
        case AVRCP_EVENT_PANEL_CNF:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_CNF %x,%x,%x",
                Parms->p.panelCnf.response,Parms->p.panelCnf.operation,Parms->p.panelCnf.press);
            break;
#ifdef __TWS__
        case AVRCP_EVENT_PANEL_PRESS:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_PRESS %x,%x,device_id=%d",
                Parms->p.panelCnf.operation,Parms->p.panelInd.operation,device_id);
            switch(Parms->p.panelInd.operation)
            {
                case AVRCP_POP_STOP:
                    TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_STOP");
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_STOP,TRUE);
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_STOP,FALSE);
                    app_bt_device.a2dp_play_pause_flag = 0;
                    break;
                case AVRCP_POP_PLAY:
                case AVRCP_POP_PAUSE:
                    if(app_bt_device.a2dp_play_pause_flag == 0){
                        AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_PLAY,TRUE);
                        AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_PLAY,FALSE);
                        app_bt_device.a2dp_play_pause_flag = 1;
                    }else{
                        AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_PAUSE,TRUE);
                        AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_PAUSE,FALSE);
                        app_bt_device.a2dp_play_pause_flag = 0;
                    }

                    break;
                case AVRCP_POP_FORWARD:
                    TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_FORWARD");
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_FORWARD,TRUE);
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_FORWARD,FALSE);
                    app_bt_device.a2dp_play_pause_flag = 1;
                    break;
                case AVRCP_POP_BACKWARD:
                    TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_BACKWARD");
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_BACKWARD,TRUE);
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_BACKWARD,FALSE);
                    app_bt_device.a2dp_play_pause_flag = 1;
                    break;
                case AVRCP_POP_VOLUME_UP:
                    TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_UP");
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_VOLUME_UP,TRUE);
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_VOLUME_UP,FALSE);
                    app_bt_stream_volumeup();
                    btapp_hfp_report_speak_gain();
                    btapp_a2dp_report_speak_gain();/* for press volume up key on slave */
                    break;
                case AVRCP_POP_VOLUME_DOWN:
                    TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_DOWN");
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_VOLUME_DOWN,TRUE);
                    AVRCP_SetPanelKey(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1],AVRCP_POP_VOLUME_DOWN,FALSE);
                    app_bt_stream_volumedown();
                    btapp_hfp_report_speak_gain();
                    btapp_a2dp_report_speak_gain();/* for press volume down key on slave */
                    break;
                default :
                    break;
            }
            break;
        case AVRCP_EVENT_PANEL_HOLD:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_HOLD %x,%x",
                Parms->p.panelCnf.operation,Parms->p.panelInd.operation);
            break;
        case AVRCP_EVENT_PANEL_RELEASE:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_RELEASE %x,%x",
                Parms->p.panelCnf.operation,Parms->p.panelInd.operation);
            break;
#endif
        case AVRCP_EVENT_ADV_TX_DONE:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_TX_DONE device_id=%d,status=%x,errorcode=%x\n",device_id,Parms->status,Parms->errorCode);
            TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_TX_DONE op:%d, transid:%x\n", Parms->p.adv.txPdu->op,Parms->p.adv.txPdu->transId);
            if (Parms->p.adv.txPdu->op == AVRCP_OP_GET_CAPABILITIES){
                if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == Parms->p.adv.txPdu){
                    app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
                    app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                }
            }
            TG_tx_done_flag = TX_DONE_FLAG_SUCCESS;
#if 0
            if (Parms->p.adv.txPdu->op == AVRCP_OP_SET_ABSOLUTE_VOLUME){
                if (Parms->p.adv.txPdu->ctype != AVCTP_RESPONSE_INTERIM){
                    if (app_bt_device.avrcp_control_rsp[device_id] == Parms->p.adv.txPdu){
                        app_bt_device.avrcp_control_rsp[device_id] = NULL;
                        app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                    }
                }
            }
            if (Parms->p.adv.txPdu->op == AVRCP_OP_REGISTER_NOTIFY){
                if (Parms->p.adv.txPdu->ctype != AVCTP_RESPONSE_INTERIM){
                    if (Parms->p.adv.txPdu->parms[0] == AVRCP_EID_VOLUME_CHANGED){
                        app_bt_device.avrcp_notify_rsp[device_id] = NULL;
                        app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                    }
                }
            }
#endif

            break;
        case AVRCP_EVENT_COMMAND:
#ifndef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
            TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND device_id=%d,role=%x\n",device_id,chnl->role);
            TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND ctype=%x,subunitype=%x\n", Parms->p.cmdFrame->ctype,Parms->p.cmdFrame->subunitType);
            TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND subunitId=%x,opcode=%x\n", Parms->p.cmdFrame->subunitId,Parms->p.cmdFrame->opcode);
            TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND operands=%x,operandLen=%x\n", Parms->p.cmdFrame->operands,Parms->p.cmdFrame->operandLen);
            TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND more=%x\n", Parms->p.cmdFrame->more);
            if(Parms->p.cmdFrame->ctype == AVRCP_CTYPE_STATUS)
            {
                uint32_t company_id = *(Parms->p.cmdFrame->operands+2) + ((uint32_t)(*(Parms->p.cmdFrame->operands+1))<<8) + ((uint32_t)(*(Parms->p.cmdFrame->operands))<<16);
                TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND company_id=%x\n", company_id);
                if(company_id == 0x001958)  //bt sig
                {
                    AvrcpOperation op = *(Parms->p.cmdFrame->operands+3);
                    uint8_t oplen =  *(Parms->p.cmdFrame->operands+6)+ ((uint32_t)(*(Parms->p.cmdFrame->operands+5))<<8);
                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND op=%x,oplen=%x\n", op,oplen);
                    switch(op)
                    {
                        case AVRCP_OP_GET_CAPABILITIES:
                        {
                                uint8_t event = *(Parms->p.cmdFrame->operands+7);
                                if(event==AVRCP_CAPABILITY_COMPANY_ID)
                                {
                                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support compay id");
                                }
                                else if(event == AVRCP_CAPABILITY_EVENTS_SUPPORTED)
                                {
                                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support event transId:%d", Parms->p.cmdFrame->transId);
                                    chnl->adv.eventMask = AVRCP_ENABLE_VOLUME_CHANGED;   ///volume control
                                    if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == NULL)
                                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->ctype = AVCTP_RESPONSE_IMPLEMENTED_STABLE;
                                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send support event transId:%d", app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId);
                                    AVRCP_CtGetCapabilities_Rsp(chnl,app_bt_device.avrcp_get_capabilities_rsp[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED,chnl->adv.eventMask);
                                }
                                else
                                {
                                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND send error event value");
                                }
                        }
                        break;
                    }

                }

            }else if(Parms->p.cmdFrame->ctype == AVCTP_CTYPE_CONTROL){
                TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVCTP_CTYPE_CONTROL\n");
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[3] == AVRCP_OP_SET_ABSOLUTE_VOLUME){
                    TRACE("::avrcp_callback_TG AVRCP_EID_VOLUME_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                    a2dp_volume_set(Parms->p.cmdFrame->operands[7]);
                    if (app_bt_device.avrcp_control_rsp[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_control_rsp[device_id]);
                    app_bt_device.avrcp_control_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                    app_bt_device.avrcp_control_rsp[device_id]->ctype = AVCTP_RESPONSE_ACCEPTED;
                    DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                    AVRCP_CtAcceptAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_control_rsp[device_id], Parms->p.cmdFrame->operands[7]);
                }
            }else if (Parms->p.cmdFrame->ctype == AVCTP_CTYPE_NOTIFY){
                BtStatus status;
                TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVCTP_CTYPE_NOTIFY\n");
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[7] == AVRCP_EID_VOLUME_CHANGED){
                    TRACE("::avrcp_callback_TG AVRCP_EID_VOLUME_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_notify_rsp[device_id]);
                    app_bt_device.avrcp_notify_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                    app_bt_device.avrcp_notify_rsp[device_id]->ctype = AVCTP_RESPONSE_INTERIM;
                    app_bt_device.volume_report[device_id] = AVCTP_RESPONSE_INTERIM;
                    status = AVRCP_CtGetAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());
                    TRACE("::avrcp_callback_TG AVRCP_EVENT_COMMAND AVRCP_EID_VOLUME_CHANGED nRet:%x\n",status);
                }
            }
#endif
            break;
        case AVRCP_EVENT_ADV_CMD_TIMEOUT:
            TRACE("::avrcp_callback_TG AVRCP_EVENT_ADV_CMD_TIMEOUT device_id=%d,role=%x\n",device_id,chnl->role);
            break;
    }
}
#endif
#endif

//#ifdef AVRCP_TRACK_CHANGED
//#define MAX_NUM_LETTER 20
//#endif
AvrcpMediaStatus    media_status;
int avrcp_callback_getplaystatus_ct(enum BT_DEVICE_ID_T device_id)
{
    AVRCP_CtGetPlayStatus(&app_bt_device.avrcp_channel[device_id], app_bt_device.avrcp_play_status_cmd[device_id]);
    return 0;
}

//Modified by ATX : Parke.Wei_20180518	 
static void handle_avrcp_media_status(AvrcpMediaStatus media_sta)
{
  
	switch(media_sta){
		case AVRCP_MEDIA_PLAYING:
		case AVRCP_MEDIA_FWD_SEEK:
		case AVRCP_MEDIA_REV_SEEK:
			app_bt_device.a2dp_play_pause_flag =1;
			break;
		case AVRCP_MEDIA_PAUSED:
		case AVRCP_MEDIA_STOPPED:
			app_bt_device.a2dp_play_pause_flag =0;	
			break;
		default:
		   break;
	}

}

void avrcp_callback_CT(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    TRACE("avrcp_callback_CT avrcp_callback : chnl %p, Parms %p\n", chnl, Parms);
    TRACE("::avrcp_callback_CT Parms->event %d\n", Parms->event);
#ifdef __TWS__
    AvrcpChannel *tws_TG = NULL;
#endif
#ifdef __BT_ONE_BRING_TWO__
    enum BT_DEVICE_ID_T device_id = (chnl == &app_bt_device.avrcp_channel[0])?BT_DEVICE_ID_1:BT_DEVICE_ID_2;
#else
    enum BT_DEVICE_ID_T device_id = BT_DEVICE_ID_1;
#endif
    switch(Parms->event)
    {
        case AVRCP_EVENT_CONNECT_IND:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_CONNECT_IND %d\n", Parms->event);
            AVRCP_ConnectRsp(chnl, 1);
            break;
        case AVRCP_EVENT_CONNECT:
            if(chnl->avrcpVersion >=0x103)//Modified by ATX : Parke.Wei_20180518  Enable Play_status notify
            {
                TRACE("::avrcp_callback_CT AVRCP_GET_CAPABILITY\n");
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd1[device_id]);
                AVRCP_CtGetCapabilities(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED);
            }

            app_bt_device.avrcp_channel[device_id].avrcpState = AVRCP_STATE_CONNECTED;
#ifdef AVRCP_TRACK_CHANGED
            if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd2[device_id]);
            AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],AVRCP_EID_TRACK_CHANGED,0);
#endif
            TRACE("::avrcp_callback_CT AVRCP_EVENT_CONNECT %x,device_id=%d\n", chnl->avrcpVersion,device_id);
            break;
        case AVRCP_EVENT_DISCONNECT:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_DISCONNECT");
            app_bt_device.avrcp_channel[device_id].avrcpState = AVRCP_STATE_DISCONNECTED;
            if (app_bt_device.avrcp_get_capabilities_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_control_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_control_rsp[device_id]);
                app_bt_device.avrcp_control_rsp[device_id] = NULL;
            }
            if (app_bt_device.avrcp_notify_rsp[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_notify_rsp[device_id]);
                app_bt_device.avrcp_notify_rsp[device_id] = NULL;
            }

            if (app_bt_device.avrcp_cmd1[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd1[device_id]);
                app_bt_device.avrcp_cmd1[device_id] = NULL;
            }
            if (app_bt_device.avrcp_cmd2[device_id]){
                app_a2dp_avrcpadvancedpdu_mempool_free(app_bt_device.avrcp_cmd2[device_id]);
                app_bt_device.avrcp_cmd2[device_id] = NULL;
            }
            app_bt_device.volume_report[device_id] = 0;
#ifdef AVRCP_TRACK_CHANGED
            app_bt_device.track_changed[device_id] = 0;
#endif
            break;
        case AVRCP_EVENT_RESPONSE:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_RESPONSE op=%x,status=%x\n", Parms->advOp,Parms->status);

            break;
        case AVRCP_EVENT_PANEL_CNF:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_PANEL_CNF %x,%x,%x",
                Parms->p.panelCnf.response,Parms->p.panelCnf.operation,Parms->p.panelCnf.press);
            break;
        case AVRCP_EVENT_ADV_RESPONSE:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_RESPONSE device_id=%d,role=%x\n",device_id,chnl->role);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_RESPONSE op=%x,status=%x\n", Parms->advOp,Parms->status);
            if(Parms->advOp == AVRCP_OP_GET_CAPABILITIES && Parms->status == BT_STATUS_SUCCESS)
            {
                TRACE("::avrcp_callback_CT AVRCP eventmask=%x\n", Parms->p.adv.rsp.capability.info.eventMask);
                chnl->adv.rem_eventMask = Parms->p.adv.rsp.capability.info.eventMask;
                if(chnl->adv.rem_eventMask & AVRCP_ENABLE_PLAY_STATUS_CHANGED)
                {
                    TRACE("::avrcp_callback_CT AVRCP send notification\n");
                    if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd1[device_id]);
                    AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_EID_MEDIA_STATUS_CHANGED,0);

                }
                if(chnl->adv.rem_eventMask & AVRCP_ENABLE_PLAY_POS_CHANGED)
                {
                    if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd2[device_id]);

                    AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],AVRCP_EID_PLAY_POS_CHANGED,1);

                }
            }
            else if(Parms->advOp == AVRCP_OP_REGISTER_NOTIFY && Parms->status == BT_STATUS_SUCCESS)
            {
                   if(Parms->p.adv.notify.event == AVRCP_EID_MEDIA_STATUS_CHANGED)
                   {
                        TRACE("::avrcp_callback_CT ACRCP notify rsp playback states=%x",Parms->p.adv.notify.p.mediaStatus);
                   // app_bt_device.a2dp_state = Parms->p.adv.notify.p.mediaStatus;
                   }
                   else if(Parms->p.adv.notify.event == AVRCP_EID_PLAY_POS_CHANGED)
                  {
                        TRACE("::avrcp_callback_CT ACRCP notify rsp play pos =%x",Parms->p.adv.notify.p.position);
                  }
                   else if(Parms->p.adv.notify.event == AVRCP_EID_VOLUME_CHANGED){
                        TRACE("::avrcp_callback_CT ACRCP notify rsp volume =%x",Parms->p.adv.notify.p.volume);
                        a2dp_volume_set(Parms->p.adv.notify.p.volume);
                   }
#ifdef AVRCP_TRACK_CHANGED
                    else if(Parms->p.adv.notify.event == AVRCP_EID_TRACK_CHANGED){
                     //   TRACE("::AVRCP_EID_TRACK_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                        if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                            app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_notify_rsp[device_id]);
   //                     app_bt_device.avrcp_notify_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                        app_bt_device.avrcp_notify_rsp[device_id]->ctype = AVCTP_RESPONSE_INTERIM;
                        app_bt_device.track_changed[device_id] = AVCTP_RESPONSE_INTERIM;
                        AVRCP_CtGetMediaInfo(chnl, app_bt_device.avrcp_notify_rsp[device_id],0x7f);
                    }
#endif
            }
#ifdef AVRCP_TRACK_CHANGED
            else if(Parms->advOp == AVRCP_OP_GET_MEDIA_INFO && Parms->status == BT_STATUS_SUCCESS){
                //TRACE("::AVRCP_OP_GET_MEDIA_INFO msU32=%x, lsU32=%x\n",Parms->p.adv.notify.p.track.msU32,Parms->p.adv.notify.p.track.lsU32);
                //char song_title[MAX_NUM_LETTER];
                //memcpy(song_title,Parms->p.adv.rsp.element.txt[0].string,Parms->p.adv.rsp.element.txt[0].length);
                TRACE("AVRCP_TRACK_CHANGED numid=%d",Parms->p.adv.rsp.element.numIds);
                for(uint8_t i=0;i<7;i++)
                {
                    if(Parms->p.adv.rsp.element.txt[i].length>0)
                    {
                        TRACE("Id=%d,%s\n",i,Parms->p.adv.rsp.element.txt[i].string);
                    }
                }

            }
#endif
            else if(Parms->advOp == AVRCP_OP_GET_PLAY_STATUS && Parms->status == BT_STATUS_SUCCESS){
                TRACE("::avrcp_callback AVRCP_OP_GET_PLAY_STATUS rsp %d/%d Status:%d",
                                                                                     Parms->p.adv.rsp.playStatus.position,
                                                                                     Parms->p.adv.rsp.playStatus.length,
                                                                                     Parms->p.adv.rsp.playStatus.mediaStatus);   

            }
            break;
        case AVRCP_EVENT_COMMAND:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND device_id=%d,role=%x\n",device_id,chnl->role);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND ctype=%x,subunitype=%x\n", Parms->p.cmdFrame->ctype,Parms->p.cmdFrame->subunitType);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND subunitId=%x,opcode=%x\n", Parms->p.cmdFrame->subunitId,Parms->p.cmdFrame->opcode);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND operands=%x,operandLen=%x\n", Parms->p.cmdFrame->operands,Parms->p.cmdFrame->operandLen);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND more=%x\n", Parms->p.cmdFrame->more);
            if(Parms->p.cmdFrame->ctype == AVRCP_CTYPE_STATUS)
            {
                uint32_t company_id = *(Parms->p.cmdFrame->operands+2) + ((uint32_t)(*(Parms->p.cmdFrame->operands+1))<<8) + ((uint32_t)(*(Parms->p.cmdFrame->operands))<<16);
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND company_id=%x\n", company_id);
                if(company_id == 0x001958)  //bt sig
                {
                    AvrcpOperation op = *(Parms->p.cmdFrame->operands+3);
                    uint8_t oplen =  *(Parms->p.cmdFrame->operands+6)+ ((uint32_t)(*(Parms->p.cmdFrame->operands+5))<<8);
                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND op=%x,oplen=%x\n", op,oplen);
                    switch(op)
                    {
                        case AVRCP_OP_GET_CAPABILITIES:
                        {
                                uint8_t event = *(Parms->p.cmdFrame->operands+7);
                                if(event==AVRCP_CAPABILITY_COMPANY_ID)
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support compay id");
                                }
                                else if(event == AVRCP_CAPABILITY_EVENTS_SUPPORTED)
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support event transId:%d", Parms->p.cmdFrame->transId);
#ifdef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
                                    chnl->adv.eventMask = 0;   ///volume control
#else
                                    chnl->adv.eventMask = AVRCP_ENABLE_VOLUME_CHANGED;   ///volume control
#endif
                                    if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == NULL)
                                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->ctype = AVCTP_RESPONSE_IMPLEMENTED_STABLE;
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send support event transId:%d", app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId);
                                    AVRCP_CtGetCapabilities_Rsp(chnl,app_bt_device.avrcp_get_capabilities_rsp[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED,chnl->adv.eventMask);
                                }
                                else
                                {
                                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND send error event value");
                                }
                        }
                        break;
                    }

                }

            }else if(Parms->p.cmdFrame->ctype == AVCTP_CTYPE_CONTROL){
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVCTP_CTYPE_CONTROL\n");
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[3] == AVRCP_OP_SET_ABSOLUTE_VOLUME){
                    TRACE("::avrcp_callback_CT AVRCP_EID_VOLUME_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                    a2dp_volume_set(Parms->p.cmdFrame->operands[7]);
                    if (app_bt_device.avrcp_control_rsp[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_control_rsp[device_id]);
                    app_bt_device.avrcp_control_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                    app_bt_device.avrcp_control_rsp[device_id]->ctype = AVCTP_RESPONSE_ACCEPTED;
                    DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                    AVRCP_CtAcceptAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_control_rsp[device_id], Parms->p.cmdFrame->operands[7]);
#ifdef __TWS__
               //     avrcp_set_slave_volume(Parms->p.cmdFrame->transId,Parms->p.cmdFrame->operands[7]);/* for volume up/down on phone */
               tws_player_set_a2dp_vol(Parms->p.cmdFrame->operands[7]);
#endif
                }
            }else if (Parms->p.cmdFrame->ctype == AVCTP_CTYPE_NOTIFY){
                BtStatus status;
                TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVCTP_CTYPE_NOTIFY\n");
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[7] == AVRCP_EID_VOLUME_CHANGED){
                    TRACE("::avrcp_callback_CT AVRCP_EID_VOLUME_CHANGED transId:%d\n", Parms->p.cmdFrame->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_notify_rsp[device_id]);
                    app_bt_device.avrcp_notify_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                    app_bt_device.avrcp_notify_rsp[device_id]->ctype = AVCTP_RESPONSE_INTERIM;
                    app_bt_device.volume_report[device_id] = AVCTP_RESPONSE_INTERIM;
                    status = AVRCP_CtGetAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());
                    TRACE("::avrcp_callback_CT AVRCP_EVENT_COMMAND AVRCP_EID_VOLUME_CHANGED nRet:%x\n",status);
#ifdef __TWS__
            //        avrcp_set_slave_volume(Parms->p.cmdFrame->transId,a2dp_volume_get_tws());/* for volume up/down press key */
#if 0  			//Modified by ATX : Parke.Wei_20180422  avoid duplicate set slave A2DP volume when press vol KEYS
                    tws_player_set_a2dp_vol(a2dp_volume_get_tws());
#endif
#endif
    			}
            }
            break;
#if 0
            /*For Sony Compability Consideration*/            
            case AVRCP_EVENT_PANEL_PRESS:
                TRACE("::avrcp_callback_TG AVRCP_EVENT_PANEL_PRESS %x,%x,device_id=%d",
                    Parms->p.panelCnf.operation,Parms->p.panelInd.operation,device_id);
                switch(Parms->p.panelInd.operation)
                {
                    case AVRCP_POP_VOLUME_UP:
                        TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_UP");
                        app_bt_stream_volumeup();
                       // a2dp_volume_set(Parms->p.cmdFrame->operands[7]);
                        tws_player_set_a2dp_vol(a2dp_volume_get_tws());
                        break;
                    case AVRCP_POP_VOLUME_DOWN:
                        TRACE("avrcp_callback_TG avrcp_key = AVRCP_KEY_VOLUME_DOWN"); 
                        app_bt_stream_volumedown();
                        //a2dp_volume_set(Parms->p.cmdFrame->operands[7]);
                        tws_player_set_a2dp_vol(a2dp_volume_get_tws());
                        break;
                    default :
                        break;
                }
                break;
#endif            
        case AVRCP_EVENT_ADV_NOTIFY:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_NOTIFY  adv.notify.event=%x,device_id=%d,chnl->role=%x\n",Parms->p.adv.notify.event,device_id,chnl->role);
            if(Parms->p.adv.notify.event == AVRCP_EID_VOLUME_CHANGED)
            {
                    TRACE("::avrcp_callback_CT ACRCP notify  vol =%x",Parms->p.adv.notify.p.volume);
                    AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_notify_rsp[device_id],AVRCP_EID_VOLUME_CHANGED,0);
            }
           else if(Parms->p.adv.notify.event == AVRCP_EID_MEDIA_STATUS_CHANGED)
           {
                TRACE("::avrcp_callback_CT ACRCP notify  playback states=%x",Parms->p.adv.notify.p.mediaStatus);
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd1[device_id]);
                AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_EID_MEDIA_STATUS_CHANGED,0);
           // app_bt_device.a2dp_state = Parms->p.adv.notify.p.mediaStatus;
				//Modified by ATX : Parke.Wei_20180518	 
				handle_avrcp_media_status(Parms->p.adv.notify.p.mediaStatus);

#if 0//def __TWS__  	//Modified by ATX : Parke.Wei_20180519 TWS used spp not avrcp

                media_status = Parms->p.adv.notify.p.mediaStatus;
                tws_TG = app_tws_get_avrcp_TGchannel();
                TRACE("!!!avrcp_callback_CT AVRCP_EVENT_ADV_NOTIFY  AVRCP_CtGetMediaStatus_Rsp TG to CT media_status=%x\n",media_status);
                if (app_bt_device.tws_notify_rsp == NULL){
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.tws_notify_rsp);
                }
                app_bt_device.tws_notify_rsp->transId = Parms->p.cmdFrame->transId;
                app_bt_device.tws_notify_rsp->ctype = AVCTP_RESPONSE_INTERIM;
                AVRCP_CtGetMediaStatus_Rsp(tws_TG, app_bt_device.tws_notify_rsp,media_status);
#endif
           }
           else if(Parms->p.adv.notify.event == AVRCP_EID_PLAY_POS_CHANGED)
          {
                TRACE("::avrcp_callback_CT ACRCP notify  play pos =%x",Parms->p.adv.notify.p.position);
                if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd2[device_id]);
                AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],AVRCP_EID_PLAY_POS_CHANGED,1);
          }
#ifdef AVRCP_TRACK_CHANGED
         else if(Parms->p.adv.notify.event == AVRCP_EID_TRACK_CHANGED){
            TRACE("::AVRCP notify track msU32=%x, lsU32=%x",Parms->p.adv.notify.p.track.msU32,Parms->p.adv.notify.p.track.lsU32);
            if (app_bt_device.avrcp_cmd2[device_id] == NULL)
                app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd2[device_id]);
            AVRCP_CtRegisterNotification(chnl,app_bt_device.avrcp_cmd2[device_id],AVRCP_EID_TRACK_CHANGED,0);
            }
#endif
            break;
        case AVRCP_EVENT_ADV_TX_DONE:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_TX_DONE device_id=%d,status=%x,errorcode=%x\n",device_id,Parms->status,Parms->errorCode);
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_TX_DONE op:%d, transid:%x\n", Parms->p.adv.txPdu->op,Parms->p.adv.txPdu->transId);
            break;
        case AVRCP_EVENT_ADV_CMD_TIMEOUT:
            TRACE("::avrcp_callback_CT AVRCP_EVENT_ADV_CMD_TIMEOUT device_id=%d,role=%x\n",device_id,chnl->role);
            break;
    }
}

#else
void a2dp_init(void)
{
    for(uint8_t i=0; i<BT_DEVICE_NUM; i++)
    {
        app_bt_device.a2dp_state[i]=0;
    }

    app_bt_device.a2dp_state[BT_DEVICE_ID_1]=0;
    app_bt_device.a2dp_play_pause_flag = 0;
    app_bt_device.curr_a2dp_stream_id= BT_DEVICE_ID_1;
}

void avrcp_callback(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    TRACE("avrcp_callback : chnl %p, Parms %p\n", chnl, Parms);
    TRACE("::Parms->event %d\n", Parms->event);
    switch(Parms->event)
    {
        case AVRCP_EVENT_CONNECT_IND:
            TRACE("::AVRCP_EVENT_CONNECT_IND %d\n", Parms->event);
            AVRCP_ConnectRsp(chnl, 1);
            break;
        case AVRCP_EVENT_CONNECT:
            TRACE("::AVRCP_EVENT_CONNECT %d\n", Parms->event);
            break;
        case AVRCP_EVENT_RESPONSE:
            TRACE("::AVRCP_EVENT_RESPONSE %d\n", Parms->event);

            break;
        case AVRCP_EVENT_PANEL_CNF:
            TRACE("::AVRCP_EVENT_PANEL_CNF %x,%x,%x",
                Parms->p.panelCnf.response,Parms->p.panelCnf.operation,Parms->p.panelCnf.press);
#if 0
            if((Parms->p.panelCnf.response == AVCTP_RESPONSE_ACCEPTED) && (Parms->p.panelCnf.press == TRUE))
            {
                AVRCP_SetPanelKey(chnl,Parms->p.panelCnf.operation,FALSE);
            }
#endif
            break;
    }
}
#endif
//void avrcp_init(void)
//{
//  hal_uart_open(HAL_UART_ID_0,NULL);
//    TRACE("avrcp_init...OK\n");
//}
//

#if defined(A2DP_AAC_ON)
//Modified by ATX : Leon.He_20180622: move AAC bitrate into app_bt.h
const unsigned char a2dp_codec_aac_elements[A2DP_AAC_OCTET_NUMBER] =
{
    A2DP_AAC_OCTET0_MPEG2_AAC_LC,
    A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100,
    A2DP_AAC_OCTET2_CHANNELS_1|A2DP_AAC_OCTET2_CHANNELS_2 |A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000,
    A2DP_AAC_OCTET3_VBR_SUPPORTED | ((MAX_AAC_BITRATE >> 16)&0x7f),
    /* left bit rate 0 for unkown */
    (MAX_AAC_BITRATE >> 8) & 0xff,
    (MAX_AAC_BITRATE)& 0xff
};

const unsigned char a2dp_codec_aac_elements_limited[A2DP_AAC_OCTET_NUMBER] =
{
    A2DP_AAC_OCTET0_MPEG2_AAC_LC,
    A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100,
    A2DP_AAC_OCTET2_CHANNELS_1|A2DP_AAC_OCTET2_CHANNELS_2 |A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000,
     A2DP_AAC_OCTET3_VBR_SUPPORTED | ((MAX_AAC_BITRATE_192K >> 16)&0x7f),
    /* left bit rate 0 for unkown */
    (MAX_AAC_BITRATE_192K >> 8) & 0xff,
    (MAX_AAC_BITRATE_192K)& 0xff
};

AvdtpCodec a2dp_aac_avdtpcodec;
#endif

#if defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
const unsigned char a2dp_codec_opus_elements[] =
{
    A2D_SBC_IE_SAMP_FREQ_48 | A2D_SBC_IE_SAMP_FREQ_44 | A2D_SBC_IE_CH_MD_STEREO | A2D_SBC_IE_CH_MD_JOINT,
    A2D_SBC_IE_BLOCKS_16 | A2D_SBC_IE_BLOCKS_12 | A2D_SBC_IE_SUBBAND_8 | A2D_SBC_IE_ALLOC_MD_L,
    A2D_SBC_IE_MIN_BITPOOL,
    BTA_AV_CO_SBC_MAX_BITPOOL
};
#endif


const unsigned char a2dp_codec_elements[] =
{
    A2D_SBC_IE_SAMP_FREQ_48 | A2D_SBC_IE_SAMP_FREQ_44 | A2D_SBC_IE_CH_MD_STEREO | A2D_SBC_IE_CH_MD_JOINT,
    A2D_SBC_IE_BLOCKS_16 | A2D_SBC_IE_BLOCKS_12 | A2D_SBC_IE_SUBBAND_8 | A2D_SBC_IE_ALLOC_MD_L,
    A2D_SBC_IE_MIN_BITPOOL,
    BTA_AV_CO_SBC_MAX_BITPOOL
};

int store_sbc_buffer(unsigned char *buf, unsigned int len);
int a2dp_audio_sbc_set_frame_info(int rcv_len, int frame_num);


#if defined(__BT_RECONNECT__) && defined(__BT_A2DP_RECONNECT__)
struct BT_A2DP_RECONNECT_T bt_a2dp_reconnect;

extern "C" void cancel_a2dp_reconnect(void)
{
    if(bt_a2dp_reconnect.TimerNotifyFunc) {
        osTimerStop(bt_a2dp_reconnect.TimerID);
        osTimerDelete(bt_a2dp_reconnect.TimerID);
        bt_a2dp_reconnect.TimerNotifyFunc= 0;
    }
}

void reconnect_a2dp_stream(enum BT_DEVICE_ID_T stream_id)
{
    A2dpStream *Stream;

    TRACE("a2dp_reconnect:\n");
    Stream = bt_a2dp_reconnect.copyStream[stream_id];
    A2DP_OpenStream(Stream, &bt_a2dp_reconnect.copyAddr[stream_id]);
}

void a2dp_reconnect1(void)
{
    reconnect_a2dp_stream(BT_DEVICE_ID_1);
}

#ifdef __BT_ONE_BRING_TWO__
void a2dp_reconnect2(void)
{
    reconnect_a2dp_stream(BT_DEVICE_ID_2);
}
#endif

void a2dp_reconnect_timer_callback(void const *n) {
    if(bt_a2dp_reconnect.TimerNotifyFunc)
        bt_a2dp_reconnect.TimerNotifyFunc();
}
osTimerDef(a2dp_reconnect_timer, a2dp_reconnect_timer_callback);
#endif



void btapp_send_pause_key(enum BT_DEVICE_ID_T stream_id)
{
    TRACE("btapp_send_pause_key id = %x",stream_id);
    AVRCP_SetPanelKey(&(app_bt_device.avrcp_channel[stream_id]),AVRCP_POP_PAUSE,TRUE);
    AVRCP_SetPanelKey(&(app_bt_device.avrcp_channel[stream_id]),AVRCP_POP_PAUSE,FALSE);
 //   app_bt_device.a2dp_play_pause_flag = 0;
}


void btapp_a2dp_suspend_music(enum BT_DEVICE_ID_T stream_id)
{
    TRACE("btapp_a2dp_suspend_music id = %x",stream_id);

    btapp_send_pause_key(stream_id);
}



extern enum AUD_SAMPRATE_T a2dp_sample_rate;

#define A2DP_TIMESTAMP_DEBOUNCE_DURATION (1000)
#define A2DP_TIMESTAMP_MODE_SAMPLE_THRESHOLD (2000)

#define A2DP_TIMESTAMP_SYNC_LIMIT_CNT (200)
#define A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD (100)
#define A2DP_TIMESTAMP_SYNC_SAMPLE_THRESHOLD (a2dp_sample_rate/1000*A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD)

#define RICE_THRESHOLD
#define RICE_THRESHOLD

struct A2DP_TIMESTAMP_INFO_T{
    uint16_t rtp_timestamp;
    uint32_t loc_timestamp;
    uint16_t frame_num;
};

enum A2DP_TIMESTAMP_MODE_T{
    A2DP_TIMESTAMP_MODE_NONE,
    A2DP_TIMESTAMP_MODE_SAMPLE,
    A2DP_TIMESTAMP_MODE_TIME,
};

enum A2DP_TIMESTAMP_MODE_T a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_NONE;

struct A2DP_TIMESTAMP_INFO_T a2dp_timestamp_pre = {0,0,0};
bool a2dp_timestamp_parser_need_sync = false;
int a2dp_timestamp_parser_init(void)
{
    a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_NONE;
    a2dp_timestamp_pre.rtp_timestamp = 0;
    a2dp_timestamp_pre.loc_timestamp = 0;
    a2dp_timestamp_pre.frame_num = 0;
    a2dp_timestamp_parser_need_sync = false;
    return 0;
}

int a2dp_timestamp_parser_needsync(void)
{
    a2dp_timestamp_parser_need_sync = true;
    return 0;
}

int a2dp_timestamp_parser_run(uint16_t timestamp, uint16_t framenum)
{
    static int skip_cnt = 0;
    struct A2DP_TIMESTAMP_INFO_T curr_timestamp;
    int skipframe = 0;
    int16_t rtpdiff;
    int16_t locdiff;
    bool needsave_timestamp = true;

    curr_timestamp.rtp_timestamp = timestamp;
    curr_timestamp.loc_timestamp = hal_sys_timer_get();
    curr_timestamp.frame_num = framenum;

    switch(a2dp_timestamp_mode) {
        case A2DP_TIMESTAMP_MODE_NONE:

            TRACE("parser rtp:%d loc:%d num:%d prertp:%d preloc:%d\n", curr_timestamp.rtp_timestamp, curr_timestamp.loc_timestamp, curr_timestamp.frame_num,
                                                   a2dp_timestamp_pre.rtp_timestamp, a2dp_timestamp_pre.loc_timestamp);
            if (a2dp_timestamp_pre.rtp_timestamp){
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                if (TICKS_TO_MS(locdiff) > A2DP_TIMESTAMP_DEBOUNCE_DURATION){
                    rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                    if (ABS((int16_t)TICKS_TO_MS(locdiff)-rtpdiff)>A2DP_TIMESTAMP_MODE_SAMPLE_THRESHOLD){
                        a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_SAMPLE;
                        TRACE("A2DP_TIMESTAMP_MODE_SAMPLE\n");
                    }else{
                        a2dp_timestamp_mode = A2DP_TIMESTAMP_MODE_TIME;
                        TRACE("A2DP_TIMESTAMP_MODE_TIME\n");
                    }
                }else{
                    needsave_timestamp = false;
                }
            }
            break;
        case A2DP_TIMESTAMP_MODE_SAMPLE:
            if (a2dp_timestamp_parser_need_sync){
                skip_cnt++;
                rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                TRACE("A2DP_TIMESTAMP_MODE_TIME SYNC diff:%d cnt:%d\n", ABS((int16_t)(MS_TO_TICKS(locdiff)*(a2dp_sample_rate/1000)) - rtpdiff), skip_cnt);
                if ((ABS((int16_t)(TICKS_TO_MS(locdiff)*(a2dp_sample_rate/1000)) - rtpdiff)) < (uint32_t)A2DP_TIMESTAMP_SYNC_SAMPLE_THRESHOLD){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else if (skip_cnt > A2DP_TIMESTAMP_SYNC_LIMIT_CNT){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else{
                    needsave_timestamp = false;
                    skipframe = 1;
                }
            }
            break;
        case A2DP_TIMESTAMP_MODE_TIME:
            if (a2dp_timestamp_parser_need_sync){
                skip_cnt++;
                rtpdiff = curr_timestamp.rtp_timestamp - a2dp_timestamp_pre.rtp_timestamp;
                locdiff = curr_timestamp.loc_timestamp - a2dp_timestamp_pre.loc_timestamp;
                TRACE("A2DP_TIMESTAMP_MODE_TIME SYNC diff:%d cnt:%d\n", ABS((int16_t)TICKS_TO_MS(locdiff) - rtpdiff), skip_cnt);
                if (ABS((int16_t)TICKS_TO_MS(locdiff) - rtpdiff) < A2DP_TIMESTAMP_SYNC_TIME_THRESHOLD){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else if (skip_cnt > A2DP_TIMESTAMP_SYNC_LIMIT_CNT){
                    skip_cnt = 0;
                    a2dp_timestamp_parser_need_sync = false;
                }else{
                    needsave_timestamp = false;
                    skipframe = 1;
                }
            }
            break;
    }

    if (needsave_timestamp){
        a2dp_timestamp_pre.rtp_timestamp = curr_timestamp.rtp_timestamp;
        a2dp_timestamp_pre.loc_timestamp = curr_timestamp.loc_timestamp;
    }
#if 0
    if (rtpdiff<=0){
        a2dp_timestamp_parser_init();
        a2dp_timestamp_parser_need_sync = false;
        skipframe = 0;
    }
 #endif
    return skipframe;
}

static struct BT_DEVICE_ID_DIFF stream_id_flag;
#ifdef __BT_ONE_BRING_TWO__
void a2dp_stream_id_distinguish(A2dpStream *Stream)
{
    if(Stream == &app_bt_device.a2dp_stream[BT_DEVICE_ID_1]){
        stream_id_flag.id = BT_DEVICE_ID_1;
        stream_id_flag.id_other = BT_DEVICE_ID_2;
    }else if(Stream == &app_bt_device.a2dp_stream[BT_DEVICE_ID_2]){
        stream_id_flag.id = BT_DEVICE_ID_2;
        stream_id_flag.id_other = BT_DEVICE_ID_1;
    }
}
#endif


#if defined( __EARPHONE__) && defined(__BT_RECONNECT__)

#ifdef __BT_ONE_BRING_TWO__
extern BtDeviceRecord record2_copy;
extern uint8_t record2_avalible;
#endif

#endif


#if defined(__TWS__)
    static AvdtpCodec   setconfig_codec;
 //   static u8 tmp_element[10];
#endif
uint8_t app_tws_get_mode(void);

void a2dp_set_config_codec(AvdtpCodec *config_codec,const A2dpCallbackParms *Info)
{
    static u8 codec_element[10];

    config_codec->codecType = Info->p.codec->codecType;
    config_codec->discoverable = Info->p.codec->discoverable;
    config_codec->elemLen = Info->p.codec->elemLen;
    config_codec->elements = codec_element;
#if defined(A2DP_AAC_ON)
    if(Info->p.codec->codecType == AVDTP_CODEC_TYPE_SBC)
#endif
    {
        config_codec->elements[0] = (Info->p.codec->elements[0]) & (a2dp_codec_elements[0]);
        config_codec->elements[1] = (Info->p.codec->elements[1]) & (a2dp_codec_elements[1]);

        if(Info->p.codec->elements[2] <= a2dp_codec_elements[2])
            config_codec->elements[2] = a2dp_codec_elements[2];////[2]:MIN_BITPOOL
        else
            config_codec->elements[2] = Info->p.codec->elements[2];

        if(Info->p.codec->elements[3] >= a2dp_codec_elements[3])
            config_codec->elements[3] = a2dp_codec_elements[3];////[3]:MAX_BITPOOL
        else
            config_codec->elements[3] = Info->p.codec->elements[3];

        ///////null set situation:
        if(config_codec->elements[3] < a2dp_codec_elements[2]){
            config_codec->elements[2] = a2dp_codec_elements[2];
            config_codec->elements[3] = a2dp_codec_elements[3];
        }
        else if(config_codec->elements[2] > a2dp_codec_elements[3]){
            config_codec->elements[2] = a2dp_codec_elements[3];
            config_codec->elements[3] = a2dp_codec_elements[3];
        }
        TRACE("!!!config_codec.elements[2]:%d,config_codec.elements[3]:%d\n",config_codec->elements[2],config_codec->elements[3]);

        config_codec->elements[0] = get_valid_bit(config_codec->elements[0],A2D_SBC_IE_SAMP_FREQ_MSK);
        config_codec->elements[0] = get_valid_bit(config_codec->elements[0],A2D_SBC_IE_CH_MD_MSK);
        config_codec->elements[1] = get_valid_bit(config_codec->elements[1],A2D_SBC_IE_BLOCKS_MSK);
        config_codec->elements[1] = get_valid_bit(config_codec->elements[1],A2D_SBC_IE_SUBBAND_MSK);
        config_codec->elements[1] = get_valid_bit(config_codec->elements[1],A2D_SBC_IE_ALLOC_MD_MSK);
    }
#if defined(A2DP_AAC_ON)
    else if(Info->p.codec->codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC){

        config_codec->elements[0] = (Info->p.codec->elements[0]) & (a2dp_codec_aac_elements[0]);
        config_codec->elements[1] = (Info->p.codec->elements[1]) & (a2dp_codec_aac_elements[1]);
        config_codec->elements[2] = (Info->p.codec->elements[2]) & (a2dp_codec_aac_elements[2]);
        if (true==app_bt_device.app_aac_limited)
        {
            TRACE("#####aac limited!!!!");
            config_codec->elements[3] = (Info->p.codec->elements[3]) & (a2dp_codec_aac_elements_limited[3]);
            config_codec->elements[4] = (Info->p.codec->elements[4]) & (a2dp_codec_aac_elements_limited[4]);
            config_codec->elements[5] = (Info->p.codec->elements[5]) & (a2dp_codec_aac_elements_limited[5]);
        }
        else
        {
            TRACE("#####aac default setting!!!!");
            config_codec->elements[3] = (Info->p.codec->elements[3]) & (a2dp_codec_aac_elements[3]);
            config_codec->elements[4] = (Info->p.codec->elements[4]) & (a2dp_codec_aac_elements[4]);
            config_codec->elements[5] = (Info->p.codec->elements[5]) & (a2dp_codec_aac_elements[5]);
        }
    }
#endif
}


uint8_t enum_sample_rate_transfer_to_codec_info(enum AUD_SAMPRATE_T sample_rate)
{
    uint8_t codec_info;
    TRACE("!!!enum_sample_rate_transfer_to_codec_info  sample_rate :%d \n",sample_rate);
    switch (sample_rate)
    {
        case AUD_SAMPRATE_16000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_16;
            break;
        case AUD_SAMPRATE_32000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_32;
            break;
        case AUD_SAMPRATE_48000:
            codec_info = A2D_SBC_IE_SAMP_FREQ_48;
            break;
        case AUD_SAMPRATE_44100:
            codec_info = A2D_SBC_IE_SAMP_FREQ_44;
            break;
        default:
            ASSERT(0, "[%s] 0x%x is invalid", __func__, sample_rate);
            break;
    }
    TRACE("!!!enum_sample_rate_transfer_to_codec_info  codec_info :%x\n",codec_info);
    return codec_info;
}
#if defined(__TWS__)
BOOL app_bt_bypass_hcireceiveddata_cb(void);

void app_a2dp_buff_remaining_monitor(void)
{
    if (app_tws_get_a2dpbuff_available() < 1024){
//        TRACE("!!!suspend HciBypassProcessReceivedData");
        HciBypassProcessReceivedDataExt(app_bt_bypass_hcireceiveddata_cb);
    }
}
#endif

//#if defined(A2DP_AAC_ON)
AvdtpCodecType a2dp_get_codec_type(enum BT_DEVICE_ID_T id)
{
    return app_bt_device.codec_type[id];
}

AvdtpCodecType a2dp_get_sample_rate(enum BT_DEVICE_ID_T id)
{
    return app_bt_device.sample_rate[id];
}
//#endif

extern void app_bt_profile_connect_manager_a2dp(enum BT_DEVICE_ID_T id, A2dpStream *Stream, const A2dpCallbackParms *Info);


#ifdef __AAC_ERROR_RESUME__
uint8_t sample_rate_update;
#endif


void app_avrcp_connect_timeout_handler(void const *param)
{
    TRACE("app_avrcp_connect_timeout_handler a2dp state=%d,avrcp state=%d",app_bt_device.a2dp_state[0],app_bt_device.avrcp_channel[0].avrcpState);
    if(app_bt_device.a2dp_state[0] == 1 && app_bt_device.avrcp_channel[0].avrcpState != AVRCP_STATE_CONNECTED)
    {
//Modified by ATX : Leon.He_20180525:fix iphone failed to reconnect AVRCP
#if defined(A2DP_AAC_ON)
        if(app_bt_device.a2dp_aac_stream.stream.conn.remDev)
        {
            TRACE("start connect avrcp with AAC");
            AVRCP_Connect(&app_bt_device.avrcp_channel[0], &app_bt_device.a2dp_aac_stream.stream.conn.remDev->bdAddr);
        }
#endif
        if(app_bt_device.a2dp_stream[0].stream.conn.remDev)
        {
            TRACE("start connect avrcp");
            AVRCP_Connect(&app_bt_device.avrcp_channel[0], &app_bt_device.a2dp_stream[0].stream.conn.remDev->bdAddr);
        }
    }
}

osTimerDef (APP_AVRCP_CONNECT, app_avrcp_connect_timeout_handler);
static osTimerId POSSIBLY_UNUSED app_avrcp_connect_timer = NULL;
int hfp_volume_get(void);

void a2dp_callback(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
    int header_len = 0;
    AvdtpMediaHeader header;
    static uint32_t last_seq = 0;
    int skip_frame=0;
#ifdef __A2DP_AVDTP_CP__    
    static AvdtpContentProt   setconfig_cp;
#endif
    
#if defined(__TWS__)
    A2dpStream * tws_source_stream = NULL;
    AvdtpCodec tws_set_codec;
    enum AUD_SAMPRATE_T a2dp_sample_rate;
#else
#if defined(__BT_RECONNECT__)
    static AvdtpCodec   setconfig_codec;
    static u8 tmp_element[10];
#endif
#endif
#if defined(__TWS__)
    unsigned int sbc_frame_num = 0;
if (tws_a2dp_callback(Stream, Info))
{
    return;
}
#endif

#ifdef __BT_ONE_BRING_TWO__
    a2dp_stream_id_distinguish(Stream);
#else
    stream_id_flag.id = BT_DEVICE_ID_1;
#endif

    switch(Info->event) {
        case A2DP_EVENT_AVDTP_CONNECT:
            TRACE("::A2DP_EVENT_AVDTP_CONNECT %d\n", Info->event);
            break;
        case A2DP_EVENT_STREAM_OPEN:
            TRACE("::A2DP_EVENT_STREAM_OPEN stream=%x,stream_id:%d, sample_rate codec.elements 0x%x\n", Stream,stream_id_flag.id,Info->p.configReq->codec.elements[0]);
#ifdef __A2DP_AVDTP_CP__
            A2DP_SecurityControlReq(Stream,(U8 *)&a2dp_avdtpCp_securityData,1);
#endif

            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_SBC)
            {
#if defined(A2DP_AAC_ON)
                app_bt_device.codec_type[stream_id_flag.id] = AVDTP_CODEC_TYPE_SBC;
#endif
                memcpy(app_bt_device.a2dp_codec_elements[stream_id_flag.id],Info->p.configReq->codec.elements,4);
            }
#if defined(A2DP_AAC_ON)
            else if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
                TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate codec.elements 0x%x\n", stream_id_flag.id,Info->p.configReq->codec.elements[1]);
                app_bt_device.codec_type[stream_id_flag.id] = AVDTP_CODEC_TYPE_MPEG2_4_AAC;
                // convert aac sample_rate to sbc sample_rate format
                if (Info->p.configReq->codec.elements[1] & A2DP_AAC_OCTET1_SAMPLING_FREQUENCY_44100) {
                    TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 44100\n", stream_id_flag.id);
                    app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                }
                else if (Info->p.configReq->codec.elements[2] & A2DP_AAC_OCTET2_SAMPLING_FREQUENCY_48000) {
                    TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate 48000\n", stream_id_flag.id);
                    app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_48;
                }
                else {
                    TRACE("::A2DP_EVENT_STREAM_OPEN stream_id:%d, aac sample_rate not 48000 or 44100, set to 44100\n", stream_id_flag.id);
                    app_bt_device.sample_rate[stream_id_flag.id] = A2D_SBC_IE_SAMP_FREQ_44;
                }
            }
#endif
#ifdef __TWS__
            if (app_tws_get_mode() != TWSSLAVE)
#endif
                app_bt_profile_connect_manager_a2dp(stream_id_flag.id, Stream, (A2dpCallbackParms *)Info);
#ifdef __TWS__
            if(app_tws_get_mode() == TWSSLAVE || app_tws_get_mode() == TWSMASTER)
            {
                TRACE("STREAM OPEN TWSSLAVE OR MASTER\n");
                app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, BAM_NOT_ACCESSIBLE,0,0);
            }

            tws_source_stream = app_tws_get_tws_source_stream();
            tws_set_codec = app_tws_get_set_config_codec_info();
            TRACE("a2dp_callback moblie codec info = %x,%x,%x,%x",Info->p.codec->elements[0],Info->p.codec->elements[1],
                Info->p.codec->elements[2],Info->p.codec->elements[3]);
            TRACE("a2dp_callback stream codec info = %x,%x,%x,%x",Stream->stream.codec->elements[0],Stream->stream.codec->elements[1],
                Stream->stream.codec->elements[2],Stream->stream.codec->elements[3]);


            if(app_tws_get_source_stream_connect_status()){
                TRACE("!!!Info->p.codec->elements[0]:%x,tws_set_codec.elements[0]:%x\n",Info->p.codec->elements[0],tws_set_codec.elements[0]);
                TRACE("!!!source codec element[0]:%x\n",tws_source_stream->stream.codecCfg.elements[0]) ;
#if defined(A2DP_AAC_ON)
                if(app_bt_device.codec_type[stream_id_flag.id] == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                    uint8_t tws_source_sample_rate = tws_source_stream->stream.codecCfg.elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                    if(app_bt_device.sample_rate[stream_id_flag.id] != tws_source_sample_rate){
                        tws_source_stream->stream.codecCfg.elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                        tws_source_stream->stream.codecCfg.elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                        A2DP_ReconfigStream(tws_source_stream, &tws_source_stream->stream.codecCfg,NULL);
                    }
                }else
#endif
                {
                     if(((tws_source_stream->stream.codecCfg.elements[0]>>4)&0xf) != ((Info->p.codec->elements[0]>>4)&0xf)){
                        TRACE("!!!A2DP_EVENT_STREAM_OPEN  A2DP_ReconfigStream\n");
                        A2DP_ReconfigStream(tws_source_stream, Info->p.codec,NULL);
                    }
                }
            }

            if(app_tws_get_mode() == TWSMASTER || app_tws_get_mode() == TWSON)
            {
                TRACE("AVRCP CONNECT FOR MOBILE");
         //       AVRCP_Connect(&app_bt_device.avrcp_channel[stream_id_flag.id], &Stream->stream.conn.remDev->bdAddr);

                if (app_avrcp_connect_timer == NULL)
                    app_avrcp_connect_timer = osTimerCreate(osTimer(APP_AVRCP_CONNECT), osTimerOnce, NULL);  
                osTimerStop(app_avrcp_connect_timer);
                osTimerStart(app_avrcp_connect_timer, 500);//Modified by ATX : Leon.He_20180525:change delay to 300ms
   
            }
#endif
            app_bt_stream_volume_ptr_update((uint8_t *)Stream->stream.conn.remDev->bdAddr.addr);

#ifdef __TWS__
            if(app_tws_get_mode() == TWSMASTER)
            {
              //  avrcp_set_slave_volume(0,a2dp_volume_get_tws());
                tws_player_set_a2dp_vol(a2dp_volume_get_tws());
                tws_player_set_hfp_vol(hfp_volume_get()); 
            }
#endif

//            app_bt_stream_a2dpvolume_reset();
#if defined(A2DP_AAC_ON)
            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_SBC)
#endif
            {
                app_bt_device.sample_rate[stream_id_flag.id] = (Info->p.configReq->codec.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK);
            }
            app_bt_device.a2dp_state[stream_id_flag.id] = 1;
#ifdef __BT_ONE_BRING_TWO__
            if(app_bt_device.a2dp_stream[stream_id_flag.id_other].stream.state != AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id;
            }
#endif

#if FPGA==0
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
        app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
#endif
#ifndef __TWS__
            AVRCP_Connect(&app_bt_device.avrcp_channel[stream_id_flag.id], &Stream->stream.conn.remDev->bdAddr);
#endif
            break;
        case A2DP_EVENT_STREAM_OPEN_IND:
            TRACE("::A2DP_EVENT_STREAM_OPEN_IND %d\n", Info->event);
            A2DP_OpenStreamRsp(Stream, A2DP_ERR_NO_ERROR, AVDTP_SRV_CAT_MEDIA_TRANSPORT);
            break;
        case A2DP_EVENT_STREAM_STARTED:
            TRACE("::A2DP_EVENT_STREAM_STARTED  stream_id:%d\n", stream_id_flag.id);
#if 1

#ifdef __TWS__
            if(app_tws_get_mode()  != TWSSLAVE)
                a2dp_timestamp_parser_init();
#else
                a2dp_timestamp_parser_init();
#endif
#endif
#ifndef __TWS__
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_SBC,stream_id_flag.id,MAX_RECORD_NUM,0,0);
#endif
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 1;
            break;
        case A2DP_EVENT_STREAM_START_IND:
            TRACE("::A2DP_EVENT_STREAM_START_IND stream=%x stream_id:%d\n",Stream, stream_id_flag.id);
#if defined(__TWS__)
            app_tws_set_media_suspend(false);
#endif
#ifdef __BT_ONE_BRING_TWO__
            A2DP_StartStreamRsp(&app_bt_device.a2dp_stream[stream_id_flag.id], A2DP_ERR_NO_ERROR);
            if(app_bt_device.a2dp_stream[stream_id_flag.id_other].stream.state != AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id;
            }
#else
#if defined(__TWS__)
            if(app_tws_get_mode()  != TWSMASTER)
#endif
            A2DP_StartStreamRsp(Stream, A2DP_ERR_NO_ERROR);
#endif
#if defined(__TWS__)
            if(Stream->device->delayReporting)
                A2DP_SetSinkDelay(&app_bt_device.a2dp_stream[stream_id_flag.id],300);
#endif
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            TRACE("::A2DP_EVENT_STREAM_SUSPENDED  stream_id:%d\n", stream_id_flag.id);
#ifdef __BT_ONE_BRING_TWO__
            if(app_bt_device.a2dp_stream[stream_id_flag.id_other].stream.state == AVDTP_STRM_STATE_STREAMING){
                app_bt_device.curr_a2dp_stream_id = stream_id_flag.id_other;
                app_bt_device.a2dp_play_pause_flag = 1;
            }else{
                app_bt_device.a2dp_play_pause_flag = 0;
            }
#if 1
#ifdef __TWS__
            if(app_tws_get_mode()  != TWSSLAVE)
                a2dp_timestamp_parser_init();
#else
                a2dp_timestamp_parser_init();
#endif
#endif
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
#else
#ifndef __TWS__
            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,BT_DEVICE_ID_1,0,0,0);
#endif
            app_bt_device.a2dp_play_pause_flag = 0;
#endif
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 0;
            break;
        case A2DP_EVENT_STREAM_DATA_IND:




#ifdef __BT_ONE_BRING_TWO__
                ////play music of curr_a2dp_stream_id
                if(app_bt_device.curr_a2dp_stream_id  == stream_id_flag.id &&
                   app_bt_device.hf_audio_state[stream_id_flag.id]==HF_AUDIO_DISCON &&
                   app_bt_device.hf_audio_state[stream_id_flag.id_other]==HF_AUDIO_DISCON &&
                   app_bt_device.hfchan_callSetup[stream_id_flag.id_other] == HF_CALL_SETUP_NONE){
                    header_len = AVDTP_ParseMediaHeader(&header, Info->p.data);
                    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC) && (Stream->stream.state == AVDTP_STRM_STATE_STREAMING)){
                        a2dp_audio_sbc_set_frame_info(Info->len - header_len - 1, (*(((unsigned char *)Info->p.data) + header_len)));
                        store_sbc_buffer(((unsigned char *)Info->p.data) + header_len + 1 , Info->len - header_len - 1);
                    }
                }
#else
                header_len = AVDTP_ParseMediaHeader(&header, Info->p.data);
#if 1
#ifdef __TWS__
            if(app_tws_get_mode()  != TWSSLAVE)
                skip_frame = a2dp_timestamp_parser_run(header.timestamp,(*(((unsigned char *)Info->p.data) + header_len)));
#else
                skip_frame = a2dp_timestamp_parser_run(header.timestamp,(*(((unsigned char *)Info->p.data) + header_len)));
#endif

#ifdef __TWS__
            if(0)//(skip_frame && (app_tws_get_mode()  != TWSSLAVE))
            {
                TRACE("skip sbc packet!!");
                return;
            }
#else
            if(skip_frame)
            {
                TRACE("skip sbc packet!!");
                return;
            }
#endif
#endif
#ifdef __TWS__
                if(app_tws_get_mode() == TWSSLAVE){
#if defined(A2DP_AAC_ON)
                    if(a2dp_get_codec_type(stream_id_flag.id) == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                        TRACE("aac time=%d,seq=%d \n",header.timestamp,header.sequenceNumber);
                    }else
#endif
                    {
                        TRACE("time=%d,seq=%d,framnumber=%d,curr t=%d",header.timestamp,header.sequenceNumber,*(((unsigned char *)Info->p.data) + header_len),bt_syn_get_curr_ticks(app_tws_get_tws_conhdl()));
                    }
                }
                if (header.sequenceNumber != last_seq + 1) {
                    TRACE("media packet last_seq=%d,curr_seq=%d\n", last_seq,header.sequenceNumber);
                }
                last_seq = header.sequenceNumber;
                sbc_frame_num = (*(((unsigned char *)Info->p.data) + header_len));
#ifdef __AAC_ERROR_RESUME__

#if defined(A2DP_AAC_ON)
                if(app_tws_get_mode() == TWSMASTER || app_tws_get_mode() == TWSON)
                {
                    if(a2dp_get_codec_type(stream_id_flag.id) == AVDTP_CODEC_TYPE_MPEG2_4_AAC && app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
                    {
                        if(*(uint8_t *)(((unsigned char *)Info->p.data) + header_len)==0x9c) 
                        {
                            TRACE("ERROR !!SBC IN AAC STREAM~~~");
#if 0                            
                            sample_rate_update = *(uint8_t *)(((unsigned char *)Info->p.data) + header_len+1)>>6;

                            tws_source_stream = app_tws_get_tws_source_stream();
                            if(app_tws_get_source_stream_connect_status()){
                                TRACE("!!!source codec element[0]:%x\n",tws_source_stream->stream.codecCfg.elements[0]) ;
                                if(app_bt_device.codec_type[stream_id_flag.id] == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                                    uint8_t tws_source_sample_rate = tws_source_stream->stream.codecCfg.elements[0] &= A2D_SBC_IE_SAMP_FREQ_MSK;
                                    if(sample_rate_update == 3)sample_rate_update = 0x10;
                                    if(sample_rate_update == 2)sample_rate_update = 0x20;
                                    if(sample_rate_update != tws_source_sample_rate){
                                        tws_source_stream->stream.codecCfg.elements[0] &= ~A2D_SBC_IE_SAMP_FREQ_MSK;
                                        tws_source_stream->stream.codecCfg.elements[0] |= app_bt_device.sample_rate[stream_id_flag.id];
                                        A2DP_ReconfigStream(tws_source_stream, &tws_source_stream->stream.codecCfg,NULL);
                                    }
                                }
                           }
#endif
                                 app_tws_set_codec_type(AVDTP_CODEC_TYPE_SBC);
                                 tws_player_set_codec(AVDTP_CODEC_TYPE_SBC);        

                            
                        break;
                            
                    }

                 }
             }
#endif

#endif
#if defined(__TWS_CLK_SYNC__)
#ifdef __TWS__
                if (app_tws_get_mode() == TWSSLAVE)
                {
                    tws_store_remote_lock_time(header);
                }
#endif
#endif
#if defined(A2DP_AAC_ON)
                if(a2dp_get_codec_type(stream_id_flag.id) == AVDTP_CODEC_TYPE_MPEG2_4_AAC
#if defined (A2DP_AAC_DIRECT_TRANSFER)
                    || (app_tws_mode_is_slave() && (app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC))
#endif                    
                    ) {
                    tws_store_sbc_buffer(((unsigned char *)Info->p.data) + header_len , Info->len - header_len, sbc_frame_num);
                }
                else
#endif
                {
                    a2dp_audio_sbc_set_frame_info(Info->len - header_len - 1, (*(((unsigned char *)Info->p.data) + header_len)));
                    tws_store_sbc_buffer(((unsigned char *)Info->p.data) + header_len + 1 , Info->len - header_len - 1, sbc_frame_num);
                }
                app_a2dp_buff_remaining_monitor();
#else
                if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC) && (Stream->stream.state == AVDTP_STRM_STATE_STREAMING)){
                    a2dp_audio_sbc_set_frame_info(Info->len - header_len - 1, (*(((unsigned char *)Info->p.data) + header_len)));
#if defined(A2DP_AAC_ON)
                    if(a2dp_get_codec_type(stream_id_flag.id) == AVDTP_CODEC_TYPE_MPEG2_4_AAC) {
                        store_sbc_buffer(((unsigned char *)Info->p.data) + header_len , Info->len - header_len);
                    }
                    else
#endif
                    {
                        store_sbc_buffer(((unsigned char *)Info->p.data) + header_len + 1 , Info->len - header_len - 1);
                    }
                }
#endif
#endif
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            TRACE("::A2DP_EVENT_STREAM_CLOSED stream_id:%d, reason = %x\n", stream_id_flag.id,Info->discReason);
#ifdef __TWS__
            if (app_tws_get_mode() != TWSSLAVE)
#endif            
                app_bt_profile_connect_manager_a2dp(stream_id_flag.id, Stream, (A2dpCallbackParms *)Info);
//            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
#if 1

#ifdef __TWS__
            if(app_tws_get_mode()  != TWSSLAVE)
                a2dp_timestamp_parser_init();
#else
                a2dp_timestamp_parser_init();
#endif
#endif
#ifdef __BT_ONE_BRING_TWO__
            app_bt_device.curr_a2dp_stream_id = stream_id_flag.id_other;
            if(app_bt_device.a2dp_stream[stream_id_flag.id_other].stream.state == AVDTP_STRM_STATE_STREAMING){
         //       app_bt_device.curr_a2dp_stream_id = stream_id_flag.id_other;
                app_bt_device.a2dp_play_pause_flag = 1;
            }
            else
            {
                app_bt_device.a2dp_play_pause_flag = 0;
            }
//            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
#else
            app_bt_device.a2dp_play_pause_flag = 0;
#endif
            app_bt_device.a2dp_state[stream_id_flag.id] = 0;
            app_bt_device.a2dp_streamming[stream_id_flag.id] = 0;

#if FPGA==0
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
        app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
#endif

#ifdef __BT_ONE_BRING_TWO__
        ///a2dp disconnect so check the other stream is playing or not
        if(app_bt_device.a2dp_stream[stream_id_flag.id_other].stream.state  != AVDTP_STRM_STATE_STREAMING)
        {
            app_bt_device.a2dp_play_pause_flag = 0;
        }
#endif
            break;
        case A2DP_EVENT_CODEC_INFO:
            TRACE("::A2DP_EVENT_CODEC_INFO %d\n", Info->event);
            a2dp_set_config_codec(&setconfig_codec,Info);
#ifdef __TWS__
            tws_set_channel_cfg(&setconfig_codec);
#endif
            break;
#ifdef __A2DP_AVDTP_CP__
        case A2DP_EVENT_CP_INFO:
            TRACE("::A2DP_EVENT_CP_INFO %d\n", Info->event);
            TRACE("cpType:%x\n",Info->p.cp->cpType);
            setconfig_cp.cpType = Info->p.cp->cpType;
            setconfig_cp.data = Info->p.cp->data;
            setconfig_cp.dataLen = Info->p.cp->dataLen;
            break;
#endif
        case A2DP_EVENT_GET_CONFIG_IND:
            TRACE("::A2DP_EVENT_GET_CONFIG_IND %d\n", Info->event);
#ifdef __A2DP_AVDTP_CP__
            if(Info->p.capability->type & AVDTP_SRV_CAT_CONTENT_PROTECTION){
                TRACE("support CONTENT_PROTECTION\n");
                A2DP_SetStreamConfig(Stream, &setconfig_codec, &setconfig_cp);
            }else{
                TRACE("no CONTENT_PROTECTION\n");
                A2DP_SetStreamConfig(Stream, &setconfig_codec, NULL);
            }
#else
            A2DP_SetStreamConfig(Stream, &setconfig_codec, NULL);
#endif
            break;

#ifdef __A2DP_AVDTP_CP__
            case A2DP_EVENT_STREAM_SECURITY_IND:
                TRACE("::A2DP_EVENT_STREAM_SECURITY_IND %d\n", Info->event);
                DUMP8("%x ",Info->p.data,Info->len);
                A2DP_SecurityControlRsp(Stream,&Info->p.data[1],Info->len-1,Info->error);
                break;
            case A2DP_EVENT_STREAM_SECURITY_CNF:
                TRACE("::A2DP_EVENT_STREAM_SECURITY_CNF %d\n", Info->event);
                break;
#endif
        case A2DP_EVENT_STREAM_RECONFIG_IND:
            TRACE("::A2DP_EVENT_STREAM_RECONFIG_IND %d\n", Info->event);
            A2DP_ReconfigStreamRsp(Stream,A2DP_ERR_NO_ERROR,0);
            break;
        case A2DP_EVENT_STREAM_RECONFIG_CNF:
            TRACE("::A2DP_EVENT_STREAM_RECONFIG_CNF %d,sample rate=%x\n", Info->event,Stream->stream.codecCfg.elements[0]);
#ifdef __TWS__
#ifdef A2DP_AAC_ON
            if(Stream->stream.codecCfg.codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                set_a2dp_reconfig_sample_rate(Stream);
                app_bt_device.sample_rate[stream_id_flag.id] = Stream->stream.codecCfg.elements[0];
                TRACE("aac sample_rate:%x\n",app_bt_device.sample_rate[stream_id_flag.id]);
            }else
#endif
            {
                set_a2dp_reconfig_sample_rate(Stream);
                a2dp_sample_rate = bt_get_sbc_sample_rate();
    //            TRACE("a2dp_sample_rate:%x\n",a2dp_sample_rate);
                app_bt_device.sample_rate[stream_id_flag.id] = enum_sample_rate_transfer_to_codec_info(a2dp_sample_rate);
                TRACE("sample_rate:%x\n",app_bt_device.sample_rate[stream_id_flag.id]);
            }
#endif
            break;
    }
}

void a2dp_suspend_music_force(void)
{
    app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
}


/*AVRCP SetAbsoluteVolume  
the SetAbsoluteVolume value 0x00~0x7f  Response to the AG Volume Level 0~16
the bt local Volume Level 1~17 Response to the AG Volume Level 0~16

AG vol LEVEL = ((the SetAbsoluteVolume value&0x7f)<<4)/0x7f;
bt local Volume Level=AG vol LEVEL +1;

*/
int a2dp_volume_get(void)
{
    int vol = app_bt_stream_volume_get_ptr()->a2dp_vol-1;

    vol = 8*vol;
    if (vol > (0x7f-1))
        vol = 0x7f;

    return (vol);
}


int a2dp_volume_get_tws(void)
{
    int vol = app_bt_stream_volume_get_ptr()->a2dp_vol-1;

    vol = 8*vol;
    if (vol > (0x7f-1))
        vol = 0x7f;

    return (vol);
}

int app_bt_stream_volumeset(int8_t vol);

void a2dp_volume_local_set(int8_t vol)
{
    app_bt_stream_volume_get_ptr()->a2dp_vol = vol;
#ifndef FPGA
    nv_record_touch_cause_flush();
#endif
}

int a2dp_volume_set(U8 vol)
{
    int dest_vol;

    dest_vol = (((int)vol&0x7f)<<4)/0x7f + 1;

    if (dest_vol > TGT_VOLUME_LEVEL_15)
        dest_vol = TGT_VOLUME_LEVEL_15;
    if (dest_vol < TGT_VOLUME_LEVEL_MUTE)
        dest_vol = TGT_VOLUME_LEVEL_MUTE;

    a2dp_volume_local_set(dest_vol);
    app_bt_stream_volumeset(dest_vol);

    return (vol);
}

//Modified by ATX : parker.wei_20180305 get a2dp vol level
int get_a2dp_volume_level(U8 vol)
{
    int dest_vol;

    dest_vol = (((int)vol&0x7f)<<4)/0x7f + 1;

    if (dest_vol > TGT_VOLUME_LEVEL_15)
        dest_vol = TGT_VOLUME_LEVEL_15;
    if (dest_vol < TGT_VOLUME_LEVEL_MUTE)
        dest_vol = TGT_VOLUME_LEVEL_MUTE;

	
    return (dest_vol);
}


void btapp_a2dp_report_speak_gain(void)
{
#if AVRCP_ADVANCED_CONTROLLER == XA_ENABLED
    uint8_t i;
    int vol = a2dp_volume_get();

    for(i=0; i<BT_DEVICE_NUM; i++)
    {
        if (app_bt_device.avrcp_notify_rsp[i] == NULL)
            app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_notify_rsp[i]);
        TRACE("btapp_a2dp_report_speak_gain transId:%d a2dp_state:%d streamming:%d report:%02x\n",
        app_bt_device.avrcp_notify_rsp[i]->transId,
        app_bt_device.a2dp_state[i],
        app_bt_device.a2dp_streamming[i],
        app_bt_device.volume_report[i]);
        OS_LockStack();
        if ((app_bt_device.a2dp_state[i] == 1) &&
            (app_bt_device.a2dp_streamming[i] == 1) &&
            (app_bt_device.volume_report[i] == AVCTP_RESPONSE_INTERIM)){
            app_bt_device.volume_report[i] = AVCTP_RESPONSE_CHANGED;
            TRACE("btapp_a2dp_report_speak_gain transId:%d\n", app_bt_device.avrcp_notify_rsp[i]->transId);
            if (app_bt_device.avrcp_notify_rsp[i] != NULL){
                app_bt_device.avrcp_notify_rsp[i]->ctype = AVCTP_RESPONSE_CHANGED;
                AVRCP_CtGetAbsoluteVolume_Rsp(&app_bt_device.avrcp_channel[i], app_bt_device.avrcp_notify_rsp[i], vol);
            }
        }
        OS_UnlockStack();
    }
#endif
}

#if defined(A2DP_AAC_ON)
//Modified by ATX : Leon.He_20180622: refactor aac flag checking
static bool check_aac_flag_for_samsung_phone(void)
{
	if(app_bt_device.app_vid_of_phone != APP_VID_IN_AAC_WHITE_LIST_SAMSUNG)
		return false;
	TRACE("##check_aac_flag_for_samsung_phone:: AAC allowed");
	return true;
}

static bool check_aac_flag_for_iphone(void)
{
	if(app_bt_device.app_vid_of_phone != APP_VID_IN_AAC_WHITE_LIST_IPHONE)
		return false;
	TRACE("##check_aac_flag_for_iphone:: AAC allowed");
	return true;
}

static bool check_aac_flag_for_brcm_chip_series(void)
{
	if(app_bt_device.app_vid_of_phone != APP_VID_IN_AAC_WHITE_LIST_BRCM_CHIP_SERIES)
		return false;
	TRACE("##check_aac_flag_for_brcm_chip_series:: AAC allowed");
	return true;
}

static bool check_aac_flag_for_misc_phone(void)
{
	if(app_bt_device.app_vid_of_phone != APP_VID_IN_AAC_WHITE_LIST_MISC)
		return false;
	TRACE("##check_aac_flag_for_misc_phone:: AAC allowed");
	return true;
}

static bool check_aac_flag_for_meizu(void)
{
	if(app_bt_device.app_vid_of_phone != APP_VID_IN_AAC_WHITE_LIST_MEIZU)
		return false;
	TRACE("##check_aac_flag_for_meizu:: AAC allowed");
	return true;
}


static bool check_aac_flag_legacy_condition(AvdtpStream *strm)
{
    if(strm->codec->codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
		return false;
	TRACE("##check_aac_flag_legacy_condition:: enable AAC flag for none AAC phone condition check in low level");
	return true;
}
#endif

bool avdtp_Get_aacEnable_Flag( BtRemoteDevice* remDev, AvdtpStream *strm)
{
#ifdef __FORCE_ENABLE_AAC__
        return TRUE;
#else
#if defined(A2DP_AAC_ON)
	if(true == check_aac_flag_for_samsung_phone())
		return true;
	if(true == check_aac_flag_for_iphone())
		return true;
	if(true == check_aac_flag_for_meizu())
		return true;
	if(true == check_aac_flag_for_misc_phone())
		return true;
	if(true == check_aac_flag_for_brcm_chip_series())
		return true;
	if(true == check_aac_flag_legacy_condition(strm))
		return true;
    return FALSE;
#else
    return TRUE;
#endif

#endif
}

