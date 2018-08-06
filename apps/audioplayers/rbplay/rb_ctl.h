/* rbplay source */
/* playback control & rockbox codec porting & codec thread */
#ifndef __RB_CTL_H__
#define __RB_CTL_H__

//playlist
#define MAX_SONGS (1000)
#define FILE_PATH_LEN (128+4)
#define FILE_SHORT_NAME_LEN (14)
#define MP3_TITLE_LEN (128)
#define MP3_ARTIST_LEN (128)
#define MP3_ALBUM_LEN (128)
#define MP3_GENRE_LEN (128)
#define MP3_COMPOSER_LEN (128)
#define MP3_YEAR_STRING_LEN (8)
#define MP3_TRACK_LEN  (128)

typedef struct {
    uint16_t file_path[FILE_PATH_LEN];
    uint16_t file_name[FILE_SHORT_NAME_LEN];
#ifdef PARSER_DETAIL
	uint16_t  title[MP3_TITLE_LEN];
	uint16_t  artist[MP3_ARTIST_LEN];
	uint16_t  album[MP3_ALBUM_LEN];
	uint16_t  genre[MP3_GENRE_LEN];
	uint16_t  composer[MP3_COMPOSER_LEN];
	uint16_t  year[MP3_YEAR_STRING_LEN];
	uint16_t  track[MP3_TRACK_LEN];
#endif
    uint16_t song_idx;

    unsigned int codectype;
    unsigned int bitrate;
    unsigned long frequency;

    unsigned long filesize; /* without headers; in bytes */
    unsigned long length;   /* song length in ms */
} playlist_item;

typedef struct {
    playlist_item *current_item;
    uint16_t      total_songs;
	uint16_t      curr_play_idx;
} playlist_struct;


typedef enum {
    RB_MODULE_EVT_NONE,
	RB_MODULE_EVT_PLAY,
	RB_MODULE_EVT_PLAY_IDX,
    RB_MODULE_EVT_STOP,
    RB_MODULE_EVT_SUSPEND,
    RB_MODULE_EVT_RESUME,
    RB_MODULE_EVT_PLAY_NEXT,
	RB_MODULE_EVT_CHANGE_VOL,
	RB_MODULE_EVT_SET_VOL,
	RB_MODULE_EVT_CHANGE_IDLE,
	RB_MODULE_EVT_DEL_FILE,

	RB_MODULE_EVT_RESTORE_DUAL_PLAY,
	RB_MODULE_EVT_LINEIN_START,
	RB_MODULE_EVT_RECONFIG_STREAM,
	RB_MODULE_EVT_SET_TWS_MODE,

    SBCREADER_ACTION_NONE,
    SBCREADER_ACTION_INIT,
    SBCREADER_ACTION_RUN,
    SBCREADER_ACTION_STOP,

	SBC_RECORD_ACTION_START,
	SBC_RECORD_ACTION_DATA_IND,
	SBC_RECORD_ACTION_STOP,

    RB_MODULE_EVT_MAX
} RB_MODULE_EVT;

typedef enum {
    RB_CTL_IDLE,
    RB_CTL_PLAYING,
    RB_CTL_SUSPEND,
} rb_ctl_status;

typedef struct {
    struct mp3entry song_id3;
    char rb_fname[FILE_SHORT_NAME_LEN];
    rb_ctl_status status ; //playing,idle,pause,
    uint16_t rb_audio_file[FILE_PATH_LEN];
    uint16_t curr_song_idx ;
    uint8_t rb_player_vol ;
    int     file_handle;
	int 	init_done;
	BOOL	sbc_record_on;
} rb_ctl_struct;

extern  int app_rbmodule_post_msg(RB_MODULE_EVT evt,uint32_t arg);
extern int rb_ctl_is_audio_file(const char* file_path);

#endif

