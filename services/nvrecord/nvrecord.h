#ifndef SECTIONS_ADP_H
#define SECTIONS_ADP_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>
#include "me.h"
#include "cmsis_os.h"
#include "cmsis.h"
#include "nvrec_config.h"

#define DDB_RECORD_NUM  10
#define section_name_ddbrec     "ddbrec"
#define nvrecord_mem_pool_size      4096
#define nvrecord_tag    "nvrecord"
#define nvdev_tag       "nvdev_tag"
#define section_name_other     "other"

/*
    version|magic           16bit|16bit             0001 cb91
    crc                            32bit
    reserve[0]                 32bit
    reserve[1]                 32bit
    config_addr                32bit
    heap_contents            32bit
    mempool                   ...
*/
#define nvrecord_struct_version         1
#define nvrecord_magic                      0xcb91

#ifdef _PROJ_2000IZ_C002_ //Modified by ATX : Parke.Wei_20180411
#define nv_record_debug
#endif


#ifdef _PROJ_2000IZ_C003_ //Modified by ATX : Parke.Wei_20180411
#define nv_record_debug
#endif

//#define nv_record_debug
#ifdef nv_record_debug
#define nvrec_trace     TRACE
#else
#define nvrec_trace(...)
#endif

extern const char sys_build_info[];

typedef enum
{
    pos_version_and_magic,// 0
    pos_crc,                            // 1
    pos_reserv1,                    // 2
    pos_crc_buildinfo = pos_reserv1,                            // 2
    pos_reserv2,                    // 3
    pos_config_addr,            // 4
    pos_heap_contents,      // 5

}nvrec_mempool_pre_enum;



/*
    this is the nvrec dev zone struct :
    version|magic   16bit|16bit
    crc         32bit
    reserve[0]      32bit
    reserv[1]       32bit
    dev local name  max 249*8bit
    dev bt addr         64bit
    dev ble addr        64bit
    calib data      32bit
*/
#define nvrec_dev_version   1
#define nvrec_dev_magic      0xba80
typedef enum
{
    dev_version_and_magic,      //0
    dev_crc,                    // 1
    dev_reserv1,                // 2
    dev_fw_version = dev_reserv1,                // 2
    dev_reserv2,                //3// 3
    dev_name,                   //[4~66]
    dev_bt_addr = 67,                //[67~68]
    dev_ble_addr = 69,               //[69~70]
    dev_dongle_addr = 71,
    dev_xtal_fcap = 73,              //73
    dev_data_len,
}nvrec_dev_enum;

#ifdef __SAVE_FW_VERSION_IN_NV_FLASH__
typedef struct _nv_fw_ver
{
    uint16_t fw_version;
    uint8_t tws_channel;
    uint8_t reserved;
}nv_fw_ver;

#define NV_FW_VERSION_NULL 0
#define NV_FW_VERSION 7

typedef enum
{
    dev_tws_channel_none,      //0
	dev_tws_channel_right,                    // 1
	dev_tws_channel_left
}nvrec_dev_tws_channel;
#endif //__SAVE_FW_VERSION_IN_NV_FLASH__

//#define mempool_pre_offset       (((sizeof(nvrec_config_t *)+sizeof(uint32_t))/sizeof(uint32_t))+((sizeof(buffer_alloc_ctx)/sizeof(uint32_t))+1))
#define mempool_pre_offset       (1+pos_config_addr+((sizeof(buffer_alloc_ctx)/sizeof(uint32_t))+1))

typedef struct
{
    bool is_update;
    nvrec_config_t *config;
}nv_record_struct;

typedef enum
{
    section_usrdata_ddbrecord,
    section_usrdata_other,
    section_source_ring,
    section_source_others,
    section_factory_what,
    section_none
}SECTIONS_ADP_ENUM;

typedef  struct btdevice_volume{
    int8_t a2dp_vol;
    int8_t hfp_vol;
}btdevice_volume;

typedef  struct btdevice_profile{
    bool hfp_act;
    bool hsp_act;
    bool a2dp_act;
    uint8_t a2dp_codectype;
}btdevice_profile;

typedef struct _nvrec_btdevicerecord
{
    BtDeviceRecord record;
    btdevice_volume device_vol;
    btdevice_profile device_plf;
} nvrec_btdevicerecord;

BtStatus nv_record_open(SECTIONS_ADP_ENUM section_id);
BtStatus nv_record_enum_dev_records(unsigned short index,BtDeviceRecord* record);
BtStatus nv_record_add(SECTIONS_ADP_ENUM type,void *record);
BtStatus nv_record_ddbrec_find(const BT_BD_ADDR *bd_ddr,BtDeviceRecord *record);
BtStatus nv_record_ddbrec_delete(const BT_BD_ADDR *bdaddr);
int nv_record_btdevicerecord_find(const BT_BD_ADDR *bd_ddr, nvrec_btdevicerecord **record);
int nv_record_touch_cause_flush(void);
#ifdef __DUAL_USER_SECTION_BAK__
void nv_record_flash_backup(void);
#endif
void nv_record_sector_clear(void);
void nv_record_flash_flush(void);
int nv_record_enum_latest_two_paired_dev(BtDeviceRecord* record1,BtDeviceRecord* record2);
void nv_record_all_ddbrec_print(void);
int nvrec_dev_get_dongleaddr(BT_BD_ADDR *dongleaddr);
#ifdef __SAVE_FW_VERSION_IN_NV_FLASH__
void nvrec_fw_version_init(void);
#endif
size_t is_paired_devices_null(void);

#ifdef __cplusplus
}
#endif
#endif
