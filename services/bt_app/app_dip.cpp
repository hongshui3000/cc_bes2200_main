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
#include "app_dip.h"
#include "resources.h"

#include "app_bt_media_manager.h"
#include "app_dip.h"

//#define GET_IAP_INFO

#if DIP_DEVICE ==XA_ENABLED
#define DIP_AID_SPEC_ID     (0x0200) 
#define DIP_AID_VID         (0x0201) 
#define DIP_AID_PID         (0x0202) 
#define DIP_AID_VERSION     (0x0203) 
#define DIP_AID_PRI_RECORD (0x0204) 
#define DIP_AID_VID_SOURCE (0x0205) 

DipClient Dipdev;
DipPnpInfo PNP_info;
#ifdef GET_IAP_INFO
IapClient IapDev;
IapInfo  Iap1Info;

#endif
/****************************************************************************
 * SPP SDP Entries
 ****************************************************************************/

/*---------------------------------------------------------------------------
 *
 * ServiceClassIDList 
 */
static const U8 DIPClassId[] = {
    SDP_ATTRIB_HEADER_8BIT(3),        /* Data Element Sequence, 6 bytes */ 
    SDP_UUID_16BIT(SC_PNP_INFO),     /* Hands-Free UUID in Big Endian */ 
};

static const U8 DipProtoDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(13),  /* Data element sequence, 13 bytes */ 

    /* Each element of the list is a Protocol descriptor which is a 
     * data element sequence. The first element is L2CAP which only 
     * has a UUID element.  
     */ 
    SDP_ATTRIB_HEADER_8BIT(6),   /* Data element sequence for L2CAP, 3 
                                  * bytes 
                                  */ 

    SDP_UUID_16BIT(PROT_L2CAP),     /* Uuid16 L2CAP */ 
    SDP_UINT_16BIT(BT_PSM_SDP),      //PSM 0x0001


    /* Next protocol descriptor in the list is RFCOMM. It contains two 
     * elements which are the UUID and the channel. Ultimately this 
     * channel will need to filled in with value returned by RFCOMM.  
     */ 
    SDP_ATTRIB_HEADER_8BIT(3),  //UUID SDP
    SDP_UUID_16BIT(PROT_SDP),

};

/*
 * BluetoothProfileDescriptorList
 */
static const U8 DipProfileDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(8),        /* Data element sequence, 8 bytes */

    /* Data element sequence for ProfileDescriptor, 6 bytes */
    SDP_ATTRIB_HEADER_8BIT(6),

    SDP_UUID_16BIT(SC_PNP_INFO),   /* Uuid16 PNP */
    SDP_UINT_16BIT(0x0100),            /* version1.0 ????*/ 
};

/*
 * specificationID
 */
static const U8 DipProfileSpecificationID[] = {
    SDP_UINT_16BIT(BLUETOOTH_DI_SPECIFICATION),            /* version1.3 */ 
};
static const U8 DipProfileSpecificationVendorID[] = {
    SDP_UINT_16BIT(DIP_VENDOR_ID) ,         
};
static const U8 DipProfileSpecificationPruct_ID[] = {
    SDP_UINT_16BIT(DIP_PRODUCT_ID) ,         
};
static const U8 DipProfileSpecificationPruct_Version[] = {
    SDP_UINT_16BIT(DIP_PRODUCT_VERSION) ,         
};

static const U8 DipProfileSpecificationPRIMARY_RECORD[] = {
    SDP_BOOL(true)          ,
};
static const U8 DipProfileSpecificationVENDOR_ID_SOURCE[] = {
    SDP_UINT_16BIT(DI_VENDOR_ID_SOURCE_BTSIG)  ,        
};


/* DIP attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * SPP SDP record.
 */
static const SdpAttribute DIPSdpAttributes[] = {

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, DIPClassId), 
    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, DipProtoDescList),
    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, DipProfileDescList),
    
    SDP_ATTRIBUTE(ATTR_ID_SPECIFICATION_ID, DipProfileSpecificationID),
    SDP_ATTRIBUTE(ATTR_ID_VENDOR_ID, DipProfileSpecificationVendorID),
    SDP_ATTRIBUTE(ATTR_ID_PRODUCT_ID, DipProfileSpecificationPruct_ID),
    SDP_ATTRIBUTE(ATTR_ID_PRODUCT_VERSION, DipProfileSpecificationPruct_Version),
    SDP_ATTRIBUTE(ATTR_ID_PRIMARY_RECORD, DipProfileSpecificationPRIMARY_RECORD),
    SDP_ATTRIBUTE(ATTR_ID_VENDOR_ID_SOURCE, DipProfileSpecificationVENDOR_ID_SOURCE),


};



/****************************************************************************
 *
 * Functions
 *
 ****************************************************************************/


/*---------------------------------------------------------------------------
 *            DIPRegisterSdpServices()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Registers the SDP services.  
 *
 * Return:    See SDP_AddRecord().  
 */

BtStatus DIPRegisterSdpServices()
{
    BtStatus  status;
    const U8 *cu8p;

    //UNUSED_PARAMETER(Chan);


    /* Register Hands-free SDP Attributes */ 
    Assert(sizeof(DIP(dipSdpAttribute)) == sizeof(DIPSdpAttributes));
    cu8p = (const U8 *)(&DIPSdpAttributes[0]);
    //cu8p = (const void *)(&HidSdpAttributes[0]);
    OS_MemCopy((U8 *)DIP(dipSdpAttribute), 
               cu8p,
               sizeof(DIPSdpAttributes));


    DIP(dipSdpRecord).attribs = DIP(dipSdpAttribute);
    DIP(dipSdpRecord).num = 9;
    DIP(dipSdpRecord).classOfDevice = COD_MAJOR_PERIPHERAL;

    status = SDP_AddRecord(&DIP(dipSdpRecord));

    Report(("HID(hidSdpRecord)  handle=%x, record state=%x",
        DIP(dipSdpRecord).handle,DIP(dipSdpRecord).recordState));
    return status;
}




/*---------------------------------------------------------------------------
 *            DipDeregisterSdpServices()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deregisters the SDP services.  
 *
 * Return:    See SDP_RemoveRecord().  
 */
BtStatus DipDeregisterSdpServices(void)
{
    BtStatus status;    

    /* Remove the Hands-free entry */ 
    status = SDP_RemoveRecord(&DIP(dipSdpRecord));

    return status;
}

static const U8 DipServiceSearchAttribReq[] = {

    /* First parameter is the search pattern in data element format.  It 
     * is a list of 3 UUIDs.  
     */ 

    /* Data Element Sequence, 9 bytes */ 
    SDP_ATTRIB_HEADER_8BIT(3),

    SDP_UUID_16BIT(SC_PNP_INFO),


    /* The second parameter is the maximum number of bytes that can be 
     * be received for the attribute list.  
     */ 
    0x00, 0x64,                /* Max number of bytes for attribute is 100 */ 

    SDP_ATTRIB_HEADER_8BIT(18),
//    SDP_UINT_16BIT(AID_PROTOCOL_DESC_LIST), 
//    SDP_UINT_16BIT(AID_BT_PROFILE_DESC_LIST),
//    SDP_UINT_16BIT(AID_ADDITIONAL_PROT_DESC_LISTS),    
        SDP_UINT_16BIT(0x0200), 
        SDP_UINT_16BIT(0x0201),
     SDP_UINT_16BIT(0x0202),
     SDP_UINT_16BIT(0x0203),
     SDP_UINT_16BIT(0x0204),
     SDP_UINT_16BIT(0x0205)
};
#ifdef GET_IAP_INFO
//UUID: IAP 1 00 00 00 00 de ca fa de de ca de af de ca ca fe
/* 128 bit UUID in Big Endian 81c2e72a-0591-443e-a1ff-05f988593351 */
static const uint8_t IAP1_UUID[16] = {
    0xfe, 0xca, 0xca, 0xde, 0xaf, 0xde, 0xca, 0xde,
    0xde, 0xfa, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00};

//#define IAP1_UUID[] (0x00000000decafadedecadeafdecacafe)
static const U8 IAP_ServiceSearchAttribReq[] = {

    /* First parameter is the search pattern in data element format.  It 
     * is a list of 3 UUIDs.  
     */ 

    /* Data Element Sequence, 9 bytes */ 
    SDP_ATTRIB_HEADER_8BIT(17),
    SDP_UUID_128BIT(IAP1_UUID),

    //SDP_UUID_128BIT(IAP1_UUID),


    /* The second parameter is the maximum number of bytes that can be 
     * be received for the attribute list.  
     */ 
    0x00, 0x64,                /* Max number of bytes for attribute is 100 */ 

    SDP_ATTRIB_HEADER_8BIT(6),
    SDP_UINT_16BIT(AID_PROTOCOL_DESC_LIST), /* Protocol descriptor 
                                             * list ID 
                                             */ 
    SDP_UINT_16BIT(AID_BT_PROFILE_DESC_LIST), 

};

#endif

static const U8 DipServiceSearchReq[] = {

    /* First parameter is the search pattern in data element format.  It 
     * is a list of 3 UUIDs.  
     */ 

    /* Data Element Sequence, 9 bytes */ 
    SDP_ATTRIB_HEADER_8BIT(3),

    SDP_UUID_16BIT(SC_PNP_INFO),


    /* The second parameter is the maximum number of bytes that can be 
     * be received for the attribute list.  
     */ 
    0x00, 0x64,                /* Max number of bytes for attribute is 100 */ 
};


/*---------------------------------------------------------------------------
 *            DIPRegisterClient()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Registers the SDP services.  
 *
 * Return:    See SDP_AddRecord().  
 */

BtStatus DIPRegisterClient()
{
    BtStatus  status;


    return status;
}




/*---------------------------------------------------------------------------
 *            DipDeregisterSdpServices()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deregisters the SDP services.  
 *
 * Return:    See SDP_RemoveRecord().  
 */
BtStatus DipDeregisterClient(void)
{
    BtStatus status;

    return status;
}




BtStatus DipStartServiceQuery(DipChannel *Chan, SdpQueryMode mode);


/*---------------------------------------------------------------------------
 *            DipVerifySdpQueryRsp()
 *---------------------------------------------------------------------------
 *
//TODO  Î´Íê³É....
 */
static BtStatus DipVerifySdpQueryRsp( SdpQueryToken *token,DipClient *Dipdev)
{
    BtStatus status = BT_STATUS_FAILED;
     U16 val = 0;

     {

        /* Get the special ID */ 
        token->attribId = DIP_AID_SPEC_ID;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->DevieIDSpecVersion = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }

    {

        /* Get the VID */ 
        token->attribId = DIP_AID_VID;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 

                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->VID = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }
    {

        /* Get the pid */ 
        token->attribId = DIP_AID_PID;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->PID = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }
    {

        /* Get the Version */ 
        token->attribId = DIP_AID_VERSION;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->ProductVersion = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }
    {

        /* Get the pid */ 
        token->attribId = DIP_AID_PRI_RECORD;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 1) && 
                ((token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->PrimaryRecord = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }
    {

        /* Get the pid */ 
        token->attribId = DIP_AID_VID_SOURCE;
        token->uuid = 0;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                Dipdev->PnpInfo->VIDSource = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }

done:

    if (status == BT_STATUS_SDP_CONT_STATE) {
        Report(("A2DP: SDP_ParseAttributes - %s\n", 
                "Continuation State. Query Again!"));
        token->mode = BSPM_CONT_STATE;
        return status;
    }

    /* Reset the attribute to parse */ 
    token->attribId = AID_PROTOCOL_DESC_LIST;

    /* Reset the SDP parsing mode */ 
    token->mode = BSPM_BEGINNING;

    /* Return the status of the service/protocol/profile parsing, since 
     * this is the only critical information to obtain a connection.  
     */
        //TODO
//    if (Device->queryFlags & 
//        (A2DP_SDP_QUERY_FLAG_SERVICE | 
//         A2DP_SDP_QUERY_FLAG_PROTOCOL | 
//         A2DP_SDP_QUERY_FLAG_PROFILE)) {
//        return BT_STATUS_SUCCESS;
//    } else {
//        return BT_STATUS_FAILED;
//    }
    return BT_STATUS_SUCCESS;

}

/*---------------------------------------------------------------------------
 * Callback function for queryForService.
 */
static void DipQueryCallback(const BtEvent *event)
{
    BtStatus status;
    SdpQueryToken *token;

    /* The token is a member of a structure within the SppDev structure.
     *      SppDev->type.client.sdpToken (type is a union)
     * thus we can find the address of SppDev.
     */
    token  = event->p.token;
    switch (event->eType) {

    case SDEVENT_QUERY_RSP:
        memset(&PNP_info  , 0, sizeof(PNP_info));
        TRACE("VID = 0x%04X",PNP_info.VID);
        status = DipVerifySdpQueryRsp(token,&Dipdev);
    
        //TRACE("status = %d ",status);
        if (status == BT_STATUS_SUCCESS) {
            /* Value returned should be U8 for channel. */
            TRACE("DIP query complete.");
            TRACE("DevieIDSpecVersion = 0x%04X",PNP_info.DevieIDSpecVersion);
            TRACE("VID = 0x%04X",PNP_info.VID);
            TRACE("PID = 0x%04X",PNP_info.PID);
            TRACE("ProductVersion = 0x%04X",PNP_info.ProductVersion);
            TRACE("PrimaryRecord = %d(bool)",PNP_info.PrimaryRecord);
            TRACE("VIDSource = 0x%04X",PNP_info.VIDSource);
            if(Dipdev.callback!=NULL)
            {
                Dipdev.callback((const DipPnpInfo*) &PNP_info);
            }
        } else if (status == BT_STATUS_SDP_CONT_STATE) {
            /* We need to continue the query. */
            token->mode = BSPM_CONT_STATE;
            status = SDP_Query(token, BSQM_CONTINUE);
            Report(("Dip: SDP_Query() returned %d\n", status));
        }
        break;

    case SDEVENT_QUERY_ERR:
    case SDEVENT_QUERY_FAILED:
        Report(("DIP: SDP Query Failed\n"));
        //LOG_E("DIP: SDP Query Failed %d \n",event->eType);
        if(1){//((SppCallback)0 != dev->callback) {
            TRACE("TODO");
        }

        /* Do we not indicate this back somehow? */
        break;
    default:
        TRACE("etype = %d",event->eType);
        break;
    }
}
#ifdef GET_IAP_INFO
static BtStatus IAPVerifySdpQueryRsp( SdpQueryToken *token,IapClient *IapDev)
{
    BtStatus status = BT_STATUS_FAILED;
     U16 val = 0;

     {

        /* Get the special ID */ 
        token->attribId = AID_PROTOCOL_DESC_LIST;
        token->uuid = PROT_RFCOMM;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 1) && 
                 (token->availValueLen == 1)) {

                Report(("iap: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                IapDev->Iap1Info->channel_number = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }
     {

        /* Get the special ID */ 
        token->attribId = AID_BT_PROFILE_DESC_LIST;
        token->uuid = SC_SERIAL_PORT;

        status = SDP_ParseAttributes(token);
        switch(status) {
        case BT_STATUS_SUCCESS:

            /* We had better have a U16 version */ 
            if ((token->totalValueLen == 2) && 
                ((token->availValueLen == 2) || 
                 (token->availValueLen == 1))) {

                Report(("A2DP: %s AVDTP version = %x,\n",
                        "SDP_ParseAttributes succeeded!", 
                        token->valueBuff[0]));
                Report(("A2DP: Updated Buff Len = %d\n", token->remainLen));
//
//                /* Assign the Profile UUID and Profile Version */ 
                if (token->availValueLen == 1) {
                    //Device->avdtpVersion 
                        //= (U16)(Device->avdtpVersion + token->valueBuff[0]);
                      val =  (U16)(token->valueBuff[0]);
                } else {
                   // Device->avdtpVersion = SDP_GetU16(token->valueBuff);
                   val = SDP_GetU16(token->valueBuff);
                }
                IapDev->Iap1Info->version = val;
            }

            token->mode = BSPM_RESUME;
            //TODO 
            break;

        case BT_STATUS_SDP_CONT_STATE:
            if (token->availValueLen == 1) {

                /* It's possible that only a portion of the data 
                 * might occur.  If so, we'll copy what we have.  
                 * The data arrives in big endian format.  
                 */
            }
            goto done;

        default:
            break;
        }
    }


done:

    if (status == BT_STATUS_SDP_CONT_STATE) {
        Report(("A2DP: SDP_ParseAttributes - %s\n", 
                "Continuation State. Query Again!"));
        token->mode = BSPM_CONT_STATE;
        return status;
    }

    /* Reset the attribute to parse */ 
    token->attribId = AID_PROTOCOL_DESC_LIST;

    /* Reset the SDP parsing mode */ 
    token->mode = BSPM_BEGINNING;

    /* Return the status of the service/protocol/profile parsing, since 
     * this is the only critical information to obtain a connection.  
     */
        //TODO
//    if (Device->queryFlags & 
//        (A2DP_SDP_QUERY_FLAG_SERVICE | 
//         A2DP_SDP_QUERY_FLAG_PROTOCOL | 
//         A2DP_SDP_QUERY_FLAG_PROFILE)) {
//        return BT_STATUS_SUCCESS;
//    } else {
//        return BT_STATUS_FAILED;
//    }
    return BT_STATUS_SUCCESS;

}

/*---------------------------------------------------------------------------
 * Callback function for queryForService.
 */
 
static void IAPQueryCallback(const BtEvent *event)
{
    BtStatus status;
    SdpQueryToken *token;

    /* The token is a member of a structure within the SppDev structure.
     *      SppDev->type.client.sdpToken (type is a union)
     * thus we can find the address of SppDev.
     */
    token  = event->p.token;
    switch (event->eType) {

    case SDEVENT_QUERY_RSP:
        status = IAPVerifySdpQueryRsp(token,&IapDev);
    
        //TRACE("status = %d ",status);
        if (status == BT_STATUS_SUCCESS) {
            /* Value returned should be U8 for channel. */
            LOG_D("IAP query complete.");
            TRACE("channel number = 0x%04X",Iap1Info.channel_number);
            TRACE("version = 0x%04X",Iap1Info.version);

        } else if (status == BT_STATUS_SDP_CONT_STATE) {
            /* We need to continue the query. */
            token->mode = BSPM_CONT_STATE;
            status = SDP_Query(token, BSQM_CONTINUE);
            Report(("Dip: SDP_Query() returned %d\n", status));
        }
        break;

    case SDEVENT_QUERY_ERR:
    case SDEVENT_QUERY_FAILED:
        Report(("DIP: SDP Query Failed\n"));
        //LOG_E("DIP: SDP Query Failed %d \n",event->eType);
        if(1){//((SppCallback)0 != dev->callback) {
            TRACE("TODO");
        }

        /* Do we not indicate this back somehow? */
        break;
    default:
        TRACE("etype = %d",event->eType);
        break;
    }
}
#endif
/*---------------------------------------------------------------------------
 * queryForService() 
 *      Start an SDP query of the specified remoted device for a serial
 *      port.
 *     
 * Requires:
 *      Existing ACL with the remote device.
 *
 * Parameters:
 *      dev     the device to query
 *
 * Returns:
 *      SPP_INVALID_CHANNEL     SDP did not locate the serial port service
 *      On success a valid channel is returned that can be used to complete
 *      a connection to the serial port service on the remote device (see
 *      SPP_Open, sppConnect).
 */
static BtStatus DipQueryForService(DipClient *dev,BtRemoteDevice *btDevice,DipPnpInfo *Pnp)
{
    Assert(dev);
    

    if (! dev) {
        return 0;
    }
    BtStatus temp ;
	dev->sdpToken.parms = DipServiceSearchAttribReq;
	dev->sdpToken.plen = sizeof(DipServiceSearchAttribReq);
	dev->sdpToken.type = BSQT_SERVICE_SEARCH_ATTRIB_REQ; 

    dev->sdpToken.attribId     = AID_PROTOCOL_DESC_LIST;
    dev->sdpToken.uuid         = PROT_UPNP;
    dev->sdpToken.mode         = BSPM_BEGINNING;
    dev->sdpToken.callback     = DipQueryCallback;
    dev->remDev      = btDevice;
    dev->PnpInfo    = Pnp;
    dev->sdpToken.rm = btDevice;
    if ((temp =SDP_Query(&dev->sdpToken, BSQM_FIRST)) == BT_STATUS_PENDING) {
        return BT_STATUS_SUCCESS;
    } else {
        TRACE("Dip error %d",temp);
        return BT_STATUS_FAILED;
    }
} 


#ifdef GET_IAP_INFO

//********************
static BtStatus IapQueryForService(IapClient *dev,BtRemoteDevice *btDevice,IapInfo *Iap1Info)
{
    Assert(dev);
    LOG_E(" ");

    if (! dev) {
        return 0;
    }
    
    LOG_E(" ");
    BtStatus temp ;
	dev->sdpToken.parms = IAP_ServiceSearchAttribReq;
	dev->sdpToken.plen = sizeof(IAP_ServiceSearchAttribReq);
	dev->sdpToken.type = BSQT_SERVICE_SEARCH_ATTRIB_REQ; 

    dev->sdpToken.attribId     = AID_PROTOCOL_DESC_LIST;
    //dev->sdpToken.uuid         = PROT_UPNP;
    dev->sdpToken.mode         = BSPM_BEGINNING;
    dev->sdpToken.callback     = IAPQueryCallback;
    dev->Iap1Info    = Iap1Info;
    dev->remDev      = btDevice;
    dev->sdpToken.rm = btDevice;
    if ((temp =SDP_Query(&dev->sdpToken, BSQM_FIRST)) == BT_STATUS_PENDING) {
        
        LOG_E(" ");
        return BT_STATUS_SUCCESS;
    } else {
        LOG_E("Dip error %d",temp);
        return BT_STATUS_FAILED;
    }
} 
#endif
//*************************

#if XA_CONTEXT_PTR == XA_ENABLED
BtDipContext *dipContext;
#else /* XA_CONTEXT_PTR == XA_ENABLED */
BtDipContext dipContext;
#endif /* XA_CONTEXT_PTR */

BOOL DipAlloc(void)
{
    U8* ptr; 

    /* Fill memory with 0. */ 

#if XA_CONTEXT_PTR == XA_ENABLED
	dipContext = (dipContext*)OS_Pmalloc(sizeof(dipContext));		//alloc dynamically by JIMMY
    ptr = (U8*)dipContext;

#else

    ptr = (U8*)&dipContext;

#endif /* XA_CONTEXT_PTR */ 

    OS_MemSet(ptr, 0, (U32)sizeof(BtDipContext));

    __printf("----> sizeof(hidContext) heap size needed = %d bytes\r\n",sizeof(dipContext));

    return TRUE;
}

void app_DIP_init(DipCallBack cb)
{	   
#if DIP_DEVICE ==XA_ENABLED
    TRACE("app_DIP_init\n");
    /* Initialize Memory */ 
    BOOL status = DipAlloc();
    DIPRegisterSdpServices();
    Dipdev.callback = cb;
#endif
}

void IapQueryForService(BtRemoteDevice *btDevice)
{
    BtRemoteDevice * RmWarring = btDevice;    
#ifdef GET_IAP_INFO
    IapQueryForService(&IapDev,btDevice,&Iap1Info);
#endif
    return;
}

void DipGetRemotePnpInfo(BtRemoteDevice *btDevice)
{
    BtRemoteDevice * RmWarring = btDevice;
#if DIP_DEVICE ==XA_ENABLED

    DipQueryForService(&Dipdev,btDevice,&PNP_info);
    //osDelay(200);

     IapQueryForService(btDevice);
#endif
    return;
}
#endif

