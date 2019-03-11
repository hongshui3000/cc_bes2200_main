#ifndef __APP_STATUS_IND_H__
#define __APP_STATUS_IND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum APP_STATUS_INDICATION_T {
    APP_STATUS_INDICATION_POWERON = 0,
    APP_STATUS_INDICATION_INITIAL,
    APP_STATUS_INDICATION_PAGESCAN,
    APP_STATUS_INDICATION_POWEROFF,
    APP_STATUS_INDICATION_CHARGENEED,
    APP_STATUS_INDICATION_CHARGING,
    APP_STATUS_INDICATION_FULLCHARGE,
    APP_STATUS_INDICATION_NO_REPEAT_NUM,
    /* repeatable status: */
    APP_STATUS_INDICATION_BOTHSCAN = APP_STATUS_INDICATION_NO_REPEAT_NUM,
    APP_STATUS_INDICATION_CONNECTING,
    APP_STATUS_INDICATION_CONNECTED,
    APP_STATUS_INDICATION_DISCONNECTED,
    APP_STATUS_INDICATION_CALLNUMBER,
    APP_STATUS_INDICATION_INCOMINGCALL,
    APP_STATUS_INDICATION_PAIRSUCCEED,
    APP_STATUS_INDICATION_PAIRFAIL,
    APP_STATUS_INDICATION_HANGUPCALL,
    APP_STATUS_INDICATION_REFUSECALL,
    APP_STATUS_INDICATION_ANSWERCALL,
    APP_STATUS_INDICATION_CLEARSUCCEED,
    APP_STATUS_INDICATION_CLEARFAIL,
    APP_STATUS_INDICATION_WARNING,//Short double low
//Modified by ATX : cc_20180306
	APP_STATUS_INDICATION_VOL_MIN,
	APP_STATUS_INDICATION_VOL_MAX,
	APP_STATUS_INDICATION_SLAVE_PAIRING,
//Modified by ATX : Leon.He_20180109
	APP_STATUS_INDICATION_LONG_DOUBLE_LOW,
	APP_STATUS_INDICATION_SHORT_1,
	APP_STATUS_INDICATION_OTA,
//Modified by ATX : Parker.Wei_2018328
    APP_STATUS_INDICATION_USER_1,
	APP_STATUS_INDICATION_USER_2,

    APP_STATUS_INDICATION_INVALID,
#ifdef __TWS__
	APP_STATUS_INDICATION_TWS_ISMASTER,
	APP_STATUS_INDICATION_TWS_ISSLAVE,
	APP_STATUS_INDICATION_TWS_SEARCH,
	APP_STATUS_INDICATION_TWS_STOPSEARCH,
	APP_STATUS_INDICATION_TWS_DISCOVERABLE ,	
	APP_STATUS_INDICATION_TWS_LEFTCHNL,
	APP_STATUS_INDICATION_TWS_RIGHTCHNL,
	APP_STATUS_INDICATION_TWS_CONNECTED,//Modified by ATX : Leon.He_20171123
	APP_STATUS_INDICATION_TWS_PAIRING,
#endif    

    APP_STATUS_INDICATION_TESTMODE,
    APP_STATUS_INDICATION_TESTMODE1,
    APP_STATUS_INDICATION_VERSION_CHECK,//Modified by ATX : cc_20180423
	APP_STATUS_INDICATION_ATX_FACTORY_MODE,//Modified by ATX : Haorong.Wu_20190219
#if defined(TWS_RBCODEC_PLAYER)
    APP_STATUS_INDICATION_PLAYER_MODE_TWS,
    APP_STATUS_INDICATION_PLAYER_MODE_SD,
    APP_STATUS_INDICATION_PLAYER_MODE_LINEIN,
#endif
    

    APP_STATUS_INDICATION_NUM
}APP_STATUS_INDICATION_T;

int app_status_indication_filter_set(APP_STATUS_INDICATION_T status);
APP_STATUS_INDICATION_T app_status_indication_get(void);
int app_status_indication_set(APP_STATUS_INDICATION_T status);


#ifdef __cplusplus
}
#endif

#endif

