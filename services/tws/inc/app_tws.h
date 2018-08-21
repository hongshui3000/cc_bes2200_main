#ifndef __APP_TWS_H__
#define __APP_TWS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "a2dp.h"
#include "avrcp.h"
#include "avdtp.h"
#include "cqueue.h"
#include "app_audio.h"
//#ifdef A2DP_AAC_DIRECT_TRANSFER 
#include "spp.h"
//#endif

#include "app_status_ind.h"//Modified by ATX : Parke.Wei_20180316

typedef enum {
   TWSINIT,
   TWSMASTER,
   TWSSLAVE,
   TWSON,
   TWSHFD, 
   TWSINVD
}DeviceMode;

typedef enum {
    TWS_PLAYER_START = 0,
    TWS_PLAYER_SETUP,
    TWS_PLAYER_STOP,
    TWS_PLAYER_PAUSE,
    TWS_PLAYER_RESTART,
    TWS_PLAYER_NULL,
}TWS_PLAYER_STATUS_T;

enum {
    APP_TWS_CLOSE_BLE_ADV,
    APP_TWS_OPEN_BLE_ADV,
};


enum {
    APP_TWS_START_STREAM,
    APP_TWS_SUSPEND_STREAM,
    APP_TWS_CLOSE_STREAM,
    APP_TWS_OPEN_STREAM,
    APP_TWS_IDLE_OP
};

enum {
    APP_TWS_CLOSE_BLE_SCAN,
    APP_TWS_OPEN_BLE_SCAN,
};

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
typedef enum {
    TWS_STREAM_CLOSE = 1 << 0,
    TWS_STREAM_OPEN = 1 << 1,
    TWS_STREAM_START = 1 << 2,
    TWS_STREAM_SUSPEND = 1 << 3,
}TwsStreamState;
#endif

typedef struct {
   uint32_t bt_time_l; //logical count of bt timer 
   uint32_t bt_time_first_l; //logical count of bt timer   
   uint32_t seq_wrap;
}trigger_t;

typedef struct  {
    osSemaphoreId _osSemaphoreId;
    osSemaphoreDef_t _osSemaphoreDef;
#ifdef CMSIS_OS_RTX
    uint32_t _semaphore_data[2];
#endif

} tws_lock_t;

typedef struct	{
    osMutexId _osMutexId;
    osMutexDef_t _osMutexDef;
#ifdef CMSIS_OS_RTX
    int32_t _mutex_data[3];
#endif

} tws_mutex_t;

typedef struct {
    osEvent oe;
    int code;
    int len;
    uint8_t * data;
}scan_msg_t;


typedef struct {
   uint32_t status;
   tws_lock_t _lock;
   uint32_t last_trigger_time;
   APP_AUDIO_CALLBACK_T callback;
   void (*wait)(void *player);
   void (*release)(void *player);
   void (*mutex_lock)(void);
   void (*mutex_unlock)(void);
}player_t;

typedef struct {
    CQueue  queue;
    tws_lock_t buff_lock;
    uint32_t int_lock;
    tws_lock_t wait;
    tws_lock_t wait_data_l;
    uint32_t sample_rate; 
    uint32_t channel;
#if defined(__TWS_CLK_SYNC__)
    uint16_t enqueued_frame_num;
    uint16_t processed_frame_num;
    uint16_t locked_frame_num;//record remote locked clk correspond frame num
    uint32_t locked_native_clk;//dma locked local native clk
    uint32_t remote_locked_native_clk;//remote dma locked native clk
    uint16_t locked_bit_cnt;//dma locked local bitcnt
    uint16_t remote_locked_bit_cnt;//remote dma locked bitcnt
#endif
    void (*wait_space)(void *tws_buff);
    void (*put_space)(void *tws_buff);
    void (*wait_data)(void *tws_buff);
    void (*put_data)(void *tws_buff);
    void (*lock)(void *tws_buff);
    void (*unlock)(void *tws_buff);
    int  (*write)(void *tws_buff, uint8_t *buff, uint16_t len);
    int  (*read)(void *tws_buff, uint8_t *buff, uint16_t len);
    int  (*len)(void *tws_buff);
    int  (*available)(void *tws_buff);    
    int32_t (*clean)(void *tws_buff);
    int32_t (*playtime)(void *tws_buff);
}tws_buffer_t;  //pcm buffer for audout 

typedef struct{
    uint32_t channel;	
    uint32_t sample_rate;	
    uint32_t pcm_counter;	
    uint8_t * pcm_buffer;
    uint32_t size;
    uint32_t pcm_fsz;
    uint32_t pause_point;  // pcm frames from begin
    enum AUD_STREAM_ID_T stream_id;
    enum AUD_STREAM_T stream;
    int32_t (*playtime)(void *audout);
    void (*pcm_useout)(void);
}audout_t;


typedef struct {
    A2dpStream *stream;
    volatile bool connected; //link with peer
    volatile bool counting;
    volatile bool locked; 
    volatile bool blocked;    
#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    uint16_t stream_state;
    uint32_t stream_need_reconfig;
#endif    
    tws_lock_t   wait;
    tws_lock_t   sent_lock;
    tws_lock_t   stream_lock;
    volatile uint32_t inpackets;   //packets counter
    volatile uint32_t outpackets;   //packets counter
    uint32_t trans_speed;
    uint32_t trigger_time;
    uint32_t blocktime;
    
    void (*wait_data)(void *channel);
    void (*put_data)(void *channel);
    int32_t (*wait_sent)(void *channel,uint32_t timeout);
    void (*notify_send)(void *channel);	
    void (*lock_stream)(void *channel);
    void (*unlock_stream)(void *channel);	
}bt_channel_t;

typedef struct {
    trigger_t  dac_timer_trigger;
    DeviceMode tws_mode;  
    volatile bool pcm_playing; 
    osThreadId decoder_tid;
    volatile bool  decoder_running;
    volatile bool  decoder_running_wait;
    tws_mutex_t decoder_mutex;
    tws_mutex_t decoder_buffer_mutex;
    void (*lock_decoder_thread)(void *tws);
    void (*unlock_decoder_thread)(void *tws);
    osThreadId trans_tid;
    volatile bool  trans_running;
    tws_mutex_t trans_mutex;
    void (*lock_trans_thread)(void *tws);
    void (*unlock_trans_thread)(void *tws); 	
    tws_buffer_t pcmbuff;
    tws_buffer_t sbcbuff;
    tws_buffer_t a2dpbuff;
#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
    tws_buffer_t encode_queue;
#endif
    audout_t audout;
    BtPacket packet;
    bt_channel_t tws_sink;
    bt_channel_t tws_source;
    bt_channel_t mobile_sink;
//#ifdef A2DP_AAC_ON    
    AvdtpCodecType mobile_codectype;
//#ifdef A2DP_AAC_DIRECT_TRANSFER    
    SppDev* tws_spp_dev;
    uint8_t tws_set_codectype_cmp;
    uint8_t tws_spp_wait_event;
    uint8_t tws_spp_connect;
    osThreadId tws_spp_tid;
    osThreadId tws_ctrl_tid;
    osMailQId tws_mailbox;
    uint16_t * tws_aac_pcm;
//#endif
//#endif
    AvrcpChannel *rcp; //for play,stop,volume up/down
    BT_BD_ADDR peer_addr;
    BT_BD_ADDR connecting_addr;
    BT_BD_ADDR mobile_addr;
    tws_lock_t   query_lock;
    tws_lock_t   decode_lock;
    tws_lock_t   mode_lock;
    uint32_t     prop_lock;
    int32_t      btclkoffset;
    volatile bool has_tws_peer;
    volatile bool isplaying;
    volatile bool decoder_stop;
    volatile bool paused;
    volatile bool tran_blocked;
    volatile bool pausepending;   
    volatile bool wait_pcm_signal;
    volatile bool decode_discard;
    player_t player;
    void (*lock_decoder)(void *tws); 
    void (*unlock_decoder)(void *tws); 
    void (*lock_mode)(void *tws);
    void (*unlock_mode)(void *tws);
    bool  media_suspend;
    bool create_conn_pending;
    bool suspend_media_pending;
    bool start_media_waiting;
    bool start_media_pending;

    uint8_t current_operation;
    uint8_t next_operation;

    
    void (*wait)(void *tws);
    void (*notify)(void *tws);
    void (*lock_prop)(void *tws); 
    void (*unlock_prop)(void *tws); 
    uint8_t decode_channel;
    uint8_t tran_channel;
    uint16_t tws_conhdl;
    uint16_t mobile_conhdl;

    uint8_t stream_reset;
    uint8_t stream_idle;
    uint32_t pause_packets;   //packets counter
    int8_t hfp_volume;
    int8_t tws_player_restart;
    bool slave_player_restart_pending;

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
    tws_lock_t wait_stream_reconfig;
    tws_lock_t local_player_wait_restart;
    tws_lock_t wait_stream_started;
    uint8_t local_player_wait_reconfig;
    uint8_t local_player_need_pause;
    uint8_t local_player_sig_flag;
    uint8_t local_player_status;
    uint8_t local_player_stop;
    uint8_t local_player_tran_2_slave_flag;
#endif   
}tws_dev_t;

typedef struct _app_tws_slave_SbcStreamInfo {
    U8   bitPool;
    SbcAllocMethod allocMethod;
}tws_slave_SbcStreamInfo_t;

typedef BtStatus (*BLE_ADV_SCAN_FUNC)(uint8_t);

#ifdef __TWS_RECONNECT_USE_BLE__
typedef struct {
    BLE_ADV_SCAN_FUNC adv_func;
    BLE_ADV_SCAN_FUNC adv_close_func;

    BLE_ADV_SCAN_FUNC scan_func;
    uint8_t bleReconnectPending;
#ifdef _TWS_MASTER_DROP_DATA_    
    uint8_t master_filter;
#endif
    uint8_t tws_start_stream_pending;
    osTimerId master_ble_scan_timerId;
    osTimerId slave_delay_role_switch_timerId;
    uint8_t isConnectedPhone;
} app_tws_ble_reconnect_t;
#endif

typedef struct  {
    uint8_t *buff;
    uint32_t size;
}tws_env_buff_t;

typedef struct  {
    tws_env_buff_t env_pcm_buff;
    tws_env_buff_t env_sbc_buff;
    tws_env_buff_t env_a2dp_buff;
    tws_env_buff_t env_encode_queue_buff;
    tws_env_buff_t env_sbsample_buff;
    
    uint32_t env_trigger_delay;    
    uint32_t env_trigger_a2dp_buff_thr;
    uint32_t env_trigger_aac_buff_thr;    
    uint32_t env_slave_trigger_thr;
}tws_env_cfg_t;


typedef struct trans_conf_{
uint32_t transesz;
uint32_t framesz;
uint32_t discardframes;
char *opus_temp_buff;
}trans_conf_t;


typedef struct sbcpack{
    A2dpSbcPacket sbcPacket;
    char *buffer;
    int free;
}sbcpack_t;

typedef struct sbcbank{
    sbcpack_t sbcpacks[1];
    int free;
}sbcbank_t;

#ifdef A2DP_AAC_DIRECT_TRANSFER
#define TWS_PLAYER_TRIGGER_DELAY   (250000)   //us
#else
#define TWS_PLAYER_TRIGGER_DELAY   (250000)   //us
#endif
#define TWS_PLAYER__SLAVE_TRIGGER_THR   (100)   //CLK


#ifdef __TWS_CALL_DUAL_CHANNEL__

#define NONE_ROLE       0

#define SNIFFER_ROLE    1
#define FORWARD_ROLE   2
#endif

#ifdef FPGA
/* pcm buffer size */
#define PCM_BUFFER_SZ_TWS        (48*2*1024)  //1s 
/* media buffer size */
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/6)

#if defined(MASTER_USE_OPUS) || defined(SLAVE_USE_OPUS)  || defined(ALL_USE_OPUS)
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*30)
#else
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*15)
#endif

#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC) 
/* pcm buffer size */
#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define PCM_BUFFER_SZ_TWS        (12*2*1024)  //1s
#else
#if defined(__SBC_FUNC_IN_ROM_VBEST2000__) && !defined(__SBC_FUNC_IN_ROM_VBEST2000_ONLYSBC__)
#define PCM_BUFFER_SZ_TWS        (15*2*1024)  //1s
#else
#ifdef __TWS_1CHANNEL_PCM__
#define PCM_BUFFER_SZ_TWS        (12*2*1024)  //1s
#else
#define PCM_BUFFER_SZ_TWS        (22*2*1024)  //1s
#endif
#endif
#endif

#else
/* pcm buffer size */
#define PCM_BUFFER_SZ_TWS        (12*2*1024)  //1s 
#endif

#ifdef __TWS_1CHANNEL_PCM__
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/3)
#else
/* media buffer size */
#define A2DP_BUFFER_SZ_TWS       (PCM_BUFFER_SZ_TWS/6)
#endif
#if defined(MASTER_USE_OPUS) || defined(SLAVE_USE_OPUS)  || defined(ALL_USE_OPUS)
#define TRANS_BUFFER_SZ_TWS      (A2DP_BUFFER_SZ_TWS*30)
#elif defined(SLAVE_USE_ENC_SINGLE_SBC) 
#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#else
#ifdef __TWS_1CHANNEL_PCM__
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS*2)
#else
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#endif
#endif
#else
#define TRANS_BUFFER_SZ_TWS      (PCM_BUFFER_SZ_TWS)
#endif

#endif



#if defined(MASTER_USE_OPUS) || defined(SLAVE_USE_OPUS) || defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(ALL_USE_OPUS)

#define MIX_2_CHANNELS_WHEN_USED_AS_SINGLE 1

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
/* transmit size each time */
//#define SBCTRANSMITSZ            (4608)
//#define SBCTRANSMITSZ            (3072)
#define SBCTRANSMITSZ            (5120)
/* transmit frame size */
#define SBCTRANFRAMESZ           (512)

#define TRANSDISCARDSFRAMES      (8*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  
//#define TRANSDISCARDSFRAMES      (15*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

#else

#define TRANSDISCARDSFRAMES      (10*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

/* transmit size each time */
#if defined(ALL_USE_OPUS)
#define SBCTRANSMITSZ            (OPUS_FRAME_LEN)
#else
#define SBCTRANSMITSZ            (960*2*2)
#endif

/* transmit frame size */
#define SBCTRANFRAMESZ           (SBCTRANSMITSZ)

#endif


/* pcm length to decode (for opus to encode) */
#if defined(SLAVE_USE_OPUS)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#elif defined(SLAVE_USE_ENC_SINGLE_SBC)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (1024)
#elif defined(MASTER_USE_OPUS)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#elif defined(ALL_USE_OPUS)
#define SLAVE_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#endif

#if defined(MASTER_USE_OPUS)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#elif defined(ALL_USE_OPUS)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (960*2*2)
#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
#define MASTER_DECODE_PCM_FRAME_LENGTH  (1024)
#else
#define MASTER_DECODE_PCM_FRAME_LENGTH  1024
#endif

#endif

// choose the bigger one
#define DECODE_PCM_FRAME_LENGTH  (MASTER_DECODE_PCM_FRAME_LENGTH>SLAVE_DECODE_PCM_FRAME_LENGTH?MASTER_DECODE_PCM_FRAME_LENGTH:SLAVE_DECODE_PCM_FRAME_LENGTH)

#else
#define DECODE_PCM_FRAME_LENGTH  1024
#define SBCTRANSMITSZ             1024
#define SBCTRANFRAMESZ           512

#define TRANSDISCARDSFRAMES      (20*SBCTRANSMITSZ/SBCTRANFRAMESZ) //80 fs,200ms  

#endif

//#ifdef A2DP_AAC_DIRECT_TRANSFER
typedef enum {
    TWS_CTRL_PLAYER_SETCODEC,
    TWS_CTRL_PLAYER_SPPCONNECT,
    TWS_CTRL_PLAYER_SET_HFP_VOLUME,
//Modified by ATX : parker.wei_20180306
    TWS_CTRL_PLAYER_HFP_VOLUME_CHANGE,
    TWS_CTRL_PLAYER_RESTART_PLAYER,
    TWS_CTRL_SLAVE_KEY_NOTIFY,
    TWS_CTRL_PLAYER_SET_A2DP_VOLUME,
//Modified by ATX : parker.wei_20180306
    TWS_CTRL_PLAYER_A2DP_CHANGE,
    TWS_CTRL_PLAYER_UPDATE_PEER_STATE,//Modified by ATX : Parke.Wei_20180316
    TWS_MASTER_CTRL_SLAVE_ENTER_OTA,//Modified by ATX : Parke.Wei_20180413
    TWS_CTRL_PLAYER_SEND_RSP,
	TWS_CTRL_RING_SYNC,
    TWS_CTRL_PLAYER_SET_SYSTEM_VOLUME,
//Modified by ATX : Haorong.Wu_20180507
    TWS_CTRL_VOICE_REPORT,
}TWS_CTRL_EVENT;

typedef struct {
    uint32_t evt;
    uint32_t arg;
} TWS_MSG_BLOCK;
//#endif



typedef struct{
    U8   bitPool;
    U8   numBlocks;
    U8   numSubBands;
    U8   frameNum;
    U32 frameCount;
    U32 frameLen;
    U32 sampleFreq;
}TWS_SBC_FRAME_INFO;


enum ld_afh_state
{
    AFH_ON = 0,
    AFH_OFF,
    AFH_WAIT_ON_ACK,
    AFH_WAIT_ON_HSSI,
    AFH_WAIT_OFF_HSSI
};



struct chnl_map
{
    ///5-byte channel map array
    uint8_t map[10];
};


struct ld_afh_bak
{
    enum ld_afh_state state;
    ///AFH Instant
    uint32_t        instant;
    ///AFH Mode
    uint8_t         mode;
    ///AFH channel map
    struct chnl_map map;
};


struct ld_sniffer_connect_env
{
    uint8_t connect_addr[6];
    uint16_t bitoff;
    uint32_t clk_offset;
    uint8_t  chan_map[10];
    uint32_t afh_instant;
    uint8_t afh_mode;
    uint8_t enc_mode;
    uint8_t enc_key[16];
    uint8_t role;
    
};


struct lc_sco_air_params
{
    uint8_t  tr_id;
    uint8_t  esco_hdl;
    uint8_t  esco_lt_addr;
    uint8_t  flags;
    uint8_t  d_esco;
    uint8_t  t_esco;
    uint8_t  w_esco;
    uint8_t  m2s_pkt_type;
    uint8_t  s2m_pkt_type;
    uint16_t m2s_pkt_len;
    uint16_t s2m_pkt_len;
    uint8_t  air_mode;
    uint8_t  negt_st;
};

struct lmp_sco_start_pdu
{
    uint8_t  link_id;
    uint8_t  tr_id;
    uint8_t sco_type;
    uint8_t t_sco;
    uint8_t wsco;
    uint8_t dsco;
    uint8_t timeflag;
    uint8_t packet_type;
    uint16_t packet_length;
    uint8_t airmode;
    uint8_t ltaddr;
};


struct ld_sniffer_env
{
    uint8_t sniffer_active;
    uint8_t sniffer_role;
    uint8_t monitored_dev_addr[6];
    uint8_t sniffer_dev_addr[6];
    uint8_t monitored_dev_state;
    uint8_t sniffer_dev_state;

    uint8_t monitored_linkid;
    uint8_t monitored_syncid;
    uint8_t monitored_sco_linkid;
    uint8_t sniffer_linkid;
    struct ld_sniffer_connect_env  peer_dev;
    struct lc_sco_air_params  sco_para;
    struct lmp_sco_start_pdu  sco_para_bak;
    uint32_t sniffer_acl_link_timeout;
    uint16_t sync_acl_link_super_to;
    uint16_t sync_sco_link_super_to;
    uint32_t last_sco_link_clk;
    
    uint8_t sniffer_acl_link_pending;
    uint8_t sniffer_sco_link_pending;
    
};


tws_env_cfg_t  *app_tws_get_cfg(void);

bool is_tws_active(void);
int tws_a2dp_callback(A2dpStream *Stream, const A2dpCallbackParms *Info);
int tws_store_stream(char *buf, int len);
int32_t init_tws(tws_env_cfg_t *cfg);
int32_t app_tws_active(void);
int32_t app_tws_deactive(void);
int tws_store_sbc_buffer(unsigned char *buf, unsigned int len,unsigned int sbc_frame_num);
void tws_trans_sbc(void);
void  notify_tws_btenv_ready(void);
void tws_set_channel_cfg(AvdtpCodec * mcfg);
void tws_player_stop_callback(uint32_t status, uint32_t param);
void tws_player_pause_callback(uint32_t status, uint32_t param);
void tws_player_start_callback(uint32_t status, uint32_t param);
void tws_player_restart_callback(uint32_t status, uint32_t param);
int tws_audio_sendrequest(uint8_t stream_type,  TWS_PLAYER_STATUS_T oprate, 
                            uint32_t param,     
                            APP_AUDIO_CALLBACK_T callback);

void app_tws_notipy(void);
uint8_t app_tws_get_mode(void);
uint8_t *app_tws_get_peer_bdaddr(void);
void app_tws_set_have_peer(bool is_have);
void app_tws_set_tws_mode(DeviceMode mode);

void app_tws_set_media_suspend(bool enable);

uint8_t is_find_tws_peer_device_onprocess(void);

void find_tws_peer_device_start(void);

void find_tws_peer_device_stop(void);

void set_a2dp_reconfig_sample_rate(A2dpStream *Stream);
AvrcpChannel *app_tws_get_avrcp_TGchannel(void);
A2dpStream *app_tws_get_tws_source_stream(void);
bool app_tws_get_source_stream_connect_status(void);
AvdtpCodec app_tws_get_set_config_codec_info(void);
bool app_tws_reconnect_slave_process(uint8_t a2dp_event, uint8_t avctp_event);
void app_tws_disconnect_slave(void);
int app_tws_nvrecord_rebuild(void);
void app_tws_creat_tws_link(void);
uint16_t app_tws_get_tws_conhdl(void);
void  app_tws_set_tws_conhdl(uint16_t conhdl);

int32_t restore_tws_channels(void);
int32_t restore_mobile_channels(void);
bool app_tws_media_frame_analysis(void *tws, A2dpStream *stream, const A2dpCallbackParms *Info);
void tws_player_stop(uint8_t stream_type);
int app_tws_reset_tws_data(void);
bool app_tws_analysis_media_data_from_mobile(void *tws, A2dpStream *stream, A2dpCallbackParms *Info);
void app_tws_start_master_player(A2dpStream *Stream);
A2dpStream* app_tws_get_sink_stream(void);

void a2dp_callback_tws_source(A2dpStream *Stream, const A2dpCallbackParms *Info);

#ifdef __TWS_RECONNECT_USE_BLE__
extern app_tws_ble_reconnect_t app_tws_ble_reconnect;
void app_tws_ble_reconnect_init(void);
#endif
void app_tws_master_reconnect_slave(BT_BD_ADDR *bdAddr);
int tws_player_set_codec( AvdtpCodecType codec_type);
int tws_player_set_hfp_vol( int8_t vol);
int tws_player_pause_player_req( int8_t req);
int tws_player_set_system_vol( int8_t hfp_vol,int8_t a2dp_vol);

//Modified by ATX : parker.wei_20180308
int tws_player_hfp_vol_change(int8_t vol);		

//Modified by ATX : Parke.Wei_20180316
int tws_player_update_peer_state(APP_STATUS_INDICATION_T state);
//Modified by ATX : Parke.Wei_20180316
#ifdef _MASTER_CTRL_SLAVE_ENTER_OTA__
int tws_mastet_ctrl_slave_enter_ota(uint8_t arg);
#endif
int tws_player_spp_connected( unsigned char para);
#ifdef A2DP_AAC_DIRECT_TRANSFER
uint8_t app_tws_get_codec_type(void);
void tws_set_mobile_sink_stream(A2dpStream *stream);
#endif

#ifdef TWS_RING_SYNC
int tws_player_ring_sync(U16 hf_event);
uint32_t tws_media_play_calc_ring_sync_trigger_time(void);
#endif

void app_tws_check_pcm_signal(void);
void app_tws_set_trigger_time(uint32_t trigger_time, bool play_only);


int audout_pcmbuff_thres_monitor(void);

#if  defined(TWS_RBCODEC_PLAYER) || defined (TWS_LINEIN_PLAYER)
void  rb_tws_start_master_player(uint8_t stream_type);
void rb_tws_restart();
uint32_t rb_push_pcm_in_tws_buffer(uint8_t *buff, uint32_t len);
uint32_t linein_push_pcm_in_tws_buffer(uint8_t *buff, uint32_t len);
void rb_set_sbc_encoder_freq_ch(int32_t freq, uint8_t ch);
void rb_start_sink_stream();
void rb_clean_stream_buffer();
void tws_stop_local_player();
void rb_sink_stream_stared();
void tws_local_player_set_tran_2_slave_flag(uint8_t onoff);
uint8_t tws_local_player_need_tran_2_slave();
uint32_t rb_tws_get_trigger_time(void);
uint16_t app_tws_get_master_stream_state(void);
#endif
#if defined(__TWS_CLK_SYNC__)
void tws_sync_lock_time_from_pcm_buffer(void * buf);
void tws_sync_remote_lock_time_from_a2dp_buffer(void * buf);
void tws_store_remote_lock_time(AvdtpMediaHeader header);
#endif



void tws_ctrl_thread_init(void);


#ifdef __cplusplus
}
#endif
#endif /**/
