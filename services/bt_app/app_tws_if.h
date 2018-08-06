#ifndef APP_TWS_IF_H
#define APP_TWS_IF_H

#ifdef __cplusplus
#define EXTERN_C                        extern "C"
#else
#define EXTERN_C
#endif

uint8_t app_tws_get_mode(void);

A2dpStream* app_tws_get_sink_stream(void);



EXTERN_C char app_tws_mode_is_master(void);

EXTERN_C char app_tws_mode_is_slave(void);

char app_tws_mode_is_only_mobile(void);

EXTERN_C uint16_t app_tws_get_tws_conhdl(void);

void app_tws_set_tws_conhdl(uint16_t conhdl);

AvrcpChannel *app_tws_get_avrcp_TGchannel(void);

A2dpStream *app_tws_get_tws_source_stream(void);

bool app_tws_get_source_stream_connect_status(void);

uint8_t *app_tws_get_peer_bdaddr(void);

void app_tws_set_have_peer(bool is_have);

void app_tws_set_tws_mode(DeviceMode mode);

DeviceMode app_tws_get_tws_mode(void);

void app_tws_set_media_suspend(bool enable);

int app_tws_get_a2dpbuff_available(void);

void app_tws_set_eq_band_settings(int8_t *eq_band_settings);

void app_tws_get_eq_band_gain(float **eq_band_gain);

void app_tws_get_eq_band_init(void);
#if 0
uint16_t app_tws_sample_rate_hacker(uint16_t sample_rate);
#else
float app_tws_sample_rate_hacker(uint16_t sample_rate);
uint32_t tws_calc_sbc_frame_time_to_us(uint32_t totle_sample,uint16_t sample_rate);
#endif
void app_tws_reset_slave_medianum(void);
uint16_t app_tws_get_slave_medianum(void);



A2dpStream *app_tws_get_main_sink_stream(void);

EXTERN_C void app_tws_env_init(tws_env_cfg_t *cfg);

uint8_t app_tws_get_codec_type(void);
void app_tws_set_codec_type(uint8_t type);

uint8_t app_tws_get_tws_hfp_vol(void);

int tws_player_notify_key( unsigned short key,unsigned short event);

int tws_player_set_a2dp_vol( int8_t vol);
int tws_player_spp_send_rsp( uint16_t cmdid,uint8_t status);
//Modified by ATX : parker.wei_20180306 vol_up Event deal independently 
int tws_player_a2dp_vol_change( int8_t vol);

//Modified by ATX : parker.wei_20180308
int tws_player_hfp_vol_change( int8_t vol);
//Modified by ATX : Haorong.Wu_20180507
int tws_slave_voice_report(APP_STATUS_INDICATION_T id);

EXTERN_C void app_tws_buff_alloc(void);
EXTERN_C void app_tws_buff_free(void);
EXTERN_C uint8_t *app_tws_get_transmitter_buff(void);

int app_tws_get_big_buff(uint8_t **buf_p, uint32_t *size);
EXTERN_C void app_tws_set_pcm_wait_triggle(uint8_t triggle_en);
void app_tws_check_max_slot_setting(void);
#if defined(SLAVE_USE_ENC_SINGLE_SBC)
void app_tws_adjust_encode_bitpool(uint8_t updown);
void app_tws_reset_cadun_count(void);
void app_tws_kadun_count(void);
#endif


#ifdef __TWS_CALL_DUAL_CHANNEL__
#ifdef __cplusplus
extern "C" {
#endif

uint8_t app_tws_get_pcm_wait_triggle(void);

void app_tws_set_pcm_triggle(void);


void app_tws_set_btpcm_wait_triggle(uint8_t triggle_en);

uint8_t app_tws_get_btpcm_wait_triggle(void);
#ifdef __cplusplus
}
#endif
#endif

#endif

