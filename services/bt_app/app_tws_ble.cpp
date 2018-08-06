#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "app_thread.h"
#include "hal_trace.h"
#include "cmsis_os.h"
#include "nvrecord.h"
#include "app_bt.h"
#include "app_tws.h"
#include "tgt_hardware.h"
extern "C" {
#include "sys/mei.h"
#ifdef __TWS__
#include "nvrecord_env.h"
#endif
}

#ifdef __TWS_RECONNECT_USE_BLE__

#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT32_TO_STREAM(p, u32) {*(p)++ = (uint8_t)(u32); *(p)++ = (uint8_t)((u32) >> 8); *(p)++ = (uint8_t)((u32) >> 16); *(p)++ = (uint8_t)((u32) >> 24);}
#define ARRAY_TO_STREAM(p, a, len) {register int ijk; for (ijk = 0; ijk < len;        ijk++) *(p)++ = (uint8_t) a[ijk];}

#define BLE_AD_TYPE_FLAG                 0x01 
#define BLE_AD_TYPE_16SRV_PART           0x02 
#define BLE_AD_TYPE_16SRV_CMPL           0x03 
#define BLE_AD_TYPE_32SRV_PART           0x04 
#define BLE_AD_TYPE_32SRV_CMPL           0x05 
#define BLE_AD_TYPE_128SRV_PART          0x06 
#define BLE_AD_TYPE_128SRV_CMPL          0x07 
#define BLE_AD_TYPE_NAME_SHORT           0x08 
#define BLE_AD_TYPE_NAME_CMPL            0x09 
#define BLE_AD_TYPE_TX_PWR               0x0A 
#define BLE_AD_TYPE_128SOL_SRV_UUID      0x15
#define BLE_AD_TYPE_APPEARANCE           0x19
#define BLE_AD_TYPE_MANU                 0xff 

#define BLE_AD_DATA_LEN                  31
#define MIN_ADV_LENGTH                   2

#define DEFUALT_BLE_FLAG                 0x05

#define INVALID_BLE_ADV_STATE        (0)
#define TWS_RECONNECT_SEND_BLE_ADV   (1 << 0) 
#define BLE_SERVICE_SEND_BLE_ADV     (1 << 1)

#define BOTH_SEND_BLE_ADV            (TWS_RECONNECT_SEND_BLE_ADV  |  BLE_SERVICE_SEND_BLE_ADV)

/* adv tx power level */
#define BTM_BLE_ADV_TX_POWER_MIN        0           /* minimum tx power */
#define BTM_BLE_ADV_TX_POWER_LOW        1           /* low tx power     */
#define BTM_BLE_ADV_TX_POWER_MID        2           /* middle tx power  */
#define BTM_BLE_ADV_TX_POWER_UPPER      3           /* upper tx power   */
#define BTM_BLE_ADV_TX_POWER_MAX        4           /* maximum tx power */

#define LEN_UUID_16     2
#define LEN_UUID_32     4
#define LEN_UUID_128    16
#define MAX_UUID_SIZE   LEN_UUID_128

#define MAX_16B_SERVICE_NUM 8
#define MAX_32B_SERVICE_NUM 4
#define MAX_128B_SERVICE_NUM 2

#define MAX_16B_SERVICE_BUFF (LEN_UUID_16 * MAX_16B_SERVICE_NUM)
#define MAX_32B_SERVICE_BUFF (LEN_UUID_32 * MAX_32B_SERVICE_NUM)
#define MAX_128B_SERVICE_BUFF (LEN_UUID_128 * MAX_128B_SERVICE_NUM)

#if 0
typedef enum {
    BT_STATUS_SUCCESS,
    BT_STATUS_FAIL,
    BT_STATUS_NOT_READY,
    BT_STATUS_NOMEM,
    BT_STATUS_BUSY,
    BT_STATUS_DONE,        /* request already completed */
    BT_STATUS_UNSUPPORTED,
    BT_STATUS_PARM_INVALID,
    BT_STATUS_UNHANDLED,
    BT_STATUS_AUTH_FAILURE,
    BT_STATUS_RMT_DEV_DOWN,
    BT_STATUS_AUTH_REJECTED

} bt_status_t;
#endif

/** Bluetooth 128-bit UUID */
typedef struct {
   uint8_t uu[16];
} bt_uuid_t;

typedef struct
{
    uint16_t          len;

    union
    {
        uint16_t      uuid16;
        uint32_t      uuid32;
        uint8_t       uuid128[MAX_UUID_SIZE];
    } uu;

} tBT_UUID;


static uint8_t ble_adv_state = INVALID_BLE_ADV_STATE;

static int ble_tx_power[] = {-21, -15, -7, 1, 9};

static adv_para_struct ble_tws_reconnect_adv_para = {
    .interval_min =             0x0020, // 20ms
    .interval_max =             0x0020, // 20ms
    .adv_type =                 0x03,
    .own_addr_type =            0x01,
    .peer_addr_type =           0x01,
    .bd_addr = {0},
    .adv_chanmap =              0x07,
    .adv_filter_policy =        0x00
};


static unsigned char BASE_UUID[16] = {
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


static uint8_t ble_tws_reconnect_adv_data[] = {
    0x04, 0x09, 'T', 'W', 'S',  // Ble Name
    0x02, 0xff, 0x00  // manufacturer data
};

static uint16_t service_16b_buff[MAX_16B_SERVICE_NUM];
static uint32_t service_32b_buff[MAX_32B_SERVICE_NUM];
static uint8_t service_128b_buff[MAX_128B_SERVICE_BUFF];

static uint8_t service_16b_num = 0;
static uint8_t service_32b_num = 0;
static uint8_t service_128b_num = 0;

static char* p_service_manu_data_len = NULL;

static adv_para_struct ble_service_adv_para;
static char ble_service_adv_data[BLE_AD_DATA_LEN] = {0};
static uint8_t ble_service_adv_data_len = 0;

void tws_ble_timer_callback(void const *n);
osTimerDef(tws_ble_timer, tws_ble_timer_callback);
osTimerId tws_ble_timerId = NULL;

osMutexId tws_ble_mutex_id = NULL;
osMutexDef(tws_ble_mutex);

extern uint8_t app_tws_get_mode(void);

static void tws_ble_lock_thread(void)
{
    osMutexWait(tws_ble_mutex_id, osWaitForever);
}

static void tws_ble_unlock_thread(void)
{
    osMutexRelease(tws_ble_mutex_id);
}

static int uuidType(unsigned char* p_uuid)
{
    int i = 0;
    int match = 0;
    int all_zero = 1;

    for(i = 0; i != 16; ++i)
    {
        if (i == 12 || i == 13)
            continue;

        if (p_uuid[i] == BASE_UUID[i])
            ++match;

        if (p_uuid[i] != 0)
            all_zero = 0;
    }
    if (all_zero)
        return 0;
    if (match == 12)
        return LEN_UUID_32;
    if (match == 14)
        return LEN_UUID_16;
    return LEN_UUID_128;
}

static void btif_to_bta_uuid(tBT_UUID *p_dest, bt_uuid_t *p_src)
{
    char *p_byte = (char*)p_src;
    int i = 0;

    p_dest->len = uuidType(p_src->uu);

    switch (p_dest->len)
    {
        case LEN_UUID_16:
            p_dest->uu.uuid16 = (p_src->uu[13] << 8) + p_src->uu[12];
            break;

        case LEN_UUID_32:
            p_dest->uu.uuid32  = (p_src->uu[13] <<  8) + p_src->uu[12];
            p_dest->uu.uuid32 += (p_src->uu[15] << 24) + (p_src->uu[14] << 16);
            break;

        case LEN_UUID_128:
            for(i = 0; i != 16; ++i)
                p_dest->uu.uuid128[i] = p_byte[i];
            break;

        default:
            TRACE("%s: Unknown UUID length %d!", __FUNCTION__, p_dest->len);
            break;
    }
}

static void service_uuid_parser(char* service_uuid, uint16_t service_uuid_len)
{
    if (service_uuid && service_uuid_len > 0) {
        uint16_t *p_uuid_out16 = NULL;
        uint32_t *p_uuid_out32 = NULL;
        uint8_t* p_uuid_out128 = NULL;

        memset(service_16b_buff, 0x00, MAX_16B_SERVICE_BUFF);
        memset(service_32b_buff, 0x00, MAX_32B_SERVICE_BUFF);
        memset(service_128b_buff, 0x00, MAX_128B_SERVICE_BUFF);

        service_16b_num = 0;
        service_32b_num = 0;
        service_128b_num = 0;

        p_uuid_out16 = service_16b_buff;
        p_uuid_out32 = service_32b_buff;
        p_uuid_out128 = service_128b_buff; 

        while (service_uuid_len >= LEN_UUID_128)
        {
             bt_uuid_t uuid;
             tBT_UUID bt_uuid;
             memset(&uuid, 0, sizeof(bt_uuid_t));
             memcpy(&uuid.uu, service_uuid, LEN_UUID_128);

             memset(&bt_uuid, 0, sizeof(tBT_UUID));
             btif_to_bta_uuid(&bt_uuid, &uuid);

             switch(bt_uuid.len)
             {
                case (LEN_UUID_16):
                    service_16b_num++;
                    *p_uuid_out16++ = bt_uuid.uu.uuid16;
                    break;
                case (LEN_UUID_32):
                    service_32b_num++;
                    *p_uuid_out32++ = bt_uuid.uu.uuid32;
                    break;
                case (LEN_UUID_128):
                    service_128b_num++;
                    memcpy(p_uuid_out128, bt_uuid.uu.uuid128, LEN_UUID_128);
                    p_uuid_out128 += LEN_UUID_128;
                    break;
                default:
                    break;
             }

             service_uuid += LEN_UUID_128;
             service_uuid_len -= LEN_UUID_128;
        }
    }

}



static void ble_listen_process(void);
void tws_ble_timer_callback(void const *n) {
    TRACE("%s fired", __func__);
    ble_listen_process();
}

static char ble_map_adv_tx_power(int tx_power_index)
{
    if(0 <= tx_power_index && tx_power_index < BTM_BLE_ADV_TX_POWER_MAX)
        return (char)ble_tx_power[tx_power_index];
    return 0;
}

extern const char* BLE_LOCAL_NAME;
static void set_ble_adv_state(uint8_t state)
{
    tws_ble_lock_thread();

    ble_adv_state = state; 

    tws_ble_unlock_thread();
}

static uint8_t get_ble_adv_state(void)
{
    return ble_adv_state;
}

void tws_ble_adv_process(void* arg);
void gatts_ble_adv_process(void* arg);
void gatts_and_tws_ble_adv_process(void* arg);

static void ble_listen_process(void)
{
    uint8_t state;

    state = get_ble_adv_state();

    TRACE("%s state:%d", __func__, state);

    if (state == INVALID_BLE_ADV_STATE) {
        ME_Ble_SetAdv_en(0);
        return; 
    } else if (state == TWS_RECONNECT_SEND_BLE_ADV) {
        app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)tws_ble_adv_process);
    } else if (state == BLE_SERVICE_SEND_BLE_ADV) {
        app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)gatts_ble_adv_process);
    } else if (state == BOTH_SEND_BLE_ADV) {
        app_bt_send_request(APP_BT_REQ_CUSTOMER_CALL, 0 ,0, (uint32_t)gatts_and_tws_ble_adv_process);
    }
}


static uint8_t mix_ble_adv_data(uint8_t* adv_data, uint8_t* adv_data_len)
{
    memcpy(adv_data, ble_service_adv_data, ble_service_adv_data_len);
    if (p_service_manu_data_len) {
       *p_service_manu_data_len += 2;
        memcpy(adv_data + ble_service_adv_data_len, ble_tws_reconnect_adv_data + 6, 2);
        *adv_data_len = ble_service_adv_data_len + 2;
    } else {
        memcpy(adv_data + ble_service_adv_data_len, ble_tws_reconnect_adv_data + 5, 3);
        *adv_data_len = ble_service_adv_data_len + 3;
    }
    return BT_STATUS_SUCCESS;
}

void notify_slave_to_master(void)
{
    uint8_t POSSIBLY_UNUSED state;
    state = get_ble_adv_state();

    TRACE("%s", __func__);
#if defined(IAG_BLE_INCLUDE) && defined(MASTER_IS_BLE_SERVICE)
    set_ble_adv_state(state | BLE_SERVICE_SEND_BLE_ADV);

    ble_listen_process();
#elif defined(IAG_BLE_INCLUDE) && defined(SLAVE_IS_BLE_SERVICE)
    set_ble_adv_state(state & ~BLE_SERVICE_SEND_BLE_ADV);

    ble_listen_process();
#endif
}

void notify_master_to_slave(void)
{
    uint8_t POSSIBLY_UNUSED state;
    state = get_ble_adv_state();

    TRACE("%s", __func__);

#if defined(IAG_BLE_INCLUDE) && defined(MASTER_IS_BLE_SERVICE)
    set_ble_adv_state(state & ~BLE_SERVICE_SEND_BLE_ADV);

    ble_listen_process();
#elif defined(IAG_BLE_INCLUDE) && defined(SLAVE_IS_BLE_SERVICE)
    set_ble_adv_state(state | BLE_SERVICE_SEND_BLE_ADV);

    ble_listen_process();
#endif
}


void tws_reconnect_ble_set_adv_data(uint8_t isConnectedPhone)
{
    TRACE("%s", __func__);
    if (isConnectedPhone) {
        ble_tws_reconnect_adv_data[7] = 1; 
    } else {
        ble_tws_reconnect_adv_data[7] = 0; 
    }
}

void tws_reconnect_ble_set_adv_para(uint8_t isConnectedPhone)
{
    TRACE("%s", __func__);

    if (isConnectedPhone) {
        ble_tws_reconnect_adv_para.interval_min =             0x0320; // 1s
        ble_tws_reconnect_adv_para.interval_max =             0x0320; // 1s
    } else {
        ble_tws_reconnect_adv_para.interval_min =             0x0020; // 20ms
        ble_tws_reconnect_adv_para.interval_max =             0x0020; // 20ms
    }
}

void tws_ble_listen(BOOL listen)
{
    uint8_t state;

    state = get_ble_adv_state();

    TRACE("%s state: %d", __func__, state);

    if (listen && state == INVALID_BLE_ADV_STATE) {
        state = TWS_RECONNECT_SEND_BLE_ADV;
        set_ble_adv_state(state);
        ble_listen_process();
    } else if (listen && state == BLE_SERVICE_SEND_BLE_ADV) {
        state = BOTH_SEND_BLE_ADV; 
        set_ble_adv_state(state);

        ME_Ble_SetAdv_en(0);

        if (tws_ble_timerId) {
            osTimerStart(tws_ble_timerId, 200);
        }
    
    } else if (listen && (state == TWS_RECONNECT_SEND_BLE_ADV || state == BOTH_SEND_BLE_ADV)) {
    
    } else if (!listen && state == TWS_RECONNECT_SEND_BLE_ADV) {
        state = INVALID_BLE_ADV_STATE;
        set_ble_adv_state(state);

        ME_Ble_SetAdv_en(0);
    } else if (!listen && state == BOTH_SEND_BLE_ADV) {
        state = BLE_SERVICE_SEND_BLE_ADV;
        set_ble_adv_state(state);

        ME_Ble_SetAdv_en(0);

        if (tws_ble_timerId) {
            osTimerStart(tws_ble_timerId, 200);
        }
    } else if (!listen && state == BLE_SERVICE_SEND_BLE_ADV) {
    
    }
}

void gatts_ble_adv_process(void* arg)
{
    TRACE("%s enter", __func__);
    tws_ble_lock_thread();

    ME_Ble_SetAdv_parameters(&ble_service_adv_para);

    ME_Ble_SetAdv_data(ble_service_adv_data_len, (uint8_t*)ble_service_adv_data);

    ME_Ble_SetAdv_en(1);

    tws_ble_unlock_thread();
    TRACE("%s exit", __func__);
}

void tws_ble_adv_process(void* arg)
{
    TRACE("%s", __func__);
    tws_ble_lock_thread();

    ME_Ble_Set_Private_Address((BT_BD_ADDR *)&bt_addr[0]);

    ME_Ble_SetAdv_parameters(&ble_tws_reconnect_adv_para);

    ME_Ble_SetAdv_data(sizeof(ble_tws_reconnect_adv_data), ble_tws_reconnect_adv_data);

    ME_Ble_SetAdv_en(1);

    tws_ble_unlock_thread();
}

void gatts_and_tws_ble_adv_process(void* arg)
{
    uint8_t adv_data[BLE_AD_DATA_LEN] = {0};
    uint8_t adv_data_len = 0;
    adv_para_struct adv_para; 

    TRACE("%s", __func__);

    tws_ble_lock_thread();
    ME_Ble_Set_Private_Address((BT_BD_ADDR *)&bt_addr[0]);

    adv_para = ble_service_adv_para;
    adv_para.own_addr_type = 0x01;
    adv_para.interval_min = ble_tws_reconnect_adv_para.interval_min;
    adv_para.interval_max = ble_tws_reconnect_adv_para.interval_max;
    ME_Ble_SetAdv_parameters(&adv_para);

    mix_ble_adv_data(adv_data, &adv_data_len);

    ME_Ble_SetAdv_data(adv_data_len, adv_data);

    ME_Ble_SetAdv_en(1);

    tws_ble_unlock_thread();
}


void tws_ble_init(void)
{
    if (tws_ble_timerId == NULL)
        tws_ble_timerId = osTimerCreate(osTimer(tws_ble_timer), osTimerOnce, (void *)0);

    if (tws_ble_mutex_id == NULL)
        tws_ble_mutex_id = osMutexCreate((osMutex(tws_ble_mutex)));
}

extern "C" uint8_t gatts_tws_listen(BOOL start)
{
    uint8_t state;
    struct nvrecord_env_t *nvrecord_env;

    TRACE("%s", __func__);

    nv_record_env_get(&nvrecord_env);

    state = get_ble_adv_state();

#if defined(MASTER_IS_BLE_SERVICE)
    if ((app_tws_get_mode() == TWSMASTER) || ((nvrecord_env->tws_mode.mode == TWSMASTER) && (app_tws_get_mode() != TWSMASTER)) || (nvrecord_env->tws_mode.mode != TWSMASTER && (nvrecord_env->tws_mode.mode != TWSSLAVE)))
#elif defined(SLAVE_IS_BLE_SERVICE)
    if ((app_tws_get_mode() == TWSSLAVE) || ((nvrecord_env->tws_mode.mode == TWSSLAVE) && (app_tws_get_mode() != TWSSLAVE)) || (nvrecord_env->tws_mode.mode != TWSMASTER && (nvrecord_env->tws_mode.mode != TWSSLAVE)))

#elif defined(BOTH_IS_BLE_SERVICE)

#else
    return BT_STATUS_SUCCESS;
#endif
    {
        if (start && state == INVALID_BLE_ADV_STATE) {
            state = BLE_SERVICE_SEND_BLE_ADV;
            set_ble_adv_state(state);
            ble_listen_process();
        } else if (start && state == TWS_RECONNECT_SEND_BLE_ADV) {
            state = BOTH_SEND_BLE_ADV; 
            set_ble_adv_state(state);

            ME_Ble_SetAdv_en(0);

            if (tws_ble_timerId) {
                osTimerStart(tws_ble_timerId, 200);
            }

        } else if (start && (state == BLE_SERVICE_SEND_BLE_ADV || state == BOTH_SEND_BLE_ADV)) {

            return BT_STATUS_SUCCESS;
        } else if (!start && state == BLE_SERVICE_SEND_BLE_ADV) {
            state = INVALID_BLE_ADV_STATE;
            set_ble_adv_state(state);

            ME_Ble_SetAdv_en(0);
        } else if (!start && state == BOTH_SEND_BLE_ADV) {
            state = TWS_RECONNECT_SEND_BLE_ADV;
            set_ble_adv_state(state);

            ME_Ble_SetAdv_en(0);

            if (tws_ble_timerId) {
                osTimerStart(tws_ble_timerId, 200);
            }
        } else if (!start && state == TWS_RECONNECT_SEND_BLE_ADV) {
        
        }
    }
    return BT_STATUS_SUCCESS;
}

extern "C" uint8_t gatts_tws_set_adv_param(uint16_t interval_min, uint16_t interval_max, 
                               uint8_t adv_type, uint8_t own_addr_type, 
                               uint8_t peer_addr_type,
                               uint8_t adv_chanmap, uint8_t adv_filter_policy)
{
    TRACE("%s", __func__);
    ble_service_adv_para.interval_min =            interval_min; 
    ble_service_adv_para.interval_max =            interval_max;
    ble_service_adv_para.adv_type =                adv_type;
    ble_service_adv_para.own_addr_type =           own_addr_type;
    ble_service_adv_para.peer_addr_type =          peer_addr_type;
    ble_service_adv_para.adv_chanmap =             adv_chanmap;
    ble_service_adv_para.adv_filter_policy =       adv_filter_policy;
    memset(ble_service_adv_para.bd_addr.addr, 0x00, BD_ADDR_SIZE);

    return BT_STATUS_SUCCESS;
}

extern "C" uint8_t gatts_tws_set_adv_data(bool set_scan_rsp, bool include_name,
                           bool include_txpower, int appearance,
                           uint16_t manufacturer_len, char* manufacturer_data,
                           uint16_t service_data_len, char* service_data,
                           uint16_t service_uuid_len, char* service_uuid)
{
    char *p = ble_service_adv_data;
    uint8_t len = BLE_AD_DATA_LEN;
    uint8_t cp_len = 0;
    uint8_t i;

    p_service_manu_data_len = NULL; 

    TRACE("%s", __func__);

    *p++ = MIN_ADV_LENGTH; // MIN_ADV_LENGTH 
    *p++ = BLE_AD_TYPE_FLAG; // adv type flag 
    *p++ = DEFUALT_BLE_FLAG; // flag
    len -= 3;

    if (set_scan_rsp) {

    }

    if (len > 3 && appearance) {
        *p++ = 3; // length 
        *p++ = BLE_AD_TYPE_APPEARANCE; // adv type appearance
        UINT16_TO_STREAM(p, appearance);
        len -= 4;
    }

    if (len > MIN_ADV_LENGTH && include_name) {
        if (strlen(BLE_LOCAL_NAME) > (uint16_t)(len - MIN_ADV_LENGTH)) {
            *p++ = len - MIN_ADV_LENGTH + 1;
            *p++ = BLE_AD_TYPE_NAME_SHORT;
            ARRAY_TO_STREAM(p, BLE_LOCAL_NAME, len - MIN_ADV_LENGTH);
        } 
        else {
            cp_len = (uint16_t)strlen(BLE_LOCAL_NAME); 
            *p++ = cp_len + 1;
            *p++ = BLE_AD_TYPE_NAME_CMPL;
            ARRAY_TO_STREAM(p, BLE_LOCAL_NAME, cp_len);
        }
        len -= (cp_len + MIN_ADV_LENGTH);
    }

    if (len > MIN_ADV_LENGTH && include_txpower) {
        *p++ = MIN_ADV_LENGTH; // length;
        *p++ = BLE_AD_TYPE_TX_PWR; // type
        *p++ = ble_map_adv_tx_power(0);
        len -= 3;
    }

    if (service_data) {
    
    }

    if (service_uuid && service_uuid_len) {

        service_uuid_parser(service_uuid, service_uuid_len);

        if (len > MIN_ADV_LENGTH && service_16b_num) {
            if (service_16b_num * LEN_UUID_16 > (len - MIN_ADV_LENGTH)) {
                cp_len = (len - MIN_ADV_LENGTH) / LEN_UUID_16; 
                *p ++ = 1 + cp_len * LEN_UUID_16;
                *p++ = BLE_AD_TYPE_16SRV_PART;
            } else {
                cp_len = service_16b_num;
                *p++ = 1 + cp_len * LEN_UUID_16;
                *p++ = BLE_AD_TYPE_16SRV_CMPL;
            }
            for (i = 0; i < cp_len; i++) {
                UINT16_TO_STREAM(p, service_16b_buff[i]);
            }

            len -= (cp_len * LEN_UUID_16 + MIN_ADV_LENGTH);
        }

        if (len > MIN_ADV_LENGTH && service_32b_num) {

            if (service_32b_num * LEN_UUID_32 > (len - MIN_ADV_LENGTH)) {
                cp_len = (len - MIN_ADV_LENGTH) / LEN_UUID_32; 
                *p ++ = 1 + cp_len * LEN_UUID_32;
                *p++ = BLE_AD_TYPE_32SRV_PART;
            } else {
                cp_len = service_32b_num;
                *p++ = 1 + cp_len * LEN_UUID_32;
                *p++ = BLE_AD_TYPE_32SRV_CMPL;
            }
            for (i = 0; i < cp_len; i++) {
                UINT32_TO_STREAM(p, service_32b_buff[i]);
            }

            len -= (cp_len * LEN_UUID_32 + MIN_ADV_LENGTH);

        }

        if (len > (MAX_UUID_SIZE + MIN_ADV_LENGTH) && service_128b_num) {

            *p ++ = 1 + MAX_UUID_SIZE;
            *p++ = BLE_AD_TYPE_128SOL_SRV_UUID;
            ARRAY_TO_STREAM(p, service_128b_buff, MAX_UUID_SIZE);
            len -= (MAX_UUID_SIZE + MIN_ADV_LENGTH);
        }
    }
    
    if (len > MIN_ADV_LENGTH && manufacturer_data) {  // manufacturer data
        if (manufacturer_len > (len - MIN_ADV_LENGTH))
            cp_len = len - MIN_ADV_LENGTH;
        else 
            cp_len = manufacturer_len;
    
        p_service_manu_data_len = p; 

        *p++ = cp_len + 1;
        *p++ = BLE_AD_TYPE_MANU;
        ARRAY_TO_STREAM(p, manufacturer_data, cp_len);

        len -= (cp_len + MIN_ADV_LENGTH);
    }

    ble_service_adv_data_len = BLE_AD_DATA_LEN - len;

    return BT_STATUS_SUCCESS;
}

#endif
