#ifndef __APP_FACTORY_H__
#define __APP_FACTORY_H__

#define APP_FACTORY_TRACE(s,...) TRACE(s, ##__VA_ARGS__)

void app_factorymode_result_set(bool result);

void app_factorymode_result_clean(void);

bool app_factorymode_result_wait(void);

void app_factorymode_enter(void);

void app_factorymode_key_init(void);

int app_factorymode_init(uint32_t factorymode);

int app_factorymode_calib_only(void);

int app_factorymode_languageswitch_proc(void);

#endif
