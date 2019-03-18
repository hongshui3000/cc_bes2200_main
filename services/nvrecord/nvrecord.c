#include <assert.h>
#include "hal_sleep.h"
#include "besbt.h"
#include "nvrecord.h"
#include "nvrecord_dev.h"
#include "nvrecord_env.h"
#include "nvrec_mempool.h"
#include "crc32.h"
#include "btalloc.h"
#include "hal_timer.h"
#include "hal_norflash.h"
#include "pmu.h"

#define NVRECORD_CACHE_2_UNCACHE(addr) \
    ((unsigned char *)((unsigned int)addr & ~(0x04000000)))

extern uint32_t __factory_start[];
extern uint32_t __userdata_start[];
//Modified by ATX : Leon.He_20180530: add bak update flag for dual user section bak
#ifdef __DUAL_USER_SECTION_BAK__
extern uint32_t __userdata_start_backup[];
#endif
extern int app_audio_mempool_init();
extern int app_audio_mempool_get_buff(uint8_t **buff, uint32_t size);
nv_record_struct nv_record_config;
static bool nvrec_init = false;
static bool nvrec_mempool_init = false;
static bool dev_sector_valid = false;
static uint32_t usrdata_ddblist_pool[1024] __attribute__((section(".userdata_pool")));
static void nv_record_print_dev_record(const BtDeviceRecord* record)
{
#ifdef nv_record_debug

    nvrec_trace("nv record bdAddr = ");
    DUMP8("%x ",record->bdAddr.addr,sizeof(record->bdAddr.addr));
    nvrec_trace("record_trusted = ");
    DUMP8("%d ",&record->trusted,sizeof((uint8_t)record->trusted));
    nvrec_trace("record_linkKey = ");
    DUMP8("%x ",record->linkKey,sizeof(record->linkKey));
    nvrec_trace("record_keyType = ");
    DUMP8("%x ",&record->keyType,sizeof(record->keyType));
    nvrec_trace("record_pinLen = ");
    DUMP8("%x ",&record->pinLen,sizeof(record->pinLen));
#endif
}

static void nv_record_mempool_init(void)
{
    unsigned char *poolstart = 0;

    poolstart = (unsigned char *)(usrdata_ddblist_pool+mempool_pre_offset);
    if(!nvrec_mempool_init)
    {
        memory_buffer_alloc_init((unsigned char*)poolstart, (size_t)(sizeof(usrdata_ddblist_pool)-(mempool_pre_offset*sizeof(uint32_t))));
        //nvrec_trace("%s,ln = %d.",nvrecord_tag,__LINE__);
        nvrec_mempool_init = TRUE;
    }
    /*add other memory pool */

}

//Modified by ATX : Leon.He_20180530: if factory section already flashed, the bak data will be invalid
#ifdef __DUAL_USER_SECTION_BAK__
static bool is_sys_rebuilded(void)
{
    uint32_t crc_buildinfo;
    TRACE("%s",__func__);
	crc_buildinfo = crc32(0,(uint8_t *)(sys_build_info),(strlen(sys_build_info)));
    if(crc_buildinfo !=usrdata_ddblist_pool[pos_crc_buildinfo] )
    {
        TRACE("BUILD INFO CHANGE");
        return true;
    }
    return false;
}

static void copy_bak_userdata_into_userdata_pool(void)
{
    memcpy((void *)usrdata_ddblist_pool,(void *)__userdata_start_backup,sizeof(usrdata_ddblist_pool));
}

#endif

//Modified by ATX : Leon.He_20180605: separate valid condition judge and update uer data pool
static void copy_userdata_into_userdata_pool(void)
{
	memcpy((void*) usrdata_ddblist_pool, (void*) __userdata_start,sizeof(usrdata_ddblist_pool));
}

static void reset_userdata_pool(void)
{
    memset(usrdata_ddblist_pool,0,sizeof(usrdata_ddblist_pool));
}

static bool is_userdata_pool_crc_valid(void)
{
    uint32_t crc;
    uint32_t flsh_crc;
    crc = crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    nvrec_trace("%s,crc=0x%x",nvrecord_tag,crc);
    flsh_crc = usrdata_ddblist_pool[pos_crc];
    nvrec_trace("%s,read crc from flash is 0x%x",nvrecord_tag,flsh_crc);
    if (flsh_crc == crc)
        return true;
    return false;
}
static bool nv_record_data_is_valid(void)
{
    uint32_t verandmagic;
    TRACE("%s",__func__);
    verandmagic = usrdata_ddblist_pool[0];
    if(((nvrecord_struct_version<<16)|nvrecord_magic) != verandmagic)
    	return false;
    return is_userdata_pool_crc_valid();
}

static void nv_record_sync_userdata_pool_to_heapctx(void)
{
	buffer_alloc_ctx *heapctx = memory_buffer_heap_getaddr();
	memcpy(heapctx,(void *)(&usrdata_ddblist_pool[pos_heap_contents]),sizeof(buffer_alloc_ctx));
}

static void nv_record_handle_valid_userdata(void)
{
    TRACE(" %s ", __func__);
	nv_record_config.config = (nvrec_config_t *)usrdata_ddblist_pool[pos_config_addr];
	nv_record_sync_userdata_pool_to_heapctx();
	mem_pool_set_malloc_free();
}

static void nv_record_reinit_with_invalid_userdata(void)
{
    TRACE(" %s ", __func__);
    reset_userdata_pool();
    nv_record_mempool_init();
    nv_record_config.config = nvrec_config_new("PRECIOUS 4K.");
    nv_record_config.is_update = true;
    nvrec_trace("%s,nv_record_config.config=0x%x,ln=%d\n",nvrecord_tag,nv_record_config.config,__LINE__);
    nv_record_flash_flush();
}

#ifdef __DUAL_USER_SECTION_BAK__
static bool is_backup_different_with_userdata(void)
{
	return __userdata_start_backup[pos_crc]!= crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
}
#endif

BtStatus nv_record_open(SECTIONS_ADP_ENUM section_id)
{
    BtStatus ret_status = BT_STATUS_FAILED;

    if(nvrec_init)
        return BT_STATUS_SUCCESS;
    switch(section_id)
    {
        case section_usrdata_ddbrecord:
        {
//Modified by ATX : Leon.He_20180605: refactor open nv record and support dual bak
        	copy_userdata_into_userdata_pool();
            if(nv_record_data_is_valid())
            {
            	TRACE("%s,user data valid.",__func__);
            	nv_record_handle_valid_userdata();
#ifdef __DUAL_USER_SECTION_BAK__		
#ifndef _QUICK_PWR_ON_//Modified by ATX : Leon.He_20190315: only do pending  dual bak flush, reduce power on time		
				if(is_backup_different_with_userdata())
					nv_record_flash_backup();
#endif
#endif//__DUAL_USER_SECTION_BAK__				
                ret_status = BT_STATUS_SUCCESS;
                break;
            }
#ifdef __DUAL_USER_SECTION_BAK__
            copy_bak_userdata_into_userdata_pool();
            if(nv_record_data_is_valid() && false == is_sys_rebuilded())
            {
            	TRACE("%s,user bak data valid.",__func__);
            	nv_record_handle_valid_userdata();
                nv_record_config.is_update = true;
				nv_record_flash_flush();
                ret_status = BT_STATUS_SUCCESS;
                break;
            }
#endif			
			TRACE("%s,both user data and bak data invalid.",__func__);
			nv_record_reinit_with_invalid_userdata();
			ret_status = BT_STATUS_SUCCESS;
        }
            break;
        case section_source_ring:

            break;
        default:
            break;
    }
    nvrec_init = true;
    if (ret_status == BT_STATUS_SUCCESS)
        hal_sleep_set_lowpower_hook(HAL_SLEEP_HOOK_USER_1, nv_record_flash_flush);
    nvrec_trace("%s,open,ret_status=%d\n",nvrecord_tag,ret_status);
    return ret_status;
}

static size_t config_entries_get_ddbrec_length(const char *secname)
{
    size_t entries_len = 0;

    if(NULL != nv_record_config.config)
    {
        nvrec_section_t *sec = NULL;
        sec = nvrec_section_find(nv_record_config.config,secname);
        if (NULL != sec)
            entries_len = sec->entries->length;
    }
    return entries_len;
}

#if 0
static bool config_entries_has_ddbrec(const BtDeviceRecord* param_rec)
{
    char key[BD_ADDR_SIZE+1] = {0,};
    assert(param_rec != NULL);

    memcpy(key,param_rec->bdAddr.addr,BD_ADDR_SIZE);
    if(nvrec_config_has_key(nv_record_config.config,section_name_ddbrec,(const char *)key))
        return true;
    return false;
}
#endif

static void config_entries_ddbdev_delete_head(void)//delete the oldest record.
{
    list_node_t *head_node = NULL;
    list_t *entry = NULL;
    nvrec_section_t *sec = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    entry = sec->entries;
    head_node = list_begin_ext(entry);
    if(NULL != head_node)
    {
        BtDeviceRecord *rec = NULL;
        unsigned int recaddr = 0;

        nvrec_entry_t *entry_temp = list_node_ext(head_node);
        recaddr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,entry_temp->key,0);
        rec = (BtDeviceRecord *)recaddr;
        pool_free(rec);
        nvrec_entry_free(entry_temp);
         if(1 == config_entries_get_ddbrec_length(section_name_ddbrec))
            entry->head = entry->tail = NULL;
        else
            entry->head = list_next_ext(head_node);
        pool_free(head_node);
        entry->length -= 1;
    }
}

static void config_entries_ddbdev_delete_tail(void)
{
    list_node_t *node;
    list_node_t *temp_ptr;
    list_node_t *tailnode;
    size_t entrieslen = 0;
    nvrec_entry_t *entry_temp = NULL;
    nvrec_section_t *sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    BtDeviceRecord *recaddr = NULL;
    unsigned int addr = 0;

    if (!sec)
        assert(0);
    sec->entries->length -= 1;
    entrieslen = sec->entries->length;
    node = list_begin_ext(sec->entries);
    while(entrieslen > 1)
    {
        node = list_next_ext(node);
        entrieslen--;
    }
    tailnode = list_next_ext(node);
    entry_temp = list_node_ext(tailnode);
    addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,entry_temp->key,0);
    recaddr = (BtDeviceRecord *)addr;
    pool_free(recaddr);
    nvrec_entry_free(entry_temp);
    //pool_free(entry_temp);
    temp_ptr = node->next;
    node->next = NULL;
    pool_free(temp_ptr);
    sec->entries->tail = node;
}

static void config_entries_ddbdev_delete(const BtDeviceRecord* param_rec)
{
    nvrec_section_t *sec = NULL;
    list_node_t *entry_del = NULL;
    list_node_t *entry_pre = NULL;
    list_node_t *entry_next = NULL;
    list_node_t *node = NULL;
    BtDeviceRecord *recaddr = NULL;
    unsigned int addr = 0;
    int pos = 0,pos_pre=0;
    nvrec_entry_t *entry_temp = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    for(node=list_begin_ext(sec->entries);node!=NULL;node=list_next_ext(node))
    {
        nvrec_entry_t *entries = list_node_ext(node);
        pos++;
        if(0 == memcmp(entries->key,param_rec->bdAddr.addr,BD_ADDR_SIZE))
        {
            entry_del = node;
            entry_next = entry_del->next;
            break;
        }
    }

    if (entry_del){
        /*get entry_del pre node*/
        pos_pre = (pos-1);
        pos=0;
        node=list_begin_ext(sec->entries);
        pos++;
        while(pos < pos_pre)
        {
            node=list_next_ext(node);
            pos += 1;
        }
        entry_pre = node;

        /*delete entry_del following...*/
        entry_temp = list_node_ext(entry_del);
        addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)entry_temp->key,0);
        assert(0!=addr);
        recaddr = (BtDeviceRecord *)addr;
        pool_free(recaddr);
        nvrec_entry_free(entry_temp);
        //pool_free(entry_temp);
        pool_free(entry_pre->next);
        entry_pre->next = entry_next;
        sec->entries->length -= 1;
    }
}

static bool config_entries_ddbdev_is_head(const BtDeviceRecord* param_rec)
{
    list_node_t *head_node = NULL;
    nvrec_section_t *sec = NULL;
    list_t *entry = NULL;
    nvrec_entry_t *entry_temp = NULL;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    entry = sec->entries;
    head_node = list_begin_ext(entry);
    entry_temp = list_node_ext(head_node);
    if(0 == memcmp(entry_temp->key,param_rec->bdAddr.addr,BD_ADDR_SIZE))
        return true;
    return false;
}

static bool config_entries_ddbdev_is_tail(const BtDeviceRecord* param_rec)
{
    list_node_t *node;
    nvrec_entry_t *entry_temp = NULL;

    nvrec_section_t *sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if (!sec)
        assert(0);
    for (node = list_begin_ext(sec->entries); node != list_end_ext(sec->entries); node = list_next_ext(node))
    {
        entry_temp = list_node_ext(node);
    }
    if(0 == memcmp(entry_temp->key,param_rec->bdAddr.addr,BD_ADDR_SIZE))
        return true;
    return false;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
static void config_specific_entry_value_delete(const BtDeviceRecord* param_rec)
{
    if(config_entries_ddbdev_is_head(param_rec))
        config_entries_ddbdev_delete_head();
    else if(config_entries_ddbdev_is_tail(param_rec))
        config_entries_ddbdev_delete_tail();
    else
        config_entries_ddbdev_delete(param_rec);
    nv_record_config.is_update = true;
}


/**********************************************
gyt add:list head is the odest,list tail is the latest.
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
**********************************************/
static BtStatus nv_record_ddbrec_add(const BtDeviceRecord* param_rec)
{
    btdevice_volume device_vol;
    btdevice_profile device_plf;
    char key[BD_ADDR_SIZE+1] = {0,};
    nvrec_btdevicerecord *nvrec_pool_record = NULL;
    bool ddbrec_exist = false;
    int getint;

    if(NULL == param_rec)
        return BT_STATUS_FAILED;
    memcpy(key,param_rec->bdAddr.addr,BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(getint){
        ddbrec_exist = true;
        nvrec_pool_record = (nvrec_btdevicerecord *)getint;
        device_vol.a2dp_vol = nvrec_pool_record->device_vol.a2dp_vol;
        device_vol.hfp_vol = nvrec_pool_record->device_vol.hfp_vol;
        device_plf.hfp_act = nvrec_pool_record->device_plf.hfp_act;
        device_plf.hsp_act = nvrec_pool_record->device_plf.hsp_act;
        device_plf.a2dp_act = nvrec_pool_record->device_plf.a2dp_act;
        config_specific_entry_value_delete(param_rec);
    }
    if(DDB_RECORD_NUM == config_entries_get_ddbrec_length(section_name_ddbrec))
    {
        nvrec_trace("%s,ddbrec list is full,delete the oldest and add param_rec to list.",nvrecord_tag);
        config_entries_ddbdev_delete_head();
    }
    nvrec_pool_record = (nvrec_btdevicerecord *)pool_malloc(sizeof(nvrec_btdevicerecord));
    if(NULL == nvrec_pool_record)
    {
        nvrec_trace("%s,pool_malloc failure.",nvrecord_tag);
        return BT_STATUS_FAILED;
    }
    nvrec_trace("%s,pool_malloc addr = 0x%x\n",nvrecord_tag,(unsigned int)nvrec_pool_record);
    memcpy(nvrec_pool_record,param_rec,sizeof(BtDeviceRecord));
    memcpy(key,param_rec->bdAddr.addr,BD_ADDR_SIZE);
    if (ddbrec_exist){
        nvrec_pool_record->device_vol.a2dp_vol = device_vol.a2dp_vol;
        nvrec_pool_record->device_vol.hfp_vol = device_vol.hfp_vol;
        nvrec_pool_record->device_plf.hfp_act = device_plf.hfp_act;
        nvrec_pool_record->device_plf.hsp_act = device_plf.hsp_act;
        nvrec_pool_record->device_plf.a2dp_act = device_plf.a2dp_act;
    }else{
        nvrec_pool_record->device_vol.a2dp_vol = NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT;
        nvrec_pool_record->device_vol.hfp_vol = NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT;
        nvrec_pool_record->device_plf.hfp_act = false;
        nvrec_pool_record->device_plf.hsp_act = false;
        nvrec_pool_record->device_plf.a2dp_act = false;
    }
    nvrec_config_set_int(nv_record_config.config,section_name_ddbrec,(char *)key,(int)nvrec_pool_record);
#ifdef nv_record_debug
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    nvrec_trace("%s,get pool_malloc addr = 0x%x\n",nvrecord_tag,(unsigned int)getint);
#endif
    nv_record_config.is_update = true;
    return BT_STATUS_SUCCESS;
}

//Modified by ATX : Haorong.Wu_20190224 add atx factory mode for product line testing
#ifdef _ATX_FACTORY_MODE_DETECT_
extern int get_atx_factory_mode_flag(void);
#endif
/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
BtStatus nv_record_add(SECTIONS_ADP_ENUM type,void *record)
{
    BtStatus retstatus = BT_STATUS_FAILED;

#ifdef _ATX_FACTORY_MODE_DETECT_
    if(get_atx_factory_mode_flag()){
        TRACE("[%s] skip record add while factory mode on", __func__);
        return BT_STATUS_SUCCESS;
    }
#endif

    if ((NULL == record) || (section_none == type))
        return BT_STATUS_FAILED;
    switch(type)
    {
        case section_usrdata_ddbrecord:
            retstatus = nv_record_ddbrec_add(record);
            break;

        case section_usrdata_other:
            break;
        default:
            break;
    }

    return retstatus;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
BtStatus nv_record_ddbrec_find(const BT_BD_ADDR *bd_ddr,BtDeviceRecord *record)
{
    unsigned int getint = 0;
    char key[BD_ADDR_SIZE+1] = {0,};
    BtDeviceRecord *getrecaddr = NULL;

    if((NULL == nv_record_config.config) || (NULL == bd_ddr) || (NULL == record))
        return BT_STATUS_FAILED;
    memcpy(key,bd_ddr->addr,BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return BT_STATUS_FAILED;
    getrecaddr = (BtDeviceRecord *)getint;
    memcpy(record,(void *)getrecaddr,sizeof(BtDeviceRecord));
    return BT_STATUS_SUCCESS;
}

int nv_record_btdevicerecord_find(const BT_BD_ADDR *bd_ddr, nvrec_btdevicerecord **record)
{
    unsigned int getint = 0;
    char key[BD_ADDR_SIZE+1] = {0,};

    if((NULL == nv_record_config.config) || (NULL == bd_ddr) || (NULL == record))
        return -1;
    memcpy(key,bd_ddr->addr,BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return -1;
    *record = (nvrec_btdevicerecord *)getint;
    return 0;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
BtStatus nv_record_ddbrec_delete(const BT_BD_ADDR *bdaddr)
{
    unsigned int getint = 0;
    BtDeviceRecord *getrecaddr = NULL;
    char key[BD_ADDR_SIZE+1] = {0,};

    memcpy(key,bdaddr->addr,BD_ADDR_SIZE);
    getint = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)key,0);
    if(0 == getint)
        return BT_STATUS_FAILED;
    //found ddb record,del it and return succeed.
    getrecaddr = (BtDeviceRecord *)getint;
    config_specific_entry_value_delete((const BtDeviceRecord *)getrecaddr);
    return BT_STATUS_SUCCESS;
}

/*
this function should be surrounded by OS_LockStack and OS_UnlockStack when call.
*/
BtStatus nv_record_enum_dev_records(unsigned short index,BtDeviceRecord* record)
{
    nvrec_section_t *sec = NULL;
    list_node_t *node;
    unsigned short pos = 0;
    nvrec_entry_t *entry_temp = NULL;
    BtDeviceRecord *recaddr = NULL;
    unsigned int addr = 0;

    if((index >= DDB_RECORD_NUM) || (NULL == record) || (NULL == nv_record_config.config))
        return BT_STATUS_FAILED;
    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return BT_STATUS_INVALID_PARM;
    if(NULL == sec->entries)
        return BT_STATUS_INVALID_PARM;
    if(0 == sec->entries->length)
        return BT_STATUS_FAILED;
    if(index >= sec->entries->length)
        return BT_STATUS_FAILED;
    node = list_begin_ext(sec->entries);

    while(pos < index)
    {
        node = list_next_ext(node);
        pos++;
    }
    entry_temp = list_node_ext(node);
    addr = nvrec_config_get_int(nv_record_config.config,section_name_ddbrec,(char *)entry_temp->key,0);
    if(0 == addr)
        return BT_STATUS_FAILED;
    recaddr = (BtDeviceRecord *)addr;
    memcpy(record,recaddr,sizeof(BtDeviceRecord));
    nv_record_print_dev_record(record);
    return BT_STATUS_SUCCESS;
}

/*
return:
    -1:     enum dev failure.
    0:      without paired dev.
    1:      only 1 paired dev,store@record1.
    2:      get 2 paired dev.notice:record1 is the latest record.
*/
int nv_record_enum_latest_two_paired_dev(BtDeviceRecord* record1,BtDeviceRecord* record2)
{
    BtStatus getret1 = BT_STATUS_FAILED;
    BtStatus getret2 = BT_STATUS_FAILED;
    nvrec_section_t *sec = NULL;
    size_t entries_len = 0;
    list_t *entry = NULL;

    if((NULL == record1) || (NULL == record2))
        return -1;
    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return 0;
    entry = sec->entries;
    if(NULL == entry)
        return 0;
    entries_len = entry->length;
    if(0 == entries_len)
        return 0;
    else if(entries_len < 0)
        return -1;
    if(1 == entries_len)
    {
        getret1 = nv_record_enum_dev_records(0,record1);
        if(BT_STATUS_SUCCESS == getret1)
            return 1;
        return -1;
    }
    getret1 = nv_record_enum_dev_records(entries_len-1,record1);
    getret2 = nv_record_enum_dev_records(entries_len-2,record2);
    if((BT_STATUS_SUCCESS != getret1) || (BT_STATUS_SUCCESS != getret2))
    {
        memset(record1,0x0,sizeof(BtDeviceRecord));
        memset(record2,0x0,sizeof(BtDeviceRecord));
        return -1;
    }
    return 2;
}

int nv_record_touch_cause_flush(void)
{
	nv_record_config.is_update = true;
	return 0;
}

void nv_record_all_ddbrec_print(void)
{
    list_t *entry = NULL;
    int tmp_i = 0;
    nvrec_section_t *sec = NULL;
    size_t entries_len = 0;

    sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
    if(NULL == sec)
        return;
    entry = sec->entries;
    if(NULL == entry)
        return;
    entries_len = entry->length;
    if(0 == entries_len)
    {
        nvrec_trace("%s,without btdevicerec.",nvrecord_tag,__LINE__);
        return;
    }
    for(tmp_i=0;tmp_i<entries_len;tmp_i++)
    {
        BtDeviceRecord record;
        BtStatus ret_status;
        ret_status = nv_record_enum_dev_records(tmp_i,&record);
        if(BT_STATUS_SUCCESS== ret_status)
            nv_record_print_dev_record(&record);
    }
}

void nv_record_sector_clear(void)
{
    uint32_t lock;

    lock = int_lock();
    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start),0x1000);
#ifdef __DUAL_USER_SECTION_BAK__
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start_backup),0x1000);
#endif

    pmu_flash_read_config();
    nvrec_init = false;
	nvrec_mempool_init = false;
    int_unlock(lock);
}

#ifdef __DUAL_USER_SECTION_BAK__
void nv_record_flash_backup(void)
{
    uint32_t lock;
    TRACE("%s",__func__);
    lock = int_lock();
    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start_backup),sizeof(usrdata_ddblist_pool));
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start_backup),(uint8_t *)usrdata_ddblist_pool,sizeof(usrdata_ddblist_pool));
    pmu_flash_read_config();
    int_unlock(lock);
}
#endif

void nv_record_flash_flush(void)
{
    uint32_t crc;
    uint32_t crc_buildinfo;
    uint32_t lock;
    buffer_alloc_ctx *heapctx = NULL;

    if(false == nv_record_config.is_update)
    {
        //nvrec_trace("%s,without update.\n",nvrecord_tag);
        return;
    }
    if(NULL == nv_record_config.config)
    {
        nvrec_trace("%s,nv_record_config.config is null.\n",nvrecord_tag);
        return;
    }
    nvrec_trace("%s,nv_record_flash_flush,nv_record_config.config=0x%x\n",nvrecord_tag,nv_record_config.config);

    lock = int_lock();
    heapctx = memory_buffer_heap_getaddr();
    memcpy((void *)(&usrdata_ddblist_pool[pos_heap_contents]),heapctx,sizeof(buffer_alloc_ctx));
    crc = crc32(0,(uint8_t *)(&usrdata_ddblist_pool[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    crc_buildinfo = crc32(0,(uint8_t *)(sys_build_info),(strlen(sys_build_info)));

    nvrec_trace("%s,crc=%x.\n",nvrecord_tag,crc);
    nvrec_trace("%s,crc_buildinfo=%x.size %d\n",nvrecord_tag,crc_buildinfo,strlen(sys_build_info));
    usrdata_ddblist_pool[pos_version_and_magic] = ((nvrecord_struct_version<<16)|nvrecord_magic);
    usrdata_ddblist_pool[pos_crc] = crc;
#ifdef __DUAL_USER_SECTION_BAK__
    usrdata_ddblist_pool[pos_crc_buildinfo] = crc_buildinfo;
#else
    usrdata_ddblist_pool[pos_reserv1] = 0;
#endif
    usrdata_ddblist_pool[pos_reserv2] = 0;
    usrdata_ddblist_pool[pos_config_addr] = (uint32_t)nv_record_config.config;

    pmu_flash_write_config();
    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start),sizeof(usrdata_ddblist_pool));
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__userdata_start),(uint8_t *)usrdata_ddblist_pool,sizeof(usrdata_ddblist_pool));
    pmu_flash_read_config();
    int_unlock(lock);

    nv_record_config.is_update = false;

#ifdef nv_record_debug
	uint32_t recrc;
    lock = int_lock();
    recrc = crc32(0,(uint8_t *)(&__userdata_start[pos_heap_contents]),(sizeof(usrdata_ddblist_pool)-(pos_heap_contents*sizeof(uint32_t))));
    int_unlock(lock);
	ASSERT(crc == recrc,"%s user data crc check,recrc=%08x crc=%08x\n",__func__,recrc,crc);
#endif
}

/*
    dev_version_and_magic,      //0
    dev_crc,                    // 1
    dev_reserv1,                // 2
    dev_reserv2,                //3// 3
    dev_name,                   //[4~66]
    dev_bt_addr,                //[67~68]
    dev_ble_addr,               //[69~70]
    dev_xtal_fcap              //hal_trace_crashdump_get_empty_section
*/
bool nvrec_dev_data_open(void)
{
    uint32_t dev_zone_crc,dev_zone_flsh_crc;
    uint32_t vermagic;

    vermagic = __factory_start[dev_version_and_magic];
    nvrec_trace("%s,vermagic=0x%x",nvdev_tag,vermagic);
    if(((nvrec_dev_version<<16)|nvrec_dev_magic) != vermagic)
    {
        dev_sector_valid = false;
        TRACE("%s,dev sector invalid.",nvdev_tag);
        return dev_sector_valid;
    }
    dev_zone_flsh_crc = __factory_start[dev_crc];
    dev_zone_crc = crc32(0,(uint8_t *)(&__factory_start[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
    nvrec_trace("%s,vermagic=0x%x,dev_zone_flsh_crc=0x%x",nvdev_tag,vermagic,dev_zone_flsh_crc);
    if (dev_zone_flsh_crc == dev_zone_crc)
    {
        char *devicename = NULL;
        char *device_bt_addr = NULL;

        devicename = (char *)&__factory_start[dev_name];
        device_bt_addr = (char *)&__factory_start[dev_bt_addr];
        TRACE("%s,dev sector valid,local name = %s",nvdev_tag,devicename);
        DUMP8("0x%x ",device_bt_addr,6);
        dev_sector_valid = true;
    }
    return dev_sector_valid;
}

bool nvrec_dev_localname_addr_init(dev_addr_name *dev)
{
    uint32_t *p_devdata_cache = __factory_start;
    if(true == dev_sector_valid)
    {
        nvrec_trace("%s,nv dev data valid",nvdev_tag);
        memcpy((void *) dev->btd_addr,(void *)&p_devdata_cache[dev_bt_addr],BD_ADDR_SIZE);
        memcpy((void *) dev->ble_addr,(void *)&p_devdata_cache[dev_ble_addr],BD_ADDR_SIZE);
        dev->localname = (char *)&p_devdata_cache[dev_name];
        nvrec_trace("%s,localname=%s,namelen=%d",nvdev_tag,dev->localname, strlen(dev->localname));
        return true;
    }
    else
        return false;

}

static void nvrec_dev_data_fill_xtal_fcap(uint32_t *mem_pool,uint32_t val)
{
    uint8_t *btaddr = NULL;
    uint8_t *bleaddr = NULL;

    assert(0 != mem_pool);
    if(dev_sector_valid)
    {
        memcpy((void *)mem_pool,(void *)__factory_start,0x1000);
        mem_pool[dev_xtal_fcap] = val;
        mem_pool[dev_crc] = crc32(0,(uint8_t *)(&mem_pool[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
    }
    else
    {
        const char *localname = randaddrgen_get_btd_localname();
        unsigned int namelen = strlen(localname);

        btaddr = randaddrgen_get_bt_addr();
        bleaddr = randaddrgen_get_ble_addr();
        mem_pool[dev_version_and_magic] = ((nvrec_dev_version<<16)|nvrec_dev_magic);
        mem_pool[dev_reserv1] = 0;
        mem_pool[dev_reserv2] = 0;
        memcpy((void *)&mem_pool[dev_name],(void *)localname,(size_t)namelen);
        nvrec_dev_rand_btaddr_gen(btaddr);
        nvrec_dev_rand_btaddr_gen(bleaddr);
        memcpy((void *)&mem_pool[dev_bt_addr],(void *)btaddr,BD_ADDR_SIZE);
        memcpy((void *)&mem_pool[dev_ble_addr],(void *)bleaddr,BD_ADDR_SIZE);
        memset((void *)&mem_pool[dev_dongle_addr],0x0,BD_ADDR_SIZE);
        mem_pool[dev_xtal_fcap] = val;
        mem_pool[dev_crc] = crc32(0,(uint8_t *)(&mem_pool[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
        nvrec_trace("%s,%s,mem_pool[dev_crc]=%x.\n",nvdev_tag,__FUNCTION__,mem_pool[dev_crc]);
    }

}

void nvrec_dev_flash_flush(unsigned char *mempool)
{
    uint32_t lock;
#ifdef nv_record_debug
    uint32_t devdata[dev_data_len] = {0,};
    uint32_t recrc;
#endif

    if(NULL == mempool)
        return;
    lock = int_lock();
    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__factory_start),0x1000);
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__factory_start),(uint8_t *)mempool,0x1000);//dev_data_len*sizeof(uint32_t));

    pmu_flash_read_config();
    int_unlock(lock);

#ifdef nv_record_debug
    memset(devdata,0x0,dev_data_len*sizeof(uint32_t));
    memcpy(devdata,__factory_start,dev_data_len*sizeof(uint32_t));
    recrc = crc32(0,(uint8_t *)(&devdata[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
    nvrec_trace("%s,%d,devdata[dev_crc]=%x.recrc=%x\n",nvdev_tag,__LINE__,devdata[dev_crc],recrc);
    if(devdata[dev_crc] != recrc)
        assert(0);
#endif
}

void nvrec_dev_rand_btaddr_gen(uint8_t* bdaddr)
{
    unsigned int seed;
    int i;

    OS_DUMP8("%x ",bdaddr,6);
    seed = hal_sys_timer_get();
    for(i=0;i<BD_ADDR_SIZE;i++)
    {
        unsigned int randval;
        srand(seed);
        randval = rand();
        bdaddr[i] = (randval&0xff);
        seed += rand();
    }
    OS_DUMP8("%x ",bdaddr,6);
}

void nvrec_dev_set_xtal_fcap(unsigned int xtal_fcap)
{
    uint8_t *mempool = NULL;
    uint32_t lock;
#ifdef nv_record_debug
    uint32_t devdata[dev_data_len] = {0,};
    uint32_t recrc;
#endif

    app_audio_mempool_init();
    app_audio_mempool_get_buff(&mempool, 0x1000);
    nvrec_dev_data_fill_xtal_fcap((uint32_t *)mempool,(uint32_t)xtal_fcap);
    lock = int_lock();
    pmu_flash_write_config();

    hal_norflash_erase(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__factory_start),0x1000);
    hal_norflash_write(HAL_NORFLASH_ID_0,(uint32_t)NVRECORD_CACHE_2_UNCACHE(__factory_start),(uint8_t *)mempool,0x1000);//dev_data_len*sizeof(uint32_t));

    pmu_flash_read_config();
    int_unlock(lock);
#ifdef nv_record_debug
    memset(devdata,0x0,dev_data_len*sizeof(uint32_t));
    memcpy(devdata,__factory_start,dev_data_len*sizeof(uint32_t));
    nvrec_trace("LN=%d,xtal fcap = %d",__LINE__,devdata[dev_xtal_fcap]);
    recrc = crc32(0,(uint8_t *)(&devdata[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
    nvrec_trace("%s,%d,devdata[dev_crc]=%x.recrc=%x\n",nvdev_tag,__LINE__,devdata[dev_crc],recrc);
    if(devdata[dev_crc] != recrc)
        assert(0);
#endif
}

int nvrec_dev_get_xtal_fcap(unsigned int *xtal_fcap)
{
    unsigned int xtal_fcap_addr = (unsigned int)(__factory_start + dev_xtal_fcap);
    unsigned int tmpval[1] = {0,};

    if(false == dev_sector_valid)
        return -1;
    if (NULL == xtal_fcap)
        return -1;
    memcpy((void *)tmpval,(void *)xtal_fcap_addr,sizeof(unsigned int));
    *xtal_fcap = tmpval[0];
    return 0;
}

int nvrec_dev_get_dongleaddr(BT_BD_ADDR *dongleaddr)
{
    unsigned int dongle_addr_pos = (unsigned int)(__factory_start + dev_dongle_addr);

    if(false == dev_sector_valid)
        return -1;
    if(NULL == dongleaddr)
        return -1;
    memcpy((void *)dongleaddr,(void *)dongle_addr_pos,BD_ADDR_SIZE);
    return 0;
}




//Modified by ATX : Haorong.Wu_20180530

#ifdef __SAVE_FW_VERSION_IN_NV_FLASH__
bool nvrec_fw_version_update(nv_fw_ver *dev_fw_ver)
{
    uint8_t *mempool = NULL;
    uint32_t *devdata = NULL;
    if(true != dev_sector_valid)
    {
        nvrec_trace("%s,nv dev data invalid",nvdev_tag);
        return false; 
    }
    
    if(NV_FW_VERSION_NULL == dev_fw_ver->fw_version)
    {
        nvrec_trace("%s,nv dev fw_version NULL",nvdev_tag);
        return false; 
    }
    
#ifdef nv_record_debug
    nvrec_trace("%s,nv dev data valid",nvdev_tag);
    DUMP8("0x%x ",__factory_start,dev_data_len);
#endif

    app_audio_mempool_init();
    app_audio_mempool_get_buff(&mempool, 0x1000);
    memset(mempool,0x0,0x1000);
    memcpy(mempool,(void *)__factory_start,0x1000);

    devdata =(uint32_t *) mempool;
    memcpy(&devdata[dev_fw_version], dev_fw_ver, sizeof(nv_fw_ver));
    devdata[dev_crc] = crc32(0,(uint8_t *)(&devdata[dev_reserv1]),(dev_data_len-dev_reserv1)*sizeof(uint32_t));
    nvrec_dev_flash_flush(mempool);

    nvrec_trace("%s, fw_version %x, tws_channel %d\n",nvdev_tag, dev_fw_ver->fw_version,dev_fw_ver->tws_channel);
    return true;
}

void nvrec_fw_version_init(void)
{
    nv_fw_ver dev_fw_ver;
    memcpy(&dev_fw_ver, &__factory_start[dev_fw_version], sizeof(nv_fw_ver));
    nvrec_trace("<%s>", __func__);
    
    if(true != dev_sector_valid){
        nvrec_trace("%s,nv dev data invalid\n",nvdev_tag);
        return ; 
    }

    if(NV_FW_VERSION == dev_fw_ver.fw_version){
        nvrec_trace("%s,version already exist: %x\n",nvdev_tag,dev_fw_ver.fw_version);
        return; 
    }

    dev_fw_ver.fw_version = NV_FW_VERSION;
    
#ifdef __TWS_CHANNEL_LEFT__
    dev_fw_ver.tws_channel= dev_tws_channel_left;
#elif __TWS_CHANNEL_RIGHT__
    dev_fw_ver.tws_channel= dev_tws_channel_right;
#else
	dev_fw_ver.tws_channel= dev_tws_channel_none;
#endif

    nvrec_fw_version_update(&dev_fw_ver);
}
#endif
//Modified by ATX : Parke.Wei_20180418
size_t get_paired_device_nums(void)
{
    size_t entries_len = 0;

    if(NULL != nv_record_config.config)
    {
        nvrec_section_t *sec = NULL;
        sec = nvrec_section_find(nv_record_config.config,section_name_ddbrec);
        if (NULL != sec)
            entries_len = sec->entries->length;
    }

	if(entries_len<0)
		entries_len=0;
	
    return entries_len;      

}




