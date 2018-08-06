////////////////////////////////////////////////////////////////
//

#ifndef __RESOURCES_H__
#define __RESOURCES_H__

typedef enum
{
    AUD_ID_POWER_ON = 0x0,
    AUD_ID_POWER_OFF,
    AUD_ID_LANGUAGE_SWITCH,
    
    AUD_ID_NUM_0,  
    AUD_ID_NUM_1,  
    AUD_ID_NUM_2,  
    AUD_ID_NUM_3,  
    AUD_ID_NUM_4,  
    AUD_ID_NUM_5,  
    AUD_ID_NUM_6,  
    AUD_ID_NUM_7,  
    AUD_ID_NUM_8,  
    AUD_ID_NUM_9,  

    AUD_ID_BT_PAIR_ENABLE,           	
    AUD_ID_BT_PAIRING,  	
    AUD_ID_BT_PAIRING_SUC,				 
    AUD_ID_BT_PAIRING_FAIL, 				
    AUD_ID_BT_CALL_REFUSE, 				
    AUD_ID_BT_CALL_OVER, 				
    AUD_ID_BT_CALL_ANSWER, 	
    AUD_ID_BT_CALL_HUNG_UP, 	
    AUD_ID_BT_CALL_INCOMING_CALL, 	
    AUD_ID_BT_CALL_INCOMING_NUMBER,	
    AUD_ID_BT_CHARGE_PLEASE, 	
    AUD_ID_BT_CHARGE_FINISH, 	
    AUD_ID_BT_CLEAR_SUCCESS, 	
    AUD_ID_BT_CLEAR_FAIL, 	
    AUD_ID_BT_CONNECTED,
    AUD_ID_BT_DIS_CONNECT,
    AUD_ID_BT_WARNING,
//Modified by ATX : cc_20180306    
	AUD_ID_BT_VOL_MIN,
	AUD_ID_BT_VOL_MAX,
//Modified by ATX : Leon.He_20180109
	AUD_ID_BT_LONG_DOUBLE_LOW,
	AUD_ID_BT_SHORT_1,
//Modified by ATX : Parker.Wei_2018328
	AUD_ID_BT_USER_1,
	AUD_ID_BT_USER_2,

#ifdef __TWS__
    AUD_ID_BT_TWS_ISMASTER,
    AUD_ID_BT_TWS_ISSLAVE,
    AUD_ID_BT_TWS_SEARCH,
    AUD_ID_BT_TWS_STOPSEARCH,
    AUD_ID_BT_TWS_DISCOVERABLE ,	
    AUD_ID_BT_TWS_LEFTCHNL,
    AUD_ID_BT_TWS_RIGHTCHNL,
    AUD_ID_BT_TWS_CONNECTED,//Modified by ATX : Leon.He_20171123
	AUD_ID_BT_TWS_PAIRING,
#endif    
    
	MAX_RECORD_NUM
}AUD_ID_ENUM;


enum ENUM_RESOURCE_ID
{
    RES_ENGLISH_ID = 0xFF00,
    RES_CHINESE_ID = 0xFF01,

    MAX_RES_LANGUAGE_ID
};

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;

void init_audio_resource(void* gResource);

U8* aud_get_reouce(AUD_ID_ENUM id, UINT32* leng, UINT16* type);

extern U8 BIN_FILE[];

#endif//__RESOURCES_H__

