/* rbplay source */
/* playback control & rockbox codec porting & codec thread */

#include "mbed.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "rtos.h"
#include <ctype.h>
#include <unistd.h>
#include "SDFileSystem.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "audioflinger.h"
#include "cqueue.h"
#include "app_audio.h"

#include "eq.h"
#include "pga.h"
#include "metadata.h"
#include "dsp_core.h"
#include "codecs.h"
#include "codeclib.h"
#include "compressor.h"
#include "channel_mode.h"
#include "audiohw.h"
#include "codec_platform.h"
#include "metadata_parsers.h"

#include "hal_overlay.h"
#include "app_overlay.h"
#include "rbpcmbuf.h"
#include "rbplay.h"
#include "app_thread.h"
#include "app_utils.h"
#include "app_key.h"
/* Internals */
#ifdef __TWS__
#include "app_tws.h"
#endif

#include "SDFileSystem.h"
//#include "USBFileSystem.h"

#define _RB_TASK_STACK_SIZE (4096)
#define  BT_STREAM_RBCODEC   0x08    //from rockbox decoder

#define _LOG_TAG "[rbplay] "
#define RBPLAY_DEBUG 0
#if  RBPLAY_DEBUG
#define _LOG_DBG(str,...) TRACE(_LOG_TAG""str, ##__VA_ARGS__)
#define _LOG_ERR(str,...) TRACE(_LOG_TAG"err:"""str, ##__VA_ARGS__)

#define _LOG_FUNC_LINE() TRACE(_LOG_TAG"%s:%d\n", __FUNCTION__, __LINE__)
#define _LOG_FUNC_IN() TRACE(_LOG_TAG"%s ++++\n", __FUNCTION__)
#define _LOG_FUNC_OUT() TRACE(_LOG_TAG"%s ----\n", __FUNCTION__)
#else
#define _LOG_DBG(str,...)
#define _LOG_ERR(str,...) TRACE(_LOG_TAG"err:"""str, ##__VA_ARGS__)

#define _LOG_FUNC_LINE()
#define _LOG_FUNC_IN()
#define _LOG_FUNC_OUT()
#endif

extern uint8_t rb_ctl_get_vol(void);

extern void app_rbplay_reopen_file(int *dest_fd);

extern "C" {
    void flac_codec_main(int r);
    void flac_codec_run(void);
    void wav_codec_main(int r);
    void wav_codec_run(void);
    void mpa_codec_main(int r);
    void mpa_codec_run(void);
    void ape_codec_main(int r);
    void ape_codec_run(void);
    void sbc_codec_main(int r);
    void sbc_codec_run(void);
    extern  void hal_cmu_switch_sys_usbpll( bool tousb );
    extern  void analog_usb_300M( void );
    extern  void  tester_init_k1(void) ;
    extern  void  tester_init_k2(void) ;
    extern  uint32_t  tester_get_k1_cost(void) ;
    extern  uint32_t  tester_get_k2_cost(void) ;
    extern  void hal_sdmmc_set_dma_tid(void );
    extern  void UsbWorkLoopStart();
    extern  void osDelay200(void);
}

extern void _rb_pcmbuf_w_waiter_release(void);
extern void _rb_pcmbuf_r_waiter_release(void);
extern void _rb_pcmbuf_media_buff_mutex_release(void);
extern void app_rbplay_doplay_next_song(void);
extern void rb_player_pause_control(void);
extern void rb_player_set_resume_flag(bool en) ;
extern void app_rbplay_enter_sd_and_play(void);
extern void rb_player_key_stop(void) ;
extern void app_rbplay_scan_sd_songs(void);
extern void rb_pcm_player_open(enum AUD_BITS_T bits,enum AUD_SAMPRATE_T sample_rate,enum AUD_CHANNEL_NUM_T channel_num,uint8_t vol) ;
extern int32_t app_rbplay_sd_play_song(uint16_t idx);

#if defined(__TWS__)
typedef struct  _rb_tws_codec_info{
uint8_t update_codec_info;
int32_t sample_freq;
uint8_t channel_num;
}rb_tws_codec_info;

rb_tws_codec_info codec_info = {1, 44100, 2};
#endif

extern void app_auto_power_off_timer(bool turnon);

void rb_player_sync_close_done(void) ;
enum HAL_CMU_FREQ_T rb_decoder_get_freq(void);
enum APP_OVERLAY_ID_T g_rb_overlay_id = APP_OVERLAY_ID_QTY;

static osThreadId rb_decode_tid = NULL;
static osThreadId rb_caller_tid = NULL;

typedef struct {
    uint32_t evt;
    uint32_t arg;
} RBTHREAD_MSG_BLOCK;

#define RBTHREAD_MAILBOX_MAX (10)
osMailQDef (rb_decode_mailbox, RBTHREAD_MAILBOX_MAX, RBTHREAD_MSG_BLOCK);
int rb_decode_mailbox_put(RBTHREAD_MSG_BLOCK* msg_src);
static osMailQId rb_decode_mailbox = NULL;

static void rb_decode_thread(void const *argument);
osThreadDef(rb_decode_thread, osPriorityAboveNormal, _RB_TASK_STACK_SIZE);

/* Rb codec Intface Implements */

static int song_fd = -1;
static int song_format = 0;
struct mp3entry *current_id3;
static uint8_t rbplay_loop_on = 0;

static volatile int rb_decode_halt_flag = 1;

struct codec_api ci_api;
struct codec_api *ci = &ci_api;

volatile osThreadId thread_tid_waiter = NULL;

static int local_player_role = 0; //0:sd,1:usbfs
const char sd_label[] = "sd";

void init_dsp(void);
void codec_configure(int setting, intptr_t value);

uint16_t rb_audio_file[128];
uint16_t g_rbplayer_curr_song_idx = 0;

extern void app_rbplay_exit(void);

extern  void bt_change_to_iic(APP_KEY_STATUS *status, void *param);

extern  void rb_thread_send_switch(bool next);
extern void rb_thread_send_status_change(void );

enum APP_SYSFREQ_FREQ_T rb_player_get_work_freq(void);

static void _ci_yield(void)
{
//for test!!!
    // osThreadYield();
}

static unsigned int _ci_sleep(unsigned int ms)
{
    return 0;
}
static void init_ci_base(void)
{
    _LOG_FUNC_LINE();
    memset(ci, 0, sizeof(struct codec_api));
    _LOG_FUNC_LINE();

    ci->dsp = dsp_get_config(CODEC_IDX_AUDIO);
    _LOG_FUNC_LINE();

    ci->configure = codec_configure;

    /* kernel/ system */
    ci->sleep = _ci_sleep;
    ci->yield = _ci_yield;

    /* strings and memory */
    ci->strcpy = strcpy;
    ci->strlen = strlen;
    ci->strcmp = strcmp;
    ci->strcat = strcat;
    ci->memset = memset;
    ci->memcpy = memcpy;
    ci->memmove = memmove;
    ci->memcmp = memcmp;
    ci->memchr = memchr;
}

extern void rb_check_stream_reconfig(int32_t freq, uint8_t ch);
static void f_codec_pcmbuf_insert_callback(
    const void *ch1, const void *ch2, int count)
{
    struct dsp_buffer src;
    struct dsp_buffer dst;

    _LOG_FUNC_LINE();

    src.remcount  = count;
    src.pin[0]    = (const unsigned char *)ch1;
    src.pin[1]    = (const unsigned char *)ch2;
    src.proc_mask = 0;

//  TRACE("%s output:%d \n",__func__,count);
#ifndef __TWS__ 
    while (1) {
        dst.remcount = 0;
        dst.bufcount = MIN(src.remcount, 128); /* Arbitrary min request */

        if(rb_codec_running() == 0) return ;

        if ((dst.p16out = (short *)rb_pcmbuf_request_buffer(&dst.bufcount)) == NULL) {
            _LOG_DBG("wait pcmbuf  %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",dst.bufcount);
            //osDelay(1);
            return ;
        } else {
            _LOG_DBG("pcmbuf %d, src.rem %d\n", dst.bufcount, src.remcount);

            //TRACE("%s putbuflen(32): buf: 0x%x, fill 0x%x, COST:0x%x \n",__func__,dst.bufcount,dst.remcount,tester_get_k1_cost());
            dsp_process(ci->dsp, &src, &dst);

            if (dst.remcount > 0) {
                rb_pcmbuf_write(dst.remcount);
            }

            if (src.remcount <= 0) {
                return; /* No input remains and DSP purged */
            }
        }
    }
#else

    if(codec_info.update_codec_info){
        rb_set_sbc_encoder_freq_ch(codec_info.sample_freq, codec_info.channel_num); //should call this to set trigger timer
        rb_check_stream_reconfig(codec_info.sample_freq, codec_info.channel_num);
        codec_info.update_codec_info = 0;
    }

    if(tws_local_player_need_tran_2_slave()){
        rb_tws_start_master_player(BT_STREAM_RBCODEC);  
    }
     _LOG_FUNC_LINE();
    while(1){
        uint8_t * pcm_buff = NULL;
        dst.remcount = 0;
        dst.bufcount = MIN(src.remcount, 128); /* Arbitrary min request */
        dst.p16out = (short *)rb_pcmbuf_request_buffer(&dst.bufcount);
        pcm_buff = (uint8_t *)dst.p16out;
        
        if(rb_codec_running() == 0){ 
             _LOG_FUNC_LINE();
            return ;
        }
        dsp_process(ci->dsp, &src, &dst);

        if (dst.remcount > 0) {
            while(rb_push_pcm_in_tws_buffer(pcm_buff, dst.remcount*2*2) == 0){
                if(rb_codec_running() == 0) return ;
                    osDelay(2);
            }
        }

        if (src.remcount <= 0) {
            _LOG_FUNC_LINE();
            return; /* No input remains and DSP purged */
        }
    }
	_LOG_FUNC_LINE();
#endif
}
void f_audio_codec_update_elapsed(unsigned long elapsed)
{
    _LOG_FUNC_LINE();
}
static size_t f_codec_filebuf_callback(void *ptr, size_t size)
{
    _LOG_FUNC_LINE();
    ssize_t ret;
    //TRACE("filebuf ptr 0x%x, size %d [1111111111111111]\n", ptr, size);
    // tester_init_k1();
    if(rb_codec_running() == 0) {
        TRACE("%s fail rbcodec should be end\n",__func__);
        osDelay(100);
        return 0;
    }
    ret = read(song_fd, ptr, size);
    if(ret < 0) {
        TRACE("%s failed with %d\n",__func__,ret);
        osDelay(100);
        return 0;
    }
    //TRACE("%s GETFILELEN 0x%x,COST:0x%x \n",__func__,size,tester_get_k1_cost());
    //osDelay(1);

    return ret;

}
static void * f_codec_request_buffer_callback(size_t *realsize, size_t reqsize)
{
    _LOG_FUNC_LINE();
    return NULL;

}

static void * f_codec_advance_buffer_callback(size_t amount)
{
    _LOG_FUNC_LINE();
    _LOG_DBG("adv %d\n", amount);

seek:
    if(rb_codec_running() == 0) {
        TRACE("%s fail rbcodec should be end\n",__func__);
        osDelay(100);
        return NULL;
    }

    if(lseek(song_fd, (off_t)(ci->curpos + amount), SEEK_SET) < 0) {
//repon file here
        TRACE("%s fail !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__);
        osDelay(100);
        app_rbplay_reopen_file(&song_fd);
        goto seek;
    }

    ci->curpos += amount;
    return (void*)ci;

}

static bool f_codec_seek_buffer_callback(size_t newpos)
{
    _LOG_FUNC_LINE();
    _LOG_DBG("seek %d %d\n", newpos,song_fd);

seek:
    if(rb_codec_running() == 0) {
        TRACE("%s fail rbcodec should be end\n",__func__);
        osDelay(100);
        return false;
    }
    if(lseek(song_fd, (off_t)newpos, SEEK_SET) < 0) {
        TRACE("%s fail \n",__func__);
        osDelay(100);
        app_rbplay_reopen_file(&song_fd);
        goto seek;
    }

    ci->curpos = newpos;

    return true;

}
static void f_codec_seek_complete_callback(void)
{
    _LOG_FUNC_LINE();
    /* Clear DSP */
    if(rb_codec_running() == 0) {
        TRACE("%s fail rbcodec should be end\n",__func__);
        osDelay(100);
        //return ;
    }
    dsp_configure(ci->dsp, DSP_FLUSH, 0);
}
void f_audio_codec_update_offset(size_t offset)
{
    _LOG_FUNC_LINE();
}
static void f_codec_configure_callback(int setting, intptr_t value)
{
    _LOG_FUNC_LINE();
    dsp_configure(ci->dsp, setting, value);
#ifdef __TWS__
    if(setting == DSP_SET_FREQUENCY){
        if(codec_info.sample_freq != value)
            codec_info.update_codec_info = 1;
        codec_info.sample_freq = value;
    }
    else if(setting == DSP_SET_STEREO_MODE){
        if(codec_info.channel_num != (value == STEREO_MONO ? 1 : 2))
            codec_info.update_codec_info = 1;
        codec_info.channel_num = value == STEREO_MONO ? 1 : 2;
    }
#endif    
    _LOG_FUNC_LINE();    
}
static enum codec_command_action f_codec_get_command_callback(intptr_t *param)
{
    //_LOG_FUNC_LINE();
    if (rb_decode_halt_flag == 1)
        return CODEC_ACTION_HALT ;

    return CODEC_ACTION_NULL;
}
static bool f_codec_loop_track_callback(void)
{
    _LOG_FUNC_LINE();
    return false;
}
static void init_ci_file(void)
{
    _LOG_FUNC_LINE();
    ci->codec_get_buffer = 0;
    ci->pcmbuf_insert    = f_codec_pcmbuf_insert_callback;
    ci->set_elapsed      = f_audio_codec_update_elapsed;
    ci->read_filebuf     = f_codec_filebuf_callback;
    ci->request_buffer   = f_codec_request_buffer_callback;
    ci->advance_buffer   = f_codec_advance_buffer_callback;
    ci->seek_buffer      = f_codec_seek_buffer_callback;
    ci->seek_complete    = f_codec_seek_complete_callback;
    ci->set_offset       = f_audio_codec_update_offset;
    ci->configure        = f_codec_configure_callback;
    ci->get_command      = f_codec_get_command_callback;
    ci->loop_track       = f_codec_loop_track_callback;
}

int rb_codec_init(void)
{
    _LOG_FUNC_LINE();
    _LOG_FUNC_LINE();
    dsp_init();
    _LOG_FUNC_LINE();
    init_ci_base();
    /* rb codec dsp init */
    _LOG_FUNC_LINE();
    /* eq dsp init */
    _LOG_FUNC_LINE();
    init_dsp();
    _LOG_FUNC_LINE();

    init_ci_file();

    return 0;
}


void rb_codec_deinit(void)
{
    _LOG_FUNC_IN();
    _LOG_FUNC_OUT();
}

void rb_play_init(RB_ST_Callback cb)
{
    _LOG_FUNC_LINE();

    rb_codec_init();

    _LOG_FUNC_LINE();

    rb_pcmbuf_init();

    _LOG_FUNC_OUT();
}

void rb_play_prepare_mem(const char* mem_address)
{
    _LOG_FUNC_IN();
    _LOG_FUNC_OUT();
}

void rb_play_prepare_file_raw(const char* file_path, unsigned int len, int type)
{
    _LOG_FUNC_IN();
    _LOG_FUNC_OUT();
}

void rb_play_prepare_mem_raw(const char *mem_address, unsigned int len, int type)
{
    _LOG_FUNC_IN();
    _LOG_FUNC_OUT();
}

void rb_play_deinit(void)
{
    _LOG_FUNC_IN();
    _LOG_FUNC_OUT();
}

//static void rb_decode_put_evt(RB_CTRL_CMD_T evt)
//{
//    osSignalSet(rb_decode_tid,(int32_t) evt);
//}

//RB_CTRL_CMD_T rb_play_action;

void rb_play_codec_init(void)
{
    _LOG_DBG("%s %d\n",__func__ , song_format);

    RBTHREAD_MSG_BLOCK msg;
    msg.evt = (uint32_t)RB_CTRL_CMD_CODEC_INIT;
    msg.arg = (uint32_t)0;
    rb_decode_mailbox_put(&msg);

//  rb_play_action = RB_CTRL_CMD_CODEC_INIT;
//    rb_decode_put_evt(RB_CTRL_CMD_CODEC_INIT);

    //osDelay(100);
//    TRACE(" %s try wait , pleasecheck",__func__);
//   osSignalWait(0x1203,osWaitForever );

//   TRACE(" %s wait out , pleasecheck",__func__);
}

void rb_play_codec_run(void)
{
    _LOG_DBG("%s %d\n",__func__ , song_format);
    RBTHREAD_MSG_BLOCK msg;
    msg.evt = (uint32_t)RB_CTRL_CMD_CODEC_RUN;
    msg.arg = (uint32_t)0;
    rb_decode_mailbox_put(&msg);
//    TRACE(" %s try wait , pleasecheck",__func__);
//    osSignalWait(0x1203,osWaitForever );
//    TRACE(" %s wait out , pleasecheck",__func__);
//  rb_play_action = RB_CTRL_CMD_CODEC_RUN;
//    rb_decode_put_evt(RB_CTRL_CMD_CODEC_RUN);
}

int rb_codec_init_desc(void )
{
    _LOG_DBG("%s %d\n",__func__ , song_format);
    int type = song_format;

    switch (type) {
#if 0
        case AFMT_FLAC:
#if FPGA==0
            app_overlay_select(APP_OVERLAY_FLAC);
#endif
            g_rb_overlay_id = APP_OVERLAY_FLAC;
            flac_codec_main(CODEC_LOAD);
            break;
        case AFMT_PCM_WAV:
            _LOG_FUNC_LINE();
#if FPGA==0
            app_overlay_select(APP_OVERLAY_WAV);
#endif
            g_rb_overlay_id = APP_OVERLAY_WAV;

            wav_codec_main(CODEC_LOAD);
            break;
#endif
        case AFMT_MPA_L1:
        case AFMT_MPA_L2:
        case AFMT_MPA_L3:
            _LOG_FUNC_LINE();
#if FPGA==0
            app_overlay_select(APP_OVERLAY_MPA);
#endif
            g_rb_overlay_id = APP_OVERLAY_MPA;
            mpa_codec_main(CODEC_LOAD);
            break;
			// TODO: add APP_OVERLAY_APE
#if 0
        case AFMT_APE:
#if FPGA==0
            app_overlay_select(APP_OVERLAY_APE);
#endif
            g_rb_overlay_id = APP_OVERLAY_APE;
            ape_codec_main(CODEC_LOAD);
            break;
		case AFMT_SBC:
#if FPGA==0
			app_overlay_select(APP_OVERLAY_A2DP);
#endif
			g_rb_overlay_id = APP_OVERLAY_A2DP;
			sbc_codec_main(CODEC_LOAD);
			break;
#endif
        default:
            _LOG_ERR("unkown codec type init\n");
            break;
    }
    _LOG_DBG("%s %d end\n",__func__ , song_format);

    return 0;
}

int rb_codec_loop_on(void)
{
    TRACE("E %s %d\n",__func__ , song_format);
#ifdef __TWS__
    //set start transfer to slave
    tws_local_player_set_tran_2_slave_flag(1);
#endif
    switch (song_format) {
#if 0
        case AFMT_FLAC:
            flac_codec_run();
            break;
        case AFMT_PCM_WAV:
            _LOG_FUNC_LINE();
            wav_codec_run();
            break;
        case AFMT_APE:
            ape_codec_run();
            break;
#endif
        case AFMT_MPA_L1:
        case AFMT_MPA_L2:
        case AFMT_MPA_L3:
            _LOG_FUNC_LINE();
            mpa_codec_run();
            break;
#if 0
		case AFMT_SBC:
			sbc_codec_run();
			break;
#endif
        default:
            _LOG_ERR("unkown codec type run\n");
            break;
    }
    return 0;
}

static int rb_thread_process_evt(RB_CTRL_CMD_T evt)
{

    TRACE("rb_thread_process_evt %d", evt);

    switch(evt) {
        case RB_CTRL_CMD_CODEC_INIT:
            rb_decode_halt_flag = 0;

            rb_play_init(NULL);

            /* get id3 */
            /* init ci info */
            ci->filesize = filesize(song_fd);
            ci->id3 = current_id3;
            ci->curpos = 0;

            dsp_configure(ci->dsp, DSP_RESET, 0);
            dsp_configure(ci->dsp, DSP_FLUSH, 0);

            rb_codec_init_desc();
            break;
        case RB_CTRL_CMD_CODEC_RUN:
            rbplay_loop_on = 1;
            app_sysfreq_req(APP_SYSFREQ_USER_APP_3, rb_player_get_work_freq());
            app_auto_power_off_timer(false);
			rb_codec_loop_on();
            app_auto_power_off_timer(true);
			song_fd = 0;
            rb_decode_halt_flag = 1;
            if(thread_tid_waiter) {
                rb_player_sync_close_done();
            } else {
                rb_thread_send_status_change();
                rb_thread_send_switch(true);
            }
#ifdef __TWS__
            //should update codec info after play one music
            codec_info.update_codec_info = 1;
#endif 
            app_sysfreq_req(APP_SYSFREQ_USER_APP_3, APP_SYSFREQ_32K);
            rbplay_loop_on = 0;
            TRACE("rb_thread_process_evt song play end fd=%d,tid 0x%x\n",song_fd,thread_tid_waiter);
            break;
        default:
            _LOG_ERR("unkown rb cmd %d\n",evt);
            break;
    }
    TRACE("rb_thread_process_evt %d done", evt);

    return 0;
}


static int rb_decode_mailbox_init(void)
{
    rb_decode_mailbox = osMailCreate(osMailQ(rb_decode_mailbox), NULL);
    if (rb_decode_mailbox == NULL)  {
        TRACE("Failed to Create rb_decode_mailbox\n");
        return -1;
    }
    return 0;
}

int rb_decode_mailbox_put(RBTHREAD_MSG_BLOCK* msg_src)
{
    osStatus status;

    RBTHREAD_MSG_BLOCK *msg_p = NULL;

    msg_p = (RBTHREAD_MSG_BLOCK*)osMailAlloc(rb_decode_mailbox, 0);
    if(!msg_p) {
        TRACE("%s fail, evt:%d,arg=%d \n",__func__,msg_src->evt,msg_src->arg);
        return -1;
    }

    msg_p->evt = msg_src->evt;
    msg_p->arg = msg_src->arg;

    status = osMailPut(rb_decode_mailbox, msg_p);

    return (int)status;
}

int rb_decode_mailbox_free(RBTHREAD_MSG_BLOCK* msg_p)
{
    osStatus status;

    status = osMailFree(rb_decode_mailbox, msg_p);

    return (int)status;
}

int rb_decode_mailbox_get(RBTHREAD_MSG_BLOCK** msg_p)
{
    osEvent evt;
    evt = osMailGet(rb_decode_mailbox, osWaitForever);
    if (evt.status == osEventMail) {
        *msg_p = (RBTHREAD_MSG_BLOCK *)evt.value.p;
        return 0;
    }
    return -1;
}

static void rb_decode_thread(void const *argument)
{
    RB_CTRL_CMD_T action;
    RBTHREAD_MSG_BLOCK* msg_p;

//    hal_sdmmc_set_dma_tid();

    while(1) {
        app_sysfreq_req(APP_SYSFREQ_USER_APP_3, APP_SYSFREQ_32K);
        //  evt = osSignalWait(0, osWaitForever);
        if(0 == rb_decode_mailbox_get(&msg_p)) {
            app_sysfreq_req(APP_SYSFREQ_USER_APP_3, APP_SYSFREQ_104M);

            action = (RB_CTRL_CMD_T) msg_p->evt;
            rb_caller_tid = (osThreadId) msg_p->arg ;

            TRACE("[%s] action:%d ,tid,0x%x", __func__,  action,rb_caller_tid);
            rb_thread_process_evt(action);

            rb_decode_mailbox_free(msg_p);
            if( rb_caller_tid)
                osSignalSet(rb_decode_tid, 0x1203);
            rb_caller_tid = NULL;
        }

        //signals = evt.value.signals;
        //osSignalClear(rb_decode_tid,signals);

        //action = rb_play_action;
        //rb_play_action = RB_CTRL_CMD_NONE;

        // if(evt.status == osEventSignal) {
        // } else {
        //     TRACE(" %s unknow evt:%d",__func__,evt.status);
        // }

    }
}

int app_rbplay_open(void)
{
    _LOG_DBG("%s \n",__func__);

    rb_decode_mailbox_init();

    if(rb_decode_tid == NULL) {
        rb_decode_tid = osThreadCreate(osThread(rb_decode_thread), NULL);
        if (rb_decode_tid == NULL)  {
            _LOG_ERR("Failed to Create rb_thread \n");
            return -1;
        }
    }

    return 0;
}

int rb_codec_running(void)
{
    return ((rb_decode_halt_flag == 0)?1:0);
}

void rb_codec_set_halt(int halt)
{
    rb_decode_halt_flag = halt;
}


void rb_thread_set_decode_vars(int fd, int type ,void* id3)
{
    song_fd =fd;
    song_format = type;
    current_id3 = (struct mp3entry *)id3;
}
void rb_player_sync_set_wait_thread(osThreadId tid)
{
    if(rbplay_loop_on)
        thread_tid_waiter = tid;
    else
        thread_tid_waiter = NULL;
}

void rb_player_sync_wait_close(void )
{
    // thread_tid_waiter = osThreadGetId();
    _LOG_DBG("%s the waiter 0x%x\n",__func__,thread_tid_waiter);
//    osSignalWait(0x8080,0x8000);
    while(NULL != thread_tid_waiter) {
        osThreadYield();
    }
    _LOG_DBG("%s end\n",__func__);
}

void rb_player_sync_close_done(void)
{
    _LOG_DBG("%s the waiter 0x%x\n",__func__,thread_tid_waiter);
    //osSignalSet(thread_tid_waiter,0x8080);
    thread_tid_waiter = NULL;
}
enum APP_SYSFREQ_FREQ_T rb_player_get_work_freq(void)
{
    enum APP_SYSFREQ_FREQ_T  freq;

    hal_sysfreq_print();

    TRACE("%s bitrate:%d freq:%d\n",__func__,ci->id3->bitrate,ci->id3->frequency);
#ifndef __TWS__
    enum AUD_SAMPRATE_T sample_rate = AUD_SAMPRATE_44100;
    sample_rate =(enum AUD_SAMPRATE_T ) ci->id3->frequency;
    if(sample_rate > AUD_SAMPRATE_48000)
        freq = APP_SYSFREQ_208M;
    else if (sample_rate > AUD_SAMPRATE_44100)
        freq = APP_SYSFREQ_104M;
    else
        freq = APP_SYSFREQ_52M;

    if(ci->id3->bitrate > 192)
        freq = APP_SYSFREQ_208M;
    else if (ci->id3->bitrate > 128)
        freq = APP_SYSFREQ_104M;
    else
        freq = APP_SYSFREQ_52M;

    switch( song_format ) {
        case AFMT_APE:
            freq = APP_SYSFREQ_208M;
            break;
        case AFMT_FLAC:
            freq = APP_SYSFREQ_208M;
            break;
        case AFMT_PCM_WAV:
            freq = APP_SYSFREQ_208M;
            break;
        default:
            break;
    }
#else
    freq = APP_SYSFREQ_208M;
#endif
    TRACE("%s will run @ 0x%xMHz \n",__func__,freq);
    return freq;
}


void rb_pcm_player_open_with_ci(void)
{
    enum AUD_CHANNEL_NUM_T channel_num = AUD_CHANNEL_NUM_2;
    enum AUD_SAMPRATE_T sample_rate = AUD_SAMPRATE_44100;
    enum AUD_BITS_T bits = AUD_BITS_16;

    TRACE("%s bitrate:%d freq:%d\n",__func__,ci->id3->bitrate,ci->id3->frequency);

    sample_rate =(enum AUD_SAMPRATE_T ) ci->id3->frequency;
	if(sample_rate == 0)
	sample_rate = AUD_SAMPRATE_44100;

    rb_pcm_player_open(bits,sample_rate,channel_num,rb_ctl_get_vol());
}


void osDelay200(void)
{
    osDelay(10);
}

