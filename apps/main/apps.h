#ifndef __APPS_H__
#define __APPS_H__

#include "app_status_ind.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "plat_types.h"

enum APP_SYSTEM_STATUS_T {
    APP_SYSTEM_STATUS_INVALID = 0,
    APP_SYSTEM_STATUS_NORMAL_OP,    
    APP_SYSTEM_STATUS_POWERON_PROC,
    APP_SYSTEM_STATUS_POWEROFF_PROC,
};

int app_init(void);

int app_deinit(int deinit_case);

int app_shutdown(void);

int app_reset(void);

uint8_t app_system_status_get(void);

uint8_t app_system_status_set(int flag);

int app_status_battery_report(uint8_t level);

int app_voice_report( APP_STATUS_INDICATION_T status,uint8_t device_id);

/*FixME*/
void app_status_set_num(const char* p);

////////////10 second tiemr///////////////
#define APP_PAIR_TIMER_ID    0
#define APP_POWEROFF_TIMER_ID 1

#ifdef __MOBILE_CONNECTED_AND_NO_AUDIO_AUDO_POWER_OFF_//Modified by ATX : Parker.Wei_20180324
#define APP_NO_AUDIO_TIMER_ID 2 
#endif

void app_stop_10_second_timer(uint8_t timer_id);
void app_start_10_second_timer(uint8_t timer_id);




////////////////////
//Modified by ATX : cc_20180326
bool app_clear_phone_paired_record(void);
//Modified by ATX : Leon.He_20180109: support reset PDL
void app_reset_paired_device_list(void);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
uint8_t app_get_fake_poweroff_flag(void);
void app_set_fake_poweroff_flag(uint8_t state);
int app_poweron_from_fake_poweroff(void);
void app_fake_poweroff(void);
#endif
#ifdef __cplusplus
}
#endif
#endif//__FMDEC_H__
