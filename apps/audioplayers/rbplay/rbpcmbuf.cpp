/* rbpcmbuf source */
/* pcmbuf management & af control & mixer */
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
#include "app_utils.h"

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
#include "rbplay.h"
#include "rbpcmbuf.h"

extern 	uint32_t app_ring_merge_more_data( uint8_t *buf, uint32_t len );
extern  void rb_player_restore_overlay(void );

#undef __attribute__(a)
/* Internals */
#define _LOG_TAG "[rbpcmbuf] "

#define DEBUG_RBPCMBUF	1

#if DEBUG_RBPCMBUF
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

/* Internals */
#ifndef __TWS__
#define RB_PCMBUF_DMA_BUFFER_SIZE (1024*12)
#define RB_PCMBUF_MEDIA_BUFFER_SIZE (1024*12)
#define RB_PCMBUF_DECODE_BUFFER_SIZE (1024*8)
#else
#define RB_PCMBUF_DMA_BUFFER_SIZE (4)
#define RB_PCMBUF_MEDIA_BUFFER_SIZE (1024*2)
#define RB_PCMBUF_DECODE_BUFFER_SIZE (1024)
#endif
#define RB_PCMBUF_THR_TRIGGER_PLAY (RB_PCMBUF_MEDIA_BUFFER_SIZE)

static uint32_t rb_decode_out_buff[RB_PCMBUF_DECODE_BUFFER_SIZE/4];

uint8_t* _rb_pcmbuf_dma_buffer;

CQueue _rb_pcmbuf_media_buf_queue;
uint8_t* _rb_pcmbuf_media_buffer;
osMutexId _rb_media_buf_queue_mutex_id = NULL;
osMutexDef(_rb_media_buf_queue_mutex);

#define LOCK_MEDIA_BUF_QUEUE() \
		if(osErrorISR == osMutexWait(_rb_media_buf_queue_mutex_id, osWaitForever))	{\
			_LOG_DBG("%s LOCK_MEDIA_BUF_QUEUE from IRQ!!!!!!!\n",__func__);\
		}\

#define UNLOCK_MEDIA_BUF_QUEUE() \
		if(osErrorISR == osMutexRelease(_rb_media_buf_queue_mutex_id)){    \
			_LOG_DBG("%s UNLOCK_MEDIA_BUF_QUEUE from IRQ!!!!!!\n");     \
		}    \


RB_PCMBUF_AUD_STATE_T _rb_pcmbuf_audio_state = RB_PCMBUF_AUD_STATE_STOP;

#define _RB_PCMBUF_W_WAITER_SIGNAL (0xcc)
#define _RB_PCMBUF_R_WAITER_SIGNAL (0xcb)
#define _RB_PCMBUF_LOCK_SIGNAL (0xca)
#define _RB_PCMBUF_LOCK_WAITER_SIGNAL (0xc9)
osThreadId _rb_pcmbuf_w_waiter_tid = NULL;
osThreadId _rb_pcmbuf_r_waiter_tid = NULL;
osThreadId _rb_pcmbuf_thread_locked_waiter_tid = NULL;
static bool rb_pcmbuf_pause_flag = false;

static bool rb_codec_wait_status = false;
static bool rb_player_resume_enable = false;
static bool rb_player_pause_status = false;

void rb_ctl_wait_lock_thread(bool lock)
{
	TRACE(" [%s] %d ",__func__,lock);
	rb_pcmbuf_pause_flag = lock;
	if(!lock) {
		osSignalSet(_rb_pcmbuf_w_waiter_tid, _RB_PCMBUF_LOCK_SIGNAL);
	} else {
        _rb_pcmbuf_thread_locked_waiter_tid = osThreadGetId();
		osSignalClear(_rb_pcmbuf_thread_locked_waiter_tid, _RB_PCMBUF_LOCK_WAITER_SIGNAL);
		osSignalWait(_RB_PCMBUF_LOCK_WAITER_SIGNAL, 60);
	}
}

static bool rb_pcmbuf_thread_locked(void)
{
//	TRACE(" [%s] %d ",__func__,rb_pcmbuf_pause_flag);

	return rb_pcmbuf_pause_flag;
}

static void rb_pcmbuf_release_lock_waiter(void)
{
	TRACE(" [%s] %d ",__func__,rb_pcmbuf_pause_flag);

	osSignalSet(_rb_pcmbuf_thread_locked_waiter_tid, _RB_PCMBUF_LOCK_WAITER_SIGNAL);
}

bool rb_pcmbuf_suspend_play_loop(void)
{
	return rb_codec_wait_status;
}

void _rb_pcmbuf_w_waiter_wait(void)
{
    osEvent ret;
    if ( _rb_pcmbuf_w_waiter_tid == NULL )
    {
        _rb_pcmbuf_w_waiter_tid = osThreadGetId();
		_LOG_DBG("%s _rb_pcmbuf_w_waiter_tid 0x%x !!!!!!!\n",__func__,_rb_pcmbuf_w_waiter_tid);
	}
	if( _rb_pcmbuf_w_waiter_tid == NULL ) {
       _LOG_DBG("%s getid called from IRQ!!!!!!!\n",__func__);
	   return;
	}
	rb_codec_wait_status = true;
	if(rb_pcmbuf_thread_locked()){
		osSignalClear(_rb_pcmbuf_w_waiter_tid, _RB_PCMBUF_LOCK_SIGNAL);
		
		rb_pcmbuf_release_lock_waiter();
		
		osSignalWait(_RB_PCMBUF_LOCK_SIGNAL, osWaitForever);
	}
		
    ret = osSignalWait(_RB_PCMBUF_W_WAITER_SIGNAL, osWaitForever);
	rb_codec_wait_status = false;
	if (ret.status == osErrorISR) {
		_LOG_DBG("%s called from IRQ!!!!!!!\n",__func__);
	}
}

void _rb_pcmbuf_w_waiter_release(void)
{

    if (_rb_pcmbuf_w_waiter_tid == NULL ) {
	   return;
	}
    osSignalSet(_rb_pcmbuf_w_waiter_tid, _RB_PCMBUF_W_WAITER_SIGNAL);
}

void _rb_pcmbuf_r_waiter_wait(void)
{
	osEvent ret;

    _rb_pcmbuf_r_waiter_tid = osThreadGetId();

	if(_rb_pcmbuf_r_waiter_tid == NULL) {
       _LOG_DBG("%s get id called from IRQ!!!!!!!\n",__func__);
	   return;
	}
    ret = osSignalWait(_RB_PCMBUF_R_WAITER_SIGNAL, osWaitForever);
	if (ret.status == osErrorISR) {
		_LOG_DBG("%s called from IRQ!!!!!!!\n",__func__);
	}
}

void _rb_pcmbuf_r_waiter_release(void)
{
    if (_rb_pcmbuf_r_waiter_tid == NULL) {
		return;
	}
    osSignalSet(_rb_pcmbuf_r_waiter_tid, _RB_PCMBUF_R_WAITER_SIGNAL);
}

void _rb_pcmbuf_media_buff_mutex_release(void)
{
	if(_rb_media_buf_queue_mutex_id)
		osMutexRelease(_rb_media_buf_queue_mutex_id);
}

uint32_t _rb_pcmbuf_playback_callback(uint8_t *buf, uint32_t len)
{
    char *e1 = 0;
    char *e2 = 0;
    unsigned int len1 = 0;
    unsigned int len2 = 0;

    LOCK_MEDIA_BUF_QUEUE();
    PeekCQueue(&_rb_pcmbuf_media_buf_queue, len, (CQItemType **)&e1, &len1, (CQItemType **)&e2, &len2);
    UNLOCK_MEDIA_BUF_QUEUE();

	//TRACE(" %s %d/%d ",__func__,len,LengthOfCQueue(&_rb_pcmbuf_media_buf_queue));
    if((len1  + len2) >= len) {
		if(len1 >= len) {
			memcpy(buf, e1, len);
			LOCK_MEDIA_BUF_QUEUE();
			DeCQueue(&_rb_pcmbuf_media_buf_queue, 0, len);
			UNLOCK_MEDIA_BUF_QUEUE();
			goto exit;
		}
		memcpy(buf, e1, len1);
        memcpy(buf + len1, e2, len-len1);
		LOCK_MEDIA_BUF_QUEUE();
		DeCQueue(&_rb_pcmbuf_media_buf_queue, 0, len);
		UNLOCK_MEDIA_BUF_QUEUE();
	} else {
		 _LOG_DBG("%s some dma need wait but make play end problem! \n",__func__);
		// _rb_pcmbuf_r_waiter_wait();
		memset(buf,0x0,len);   // goto try_again;
	}

exit:
	_rb_pcmbuf_w_waiter_release();


    return len;
}

/* APIs */
void rb_pcmbuf_init(void)
{
	_LOG_DBG("pcmbuff init");
	if(!_rb_media_buf_queue_mutex_id)
	    _rb_media_buf_queue_mutex_id  = osMutexCreate((osMutex(_rb_media_buf_queue_mutex)));
	/* sbc queue*/
		/* sbc queue*/
#ifndef __TWS__
	app_audio_mempool_init();

	app_audio_mempool_get_buff(&_rb_pcmbuf_media_buffer, RB_PCMBUF_MEDIA_BUFFER_SIZE);
#endif
	memset(_rb_pcmbuf_media_buffer, 0, RB_PCMBUF_MEDIA_BUFFER_SIZE);
	InitCQueue(&_rb_pcmbuf_media_buf_queue, RB_PCMBUF_MEDIA_BUFFER_SIZE, (unsigned char *)_rb_pcmbuf_media_buffer);
#ifndef __TWS__
	app_audio_mempool_get_buff(&_rb_pcmbuf_dma_buffer, RB_PCMBUF_DMA_BUFFER_SIZE);
#endif
	memset(_rb_pcmbuf_dma_buffer, 0, RB_PCMBUF_DMA_BUFFER_SIZE);

    _rb_pcmbuf_audio_state = RB_PCMBUF_AUD_STATE_STOP;

	memset(rb_decode_out_buff,0, RB_PCMBUF_DECODE_BUFFER_SIZE);

	_LOG_DBG("%s  _rb_pcmbuf_audio_state %d\n", __func__, _rb_pcmbuf_audio_state);
}
void *rb_pcmbuf_request_buffer(int *size)
{
	*size = RB_PCMBUF_DECODE_BUFFER_SIZE/4;

	return rb_decode_out_buff;


}

void rb_pcm_player_open(enum AUD_BITS_T bits,enum AUD_SAMPRATE_T sample_rate,enum AUD_CHANNEL_NUM_T channel_num,uint8_t vol) {
	struct AF_STREAM_CONFIG_T stream_cfg;

	memset(&stream_cfg, 0, sizeof(stream_cfg));

	stream_cfg.bits = bits;
	stream_cfg.channel_num = channel_num;
	stream_cfg.sample_rate = sample_rate;
	stream_cfg.device = AUD_STREAM_USE_INT_CODEC;
	stream_cfg.io_path = AUD_OUTPUT_PATH_SPEAKER;
	stream_cfg.vol = vol;
	stream_cfg.handler = _rb_pcmbuf_playback_callback;
	stream_cfg.data_ptr = _rb_pcmbuf_dma_buffer;
	stream_cfg.data_size = RB_PCMBUF_DMA_BUFFER_SIZE;

	af_stream_open(AUD_STREAM_ID_0,AUD_STREAM_PLAYBACK, &stream_cfg);
	af_stream_start(AUD_STREAM_ID_0,AUD_STREAM_PLAYBACK);
}

void rb_pcmbuf_write(unsigned int size)
{
    int len = 0;
	int ret ;
wait_space:
	if(rb_codec_running() == 0) {
		_LOG_DBG("%s stop!",__func__);
		_rb_pcmbuf_r_waiter_release();
		return ;
	}

    LOCK_MEDIA_BUF_QUEUE();
    ret = EnCQueue(&_rb_pcmbuf_media_buf_queue, (CQItemType *)rb_decode_out_buff, size*(2*2));
  //  len = LengthOfCQueue(&_rb_pcmbuf_media_buf_queue);
    UNLOCK_MEDIA_BUF_QUEUE();
#ifndef __TWS__

	if(CQ_ERR == ret) {
        _rb_pcmbuf_w_waiter_wait();
		goto wait_space;
	}

    if (1){//len >= RB_PCMBUF_THR_TRIGGER_PLAY) {
        if (_rb_pcmbuf_audio_state == RB_PCMBUF_AUD_STATE_STOP) {

            rb_pcm_player_open_with_ci();

            _rb_pcmbuf_audio_state = RB_PCMBUF_AUD_STATE_START;
        }
    }
#else
    if(_rb_pcmbuf_audio_state != RB_PCMBUF_AUD_STATE_START){
        
        _rb_pcmbuf_audio_state = RB_PCMBUF_AUD_STATE_START;
    }
#endif
    
    _rb_pcmbuf_r_waiter_release();

}

void rb_player_set_resume_flag(bool en) 
{
	rb_player_resume_enable = en;
}

bool rb_player_get_resume_flag(void) 
{
	return rb_player_resume_enable ;
}

void rb_player_pause_oper(void ) 
{
	_LOG_DBG("%s st:%d ",__func__,rb_player_pause_status);
	if(rb_player_pause_status)
		return ;
	
	af_stream_stop(AUD_STREAM_ID_0,AUD_STREAM_PLAYBACK);
	_LOG_DBG(" af	stream close \n");
	af_stream_close(AUD_STREAM_ID_0,AUD_STREAM_PLAYBACK);
//wait decoder suspend
	//osDelay(200);
	while(!rb_codec_wait_status)
	{
		osThreadYield();
	}
	rb_player_pause_status = true;
	rb_player_set_resume_flag(false);
	_LOG_DBG("%s sucessed ",__func__);
}

void rb_player_resume_oper(void ) 
{
	_LOG_DBG("%s st:%d ",__func__,rb_player_pause_status);
//reset buff
	if(!rb_player_pause_status)
		return ;
	
	rb_pcmbuf_init();
	rb_player_restore_overlay();
	rb_player_pause_status = false;
	_rb_pcmbuf_w_waiter_release();
	rb_player_set_resume_flag(true);
}

void rb_player_pause_control(void) 
{
	_LOG_DBG(" %s resume en?%d, pause stat:%d",__func__, rb_player_resume_enable, rb_player_pause_status);
	if(rb_player_resume_enable&& !rb_player_pause_status) {
		rb_player_pause_oper();
	} else if(rb_player_resume_enable&& rb_player_pause_status) {
		rb_player_resume_oper();
	}
}

void rb_player_toggle_suspend_resume(void) 
{
	TRACE("%s resume flag:%d",__func__,rb_player_get_resume_flag());
	if(rb_player_get_resume_flag()) {
		rb_player_pause_oper();
	} else {
		rb_player_resume_oper();
	}
}

