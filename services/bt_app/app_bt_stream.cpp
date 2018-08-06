//#include "mbed.h"
#include <stdio.h>
#include <assert.h>

#include "cmsis_os.h"
#include "tgt_hardware.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "audioflinger.h"
#include "lockcqueue.h"
#include "hal_trace.h"
#include "hal_cmu.h"
#include "hal_chipid.h"
#include "analog.h"
#include "app_bt_stream.h"
#include "app_overlay.h"
#include "app_audio.h"
#include "app_utils.h"
#ifdef __TWS__
#include "app_tws.h"
#include "app_tws_if.h"
#endif
#include "nvrecord.h"
#include "nvrecord_env.h"
#include "nvrecord_dev.h"

#ifdef MEDIA_PLAYER_SUPPORT
#include "resources.h"
#include "app_media_player.h"
#endif
#ifdef __FACTORY_MODE_SUPPORT__
#include "app_factory_audio.h"
#endif

#include "app_ring_merge.h"

#include "bt_drv.h"
#include "bt_xtal_sync.h"
#include "besbt.h"
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

#include "cqueue.h"
#include "btapp.h"

#ifdef TWS_RBCODEC_PLAYER
#include "rbplay.h"
#endif

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
#include "app_key.h"
#include "player_role.h"
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)|| defined(__HW_CODEC_IIR_EQ_PROCESS__)
#include "audio_eq.h"
#endif

#include "app_bt_media_manager.h"

#include "string.h"
#include "hal_location.h"
#include "hal_codec.h"
#include "bt_drv_interface.h"

#include "audio_resample_ex.h"
#include "hal_sleep.h"
enum PLAYER_OPER_T {
    PLAYER_OPER_START,
    PLAYER_OPER_STOP,
    PLAYER_OPER_RESTART,
};

#if (AUDIO_OUTPUT_VOLUME_DEFAULT < 1) || (AUDIO_OUTPUT_VOLUME_DEFAULT > 17)
#error "AUDIO_OUTPUT_VOLUME_DEFAULT out of range"
#endif
int8_t stream_local_volume = (AUDIO_OUTPUT_VOLUME_DEFAULT);

struct btdevice_volume *btdevice_volume_p;
#ifdef __BT_ANC__
#define ANC_SCO_SPK_BUF_SZ      (48*2*2*30)  // 96:sample rate; 2:LR; 2: short; 30:30ms pingpong
uint32_t anc_sco_spk_buf[ANC_SCO_SPK_BUF_SZ/4];
uint8_t bt_sco_samplerate_ratio = 0;
extern void us_fir_init(void);
extern uint32_t voicebtpcm_pcm_resample (short* src_samp_buf, uint32_t src_smpl_cnt, short* dst_samp_buf);
#endif
extern "C" uint8_t is_sco_mode (void);

static uint8_t bt_sco_mode;
extern "C"   uint8_t __attribute__((section(".fast_text_sram")))  is_sco_mode(void) 
{
    return bt_sco_mode;
}
static uint8_t bt_sbc_mode;
extern "C" uint8_t __attribute__((section(".fast_text_sram")))  is_sbc_mode(void)
{
    return bt_sbc_mode;
}



uint32_t a2dp_audio_more_data(uint8_t *buf, uint32_t len);
int a2dp_audio_init(void);
int a2dp_audio_deinit(void);

enum AUD_SAMPRATE_T a2dp_sample_rate = AUD_SAMPRATE_48000;

uint16_t bt_sco_current_codecid = HF_SCO_CODEC_CVSD;
void bt_sco_set_current_codecid(uint16_t id)
{
    bt_sco_current_codecid = id;
}
uint16_t bt_sco_get_current_codecid(void)
{
    return bt_sco_current_codecid;
}

#if 0
int bt_sbc_player_setup(uint8_t freq)
{
    struct AF_STREAM_CONFIG_T *stream_cfg = NULL;
    static uint8_t sbc_samp_rate = 0xff;

    if (sbc_samp_rate == freq)
        return 0;

    switch (freq) {
        case A2D_SBC_IE_SAMP_FREQ_16:
        case A2D_SBC_IE_SAMP_FREQ_32:
        case A2D_SBC_IE_SAMP_FREQ_48:
            a2dp_sample_rate = AUD_SAMPRATE_48000;
            break;
        case A2D_SBC_IE_SAMP_FREQ_44:
            a2dp_sample_rate = AUD_SAMPRATE_44100;
            break;
        default:
            break;
    }

    af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg, true);
    stream_cfg->sample_rate = a2dp_sample_rate;
    af_stream_setup(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, stream_cfg);
    sbc_samp_rate = freq;

    return 0;
}
#else

void bt_sbc_player_retrigger(uint32_t trigger_time)
{
    TRACE("[%s] trigger_time = %d", __func__, trigger_time);

    // hal_audma_start and hal_audma_stop will produce noise
//    af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
//    af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);

    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    app_tws_set_trigger_time(trigger_time);
    af_stream_restart(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}

void bt_sbc_player_restart(uint32_t trigger_time)
{
    TRACE("[%s] trigger_time = %d", __func__, trigger_time);

    // hal_audma_start and hal_audma_stop will produce noise
//    af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
//    af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);

    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    app_tws_set_trigger_time(trigger_time);
    af_stream_restart(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}

void bt_sbc_player_pause()
{
    TRACE("[%s]...", __func__);

    //app_tws_set_trigger_time(0);
    af_stream_pause(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
}



int bt_sbc_player_setup(uint8_t opt, uint32_t param)
{
    switch (opt)
    {
        case APP_BT_SBC_PLAYER_TRIGGER:
            bt_sbc_player_retrigger(param);
            break;
        case APP_BT_SBC_PLAYER_RESTART:
            bt_sbc_player_restart(param);
            break;
        case APP_BT_SBC_PLAYER_PAUSE:
            bt_sbc_player_pause();
            break;
        default:
            ASSERT(0, "[%s] is invalid opt", __func__, opt);
            break;
    }

    return 0;
}


#endif


enum AUD_SAMPRATE_T bt_parse_sbc_sample_rate(uint8_t sbc_samp_rate)
{
    enum AUD_SAMPRATE_T sample_rate;
    sbc_samp_rate = sbc_samp_rate & A2D_SBC_IE_SAMP_FREQ_MSK;

    switch (sbc_samp_rate)
    {
        case A2D_SBC_IE_SAMP_FREQ_16:
//            sample_rate = AUD_SAMPRATE_16000;
//            break;
        case A2D_SBC_IE_SAMP_FREQ_32:
//            sample_rate = AUD_SAMPRATE_32000;
//            break;
        case A2D_SBC_IE_SAMP_FREQ_48:
            sample_rate = AUD_SAMPRATE_48000;
            break;
        case A2D_SBC_IE_SAMP_FREQ_44:
            sample_rate = AUD_SAMPRATE_44100;
            break;
        default:
            ASSERT(0, "[%s] 0x%x is invalid", __func__, sbc_samp_rate);
            break;
    }
    return sample_rate;
}

void bt_store_sbc_sample_rate(enum AUD_SAMPRATE_T sample_rate)
{
    a2dp_sample_rate = sample_rate;
}

enum AUD_SAMPRATE_T bt_get_sbc_sample_rate(void)
{
    return a2dp_sample_rate;
}

enum AUD_SAMPRATE_T bt_parse_store_sbc_sample_rate(uint8_t sbc_samp_rate)
{
    enum AUD_SAMPRATE_T sample_rate;

    sample_rate = bt_parse_sbc_sample_rate(sbc_samp_rate);
    bt_store_sbc_sample_rate(sample_rate);

    return sample_rate;
}

void merge_stereo_to_mono_16bits(int16_t *src_buf, int16_t *dst_buf,  uint32_t src_len)
{
    uint32_t i = 0;
    for (i = 0; i < src_len; i+=2) {
        dst_buf[i] = (src_buf[i]>>1) + (src_buf[i+1]>>1);
        dst_buf[i+1] = dst_buf[i];
    }
}

#ifdef __TWS__
extern uint32_t tws_audout_pcm_more_data(uint8_t * buf, uint32_t len);

extern  tws_dev_t  tws;
extern "C" void OS_NotifyEvm(void);


#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#define TWS_RESAMPLE_ITER_NUM               512
static struct APP_RESAMPLE_T *tws_resample;

static int tws_resample_iter(uint8_t *buf, uint32_t len)
{
    int ret;

    ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);

    return ret;
}
#endif

#if defined(TWS_SBC_PATCH_SAMPLES)
static uint32_t curr_sample_counter = 0;
#endif

#ifdef HCI_DEBUG
extern "C" uint32_t thread_index;
extern "C" uint32_t thread_count;
#endif
extern void app_tws_check_to_restore_from_stuttering(void);
uint32_t tws_audout_pcm_more_data(uint8_t * buf, uint32_t len)
{
    int ret = 0;
    if(tws.wait_pcm_signal == true){
        tws.lock_prop(&tws);
        tws.wait_pcm_signal = false;
        tws.unlock_prop(&tws);
    }
#ifdef HCI_DEBUG
    TRACE("len:%d p: %d a:%d %d %d \n", len, LengthOfCQueue(&tws.pcmbuff.queue), LengthOfCQueue(&tws.a2dpbuff.queue), 
                thread_index, thread_count);
#endif
#if defined(TWS_SBC_PATCH_SAMPLES)
    if ((tws.mobile_codectype == AVDTP_CODEC_TYPE_SBC)
            && (curr_sample_counter < TWS_SBC_PATCH_SAMPLES_CNT_NUM) && ((curr_sample_counter + len) >= TWS_SBC_PATCH_SAMPLES_CNT_NUM)) {
        ASSERT(len > TWS_SBC_PATCH_SAMPLES_PAT_NUM, "%s:len should > TWS_SBC_PATCH_SAMPLES_PAT_NUM", __func__);
        len -= TWS_SBC_PATCH_SAMPLES_PAT_NUM;
        ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
        memcpy(buf + len , buf + len - TWS_SBC_PATCH_SAMPLES_PAT_NUM, TWS_SBC_PATCH_SAMPLES_PAT_NUM);
        curr_sample_counter += len;
        curr_sample_counter %= TWS_SBC_PATCH_SAMPLES_CNT_NUM;
    }
    else
#endif
    {
#if defined(TWS_SBC_PATCH_SAMPLES)
        curr_sample_counter += len;
#endif
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#ifdef A2DP_AAC_DIRECT_TRANSFER
        if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
        }
        else
#endif
        {
            ret = app_playback_resample_run(tws_resample, buf, len);
        }
#else
        ret = tws.pcmbuff.read(&(tws.pcmbuff),buf,len);
#endif
    }
    tws.pcmbuff.put_space(&(tws.pcmbuff));

#if defined(ADJUST_BITPOOL_BETWEEN_TWS)
	if (TWSMASTER == tws.tws_mode)
	{
		app_tws_check_to_restore_from_stuttering();
	}
#endif
    OS_NotifyEvm();

    if(ret) {
        if(tws.tws_mode == TWSSLAVE && tws.slave_player_restart_pending == false)
        {
            tws.lock_prop(&tws);
            app_tws_reset_slave_medianum();
            tws.unlock_prop(&tws);
 //           tws_player_pause_player_req(1);
    //        tws.slave_player_restart_pending = true;
   //          tws_player_stop();   
            
        }
        TRACE("audout underflow \n");
        ret = 0;
    }else{

        if(tws.paused == false){
            audout_pcmbuff_thres_monitor();
        }
#if defined(__TWS_CLK_SYNC__)
        {
            uint32_t offset,intraslot_offset,calc_master_bit_cnt,calc_master_clk;
            int32_t diff;
            intraslot_offset = btdrv_rf_bit_offset_get()&0x3FF;
            offset = btdrv_syn_get_offset_ticks(btdrv_conhdl_to_linkid(app_tws_get_tws_conhdl()));
#if defined(A2DP_AAC_DIRECT_TRANSFER)
            if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC){
                tws.pcmbuff.processed_frame_num += len/2048;
            }else
#endif
            {
                tws.pcmbuff.processed_frame_num += len/256;
            }
            tws.pcmbuff.locked_bit_cnt = btdrv_get_locked_bit_cnt();//us
            tws.pcmbuff.locked_native_clk = btdrv_get_locked_native_clk();//frame
            tws_sync_remote_lock_time_from_a2dp_buffer(&tws.pcmbuff);
#ifdef BT_XTAL_SYNC
            if(tws.pcmbuff.remote_locked_native_clk != 0xFFFFFFFF)
            {
                calc_master_bit_cnt = (tws.pcmbuff.locked_bit_cnt+625-intraslot_offset)%625;
                calc_master_clk = tws.pcmbuff.locked_native_clk*2+offset+(tws.pcmbuff.locked_bit_cnt+625-intraslot_offset)/625*2;
                diff = ((int32_t)(calc_master_clk-tws.pcmbuff.remote_locked_native_clk*2))*625/2 + (int32_t)(calc_master_bit_cnt-tws.pcmbuff.remote_locked_bit_cnt);
                bt_xtal_sync_new(diff);
            }
#endif
        }
#else
#ifdef BT_XTAL_SYNC
        bt_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif
#endif
        ret = len;
    }
#if 0    
    if(tws.tws_mode == TWSSLAVE && slave_packet_count >=2000 && tws.slave_player_restart_pending == false)
    {
     //   tws.lock_prop(&tws);
        slave_packet_count = 0;
     //   app_tws_reset_slave_medianum();
     //   tws.unlock_prop(&tws);
        tws_player_pause_player_req(1);
        tws.slave_player_restart_pending = true;     
//        tws_player_stop();   
    }
#endif    
    return ret;
}


#endif

FRAM_TEXT_LOC uint32_t bt_sbc_player_more_data(uint8_t *buf, uint32_t len)
{
#ifdef BT_XTAL_SYNC
#ifndef __TWS__
    bt_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif
#endif

#ifdef __TWS__
    tws_audout_pcm_more_data(buf, len);
#ifdef __BT_WARNING_TONE_MERGE_INTO_STREAM_SBC__
    app_ring_merge_more_data(buf, len);
#endif

#else
    a2dp_audio_more_data(buf, len);
    app_ring_merge_more_data(buf, len);
#ifdef __AUDIO_OUTPUT_MONO_MODE__
    merge_stereo_to_mono_16bits((int16_t *)buf, (int16_t *)buf, len/2);
#endif
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__) || defined(__AUDIO_DRC__) || defined(__HW_CODEC_IIR_EQ_PROCESS__)
    audio_eq_run(buf, len);
#endif
#ifndef __TWS__
    OS_NotifyEvm();
#endif
    return len;
}

int bt_sbc_player(enum PLAYER_OPER_T on, enum APP_SYSFREQ_FREQ_T freq,uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    enum AUD_SAMPRATE_T sample_rate;
    static bool isRun =  false;
    uint8_t* bt_audio_buff = NULL;
#if defined(__HW_FIR_EQ_PROCESS__)
    uint8_t* bt_eq_buff = NULL;
    uint32_t eq_buff_size;
#endif

    TRACE("bt_sbc_player work:%d op:%d freq:%d,samplerate=%d,ticks=%d", isRun, on, freq,a2dp_sample_rate,trigger_ticks);

    if ((isRun && on == PLAYER_OPER_START) || (!isRun && on == PLAYER_OPER_STOP))
        return 0;

    if (on == PLAYER_OPER_STOP || on == PLAYER_OPER_RESTART) {
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__) ||defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_close();
#endif
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
		bt_sbc_mode = 0;
        if (on == PLAYER_OPER_STOP) {
#ifndef FPGA
#ifdef BT_XTAL_SYNC
//            osDelay(50);
            bt_term_xtal_sync();
#endif
#endif
            a2dp_audio_deinit();
#ifdef __TWS__
            app_tws_deactive();
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
            app_playback_resample_close(tws_resample);
            tws_resample = NULL;
#endif
#endif
            app_overlay_unloadall();

            hal_cpu_wake_unlock(HAL_CPU_WAKE_LOCK_USER_6);
            app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
            af_set_priority(osPriorityAboveNormal);
        }
    }

    if (on == PLAYER_OPER_START || on == PLAYER_OPER_RESTART) {
        af_set_priority(osPriorityHigh);
        if (freq < APP_SYSFREQ_52M) {
            freq = APP_SYSFREQ_52M;
        }
#ifdef __BT_ONE_BRING_TWO__
        if (MEC(activeCons)>1){
            freq = APP_SYSFREQ_104M;
        }
#endif
        hal_cpu_wake_lock(HAL_CPU_WAKE_LOCK_USER_6);

        stream_local_volume = btdevice_volume_p->a2dp_vol;
        app_audio_mempool_init();
        app_audio_mempool_get_buff(&bt_audio_buff, BT_AUDIO_BUFF_SIZE);

#if defined(__SW_IIR_EQ_PROCESS__)&&defined(__HW_FIR_EQ_PROCESS__)
#ifdef __TWS__
        freq = APP_SYSFREQ_78M;
#endif
        if (audio_eq_fir_cfg.len > 128) {
            if (freq < APP_SYSFREQ_104M) {
                freq = APP_SYSFREQ_104M;
            }
        }
#else // !EQ
#ifdef __TWS__
        if(app_tws_mode_is_only_mobile())
        {
            freq = APP_SYSFREQ_52M;
        }
#ifdef A2DP_AAC_DIRECT_TRANSFER
        else if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            freq = APP_SYSFREQ_104M;
        }
#endif
        else
        {
#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
            freq = APP_SYSFREQ_104M;
#else        
            freq = APP_SYSFREQ_52M;
#endif
        }
#endif // __TWS__
#endif // !EQ
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, freq);

        memset(&stream_cfg, 0, sizeof(stream_cfg));
        memset(bt_audio_buff, 0, BT_AUDIO_BUFF_SIZE);
        if (on == PLAYER_OPER_START) {
#ifndef FPGA
#if defined(A2DP_AAC_ON)
            if (a2dp_get_codec_type(BT_DEVICE_ID_1) == AVDTP_CODEC_TYPE_MPEG2_4_AAC
                    || app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC) {
            	app_overlay_select(APP_OVERLAY_A2DP_AAC);
            }
            else
#endif
            {
                app_overlay_select(APP_OVERLAY_A2DP);
            }
#endif

#ifdef __TWS__
            app_tws_active();

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)
#ifdef A2DP_AAC_DIRECT_TRANSFER
            if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
            }
            else
#endif
            {
                enum AUD_CHANNEL_NUM_T chans;

#ifdef __TWS_1CHANNEL_PCM__
                chans = AUD_CHANNEL_NUM_1;
#else
                chans = AUD_CHANNEL_NUM_2;
#endif

#ifdef RESAMPLE_ANY_SAMPLE_RATE
                tws_resample = app_playback_resample_any_open(chans, tws_resample_iter, TWS_RESAMPLE_ITER_NUM * chans,
                    (float)a2dp_sample_rate / AUD_SAMPRATE_50781);
#else
                tws_resample = app_playback_resample_open(a2dp_sample_rate, chans, tws_resample_iter, TWS_RESAMPLE_ITER_NUM * chans);
#endif
            }
#endif
#endif // __TWS__

#ifdef BT_XTAL_SYNC
#if 0
            if (app_tws_mode_is_slave()) {
                btdrv_rf_bit_offset_track_enable(true);
            }
            else {
                btdrv_rf_bit_offset_track_enable(false);
            }
#endif

#ifdef __TWS__
            if(app_tws_mode_is_only_mobile())
            {
                btdrv_rf_bit_offset_track_enable(false);
            }
            else
#endif
            {
                btdrv_rf_bit_offset_track_enable(true);
            }
            bt_init_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif // BT_XTAL_SYNC

#ifdef  __FPGA_FLASH__
            app_overlay_select(APP_OVERLAY_A2DP);
#endif
			bt_sbc_mode = 1;

        }

        if (on == PLAYER_OPER_START) {
            a2dp_audio_init();
        }

#if defined(__AUDIO_RESAMPLE__) && defined(SW_PLAYBACK_RESAMPLE)

#ifdef A2DP_AAC_DIRECT_TRANSFER
        if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            sample_rate = a2dp_sample_rate;
        }
        else
#endif
        {
            sample_rate = AUD_SAMPRATE_50781;
        }
#else
        sample_rate = a2dp_sample_rate;
#endif

        stream_cfg.bits = AUD_BITS_16;
//        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = sample_rate;
#ifdef FPGA
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#endif
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
#ifdef __TWS_1CHANNEL_PCM__

        if(CFG_HW_AUD_OUTPUT_PATH_SPEAKER_DEV == (AUD_CHANNEL_MAP_CH0 | AUD_CHANNEL_MAP_CH1))
        {
            ASSERT(0,"CAN'T SET STEREO CHANNEL IN THIS FUNCION");
        }
        else
        {
            stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
        }
#else
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
#endif
        
        TRACE("SBC START CHANNEL NUM=%x",stream_cfg.channel_num);
        stream_cfg.vol = stream_local_volume;
        stream_cfg.handler = bt_sbc_player_more_data;

        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        stream_cfg.data_size = BT_AUDIO_BUFF_SIZE;

        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

#if 0//def __TWS__
        struct HAL_CODEC_CONFIG_T codec_cfg;
        float shift_level;
        codec_cfg.sample_rate = stream_cfg.sample_rate;

#ifdef A2DP_AAC_DIRECT_TRANSFER
        
        if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_SBC)
        {
            shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
            TRACE("bt stream sbc level ");            
        }
        else if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
        {
            shift_level = AAC_SMAPLE_RATE_SHIFT_LEVEL;
            TRACE("bt stream aac level ");                        
        }
#else
        shift_level = A2DP_SMAPLE_RATE_SHIFT_LEVEL;
#endif
        hal_codec_setup_stream_smaple_rate_shift(HAL_CODEC_ID_0, AUD_STREAM_PLAYBACK, &codec_cfg, shift_level);
#endif

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__ )||defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_init();
#ifdef __HW_FIR_EQ_PROCESS__
#if defined(CHIP_BEST2000)
        eq_buff_size = BT_AUDIO_BUFF_SIZE*2;
#else
        eq_buff_size = BT_AUDIO_BUFF_SIZE;
#endif
        app_audio_mempool_get_buff(&bt_eq_buff, eq_buff_size);

#ifdef __TWS_1CHANNEL_PCM__
        audio_eq_open(sample_rate, AUD_BITS_16, AUD_CHANNEL_NUM_1, bt_eq_buff, eq_buff_size);
#else
        audio_eq_open(sample_rate, AUD_BITS_16, AUD_CHANNEL_NUM_2, bt_eq_buff, eq_buff_size);
#endif
#else
#ifdef __TWS_1CHANNEL_PCM__
        audio_eq_open(sample_rate, AUD_BITS_16, AUD_CHANNEL_NUM_1, NULL, 0);

#else
        audio_eq_open(sample_rate, AUD_BITS_16, AUD_CHANNEL_NUM_2, NULL, 0);
#endif
#endif
#endif
#ifdef __TWS_RESAMPLE_ADJUST__
#if defined(__AUDIO_RESAMPLE__) && !defined(SW_PLAYBACK_RESAMPLE)
        if(hal_get_chip_metal_id() >= HAL_CHIP_METAL_ID_2)
        {

#ifdef A2DP_AAC_DIRECT_TRANSFER
            if(app_tws_get_codec_type() == AVDTP_CODEC_TYPE_MPEG2_4_AAC)
            {
                af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
            }
            else
#endif
            {
                if(sample_rate == 44100)
                    af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 0.99988);
                else if(sample_rate == 48000)
                    af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 0.99989);
            }
        }
#endif
#else
        af_codec_tune_resample_rate(AUD_STREAM_PLAYBACK, 1);
#endif

        app_tws_set_trigger_time(trigger_ticks);
#if defined(__TWS_CLK_SYNC__)
        btdrv_enable_dma_lock_clk();
#endif
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    }

    isRun = (on != PLAYER_OPER_STOP);
    return 0;
}

#if defined(TWS_RBCODEC_PLAYER) || defined(TWS_LINEIN_PLAYER)
void bt_sbc_player_retrigger(uint32_t trigger_time);
int bt_sbc_local_player_init(bool on, uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    static uint8_t* bt_audio_buff = NULL;
    static uint8_t* bt_eq_buff = NULL;

    if (bt_audio_buff == NULL) {
        app_audio_mempool_get_buff(&bt_audio_buff, BT_AUDIO_BUFF_SIZE);
    }

    TRACE("bt_sbc_local_player_init work:%d op:%d %d", isRun, on, trigger_ticks);
    if(on && isRun){
        bt_sbc_player_retrigger(trigger_ticks);
        return 0;
    }
    // app_bt_stream_volumeset(stream_volume.a2dp_vol);
    if (on)
    {
        TRACE("TODO: get volume?");
        // stream_local_volume = btdevice_volume_p->a2dp_vol;

#ifdef __TWS__
        app_tws_active();

        a2dp_audio_init();
#endif  //__TWS__

#ifdef BT_XTAL_SYNC
        bt_init_xtal_sync(BT_XTAL_SYNC_MODE_MUSIC);
#endif
        memset(bt_audio_buff, 0, sizeof(bt_audio_buff));
        memset(&stream_cfg, 0, sizeof(stream_cfg));

        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.sample_rate = bt_get_sbc_sample_rate();;
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;

        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;

        stream_cfg.vol = stream_local_volume;
        stream_cfg.handler = bt_sbc_player_more_data;
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        stream_cfg.data_size = BT_AUDIO_BUFF_SIZE;
        stream_cfg.trigger_time = trigger_ticks;

        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__ )||defined(__HW_CODEC_IIR_EQ_PROCESS__)
        audio_eq_init();
		if (bt_eq_buff == NULL) {
			app_audio_mempool_get_buff(&bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
		}
#if 0//def __AUDIO_RESAMPLE__
        audio_eq_open(AUD_SAMPRATE_50700, AUD_BITS_16,AUD_CHANNEL_NUM_2, bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
#else
        audio_eq_open(AUD_SAMPRATE_44100, AUD_BITS_16,AUD_CHANNEL_NUM_2, bt_eq_buff, BT_AUDIO_BUFF_SIZE*2);
#endif
#endif

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
    }else{
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#if defined(__SW_IIR_EQ_PROCESS__) || defined(__HW_FIR_EQ_PROCESS__)
        audio_eq_close();
#endif
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#ifdef BT_XTAL_SYNC
        osDelay(50);
        bt_term_xtal_sync();
#endif
        a2dp_audio_deinit();
#ifdef __TWS__
        app_tws_deactive();
#endif

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
    }

    isRun=on;
    return 0;
}
#endif


#ifdef SPEECH_SIDETONE
extern "C" {
void hal_codec_sidetone_enable(int ch, int gain);
void hal_codec_sidetone_disable(void);
}
#endif

void speech_tx_aec_set_frame_len(int len);
int voicebtpcm_pcm_audio_init(void);
int voicebtpcm_pcm_audio_deinit(void);
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len);
#if defined(__BT_ANC__)
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len,uint8_t *ec_buf);
#else
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len);
#endif
int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len);
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len);
uint8_t btapp_hfp_need_mute(void);

bool bt_sco_codec_is_msbc(void)
{
    bool en = false;
#ifdef HFP_1_6_ENABLE
    if (bt_sco_get_current_codecid() == HF_SCO_CODEC_MSBC)
    {
        en = true;
    }
    else
#endif
    {
        en = false;
    }

    return en;
}

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
#ifdef CHIP_BEST1000
#error "Unsupport SW_SCO_RESAMPLE on best1000 by now"
#endif
#ifdef NO_SCO_RESAMPLE
#error "Conflicted config: NO_SCO_RESAMPLE and SW_SCO_RESAMPLE"
#endif

#define SCO_MONO_RESAMPLE_ITER_LEN          (BT_AUDIO_SCO_BUFF_SIZE / 2)

static struct APP_RESAMPLE_T *sco_capture_resample;

FRAM_TEXT_LOC  static int bt_sco_capture_resample_iter(uint8_t *buf, uint32_t len)
{
    voicebtpcm_pcm_audio_data_come(buf, len);
    return 0;
}

#endif

//( codec:mic-->btpcm:tx
// codec:mic
FRAM_TEXT_LOC  static uint32_t bt_sco_codec_capture_data(uint8_t *buf, uint32_t len)
{
#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
    if(hal_cmu_get_audio_resample_status())
    {
        app_capture_resample_run(sco_capture_resample, buf, len);
    }
    else
#endif
    {
        voicebtpcm_pcm_audio_data_come(buf, len);
    }

    return len;
}

// btpcm:tx


uint8_t msbc_received=0;


#if  defined(HFP_1_6_ENABLE)

uint8_t msbc_find_tx_sync(uint8_t *buff)
{
    uint8_t i;
    for(i=0;i<60;i++)
    {
        if(buff[i]==0x1 && buff[(i+2)%60] == 0xad)
        {
        //    TRACE("MSBC tx sync find =%d",i);
            return i;
        }
    }
    TRACE("TX No pACKET");
    return 0;
}
#endif


static void tws_stop_sco_callback(uint32_t status, uint32_t param);

uint8_t trigger_test=0;

enum SCO_PLAYER_RESTART_STATUS_E
{
    SCO_PLAYER_RESTART_STATUS_IDLE,
    SCO_PLAYER_RESTART_STATUS_STARTING,
    SCO_PLAYER_RESTART_STATUS_ONPROCESS,
}sco_player_restart_status;

int bt_sco_player_restart_requeset(bool needrestart)
{
    switch (sco_player_restart_status)
    {
        case SCO_PLAYER_RESTART_STATUS_IDLE:
            if (needrestart){
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_STARTING;
            }else{
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_IDLE;
            }
            break;
        case SCO_PLAYER_RESTART_STATUS_STARTING:
        case SCO_PLAYER_RESTART_STATUS_ONPROCESS:
        default:            
            if (needrestart){
                // do nothing
            }else{
                sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_IDLE;
            }
            break;
    }

    return 0;
}

inline bool bt_sco_player_need_restart(void)
{
    bool nRet = false;
    if (sco_player_restart_status == SCO_PLAYER_RESTART_STATUS_STARTING){
        sco_player_restart_status = SCO_PLAYER_RESTART_STATUS_ONPROCESS;
        nRet = true;
    }
    return nRet;
}

FRAM_TEXT_LOC static uint32_t bt_sco_btpcm_playback_data(uint8_t *buf, uint32_t len)
{
#if  defined(HFP_1_6_ENABLE)
    uint8_t offset;
#endif


    get_voicebtpcm_p2m_frame(buf, len);

    if (btapp_hfp_need_mute()){
        memset(buf, 0, len);
    }
#if 0
    TRACE("SCO D:");
    DUMP8("%02x ",(uint8_t *)0xd0212a50,10);
    DUMP8("%02x ",(uint8_t *)0xd0212a50+50,10);
#endif
#if  defined(HFP_1_6_ENABLE)
     if ((bt_sco_get_current_codecid() == HF_SCO_CODEC_MSBC) && msbc_received == 1) {
        if((offset = msbc_find_tx_sync((uint8_t *)0xd0212a50)) !=0)
        {
          //  btdrv_set_bt_pcm_triggler_delay_reset((((BTDIGITAL_REG(0xd0224024) & 0x3f00)>>8) +(60-offset))%64);
           trigger_test = (((BTDIGITAL_REG(0xd0224024) & 0x3f00)>>8) +(60-offset))%64;
           TRACE("TX BUF ERROR trigger_test=%d",trigger_test);
           bt_sco_player_restart_requeset(true);

//            app_tws_set_pcm_triggle();
            
        }
        if (bt_sco_player_need_restart()){
           app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_sco_callback);
        }
    }
#endif

    
    return len;
}
//)

uint8_t header_pos=0xff;
uint8_t header_sn;
uint8_t msbc_find_msbc_header(uint8_t *buff,uint8_t count,uint8_t max_count)
{
    uint8_t i;
    for(i=0;i<60;i++)
    {
        if(buff[i]==0x1 && buff[i+2] == 0xad)
        {
 //           TRACE("msbc rx seq=%x,i=%d",buff[i+1],i);
            header_pos = i;
            header_sn = buff[i+1];
            if(msbc_received == 0)
            {
                msbc_received = 1;
            }
            return i;
        }
    }
//    TRACE("NO SEQ");


    
    if(header_sn == 0x8)
        header_sn = 0x38;
    else if(header_sn ==0x38)
        header_sn = 0xc8;
    else if(header_sn ==0xc8)
        header_sn = 0xf8;
    else if(header_sn ==0xf8)
        header_sn = 0x08;
    if((count == max_count) && (header_pos+2)>=60)
        return 0;
    if(buff[header_pos+2] == 0xad)
    {
        buff[header_pos] = 0x01;
        buff[header_pos+1] = header_sn;
        
    //    DUMP8("%02x ",buff+header_pos,10);
    }
    else
    {
  //      hal_trace_tportset(0);
 //       hal_trace_tportclr(0);        
    }
    return 0;
}






//( btpcm:rx-->codec:spk
// btpcm:rx

#ifdef __TWS_CALL_DUAL_CHANNEL__
uint32_t sco_trigger_time=0;
#endif

FRAM_TEXT_LOC  static uint32_t bt_sco_btpcm_capture_data(uint8_t *buf, uint32_t len)
{
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if(app_tws_get_btpcm_wait_triggle() ==1)
    {
        TRACE("CLEAR PCM WAIT TRIGGLER ct=%x",btdrv_syn_get_curr_ticks());
        app_tws_set_btpcm_wait_triggle(0);
        btdrv_syn_clr_trigger();
        btdrv_syn_trigger_codec_en(0);        
        *(uint32_t *)0xc000645c = 0;
        sco_trigger_time = 0;
        return 0;

    }
#endif

#if defined(HFP_1_6_ENABLE)
    if (bt_sco_get_current_codecid() == HF_SCO_CODEC_MSBC) {
        uint16_t * temp_buf = NULL;
        temp_buf=(uint16_t *)buf;
        len /= 2;
        for(uint32_t i=0; i<len; i++) {
            buf[i]=temp_buf[i]>>8;
        }

//    TRACE("CAP: len=%d",len);
        for(uint8_t i=0;i<len/60;i++)
        {
            msbc_find_msbc_header(buf+i*60,i,len/60-1);    
        }
        
    }
#endif
    store_voicebtpcm_m2p_buffer(buf, len);

    return len;
}

// codec:spk
static int16_t *bt_sco_codec_playback_cache_ping = NULL;
static int16_t *bt_sco_codec_playback_cache_pang = NULL;
extern CQueue voicebtpcm_m2p_queue;

enum BTSTREAM_PP_T{
    PP_PING = 0,
    PP_PANG = 1
};

static enum BTSTREAM_PP_T bt_sco_codec_playback_cache_pp = PP_PING;

#ifdef __TWS_CALL_DUAL_CHANNEL__
extern uint8_t slave_sco_active;
#endif
extern struct BT_DEVICE_T  app_bt_device;


static void tws_restart_voice_callback(uint32_t status, uint32_t param)
{

    uint8_t sco_state;
    uint32_t lock = int_lock();
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
#endif
    {
        sco_state = (app_bt_device.hf_audio_state[0] == HF_AUDIO_CON)?1:0;
    }
    int_unlock(lock);
  TRACE("tws_restart_voice_callback audio state=%d",sco_state);

  
  if(sco_state == 0)
  {
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,0);  
  }
}



static void tws_stop_sco_callback(uint32_t status, uint32_t param)
{
    uint8_t sco_state;
    uint32_t lock = int_lock();
#ifdef __TWS_CALL_DUAL_CHANNEL__
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
#endif
    {
        sco_state = (app_bt_device.hf_audio_state[0] == HF_AUDIO_CON)?1:0;
    }
    int_unlock(lock);

  TRACE("tws_stop_sco_callback audio state=%d",sco_state);

  
  if(sco_state)
  {
//      osDelay(30);
      app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_START,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,1,(uint32_t)tws_restart_voice_callback);
  }
}

#ifdef __TWS_CALL_DUAL_CHANNEL__
#define __TWS_RETRIGGER_VOICE__

void bt_sco_check_pcm(void)
{
#ifdef __TWS_RETRIGGER_VOICE__
    if(sco_trigger_time == 0)
        return;
    uint32_t curr_time = btdrv_syn_get_curr_ticks();
    uint8_t sco_state;
    if(app_tws_mode_is_slave())
    {
        sco_state = slave_sco_active;
    }
    else
    {
        sco_state = (app_bt_device.hf_audio_state[0] == HF_AUDIO_CON)?1:0;
    }
    if(sco_state  && ((curr_time - sco_trigger_time)>0x400))
    {
        sco_trigger_time =0;
        app_audio_manager_sendrequest(APP_BT_STREAM_MANAGER_STOP,BT_STREAM_VOICE,BT_DEVICE_ID_1,MAX_RECORD_NUM,0,(uint32_t)tws_stop_sco_callback);
    }
#endif
}
#endif


extern uint32_t sco_active_bak;

FRAM_TEXT_LOC static uint32_t bt_sco_codec_playback_data(uint8_t *buf, uint32_t len)
{
    int16_t *bt_sco_codec_playback_cache;

#ifdef __TWS_CALL_DUAL_CHANNEL__
    if(app_tws_get_pcm_wait_triggle() ==1)
    {
         TRACE("SET PCM WAIT TRIGGLER,ct=%x",btdrv_syn_get_curr_ticks());
        msbc_received=0;
        if(app_tws_mode_is_slave())
        {
            if(hal_get_chip_metal_id() >=HAL_CHIP_METAL_ID_2)
            {
                *(uint32_t *)0xc000490c = sco_active_bak;       
                //*(uint32_t *)0xc000645c = 0;
            }
        }
        btdrv_syn_clr_trigger();
        app_tws_set_pcm_wait_triggle(0);
        app_tws_set_pcm_triggle();
        app_tws_set_btpcm_wait_triggle(1);
        memset(buf, 0, len);
        sco_trigger_time = btdrv_syn_get_curr_ticks();
        
    }
#endif   


    if (bt_sco_codec_playback_cache_pp == PP_PING){
        bt_sco_codec_playback_cache_pp = PP_PANG;
        bt_sco_codec_playback_cache = bt_sco_codec_playback_cache_ping;
    }else{
        bt_sco_codec_playback_cache_pp = PP_PING;
        bt_sco_codec_playback_cache = bt_sco_codec_playback_cache_pang;
    }

#ifdef BT_XTAL_SYNC
    bt_xtal_sync(BT_XTAL_SYNC_MODE_VOICE);
#endif

#ifndef FPGA
#ifdef __BT_ANC__
    if (bt_sco_codec_playback_cache){
		//TRACE(" %s %d %d %d",__func__,len,bt_sco_samplerate_ratio,len/bt_sco_samplerate_ratio/2/2);

		uint32_t temp_sample_cnt = (len>>2)/bt_sco_samplerate_ratio;
 		short *p16buf = (short*)buf;
		//TRACE(" %s %d %d %d",__func__,len,bt_sco_samplerate_ratio, temp_sample_cnt);
		voicebtpcm_pcm_audio_more_data((uint8_t*)bt_sco_codec_playback_cache, temp_sample_cnt<<1,buf);
		app_ring_merge_more_data((uint8_t*)bt_sco_codec_playback_cache, temp_sample_cnt<<1);
		voicebtpcm_pcm_resample(bt_sco_codec_playback_cache, temp_sample_cnt, (short*)p16buf);
	
	 	for( int i = len/2/2 ; i > 0 ; i--){
			  *(p16buf+(i-1)*2 +1) = *(p16buf+(i-1)*2) = *(p16buf+i-1);
	 	}
    }else{
		TRACE(" %s %x %d %x",__func__,bt_sco_codec_playback_cache_ping, bt_sco_codec_playback_cache_pp,bt_sco_codec_playback_cache_ping);
        memset(buf, 0, len);
        app_ring_merge_more_data(buf, len);
    }
#else
    if (bt_sco_codec_playback_cache){
        voicebtpcm_pcm_audio_more_data((uint8_t *)bt_sco_codec_playback_cache, len/2);
        app_ring_merge_more_data((uint8_t *)bt_sco_codec_playback_cache, len/2);
        app_bt_stream_copy_track_one_to_two_16bits((int16_t *)buf, bt_sco_codec_playback_cache, len/2/2);
    }else{
        memset(buf, 0, len);
        app_ring_merge_more_data(buf, len);
    }
#endif

#else
    voicebtpcm_pcm_audio_more_data(buf, len);
    app_ring_merge_more_data(buf, len);
#endif

    return len;
}
//)

#ifdef SPEECH_SPK_FIR_EQ
#include "fir_process.h"
extern const FIR_CFG_T speech_spk_eq_16k_cfg;
extern const FIR_CFG_T speech_spk_eq_8k_cfg;

void speech_fir_run(short *buf_pcm, int pcm_len)
{    
    fir_run((uint8_t *)buf_pcm, pcm_len);
}
#endif

// #define SPEECH_CODEC_FRAME_MS       (16)
// #define SPEECH_SCO_FRAME_MS         (15)

// #define TIME_TO_FRAME_SIZE(fs, ms)      ((fs) / 1000 * (ms))
// #define TIME_TO_BUF_SIZE(fs, ms)        (TIME_TO_FRAME_SIZE(fs, ms)  * 2 * 2)

// #define APP_BT_CODEC_FRAME_SIZE         TIME_TO_FRAME_SIZE(fs, ms)

int speech_get_frame_size(int fs, int ch, int ms)
{
    return (fs / 1000 * ch * ms);
}

int speech_get_af_buffer_size(int fs, int ch, int ms)
{
    return speech_get_frame_size(fs, ch, ms) * 2 * 2;
}

enum AUD_SAMPRATE_T bt_sco_get_sample_rate(void)
{
    enum AUD_SAMPRATE_T sample_rate;

#if defined(HFP_1_6_ENABLE) && defined(MSBC_16K_SAMPLE_RATE)
    if (bt_sco_codec_is_msbc())
    {
        sample_rate = AUD_SAMPRATE_16000;
    }
    else
#endif
    {
        sample_rate = AUD_SAMPRATE_8000;
    }

    return sample_rate;
}

int bt_sco_player(bool on, enum APP_SYSFREQ_FREQ_T freq,uint32_t trigger_ticks)
{
    struct AF_STREAM_CONFIG_T stream_cfg;
    static bool isRun =  false;
    uint8_t * bt_audio_buff = NULL;
    enum AUD_SAMPRATE_T sample_rate;

    TRACE("bt_sco_player work:%d op:%d freq:%d", isRun, on, freq);
//  osDelay(1);

    if (isRun==on)
        return 0;

    if (on){
        //bt_syncerr set to max(0x0a)
//        BTDIGITAL_REG_SET_FIELD(REG_BTCORE_BASE_ADDR, 0x0f, 0, 0x0f);
//        af_set_priority(osPriorityRealtime);
        af_set_priority(osPriorityHigh);
        app_audio_manager_sco_status_checker();

#if defined( __TWS__) && defined(__TWS_CALL_DUAL_CHANNEL__)
        if(app_tws_mode_is_slave())
        {
            stream_local_volume = app_tws_get_tws_hfp_vol()+2;
        }
        else
        {
            stream_local_volume = btdevice_volume_p->hfp_vol;
        }
#else
         stream_local_volume = btdevice_volume_p->hfp_vol;
#endif

        TRACE("bt_sco_player volume=%d",stream_local_volume);


        if (freq < APP_SYSFREQ_104M)
        {
            freq = APP_SYSFREQ_104M;
        }

#if defined(SPEECH_TX_2MIC_NS3)
        if (freq < APP_SYSFREQ_208M)
        {
            freq = APP_SYSFREQ_208M;
        }
#endif

#if defined(AUDIO_ANC_FB_MC) && defined(__BT_ANC__) && !defined(__AUDIO_RESAMPLE__)
        if (freq < APP_SYSFREQ_208M) {
            freq = APP_SYSFREQ_208M;
        }
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
        if (freq < APP_SYSFREQ_208M) {
            freq = APP_SYSFREQ_208M;
        }
#endif

        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, freq);
        TRACE("bt_sco_player: app_sysfreq_req %d", freq);
        //TRACE("sys freq calc : %d\n", hal_sys_timer_calc_cpu_freq(0));

#ifdef __TWS__
        app_tws_deactive();
#endif
#ifndef FPGA
        app_overlay_select(APP_OVERLAY_HFP);
#ifdef BT_XTAL_SYNC
        bt_init_xtal_sync(BT_XTAL_SYNC_MODE_VOICE);
#endif
#endif

        int aec_frame_len = speech_get_frame_size(bt_sco_get_sample_rate(), 1, SPEECH_CODEC_FRAME_MS);
        speech_tx_aec_set_frame_len(aec_frame_len);

        btdrv_rf_bit_offset_track_enable(true);

        app_audio_mempool_use_mempoolsection_init();
        voicebtpcm_pcm_audio_init();

        memset(&stream_cfg, 0, sizeof(stream_cfg));
#if defined(SPEECH_CAPTURE_TWO_CHANNEL)
        sample_rate = AUD_SAMPRATE_16000;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
#else
        sample_rate = bt_sco_get_sample_rate();  
        stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
#endif

        // codec:mic
        stream_cfg.data_size = speech_get_af_buffer_size(sample_rate, stream_cfg.channel_num, SPEECH_CODEC_FRAME_MS);

#if defined(__AUDIO_RESAMPLE__) && defined(NO_SCO_RESAMPLE)
        // When __AUDIO_RESAMPLE__ is defined,
        // resample is off by default on best1000, and on by default on other platforms
#ifndef CHIP_BEST1000
        hal_cmu_audio_resample_disable();
#endif
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        if (sample_rate == AUD_SAMPRATE_8000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_8463;
        }
        else if (sample_rate == AUD_SAMPRATE_16000)
        {
            stream_cfg.sample_rate = AUD_SAMPRATE_16927;
        }
        sco_capture_resample = app_capture_resample_open(sample_rate, stream_cfg.channel_num,
                            bt_sco_capture_resample_iter, stream_cfg.data_size / 2);
#else
        stream_cfg.sample_rate = sample_rate;
#endif
        stream_cfg.bits = AUD_BITS_16;
        stream_cfg.vol = stream_local_volume;

        // codec:mic
#ifdef FPGA
        stream_cfg.device = AUD_STREAM_USE_EXT_CODEC;
#else
        stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
#endif
        stream_cfg.io_path = AUD_INPUT_PATH_MAINMIC;
        stream_cfg.handler = bt_sco_codec_capture_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
#ifdef __BT_ANC__
       // stream_cfg.data_size = ANC_SCO_MIC_BUF_SZ;          // 15ms per int
       // stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(anc_sco_mic_buf);
		//stream_cfg.sample_rate = AUD_SAMPRATE_96000;

      //  damic_init();
		//init_amic_dc_bt();
       // ds_fir_init();
        us_fir_init();		
#endif	
        bt_sco_mode = 1;

        TRACE("capture sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);	
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &stream_cfg);

        if(trigger_test)
        {
            btdrv_set_bt_pcm_triggler_delay(trigger_test);
            trigger_test = 0;
        }
        else
        {
            btdrv_set_bt_pcm_triggler_delay(55);
        }
        app_tws_set_trigger_time(trigger_ticks);

#ifdef SPEECH_SPK_FIR_EQ
        uint8_t *fir_eq_buf = NULL;
        app_audio_mempool_get_buff(&fir_eq_buf, 2048);
        fir_open(AUD_SAMPRATE_16000, AUD_BITS_16, AUD_CHANNEL_NUM_1, fir_eq_buf, 2048);

#if defined(HFP_1_6_ENABLE) && defined(MSBC_16K_SAMPLE_RATE)
        if (bt_sco_get_current_codecid() == HF_SCO_CODEC_MSBC) {
            fir_set_cfg(&speech_spk_eq_16k_cfg);
        }
        else
#endif
        {
            fir_set_cfg(&speech_spk_eq_8k_cfg);
        }     
#endif

#ifdef __TWS_FORCE_CRC_ERROR__   
        btdrv_set_pcm_data_ignore_crc();
#endif

        // codec:spk
        sample_rate = bt_sco_get_sample_rate();  
        stream_cfg.sample_rate = sample_rate;
        stream_cfg.channel_num = AUD_CHANNEL_NUM_2;
        stream_cfg.data_size = speech_get_af_buffer_size(sample_rate, stream_cfg.channel_num, SPEECH_CODEC_FRAME_MS);
        stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
        stream_cfg.handler = bt_sco_codec_playback_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        bt_sco_codec_playback_cache_pp = PP_PING;
        // /2: channel number; /2: pingpong
        app_audio_mempool_get_buff((uint8_t **)&bt_sco_codec_playback_cache_ping, stream_cfg.data_size / stream_cfg.channel_num /2);    
        app_audio_mempool_get_buff((uint8_t **)&bt_sco_codec_playback_cache_pang, stream_cfg.data_size / stream_cfg.channel_num /2);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);

#ifdef __BT_ANC__
	    stream_cfg.sample_rate = AUD_SAMPRATE_48000;
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(anc_sco_spk_buf);
		bt_sco_samplerate_ratio = AUD_SAMPRATE_48000/sample_rate;
        stream_cfg.data_size = ANC_SCO_SPK_BUF_SZ/(sample_rate/AUD_SAMPRATE_8000);          // 15ms per int
#endif

        TRACE("playback sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);
        af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg);

        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);

#ifdef SPEECH_SIDETONE
        // channel {0: left channel; 1: right channel}
        // reduce gain, step 2dB, {for example, 0x0a: -20dB}
        hal_codec_sidetone_enable(0, 0x0a);
#endif            


#ifdef _SCO_BTPCM_CHANNEL_
        stream_cfg.sample_rate = bt_sco_get_sample_rate();
        stream_cfg.channel_num = AUD_CHANNEL_NUM_1;
        // stream_cfg.data_size = BT_AUDIO_SCO_BUFF_SIZE * stream_cfg.channel_num;
        stream_cfg.data_size = speech_get_af_buffer_size(stream_cfg.sample_rate, stream_cfg.channel_num, SPEECH_SCO_FRAME_MS);
        // btpcm:rx
        stream_cfg.device = AUD_STREAM_USE_BT_PCM;
        stream_cfg.handler = bt_sco_btpcm_capture_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);

        TRACE("sco btpcm sample_rate:%d, data_size:%d",stream_cfg.sample_rate,stream_cfg.data_size);
        af_stream_open(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE, &stream_cfg);

        // btpcm:tx
        stream_cfg.device = AUD_STREAM_USE_BT_PCM;
        stream_cfg.handler = bt_sco_btpcm_playback_data;
        app_audio_mempool_get_buff(&bt_audio_buff, stream_cfg.data_size);
        stream_cfg.data_ptr = BT_AUDIO_CACHE_2_UNCACHE(bt_audio_buff);
        af_stream_open(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK, &stream_cfg);

        hal_cmu_pcm_set_slave_mode();
        af_stream_start(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);
        af_stream_start(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);
#else
        memset(&hf_sendbuff_ctrl, 0, sizeof(hf_sendbuff_ctrl));
#endif

#ifdef FPGA
        app_bt_stream_volumeset(stream_local_volume);
#endif
        TRACE("bt_sco_player on");
    } else {
#ifdef __TWS_CALL_DUAL_CHANNEL__
        sco_trigger_time=0;
        msbc_received = 0;
#endif
    
#ifndef FPGA
        osDelay(10);        //del by wanghaihui for discon two call the af_dma_irq_handler disappear on FPGA
#endif
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);
#ifdef _SCO_BTPCM_CHANNEL_
        af_stream_stop(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);
        af_stream_stop(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);

        af_stream_close(AUD_STREAM_ID_1, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_1, AUD_STREAM_PLAYBACK);
#endif

#ifdef SPEECH_SPK_FIR_EQ
        fir_close();
#endif

        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK);

        osDelay(10);

#ifdef SPEECH_SIDETONE
        hal_codec_sidetone_disable();
#endif           

#if defined(__AUDIO_RESAMPLE__) && defined(SW_SCO_RESAMPLE)
        app_capture_resample_close(sco_capture_resample);
        sco_capture_resample = NULL;
#endif

#if defined(__AUDIO_RESAMPLE__) && defined(NO_SCO_RESAMPLE)
#ifndef CHIP_BEST1000
        // When __AUDIO_RESAMPLE__ is defined,
        // resample is off by default on best1000, and on by default on other platforms
        hal_cmu_audio_resample_enable();
#endif
#endif

#ifndef FPGA
        bt_sco_codec_playback_cache_ping = NULL;
        bt_sco_codec_playback_cache_pang = NULL;
		bt_sco_codec_playback_cache_pp = PP_PING;
#endif
#ifdef __BT_ANC__
   //     damic_deinit();
  //      app_cap_thread_stop();
#endif
        bt_sco_mode = 0;

        voicebtpcm_pcm_audio_deinit();

#ifndef FPGA
#ifdef BT_XTAL_SYNC
//        osDelay(50);
        bt_term_xtal_sync();
        if(app_tws_mode_is_slave())
        {
            btdrv_rf_init_ext();
        }
#endif
#endif
        TRACE("bt_sco_player off");
        app_overlay_unloadall();
        app_sysfreq_req(APP_SYSFREQ_USER_APP_0, APP_SYSFREQ_32K);
        af_set_priority(osPriorityAboveNormal);

        //bt_syncerr set to default(0x07)
 //       BTDIGITAL_REG_SET_FIELD(REG_BTCORE_BASE_ADDR, 0x0f, 0, 0x07);
    }

    isRun=on;
    return 0;
}

APP_BT_STREAM_T gStreamplayer = APP_BT_STREAM_NUM;
int app_bt_stream_open(APP_AUDIO_STATUS* status)
{
    int nRet = -1;
    APP_BT_STREAM_T player = (APP_BT_STREAM_T)status->id;
    enum APP_SYSFREQ_FREQ_T freq = (enum APP_SYSFREQ_FREQ_T)status->freq;

    TRACE("app_bt_stream_open prev:%d cur:%d freq:%d,param=%d", gStreamplayer, player, freq,status->param);

    if (gStreamplayer != APP_BT_STREAM_NUM){
        TRACE("Close prev bt stream before opening");
        nRet = app_bt_stream_close(gStreamplayer);
        if (nRet)
            return -1;
    }

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            nRet = bt_sco_player(true, freq,status->param);
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_START, freq,status->param);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            nRet = app_factorymode_audioloop(true, freq);
            break;
#endif
   #ifdef MEDIA_PLAYER_SUPPORT
        case APP_PLAY_BACK_AUDIO:
            nRet = app_play_audio_onoff(true, status);
            break;
    #endif
#ifdef TWS_RBCODEC_PLAYER
		case APP_BT_STREAM_RBCODEC:
			nRet = bt_sbc_local_player_init(true, status->param);
			break;
#endif
#ifdef TWS_LINEIN_PLAYER
		case APP_BT_STREAM_LINEIN:
			nRet = app_linein_audio_onoff(true, status->param);
			break;
#endif
        default:
            nRet = -1;
            break;
    }

    if (!nRet)
        gStreamplayer = player;

    return nRet;
}

int app_bt_stream_close(APP_BT_STREAM_T player)
{
    int nRet = -1;
    TRACE("app_bt_stream_close prev:%d cur:%d", gStreamplayer, player);
//  osDelay(1);

    if (gStreamplayer != player)
        return -1;

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            nRet = bt_sco_player(false, APP_SYSFREQ_32K,0);
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_STOP, APP_SYSFREQ_32K,0);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            nRet = app_factorymode_audioloop(false, APP_SYSFREQ_32K);
            break;
#endif
#ifdef MEDIA_PLAYER_SUPPORT
       case APP_PLAY_BACK_AUDIO:
            nRet = app_play_audio_onoff(false, NULL);
            break;
#endif
#ifdef TWS_RBCODEC_PLAYER
		case APP_BT_STREAM_RBCODEC:
			nRet = bt_sbc_local_player_init(false, 0);
			break;
#endif
#ifdef TWS_LINEIN_PLAYER
		case APP_BT_STREAM_LINEIN:
			bt_sbc_local_player_init(false, 0);
			nRet = app_linein_audio_onoff(false, 0);
			break;
#endif
        default:
            nRet = -1;
            break;
    }
    if (!nRet)
        gStreamplayer = APP_BT_STREAM_NUM;
    return nRet;
}



int app_bt_stream_setup(APP_BT_STREAM_T player, uint32_t param)
{
    int nRet = -1;
    uint8_t opt;

//    opt = APP_BT_SBC_PLAYER_TRIGGER;
//    opt = param>>28;
//    param &= 0x0fffffff;
    if(param)
    {
        opt = APP_BT_SBC_PLAYER_RESTART;
    }
    else
    {
        opt = APP_BT_SBC_PLAYER_PAUSE;
    }

    switch (player)
    {
#ifdef TWS_RBCODEC_PLAYER    
        case APP_BT_STREAM_RBCODEC:
#endif
#ifdef TWS_LINEIN_PLAYER
        case APP_BT_STREAM_LINEIN:
#endif            
        case APP_BT_STREAM_A2DP_SBC:
            nRet = bt_sbc_player_setup(opt, param);
            break;
        default:
            ASSERT(0, "[%s] %d is invalid player", __func__, player);
            break;
    }

    return nRet;
}

int app_bt_stream_restart(APP_AUDIO_STATUS* status)
{
    int nRet = -1;
    APP_BT_STREAM_T player = (APP_BT_STREAM_T)status->id;
    enum APP_SYSFREQ_FREQ_T freq = (enum APP_SYSFREQ_FREQ_T)status->freq;

    TRACE("app_bt_stream_restart prev:%d cur:%d freq:%d", gStreamplayer, player, freq);

    if (gStreamplayer != player)
        return -1;

    switch (player) {
        case APP_BT_STREAM_HFP_PCM:
        case APP_BT_STREAM_HFP_CVSD:
        case APP_BT_STREAM_HFP_VENDOR:
            break;
        case APP_BT_STREAM_A2DP_SBC:
        case APP_BT_STREAM_A2DP_AAC:
        case APP_BT_STREAM_A2DP_VENDOR:
            nRet = bt_sbc_player(PLAYER_OPER_RESTART, freq,status->param);
            break;
#ifdef __FACTORY_MODE_SUPPORT__
        case APP_FACTORYMODE_AUDIO_LOOP:
            break;
#endif
#ifdef MEDIA_PLAYER_SUPPORT
        case APP_PLAY_BACK_AUDIO:
            break;
#endif
        default:
            nRet = -1;
            break;
    }

    return nRet;
}

//Modified by ATX : parker.wei_20180310  refactor the function
#if 0
void app_bt_stream_volumeup(void)
{
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        TRACE("%s set audio volume", __func__);
        btdevice_volume_p->a2dp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        TRACE("%s set hfp volume", __func__);
        btdevice_volume_p->hfp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_NUM))
    {
        TRACE("%s set idle volume", __func__);
        btdevice_volume_p->a2dp_vol++;
    }

//Modified by ATX : parker.wei_20180305 MAX TONE

    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC)||app_bt_stream_isrun(APP_BT_STREAM_NUM))
	 {   
	 	if (btdevice_volume_p->a2dp_vol >= TGT_VOLUME_LEVEL_15)
	    {
	        btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_15;
#ifdef MEDIA_PLAYER_SUPPORT
	        media_PlayAudio(AUD_ID_BT_VOL_MAX,0);
#endif
	    }
    }
    //Modified by ATX : parker.wei_20180305
    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
		if (btdevice_volume_p->hfp_vol >= TGT_VOLUME_LEVEL_15)
	    {
	        btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_15;
#ifdef MEDIA_PLAYER_SUPPORT
	        media_PlayAudio(AUD_ID_BT_VOL_MAX,0);
#endif
	    }

    }
    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif

    TRACE("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    TRACE("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);
}

void app_bt_stream_volumedown(void)
{
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        TRACE("%s set audio volume", __func__);
        btdevice_volume_p->a2dp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        TRACE("%s set hfp volume", __func__);
        btdevice_volume_p->hfp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_NUM))
    {
        TRACE("%s set idle volume", __func__);
        btdevice_volume_p->a2dp_vol--;
    }
	
//Modified by ATX : parker.wei_20180305 MIN TONE
    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC)||app_bt_stream_isrun(APP_BT_STREAM_NUM))
    {
	    if (btdevice_volume_p->a2dp_vol <= TGT_VOLUME_LEVEL_0)
	    {
	        btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_0;
#ifdef MEDIA_PLAYER_SUPPORT
	        media_PlayAudio(AUD_ID_BT_VOL_MIN,0);
#endif
	    }
    }	
//Modified by ATX : parker.wei_20180305 MIN TONE
    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
	    if (btdevice_volume_p->hfp_vol <= TGT_VOLUME_LEVEL_0)
	    {
	        btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_0;
#ifdef MEDIA_PLAYER_SUPPORT
	        media_PlayAudio(AUD_ID_BT_VOL_MIN,0);
#endif
	    }
    }
    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif

    TRACE("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    TRACE("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);
}

#else

uint8_t app_bt_stream_volumeup(void)
{
    uint8_t vol_type=INVALID_VOL_TYPE;
	if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        btdevice_volume_p->a2dp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
		vol_type=A2DP_VOL_TYPE;
		
		TRACE("%s a2dp", __func__);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        btdevice_volume_p->hfp_vol++;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
		vol_type=HFP_VOL_TYPE;
		
		TRACE("%s hfp", __func__);

    }

    if (app_bt_stream_isrun(APP_BT_STREAM_NUM))
    {
		TRACE("%s idle", __func__);
		vol_type=INVALID_VOL_TYPE;
	
    }

	if (btdevice_volume_p->a2dp_vol >= TGT_VOLUME_LEVEL_15)
	{
		btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_15;
	}
	if (btdevice_volume_p->hfp_vol >= TGT_VOLUME_LEVEL_15)
	{
		btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_15;
	}
	
#ifndef FPGA
    nv_record_touch_cause_flush();
#endif

    TRACE("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    TRACE("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);

  	return  vol_type;	

}


uint8_t app_bt_stream_volumedown(void)
{
    uint8_t vol_type=INVALID_VOL_TYPE;

    if (app_bt_stream_isrun(APP_BT_STREAM_A2DP_SBC))
    {
        btdevice_volume_p->a2dp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->a2dp_vol);
		vol_type=A2DP_VOL_TYPE;
		TRACE("%s a2dp", __func__);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_HFP_PCM))
    {
        btdevice_volume_p->hfp_vol--;
        app_bt_stream_volumeset(btdevice_volume_p->hfp_vol);
		vol_type=HFP_VOL_TYPE;

		TRACE("%s hfp", __func__);
    }

    if (app_bt_stream_isrun(APP_BT_STREAM_NUM))
    {
		vol_type=INVALID_VOL_TYPE;
		TRACE("%s set idle volume", __func__);

		
    }

    if (btdevice_volume_p->a2dp_vol <= TGT_VOLUME_LEVEL_MUTE)
    {
        btdevice_volume_p->a2dp_vol = TGT_VOLUME_LEVEL_MUTE;
    }

    if (btdevice_volume_p->hfp_vol <= TGT_VOLUME_LEVEL_0)
    {
        btdevice_volume_p->hfp_vol = TGT_VOLUME_LEVEL_0;
    }

    #ifndef FPGA
    nv_record_touch_cause_flush();
    #endif

    TRACE("%s a2dp: %d", __func__, btdevice_volume_p->a2dp_vol);
    TRACE("%s hfp: %d", __func__, btdevice_volume_p->hfp_vol);

	return  vol_type;
	
}

#endif


int app_bt_stream_volumeset(int8_t vol)
{
    struct AF_STREAM_CONFIG_T *stream_cfg = NULL;

    if (vol > TGT_VOLUME_LEVEL_15)
        vol = TGT_VOLUME_LEVEL_15;
    if (vol < TGT_VOLUME_LEVEL_MUTE)
        vol = TGT_VOLUME_LEVEL_MUTE;
    TRACE("app_bt_stream_volumeset vol=%d", vol);

    stream_local_volume = vol;
    if (!app_bt_stream_isrun(APP_PLAY_BACK_AUDIO)){
        af_stream_get_cfg(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, &stream_cfg, true);
        stream_cfg->vol = vol;
        af_stream_setup(AUD_STREAM_ID_0, AUD_STREAM_PLAYBACK, stream_cfg);
    }
    return 0;
}

int app_bt_stream_local_volume_get(void)
{
    return stream_local_volume;
}

void app_bt_stream_a2dpvolume_reset(void)
{
     btdevice_volume_p->a2dp_vol = NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT ;
}

void app_bt_stream_hfpvolume_reset(void)
{
     btdevice_volume_p->hfp_vol = NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT;
}

void app_bt_stream_volume_ptr_update(uint8_t *bdAddr)
{
    static struct btdevice_volume stream_volume = {NVRAM_ENV_STREAM_VOLUME_A2DP_VOL_DEFAULT,NVRAM_ENV_STREAM_VOLUME_HFP_VOL_DEFAULT};
    nvrec_btdevicerecord *record = NULL;

#ifndef FPGA
    if (!nv_record_btdevicerecord_find((BT_BD_ADDR *)bdAddr,&record)){
        btdevice_volume_p = &(record->device_vol);
        DUMP8("0x%02x ", bdAddr, BD_ADDR_SIZE);
        TRACE("%s a2dp_vol:%d hfp_vol:%d ptr:0x%x", __func__, btdevice_volume_p->a2dp_vol, btdevice_volume_p->hfp_vol,btdevice_volume_p);
    }else
#endif
    {
        btdevice_volume_p = &stream_volume;
        TRACE("%s default", __func__);
    }
}

struct btdevice_volume * app_bt_stream_volume_get_ptr(void)
{
    return btdevice_volume_p;
}

bool app_bt_stream_isrun(APP_BT_STREAM_T player)
{

    if (gStreamplayer == player)
        return true;
    else
        return false;
}

int app_bt_stream_closeall()
{
    TRACE("app_bt_stream_closeall");

    bt_sco_player(false, APP_SYSFREQ_32K,0);
    bt_sbc_player(PLAYER_OPER_STOP, APP_SYSFREQ_32K,0);

    #ifdef MEDIA_PLAYER_SUPPORT
    app_play_audio_onoff(false, NULL);
    #endif
    gStreamplayer = APP_BT_STREAM_NUM;
    return 0;
}

void app_bt_stream_copy_track_one_to_two_16bits(int16_t *dst_buf, int16_t *src_buf, uint32_t src_len)
{
    uint32_t i = 0;
    for (i = 0; i < src_len; i++) {
        //dst_buf[i*2 + 0] = dst_buf[i*2 + 1] = ((unsigned short)(src_buf[i])<<1);
        dst_buf[i*2 + 0] = dst_buf[i*2 + 1] = src_buf[i];
    }
}

// =======================================================
// APP RESAMPLE
// =======================================================

#include "resample_coef.h"

//static float ratio_step_factor = 1.0002;
//static float ratio_step_factor = 0.99999;    ////1k

static float ratio_step_factor = 0.9998;

static void memzero_int16(void *dst, uint32_t len)
{
    int16_t *dst16 = (int16_t *)dst;
    int16_t *dst16_end = dst16 + len / 2;

    while (dst16 < dst16_end) {
        *dst16++ = 0;
    }
}

static struct APP_RESAMPLE_T *app_resample_open(const struct RESAMPLE_COEF_T *coef, enum AUD_CHANNEL_NUM_T chans,
                                                APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
                                                float ratio_step)
{
    struct APP_RESAMPLE_T *resamp;
    struct RESAMPLE_CFG_T cfg;
    enum RESAMPLE_STATUS_T status;
    uint32_t size, resamp_size;
    uint8_t *buf;

    resamp_size = audio_resample_ex_get_buffer_size(chans, AUD_BITS_16, coef->phase_coef_num);

    size = sizeof(struct APP_RESAMPLE_T);
    size += ALIGN(iter_len, 4);
    size += resamp_size;

    app_audio_mempool_get_buff(&buf, size);

    resamp = (struct APP_RESAMPLE_T *)buf;
    buf += sizeof(*resamp);
    resamp->cb = cb;
    resamp->iter_buf = buf;
    buf += ALIGN(iter_len, 4);
    resamp->iter_len = iter_len;
    resamp->offset = iter_len;

    memset(&cfg, 0, sizeof(cfg));
    cfg.chans = chans;
    cfg.bits = AUD_BITS_16;
    cfg.ratio_step = ratio_step * ratio_step_factor;
    cfg.coef = coef;
    cfg.buf = buf;
    cfg.size = resamp_size;

    status = audio_resample_ex_open(&cfg, (RESAMPLE_ID *)&resamp->id);
    ASSERT(status == RESAMPLE_STATUS_OK, "%s: Failed to open resample: %d", __func__, status);

#ifdef CHIP_BEST1000
    hal_cmu_audio_resample_enable();
#endif

    return resamp;
}

static int app_resample_close(struct APP_RESAMPLE_T *resamp)
{
#ifdef CHIP_BEST1000
    hal_cmu_audio_resample_disable();
#endif

    if (resamp) {
        audio_resample_ex_close((RESAMPLE_ID *)resamp->id);
    }

    return 0;
}

struct APP_RESAMPLE_T *app_playback_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
                                                  APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len)
{
    const struct RESAMPLE_COEF_T *coef = NULL;

    if (sample_rate == AUD_SAMPRATE_8000) {
        coef = &resample_coef_8k_to_8p4k;
    } else if (sample_rate == AUD_SAMPRATE_32000) {
        coef = &resample_coef_32k_to_50p7k;
    } else if (sample_rate == AUD_SAMPRATE_44100) {
        coef = &resample_coef_44p1k_to_50p7k;
    } else if (sample_rate == AUD_SAMPRATE_48000) {
        coef = &resample_coef_48k_to_50p7k;
    } else {
        ASSERT(false, "%s: Bad sample rate: %u", __func__, sample_rate);
    }

    return app_resample_open(coef, chans, cb, iter_len, 0);
}

struct APP_RESAMPLE_T *app_playback_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
                                                      APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
                                                      float ratio_step)
{
    const struct RESAMPLE_COEF_T *coef = &resample_coef_any_up256;

    return app_resample_open(coef, chans, cb, iter_len, ratio_step);
}

int app_playback_resample_close(struct APP_RESAMPLE_T *resamp)
{
    return app_resample_close(resamp);
}

int app_playback_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len)
{
    uint32_t in_size, out_size;
    struct RESAMPLE_IO_BUF_T io;
    enum RESAMPLE_STATUS_T status;
    int ret;

    if (resamp == NULL) {
        goto _err_exit;
    }

    io.out_cyclic_start = NULL;
    io.out_cyclic_end = NULL;

    if (resamp->offset < resamp->iter_len) {
        io.in = resamp->iter_buf + resamp->offset;
        io.in_size = resamp->iter_len - resamp->offset;
        io.out = buf;
        io.out_size = len;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
                status != RESAMPLE_STATUS_DONE) {
            goto _err_exit;
        }

        buf += out_size;
        len -= out_size;
        resamp->offset += in_size;

        ASSERT(len == 0 || resamp->offset == resamp->iter_len,
            "%s: Bad resample offset: len=%d offset=%u iter_len=%u",
            __func__, len, resamp->offset, resamp->iter_len);
    }

    while (len) {
        ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
        if (ret) {
            goto _err_exit;
        }

        io.in = resamp->iter_buf;
        io.in_size = resamp->iter_len;
        io.out = buf;
        io.out_size = len;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
                status != RESAMPLE_STATUS_DONE) {
            goto _err_exit;
        }

        ASSERT(out_size <= len, "%s: Bad resample out_size: out_size=%u len=%d", __func__, out_size, len);
        ASSERT(in_size <= resamp->iter_len, "%s: Bad resample in_size: in_size=%u iter_len=%u", __func__, in_size, resamp->iter_len);

        buf += out_size;
        len -= out_size;
        if (in_size != resamp->iter_len) {
            resamp->offset = in_size;

            ASSERT(len == 0, "%s: Bad resample len: len=%d out_size=%u", __func__, len, out_size);
        }
    }

    return 0;

_err_exit:
    if (resamp) {
        audio_resample_ex_flush((RESAMPLE_ID *)resamp->id);
        resamp->offset = 0;
    }

    memzero_int16(buf, len);

    return 1;
}

struct APP_RESAMPLE_T *app_capture_resample_open(enum AUD_SAMPRATE_T sample_rate, enum AUD_CHANNEL_NUM_T chans,
                                                 APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len)
{
    const struct RESAMPLE_COEF_T *coef = NULL;

    if (sample_rate == AUD_SAMPRATE_8000) {
        coef = &resample_coef_8p4k_to_8k;
    } else if (sample_rate == AUD_SAMPRATE_16000) {
        // Same coef as 8K sample rate
        coef = &resample_coef_8p4k_to_8k;
    } else {
        ASSERT(false, "%s: Bad sample rate: %u", __func__, sample_rate);
    }

    return app_resample_open(coef, chans, cb, iter_len, 0);
}

struct APP_RESAMPLE_T *app_capture_resample_any_open(enum AUD_CHANNEL_NUM_T chans,
                                                     APP_RESAMPLE_ITER_CALLBACK cb, uint32_t iter_len,
                                                     float ratio_step)
{
    const struct RESAMPLE_COEF_T *coef = &resample_coef_any_up256;

    return app_resample_open(coef, chans, cb, iter_len, ratio_step);
}

int app_capture_resample_close(struct APP_RESAMPLE_T *resamp)
{
    return app_resample_close(resamp);
}

int app_capture_resample_run(struct APP_RESAMPLE_T *resamp, uint8_t *buf, uint32_t len)
{
    uint32_t in_size, out_size;
    struct RESAMPLE_IO_BUF_T io;
    enum RESAMPLE_STATUS_T status;
    int ret;

    if (resamp == NULL) {
        goto _err_exit;
    }

    io.out_cyclic_start = NULL;
    io.out_cyclic_end = NULL;

    if (resamp->offset < resamp->iter_len) {
        io.in = buf;
        io.in_size = len;
        io.out = resamp->iter_buf + resamp->offset;
        io.out_size = resamp->iter_len - resamp->offset;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
                status != RESAMPLE_STATUS_DONE) {
            goto _err_exit;
        }

        buf += in_size;
        len -= in_size;
        resamp->offset += out_size;

        ASSERT(len == 0 || resamp->offset == resamp->iter_len,
            "%s: Bad resample offset: len=%d offset=%u iter_len=%u",
            __func__, len, resamp->offset, resamp->iter_len);

        if (resamp->offset == resamp->iter_len) {
            ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
            if (ret) {
                goto _err_exit;
            }
        }
    }

    while (len) {
        io.in = buf;
        io.in_size = len;
        io.out = resamp->iter_buf;
        io.out_size = resamp->iter_len;

        status = audio_resample_ex_run((RESAMPLE_ID *)resamp->id, &io, &in_size, &out_size);
        if (status != RESAMPLE_STATUS_OUT_FULL && status != RESAMPLE_STATUS_IN_EMPTY &&
                status != RESAMPLE_STATUS_DONE) {
            goto _err_exit;
        }

        ASSERT(in_size <= len, "%s: Bad resample in_size: in_size=%u len=%u", __func__, in_size, len);
        ASSERT(out_size <= resamp->iter_len, "%s: Bad resample out_size: out_size=%u iter_len=%u", __func__, out_size, resamp->iter_len);

        buf += in_size;
        len -= in_size;
        if (out_size == resamp->iter_len) {
            ret = resamp->cb(resamp->iter_buf, resamp->iter_len);
            if (ret) {
                goto _err_exit;
            }
        } else {
            resamp->offset = out_size;

            ASSERT(len == 0, "%s: Bad resample len: len=%u in_size=%u", __func__, len, in_size);
        }
    }

    return 0;

_err_exit:
    if (resamp) {
        audio_resample_ex_flush((RESAMPLE_ID *)resamp->id);
        resamp->offset = 0;
    }

    memzero_int16(buf, len);

    return 1;
}

void app_resample_reset(struct APP_RESAMPLE_T *resamp)
{
    audio_resample_ex_flush((RESAMPLE_ID *)resamp->id);
    resamp->offset = resamp->iter_len;
}

void app_resample_tune(struct APP_RESAMPLE_T *resamp, uint32_t rate, int32_t sample, uint32_t ms)
{
    float freq_offset;
    float old_step;
    enum RESAMPLE_STATUS_T ret;

    if (resamp == NULL) {
        return;
    }

    ret = audio_resample_ex_get_ratio_step(resamp->id, &old_step);
    if (ret != RESAMPLE_STATUS_OK) {
        return;
    }

    freq_offset = (float)sample * 1000 / (rate * ms);
    ratio_step_factor *= 1 - freq_offset;

    TRACE("%s: ppb=%d", __FUNCTION__, (int)(freq_offset * 1000 * 1000 * 1000));
    audio_resample_ex_set_ratio_step(resamp->id, old_step * ratio_step_factor);
}

void app_resample_set_tune_factor(struct APP_RESAMPLE_T *resamp, float factor)
{
    float old_step;
    enum RESAMPLE_STATUS_T ret;

    ratio_step_factor = factor;

    if (resamp == NULL) {
        return;
    }

    ret = audio_resample_ex_get_ratio_step(resamp->id, &old_step);
    if (ret != RESAMPLE_STATUS_OK) {
        return;
    }

    audio_resample_ex_set_ratio_step(resamp->id, old_step * ratio_step_factor);
}

float app_resample_get_tune_factor(void)
{
    return ratio_step_factor;
}

