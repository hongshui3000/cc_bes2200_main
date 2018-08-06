#include "cmsis_os.h"
#include "string.h"
#include "hal_trace.h"

#include "besbt.h"
#include "app_bt_func.h"

#include "app_tws_spp.h"
#include "btapp.h"

extern "C" void OS_NotifyEvm(void);

#define APP_BT_MAILBOX_MAX (15)
osMailQDef (app_bt_mailbox, APP_BT_MAILBOX_MAX, APP_BT_MAIL);
static osMailQId app_bt_mailbox = NULL;
//void btapp_hfp_report_speak_gain(void);

void btapp_a2dp_report_speak_gain(void);

inline int app_bt_mail_process(APP_BT_MAIL* mail_p)
{
    BtStatus status = BT_STATUS_LAST_CODE;
    if (mail_p->request_id != CMGR_SetSniffTimer_req){
        TRACE("%s src_thread:0x%08x request_id:%d enter",__func__, mail_p->src_thread, mail_p->request_id);
    }
    switch (mail_p->request_id) { 
        case Bt_Generic_req:
            if (mail_p->param.Generic_req_param.handler){
				((APP_BT_GENERIC_REQ_CB_T)(mail_p->param.Generic_req_param.handler))(
                    (void *)mail_p->param.Generic_req_param.param0, 
                    (void *)mail_p->param.Generic_req_param.param1);
			}
            break;
        case Me_switch_sco_req:
            status = Me_switch_sco(mail_p->param.Me_switch_sco_param.scohandle);
            break;
        case ME_SwitchRole_req:
            status = ME_SwitchRole(mail_p->param.ME_SwitchRole_param.remDev);
            break;
        case ME_SetConnectionRole_req:
            ME_SetConnectionRole(mail_p->param.BtConnectionRole_param.role);
            status = BT_STATUS_SUCCESS;
            break;            
        case MeDisconnectLink_req:
            MeDisconnectLink(mail_p->param.MeDisconnectLink_param.remDev);
            status = BT_STATUS_SUCCESS;
            break;
        case ME_StopSniff_req:
            status = ME_StopSniff(mail_p->param.ME_StopSniff_param.remDev);
            break;
        case ME_SetAccessibleMode_req:
            status = ME_SetAccessibleMode(mail_p->param.ME_SetAccessibleMode_param.mode,
                                          &mail_p->param.ME_SetAccessibleMode_param.info);
            break;
        case Me_SetLinkPolicy_req:
            status = Me_SetLinkPolicy(mail_p->param.Me_SetLinkPolicy_param.remDev,
                                      mail_p->param.Me_SetLinkPolicy_param.policy);
            break;
        case ME_SetConnectionQosInfo_req:
            status = ME_SetConnectionQosInfo(mail_p->param.ME_SetConnectionQosInfo_param.remDev,
                                             &mail_p->param.ME_SetConnectionQosInfo_param.qosInfo);
            break;
        case CMGR_SetSniffTimer_req:
            if (mail_p->param.CMGR_SetSniffTimer_param.SniffInfo.maxInterval == 0){
                status = CMGR_SetSniffTimer(mail_p->param.CMGR_SetSniffTimer_param.Handler,
                                            NULL,
                                            mail_p->param.CMGR_SetSniffTimer_param.Time);
            }else{
                status = CMGR_SetSniffTimer(mail_p->param.CMGR_SetSniffTimer_param.Handler,
                                            &mail_p->param.CMGR_SetSniffTimer_param.SniffInfo,
                                            mail_p->param.CMGR_SetSniffTimer_param.Time);
            }
            break;
        case CMGR_SetSniffInofToAllHandlerByRemDev_req:
            status = CMGR_SetSniffInofToAllHandlerByRemDev(&mail_p->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.SniffInfo,
                                                            mail_p->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.RemDev);
            break;
        case A2DP_OpenStream_req:
            status = A2DP_OpenStream(mail_p->param.A2DP_OpenStream_param.Stream,
                                     mail_p->param.A2DP_OpenStream_param.Addr);
            break;
        case A2DP_CloseStream_req:
            status = A2DP_CloseStream(mail_p->param.A2DP_CloseStream_param.Stream);
            break;
        case A2DP_SetMasterRole_req:
            status = A2DP_SetMasterRole(mail_p->param.A2DP_SetMasterRole_param.Stream,
                                        mail_p->param.A2DP_SetMasterRole_param.Flag);
            break;
        case HF_CreateServiceLink_req:
            status = HF_CreateServiceLink(mail_p->param.HF_CreateServiceLink_param.Chan,
                                          mail_p->param.HF_CreateServiceLink_param.Addr);
            break;
        case HF_DisconnectServiceLink_req:
            status = HF_DisconnectServiceLink(mail_p->param.HF_DisconnectServiceLink_param.Chan);
            break;
        case HF_CreateAudioLink_req:
            status = HF_CreateAudioLink(mail_p->param.HF_CreateAudioLink_param.Chan);
            break;
        case HF_DisconnectAudioLink_req:
            status = HF_DisconnectAudioLink(mail_p->param.HF_DisconnectAudioLink_param.Chan);
            break;
        case HF_EnableSniffMode_req:
            status = HF_EnableSniffMode(mail_p->param.HF_EnableSniffMode_param.Chan,
                                        mail_p->param.HF_EnableSniffMode_param.Enable);
            break;
        case HF_SetMasterRole_req:
            status = HF_SetMasterRole(mail_p->param.HF_SetMasterRole_param.Chan,
                                      mail_p->param.HF_SetMasterRole_param.Flag);
            break;
#if defined (__HSP_ENABLE__)
        case HS_CreateServiceLink_req:
            status = HS_CreateServiceLink(mail_p->param.HS_CreateServiceLink_param.Chan,
                                          mail_p->param.HS_CreateServiceLink_param.Addr);
            break;
        case HS_CreateAudioLink_req:
            status = HS_CreateAudioLink(mail_p->param.HS_CreateAudioLink_param.Chan);
            break;
        case HS_DisconnectAudioLink_req:
            status = HS_DisconnectAudioLink(mail_p->param.HS_DisconnectAudioLink_param.Chan);
            break;
		case HS_DisconnectServiceLink_req:
			status = HS_DisconnectServiceLink(mail_p->param.HS_DisconnectServiceLink_param.Chan);
			break;
        case HS_EnableSniffMode_req:
            status = HS_EnableSniffMode(mail_p->param.HS_EnableSniffMode_param.Chan,
                                        mail_p->param.HS_EnableSniffMode_param.Enable);
            break;
#endif
        case HF_Report_Gain:
            btapp_hfp_report_speak_gain();
            break;
        case A2DP_Reort_Gain:
            btapp_a2dp_report_speak_gain();
            break;           
        case SPP_write:
            TRACE("$$$$$$$$$$mail spp write process");
            btapp_process_spp_write(mail_p->param.SPP_WRITE_param.cmdid,mail_p->param.SPP_WRITE_param.param,
                  mail_p->param.SPP_WRITE_param.dataptr,mail_p->param.SPP_WRITE_param.datalen);
            break;
        case SPP_send_rsp:
            TWS_SPP_CMD_RSP rsp;
            rsp.cmd_id = mail_p->param.SPP_WRITE_param.cmdid;
            rsp.status = mail_p->param.SPP_WRITE_param.param;
            
            TRACE("$$$$$$$$$$mail spp send rsp process");
            tws_spp_send_rsp((uint8_t *)&rsp,sizeof(rsp));

            break;
            
    }
    
    if (mail_p->request_id != CMGR_SetSniffTimer_req){
        TRACE("%s request_id:%d :status:%d exit",__func__, mail_p->request_id, status);
    }
    return 0;
}

inline int app_bt_mail_alloc(APP_BT_MAIL** mail)
{
	*mail = (APP_BT_MAIL*)osMailAlloc(app_bt_mailbox, 0);
	ASSERT(*mail, "app_bt_mail_alloc error");
    return 0;
}

inline int app_bt_mail_send(APP_BT_MAIL* mail)
{
	osStatus status;

	ASSERT(mail, "osMailAlloc NULL");	
	status = osMailPut(app_bt_mailbox, mail);
    ASSERT(osOK == status, "osMailAlloc Put failed");

    OS_NotifyEvm();

	return (int)status;
}

inline int app_bt_mail_free(APP_BT_MAIL* mail_p)
{
	osStatus status;
	
	status = osMailFree(app_bt_mailbox, mail_p);
    ASSERT(osOK == status, "osMailAlloc Put failed");
	
	return (int)status;
}

inline int app_bt_mail_get(APP_BT_MAIL** mail_p)
{
	osEvent evt;
	evt = osMailGet(app_bt_mailbox, 0);
	if (evt.status == osEventMail) {
		*mail_p = (APP_BT_MAIL *)evt.value.p;
		return 0;
	}
	return -1;
}

static void app_bt_mail_poll(void)
{
	APP_BT_MAIL *mail_p = NULL;
	if (!app_bt_mail_get(&mail_p)){
        app_bt_mail_process(mail_p);
		app_bt_mail_free(mail_p);
	}        
}

int app_bt_mail_init(void)
{
	app_bt_mailbox = osMailCreate(osMailQ(app_bt_mailbox), NULL);
	if (app_bt_mailbox == NULL)  {
        TRACE("Failed to Create app_mailbox\n");
		return -1;
	}
    Besbt_hook_handler_set(BESBT_HOOK_USER_1, app_bt_mail_poll);

	return 0;
}

int app_bt_generic_req(uint32_t param0, uint32_t param1, uint32_t handler)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Bt_Generic_req;
    mail->param.Generic_req_param.handler = handler;
    mail->param.Generic_req_param.param0 = param0;
    mail->param.Generic_req_param.param1 = param1;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_Me_switch_sco(uint16_t  scohandle)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Me_switch_sco_req;
    mail->param.Me_switch_sco_param.scohandle = scohandle;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SwitchRole(BtRemoteDevice* remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SwitchRole_req;
    mail->param.ME_SwitchRole_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetConnectionRole(BtConnectionRole role)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetConnectionRole_req;
    mail->param.BtConnectionRole_param.role = role;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_MeDisconnectLink(BtRemoteDevice* remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = MeDisconnectLink_req;
    mail->param.MeDisconnectLink_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_StopSniff(BtRemoteDevice *remDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_StopSniff_req;
    mail->param.ME_StopSniff_param.remDev = remDev;
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetAccessibleMode(BtAccessibleMode mode, const BtAccessModeInfo *info)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetAccessibleMode_req;
    mail->param.ME_SetAccessibleMode_param.mode = mode;    
    memcpy(&mail->param.ME_SetAccessibleMode_param.info, info, sizeof(BtAccessModeInfo));
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_Me_SetLinkPolicy(BtRemoteDevice *remDev, BtLinkPolicy policy)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = Me_SetLinkPolicy_req;
    mail->param.Me_SetLinkPolicy_param.remDev = remDev;    
    mail->param.Me_SetLinkPolicy_param.policy = policy;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_ME_SetConnectionQosInfo(BtRemoteDevice *remDev,
                                              BtQosInfo *qosInfo)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = ME_SetConnectionQosInfo_req;
    mail->param.ME_SetConnectionQosInfo_param.remDev = remDev;    
    memcpy(&mail->param.ME_SetConnectionQosInfo_param.qosInfo, qosInfo, sizeof(BtQosInfo));    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_CMGR_SetSniffTimer(   CmgrHandler *Handler, 
                                                BtSniffInfo* SniffInfo, 
                                                TimeT Time)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = CMGR_SetSniffTimer_req;
    mail->param.CMGR_SetSniffTimer_param.Handler = Handler;
    if (SniffInfo){
        memcpy(&mail->param.CMGR_SetSniffTimer_param.SniffInfo, SniffInfo, sizeof(BtSniffInfo));
    }else{
        memset(&mail->param.CMGR_SetSniffTimer_param.SniffInfo, 0, sizeof(BtSniffInfo));
    }
    mail->param.CMGR_SetSniffTimer_param.Time = Time;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_CMGR_SetSniffInofToAllHandlerByRemDev(BtSniffInfo* SniffInfo, 
                            								     BtRemoteDevice *RemDev)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = CMGR_SetSniffInofToAllHandlerByRemDev_req;
    memcpy(&mail->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.SniffInfo, SniffInfo, sizeof(BtSniffInfo));    
    mail->param.CMGR_SetSniffInofToAllHandlerByRemDev_param.RemDev = RemDev;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_OpenStream(A2dpStream *Stream, BT_BD_ADDR *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_OpenStream_req;
    mail->param.A2DP_OpenStream_param.Stream = Stream;    
    mail->param.A2DP_OpenStream_param.Addr = Addr;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_CloseStream(A2dpStream *Stream)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_CloseStream_req;
    mail->param.A2DP_CloseStream_param.Stream = Stream;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_A2DP_SetMasterRole(A2dpStream *Stream, BOOL Flag)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_SetMasterRole_req;
    mail->param.A2DP_SetMasterRole_param.Stream = Stream;    
    mail->param.A2DP_SetMasterRole_param.Flag = Flag;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_CreateServiceLink(HfChannel *Chan, BT_BD_ADDR *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_CreateServiceLink_req;
    mail->param.HF_CreateServiceLink_param.Chan = Chan;    
    mail->param.HF_CreateServiceLink_param.Addr = Addr;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_DisconnectServiceLink(HfChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_DisconnectServiceLink_req;
    mail->param.HF_DisconnectServiceLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_CreateAudioLink(HfChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_CreateAudioLink_req;
    mail->param.HF_CreateAudioLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_DisconnectAudioLink(HfChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_DisconnectAudioLink_req;
    mail->param.HF_DisconnectAudioLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_EnableSniffMode(HfChannel *Chan, BOOL Enable)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_EnableSniffMode_req;
    mail->param.HF_EnableSniffMode_param.Chan = Chan;    
    mail->param.HF_EnableSniffMode_param.Enable = Enable;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HF_SetMasterRole(HfChannel *Chan, BOOL Flag)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_SetMasterRole_req;
    mail->param.HF_SetMasterRole_param.Chan = Chan;    
    mail->param.HF_SetMasterRole_param.Flag = Flag;    
    app_bt_mail_send(mail);
    return 0;
}

#if defined (__HSP_ENABLE__)
int app_bt_HS_CreateServiceLink(HsChannel *Chan, BT_BD_ADDR *Addr)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_CreateServiceLink_req;
    mail->param.HS_CreateServiceLink_param.Chan = Chan;    
    mail->param.HS_CreateServiceLink_param.Addr = Addr;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HS_CreateAudioLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_CreateAudioLink_req;
    mail->param.HS_CreateAudioLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_HS_DisconnectAudioLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_DisconnectAudioLink_req;
    mail->param.HS_DisconnectAudioLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}

int app_bt_HS_DisconnectServiceLink(HsChannel *Chan)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_DisconnectServiceLink_req;
    mail->param.HS_DisconnectServiceLink_param.Chan = Chan;    
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_HS_EnableSniffMode(HsChannel *Chan, BOOL Enable)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HS_EnableSniffMode_req;
    mail->param.HS_EnableSniffMode_param.Chan = Chan;    
    mail->param.HS_EnableSniffMode_param.Enable = Enable;    
    app_bt_mail_send(mail);
    return 0;
}




#endif



int app_bt_HF_Report_Gain(void)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = HF_Report_Gain;
    app_bt_mail_send(mail);
    return 0;
}


int app_bt_A2dp_Report_Gain(void)
{
    APP_BT_MAIL* mail;
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = A2DP_Reort_Gain;
    app_bt_mail_send(mail);
    return 0;
}

void tws_spp_wait_send_done(void);

int app_bt_SPP_Write_Cmd(uint16_t id,uint32_t param,uint8_t *ptr ,uint32_t len)
{
    APP_BT_MAIL* mail;
    TRACE("SPP WRITE CMD ID=%d,param=%x",id,param);
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = SPP_write;
    mail->param.SPP_WRITE_param.cmdid = id;
    mail->param.SPP_WRITE_param.param = param;
    mail->param.SPP_WRITE_param.dataptr= ptr;
    mail->param.SPP_WRITE_param.datalen = len;
    
    app_bt_mail_send(mail);

    tws_spp_wait_send_done();
    
    return 0;
    
}



int app_bt_SPP_Write_Rsp(uint16_t id,uint32_t param)
{
    APP_BT_MAIL* mail;
    TRACE("SPP WRITE CMD ID=%d,param=%x",id,param);
    app_bt_mail_alloc(&mail);
    mail->src_thread = (uint32_t)osThreadGetId();
    mail->request_id = SPP_send_rsp;
    mail->param.SPP_WRITE_param.cmdid = id;
    mail->param.SPP_WRITE_param.param = param;
   
    app_bt_mail_send(mail);

    tws_spp_wait_send_done();
    
    return 0;
}

