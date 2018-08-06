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
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#include "app_utils.h"
 

#ifdef __APP_A2DP_SOURCE__

#define APP_SOURCE_DEBUG
#ifdef APP_SOURCE_DEBUG
#define SOURCE_DBLOG TRACE        
#else
#define SOURCE_DBLOG(...)
#endif


extern struct BT_DEVICE_T  app_bt_device;
static AvdtpCodec sink_config_codec;

#define A2DP_LINEIN_SIZE    (48*2*1024)
#define A2DP_TRANS_SIZE     2048
//#define A2DP_SBCBUFF_SIZE   (A2DP_LINEIN_SIZE/2)
 uint8_t a2dp_linein_buff[A2DP_LINEIN_SIZE];
//uint8_t a2dp_sbcbuff[A2DP_SBCBUFF_SIZE];
static  char a2dp_transmit_buffer[A2DP_TRANS_SIZE];

typedef struct _SCB {
  U8     cb_type;                 /* Control Block Type                      */
  U8     mask;                    /* Semaphore token mask                    */
  U16    tokens;                  /* Semaphore tokens                        */
  struct OS_TCB *p_lnk;           /* Chain of tasks waiting for tokens       */
} *PSCB;


typedef struct  {
    osSemaphoreId _osSemaphoreId;
    osSemaphoreDef_t _osSemaphoreDef;
#ifdef CMSIS_OS_RTX
    uint32_t _semaphore_data[2];
#endif

} a2dp_source_lock_t;



typedef struct{
    CQueue  pcm_queue;
    osThreadId sbc_send_id;
    a2dp_source_lock_t  data_lock;
    a2dp_source_lock_t  sbc_send_lock;
    enum AUD_SAMPRATE_T sample_rate;
    uint8_t sbcen_samplerate;
    
    
}A2DP_SOURCE_STRUCT;


typedef struct sbcpack{
    A2dpSbcPacket sbcPacket;
    char buffer[A2DP_TRANS_SIZE];
    int free;
}sbcpack_t;

typedef struct sbcbank{
    sbcpack_t sbcpacks[1];
    int free;
}sbcbank_t;

__SRAMDATA sbcbank_t  sbcbank;

static  sbcpack_t *get_sbcPacket(void)
{
   int index = sbcbank.free;
   sbcbank.free +=1;
   if(sbcbank.free == 1) {
      sbcbank.free = 0;
   }
   return &(sbcbank.sbcpacks[index]);
}


A2DP_SOURCE_STRUCT  a2dp_source;

#if 0
typedef struct {
    AvrcpAdvancedPdu pdu;
    uint8_t para_buf[10];
}APP_A2DP_AVRCPADVANCEDPDU;



extern osPoolId   app_a2dp_avrcpadvancedpdu_mempool;

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


void a2dp_source_volume_local_set(int8_t vol)
{
    app_bt_stream_volume_get_ptr()->a2dp_vol = vol;
    nv_record_touch_cause_flush();
}

static int a2dp_volume_get(void)
{
    int vol = app_bt_stream_volume_get_ptr()->a2dp_vol;

    vol = 8*vol-1; 
    if (vol > (0x7f-1))
        vol = 0x7f;

    return (vol);
}


static int a2dp_volume_set(U8 vol)
{
    int dest_vol;

    dest_vol = (((int)vol&0x7f)<<4)/0x7f + 1;

    if (dest_vol > TGT_VOLUME_LEVEL_15)
        dest_vol = TGT_VOLUME_LEVEL_15;
    if (dest_vol < TGT_VOLUME_LEVEL_0)
        dest_vol = TGT_VOLUME_LEVEL_0;

    a2dp_source_volume_local_set(dest_vol);
    app_bt_stream_volumeset(dest_vol);

    return (vol);
}



void avrcp_source_callback_TG(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    TRACE("%s : chnl %p, Parms %p,Parms->event\n", __FUNCTION__,chnl, Parms,Parms->event);

    enum BT_DEVICE_ID_T device_id = BT_DEVICE_ID_1;
    switch(Parms->event)
    {
        case AVRCP_EVENT_CONNECT:
            if(0)//(chnl->avrcpVersion >=0x103)
            {
                TRACE("::%s AVRCP_GET_CAPABILITY\n",__FUNCTION__);
                if (app_bt_device.avrcp_cmd1[device_id] == NULL)
                    app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_cmd1[device_id]);
                AVRCP_CtGetCapabilities(chnl,app_bt_device.avrcp_cmd1[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED);
            }            

            app_bt_device.avrcp_channel[device_id].avrcpState = AVRCP_STATE_CONNECTED;
            
            TRACE("::%s  AVRCP_EVENT_CONNECT %x,device_id=%d\n",__FUNCTION__, chnl->avrcpVersion,device_id);
            break;
        case AVRCP_EVENT_DISCONNECT:
            TRACE("::%s  AVRCP_EVENT_DISCONNECT",__FUNCTION__);
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
            break;       
        case AVRCP_EVENT_RESPONSE:
            TRACE("::%s  AVRCP_EVENT_RESPONSE op=%x,status=%x\n",__FUNCTION__, Parms->advOp,Parms->status);
        
            break;
        case AVRCP_EVENT_PANEL_CNF:
            TRACE("::%s AVRCP_EVENT_PANEL_CNF %x,%x,%x",__FUNCTION__,
                Parms->p.panelCnf.response,Parms->p.panelCnf.operation,Parms->p.panelCnf.press);
            break;
        case AVRCP_EVENT_ADV_TX_DONE:
            TRACE("::%s AVRCP_EVENT_ADV_TX_DONE device_id=%d,status=%x,errorcode=%x\n",__FUNCTION__,device_id,Parms->status,Parms->errorCode);
            TRACE("::%s AVRCP_EVENT_ADV_TX_DONE op:%d, transid:%x\n", __FUNCTION__,Parms->p.adv.txPdu->op,Parms->p.adv.txPdu->transId);
            if (Parms->p.adv.txPdu->op == AVRCP_OP_GET_CAPABILITIES){
                if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == Parms->p.adv.txPdu){
                    app_bt_device.avrcp_get_capabilities_rsp[device_id] = NULL;
                    app_a2dp_avrcpadvancedpdu_mempool_free(Parms->p.adv.txPdu);
                }
            }

            break;
        case AVRCP_EVENT_COMMAND:
#ifndef __AVRCP_EVENT_COMMAND_VOLUME_SKIP__
            TRACE("::%s AVRCP_EVENT_COMMAND device_id=%d,role=%x\n",__FUNCTION__,device_id,chnl->role);
            TRACE("::%s AVRCP_EVENT_COMMAND ctype=%x,subunitype=%x\n",__FUNCTION__, Parms->p.cmdFrame->ctype,Parms->p.cmdFrame->subunitType);
            TRACE("::%s AVRCP_EVENT_COMMAND subunitId=%x,opcode=%x\n",__FUNCTION__, Parms->p.cmdFrame->subunitId,Parms->p.cmdFrame->opcode);
            TRACE("::%s AVRCP_EVENT_COMMAND operands=%x,operandLen=%x\n",__FUNCTION__, Parms->p.cmdFrame->operands,Parms->p.cmdFrame->operandLen);
            TRACE("::%s AVRCP_EVENT_COMMAND more=%x\n", Parms->p.cmdFrame->more);
            if(Parms->p.cmdFrame->ctype == AVRCP_CTYPE_STATUS)
            {
                uint32_t company_id = *(Parms->p.cmdFrame->operands+2) + ((uint32_t)(*(Parms->p.cmdFrame->operands+1))<<8) + ((uint32_t)(*(Parms->p.cmdFrame->operands))<<16);
                TRACE("::%s AVRCP_EVENT_COMMAND company_id=%x\n",__FUNCTION__, company_id);
                if(company_id == 0x001958)  //bt sig
                {
                    AvrcpOperation op = *(Parms->p.cmdFrame->operands+3);
                    uint8_t oplen =  *(Parms->p.cmdFrame->operands+6)+ ((uint32_t)(*(Parms->p.cmdFrame->operands+5))<<8);
                    TRACE("::%s AVRCP_EVENT_COMMAND op=%x,oplen=%x\n",__FUNCTION__, op,oplen);
                    switch(op)
                    {
                        case AVRCP_OP_GET_CAPABILITIES:
                        {
                                uint8_t event = *(Parms->p.cmdFrame->operands+7);
                                if(event==AVRCP_CAPABILITY_COMPANY_ID)
                                {
                                    TRACE("::%s AVRCP_EVENT_COMMAND send support compay id",__FUNCTION__);
                                }
                                else if(event == AVRCP_CAPABILITY_EVENTS_SUPPORTED)
                                {
                                    TRACE("::%s AVRCP_EVENT_COMMAND send support event transId:%d", __FUNCTION__,Parms->p.cmdFrame->transId);
                                    chnl->adv.eventMask = AVRCP_ENABLE_VOLUME_CHANGED;   ///volume control
                                    if (app_bt_device.avrcp_get_capabilities_rsp[device_id] == NULL)
                                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_get_capabilities_rsp[device_id]);
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                                    app_bt_device.avrcp_get_capabilities_rsp[device_id]->ctype = AVCTP_RESPONSE_IMPLEMENTED_STABLE;
                                    TRACE("::%s AVRCP_EVENT_COMMAND send support event transId:%d", __FUNCTION__,app_bt_device.avrcp_get_capabilities_rsp[device_id]->transId);
                                    AVRCP_CtGetCapabilities_Rsp(chnl,app_bt_device.avrcp_get_capabilities_rsp[device_id],AVRCP_CAPABILITY_EVENTS_SUPPORTED,chnl->adv.eventMask);
                                }
                                else
                                {
                                    TRACE("::%s AVRCP_EVENT_COMMAND send error event value",__FUNCTION__);
                                }
                        }
                        break;
                    }
                    
                }
                
            }else if(Parms->p.cmdFrame->ctype == AVCTP_CTYPE_CONTROL){        
                TRACE("::%s AVRCP_EVENT_COMMAND AVCTP_CTYPE_CONTROL\n",__FUNCTION__);
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[3] == AVRCP_OP_SET_ABSOLUTE_VOLUME){
                    TRACE("::%s AVRCP_EID_VOLUME_CHANGED transId:%d\n", __FUNCTION__,Parms->p.cmdFrame->transId);
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
                TRACE("::%s AVRCP_EVENT_COMMAND AVCTP_CTYPE_NOTIFY\n",__FUNCTION__);
                DUMP8("%02x ", Parms->p.cmdFrame->operands, Parms->p.cmdFrame->operandLen);
                if (Parms->p.cmdFrame->operands[7] == AVRCP_EID_VOLUME_CHANGED){
                    TRACE("::%s AVRCP_EID_VOLUME_CHANGED transId:%d\n", __FUNCTION__,Parms->p.cmdFrame->transId);
                    if (app_bt_device.avrcp_notify_rsp[device_id] == NULL)
                        app_a2dp_avrcpadvancedpdu_mempool_calloc(app_bt_device.avrcp_notify_rsp[device_id]);
                    app_bt_device.avrcp_notify_rsp[device_id]->transId = Parms->p.cmdFrame->transId;
                    app_bt_device.avrcp_notify_rsp[device_id]->ctype = AVCTP_RESPONSE_INTERIM;
                    app_bt_device.volume_report[device_id] = AVCTP_RESPONSE_INTERIM;
                    status = AVRCP_CtGetAbsoluteVolume_Rsp(chnl, app_bt_device.avrcp_notify_rsp[device_id], a2dp_volume_get());
                    TRACE("::%s AVRCP_EVENT_COMMAND AVRCP_EID_VOLUME_CHANGED nRet:%x\n",__FUNCTION__,status);
                }
            }
#endif             
            break;
        case AVRCP_EVENT_ADV_CMD_TIMEOUT:
            TRACE("::%s AVRCP_EVENT_ADV_CMD_TIMEOUT device_id=%d,role=%x\n",__FUNCTION__,device_id,chnl->role);
            break;
    }
}
#endif

osMutexId a2dp_source_mutex_id = NULL;
osMutexDef(a2dp_source_mutex);

static void a2dp_source_mutex_lock(void)
{
    osMutexWait(a2dp_source_mutex_id, osWaitForever);
}

static void a2dp_source_mutex_unlock(void)
{
    osMutexRelease(a2dp_source_mutex_id);
}

static void a2dp_source_sem_lock(a2dp_source_lock_t * lock)
{
    osSemaphoreWait(lock->_osSemaphoreId, osWaitForever);
}

static void a2dp_source_sem_unlock(a2dp_source_lock_t * lock)
{

    osSemaphoreRelease(lock->_osSemaphoreId);
}



static void a2dp_source_reset_send_lock(void)
{
    PSCB p_scb =(PSCB)(a2dp_source.sbc_send_lock._osSemaphoreDef.semaphore);
    uint32_t lock = int_lock();
    p_scb->tokens = 0;
    int_unlock(lock);
}


static bool a2dp_source_is_send_wait(void)
{
    bool ret = false;
    uint32_t lock = int_lock();
    PSCB p_scb =(PSCB)(a2dp_source.sbc_send_lock._osSemaphoreDef.semaphore);
    if(p_scb->p_lnk){
        ret = true;
    }
    int_unlock(lock);
    return ret;
}


static void a2dp_source_wait_pcm_data(void)
{

    a2dp_source_lock_t *lock = &(a2dp_source.data_lock); 
    PSCB p_scb =(PSCB)(lock->_osSemaphoreDef.semaphore);
    uint32_t iflag = int_lock();
    p_scb->tokens = 0;
    int_unlock(iflag);
    a2dp_source_sem_lock(lock);
}

static void a2dp_source_put_data(void)
{
    a2dp_source_lock_t *lock = &(a2dp_source.data_lock); 
    a2dp_source_sem_unlock(lock);
}

#if 0
static void a2dp_source_lock_sbcsending(void * channel)
{
    tws_lock_t *lock = &(a2dp_source.sbc_send_lock); 
    sem_lock(lock);
}
static void a2dp_source_unlock_sbcsending(void * channel)
{
    tws_lock_t *lock = &(a2dp_source.sbc_send_lock); 
    sem_unlock(lock);
}
#endif

static int32_t a2dp_source_wait_sent(uint32_t timeout)
{
    int32_t ret = 0;
    a2dp_source_lock_t *lock = &(a2dp_source.sbc_send_lock);
    a2dp_source_reset_send_lock();
    ret = osSemaphoreWait(lock->_osSemaphoreId, timeout);
    return ret;
    
}

static void a2dp_source_notify_send(void)
{
    if (a2dp_source_is_send_wait()){ //task wait lock
     //   TWS_DBLOG("\nNOTIFY SEND\n");
        a2dp_source_sem_unlock(&(a2dp_source.sbc_send_lock));
    }
    
}



static int a2dp_source_pcm_buffer_write(uint8_t * pcm_buf, uint16_t len)
{
    int status;
    //TWS_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    a2dp_source_mutex_lock();
    status = EnCQueue(&(a2dp_source.pcm_queue), pcm_buf, len);
    a2dp_source_mutex_unlock();
    //TWS_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);
    return status;
}


static int a2dp_source_pcm_buffer_read(uint8_t *buff, uint16_t len)
{
    uint8_t *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    int status;
    a2dp_source_mutex_lock();
    status = PeekCQueue(&(a2dp_source.pcm_queue), len, &e1, &len1, &e2, &len2);
    if (len==(len1+len2)){
        memcpy(buff,e1,len1);
        memcpy(buff+len1,e2,len2);
        DeCQueue(&(a2dp_source.pcm_queue), 0, len);
    }else{
        memset(buff, 0x00, len);
        status = -1;
    }
    a2dp_source_mutex_unlock();
    return status;
}

uint32_t a2dp_source_linein_more_pcm_data(uint8_t * pcm_buf, uint32_t len)
{
    int status;
    status = a2dp_source_pcm_buffer_write(pcm_buf,len);
    if(status !=CQ_OK)
    {
        SOURCE_DBLOG("linin buff overflow!");
    }
    a2dp_source_put_data();
    return len;
}




void a2dp_callback_source(A2dpStream *Stream, const A2dpCallbackParms *Info)
{
//    int header_len = 0;
//    AvdtpMediaHeader header;

    switch(Info->event) {
        case A2DP_EVENT_AVDTP_CONNECT:
            SOURCE_DBLOG("::A2DP_EVENT_AVDTP_CONNECT %d\n", Info->event);
            break;
        case A2DP_EVENT_STREAM_OPEN:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_OPEN stream_id:%d, sample_rate codec.elements 0x%x\n", BT_DEVICE_ID_1,Info->p.configReq->codec.elements[0]);
            if(Info->p.configReq->codec.codecType == AVDTP_CODEC_TYPE_SBC)
            {
                memcpy(app_bt_device.a2dp_codec_elements[BT_DEVICE_ID_1],Info->p.configReq->codec.elements,4);
            }
            if(Info->p.configReq->codec.elements[1] & A2D_SBC_IE_SUBBAND_MSK == A2D_SBC_IE_SUBBAND_4)
                TRACE("numSubBands is only support 8!");
            a2dp_source.sample_rate =  bt_parse_sbc_sample_rate(Stream->stream.codecCfg.elements[0]);
            app_bt_device.sample_rate[BT_DEVICE_ID_1] = (Info->p.configReq->codec.elements[0] & A2D_SBC_IE_SAMP_FREQ_MSK);
            app_bt_device.a2dp_state[BT_DEVICE_ID_1] = 1;

#if FPGA==0
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
        app_stop_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
#endif
       //     AVRCP_Connect(&app_bt_device.avrcp_channel[BT_DEVICE_ID_1], &Stream->stream.conn.remDev->bdAddr);
            break;
        case A2DP_EVENT_STREAM_OPEN_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_OPEN_IND %d\n", Info->event);
            A2DP_OpenStreamRsp(Stream, A2DP_ERR_NO_ERROR, AVDTP_SRV_CAT_MEDIA_TRANSPORT);
            break;
        case A2DP_EVENT_STREAM_STARTED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_STARTED  stream_id:%d\n",BT_DEVICE_ID_1);

            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 1;
            break;
        case A2DP_EVENT_STREAM_START_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_START_IND stream_id:%d\n", BT_DEVICE_ID_1);

            A2DP_StartStreamRsp(Stream, A2DP_ERR_NO_ERROR);
            app_bt_device.a2dp_play_pause_flag = 1;
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_SUSPENDED  stream_id:%d\n",BT_DEVICE_ID_1);
            app_bt_device.a2dp_play_pause_flag = 0;
            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 0;
            break;
        case A2DP_EVENT_STREAM_DATA_IND:
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_CLOSED stream_id:%d, reason = %x\n", BT_DEVICE_ID_1,Info->discReason);
//            app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_SBC,stream_id_flag.id,0,0,0);
            app_bt_device.a2dp_state[BT_DEVICE_ID_1] = 0;
            app_bt_device.a2dp_streamming[BT_DEVICE_ID_1] = 0;

#if FPGA==0
#if defined(__EARPHONE__) && defined(__AUTOPOWEROFF__)
//        app_start_10_second_timer(APP_POWEROFF_TIMER_ID);
#endif
#endif
            a2dp_source_notify_send();
            break;
        case A2DP_EVENT_CODEC_INFO:
            SOURCE_DBLOG("::A2DP_EVENT_CODEC_INFO %d\n", Info->event);
            a2dp_set_config_codec(&sink_config_codec,Info);
            break;
        case A2DP_EVENT_GET_CONFIG_IND:
            SOURCE_DBLOG("::A2DP_EVENT_GET_CONFIG_IND %d\n", Info->event);
 //           sink_config_codec.elements[0] = (sink_config_codec.elements[0]&0xef) |  A2D_SBC_IE_SAMP_FREQ_44;
            A2DP_SetStreamConfig(Stream, &sink_config_codec, NULL);
            break;
        case A2DP_EVENT_STREAM_RECONFIG_IND:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_RECONFIG_IND %d\n", Info->event);
            A2DP_ReconfigStreamRsp(Stream,A2DP_ERR_NO_ERROR,0);
            break;
        case A2DP_EVENT_STREAM_RECONFIG_CNF:
            SOURCE_DBLOG("::A2DP_EVENT_STREAM_RECONFIG_CNF %d\n", Info->event);
            break;
        case A2DP_EVENT_STREAM_SBC_PACKET_SENT:
            a2dp_source_notify_send();
            break;

    }
}



__SRAMDATA static BtHandler a2dp_source_handler;
static uint8_t app_a2dp_source_find_process=0;


void app_a2dp_source_stop_find(void)
{
    app_a2dp_source_find_process=0;
//    ME_UnregisterGlobalHandler(&a2dp_source_handler);

}



static void bt_a2dp_source_call_back(const BtEvent* event)
{
    //   TWS_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
//    uint8_t device_name[64];
//    uint8_t device_name_len;
    switch(event->eType){
        case BTEVENT_NAME_RESULT:
            SOURCE_DBLOG("\n%s %d BTEVENT_NAME_RESULT\n",__FUNCTION__,__LINE__);
            break;
        case BTEVENT_INQUIRY_RESULT: 
            SOURCE_DBLOG("\n%s %d BTEVENT_INQUIRY_RESULT\n",__FUNCTION__,__LINE__);
            DUMP8("%02x ", event->p.inqResult.bdAddr.addr, 6);
            SOURCE_DBLOG("inqmode = %x",event->p.inqResult.inqMode);
            DUMP8("%02x ", event->p.inqResult.extInqResp, 20);
            SOURCE_DBLOG("classdevice=%x",event->p.inqResult.classOfDevice);            
            ///check the class of device to find the handfree device
            if(event->p.inqResult.classOfDevice & COD_MAJOR_AUDIO)
            {
                ME_CancelInquiry(); 
                app_a2dp_source_stop_find();
                SOURCE_DBLOG("start to connect peer device");
                A2DP_OpenStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_1], (BT_BD_ADDR *)&event->p.inqResult.bdAddr);
            }

            break;
        case BTEVENT_INQUIRY_COMPLETE: 
            SOURCE_DBLOG("\n%s %d BTEVENT_INQUIRY_COMPLETE\n",__FUNCTION__,__LINE__);
            app_a2dp_source_stop_find();
            break;
            /** The Inquiry process is canceled. */
        case BTEVENT_INQUIRY_CANCELED:
            SOURCE_DBLOG("\n%s %d BTEVENT_INQUIRY_CANCELED\n",__FUNCTION__,__LINE__);
            break;
        case BTEVENT_LINK_CONNECT_CNF:
            SOURCE_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_CNF stats=%x\n",__FUNCTION__,__LINE__,event->errCode);
            break;
        case BTEVENT_LINK_CONNECT_IND:
            SOURCE_DBLOG("\n%s %d BTEVENT_LINK_CONNECT_IND stats=%x\n",__FUNCTION__,__LINE__,event->errCode);
        default:
            //SOURCE_DBLOG("\n%s %d etype:%d\n",__FUNCTION__,__LINE__,event->eType);
            break;


    }

    //SOURCE_DBLOG("\nexit: %s %d\n",__FUNCTION__,__LINE__);

}

void app_a2dp_start_stream(void)
{
    A2DP_StartStream(&app_bt_device.a2dp_stream[BT_DEVICE_ID_1]);
}



static void _find_a2dp_sink_peer_device_start(void)
{
    SOURCE_DBLOG("\nenter: %s %d\n",__FUNCTION__,__LINE__);
    BtStatus stat;
    if(app_a2dp_source_find_process ==0 && app_bt_device.a2dp_state[0]==0)
    {
        app_a2dp_source_find_process = 1;

    again:  
//Modified by ATX : Leon.He_20171123	
#ifdef __LIAC_FOR_TWS_PAIRING__
        stat = ME_Inquiry(BT_IAC_LIAC, 30, 0);
#else
        stat = ME_Inquiry(BT_IAC_GIAC, 30, 0);
#endif
        SOURCE_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
        if (stat != BT_STATUS_PENDING){
            osDelay(500);
            goto again;
        }
        SOURCE_DBLOG("\n%s %d\n",__FUNCTION__,__LINE__);
    }
}

void app_a2dp_source_find_sink(void)
{
    _find_a2dp_sink_peer_device_start();
}


static bool need_init_encoder = true;
__PSRAMDATA static SbcStreamInfo StreamInfo = {0};
__SRAMDATA static SbcEncoder sbc_encoder;


static uint8_t app_a2dp_source_samplerate_2_sbcenc_type(enum AUD_SAMPRATE_T sample_rate)
{
    uint8_t rate;
    switch(sample_rate)
    {
        case AUD_SAMPRATE_16000:
            rate =  SBC_CHNL_SAMPLE_FREQ_16;
            break;
        case AUD_SAMPRATE_32000:
            rate =  SBC_CHNL_SAMPLE_FREQ_32;
            break;
        case AUD_SAMPRATE_44100:
            rate =  SBC_CHNL_SAMPLE_FREQ_44_1;
            break;
        case AUD_SAMPRATE_48000:
            rate =  SBC_CHNL_SAMPLE_FREQ_48;
            break;
         default:
            TRACE("error!  sbc enc don't support other samplerate");
            break;
    }
        SOURCE_DBLOG("\n%s %d rate = %x\n",__FUNCTION__,__LINE__,rate);
        return rate;
    
}


static void a2dp_source_send_sbc_packet(void)
{
    uint32_t frame_size = 512;
    uint32_t frame_num = A2DP_TRANS_SIZE/frame_size;
//    a2dp_source.lock_stream(&(tws.tws_source));
    uint16_t byte_encoded = 0;
//    uint16_t pcm_frame_size = 512/2;
    unsigned short enc_len = 0;
    BtStatus status = BT_STATUS_FAILED;
    
    int lock = int_lock();
    sbcpack_t *sbcpack = get_sbcPacket();
    A2dpSbcPacket *sbcPacket = &(sbcpack->sbcPacket);
    sbcPacket->data = (U8 *)sbcpack->buffer;
    memcpy(sbcpack->buffer,a2dp_transmit_buffer,A2DP_TRANS_SIZE);

//    sbcPacket->dataLen = len;
//    sbcPacket->frameSize = len/frame_num;

        SbcPcmData PcmEncData;
    	if(need_init_encoder) {
            SBC_InitEncoder(&sbc_encoder);
            sbc_encoder.streamInfo.numChannels = 2;
            sbc_encoder.streamInfo.channelMode = SBC_CHNL_MODE_STEREO;
            sbc_encoder.streamInfo.bitPool     = 53;
            sbc_encoder.streamInfo.sampleFreq  = app_a2dp_source_samplerate_2_sbcenc_type(a2dp_source.sample_rate);
            sbc_encoder.streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
            sbc_encoder.streamInfo.numBlocks   = 16;
            sbc_encoder.streamInfo.numSubBands = 8;
            sbc_encoder.streamInfo.mSbcFlag    = 0;
            need_init_encoder = 0;
            TRACE("sbc_encoder.streamInfo.sampleFreq = %x",sbc_encoder.streamInfo.sampleFreq);
        }
        PcmEncData.data = (uint8_t *)a2dp_transmit_buffer;
        PcmEncData.dataLen = A2DP_TRANS_SIZE;
        PcmEncData.numChannels = 2;
        PcmEncData.sampleFreq = sbc_encoder.streamInfo.sampleFreq;
        SBC_EncodeFrames(&sbc_encoder, &PcmEncData, &byte_encoded,
                                        (unsigned char*)sbcpack->buffer, &enc_len, A2DP_TRANS_SIZE) ;
        sbcPacket->dataLen = enc_len;
        sbcPacket->frameSize = enc_len/frame_num;
    
    int_unlock(lock);
    status = A2DP_StreamSendSbcPacket(&app_bt_device.a2dp_stream[BT_DEVICE_ID_1],sbcPacket,&StreamInfo);
    if(status == BT_STATUS_PENDING)
        a2dp_source_wait_sent(osWaitForever);
    else 
        TRACE("SEND ERROR CHECK IT!");
}



#define BT_A2DP_SOURCE_LINEIN_BUFF_SIZE    	(512*5*2*2)

//////////start the audio linein stream for capure the pcm data 
int app_a2dp_source_linein_on(bool on)
{
    uint8_t *buff_play = NULL;
    uint8_t *buff_capture = NULL;
    uint8_t *buff_loop = NULL;
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    SOURCE_DBLOG("app_a2dp_source_linein_on work:%d op:%d", isRun, on);

    if (isRun==on)
        return 0;

    if (on){
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
        app_audio_mempool_init(app_audio_get_basebuf_ptr(APP_MEM_LINEIN_AUDIO), app_audio_get_basebuf_size(APP_MEM_LINEIN_AUDIO));
#else
        app_audio_mempool_init();
#endif
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_208M);

        app_audio_mempool_get_buff(&buff_play, BT_A2DP_SOURCE_LINEIN_BUFF_SIZE);

        memset(&stream_cfg, 0, sizeof(stream_cfg));
        
        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = a2dp_source.sample_rate;
#if FPGA==0
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#endif
        stream_cfg.trigger_time = 0;        

        stream_cfg.vol = 10;
        stream_cfg.io_path = AUD_INPUT_PATH_HP_MIC;
        stream_cfg.handler = a2dp_source_linein_more_pcm_data;
        stream_cfg.data_ptr = buff_play;
        stream_cfg.data_size = BT_A2DP_SOURCE_LINEIN_BUFF_SIZE;
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        SOURCE_DBLOG("app_factorymode_audioloop on");
    } else {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        SOURCE_DBLOG("app_factorymode_audioloop off");
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}














////////////////////////////creat the thread for send sbc data to a2dp sink device ///////////////////
static void send_thread(const void *arg);


__SRAMDATA osThreadDef(send_thread, osPriorityHigh, 1024*2);



static void send_thread(const void *arg)
{
    while(1){
        a2dp_source_wait_pcm_data();
        while(a2dp_source_pcm_buffer_read((uint8_t *)a2dp_transmit_buffer,A2DP_TRANS_SIZE)==0)
        {
            a2dp_source_send_sbc_packet();
            
        }
    }
}



///////init the a2dp source feature 
void app_a2dp_source_init(void)
{
        a2dp_source_lock_t *lock;
        //register the bt global handler
        a2dp_source_handler.callback = bt_a2dp_source_call_back;
        ME_RegisterGlobalHandler(&a2dp_source_handler);
        ME_SetEventMask(&a2dp_source_handler, BEM_LINK_DISCONNECT|BEM_ROLE_CHANGE|BEM_INQUIRY_RESULT|
                BEM_INQUIRY_COMPLETE|BEM_INQUIRY_CANCELED|BEM_LINK_CONNECT_CNF|BEM_LINK_CONNECT_IND);    

    InitCQueue(&a2dp_source.pcm_queue, A2DP_LINEIN_SIZE, ( CQItemType *)a2dp_linein_buff);
 //   InitCQueue(&a2dp_source.pcm_queue, A2DP_LINEIN_SIZE, ( CQItemType *)a2dp_linein_buff);

    if(a2dp_source_mutex_id == NULL)
    {
        a2dp_source_mutex_id = osMutexCreate((osMutex(a2dp_source_mutex)));
    }


    lock = &(a2dp_source.data_lock);
    memset(lock,0,sizeof(a2dp_source_lock_t));
    lock->_osSemaphoreDef.semaphore = lock->_semaphore_data;
    lock->_osSemaphoreId = osSemaphoreCreate(&(lock->_osSemaphoreDef), 0);

    lock = &(a2dp_source.sbc_send_lock);
    memset(lock,0,sizeof(a2dp_source_lock_t));
    lock->_osSemaphoreDef.semaphore = lock->_semaphore_data;
    lock->_osSemaphoreId = osSemaphoreCreate(&(lock->_osSemaphoreDef), 0);



    a2dp_source.sbc_send_id = osThreadCreate(osThread(send_thread), NULL);
    a2dp_source.sample_rate = AUD_SAMPRATE_44100;
        
}

#endif
