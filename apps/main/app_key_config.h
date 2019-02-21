/*
Copyright (c) ATX ShenZhen, Ltd.
*/

#ifndef __APP_KEY_CONFIG_H__
#define __APP_KEY_CONFIG_H__
#include "app_key.h"

#ifdef __HALL_CONTROL_POWER_
#if defined(__SINGLE_KEY_)
#define KEY_HANDLE_NUM		7
#elif defined(_PROJ_2000IZ_C002_)||defined(_PROJ_2000IZ_C003_KEY_) //Modified by ATX : Parke.Wei_20180326
#define KEY_HANDLE_NUM		10
#elif defined(__SINGLE_TOUCH_SIMPLE_MODE_)
#define KEY_HANDLE_NUM		5
#elif defined(__SINGLE_TOUCH_SIMPLE_MODE_WITH_TRICPLE_)
#ifdef __FACTORY_KEY_SUPPORT_
#define KEY_HANDLE_NUM		9
#else
#define KEY_HANDLE_NUM		6
#endif //__FACTORY_KEY_SUPPORT_
#elif  defined(__TWO_KEYS_)
#define KEY_HANDLE_NUM		5 //Modified by ATX : Parke.Wei_20180322
#elif  defined(__THREE_KEYS_)
#define KEY_HANDLE_NUM		8
#endif
#else//__HALL_CONTROL_POWER_
#if defined(__SINGLE_KEY_)
#define KEY_HANDLE_NUM		7
#elif  defined(__TWO_KEYS_)
#define KEY_HANDLE_NUM		8
#elif  defined(__THREE_KEYS_)
#define KEY_HANDLE_NUM		11
#endif
#endif//__HALL_CONTROL_POWER_

extern const APP_KEY_HANDLE app_key_handle_cfg[KEY_HANDLE_NUM];

#endif//__APP_KEY_CONFIG_H__
