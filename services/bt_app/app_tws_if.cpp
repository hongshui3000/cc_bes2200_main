#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hwtimer_list.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "analog.h"
#include "bt_drv.h"
#include "bt_drv_interface.h"
#include "hal_codec.h"
#include "app_audio.h"
#include "audioflinger.h"
#include "nvrecord.h"
#include "app_key.h"
#include "app_utils.h"
#include "app_overlay.h"
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "ddb.h"

extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "a2dp.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "btalloc.h"
}


#ifdef A2DP_AAC_ON
//#ifdef A2DP_AAC_DIRECT_TRANSFER
//#endif
#include "aacdec.h"
#endif

#ifdef __TWS__

#include "rtos.h"
#include "besbt.h"

#include "cqueue.h"
#include "btapp.h"
#include "apps.h"
#include "resources.h"
#include "app_bt_media_manager.h"
#include "tgt_hardware.h"
#include "app_tws.h"
#include "app_bt.h"
#include "hal_chipid.h"

extern  tws_dev_t  tws;
extern struct BT_DEVICE_T  app_bt_device;


#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
static uint8_t tws_encode_queue_buffer[SBCTRANSMITSZ+MASTER_DECODE_PCM_FRAME_LENGTH];
#endif

#define __TWS_SHARE_BUFF__

#ifdef __TWS_SHARE_BUFF__


//static uint8_t tws_pcm_buffer[PCM_BUFFER_SZ_TWS];
//static uint8_t tws_sbc_buffer[TRANS_BUFFER_SZ_TWS];
//static uint8_t tws_a2dp_sbc_buffer[A2DP_BUFFER_SZ_TWS];

uint8_t tws_big_buff[PCM_BUFFER_SZ_TWS+TRANS_BUFFER_SZ_TWS+A2DP_BUFFER_SZ_TWS];

unsigned char *sbc_transmit_buffer=NULL;
unsigned char *sbc_packet_buffer = NULL;
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
unsigned char *sbsamples_buf=NULL;

#endif

#else

static uint8_t tws_pcm_buffer[PCM_BUFFER_SZ_TWS];
static uint8_t tws_sbc_buffer[TRANS_BUFFER_SZ_TWS];
static uint8_t tws_a2dp_sbc_buffer[A2DP_BUFFER_SZ_TWS];
unsigned char sbc_transmit_buffer[SBCTRANSMITSZ];

char sbc_packet_buffer[SBCTRANSMITSZ];
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
unsigned char sbsamples_buf[SBCTRANSMITSZ];
#endif
#endif

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
tws_slave_SbcStreamInfo_t g_app_tws_slave_streaminfo = {
    TRANS_BTA_AV_CO_SBC_MAX_BITPOOL,
    SBC_ALLOC_METHOD_SNR
};
#endif

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
#if defined(ADJUST_BITPOOL_BETWEEN_TWS)
extern SbcEncoder sbc_encoder;
#endif

void app_tws_adjust_encode_bitpool(uint8_t updown)
{
#if defined(ADJUST_BITPOOL_BETWEEN_TWS)

    uint32_t   lock = int_lock();

    if(updown == 1) ///up
    {
        sbc_encoder.streamInfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
        g_app_tws_slave_streaminfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
    }
    else  //down
    {
        sbc_encoder.streamInfo.bitPool = 10;
        g_app_tws_slave_streaminfo.bitPool = 10;
    }
    int_unlock(lock);


    TRACE("app_tws_adjust_encode_bitpool BITPOOL=%d,%d",sbc_encoder.streamInfo.bitPool,g_app_tws_slave_streaminfo.bitPool);

#endif    
}

uint32_t music_cadun_count=0;
uint32_t first_cadun_tick=0;
uint32_t ticksOfTheLastStuttering;
static uint8_t stuttering_handling_in_progress = 0;
static uint8_t isLowBitPoolForStutteringExisting = false;

void app_tws_reset_cadun_count(void)
{

#if defined(ADJUST_BITPOOL_BETWEEN_TWS)

	TRACE("%s bitpool reset", __func__);
    stuttering_handling_in_progress = 0;
    isLowBitPoolForStutteringExisting = 0;
    music_cadun_count = 0;
    sbc_encoder.streamInfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
    g_app_tws_slave_streaminfo.bitPool = TRANS_BTA_AV_CO_SBC_MAX_BITPOOL;
#endif
} 


void app_tws_check_to_restore_from_stuttering(void)
{
#if defined(ADJUST_BITPOOL_BETWEEN_TWS)
    if (stuttering_handling_in_progress || isLowBitPoolForStutteringExisting)
    {
        uint32_t curr_tick = hal_sys_timer_get();
        uint32_t msSinceLastStuterring = 0;
        if (curr_tick > ticksOfTheLastStuttering)
        {
            msSinceLastStuterring = TICKS_TO_MS(curr_tick - ticksOfTheLastStuttering);
        }
        else
        {
            msSinceLastStuterring = 
                TICKS_TO_MS((hal_sys_timer_get_max() - ticksOfTheLastStuttering + 1) + curr_tick);
        }

        // if no stuttering since the latest stuttering for 10s, need to raise the bitpool
        if (msSinceLastStuterring > 10000)
        {
            stuttering_handling_in_progress = 0;

            if (g_app_tws_slave_streaminfo.bitPool == 2)
    		{
    			TRACE("%s bitpool adjust 2->10", __func__);
    			app_tws_adjust_encode_bitpool(0);
    		}
    		else if (g_app_tws_slave_streaminfo.bitPool == 10)
    		{
    			TRACE("%s bitpool adjust 10->25", __func__);
    			app_tws_reset_cadun_count();
    		}
        }
    }
#endif
}


void app_tws_kadun_count(void)
{
#if defined(ADJUST_BITPOOL_BETWEEN_TWS)
    uint32_t curr_tick = hal_sys_timer_get();   
    ticksOfTheLastStuttering = curr_tick;

    TRACE("app_tws_kadun_count");
    if (0 == stuttering_handling_in_progress)
    {
        first_cadun_tick = curr_tick;
        stuttering_handling_in_progress = 1;
    }

    uint32_t msSinceFirstStuterring = 0;
    if (curr_tick > first_cadun_tick)
    {
        msSinceFirstStuterring = TICKS_TO_MS(curr_tick-first_cadun_tick);
    }
    else
    {
        msSinceFirstStuterring = 
            TICKS_TO_MS((hal_sys_timer_get_max() - first_cadun_tick + 1) + curr_tick);
    }

    // within 10s, ten times of stuttering happens, need to lower the bitpool
    if(msSinceFirstStuterring < 20000)
    {
        music_cadun_count++;
        if(music_cadun_count >= 30)
        {
            music_cadun_count = 0;
            stuttering_handling_in_progress = 0;
            isLowBitPoolForStutteringExisting = 1;
            if (g_app_tws_slave_streaminfo.bitPool == TRANS_BTA_AV_CO_SBC_MAX_BITPOOL)
            {
                TRACE("%s bitpool adjust 25->10", __func__);
                app_tws_adjust_encode_bitpool(0);
            }
            else if (g_app_tws_slave_streaminfo.bitPool == 10)
            {
                TRACE("%s bitpool adjust 10->2", __func__);
                sbc_encoder.streamInfo.bitPool = 2;
                g_app_tws_slave_streaminfo.bitPool = 2;
            }
        }
    }
    else
    {
        if (music_cadun_count > 0)
        {
            music_cadun_count--;
        }
        stuttering_handling_in_progress = 0;
    }
#endif    
}

#endif
static float sbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS];

uint8_t app_tws_get_mode(void)
{
    return tws.tws_mode;
}

A2dpStream* app_tws_get_sink_stream(void)
{
    return tws.tws_sink.stream;
}


A2dpStream* app_tws_get_mobile_sink_stream(void)
{
    return tws.mobile_sink.stream;
}

extern "C" char app_tws_mode_is_master(void)
{
    return (tws.tws_mode == TWSMASTER);
}

extern "C" char app_tws_mode_is_slave(void)
{
    return (tws.tws_mode == TWSSLAVE);
}


char app_tws_mode_is_only_mobile(void)
{
    return (tws.tws_mode == TWSON);
}


extern "C" uint16_t app_tws_get_tws_conhdl(void)
{
    return tws.tws_conhdl;
}


int8_t app_tws_get_tws_hfp_vol(void)
{
    return tws.hfp_volume;
}


void app_tws_set_tws_conhdl(uint16_t conhdl)
{
     tws.tws_conhdl = conhdl;
}

AvrcpChannel *app_tws_get_avrcp_TGchannel(void)
{
    return tws.rcp;
}

A2dpStream *app_tws_get_tws_source_stream(void)
{
    return tws.tws_source.stream;
}

bool app_tws_get_source_stream_connect_status(void)
{
    return tws.tws_source.connected;
}



uint8_t *app_tws_get_peer_bdaddr(void)
{
    return tws.peer_addr.addr;
}

void app_tws_set_have_peer(bool is_have)
{
    tws.has_tws_peer = is_have;
}

void app_tws_set_tws_mode(DeviceMode mode)
{
    tws.tws_mode = mode;
}

DeviceMode app_tws_get_tws_mode(void)
{
    return tws.tws_mode;
}

void app_tws_set_media_suspend(bool enable)
{
    tws.media_suspend =enable;
}

int app_tws_get_a2dpbuff_available(void)
{
    return tws.a2dpbuff.available(&(tws.a2dpbuff));
}

void app_tws_set_eq_band_settings(int8_t *eq_band_settings)
{
    uint8_t i;
    const float EQLevel[25] = {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107 , 0.5011872,
        0.6309573,  0.794328 , 1,         1.258925 , 1.584893 ,
        1.995262 ,  2.5118864, 3.1622776, 3.9810717, 5.011872 ,
        6.309573 ,  7.943282 , 10       , 12.589254, 15.848932
    };//-12~12
    for (i=0; i<sizeof(sbc_eq_band_gain)/sizeof(float); i++){
        sbc_eq_band_gain[i] = EQLevel[eq_band_settings[i]+12];
    }
}

void app_tws_get_eq_band_gain(float **eq_band_gain)
{
    *eq_band_gain = sbc_eq_band_gain;
}

void app_tws_get_eq_band_init(void)
{
    app_tws_set_eq_band_settings(cfg_hw_aud_eq_band_settings);
}


#if 0
uint16_t app_tws_sample_rate_hacker(uint16_t sample_rate)
{
    float tws_shift_level = 0;
#ifdef A2DP_AAC_DIRECT_TRANSFER
    if(tws.mobile_codectype == AVDTP_CODEC_TYPE_SBC)
    {
        //TRACE("tws sample rate level sbc");        
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }
#else
        tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }
    else if(tws.mobile_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
    {
       // TRACE("tws sample rate level aac");            
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = AAC_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = AAC_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }

#else
        tws_shift_level = AAC_SMAPLE_RATE_44100_SHIFT_LEVEL;
#endif
    }
#else

#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }
#else
    tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
#endif
    
//   TRACE("tws sample rate level = %a",tws_shift_level*100);    
    sample_rate -= (uint16_t)((uint64_t)sample_rate*tws_shift_level*5/1000);
    return sample_rate;
}

#else
float app_tws_sample_rate_hacker(uint16_t sample_rate)
{
    float tws_shift_level = 0;
    float sample =sample_rate;
#ifdef A2DP_AAC_DIRECT_TRANSFER
    if(tws.mobile_codectype == AVDTP_CODEC_TYPE_SBC)
    {
        //TRACE("tws sample rate level sbc");        
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {

#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                
tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL_C;
            }
            else
#endif     
            {
                tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
            }
        }
        else if(sample_rate == 48000)
        {
#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                
tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL_C;
            }
            else
#endif   
            {
                tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
            }
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }
#else
        tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }
    else if(tws.mobile_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
    {
       // TRACE("tws sample rate level aac");            
#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
            tws_shift_level = AAC_SMAPLE_RATE_44100_SHIFT_LEVEL;
        }
        else if(sample_rate == 48000)
        {
            tws_shift_level = AAC_SMAPLE_RATE_48000_SHIFT_LEVEL;
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }

#else
        tws_shift_level = AAC_SMAPLE_RATE_SHIFT_LEVEL;
#endif
    }
#else

#ifdef __AUDIO_RESAMPLE__
        if(sample_rate == 44100)
        {
#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                
tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL_C;
            }
            else
#endif   
            {
                tws_shift_level = A2DP_SMAPLE_RATE_44100_SHIFT_LEVEL;
            }
        }
        else if(sample_rate == 48000)
        {
#if  !defined(SW_PLAYBACK_RESAMPLE) && defined(__TWS_RESAMPLE_ADJUST__)
            if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
            {
                
tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL_C;
            }
            else
#endif   
            {
                tws_shift_level = A2DP_SMAPLE_RATE_48000_SHIFT_LEVEL;
            }
        }
        else
        {
            ASSERT(0,"ERROR SAMPLE RATE!");
        }
#else
    tws_shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
#endif
    

    sample -= (sample*tws_shift_level*5/1000);
    return sample;
}

#endif


#ifdef A2DP_AAC_DIRECT_TRANSFER
uint8_t app_tws_get_codec_type(void)
{
    return (uint8_t)tws.mobile_codectype;
}


void app_tws_set_codec_type(uint8_t type)
{
    tws.mobile_codectype = type;
}


#endif


extern uint16_t media_seqnum;
extern uint32_t media_timestamp;
void app_tws_reset_slave_medianum(void)
{
    media_seqnum = 0;
    media_timestamp = 0;
}


uint16_t app_tws_get_slave_medianum(void)
{
    return media_seqnum;
}


A2dpStream *app_tws_get_main_sink_stream(void)
{
    return &(app_bt_device.a2dp_stream[BT_DEVICE_ID_1]); 
}

  trans_conf_t tws_trans_conf;

 A2dpSbcPacket tws_sbcPacket;





 sbcbank_t  sbcbank;

sbcpack_t *get_sbcPacket(void)
{
   int index = sbcbank.free;
   sbcbank.free +=1;
   if(sbcbank.free == 1) {
      sbcbank.free = 0;
   }
   sbcbank.sbcpacks[index].buffer = (char *)sbc_packet_buffer;
   return &(sbcbank.sbcpacks[index]);
}



#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
char opus_temp_buffer[4096];
#endif
 float *sbc_eq_band_gain_p;


static void tws_sbc_init(void)
{
    app_tws_get_eq_band_init();
    app_tws_get_eq_band_gain(&sbc_eq_band_gain_p);
    //set config for sbc trans process
    tws_trans_conf.transesz = SBCTRANSMITSZ;
    tws_trans_conf.framesz = SBCTRANFRAMESZ;
    tws_trans_conf.discardframes = TRANSDISCARDSFRAMES;
#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
    tws_trans_conf.opus_temp_buff = opus_temp_buffer;
#endif

#if defined(A2DP_AAC_DIRECT_TRANSFER)

#if defined(SLAVE_USE_OPUS) || defined(MASTER_USE_OPUS) || defined(ALL_USE_OPUS)
    tws.tws_aac_pcm = (uint16_t *)opus_temp_buffer;
#else
    tws.tws_aac_pcm = (uint16_t *)sbsamples_buf;
#endif
#endif
}


extern "C"  void app_tws_env_init(tws_env_cfg_t *cfg)
{    

    tws_sbc_init();

#if defined(SLAVE_USE_ENC_SINGLE_SBC) && defined(SLAVE_USE_ENC_SINGLE_SBC_ENQUEUE_ENCODE)
    cfg->env_encode_queue_buff.buff = tws_encode_queue_buffer;
    cfg->env_encode_queue_buff.size = SBCTRANSMITSZ+MASTER_DECODE_PCM_FRAME_LENGTH;
#else
    cfg->env_encode_queue_buff.buff = 0;
    cfg->env_encode_queue_buff.size = 0;
#endif
#ifdef __TWS_SHARE_BUFF__
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;

    cfg->env_pcm_buff.buff = tws_big_buff;
    cfg->env_pcm_buff.size = PCM_BUFFER_SZ_TWS;

    cfg->env_sbc_buff.buff = tws_big_buff+PCM_BUFFER_SZ_TWS;
    cfg->env_sbc_buff.size = TRANS_BUFFER_SZ_TWS;

    cfg->env_a2dp_buff.buff = tws_big_buff+PCM_BUFFER_SZ_TWS+TRANS_BUFFER_SZ_TWS;
    cfg->env_a2dp_buff.size = A2DP_BUFFER_SZ_TWS;


#else

#if defined(SLAVE_USE_ENC_SINGLE_SBC)
    cfg->env_sbsample_buff.buff = sbsamples_buf;
    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;

#else
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;

#endif
    cfg->env_pcm_buff.buff = tws_pcm_buffer;
    cfg->env_pcm_buff.size = PCM_BUFFER_SZ_TWS;

    cfg->env_sbc_buff.buff = tws_sbc_buffer;
    cfg->env_sbc_buff.size = TRANS_BUFFER_SZ_TWS;

    cfg->env_a2dp_buff.buff = tws_a2dp_sbc_buffer;
    cfg->env_a2dp_buff.size = A2DP_BUFFER_SZ_TWS;
#endif
    cfg->env_trigger_delay = TWS_PLAYER_TRIGGER_DELAY;
    cfg->env_trigger_a2dp_buff_thr = A2DP_BUFFER_SZ_TWS/8;
    cfg->env_trigger_aac_buff_thr = 10;//Modified by ATX : Leon.He_20180310:for improve AAC TWS stability
    cfg->env_slave_trigger_thr = TWS_PLAYER__SLAVE_TRIGGER_THR;

#ifdef __TWS_CALL_DUAL_CHANNEL__
    btdrv_set_bt_pcm_triggler_en(1);
#endif

    
}


uint8_t *tws_pcm_buff=NULL;
uint8_t *tws_sbc_buff=NULL;
extern "C" void app_tws_buff_alloc(void)
{
#if defined (A2DP_AAC_DIRECT_TRANSFER)
    void * aac_info_prt = NULL;
#endif


//    uint8_t *buff;
    TRACE("%s entry",__FUNCTION__);
#if defined (A2DP_AAC_DIRECT_TRANSFER)
    if(tws.mobile_codectype == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
 //       app_audio_mempool_get_buff((uint8_t **)&twsd->tws_aac_pcm, DECODE_AAC_PCM_FRAME_LENGTH);
//       app_audio_mempool_get_buff(&aac_input_mid_buf, AAC_READBUF_SIZE);
        app_audio_mempool_get_buff((uint8_t **)&aac_info_prt, GetAACinfoSize());
        SetAACInfoBase(aac_info_prt);

    }         
#endif    
#if 0
    if(tws_pcm_buff == NULL)
        app_audio_mempool_get_buff(&tws_pcm_buff, PCM_BUFFER_SZ_TWS);
    if(tws_sbc_buff == NULL)
        app_audio_mempool_get_buff(&tws_sbc_buff, TRANS_BUFFER_SZ_TWS);
    
    InitCQueue(&(tws.pcmbuff.queue), PCM_BUFFER_SZ_TWS, ( CQItemType *)tws_pcm_buff);
    InitCQueue(&(tws.sbcbuff.queue), TRANS_BUFFER_SZ_TWS, ( CQItemType *)tws_sbc_buff);
#endif

#ifdef __TWS_SHARE_BUFF__
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
tws_env_cfg_t *cfg = app_tws_get_cfg();
#endif

    if(sbc_transmit_buffer !=NULL || sbc_packet_buffer !=NULL)
    {
        TRACE("BUFF IS NOT FREE SOMTHING IS WRONG");
    }
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
    
    if(sbsamples_buf !=NULL)
    {
        TRACE("BUFF2 IS NOT FREE SOMTHING IS WRONG");
    }
#endif    
    int lock = int_lock();
    if(sbc_transmit_buffer == NULL)
        app_audio_mempool_get_buff(&sbc_transmit_buffer, SBCTRANSMITSZ);
    if(sbc_packet_buffer == NULL)
        app_audio_mempool_get_buff(&sbc_packet_buffer, SBCTRANSMITSZ);

#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
    if(sbsamples_buf == NULL)
        app_audio_mempool_get_buff(&sbsamples_buf, SBCTRANSMITSZ);
    cfg->env_sbsample_buff.buff = sbsamples_buf;
    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;    
#if defined(A2DP_AAC_DIRECT_TRANSFER)
    tws.tws_aac_pcm = (uint16_t *)sbsamples_buf;
#endif    
#endif


    int_unlock(lock);

#endif

//    cfg->env_sbsample_buff.buff = sbsamples_buf;
//    cfg->env_sbsample_buff.size = SBCTRANSMITSZ;

    

    TRACE("%s exit",__FUNCTION__);

}


#ifdef A2DP_AAC_ON
extern HAACDecoder *hAACDecoder;
#endif
extern "C" void app_tws_buff_free(void)
{
    TRACE("%s entry",__FUNCTION__);

#if defined (A2DP_AAC_ON)
    hAACDecoder = NULL;
#if defined (A2DP_AAC_DIRECT_TRANSFER)
//    twsd->tws_aac_pcm = NULL;
 //   aac_input_mid_buf = NULL;
#endif
#endif

#ifdef __TWS_SHARE_BUFF__
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
tws_env_cfg_t *cfg = app_tws_get_cfg();
#endif
    int lock = int_lock();
    sbc_transmit_buffer = NULL;
    sbc_packet_buffer = NULL;
#if defined(SLAVE_USE_ENC_SINGLE_SBC) || defined(A2DP_AAC_DIRECT_TRANSFER)
    sbsamples_buf = NULL;
    cfg->env_sbsample_buff.buff = 0;
    cfg->env_sbsample_buff.size = 0;
    
#if defined(A2DP_AAC_DIRECT_TRANSFER)
    tws.tws_aac_pcm = NULL;
#endif
#endif
    int_unlock(lock);

#endif
    
    TRACE("%s exit",__FUNCTION__);
    
}


extern "C" uint8_t *app_tws_get_transmitter_buff(void)
{
    return sbc_transmit_buffer;
}


uint8_t pcm_wait_triggle=0;

extern "C" void app_tws_set_pcm_wait_triggle(uint8_t triggle_en)
{
    pcm_wait_triggle = triggle_en;
}

extern "C" uint8_t app_tws_get_pcm_wait_triggle(void)
{
    return pcm_wait_triggle;
}


#ifdef __TWS_CALL_DUAL_CHANNEL__


uint8_t btpcm_wait_triggle=0;

extern "C" void app_tws_set_btpcm_wait_triggle(uint8_t triggle_en)
{
    btpcm_wait_triggle = triggle_en;
}

extern "C" uint8_t app_tws_get_btpcm_wait_triggle(void)
{
    return btpcm_wait_triggle;
}

extern "C" void app_tws_set_pcm_triggle(void)
{
    uint32_t curr_ticks;
    uint32_t triggler_time;
    if(app_tws_mode_is_master() || app_tws_mode_is_slave())
    {
        curr_ticks = bt_syn_get_curr_ticks(app_tws_get_tws_conhdl());
    }
    else
    {
        curr_ticks = btdrv_syn_get_curr_ticks();
    }
    triggler_time = (curr_ticks+0x80)-((curr_ticks+0x80)%96);
    btdrv_syn_trigger_codec_en(0);
    btdrv_syn_clr_trigger();
    
    btdrv_enable_playback_triggler(SCO_TRIGGLE_MODE);
    if(app_tws_mode_is_master() || app_tws_mode_is_slave())
    {
        bt_syn_set_tg_ticks(triggler_time,app_tws_get_tws_conhdl());
    }
    else
    {
        bt_syn_set_tg_ticks(triggler_time,0);
    }
    
    btdrv_syn_trigger_codec_en(1);    
    TRACE("enable pcm triggler curr clk =%x, triggle clk=%x",btdrv_syn_get_curr_ticks(),triggler_time);
#ifdef CHIP_BEST2000
        TRACE("0xd0224024 = %x,0xd022007c=%x",*(volatile uint32_t *)0xd0224024,*(volatile uint32_t *)0xd022007c);
        TRACE("0xd02201b4 = %x,0xd02201cc=%x",*(volatile uint32_t *)0xd02201b4,*(volatile uint32_t *)0xd02201cc);
#endif


    
}

#endif

int app_tws_get_big_buff(uint8_t **buf_p, uint32_t *size)
{
    ASSERT(tws.decoder_running ==  false, "%s decoder_running:%d", __func__, tws.decoder_running);

    *buf_p = tws_big_buff;
    *size = sizeof(tws_big_buff);

    return 0;
}



void app_tws_check_max_slot_setting(void)
{
#ifdef CHIP_BEST2000
    if(app_tws_get_tws_conhdl()<0x80 || app_tws_get_tws_conhdl()>0x83)
    {
        TRACE("TWS CONHDL ERROR!! CHENCK THE CONNECT");
        return;
    }
    if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
    {
#if IS_ENABLE_BT_DRIVER_REG_DEBUG_READING    
        TRACE("MAX SLOT=%x", BT_DRIVER_GET_U32_REG_VAL(0xc0003df8));
        TRACE("MAX len=%x", BT_DRIVER_GET_U32_REG_VAL(0xc0003e04));
#endif
        uint8_t link_id = btdrv_conhdl_to_linkid(app_tws_get_tws_conhdl()); 
        uint32_t table = BT_DRIVER_GET_U32_REG_VAL(0xc0003df8);
        uint32_t len = BT_DRIVER_GET_U32_REG_VAL(0xc0003e04);
        uint16_t table_id = 0xFFFF;
        if(link_id == 0)
        {
            table_id = table &0xffff;
        }
        else if(link_id == 1)
        {
            table_id = table >>16;
        }
        
        if(table_id == 0x8 || table_id ==0xa)
        {
            TRACE("TX TABLE IS ONLY 1 SLOT PACKET");
            if(link_id ==0)
            {
                len &= 0xffff0000;
                len |= 0x2a7;
                table |= 0x110a;
                BT_DRIVER_PUT_U32_REG_VAL(0xc0003e04, len);
                BT_DRIVER_PUT_U32_REG_VAL(0xc0003df8, table);
            }
            else if(link_id ==1)
            {
                len &= 0xffff;
                len |= 0x2a70000;
                table |= 0x110a0000;
                BT_DRIVER_PUT_U32_REG_VAL(0xc0003e04, len);
                BT_DRIVER_PUT_U32_REG_VAL(0xc0003df8, table);       
            }
        }
    }
#endif    
}


#endif
