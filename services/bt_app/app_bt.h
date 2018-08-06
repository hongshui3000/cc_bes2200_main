#ifndef __APP_BT_H__
#define __APP_BT_H__

#ifdef __cplusplus
extern "C" {
#endif

enum APP_BT_REQ_T {
    APP_BT_REQ_ACCESS_MODE_SET,
    APP_BT_REQ_CONNECT_PROFILE,
    APP_BT_REQ_CUSTOMER_CALL,
    APP_BT_REQ_NUM
};

//Modified by ATX : Leon.He_20180622: move AAC bitrate define here
#define MAX_AAC_BITRATE_192K (196608)
#define MAX_AAC_BITRATE_256K (264630)

#ifdef _FORCE_TO_LIMIT_MAX_AAC_BITRATE_192K_
#define MAX_AAC_BITRATE MAX_AAC_BITRATE_192K
#else
#define MAX_AAC_BITRATE MAX_AAC_BITRATE_256K
#endif

typedef void (*APP_BT_REQ_CUSTOMER_CALl_CB_T)(void *, void *);


typedef void (*APP_BT_REQ_CONNECT_PROFILE_FN_T)(void *, void *);
#define app_bt_accessmode_set_req(accmode) do{app_bt_send_request(APP_BT_REQ_ACCESS_MODE_SET, accmode, 0, 0);}while(0)
#define app_bt_connect_profile_req(fn,param0,param1) do{app_bt_send_request(APP_BT_REQ_CONNECT_PROFILE, (uint32_t)param0, (uint32_t)param1, (uint32_t)fn);}while(0)

void PairingTransferToConnectable(void);

void PairingTimeoutHandler(void);


void app_bt_golbal_handle_init(void);

void app_bt_opening_reconnect(void);

void app_bt_accessmode_set(BtAccessibleMode mode);

void app_bt_accessmode_get(BtAccessibleMode *mode);

void app_bt_send_request(uint32_t message_id, uint32_t param0, uint32_t param1, uint32_t ptr);

void app_bt_init(void);

int app_bt_state_checker(void);
void app_bt_disconnect_all(void);

void *app_bt_profile_active_store_ptr_get(uint8_t *bdAddr);

void app_bt_profile_connect_manager_open(void);

void app_bt_profile_connect_manager_opening_reconnect(void);

BOOL app_bt_profile_connect_openreconnecting(void *ptr);
//Modified by ATX : cc_20180317: Has Profile Record?
bool app_bt_has_profile_record(U8 *addr);
//@20180304 by parker.wei  Is mobile conneted?
bool app_bt_mobile_hasConnected (void);
bool app_bt_has_connectivitys(void);

//Modified by ATX : cc_20180413  ble adv for check version 
void app_check_version_ble_adv_start(void);
//Modified by ATX : parker.wei_20180309 
#ifdef __HF_ATCMD_SEND_ 
bool app_hfp_send_accessory_cmd();
#endif

#ifdef __cplusplus
}
#endif
#endif /* BESBT_H */
