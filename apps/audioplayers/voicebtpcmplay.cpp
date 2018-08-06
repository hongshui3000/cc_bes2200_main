#ifdef MBED
#include "mbed.h"
#endif
// Standard C Included Files
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "rtos.h"
#ifdef MBED
#include "SDFileSystem.h"
#endif
#include "plat_types.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_location.h"
#include "cqueue.h"
#include "app_audio.h"
#include "app_ring_merge.h"
#include "tgt_hardware.h"
#include "audio_dump.h"
#include "iir_resample.h"
#include "iir_process.h"
#include "speech_noise_gate.h"
#include "compexp.h"
#include "ae_math.h"

//#define SPEECH_ALGORITHMGAIN (DB2LIN(0))
//#define SPEAKER_ALGORITHMGAIN (DB2LIN(0))

/**
//---------------------------8k, highpass---------------------------------
// fs: 8000, type: highpass, gain: 0.000000, freq: 100.000000, Q: 0.700000
// {0.945472, -1.890945, 0.945472, -1.888026, 0.893864},

// fs: 8000, type: highpass, gain: 0.000000, freq: 150.000000, Q: 0.700000
// {0.919350, -1.838700, 0.919350, -1.832305, 0.845095},

// fs: 8000, type: highpass, gain: 0.000000, freq: 200.000000, Q: 0.700000
// {0.893955, -1.787909, 0.893955, -1.776835, 0.798984},

// fs: 8000, type: highpass, gain: 0.000000, freq: 250.000000, Q: 0.700000
// {0.869261, -1.738522, 0.869261, -1.721657, 0.755386},

// fs: 8000, type: highpass, gain: 0.000000, freq: 300.000000, Q: 0.700000
// {0.845243, -1.690487, 0.845243, -1.666806, 0.714168},

//---------------------------8k, lowshelf-----------------------------------
// fs: 8000, type: lowshelf, gain: -10.000000, freq: 100.000000, Q: 0.200000
// {0.907860, -1.580010, 0.674895, -1.577041, 0.585724},

// fs: 8000, type: lowshelf, gain: -10.000000, freq: 100.000000, Q: 0.300000
// {0.933486, -1.697194, 0.766657, -1.694006, 0.703332},

// fs: 8000, type: lowshelf, gain: -20.000000, freq: 100.000000, Q: 0.200000
// {0.820366, -1.476138, 0.657214, -1.469649, 0.484068},

// fs: 8000, type: lowshelf, gain: -20.000000, freq: 100.000000, Q: 0.300000
// {0.867794, -1.615010, 0.748794, -1.607911, 0.623687},


//---------------------------16k, highpass---------------------------------
// fs: 16000, type: highpass, gain: 0.000000, freq: 100.000000, Q: 0.700000
// {0.972347, -1.944695, 0.972347, -1.943945, 0.945444},

// fs: 16000, type: highpass, gain: 0.000000, freq: 150.000000, Q: 0.700000
// {0.958814, -1.917628, 0.958814, -1.915964, 0.919293},

// fs: 16000, type: highpass, gain: 0.000000, freq: 200.000000, Q: 0.700000
// {0.945472, -1.890945, 0.945472, -1.888026, 0.893864},

// fs: 16000, type: highpass, gain: 0.000000, freq: 250.000000, Q: 0.700000
// {0.932319, -1.864637, 0.932319, -1.860137, 0.869137},

// fs: 16000, type: highpass, gain: 0.000000, freq: 300.000000, Q: 0.700000
// {0.919350, -1.838700, 0.919350, -1.832305, 0.845095},

//---------------------------16k, lowshelf-----------------------------------
// fs: 16000, type: lowshelf, gain: -10.000000, freq: 100.000000, Q: 0.200000
// {0.948946, -1.766993, 0.818814, -1.766165, 0.768589},

// fs: 16000, type: lowshelf, gain: -10.000000, freq: 100.000000, Q: 0.300000
// {0.964455, -1.837878, 0.874220, -1.837016, 0.839537},

// fs: 16000, type: lowshelf, gain: -20.000000, freq: 100.000000, Q: 0.200000
// {0.897529, -1.700727, 0.803613, -1.698860, 0.703009},

// fs: 16000, type: lowshelf, gain: -20.000000, freq: 100.000000, Q: 0.300000
// {0.927801, -1.789295, 0.861930, -1.787331, 0.791695},
**/

#define MSBC_PCM_PLC_ENABLE

// BT
extern "C" {
#include "eventmgr.h"
#include "me.h"
#include "sec.h"
#include "avdtp.h"
#include "avctp.h"
#include "avrcp.h"
#include "hf.h"
#include "plc_8000.h"
#include "speech_utils.h" 

#if defined(HFP_1_6_ENABLE)
#include "sbc.h"
#endif

#if defined(MSBC_PLC_ENABLE)
#include "sbcplc.h"
#if defined(MSBC_PCM_PLC_ENABLE)
#include "plc_16000.h"
#endif
#endif

#if defined(_CVSD_BYPASS_)
#include "Pcm8k_Cvsd.h"
#endif

#if defined(MSBC_PLC_ENCODER) && !defined(MSBC_PCM_PLC_ENABLE)
#error "When enable MSBC_PLC_ENCODER, must enable MSBC_PCM_PLC_ENABLE!"
#endif

#if defined( SPEECH_TX_AEC ) || defined( SPEECH_TX_NS ) || defined(SPEECH_RX_NS )
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#endif

#if defined( SPEECH_TX_AEC )
static SpeexEchoState *st=NULL;
#endif

#if defined( SPEECH_TX_AEC ) || defined( SPEECH_TX_NS )
static SpeexPreprocessState *den=NULL;
#endif

#if defined( SPEECH_RX_NS )
static SpeexPreprocessState *den_voice = NULL;
#endif

#if defined (SPEECH_RX_PLC)
static  void *speech_lc;
#endif

#if defined( SPEECH_TX_AEC ) || defined( SPEECH_TX_AEC2 ) || defined(SPEECH_TX_AEC2FLOAT)
static short *echo_buf;
static short *e_buf;
#endif

#if defined(SPEECH_TX_AEC2)
#include "bt_sco_chain_echo_suppress.h"
#endif

#if defined(SPEECH_TX_AEC2FLOAT)
#include "echo_canceller.h"
static EcState *ec2f_st = NULL;
#endif

#if defined(SPEECH_TX_NS2) || defined(SPEECH_RX_NS2)
#include "lc_mmse_ns.h"
#endif

#if defined(SPEECH_TX_NS2FLOAT) || defined(SPEECH_RX_NS2FLOAT)
#include "lc_mmse_ns_float.h"
#endif

#if defined(SPEECH_TX_NS2)
LC_MMSE_NS_State *speech_tx_ns2_st = NULL;
#endif

#if defined(SPEECH_RX_NS2)
LC_MMSE_NS_State *speech_rx_ns2_st = NULL;
#endif

#if defined(SPEECH_TX_NS2FLOAT)
LC_MMSE_NS_FLOAT_State *speech_tx_ns2f_st = NULL;
#endif

#if defined(SPEECH_RX_NS2FLOAT)
LC_MMSE_NS_FLOAT_State *speech_rx_ns2f_st = NULL;
#endif

#if defined(SPEECH_TX_NS3) || defined(SPEECH_RX_NS3)
#include "ns3.h"
#endif

#if defined(SPEECH_TX_NS3)
static Ns3State *speech_tx_ns3_st = NULL;
const Ns3Config speech_tx_ns3_cfg = {
    .mode = NS3_SUPPRESSION_HIGH,
};
#endif

#if defined(SPEECH_RX_NS3)
static Ns3State *speech_rx_ns3_st = NULL;
const Ns3Config speech_rx_ns3_cfg = {
    .mode = NS3_SUPPRESSION_HIGH,
};
#endif

#if defined(SPEECH_TX_NOISE_GATE)
static NoisegateState *speech_tx_noisegate_st = NULL;
const NoisegateConfig speech_tx_noisegate_cfg = {
    .data_threshold = 640,
    .data_shift = 1,
    .factor_up = 0.001f,
    .factor_down = 0.5f,
};
#endif

#if defined(SPEECH_TX_COMPEXP)
static CompexpState *speech_tx_compexp_st = NULL;
const CompexpConfig speech_tx_compexp_cfg = {
    .comp_threshold = -10.f,
    .comp_slope = 0.5f,
    .expand_threshold = -21.f,
    .expand_slope = -0.5f,
    .attack_time = 0.01f,
    .release_time = 0.1f,
    .makeup_gain = 6,
    .delay = 128,
};
#endif

#if defined(SPEECH_TX_2MIC_NS)
#include "dual_mic_denoise.h"
extern const DUAL_MIC_DENOISE_CFG_T dual_mic_denoise_cfg;
#endif


#if defined(SPEECH_TX_2MIC_NS2)
#include "coherent_denoise.h"
extern const COHERENT_DENOISE_CFG_T coherent_denoise_cfg;
#endif

#if defined(SPEECH_TX_2MIC_NS3)
#include "far_field_speech_enhancement.h"
#endif

#if defined(SPEECH_TX_AGC) || defined(SPEECH_RX_AGC)
#include "agc.h"
#endif

#if defined(SPEECH_TX_AGC)
static AgcState *speech_tx_agc_st = NULL;
#endif

#if defined(SPEECH_RX_AGC)
static AgcState *speech_rx_agc_st = NULL;
#endif

#if defined(SPEECH_TX_EQ) || defined(SPEECH_RX_EQ)
#include "speech_eq.h"
#endif

#if defined(SPEECH_TX_EQ)
static EqState *speech_tx_eq_st = NULL;
// A Weighting 8k
const EqConfigRaw speech_tx_eq_cfg_8k = {
    .gain = -4.0049f,
    .num = 3,
    .param = {
        {0.1038f, -0.3604f, 1.0000f, 0.0000f, -1.0000f},
        {-0.2644f, -0.6014f, 1.0000f, 0.0000f, -1.0000f},
        {-1.9679f, 0.9682f, 1.0000f, -2.0000f, 1.0000f},
    },
};
// A Weighting 16k
const EqConfigRaw speech_tx_eq_cfg_16k = {
    .gain = -5.4899f,
    .num = 3,
    .param = {
        {0.8216f, 0.1687f, 1.0000f, 2.0000f, 1.0000f},
        {-1.7055f, 0.7160f, 1.0000f, -2.0000f, 1.0000f},
        {-1.9839f, 0.9840f, 1.0000f, -2.0000f, 1.0000f},
    },
};
#endif

#if defined(SPEECH_RX_EQ)
static EqState *speech_rx_eq_st = NULL;
const EqConfig speech_rx_eq_cfg = {
    .gain = 1.f,
    .num = 1,
    .param = {
        {IIR_BIQUARD_HPF, 60, 0, 0.707f},
    },
};
#endif

#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
static int16_t *upsample_buf_for_msbc = NULL;
static int16_t *downsample_buf_for_msbc = NULL;
static IirResampleState *upsample_st = NULL;
static IirResampleState *downsample_st = NULL;
#endif

}

#define MED_MEM_POOL_SIZE       (1024*70)

#if MED_MEM_POOL_SIZE > 0
#include "med_memory.h"
static __attribute__((unused)) uint8_t *g_medMemPool = NULL;
// static uint8_t g_medMemPool[MED_MEM_POOL_SIZE];
#endif

#define VOICEBTPCM_TRACE(s,...)
//TRACE(s, ##__VA_ARGS__)

/* voicebtpcm_pcm queue */
// fs: 16000(msbc: 1 frame; cvsd: 2 frames)
#define VOICEBTPCM_PCM_FRAME_LENGTH (SPEECH_FRAME_MS_TO_LEN(16000, SPEECH_CODEC_FRAME_MS))
#define VOICEBTPCM_PCM_TEMP_BUFFER_SIZE (VOICEBTPCM_PCM_FRAME_LENGTH*2)
#define VOICEBTPCM_PCM_QUEUE_SIZE (VOICEBTPCM_PCM_TEMP_BUFFER_SIZE*4)

// #endif
CQueue voicebtpcm_p2m_queue;
CQueue voicebtpcm_m2p_queue;

uint8_t *rx_decode_buffer = NULL;
CQueue voicebtpcm_m2p_pcm_queue;

static enum APP_AUDIO_CACHE_T voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_QTY;
static enum APP_AUDIO_CACHE_T voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_QTY;

extern bool bt_sco_codec_is_msbc(void);
#define ENCODE_TEMP_PCM_LEN (120)

#if defined(HFP_1_6_ENABLE)
#define MSBC_FRAME_SIZE (60)
static uint8_t *msbc_buf_before_decode;
static uint8_t *msbc_buf_before_plc;
static char need_init_decoder = true;
static SbcDecoder msbc_decoder;
#if FPGA==1
#define CFG_HW_AUD_EQ_NUM_BANDS (8)
const int8_t cfg_hw_aud_eq_band_settings[CFG_HW_AUD_EQ_NUM_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif
static float msbc_eq_band_gain[CFG_HW_AUD_EQ_NUM_BANDS]= {0,0,0,0,0,0,0,0};

#define MSBC_ENCODE_PCM_LEN (240)

unsigned char *temp_msbc_buf;
unsigned char *temp_msbc_buf1;
static char need_init_encoder = true;
#if defined(MSBC_PLC_ENABLE)
PLC_State msbc_plc_state;
#endif

#if defined(MSBC_PLC_ENCODER)
#define MSBC_PLC_ENCODER_SIZE 57
#define MSBC_PLC_ENCODER_BUFFER_SIZE 193
#define MSBC_PLC_ENCODER_BUFFER_OFFSET 73

static SbcEncoder *msbc_plc_encoder;
static SbcPcmData *msbc_plc_encoder_data;
unsigned char *msbc_plc_encoder_buffer;
short *msbc_plc_encoder_inbuffer;
short *msbc_plc_output_pcmbuffer;
#endif

static bool msbc_input_valid = false;
static uint32_t msbc_input_offset = 0;
static SbcEncoder *msbc_encoder;
static SbcPcmData msbc_encoder_pcmdata;
static unsigned char msbc_counter = 0x08;
#endif

int decode_msbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len);
int decode_voicebtpcm_m2p_frame(unsigned char *pcm_buffer, unsigned int pcm_len);

//playback flow
//bt-->store_voicebtpcm_m2p_buffer-->decode_voicebtpcm_m2p_frame-->audioflinger playback-->speaker
//used by playback, store data from bt to memory

inline int sco_parse_synchronization_header(uint8_t *buf, uint8_t *sn);

int store_voicebtpcm_m2p_buffer(unsigned char *buf, unsigned int len)
{
    int size;
    unsigned int avail_size;

#if defined(HFP_1_6_ENABLE)
    static uint32_t error_cnt = 0;
    if (bt_sco_codec_is_msbc()){
        uint8_t msbc_raw_sn = 0xff;        
        uint8_t frame_num = 0;
        if (!msbc_input_valid){    
            unsigned int i;
            for(i=0; i<len; i++){
                if(!sco_parse_synchronization_header(&buf[i], &msbc_raw_sn) &&
                    0 == msbc_raw_sn){
                    TRACE("!!!!!!!!!!!input msbc find sync sn:%d offset:%d", msbc_raw_sn, i); 
                    msbc_input_valid = true;
                    msbc_input_offset = i;
                    error_cnt = 0;
                    break;
                }            
            }

            if (msbc_input_valid){        
                TRACE("!!!!!!!!!!!!!!!!!!!!!! input:%08x len:%d", buf+i, len-i);        
                LOCK_APP_AUDIO_QUEUE();
                APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf+i, len-i);
                UNLOCK_APP_AUDIO_QUEUE();
            }
            return len;
        }else{
            sco_parse_synchronization_header(&buf[msbc_input_offset], &msbc_raw_sn);
            frame_num = len/60;
            if (frame_num%4 == 0){
                if (msbc_raw_sn == 0){
                    error_cnt = 0;
                }else{
                    TRACE("!!!!!!!!!!!!!!!!!!!!!! inputerror sn:%d cnt:%d", msbc_raw_sn, error_cnt);
                    DUMP8("%02x ", &buf[msbc_input_offset], 6);
                    error_cnt++;
                }
            }else if (frame_num == 2){
                if ((msbc_raw_sn == 0) || 
                    (msbc_raw_sn == 2)){
                    error_cnt = 0;
                }else{
                    TRACE("!!!!!!!!!!!!!!!!!!!!!! inputerror sn:%d cnt:%d", msbc_raw_sn, error_cnt);
                    DUMP8("%02x ", &buf[msbc_input_offset], 6);
                    error_cnt++;
                }
            }else{
                ASSERT(0, "%s err len", __func__, len);
            }
            if (error_cnt>10){
                bt_sco_player_restart_requeset(true);
            }
        }
    }
#endif
    LOCK_APP_AUDIO_QUEUE();
    avail_size = APP_AUDIO_AvailableOfCQueue(&voicebtpcm_m2p_queue);
    if (len <= avail_size)
    {
        APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf, len);
    }
    else
    {
        VOICEBTPCM_TRACE( "spk buff overflow %d/%d", len, avail_size);
        if (bt_sco_codec_is_msbc())
        {
            TRACE( "drop frame");
        }
        else
        {
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len - avail_size);
            APP_AUDIO_EnCQueue(&voicebtpcm_m2p_queue, buf, len);
        }
    }
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_m2p_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    uint8_t   decode_index = 0;
    uint32_t l = 0;

    // Just support Frame size = 240 bytes
    uint32_t len_tmp = 240;
    uint8_t *buf_tmp = rx_decode_buffer;

    while(((avail_size = APP_AUDIO_AvailableOfCQueue(&voicebtpcm_m2p_pcm_queue)) >= len_tmp)
#if defined(HFP_1_6_ENABLE)        
        && (APP_AUDIO_LengthOfCQueue(&voicebtpcm_m2p_queue) >= MSBC_FRAME_SIZE)
#endif        
        )
    {
        if (bt_sco_codec_is_msbc())
        {
#if defined(HFP_1_6_ENABLE)
            l = decode_msbc_frame(buf_tmp, len_tmp);
#endif
        }
        else
        {
#if defined(_CVSD_BYPASS_)
            l = decode_cvsd_frame(buf_tmp, len_tmp);
#else
            l = decode_voicebtpcm_m2p_frame(buf_tmp, len_tmp);
#endif
        }
        if (l != len_tmp)
        {
            app_audio_memset_16bit((short *)buf_tmp, 0, len_tmp/2);
            voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_CACHEING;
            break;
        }

        LOCK_APP_AUDIO_QUEUE();
        APP_AUDIO_EnCQueue(&voicebtpcm_m2p_pcm_queue, buf_tmp, len_tmp);
        UNLOCK_APP_AUDIO_QUEUE();
        if(decode_index ++ > 3)
            break;
    }

    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_m2p_queue);

    if (size > VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
    {
        voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_OK;
    }

    VOICEBTPCM_TRACE("m2p :%d/%d", len, size);

    return 0;
}

//used by playback, decode data from memory to speaker
int decode_voicebtpcm_m2p_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int r = 0, got_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    LOCK_APP_AUDIO_QUEUE();
    r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, pcm_len - got_len, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();

    if (r)
    {
        VOICEBTPCM_TRACE( "spk buff underflow");
    }

    if(r == CQ_OK)
    {
        if (len1)
        {
            app_audio_memcpy_16bit((short *)(pcm_buffer), (short *)e1, len1/2);
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len1);
            UNLOCK_APP_AUDIO_QUEUE();
            got_len += len1;
        }
        if (len2)
        {
            app_audio_memcpy_16bit((short *)(pcm_buffer + len1), (short *)e2, len2/2);
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, len2);
            UNLOCK_APP_AUDIO_QUEUE();
            got_len += len2;
        }
    }

    return got_len;
}

#if defined(_CVSD_BYPASS_)
#define VOICECVSD_TEMP_BUFFER_SIZE 120
#define VOICECVSD_ENC_SIZE 60
static short cvsd_decode_buff[VOICECVSD_TEMP_BUFFER_SIZE*2];

int decode_cvsd_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    uint32_t r = 0, decode_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    while (decode_len < pcm_len)
    {
        LOCK_APP_AUDIO_QUEUE();
        len1 = len2 = 0;
        e1 = e2 = 0;
        r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, VOICECVSD_TEMP_BUFFER_SIZE, &e1, &len1, &e2, &len2);
        UNLOCK_APP_AUDIO_QUEUE();

        if (r == CQ_ERR)
        {
            memset(pcm_buffer, 0, pcm_len);
            TRACE( "cvsd spk buff underflow");
            return 0;
        }

        if (len1 != 0)
        {
            CvsdToPcm8k(e1, (short *)(cvsd_decode_buff), len1, 0);

            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_m2p_queue, 0, len1);
            UNLOCK_APP_AUDIO_QUEUE();

            decode_len += len1*2;
        }

        if (len2 != 0)
        {
            CvsdToPcm8k(e2, (short *)(cvsd_decode_buff), len2, 0);

            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_m2p_queue, 0, len2);
            UNLOCK_APP_AUDIO_QUEUE();

            decode_len += len2*2;
        }
    }

    memcpy(pcm_buffer, cvsd_decode_buff, decode_len);

    return decode_len;
}

int encode_cvsd_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    uint32_t r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    uint32_t processed_len = 0;
    uint32_t remain_len = 0, enc_len = 0;

    while (processed_len < pcm_len)
    {
        remain_len = pcm_len-processed_len;

        if (remain_len>(VOICECVSD_ENC_SIZE*2))
        {
            enc_len = VOICECVSD_ENC_SIZE*2;
        }
        else
        {
            enc_len = remain_len*2;
        }

        LOCK_APP_AUDIO_QUEUE();
        len1 = len2 = 0;
        e1 = e2 = 0;
        r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, enc_len, &e1, &len1, &e2, &len2);
        UNLOCK_APP_AUDIO_QUEUE();

        if (r == CQ_ERR)
        {
            memset(pcm_buffer, 0, pcm_len);
            TRACE( "cvsd spk buff underflow");
            return 0;
        }

        if (e1)
        {
            Pcm8kToCvsd((short *)e1, (unsigned char *)(pcm_buffer + processed_len), len1/2);
            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_p2m_queue, NULL, len1);
            UNLOCK_APP_AUDIO_QUEUE();
            processed_len += len1/2;
        }
        if (e2)
        {
            Pcm8kToCvsd((short *)e2, (unsigned char *)(pcm_buffer + processed_len), len2/2);
            LOCK_APP_AUDIO_QUEUE();
            DeCQueue(&voicebtpcm_p2m_queue, NULL, len2);
            UNLOCK_APP_AUDIO_QUEUE();
            processed_len += len2/2;
        }
    }

#if 0
    for (int cc = 0; cc < 32; ++cc)
    {
        TRACE("%x-", e1[cc]);
    }
#endif

    TRACE("%s: processed_len %d, pcm_len %d", __func__, processed_len, pcm_len);

    return processed_len;
}
#endif

#if defined(HFP_1_6_ENABLE)
#define MSBC_SYNC_HACKER 1
inline int sco_parse_synchronization_header(uint8_t *buf, uint8_t *sn)
{
    uint8_t sn1, sn2;
#if defined(MSBC_SYNC_HACKER)
    if (((buf[0] != 0x01) && (buf[0] != 0x00))||
        ((buf[1]&0x0f) != 0x08) ||
        (buf[2] != 0xad)){
        return -1;
    }
#else
    if ((buf[0] != 0x01) ||
        ((buf[1]&0x0f) != 0x08) ||
        (buf[2] != 0xad)){
        return -1;
    }
#endif
    
    sn1 = (buf[1]&0x30)>>4;
    sn2 = (buf[1]&0xc0)>>6;
    if ((sn1 != 0) && (sn1 != 0x3)){
        return -2;
    }    
    if ((sn2 != 0) && (sn2 != 0x3)){
        return -3;
    }

    *sn = (sn1&0x01)|(sn2&0x02);

    return 0;
}

#if 1
int decode_msbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int ttt = 0;
    //int t = 0;
#if defined(MSBC_PLC_ENABLE)
    unsigned int sync_offset = 0;
    uint8_t plc_type = 0;
    uint8_t msbc_raw_sn = 0xff;    
    static uint8_t msbc_raw_sn_pre;    
    static bool msbc_find_first_sync = 0;
#endif
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;
    static SbcPcmData pcm_data;
    static unsigned int msbc_next_frame_size;
    XaStatus ret = XA_STATUS_SUCCESS;
    unsigned short byte_decode = 0;

    unsigned int pcm_offset = 0;
    unsigned int pcm_processed = 0;

#ifdef SPEECH_DATA_DUMP
    short *pcm_buf = NULL;
#endif

#if defined(MSBC_PLC_ENABLE)
    pcm_data.data = (unsigned char*)msbc_buf_before_plc;
#else
    pcm_data.data = (unsigned char*)pcm_buffer;
#endif

    if(need_init_decoder)
    {
        TRACE("init msbc decoder\n");
        pcm_data.data = (unsigned char*)(pcm_buffer + pcm_offset);
        pcm_data.dataLen = 0;
#ifdef __SBC_FUNC_IN_ROM__
        SBC_ROM_FUNC.SBC_InitDecoder(&msbc_decoder);
#else
        SBC_InitDecoder(&msbc_decoder);
#endif

        msbc_decoder.streamInfo.mSbcFlag = 1;
        msbc_decoder.streamInfo.bitPool = 26;
        msbc_decoder.streamInfo.sampleFreq = SBC_CHNL_SAMPLE_FREQ_16;
        msbc_decoder.streamInfo.channelMode = SBC_CHNL_MODE_MONO;
        msbc_decoder.streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
        /* Number of blocks used to encode the stream (4, 8, 12, or 16) */
        msbc_decoder.streamInfo.numBlocks = MSBC_BLOCKS;
        /* The number of subbands in the stream (4 or 8) */
        msbc_decoder.streamInfo.numSubBands = 8;
        msbc_decoder.streamInfo.numChannels = 1;
#if defined(MSBC_PLC_ENABLE)
        InitPLC(&msbc_plc_state);
        msbc_raw_sn_pre = 0xff;
        msbc_find_first_sync = true;
#endif
#if defined(MSBC_PLC_ENCODER)
       SBC_InitEncoder(msbc_plc_encoder);
       msbc_plc_encoder->streamInfo.mSbcFlag = 1;
       msbc_plc_encoder->streamInfo.numChannels = 1;
       msbc_plc_encoder->streamInfo.channelMode = SBC_CHNL_MODE_MONO;

       msbc_plc_encoder->streamInfo.bitPool   = 26;
       msbc_plc_encoder->streamInfo.sampleFreq  = SBC_CHNL_SAMPLE_FREQ_16;
       msbc_plc_encoder->streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
       msbc_plc_encoder->streamInfo.numBlocks     = MSBC_BLOCKS;
       msbc_plc_encoder->streamInfo.numSubBands = 8;
#endif
        need_init_decoder = false;
    }

get_again:
    LOCK_APP_AUDIO_QUEUE();
    len1 = len2 = 0;
    e1 = e2 = 0;
    r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, MSBC_FRAME_SIZE, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();

    if (r == CQ_ERR)
    {
        pcm_processed = pcm_len;
        memset(pcm_buffer, 0, pcm_len);
        TRACE( "msbc spk buff underflow");
        goto exit;
    }

    if (!len1)
    {
        TRACE("len1 underflow %d/%d\n", len1, len2);
        goto get_again;
    }

    if (len1 > 0 && e1)
    {
        memcpy(msbc_buf_before_decode, e1, len1);
    }
    if (len2 > 0 && e2)
    {
        memcpy(msbc_buf_before_decode + len1, e2, len2);
    }


    if (msbc_find_first_sync){

        for(uint8_t i=0;i<MSBC_FRAME_SIZE-3;i++){
            if(!sco_parse_synchronization_header(&msbc_buf_before_decode[i], &msbc_raw_sn)){
                TRACE("[PLC]  msbc find sync sn:%d offset:%d", msbc_raw_sn, i); 
                msbc_find_first_sync = false;
                goto start_decoder;
            }                
        }
        memset(pcm_buffer,0,pcm_len);
        pcm_processed = pcm_len;
        goto exit;
    }    
start_decoder:

    if (sco_parse_synchronization_header(msbc_buf_before_decode, &msbc_raw_sn)){
        plc_type = 2;
    }else{
        plc_type = 1;
    }

    //TRACE("sn %d %d", msbc_raw_sn_pre, msbc_raw_sn);

    if (msbc_raw_sn_pre == 0xff && 
        msbc_raw_sn != 0xff){
        // do nothing            
        msbc_raw_sn_pre = msbc_raw_sn;
    }else{
        if (msbc_raw_sn == 0xff){
#if defined(MSBC_SYNC_HACKER)
            TRACE("[PLC] sbchd err.");   
#else
            uint8_t zero_cnt = 0;    
            for (zero_cnt=0; zero_cnt<MSBC_FRAME_SIZE; zero_cnt++){
                if (msbc_buf_before_decode[zero_cnt] != 0)
                    break;
            } 
            TRACE("[PLC] sbchd zero:%d", zero_cnt);   
            DUMP8("%02x ", msbc_buf_before_decode, 6);
#endif
            msbc_raw_sn_pre = 0xff; //(msbc_raw_sn_pre+1)%4;                
            plc_type = 2;
        }else if (((msbc_raw_sn_pre+1)%4) == msbc_raw_sn){
            // do nothing                     
            msbc_raw_sn_pre = msbc_raw_sn;
        }else{
            TRACE("[PLC] seq err:%d/%d", msbc_raw_sn, msbc_raw_sn_pre);

            //skip seq err
            msbc_raw_sn_pre = 0xff;
            plc_type = 0;
        }
    }


#if defined(MSBC_PLC_ENABLE)
    if (plc_type == 1)
    {
#ifdef __SBC_FUNC_IN_ROM__
        ret = SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, MSBC_FRAME_SIZE, &byte_decode,
                                            &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
        ret = SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, MSBC_FRAME_SIZE, &byte_decode,
                               &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif
        ttt = hal_sys_timer_get();
#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_lc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#else
        PLC_good_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
#ifdef SPEECH_DATA_DUMP
        pcm_buf =  (short *)pcm_buffer;
        for(uint32_t i=0; i< pcm_len/2; i++)
        {
            dump_buf[2*i] = pcm_buf[i];
        }
#endif
    }
    else if (plc_type == 2)
    {
#if defined(MSBC_PCM_PLC_ENABLE)
        ttt = hal_sys_timer_get();
#if defined(MSBC_PLC_ENCODER)
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_lc, (short *)pcm_buffer, msbc_plc_encoder_inbuffer, pcm_len/2);
#else
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_lc, (short *)pcm_buffer, NULL, pcm_len/2);
#endif
#else
        PLC_bad_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
#if defined(MSBC_PLC_ENCODER)
        {      
            uint16_t bytes_encoded = 0, buf_len = MSBC_ENCODE_PCM_LEN;
            msbc_plc_encoder_data->data = (uint8_t *)(msbc_plc_encoder_inbuffer+MSBC_PLC_ENCODER_BUFFER_OFFSET);
            msbc_plc_encoder_data->dataLen = pcm_len;
            msbc_plc_encoder_data->numChannels = 1;
            msbc_plc_encoder_data->sampleFreq = SBC_CHNL_SAMPLE_FREQ_16;
            SBC_EncodeFrames(msbc_plc_encoder, msbc_plc_encoder_data, &bytes_encoded, msbc_plc_encoder_buffer, &buf_len, 0xFFFF);
#ifdef __SBC_FUNC_IN_ROM__
            SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, msbc_plc_encoder_buffer, MSBC_FRAME_SIZE-3, &byte_decode,
                                             &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
            SBC_DecodeFrames(&msbc_decoder, msbc_plc_encoder_buffer, MSBC_FRAME_SIZE-3, &byte_decode,
                                &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif
        }
#endif
        TRACE("[PLC] b t:%d ret:%d\n", (hal_sys_timer_get()-ttt), ret);
#ifdef SPEECH_DATA_DUMP
        for(uint32_t i=0; i< pcm_len/2; i++)
        {
            dump_buf[2*i] = -32767;
        }
#endif
    }
    else
#endif
    {
#ifdef __SBC_FUNC_IN_ROM__
        ret = SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, MSBC_FRAME_SIZE, &byte_decode,
                                            &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
        ret = SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, MSBC_FRAME_SIZE, &byte_decode,
                               &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif  
#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_lc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#endif
#ifdef SPEECH_DATA_DUMP
        for(uint32_t i=0; i< pcm_len/2; i++)
        {
            dump_buf[2*i] = 32767;
        }
#endif
    }

    // TRACE("[%s] pcm_len/2 = %d", __func__, pcm_len/2);
#ifdef SPEECH_DATA_DUMP
    pcm_buf =  (short *)pcm_buffer;
    for(uint32_t i=0; i< pcm_len/2; i++)
    {
        dump_buf[2*i+1] = pcm_buf[i];
    }
#endif

#ifdef SPEECH_DATA_DUMP
    hal_product_test_trace_output((const unsigned char *)dump_buf, pcm_len*2);
#endif

    if(ret == XA_STATUS_FAILED)
    {
#if defined(MSBC_PCM_PLC_ENABLE)
        ttt = hal_sys_timer_get();
#if defined(MSBC_PLC_ENCODER)
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_lc, (short *)pcm_buffer, msbc_plc_encoder_inbuffer, pcm_len/2);
#else
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_lc, (short *)pcm_buffer, NULL, pcm_len/2);
#endif
#else
        PLC_bad_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
#if defined(MSBC_PLC_ENCODER)
        {      
            uint16_t bytes_encoded = 0, buf_len = MSBC_ENCODE_PCM_LEN;
            msbc_plc_encoder_data->data = (uint8_t *)(msbc_plc_encoder_inbuffer+MSBC_PLC_ENCODER_BUFFER_OFFSET);
            msbc_plc_encoder_data->dataLen = pcm_len;
            msbc_plc_encoder_data->numChannels = 1;
            msbc_plc_encoder_data->sampleFreq = SBC_CHNL_SAMPLE_FREQ_16;
            SBC_EncodeFrames(msbc_plc_encoder, msbc_plc_encoder_data, &bytes_encoded, msbc_plc_encoder_buffer, &buf_len, 0xFFFF);
#ifdef __SBC_FUNC_IN_ROM__
            SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, msbc_plc_encoder_buffer, MSBC_FRAME_SIZE-3, &byte_decode,
                                             &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
            SBC_DecodeFrames(&msbc_decoder, msbc_plc_encoder_buffer, MSBC_FRAME_SIZE-3, &byte_decode,
                                &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif
        }
#endif
        plc_type = 2;
        TRACE("[PLC]sbcerr b t:%d ret:%d\n", (hal_sys_timer_get()-ttt), ret);
    }


    if (plc_type == 2)
    {
#if defined(MSBC_PLC_ENCODER)
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;
#else
        pcm_processed = pcm_len;
#endif
    }
    else if(ret == XA_STATUS_SUCCESS)
    {
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;
    }
    else
    {
        TRACE( "msbc decode error");
    }

exit:
    if (pcm_processed != pcm_len)
    {
        TRACE( "media_msbc_decoder error~~~ %d/%d/%d %d/%d\n ", pcm_processed, pcm_len, byte_decode, ret, plc_type);
    }

    LOCK_APP_AUDIO_QUEUE();
    APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, MSBC_FRAME_SIZE);
    UNLOCK_APP_AUDIO_QUEUE();

    return pcm_processed;
}

#else

static uint16_t msbc_frame_size = MSBC_FRAME_SIZE;
static char msbc_need_check_sync_header = 0;

int decode_msbc_frame(unsigned char *pcm_buffer, unsigned int pcm_len)
{
    int ttt = 0;
    //int t = 0;
    uint8_t underflow = 0;
#if defined(MSBC_PLC_ENABLE)
    unsigned int sync_offset = 0;
    uint8_t plc_type = 0;
    uint8_t msbc_raw_sn = 0xff;    
    static uint8_t msbc_raw_sn_pre;    
    static bool msbc_find_first_sync = 0;
#endif
    int r = 0;
    unsigned char *e1 = NULL, *e2 = NULL, *msbc_buff = NULL;
    unsigned int len1 = 0, len2 = 0;
    static SbcPcmData pcm_data;
    static unsigned int msbc_next_frame_size;
    XaStatus ret = XA_STATUS_SUCCESS;
    unsigned short byte_decode = 0;

    unsigned int pcm_offset = 0;
    unsigned int pcm_processed = 0;

#if defined(MSBC_PLC_ENABLE)
    pcm_data.data = (unsigned char*)msbc_buf_before_plc;
#else
    pcm_data.data = (unsigned char*)pcm_buffer;
#endif
    if (!msbc_next_frame_size)
    {
        msbc_next_frame_size = MSBC_FRAME_SIZE;
    }

//reinit:
    if(need_init_decoder)
    {
        TRACE("init msbc decoder\n");
        pcm_data.data = (unsigned char*)(pcm_buffer + pcm_offset);
        pcm_data.dataLen = 0;
#ifdef __SBC_FUNC_IN_ROM__
        SBC_ROM_FUNC.SBC_InitDecoder(&msbc_decoder);
#else
        SBC_InitDecoder(&msbc_decoder);
#endif

        msbc_decoder.streamInfo.mSbcFlag = 1;
        msbc_decoder.streamInfo.bitPool = 26;
        msbc_decoder.streamInfo.sampleFreq = SBC_CHNL_SAMPLE_FREQ_16;
        msbc_decoder.streamInfo.channelMode = SBC_CHNL_MODE_MONO;
        msbc_decoder.streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
        /* Number of blocks used to encode the stream (4, 8, 12, or 16) */
        msbc_decoder.streamInfo.numBlocks = MSBC_BLOCKS;
        /* The number of subbands in the stream (4 or 8) */
        msbc_decoder.streamInfo.numSubBands = 8;
        msbc_decoder.streamInfo.numChannels = 1;
#if defined(MSBC_PLC_ENABLE)
        InitPLC(&msbc_plc_state);
        msbc_need_check_sync_header = 0;        
        msbc_raw_sn_pre = 0xff;
        msbc_find_first_sync = true;
#endif
        need_init_decoder = false;
    }

    msbc_buff = msbc_buf_before_decode;

get_again:
    LOCK_APP_AUDIO_QUEUE();
    len1 = len2 = 0;
    e1 = e2 = 0;
    r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_queue, msbc_next_frame_size, &e1, &len1, &e2, &len2);
    UNLOCK_APP_AUDIO_QUEUE();

    if (r == CQ_ERR)
    {
        pcm_processed = pcm_len;
        memset(pcm_buffer, 0, pcm_len);
        TRACE( "msbc spk buff underflow");
        goto exit;
    }

    if (!len1)
    {
        TRACE("len1 underflow %d/%d\n", len1, len2);
        goto get_again;
    }

    if (len1 > 0 && e1)
    {
        memcpy(msbc_buff, e1, len1);
    }
    if (len2 > 0 && e2)
    {
        memcpy(msbc_buff + len1, e2, len2);
    }


    if (msbc_find_first_sync){

        for(uint8_t i=0;i<MSBC_FRAME_SIZE-3;i++){
            if(!sco_parse_synchronization_header(&msbc_buff[i], &msbc_raw_sn)){
                TRACE("!!!!!!!!!!! msbc find sync sn:%d offset:%d", msbc_raw_sn, i); 
                msbc_find_first_sync = false;
                msbc_need_check_sync_header = true;
                goto start_decoder;
            }                
        }
        LOCK_APP_AUDIO_QUEUE();
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, MSBC_FRAME_SIZE);
        UNLOCK_APP_AUDIO_QUEUE();
        memset(pcm_buffer,0,pcm_len);
        pcm_processed = pcm_len;
        goto exit;
    }    
    start_decoder:


    //DUMP8("%02x ", msbc_buf_before_decode, 8);
#if defined(MSBC_PLC_ENABLE)
    // [0]-1,msbc sync byte
    // [1]-seqno
    // code_frame(57 bytes)
    // padding(1 byte)

    if(msbc_need_check_sync_header)
    {
        do
        {
            if(!sco_parse_synchronization_header(&msbc_buff[sync_offset], &msbc_raw_sn)){
                break;
            }
            sync_offset ++;
        }while(sync_offset < (MSBC_FRAME_SIZE - 3));

        msbc_need_check_sync_header = 0;
        if(sync_offset > 0 && sync_offset != (MSBC_FRAME_SIZE - 3)){
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, sync_offset);                
            UNLOCK_APP_AUDIO_QUEUE();
            TRACE("fix sync:%d", sync_offset);                
            goto get_again;
            //TRACE("***msbc_need_check_sync %d :%x %x %x\n", sync_offset, msbc_buf_before_decode[0],
            //        msbc_buf_before_decode[1],
            //        msbc_buf_before_decode[2]);
        }else if(sync_offset == (MSBC_FRAME_SIZE - 3)){
            TRACE("!!!!!!!!!!! all zero");
        }else if (!sync_offset){
            // do nothing            
        }else{
            TRACE("3 msbc_need_check_sync %d\n", sync_offset);
        }
    }
    // 0 - normal decode without plc proc, 1 - normal decode with plc proc, 2 - special decode with plc proc
    plc_type = 0;
    if (sco_parse_synchronization_header(msbc_buf_before_decode, &msbc_raw_sn)){
        plc_type = 2;
        msbc_need_check_sync_header = 1;
    }else{
        plc_type = 1;
    }

    if (msbc_raw_sn_pre == 0xff && 
        msbc_raw_sn != 0xff){
        // do nothing            
        msbc_raw_sn_pre = msbc_raw_sn;
    }else{
        if (((msbc_raw_sn_pre+1)%4) == msbc_raw_sn){
            // do nothing                     
            msbc_raw_sn_pre = msbc_raw_sn;
        }else if (msbc_raw_sn == 0xff){
            TRACE("sbchd err:%d", MSBC_FRAME_SIZE-3);            
            msbc_need_check_sync_header = 1;
            LOCK_APP_AUDIO_QUEUE();
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, MSBC_FRAME_SIZE);                
            UNLOCK_APP_AUDIO_QUEUE();
            msbc_raw_sn_pre = (msbc_raw_sn_pre+1)%4;                
            plc_type = 2;
        }else{
            TRACE("seq err:%d/%d", msbc_raw_sn, msbc_raw_sn_pre);
            msbc_need_check_sync_header = 1;
            plc_type = 2;
            //skip seq err
            msbc_raw_sn_pre = 0xff;
            plc_type = 0;
        }
    }

    //TRACE("type %d, seqno 0x%x, q_space %d\n", plc_type, cur_pkt_seqno, APP_AUDIO_AvailableOfCQueue(&voicebtpcm_m2p_queue));
#endif

    //DUMP8("%02x ", msbc_buf_before_decode, msbc_next_frame_size);
    //TRACE("\n");

#if defined(MSBC_PLC_ENABLE)
    if (plc_type == 1)
    {
#ifdef __SBC_FUNC_IN_ROM__
        ret = SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, msbc_next_frame_size, &byte_decode,
                                            &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
        ret = SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, msbc_next_frame_size, &byte_decode,
                               &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif
        ttt = hal_sys_timer_get();
#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_lc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#else
        PLC_good_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif

    }
    else if (plc_type == 2)
    {
#if defined(MSBC_PCM_PLC_ENABLE)
        ret = XA_STATUS_SUCCESS;
        ttt = hal_sys_timer_get();
        speech_plc_16000_Dofe((PlcSt_16000 *)speech_lc, (short *)pcm_buffer, pcm_len/2);
#else
        PLC_bad_frame(&msbc_plc_state, (short *)pcm_data.data, (short *)pcm_buffer);
#endif
        TRACE("b t:%d ret:%d\n", (hal_sys_timer_get()-ttt), ret);
    }
    else
#endif
    {
#ifdef __SBC_FUNC_IN_ROM__
        ret = SBC_ROM_FUNC.SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, msbc_next_frame_size, &byte_decode,
                                            &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#else
        ret = SBC_DecodeFrames(&msbc_decoder, (unsigned char *)msbc_buf_before_decode, msbc_next_frame_size, &byte_decode,
                               &pcm_data, pcm_len - pcm_offset, msbc_eq_band_gain);
#endif  
#if defined(MSBC_PCM_PLC_ENABLE)
        speech_plc_16000_AddToHistory((PlcSt_16000 *)speech_lc, (short *)pcm_data.data, pcm_len/2);
        memcpy(pcm_buffer, pcm_data.data, pcm_len);
#endif
    }

#if 0
    TRACE("[0] %x", msbc_buf_before_decode[0]);
    TRACE("[1] %x", msbc_buf_before_decode[1]);
    TRACE("[2] %x", msbc_buf_before_decode[2]);
    TRACE("[3] %x", msbc_buf_before_decode[3]);
    TRACE("[4] %x", msbc_buf_before_decode[4]);
#endif

    //TRACE("sbcd ret %d %d\n", ret, byte_decode);

    if(ret == XA_STATUS_CONTINUE)
    {
        need_init_decoder = false;
        LOCK_APP_AUDIO_QUEUE();
        VOICEBTPCM_TRACE("000000 byte_decode =%d, current_len1 =%d",byte_decode, msbc_next_frame_size);
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();

        msbc_next_frame_size = (MSBC_FRAME_SIZE -byte_decode)>0?(MSBC_FRAME_SIZE -byte_decode):MSBC_FRAME_SIZE;
        goto get_again;
    }

    else if(ret == XA_STATUS_SUCCESS)
    {
        need_init_decoder = false;
        pcm_processed = pcm_data.dataLen;
        pcm_data.dataLen = 0;

        LOCK_APP_AUDIO_QUEUE();
#if defined(MSBC_PLC_ENABLE)
        if (plc_type == 0)
        {
            byte_decode += 1;//padding
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, byte_decode);
        }
        else
        {
#if defined(MSBC_PCM_PLC_ENABLE)
        if (plc_type == 2){
            pcm_processed = pcm_len;
        }else{
            if(msbc_next_frame_size < MSBC_FRAME_SIZE)
                msbc_next_frame_size += 1; //padding
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, msbc_next_frame_size);
        }
#else        
            if(msbc_next_frame_size < MSBC_FRAME_SIZE)
                msbc_next_frame_size += 1; //padding
            APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, msbc_next_frame_size);
#endif
        }
#else
        if(msbc_next_frame_size < MSBC_FRAME_SIZE)
            msbc_next_frame_size += 1; //padding
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, msbc_next_frame_size);
#endif
        UNLOCK_APP_AUDIO_QUEUE();

        msbc_next_frame_size = MSBC_FRAME_SIZE;
    }
    else if(ret == XA_STATUS_FAILED)
    {
        need_init_decoder = true;
        pcm_processed = pcm_len;
        pcm_data.dataLen = 0;

        memset(pcm_buffer, 0, pcm_len);
        TRACE("err mutelen:%d\n",pcm_processed);

        LOCK_APP_AUDIO_QUEUE();
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();

        msbc_next_frame_size = MSBC_FRAME_SIZE;

        /* leave */
    }
    else if(ret == XA_STATUS_NO_RESOURCES)
    {
        need_init_decoder = true;
        pcm_processed = pcm_len;
        pcm_data.dataLen = 0;

        memset(pcm_buffer, 0, pcm_len);
        TRACE("no_res mutelen:%d\n",pcm_processed);

        LOCK_APP_AUDIO_QUEUE();
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_queue, 0, byte_decode);
        UNLOCK_APP_AUDIO_QUEUE();

        msbc_next_frame_size = MSBC_FRAME_SIZE;
    }

exit:
    if (underflow||need_init_decoder)
    {
        TRACE( "media_msbc_decoder underflow len:%d\n ", pcm_len);
    }

//    TRACE("pcm_processed %d", pcm_processed);

    return pcm_processed;
}
#endif
#endif
//capture flow
//mic-->audioflinger capture-->store_voicebtpcm_p2m_buffer-->get_voicebtpcm_p2m_frame-->bt
//used by capture, store data from mic to memory
int store_voicebtpcm_p2m_buffer(unsigned char *buf, unsigned int len)
{
    int POSSIBLY_UNUSED size;
    unsigned int avail_size = 0;
    LOCK_APP_AUDIO_QUEUE();
//    merge_two_trace_to_one_track_16bits(0, (uint16_t *)buf, (uint16_t *)buf, len>>1);
//    r = APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len>>1);
    avail_size = APP_AUDIO_AvailableOfCQueue(&voicebtpcm_p2m_queue);
    if (len <= avail_size)
    {
        APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len);
    }
    else
    {
        VOICEBTPCM_TRACE( "mic buff overflow %d/%d", len, avail_size);
        APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len - avail_size);
        APP_AUDIO_EnCQueue(&voicebtpcm_p2m_queue, buf, len);
    }
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    VOICEBTPCM_TRACE("p2m :%d/%d", len, size);

    return 0;
}

#if defined(HFP_1_6_ENABLE)
unsigned char get_msbc_counter(void)
{
    if (msbc_counter == 0x08)
    {
        msbc_counter = 0x38;
    }
    else if (msbc_counter == 0x38)
    {
        msbc_counter = 0xC8;
    }
    else if (msbc_counter == 0xC8)
    {
        msbc_counter = 0xF8;
    }
    else if (msbc_counter == 0xF8)
    {
        msbc_counter = 0x08;
    }

    return msbc_counter;
}
#endif

//used by capture, get the memory data which has be stored by store_voicebtpcm_p2m_buffer()
int get_voicebtpcm_p2m_frame(unsigned char *buf, unsigned int len)
{
    int r = 0, got_len = 0;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    // TRACE("[%s] pcm_len = %d", __func__, len / 2);
    if (voicebtpcm_cache_p2m_status == APP_AUDIO_CACHE_CACHEING)
    {
        app_audio_memset_16bit((short *)buf, 0, len/2);
        TRACE("[%s] APP_AUDIO_CACHE_CACHEING", __func__);
        return len;
    }

    int msbc_encode_temp_len = ENCODE_TEMP_PCM_LEN * sizeof(short);

    ASSERT(len % msbc_encode_temp_len == 0 , "[%s] len(%d) is invalid", __func__, len);

    int loop_cnt = len / msbc_encode_temp_len;
    len = msbc_encode_temp_len;

    for (int cnt=0; cnt<loop_cnt; cnt++)
    {   
        got_len = 0; 
        buf += msbc_encode_temp_len * cnt; 
        if (bt_sco_codec_is_msbc())
        {
#ifdef HFP_1_6_ENABLE
            uint16_t bytes_encoded = 0, buf_len = len;
            uint16_t *dest_buf = 0, offset = 0;

            dest_buf = (uint16_t *)buf;

    //reinit_encoder:
            if(need_init_encoder)
            {
                SBC_InitEncoder(msbc_encoder);
                msbc_encoder->streamInfo.mSbcFlag = 1;
                msbc_encoder->streamInfo.numChannels = 1;
                msbc_encoder->streamInfo.channelMode = SBC_CHNL_MODE_MONO;

                msbc_encoder->streamInfo.bitPool     = 26;
                msbc_encoder->streamInfo.sampleFreq  = SBC_CHNL_SAMPLE_FREQ_16;
                msbc_encoder->streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
                msbc_encoder->streamInfo.numBlocks   = MSBC_BLOCKS;
                msbc_encoder->streamInfo.numSubBands = 8;
                need_init_encoder = 0;
            }
            LOCK_APP_AUDIO_QUEUE();
            r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, MSBC_ENCODE_PCM_LEN * sizeof(short), &e1, &len1, &e2, &len2);
            UNLOCK_APP_AUDIO_QUEUE();

            if(r == CQ_OK)
            {
                if (len1)
                {
                    memcpy(temp_msbc_buf, e1, len1);
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len1);
                    UNLOCK_APP_AUDIO_QUEUE();
                    got_len += len1;
                }
                if (len2 != 0)
                {
                    memcpy(temp_msbc_buf+got_len, e2, len2);
                    got_len += len2;
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len2);
                    UNLOCK_APP_AUDIO_QUEUE();
                }

                //int t = 0;
                //t = hal_sys_timer_get();
                msbc_encoder_pcmdata.data = temp_msbc_buf;
                msbc_encoder_pcmdata.dataLen = MSBC_ENCODE_PCM_LEN;
                memset(temp_msbc_buf1, 0, MSBC_ENCODE_PCM_LEN * sizeof(short));
                SBC_EncodeFrames(msbc_encoder, &msbc_encoder_pcmdata, &bytes_encoded, temp_msbc_buf1, (uint16_t *)&buf_len, 0xFFFF);
                //TRACE("enc msbc %d t\n", hal_sys_timer_get()-t);
                //TRACE("encode len %d, out len %d\n", bytes_encoded, buf_len);

                dest_buf[offset++] = 1<<8;
                dest_buf[offset++] = get_msbc_counter()<<8;

                for (int i = 0; i < buf_len; ++i)
                {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }

                dest_buf[offset++] = 0; //padding

                msbc_encoder_pcmdata.data = temp_msbc_buf + MSBC_ENCODE_PCM_LEN;
                msbc_encoder_pcmdata.dataLen = MSBC_ENCODE_PCM_LEN;
                memset(temp_msbc_buf1, 0, MSBC_ENCODE_PCM_LEN * sizeof(short));
                SBC_EncodeFrames(msbc_encoder, &msbc_encoder_pcmdata, &bytes_encoded, temp_msbc_buf1, (uint16_t *)&buf_len, 0xFFFF);
                //TRACE("encode len %d, out len %d\n", bytes_encoded, buf_len);

                dest_buf[offset++] = 1<<8;
                dest_buf[offset++] = get_msbc_counter()<<8;

                for (int i = 0; i < buf_len; ++i)
                {
                    dest_buf[offset++] = temp_msbc_buf1[i]<<8;
                }
                dest_buf[offset++] = 0; //padding

                got_len = len;
            }
#endif
        }
        else
        {
#if defined(_CVSD_BYPASS_)
            got_len = encode_cvsd_frame(buf, len);
#else
            LOCK_APP_AUDIO_QUEUE();
            //        size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
            r = APP_AUDIO_PeekCQueue(&voicebtpcm_p2m_queue, len - got_len, &e1, &len1, &e2, &len2);
            UNLOCK_APP_AUDIO_QUEUE();

            //        VOICEBTPCM_TRACE("p2m :%d/%d", len, APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue));

            if(r == CQ_OK)
            {
                if (len1)
                {
                    app_audio_memcpy_16bit((short *)buf, (short *)e1, len1/2);
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len1);
                    UNLOCK_APP_AUDIO_QUEUE();
                    got_len += len1;
                }
                if (len2 != 0)
                {
                    app_audio_memcpy_16bit((short *)(buf+got_len), (short *)e2, len2/2);
                    got_len += len2;
                    LOCK_APP_AUDIO_QUEUE();
                    APP_AUDIO_DeCQueue(&voicebtpcm_p2m_queue, 0, len2);
                    UNLOCK_APP_AUDIO_QUEUE();
                }
            }
            else
            {
                VOICEBTPCM_TRACE( "mic buff underflow");
                app_audio_memset_16bit((short *)buf, 0, len/2);
                got_len = len;
                voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_CACHEING;
            }
#endif
        }
    }


    return len;
}

#if 1
static short Coefs_2mic[2] = { 5243, 1392 };

#if (defined(SPEECH_TX_2MIC_NS) || defined(SPEECH_TX_2MIC_NS2))
static int filterState[2] = { 0, 0 };
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
static int filterState1[2] = { 0, 0 };
#endif
#endif

void LowPassFilter(short* signal_in, short* signal_out, int* filter_state, int in_length)
{
    short tmp16_1 = 0, tmp16_2 = 0;
    int tmp32_1 = filter_state[0];
    int tmp32_2 = filter_state[1];
    int n = 0;
    int half_length = (in_length >> 1);  // Downsampling by 2 gives half length.

    for (n = 0; n < half_length; n++)
    {
        tmp16_1 = (short)((tmp32_1 >> 1) + (((int)Coefs_2mic[0] * (*signal_in)) >> 14));
        tmp32_1 = (int)(*signal_in++) - (((int)Coefs_2mic[0] * tmp16_1) >> 12);
        *signal_out = tmp16_1;
        tmp16_2 = (short)((tmp32_2 >> 1) + (((int)Coefs_2mic[1] * (*signal_in) >> 14)));
        tmp32_2 = (int)(*signal_in++) - (((int)Coefs_2mic[1] * tmp16_2) >> 12);
        *signal_out++ += tmp16_2;
    }
    // Store the filter states.
    filter_state[0] = tmp32_1;
    filter_state[1] = tmp32_2;
}

short soir_filter4(int32_t PcmValue)
{
    float gain = 1.0F;
    //const float NUM[3] = {0.2929, 0.5858, 0.2929};
    //const float DEN[3] = {1.0000, -0.0000, 0.1716};
    const float NUM[5]={
         6.17839070e-02,     2.47135628e-01,     3.70703442e-01,     2.47135628e-01, 6.17839070e-02
    };
    const float DEN[5] = {
         1.00 ,   -4.88397684e-01,     5.61516703e-01,    -1.06944660e-01, 2.23681542e-02
    };

    static float y0 = 0, y1 = 0, y2 = 0,y3=0,y4=0, x0 = 0, x1 = 0, x2 = 0,x3=0,x4=0;
    int32_t PcmOut = 0;

    // Left channel
    x0 = PcmValue* gain;
    y0 = x0*NUM[0] + x1*NUM[1] + x2*NUM[2]+x3*NUM[3]+x4*NUM[4] - y1*DEN[1] - y2*DEN[2]-y3*DEN[3]-y4*DEN[4];
    y4=y3;
    y3=y2;
    y2 = y1;
    y1 = y0;
    x4=x3;
    x3=x2;
    x2 = x1;
    x1 = x0;

    PcmOut = (int32_t)y0;

    PcmOut = __SSAT(PcmOut,16);

    return (short)PcmOut;
}
#endif

#if 0
void get_mic_data_max(short *buf, uint32_t len)
{
    int max0 = -32768, min0 = 32767, diff0 = 0;
    int max1 = -32768, min1 = 32767, diff1 = 0;

    for(uint32_t i=0; i<len/2; i+=2)
    {
        if(buf[i+0]>max0)
        {
            max0 = buf[i+0];
        }

        if(buf[i+0]<min0)
        {
            min0 = buf[i+0];
        }

        if(buf[i+1]>max1)
        {
            max1 = buf[i+1];
        }

        if(buf[i+1]<min1)
        {
            min1 = buf[i+1];
        }
    }
    TRACE("min0 = %d, max0 = %d, diff0 = %d, min1 = %d, max1 = %d, diff1 = %d", min0, max0, max0 - min0, min1, max1, max1 - min1);
}
#endif

#include "speech_ssat.h"
//just for test the gain of two channel input
static int State_M_2[2] = { 0 };

void dc_filter_2(short *in, int len, float left_gain, float right_gain)
{
    int tmp1;

    for (int i = 0; i<len; i+=2)
    {
        State_M_2[0] = (15 * State_M_2[0] + in[i]) >> 4;
        State_M_2[1] = (15 * State_M_2[1] + in[i + 1]) >> 4;

        tmp1 = in[i];
        tmp1 -= State_M_2[0];
        tmp1 = (int)((float)tmp1 * left_gain);//ÏîÈ¦¶ú»ú
        in[i] = speech_ssat_int16(tmp1);

        tmp1 = in[i + 1];
        tmp1 -= State_M_2[1];
        tmp1 = (int)((float)tmp1 * right_gain);//ÏîÈ¦¶ú»ú
        in[i + 1] = speech_ssat_int16(tmp1);
        //in[2 * i + 1] = tmp1*1.05f/2.0f;//haran Í·´÷Ê½
    }
}

#ifdef __BT_ANC__
extern uint8_t bt_sco_samplerate_ratio;
#define US_COEF_NUM (24)

/*
8K -->48K
single phase coef Number:=24,    upsample factor:=6,
      64, -311,  210, -320,  363, -407,  410, -356,  196,  191,-1369,30721, 3861,-2265, 1680,-1321, 1049, -823,  625, -471,  297, -327,   34,   19,
   72, -254,   75, -117,   46,   58, -256,  583,-1141, 2197,-4866,28131,10285,-4611, 2935,-2061, 1489,-1075,  755, -523,  310, -291,  -19,   31,
   65, -175,  -73,   84, -245,  457, -786, 1276,-2040, 3377,-6428,23348,17094,-6200, 3592,-2347, 1588,-1073,  703, -447,  236, -203,  -92,   49,
   49,  -92, -203,  236, -447,  703,-1073, 1588,-2347, 3592,-6200,17094,23348,-6428, 3377,-2040, 1276, -786,  457, -245,   84,  -73, -175,   65,
   31,  -19, -291,  310, -523,  755,-1075, 1489,-2061, 2935,-4611,10285,28131,-4866, 2197,-1141,  583, -256,   58,   46, -117,   75, -254,   72,
   19,   34, -327,  297, -471,  625, -823, 1049,-1321, 1680,-2265, 3861,30721,-1369,  191,  196, -356,  410, -407,  363, -320,  210, -311,   64,
*/

const static short coef_8k_upto_48k[6][US_COEF_NUM]__attribute__((section(".sram_data")))  =
{
    {64, -311,  210, -320,  363, -407,  410, -356,  196,    191,-1369,30721, 3861,-2265, 1680,-1321, 1049, -823,  625, -471,  297, -327,   34,   19},
    {72, -254,  75, -117,   46,   58, -256,  583,-1141, 2197,-4866,28131,10285,-4611, 2935,-2061, 1489,-1075,  755, -523,  310, -291,  -19,   31   },
    {65, -175,  -73,   84, -245,  457, -786, 1276,-2040, 3377,-6428,23348,17094,-6200, 3592,-2347, 1588,-1073,  703, -447,  236, -203,  -92,   49  },
    {49,  -92, -203,  236, -447,  703,-1073, 1588,-2347, 3592,-6200,17094,23348,-6428, 3377,-2040, 1276, -786,  457, -245,  84,  -73, -175,   65  },
    {31,  -19, -291,  310, -523,  755,-1075, 1489,-2061, 2935,-4611,10285,28131,-4866, 2197,-1141,  583, -256,  58,   46, -117,   75, -254,   72  },
    {19,   34, -327,  297, -471,  625, -823, 1049,-1321, 1680,-2265, 3861,30721,-1369,  191,  196, -356,  410, -407,  363, -320,  210, -311,   64 }
};

/*
16K -->48K

single phase coef Number:=24,    upsample factor:=3,
       1, -291,  248, -327,  383, -405,  362, -212, -129,  875,-2948,29344, 7324,-3795, 2603,-1913, 1418,-1031,  722, -478,  292, -220,  -86,   16,
   26, -212,    6,   45, -185,  414, -764, 1290,-2099, 3470,-6431,20320,20320,-6431, 3470,-2099, 1290, -764,  414, -185,   45,    6, -212,   26,
   16,  -86, -220,  292, -478,  722,-1031, 1418,-1913, 2603,-3795, 7324,29344,-2948,  875, -129, -212,  362, -405,  383, -327,  248, -291,    1,
*/

const static short coef_16k_upto_48k[3][US_COEF_NUM]  __attribute__((section(".sram_data"))) =
{
    {1, -291,  248, -327,  383, -405,  362, -212, -129, 875,-2948,29344, 7324,-3795, 2603,-1913, 1418,-1031,  722, -478,  292, -220,  -86,   16},
    {26, -212,   6,   45, -185,  414, -764, 1290,-2099, 3470,-6431,20320,20320,-6431, 3470,-2099, 1290, -764,  414, -185,   45,    6, -212,   26},
    {16,  -86, -220,  292, -478,  722,-1031, 1418,-1913, 2603,-3795, 7324,29344,-2948,  875, -129, -212,  362, -405,  383, -327,  248, -291,    1}
};

static short us_para_lst[US_COEF_NUM-1];

static inline short us_get_coef_para(U32 samp_idx,U32 coef_idx)
{
    if(bt_sco_samplerate_ratio == 6)
        return coef_8k_upto_48k[samp_idx][coef_idx];
    else
        return coef_16k_upto_48k[samp_idx][coef_idx];
}

void us_fir_init (void)
{
    app_audio_memset_16bit(us_para_lst, 0, sizeof(us_para_lst)/sizeof(short));
}


__attribute__((section(".fast_text_sram"))) U32 us_fir_run (short* src_buf, short* dst_buf, U32 in_samp_num)
{
    U32 in_idx, samp_idx, coef_idx, real_idx, out_idx;
    int para, out;

    for (in_idx = 0, out_idx = 0; in_idx < in_samp_num; in_idx++)
    {
        for (samp_idx = 0; samp_idx < bt_sco_samplerate_ratio; samp_idx++)
        {
            out = 0;
            for (coef_idx = 0; coef_idx < US_COEF_NUM; coef_idx++)
            {
                real_idx = coef_idx + in_idx;
                para = (real_idx < (US_COEF_NUM-1))?us_para_lst[real_idx]:src_buf[real_idx - (US_COEF_NUM-1)];
                out += para * us_get_coef_para(samp_idx,coef_idx);
            }

            dst_buf[out_idx] = (short)(out>>16);
            out_idx++;
        }
    }

    if (in_samp_num >= (US_COEF_NUM-1))
    {
        app_audio_memcpy_16bit(us_para_lst,
                               (src_buf+in_samp_num-US_COEF_NUM+1),
                               (US_COEF_NUM-1));
    }
    else
    {
        U32 start_idx = (US_COEF_NUM-1-in_samp_num);

        app_audio_memcpy_16bit(us_para_lst,
                               (us_para_lst+in_samp_num),
                               start_idx);

        app_audio_memcpy_16bit((us_para_lst + start_idx),
                               src_buf,
                               in_samp_num);
    }
    return out_idx;
}

uint32_t voicebtpcm_pcm_resample (short* src_samp_buf, uint32_t src_smpl_cnt, short* dst_samp_buf)
{
    return us_fir_run (src_samp_buf, dst_samp_buf, src_smpl_cnt);
}

#endif

static int speech_tx_aec_frame_len = 0;

int speech_tx_aec_get_frame_len(void)
{
    return speech_tx_aec_frame_len;
}

void speech_tx_aec_set_frame_len(int len)
{
    TRACE("[%s] len = %d", __func__, len);
    speech_tx_aec_frame_len = len;
}

#if 1
//used by capture, store data from mic to memory
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len)
{
    int16_t POSSIBLY_UNUSED ret = 0;
    bool POSSIBLY_UNUSED vdt = false;
    int size = 0;

    short *pcm_buf = (short*)buf;
    uint32_t pcm_len = len / 2;

    // TRACE("[%s] pcm_len = %d", __func__, pcm_len);

//     dc_filter_2(pcm_buf, pcm_len, 1.0, 1.0);

    audio_dump_clear_up();

    audio_dump_add_channel_data(0, pcm_buf, pcm_len);

#if 0
    // Test MIC: Get one channel data
    // Enable this, should bypass SPEECH_TX_2MIC_NS and SPEECH_TX_2MIC_NS2
    for(uint32_t i=0; i<pcm_len/2; i++)
    {
        pcm_buf[i] = pcm_buf[2*i];      // Left channel
        // pcm_buf[i] = pcm_buf[2*i+1]; // Right channel
    }

    pcm_len >>= 1;
#endif

#ifdef SPEECH_TX_2MIC_NS
    dual_mic_denoise_run(pcm_buf, pcm_len, pcm_buf);
    // Channel num: two-->one
    pcm_len >>= 1;
#endif

#ifdef SPEECH_TX_2MIC_NS2
    coherent_denoise_run(pcm_buf, pcm_len, pcm_buf);
    // Channel num: two-->one
    pcm_len >>= 1;
#endif

    // Sample rate: 16k-->8k
// #if 0
#if (defined(SPEECH_TX_2MIC_NS) || defined(SPEECH_TX_2MIC_NS2)) && !defined(MSBC_16K_SAMPLE_RATE)
    {
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
        LowPassFilter(echo_buf, echo_buf, filterState1, pcm_len);
#endif
        LowPassFilter(pcm_buf, pcm_buf, filterState, pcm_len);
        pcm_len >>= 1;
    }
#endif

    audio_dump_add_channel_data(1, pcm_buf, pcm_len);

#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
    audio_dump_add_channel_data(2, echo_buf, pcm_len);
#else
    audio_dump_add_channel_data(2, pcm_buf, pcm_len);
#endif

#if defined (SPEECH_TX_AEC)
    speex_echo_cancellation(st, pcm_buf, echo_buf, e_buf);
    memcpy(pcm_buf, e_buf, pcm_len * sizeof(int16_t));
#endif

#ifdef SPEECH_TX_AEC2
    bt_sco_chain_echo_suppress_sendchain(pcm_buf, echo_buf, pcm_len);
#endif

#ifdef SPEECH_TX_AEC2FLOAT
    //uint32_t start = hal_sys_timer_get();
    ec_process(ec2f_st, pcm_buf, echo_buf, e_buf);
    memcpy(pcm_buf, e_buf, pcm_len * sizeof(int16_t));
    //uint32_t end = hal_sys_timer_get();
    //TRACE("ec2 float cost %d ticks", end - start);
#endif

#if defined (SPEECH_TX_NS)
    speex_preprocess_run(den, pcm_buf);
#endif

    audio_dump_add_channel_data(3, pcm_buf, pcm_len);

#ifdef SPEECH_TX_NS2
    //uint32_t start = hal_sys_timer_get();
    lc_mmse_ns_run(speech_tx_ns2_st, pcm_buf);
    //uint32_t end = hal_sys_timer_get();
    //TRACE("ns2 cost %d ticks", end - start);
#endif

#ifdef SPEECH_TX_NS2FLOAT
    //uint32_t start = hal_sys_timer_get();
    lc_mmse_ns_float_run(speech_tx_ns2f_st, pcm_buf);
    //uint32_t end = hal_sys_timer_get();
    //TRACE("ns2 cost %d ticks", end - start);
#endif

#ifdef SPEECH_TX_NS3
    ns3_process(speech_tx_ns3_st, pcm_buf, pcm_len);
#endif

#ifdef SPEECH_TX_NOISE_GATE
    speech_noise_gate_run(speech_tx_noisegate_st, pcm_buf, pcm_len);
#endif

#ifdef SPEECH_TX_COMPEXP
    compexp_process(speech_tx_compexp_st, pcm_buf, pcm_len);
#endif

    audio_dump_add_channel_data(4, pcm_buf, pcm_len);

#if defined(SPEECH_TX_AGC)
    agc_process(speech_tx_agc_st, pcm_buf);
#endif

#if defined (SPEECH_TX_EQ)
    eq_process(speech_tx_eq_st, pcm_buf, pcm_len);
#endif

    audio_dump_add_channel_data(5, pcm_buf, pcm_len);

#if defined(SPEECH_ALGORITHMGAIN)
    iir_run_mono_algorithmgain(SPEECH_ALGORITHMGAIN, pcm_buf, pcm_len);
#endif

    // up resample: 8k/16k-->16k
#if 0
    for(int i=pcm_len-1; i>=0; i--)
    {
        pcm_buf[2*i]=pcm_buf[i];
        pcm_buf[2*i+1]=pcm_buf[i];
    }

    pcm_len <<= 1;

    for(uint32_t i=0; i<pcm_len; i++)
    {
        pcm_buf[i]=soir_filter4((int32_t)pcm_buf[i]);
    }
#endif
    //END

    int16_t *encode_buf = pcm_buf;
    int encode_len = pcm_len;
#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
    if (bt_sco_codec_is_msbc() == true) {
        iir_resample_process(upsample_st, pcm_buf, upsample_buf_for_msbc, pcm_len);
        encode_buf = upsample_buf_for_msbc;
        encode_len = pcm_len * 2;
    }
#endif

    LOCK_APP_AUDIO_QUEUE();
    store_voicebtpcm_p2m_buffer((uint8_t *)encode_buf, encode_len*2);
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    if (size > VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
    {
        voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_OK;
    }

    audio_dump_run();

    return pcm_len*2;
}
#else
//used by capture, store data from mic to memory
uint32_t voicebtpcm_pcm_audio_data_come(uint8_t *buf, uint32_t len)
{
    int16_t POSSIBLY_UNUSED ret = 0;
    bool POSSIBLY_UNUSED vdt = false;
    int size = 0;

    short *pcm_buf = (short*)buf;
    uint32_t pcm_len = len / 2;

    // TRACE("[%s] pcm_len = %d", __func__, pcm_len);

    LOCK_APP_AUDIO_QUEUE();
    store_voicebtpcm_p2m_buffer((uint8_t *)pcm_buf, pcm_len*2);
    size = APP_AUDIO_LengthOfCQueue(&voicebtpcm_p2m_queue);
    UNLOCK_APP_AUDIO_QUEUE();

    if (size > VOICEBTPCM_PCM_TEMP_BUFFER_SIZE)
    {
        voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_OK;
    }

    return pcm_len*2;
}
#endif


#ifdef SPEECH_SPK_FIR_EQ
extern void speech_fir_run(short *buf_pcm, int pcm_len);
#endif

//used by playback, play data from memory to speaker
#if defined(__BT_ANC__)
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len,uint8_t* ec_buf)
#else
uint32_t voicebtpcm_pcm_audio_more_data(uint8_t *buf, uint32_t len)
#endif
{
    uint32_t l = 0;

    // TRACE("[%s] pcm_len = %d", __func__, len/2);

    if (voicebtpcm_cache_m2p_status == APP_AUDIO_CACHE_CACHEING)
    {
        app_audio_memset_16bit((short *)buf, 0, len/2);
        l = len;
    }
    else
    {
#if defined(SPEECH_TX_AEC) || defined(SPEECH_TX_AEC2) || defined(SPEECH_TX_AEC2FLOAT)
#ifdef __BT_ANC__
        {
            short *buf_p=(short *)ec_buf;
            for(uint32_t sample = 0; sample < len/2; sample++)
            {
                *(echo_buf+sample) = *(buf_p + sample*(bt_sco_samplerate_ratio<<1));
            }
        }

#else
        {
            short *buf_p=(short *)buf;
            app_audio_memcpy_16bit((short *)echo_buf, buf_p, len/2);
        }
#endif
#endif

        int decode_len = len;
        uint8_t *decode_buf = buf;
#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
        if (bt_sco_codec_is_msbc() == true) {
            decode_len = len * 2;
            decode_buf = (uint8_t *)downsample_buf_for_msbc;
        }
#endif

        unsigned int len1 = 0, len2 = 0;
        unsigned char *e1 = NULL, *e2 = NULL;
        int r = 0;
        
        LOCK_APP_AUDIO_QUEUE();
        len1 = len2 = 0;
        e1 = e2 = 0;
        r = APP_AUDIO_PeekCQueue(&voicebtpcm_m2p_pcm_queue, decode_len, &e1, &len1, &e2, &len2);
        UNLOCK_APP_AUDIO_QUEUE();
        if (r == CQ_ERR)
        {
            TRACE( "pcm buff underflow");
            return l;
        }

        if (!len1)
        {
            TRACE("pcm  len1 underflow %d/%d\n", len1, len2);
             return l;
        }

        if (len1 > 0 && e1)
        {
            memcpy(decode_buf, e1, len1);
        }
        if (len2 > 0 && e2)
        {
            memcpy(decode_buf + len1, e2, len2);
        }        
        LOCK_APP_AUDIO_QUEUE();
        APP_AUDIO_DeCQueue(&voicebtpcm_m2p_pcm_queue, 0, decode_len);
        UNLOCK_APP_AUDIO_QUEUE();        
    } // if (voicebtpcm_cache_m2p_status == APP_AUDIO_CACHE_CACHEING)

#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
    // downsample_buf_for_msbc size is len * 2
    if (bt_sco_codec_is_msbc() == true) {
        iir_resample_process(downsample_st, downsample_buf_for_msbc, (int16_t *)buf, len);
    }
#endif

#if defined ( SPEECH_RX_NS )
    speex_preprocess_run(den_voice, (short *)buf);
#endif

    app_ring_merge_more_data(buf, len);

    short * POSSIBLY_UNUSED pcm_buf = (short*)buf;
    uint32_t POSSIBLY_UNUSED pcm_len = len / 2;

#ifdef SPEECH_RX_NS2
    // fix 0dB signal
    for(uint32_t i=0; i<pcm_len; i++)
    {
        pcm_buf[i] = pcm_buf[i] * 0.94;
    }
    lc_mmse_ns_run(speech_rx_ns2_st, pcm_buf);
#endif

#ifdef SPEECH_RX_NS2FLOAT
    lc_mmse_ns_float_run(speech_rx_ns2f_st, pcm_buf);
#endif

#ifdef SPEECH_RX_NS3
    ns3_process(speech_rx_ns3_st, pcm_buf, pcm_len);
#endif

#ifdef SPEECH_RX_AGC
    agc_process(speech_rx_agc_st, pcm_buf);
#endif

#if defined (SPEECH_RX_EQ)
    eq_process(speech_rx_eq_st, pcm_buf, pcm_len);
#endif

#ifdef SPEECH_SPK_FIR_EQ
    for (uint32_t i = 0; i < pcm_len; i++)
    {
        pcm_buf[i] = (short)(pcm_buf[i] * 0.7f);
    }
    speech_fir_run(pcm_buf, pcm_len);
#endif

#if defined(SPEAKER_ALGORITHMGAIN)
    iir_run_mono_algorithmgain(SPEAKER_ALGORITHMGAIN, pcm_buf, pcm_len);
#endif

    return l;
}

void *voicebtpcm_get_ext_buff(int size)
{
    uint8_t *pBuff = NULL;
    if (size % 4)
    {
        size = size + (4 - size % 4);
    }
    app_audio_mempool_get_buff(&pBuff, size);
    VOICEBTPCM_TRACE( "[%s] len:%d", __func__, size);
    return (void*)pBuff;
}

int voicebtpcm_pcm_audio_init(void)
{
    // Get system parameter
    int POSSIBLY_UNUSED tx_sample_rate;
    int POSSIBLY_UNUSED tx_frame_length;

    int POSSIBLY_UNUSED rx_sample_rate;
    int POSSIBLY_UNUSED rx_frame_length;

#if defined(HFP_1_6_ENABLE) && defined(MSBC_16K_SAMPLE_RATE)
    if (bt_sco_codec_is_msbc())
    {
        rx_sample_rate = tx_sample_rate = 16000;
    }
    else
#endif
    {
        rx_sample_rate = tx_sample_rate = 8000;
    }

    rx_frame_length = tx_frame_length = SPEECH_FRAME_MS_TO_LEN(tx_sample_rate, SPEECH_CODEC_FRAME_MS);

    TRACE("[%s] TX: sample rate = %d, frame len = %d", __func__, tx_sample_rate, tx_frame_length);
    TRACE("[%s] RX: sample rate = %d, frame len = %d", __func__, rx_sample_rate, rx_frame_length);

    // init cqueue
    uint8_t *p2m_buff = NULL;
    uint8_t *m2p_buff = NULL;
    uint8_t *m2p_pcm_buff = NULL;

    app_audio_mempool_get_buff(&p2m_buff, VOICEBTPCM_PCM_QUEUE_SIZE);
    app_audio_mempool_get_buff(&m2p_buff, VOICEBTPCM_PCM_QUEUE_SIZE);
    app_audio_mempool_get_buff(&m2p_pcm_buff, VOICEBTPCM_PCM_QUEUE_SIZE);
    app_audio_mempool_get_buff(&rx_decode_buffer, ENCODE_TEMP_PCM_LEN * sizeof(short));

    LOCK_APP_AUDIO_QUEUE();
    APP_AUDIO_InitCQueue(&voicebtpcm_p2m_queue, VOICEBTPCM_PCM_QUEUE_SIZE, p2m_buff);
    APP_AUDIO_InitCQueue(&voicebtpcm_m2p_queue, VOICEBTPCM_PCM_QUEUE_SIZE, m2p_buff);
    APP_AUDIO_InitCQueue(&voicebtpcm_m2p_pcm_queue, VOICEBTPCM_PCM_QUEUE_SIZE, m2p_pcm_buff);
    UNLOCK_APP_AUDIO_QUEUE();

    voicebtpcm_cache_m2p_status = APP_AUDIO_CACHE_CACHEING;
    voicebtpcm_cache_p2m_status = APP_AUDIO_CACHE_CACHEING;

    // Speech process
#if MED_MEM_POOL_SIZE > 0
    app_audio_mempool_get_buff(&g_medMemPool, MED_MEM_POOL_SIZE);
    med_system_heap_init(&g_medMemPool[0], &g_medMemPool[MED_MEM_POOL_SIZE - 1]);
#endif

#if defined( SPEECH_TX_NS ) || defined( SPEECH_RX_NS )
    float gain, l;
    spx_int32_t max_gain;
#endif

#if defined( SPEECH_TX_NS ) || defined( SPEECH_RX_NS ) || defined( SPEECH_TX_AEC )
    spx_int32_t l2;
#endif

#if defined(HFP_1_6_ENABLE)
    const float EQLevel[25] =
    {
        0.0630957,  0.0794328, 0.1,       0.1258925, 0.1584893,
        0.1995262,  0.2511886, 0.3162278, 0.398107, 0.5011872,
        0.6309573,  0.794328, 1,         1.258925, 1.584893,
        1.995262,  2.5118864, 3.1622776, 3.9810717, 5.011872,
        6.309573,  7.943282, 10, 12.589254, 15.848932
    };//-12~12
    uint8_t i;

    for (i=0; i<sizeof(msbc_eq_band_gain)/sizeof(float); i++)
    {
        msbc_eq_band_gain[i] = EQLevel[cfg_aud_eq_sbc_band_settings[i]+12];
    }

    bt_sco_player_restart_requeset(false);

    msbc_input_valid = false;
    msbc_input_offset = 0;
    need_init_encoder = 1;
    msbc_counter = 0x08;

    if (bt_sco_codec_is_msbc())
    {
        need_init_decoder = true;
        app_audio_mempool_get_buff((uint8_t **)&msbc_encoder, sizeof(SbcEncoder));
#if defined(MSBC_PLC_ENABLE)
        app_audio_mempool_get_buff(&msbc_buf_before_plc, MSBC_ENCODE_PCM_LEN);
#endif
        app_audio_mempool_get_buff(&msbc_buf_before_decode, MSBC_FRAME_SIZE * sizeof(short));
        app_audio_mempool_get_buff(&temp_msbc_buf, MSBC_ENCODE_PCM_LEN * sizeof(short));
        app_audio_mempool_get_buff(&temp_msbc_buf1, MSBC_ENCODE_PCM_LEN * sizeof(short));
#if defined(MSBC_PLC_ENCODER)
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_buffer, MSBC_PLC_ENCODER_SIZE);
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_data, sizeof(SbcPcmData));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_output_pcmbuffer, (MSBC_PLC_ENCODER_BUFFER_SIZE-MSBC_PLC_ENCODER_BUFFER_OFFSET)*sizeof(short));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder_inbuffer, MSBC_PLC_ENCODER_BUFFER_SIZE*sizeof(short));
        app_audio_mempool_get_buff((uint8_t **)&msbc_plc_encoder, sizeof(SbcEncoder));
        SBC_InitEncoder(msbc_plc_encoder);
        msbc_plc_encoder->streamInfo.mSbcFlag = 1;
        msbc_plc_encoder->streamInfo.numChannels = 1;
        msbc_plc_encoder->streamInfo.channelMode = SBC_CHNL_MODE_MONO;
        msbc_plc_encoder->streamInfo.bitPool     = 26;
        msbc_plc_encoder->streamInfo.sampleFreq  = SBC_CHNL_SAMPLE_FREQ_16;
        msbc_plc_encoder->streamInfo.allocMethod = SBC_ALLOC_METHOD_LOUDNESS;
        msbc_plc_encoder->streamInfo.numBlocks   = MSBC_BLOCKS;
        msbc_plc_encoder->streamInfo.numSubBands = 8;
#endif
    }
#endif

#if defined(_CVSD_BYPASS_)
    Pcm8k_CvsdInit();
#endif

#if defined( SPEECH_TX_AEC ) || defined( SPEECH_TX_AEC2 ) || defined(SPEECH_TX_AEC2FLOAT)
    int aec_frame_len = speech_tx_aec_get_frame_len();
    TRACE("[%s] aec_frame_len = %d", __func__, aec_frame_len);

    echo_buf = (short *)voicebtpcm_get_ext_buff(aec_frame_len * sizeof(short));
    e_buf = (short *)voicebtpcm_get_ext_buff(aec_frame_len * sizeof(short));
#endif

#if defined( SPEECH_TX_AEC )
        int delay = 60;
        st = speex_echo_state_init(tx_frame_length, tx_frame_length, delay, voicebtpcm_get_ext_buff);
        speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &tx_sample_rate);

        int leak_estimate = 16383;//Better 32767/2^n
        speex_echo_ctl(st, SPEEX_ECHO_SET_FIXED_LEAK_ESTIMATE, &leak_estimate);

        // int leak_estimate_shift = 4;//Better 32767/2^n
        // speex_echo_ctl(st, SPEEX_ECHO_SET_LEAK_ESTIMATE_SHIFT, &leak_estimate_shift);
#endif

#if defined( SPEECH_TX_NS )
        den = speex_preprocess_state_init(tx_frame_length, tx_sample_rate, voicebtpcm_get_ext_buff);
#endif

#if defined ( SPEECH_RX_NS )
        den_voice = speex_preprocess_state_init(rx_frame_length, rx_sample_rate, voicebtpcm_get_ext_buff);
#endif

#if defined( SPEECH_TX_AEC ) && defined( SPEECH_TX_NS )
        speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);
#endif

#if defined( SPEECH_TX_NS )
#if defined(SPEECH_TX_NS2) || defined(SPEECH_TX_NS2FLOAT)
    l2 = 0;
#else
    l2 = 1;
#endif
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_DENOISE, &l2);

    l= 32000;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_AGC_LEVEL, &l);

    l2 = 0;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_AGC, &l2);

    gain = 0;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_AGC_FIX_GAIN, &gain);

    max_gain = 24;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &max_gain);
    max_gain /=2;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_INIT_MAX, &max_gain);
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_VOC_DEC_RST_GAIN, &max_gain);

    l2 = -12;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &l2);

#endif

#if defined( SPEECH_TX_AEC )
    l2 = -39;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &l2);

    l2 = -6;
    speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &l2);
#endif

#if defined ( SPEECH_RX_NS )
    l= 32000;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_AGC_LEVEL, &l);

    l2 = 0;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_AGC, &l2);

    gain = 0;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_AGC_FIX_GAIN, &gain);

    max_gain = 20;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &max_gain);

    l2 = -20;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &l2);

    l = 0.3f;
    speex_preprocess_ctl(den_voice, SPEEX_PREPROCESS_SET_NOISE_PROB, &l);
#endif

#ifdef SPEECH_TX_AEC2
    bt_sco_chain_echo_suppress_init(tx_sample_rate);
#endif

#ifdef SPEECH_TX_AEC2FLOAT
    EcConfig ec2f_config = {
        .hpf_enabled = true,
        .af_enabled = true,
        .nlp_enabled = true,
        .blocks = 1,
        .delay = 0,
        .min_ovrd = 2,
        .target_supp = -40
    };
    ec2f_st = ec_init(tx_sample_rate, tx_frame_length, &ec2f_config);
#endif

#if defined(SPEECH_TX_NS2)
    speech_tx_ns2_st = lc_mmse_ns_init(tx_sample_rate, tx_frame_length, -15);
#if defined(SPEECH_TX_AEC) && !defined(SPEECH_TX_NS)
    lc_mmse_ns_set_echo_state(speech_tx_ns2_st, st);
    lc_mmse_ns_set_echo_suppress(speech_tx_ns2_st, -40);
#endif
#endif

#if defined(SPEECH_TX_NS2FLOAT)
    speech_tx_ns2f_st = lc_mmse_ns_float_init(tx_sample_rate, tx_frame_length, -15);
#if defined(SPEECH_TX_AEC) && !defined(SPEECH_TX_NS)
    lc_mmse_ns_float_set_echo_state(speech_tx_ns2f_st, st);
    lc_mmse_ns_float_set_echo_suppress(speech_tx_ns2f_st, -40);
#endif
#endif

#if defined(SPEECH_TX_NS3)
    speech_tx_ns3_st = ns3_init(tx_sample_rate, tx_frame_length, &speech_tx_ns3_cfg);
#endif

#if defined(SPEECH_TX_NOISE_GATE)
    speech_tx_noisegate_st = speech_noise_gate_init(tx_sample_rate, tx_frame_length, &speech_tx_noisegate_cfg);
#endif

#if defined(SPEECH_TX_COMPEXP)
    speech_tx_compexp_st = compexp_init(tx_sample_rate, &speech_tx_compexp_cfg);
#endif

#ifdef SPEECH_TX_2MIC_NS
    dual_mic_denoise_init(NULL, &dual_mic_denoise_cfg);
#endif

#ifdef SPEECH_TX_2MIC_NS2
    coherent_denoise_init(64, &coherent_denoise_cfg);
    TRACE("[%s] delay = %d", __func__, coherent_denoise_cfg.delaytaps);
#endif

#ifdef SPEECH_TX_2MIC_NS3
    far_field_speech_enhancement_init();
    far_field_speech_enhancement_start();
#endif

#if defined(SPEECH_TX_AGC)
    speech_tx_agc_st = agc_state_init(tx_sample_rate, tx_frame_length);
    agc_set_config(speech_tx_agc_st, 2, 3, 1);
#endif

#if defined(SPEECH_TX_EQ)
    if (tx_sample_rate == 8000)
        speech_tx_eq_st = eq_init_raw(tx_sample_rate, tx_frame_length, &speech_tx_eq_cfg_8k);
    else
        speech_tx_eq_st = eq_init_raw(tx_sample_rate, tx_frame_length, &speech_tx_eq_cfg_16k);
#endif

#if defined(SPEECH_RX_PLC)
#if defined(HFP_1_6_ENABLE)
    if (bt_sco_codec_is_msbc())
    {
#ifdef MSBC_PCM_PLC_ENABLE
        speech_lc = (PlcSt_16000 *)speech_plc_16000_init(voicebtpcm_get_ext_buff);
#else
        // Add
#endif
    }
    else
#endif
    {
        speech_lc = (PlcSt_8000 *)speech_plc_8000_init(voicebtpcm_get_ext_buff);
    }
#endif

#ifdef SPEECH_RX_NS2
    speech_rx_ns2_st = lc_mmse_ns_init(rx_sample_rate, rx_frame_length, 0);
#endif

#ifdef SPEECH_RX_NS2FLOAT
    speech_rx_ns2f_st = lc_mmse_ns_float_init(rx_sample_rate, rx_frame_length, 0);
#endif

#ifdef SPEECH_RX_NS3
    speech_rx_ns3_st = ns3_init(rx_sample_rate, rx_frame_length, &speech_rx_ns3_cfg);
#endif

#ifdef SPEECH_RX_AGC
    speech_rx_agc_st = agc_state_init(rx_sample_rate, rx_frame_length);
    agc_set_config(speech_rx_agc_st, 3, 6, 1);
#endif

#if defined(SPEECH_RX_EQ)
    speech_rx_eq_st = eq_init(rx_sample_rate, rx_frame_length, &speech_rx_eq_cfg);
#endif

#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
    upsample_buf_for_msbc = (int16_t *)voicebtpcm_get_ext_buff(sizeof(int16_t) * tx_frame_length * 2);
    downsample_buf_for_msbc = (int16_t *)voicebtpcm_get_ext_buff(sizeof(int16_t) * tx_frame_length * 2);
    upsample_st = iir_resample_init(tx_frame_length, IIR_RESAMPLE_MODE_1TO2);
    downsample_st = iir_resample_init(rx_frame_length * 2, IIR_RESAMPLE_MODE_2TO1);
#endif

    audio_dump_init(tx_frame_length, 2);

    return 0;
}

int voicebtpcm_pcm_audio_deinit(void)
{
    TRACE("[%s] Close...", __func__);
    // TRACE("[%s] app audio buffer free = %d", __func__, app_audio_mempool_free_buff_size());

#if defined(SPEECH_RX_EQ)
    eq_destroy(speech_rx_eq_st);
#endif

#if defined(SPEECH_TX_EQ)
    eq_destroy(speech_tx_eq_st);
#endif

#ifdef SPEECH_TX_AGC
    agc_state_destroy(speech_tx_agc_st);
#endif

#ifdef SPEECH_RX_AGC
    agc_state_destroy(speech_rx_agc_st);
#endif

#ifdef SPEECH_TX_2MIC_NS3
    far_field_speech_enhancement_deinit();
#endif

#ifdef SPEECH_TX_2MIC_NS2
    coheren_denoise_deinit();
#endif

#ifdef SPEECH_TX_2MIC_NS
    dual_mic_denoise_deinit();
#endif

#ifdef SPEECH_TX_NS2
    lc_mmse_ns_deinit(speech_tx_ns2_st);
#endif

#ifdef SPEECH_RX_NS2
    lc_mmse_ns_deinit(speech_rx_ns2_st);
#endif

#ifdef SPEECH_TX_NS2FLOAT
    lc_mmse_ns_float_deinit(speech_tx_ns2f_st);
#endif

#ifdef SPEECH_RX_NS2FLOAT
    lc_mmse_ns_float_deinit(speech_rx_ns2f_st);
#endif

#ifdef SPEECH_TX_NS3
    ns3_destroy(speech_tx_ns3_st);
#endif

#ifdef SPEECH_RX_NS3
    ns3_destroy(speech_rx_ns3_st);
#endif

#ifdef SPEECH_TX_NOISE_GATE
    speech_noise_gate_deinit(speech_tx_noisegate_st);
#endif

#ifdef SPEECH_TX_COMPEXP
    compexp_destroy(speech_tx_compexp_st);
#endif

#ifdef SPEECH_TX_AEC2
     bt_sco_chain_echo_suppress_deinit();
#endif

#ifdef SPEECH_TX_AEC2FLOAT
    ec_destroy(ec2f_st);
#endif

#ifdef SPEECH_RX_NS
    if(den_voice)
    {
        speex_preprocess_state_destroy(den_voice);
        den_voice = NULL;
    }
    else
    {
        TRACE("[%s] WARNING: den_voice == NULL", __func__);
    }
#endif

#ifdef SPEECH_TX_NS
    if(den)
    {
        speex_preprocess_state_destroy(den);
        den = NULL;
    }
    else
    {
        TRACE("[%s] WARNING: den == NULL", __func__);
    }
#endif

#ifdef SPEECH_TX_AEC
    if(st)
    {
        speex_echo_state_destroy(st);
        st = NULL;
    }
    else
    {
        TRACE("[%s] WARNING: st == NULL", __func__);
    }
#endif

#if defined(HFP_1_6_ENABLE) && !defined(MSBC_16K_SAMPLE_RATE)
    iir_resample_destroy(upsample_st);
    iir_resample_destroy(downsample_st);
#endif

#if MED_MEM_POOL_SIZE > 0
    uint32_t total = 0, used = 0, max_used = 0;
    med_memory_info(&total, &used, &max_used);
    TRACE("MED MALLOC MEM: total - %d, used - %d, max_used - %d.", total, used, max_used);
    ASSERT(used == 0, "[%s] used != 0", __func__);
#endif

    rx_decode_buffer = NULL;

    return 0;
}

