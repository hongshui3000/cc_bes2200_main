/*
Copyright (c) ATX ShenZhen, Ltd.
*/
#ifndef __APP_CUSTOMIZED_KEY_HANDLE_H__
#define __APP_CUSTOMIZED_KEY_HANDLE_H__
#include "app_key.h"

#ifdef __cplusplus
extern "C" {
#endif

void bt_key_handle_func_key(uint16_t event);
bool bt_key_handle_local_func_key(uint16_t event);
void bt_key_handle_up_key(enum APP_KEY_EVENT_T event);
void bt_key_handle_down_key(enum APP_KEY_EVENT_T event);
#ifdef __EXTRA_KEY_FOR_PRODUCT_LINE_
void factory_key_combo_handle(enum APP_KEY_EVENT_T event);
void factory_key_handle(enum APP_KEY_EVENT_T event);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __APP_CUSTOMIZED_KEY_HANDLE_H__ */
