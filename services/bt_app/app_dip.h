#ifndef _APP_DIP_H_

#define _APP_DIP_H_
#include "overide.h"

#if DIP_DEVICE == XA_ENABLED
extern "C"{
#include "sdp.h"
#include "conmgr.h"
}
typedef struct _DipCallbackParms DipCallbackParms;
typedef struct _DipChannel   DipChannel;


#define DIP_VENDOR_ID  (0x02B0) //BesTechnic(Shanghai),Ltd
#define DIP_PRODUCT_ID (0x0000) //
#define DIP_PRODUCT_VERSION (0x001F)  //0.1.F



#define SRC_BT (1)
#define SRC_USB (2)
#define SRC_BT_SANSUMG (0x0075)
#define SRC_BT_APPLE   (0X004c)
#define SRC_BT_BRCM   (0X000f)
#define SRC_BT_MEIZU   (0x0046)

#define SRC_BT_MISC_ONE   (0X000f)
#define SRC_BT_MISC_TWO  (0x010F)
#define SRC_BT_MISC_THREE   (0x001D)


#define SRC_USB_APPLE   (0x05AC)

#ifndef SDP_DEFS_H

//Modified by ATX : Leon.He_20180625: PIDs for phones
#define IPHONE_PID_MASK			(0xFF00)
#define SRT_BT_PID_IPHONE6			(0x6E00) //iphone 6: 0x6e00, iphone 6 plus: 0x6e01
#define SRT_BT_PID_IPHONE6S			(0x6F00)//iphone 6s: 0x6f02, iphone 6s plus: 0x6f01
#define SRT_BT_PID_MEIZU				(0x1200)


#define SRT_BT_PID_MISC_ONE				(0x1200)
#define SRT_BT_PID_MISC_TWO				(0x107E)



/* Device Identification (DI)
*/
#define ATTR_ID_SPECIFICATION_ID                0x0200
#define ATTR_ID_VENDOR_ID                       0x0201
#define ATTR_ID_PRODUCT_ID                      0x0202
#define ATTR_ID_PRODUCT_VERSION                 0x0203
#define ATTR_ID_PRIMARY_RECORD                  0x0204
#define ATTR_ID_VENDOR_ID_SOURCE                0x0205

#define BLUETOOTH_DI_SPECIFICATION              0x0103  /* 1.3 */
#define DI_VENDOR_ID_DEFAULT                    0xFFFF
#define DI_VENDOR_ID_SOURCE_BTSIG               0x0001
#define DI_VENDOR_ID_SOURCE_USBIF               0x0002
#endif
typedef struct _BtDipContext {

    SdpAttribute      dipSdpAttribute[9];
    SdpRecord         dipSdpRecord;


} BtDipContext;

struct _DipCallbackParms{
    //HidEvent    event;    /* Event associated with the callback       */

    BtStatus    status;  /* Status of the callback event             */
    BtErrorCode errCode; /* Error code (reason) on disconnect events */
    //HidChannel *chnl;
    
    union {
        BtRemoteDevice *remDev;
        U8 idle_rate;

    } p;
};


typedef void (*DipCallback)(DipChannel *Chan, DipCallbackParms *Info);

typedef struct _DipPnpInfo {
    /* === Internal use only === */
    U16 DevieIDSpecVersion;
    U16 VID;
    U16 PID;
    U16 ProductVersion;
    U16 PrimaryRecord;
    U16 VIDSource;
} DipPnpInfo;
typedef void (*DipCallBack)(const DipPnpInfo*);

typedef struct _DipClient {
    /* === Internal use only === */
    BtRemoteDevice      *remDev;
    DipPnpInfo          *PnpInfo;
    U8                  serverId;
    SdpQueryToken       sdpToken;
    DipCallBack         callback;
} DipClient;




extern BtDipContext dipContext;

#define DIP(s) (dipContext.s)



void app_DIP_init(DipCallBack cb);
void DipGetRemotePnpInfo(BtRemoteDevice *btDevice);
#endif

#endif
