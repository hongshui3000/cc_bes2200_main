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

#include "app_bt_media_manager.h"
#ifdef __TWS__
#include "app_tws.h"
#endif

static void spp_read_thread(const void *arg);

osThreadDef(spp_read_thread, osPriorityHigh, 1024*1);

#define SPP1 0
#define SPP2 1
#define SPP_NUM 2

#define BUF_64 64

extern struct BT_DEVICE_T app_bt_device;

BtRemoteDevice *sppBtDevice = NULL;

spp_mutex_t spp_mutex[SPP_NUM];
OsSppDev    osSppDev[SPP_NUM];

#if SPP_SERVER == XA_ENABLED
SppService  sppService[SPP_NUM];
SdpRecord   sppSdpRecord[SPP_NUM];

#define SPP(id, s) (app_bt_device.spp_dev[id].type.sppService->s)

/****************************************************************************
 * SPP SDP Entries
 ****************************************************************************/

/*---------------------------------------------------------------------------
 *
 * ServiceClassIDList 
 */
static const U8 SppClassId[] = {
    SDP_ATTRIB_HEADER_8BIT(3),        /* Data Element Sequence, 6 bytes */ 
    SDP_UUID_16BIT(SC_SERIAL_PORT),     /* Hands-Free UUID in Big Endian */ 
};

static const U8 SppProtoDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(12),  /* Data element sequence, 12 bytes */ 

    /* Each element of the list is a Protocol descriptor which is a 
     * data element sequence. The first element is L2CAP which only 
     * has a UUID element.  
     */ 
    SDP_ATTRIB_HEADER_8BIT(3),   /* Data element sequence for L2CAP, 3 
                                  * bytes 
                                  */ 

    SDP_UUID_16BIT(PROT_L2CAP),  /* Uuid16 L2CAP */ 

    /* Next protocol descriptor in the list is RFCOMM. It contains two 
     * elements which are the UUID and the channel. Ultimately this 
     * channel will need to filled in with value returned by RFCOMM.  
     */ 

    /* Data element sequence for RFCOMM, 5 bytes */
    SDP_ATTRIB_HEADER_8BIT(5),

    SDP_UUID_16BIT(PROT_RFCOMM), /* Uuid16 RFCOMM */

    /* Uint8 RFCOMM channel number - value can vary */
    SDP_UINT_8BIT(2)
};

/*
 * BluetoothProfileDescriptorList
 */
static const U8 SppProfileDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(8),        /* Data element sequence, 8 bytes */

    /* Data element sequence for ProfileDescriptor, 6 bytes */
    SDP_ATTRIB_HEADER_8BIT(6),

    SDP_UUID_16BIT(SC_SERIAL_PORT),   /* Uuid16 SPP */
    SDP_UINT_16BIT(0x0102)            /* As per errata 2239 */ 
};

/*
 * * OPTIONAL *  ServiceName
 */
static const U8 SppServiceName1[] = {
    SDP_TEXT_8BIT(5),          /* Null terminated text string */ 
    'S', 'p', 'p', '1', '\0'
};

static const U8 SppServiceName2[] = {
    SDP_TEXT_8BIT(5),          /* Null terminated text string */ 
    'S', 'p', 'p', '2', '\0'
};

/* SPP attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * SPP SDP record.
 */
static const SdpAttribute sppSdpAttributes1[] = {

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, SppClassId), 

    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, SppProtoDescList),

    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, SppProfileDescList),

    /* SPP service name*/
    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), SppServiceName1),
};

static const SdpAttribute sppSdpAttributes2[] = {

    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, SppClassId), 

    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, SppProtoDescList),

    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, SppProfileDescList),

    /* SPP service name*/
    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), SppServiceName2),
};

BtStatus SppRegisterSdpServices(SppDev *dev, U8 sppId)
{
    BtStatus  status = BT_STATUS_SUCCESS;
    const U8 *cu8p;

    UNUSED_PARAMETER(dev);

    /* Register SPP SDP Attributes */ 
    /* Assert(sizeof(SPP(sppId, sppSdpAttribute)) == sizeof(sppSdpAttributes1[0])); */

	if (sppId == SPP1)
	{
	    cu8p = (const U8 *)(&sppSdpAttributes1[0]);
    	OS_MemCopy((U8 *)SPP(sppId, sppSdpAttribute), cu8p, sizeof(sppSdpAttributes1));
	}
#if (BT_DEVICE_NUM > 1)
	else if (sppId == SPP2)
	{
		cu8p = (const U8 *)(&sppSdpAttributes2[0]);
		OS_MemCopy((U8 *)SPP(sppId, sppSdpAttribute), cu8p, sizeof(sppSdpAttributes2));
	}
#endif

	SPP(sppId, service).serviceId = 0;
	SPP(sppId, numPorts) = 0;

    SPP(sppId, sdpRecord)->attribs = SPP(sppId, sppSdpAttribute);
    SPP(sppId, sdpRecord)->num = 4;
    SPP(sppId, sdpRecord)->classOfDevice = COD_MAJOR_PERIPHERAL;

    return status;
}
#endif

void spp_test_write()
{
	BtStatus status = BT_STATUS_SUCCESS;
	static const char *buf = "88888";
	U16 len = 5;
	
	status = SPP_Write(&app_bt_device.spp_dev[0], (char *)buf, &len);
	if (status != BT_STATUS_SUCCESS)
	{
		TRACE("spp write failed\n");
	}
}

static void spp_read_thread(const void *arg)
{
	char buffer[BUF_64];
	U16 maxBytes = BUF_64;
	
    while (1)
	{
		TRACE("\nSPP: spp_read_thread -> reading...... \n");
		
		memset(buffer, 0, BUF_64);
		maxBytes = BUF_64;
		
		SPP_Read(&app_bt_device.spp_dev[0], buffer, &maxBytes);

		TRACE("data: \n");

		for (uint8_t i = 0; i < maxBytes; i++)
		{
			TRACE("buffer[%d], %02x", i, buffer[i]);
		}		
    }
}

void spp_create_read_thread(void)
{
	osThreadCreate(osThread(spp_read_thread), NULL);
}

void spp_callback(SppDev *locDev, SppCallbackParms *Info)
{
	TRACE("spp_callback : locDev %p, Info %p\n", locDev, Info);
	TRACE("::spp Info->event %d\n", Info->event);

	if (Info->event == SPP_EVENT_REMDEV_CONNECTED)
	{
		TRACE("::SPP_EVENT_REMDEV_CONNECTED %d\n", Info->event);

		//spp_test_write();
	}
	else if (Info->event == SPP_EVENT_REMDEV_DISCONNECTED)
	{
		TRACE("::SPP_EVENT_REMDEV_DISCONNECTED %d\n", Info->event);
	}
	else
	{
		TRACE("::unknown event %d\n", Info->event);
	}
}

void app_spp_init(void)
{
	for (uint8_t i = 0; i < SPP_NUM; i++)
	{
		app_bt_device.spp_dev[i].portType = SPP_SERVER_PORT;
		app_bt_device.spp_dev[i].osDev	  = (void *)&osSppDev[i];
#if SPP_SERVER == XA_ENABLED
		app_bt_device.spp_dev[i].type.sppService = &sppService[i];
		app_bt_device.spp_dev[i].type.sppService->sdpRecord = &sppSdpRecord[i];
#endif
		osSppDev[i].mutex = (void *)&spp_mutex[i];
		osSppDev[i].sppDev = &app_bt_device.spp_dev[i];
		OS_MemSet(osSppDev[i].sppRxBuffer, 0, SPP_RECV_BUFFER_SIZE);
		osSppDev[i].sppRxLen = 0;
	}

	app_bt_device.numPackets = 0;
	
#if SPP_SERVER == XA_ENABLED
	SppRegisterSdpServices(&app_bt_device.spp_dev[0], SPP1);
	SppRegisterSdpServices(&app_bt_device.spp_dev[1], SPP2);
#endif
}
