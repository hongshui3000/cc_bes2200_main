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

#define _LOG_TAG "[sdplay] "
#undef __attribute__(a)
#define RBPLAY_DEBUG 1
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

/* Rb codec Intface Implements */


#include "SDFileSystem.h"

#include "rb_ctl.h"

extern "C" {
    extern  void pmu_sd_config_ldo(void);
    extern  void hal_cmu_turn_usbpll(bool on);
    extern  void osDelay200(void);

}
static const WCHAR sd_playlist_name[]= {'P','l','a','y','l','i','s','t','\0'};

extern void iomux_sdmmc(void);
extern void app_rbplay_enter_sd_and_play(void);
extern  void bt_change_to_iic(APP_KEY_STATUS *status, void *param);
extern void sd_hotplug_detect_open(void (* cb)(uint8_t));
extern void rb_play_scan_sd_files(void);
extern void rb_play_codec_parse_file(uint16_t);
extern void rb_player_pause_oper(void );
extern void rb_player_resume_oper(void );
extern void rb_player_key_volkey(bool inc) ;
extern void rb_player_key_stop(void);
void app_rbplay_store_playlist(playlist_item* item);
int app_rbplay_del_file(const char* fname);

extern uint16_t g_rbplayer_curr_song_idx ;
static int sd_device_on = 1;
const char sd_label[] = "sd";

SDFileSystem *sdfs = NULL;//("sd");

extern unsigned short  rb_audio_file[128];
static struct mp3entry current_id3;

extern playlist_struct  sd_playlist ;
playlist_item  sd_curritem;
playlist_item* sd_curritem_p ;

bool app_sd_is_inserted( void)
{
    return (sd_device_on==1);
}

bool app_sdfs_is_created( void)
{
    return (sdfs != NULL) ;
}

void app_sdfs_destory(void)
{
    if (sdfs)
        delete sdfs;
    sdfs = NULL;   
}
static int app_sd_post_msg(void)
{
    int ret;
    APP_MESSAGE_BLOCK msg;

    msg.mod_id = APP_MODUAL_SD;
    msg.msg_body.message_id = 0;
    msg.msg_body.message_ptr = (uint32_t)NULL;
    ret = app_mailbox_put(&msg);

    TRACE("%s  sd_device_on  %d ret %d\n",__func__,sd_device_on,ret);

    return 0;
}

void sd_hotplug_detect_callback(uint8_t  s)
{
    _LOG_DBG("%s s:%d\n",__func__,s);
    sd_device_on = s;
    app_sd_post_msg();
}

static int app_sd_handle_msg(APP_MESSAGE_BODY *msg_body)
{
    TRACE("%s  sd_device_on  %d \n",__func__,sd_device_on);
    if(sd_device_on) {
        if(!sdfs)
            sdfs = new SDFileSystem(sd_label);     
    }
    return 0;
}

void app_rbplay_sd_removed(void)
{
    TRACE("%s !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",__func__);
    if(sdfs)
        delete sdfs;
    sdfs = NULL;   
}
int app_rbplay_sd_open(void)
{
    _LOG_DBG("%s \n",__func__);

    //sd_hotplug_detect_open(NULL);//sd_hotplug_detect_callback);

    if(!sdfs) {
        sd_device_on = 1;
        sdfs = new SDFileSystem(sd_label);
    }
    _LOG_DBG("%s sdfs= %x\n",__func__,sdfs);
	sd_curritem_p = &sd_curritem;

    return 0;
}

int app_rbplay_sd_close(void)
{
//    app_set_threadhandle(APP_MODUAL_SD, NULL);
    return 0;
}

bool app_rbplay_check_sd_status(void)
{
    DIR *d;

    if(sd_device_on != 1) {
        TRACE("sd card not exist!!!!!!!!!!!!!!!!!\n");
        return false;
    }
    
    if(!sdfs) {
        TRACE("sdfs new  failed!!!!!!!!!!!!!!!!!\n");
        return false;
    }

    d = opendir("/sd");

    if (!d) {
        TRACE("sd file system open sd fail  xxxxxxxxxxxxxxx\n");
        return false;
    }

    closedir(d);

    return true;
}

void app_rbplay_set_play_name(const char* fname)
{
    memcpy(rb_audio_file,fname,sizeof(rb_audio_file));
}
void app_rbplay_reopen_file(int *dest_fd)
{
    if(*dest_fd) {
        close(*dest_fd);
        *dest_fd = open((const char *) rb_audio_file,O_RDWR);
    }
}

extern uint32_t  hal_sys_timer_get(void);

bool app_rbplay_get_playlist(playlist_struct *list)
{
    class FileHandle *fh;
//always gen playlist .
	app_rbplay_del_file((const char *)sd_playlist_name);

    fh = sdfs->open((const char *)sd_playlist_name, O_RDONLY);

    if (fh <= 0) {
        TRACE("sd playlist not exist\n");
        return false;
    }

    memset(list, 0,sizeof(playlist_struct));
    memset(sd_curritem_p,0x0, sizeof(playlist_item));
    list->current_item = sd_curritem_p;
    list->curr_play_idx = 0;
    list->total_songs = fh->flen()/sizeof( playlist_item);
    TRACE(" playlist open ok %d, %d songs\n",fh->flen(),list->total_songs);
    fh->close();

    return true;
}

void app_rbplay_gen_playlist(playlist_struct *list)
{
    struct dirent *p;
    DIR *d;
    uint32_t total;
    int fd = 0;
    char* str;
    memset(list,0x0,sizeof(playlist_struct));

    d = opendir("/sd");
    if (!d) {
        TRACE("%s sd open fail \n",__func__);
        return;
    }

    TRACE("---------gen audio list---------\n");

    total = 0;
//check the audio file count
	const char root_path[] = "/sd/";

    while (p = readdir(d)) {
        if (rb_ctl_is_audio_file((char *)p->d_name)) {
//add to playlist
            //set the scan idx as idx
            memset(&sd_curritem,0x0,sizeof(playlist_item));
            sd_curritem.song_idx = total;
            //compose the file path for play
			sprintf((char *)sd_curritem.file_path, "%s%s", root_path, p->d_name);
			sprintf((char *)sd_curritem.file_name, "%s", p->d_name);

			TRACE("file: %s", sd_curritem.file_path);

            fd = open((const char *)sd_curritem.file_path, O_RDWR);

            if (fd <= 0) {
                TRACE(" %s error !!!!");
                return;
            }
            get_metadata(&current_id3, fd, (char *)sd_curritem.file_path);
            close(fd);
            if(current_id3.bitrate == 0 || current_id3.filesize == 0 ||current_id3.length == 0) {
                TRACE(" %s bits:%d,fz:%d,len:%d not vailid", __func__,current_id3.bitrate,current_id3.filesize,current_id3.length);
                //continue;
            }
            sd_curritem_p->bitrate    = current_id3.bitrate;
            sd_curritem_p->codectype  = current_id3.codectype;
            sd_curritem_p->filesize   = current_id3.filesize;
            sd_curritem_p->length     = current_id3.length;
            sd_curritem_p->frequency  = current_id3.frequency;

#ifdef PARSER_DETAIL
            str = current_id3.title;
            if(str != NULL) {
                TRACE(" %s title %s ",__func__,str);
				DUMP16(" %x ",str,10);
                memset(sd_curritem_p->title,0x0,MP3_TITLE_LEN);
                memcpy(sd_curritem_p->title ,str,strlen(str)>MP3_TITLE_LEN?MP3_TITLE_LEN:strlen(str));
            }

            str = current_id3.artist;
            if(str != NULL) {
                TRACE(" %s artist %s ",__func__,str);
				DUMP16(" %x ",str,10);
                memset(sd_curritem_p->artist,0x0,MP3_ARTIST_LEN);
                memcpy(sd_curritem_p->artist ,str,strlen(str)>MP3_ARTIST_LEN?MP3_ARTIST_LEN:strlen(str));
            }

            str = current_id3.album;
            if(str != NULL) {
                TRACE(" %s album %s ",__func__,str);
				DUMP16(" %x ",str,10);
                memset(sd_curritem_p->album,0x0,MP3_ALBUM_LEN);
                memcpy(sd_curritem_p->album ,str,strlen(str)>MP3_ALBUM_LEN?MP3_ALBUM_LEN:strlen(str));
            }

            str = current_id3.genre_string;
            if(str != NULL) {
                TRACE(" %s genre_string %s ",__func__,str);
				DUMP16(" %x ",str,10);
                memset(sd_curritem_p->genre,0x0,MP3_GENRE_LEN);
                memcpy(sd_curritem_p->genre ,str,strlen(str)>MP3_GENRE_LEN?MP3_GENRE_LEN:strlen(str));
            }

            str = current_id3.composer;
            if(str != NULL) {
                TRACE(" %s composer %s ",__func__,str);
				DUMP16(" %x ",str,10);
                memset(sd_curritem_p->composer,0x0,MP3_COMPOSER_LEN);
                memcpy(sd_curritem_p->composer ,str,strlen(str)>MP3_COMPOSER_LEN?MP3_COMPOSER_LEN:strlen(str));
            }
#endif
            app_rbplay_store_playlist(sd_curritem_p);
            //memcpy(&list->items[total],&sd_curritem_p ,sizeof(playlist_item));
            total++;
        }

        if(total >= MAX_SONGS) {
            TRACE("%s too many songs on the devices",__func__);
            break;
        }
    }
    list->total_songs = total ;
    list->current_item = sd_curritem_p;
    closedir(d);
    TRACE("---------%d audio file searched---------\n" , total);

}

void app_rbplay_store_playlist(playlist_item* item)
{
    class FileHandle *fh;

    int flag = O_RDWR;
	
    fh = sdfs->open((const char *)sd_playlist_name, flag);

    if( fh == NULL ) {
        TRACE(" %s File not exist ,creat it \n",__func__);
        fh = sdfs->open((const char *)sd_playlist_name, O_CREAT|O_RDWR);
    }

    if (fh == NULL) {
        TRACE(" %s sd playlist can not create \n",__func__);
        return ;
    }
    fh->lseek( (item->song_idx)*sizeof(playlist_item),SEEK_SET);
    fh->write((const void*)item,sizeof(playlist_item));

    TRACE(" %s w end!!!! cnt %d, fsize %d \n",__func__,item->song_idx,fh->flen());
    fh->close();
}

void app_rbplay_load_playlist(playlist_struct *list )
{
    if(!sdfs) {
        sd_device_on = 1;
        sdfs = new SDFileSystem(sd_label);
    }

    if( !app_rbplay_check_sd_status() ) {
        TRACE("%s find no sd devices.",__func__);
        return ;
    }
    //check if playlist exsit
    if( !app_rbplay_get_playlist(list) ) {
        //if no exist ,scan emmc to find out songs
        app_rbplay_gen_playlist(list);
    }

    if(sd_playlist.total_songs == 0) {
        app_rbplay_gen_playlist(list);
        //store playlist to emmc
    }
}

playlist_item *app_rbplay_get_playitem(uint16_t idx)
{
    class FileHandle *fh;
	uint16_t want_idx = idx;
    if(idx >= sd_playlist.total_songs ) {
        TRACE("%s error idx %d / %d",__func__,idx,sd_playlist.total_songs);
        return NULL;
    }

    if(sd_playlist.current_item == NULL ) {
        TRACE("%s error item %d / %d",__func__,idx,sd_playlist.total_songs);
        return NULL;
    }
    fh = sdfs->open((const char *)sd_playlist_name, O_RDONLY);

    if (fh == NULL) {
        TRACE(" %s sd playlist can not open \n",__func__);
        return NULL;
    }
	do {
	    fh->lseek(sizeof(playlist_item)*want_idx,SEEK_SET);
	    fh->read((void*)sd_playlist.current_item ,sizeof(playlist_item));
	    TRACE(" %s  idx: %d/%d ,song idx %d \n",__func__,want_idx,sd_playlist.total_songs,sd_playlist.current_item->song_idx);
		want_idx++;
		if(want_idx >= sd_playlist.total_songs )
		{
			want_idx = 0;
		}
		if(want_idx == idx)
		{
			fh->close();
			
			if(sd_playlist.current_item->song_idx == 0xffffffff)
				return NULL;
			else
				return sd_playlist.current_item;
		}
	}while(sd_playlist.current_item->song_idx == 0xffffffff);
	
    fh->close();

    return sd_playlist.current_item;
}

char *app_rbplay_get_playitem_name(uint16_t idx)
{
    playlist_item* pItem = app_rbplay_get_playitem(idx);
    if (NULL != pItem)
    {
        return (char *)pItem->file_path;
    }
    else
    {
        return NULL;
    }
}

unsigned long rb_ctl_get_song_playedms(void)
{
    return current_id3.elapsed;
}

SDFileSystem *app_rbplay_get_sdfs(void)
{
    return sdfs;
}

int app_rbplay_del_file(const char* fname)
{
	if(!sdfs) {
		TRACE(" %s fail no sdfs",__func__);
	}
	return sdfs->remove(fname);
}

int app_ctl_remove_file(uint32_t idx)
{
    class FileHandle *fh;
    playlist_item want_item;
	uint16_t idx16 = (uint16_t)idx;
	if(sd_playlist.total_songs <= idx)
	{
		TRACE("%s err idx %d/%d ",__func__,idx16,sd_playlist.total_songs);
		return -1;
	}
	if(!sdfs) {
		TRACE(" %s sdfs not exist.",__func__);
		return -1;
	}
    fh = sdfs->open((const char *)sd_playlist_name, O_RDWR);
    fh->lseek(sizeof(playlist_item)*idx16,SEEK_SET);
	
    fh->read((void*)&want_item ,sizeof(playlist_item));
	if(app_rbplay_del_file((const char *) want_item.file_path) == 0)
	{
		fh->lseek(sizeof(playlist_item)*idx16,SEEK_SET);
		want_item.song_idx = 0xffffffff;
		fh->write((const void*)&want_item,sizeof(playlist_item));
		fh->close();
		return 0;
	}
	
	fh->close();
	TRACE(" %s fail .",__func__);
	return -1;
}


