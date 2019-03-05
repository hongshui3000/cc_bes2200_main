/*
Copyright (c) ATX ShenZhen, Ltd.
*/

#include "cmsis_os.h"
#include "app_key_config.h"
#include "hal_trace.h"

extern void app_bt_key_shutdown(APP_KEY_STATUS *status, void *param);
extern void app_bt_key(APP_KEY_STATUS *status, void *param);
extern void app_bt_key_enter_testmode(APP_KEY_STATUS *status, void *param);
extern void app_test_key_handler(APP_KEY_STATUS *status, void *param);

#ifdef __TOUCH_KEY__
extern "C" void touch_key_handler(APP_KEY_STATUS *key_status, void *param);
#endif


#ifdef __HALL_CONTROL_POWER_
#if defined(__SINGLE_KEY_)
extern const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
};
#elif defined(_PROJ_2000IZ_C002_)||defined(_PROJ_2000IZ_C003_KEY_) //Modified by ATX : Parke.Wei_20180326
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN2,APP_KEY_EVENT_CLICK},"bt volume up key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN2,APP_KEY_EVENT_RAMPAGECLICK},"bt play backward key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN3,APP_KEY_EVENT_DOUBLECLICK},"bt volume down key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN3,APP_KEY_EVENT_CLICK},"bt volume down key",app_bt_key, NULL},
#ifdef __FACTORY_KEY_SUPPORT_
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},	
#endif
#ifdef __TOUCH_KEY__
    {{APP_KEY_CODE_FN4,APP_KEY_EVENT_CLICK},"bt function key",touch_key_handler, NULL},	
#endif


};

#elif defined(__SINGLE_TOUCH_SIMPLE_MODE_)
extern const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_REPEAT},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
};
#elif defined(__SINGLE_TOUCH_SIMPLE_MODE_WITH_TRICPLE_)
extern const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_REPEAT},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
#ifdef __FACTORY_KEY_SUPPORT_
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2|APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
#endif
};
#elif  defined(__TWO_KEYS_) //Modified by ATX : Parke.Wei_20180322
extern const APP_KEY_HANDLE  app_key_handle_cfg[] = {
	{{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt function key",app_bt_key, NULL},
	{{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt function key",app_bt_key_enter_testmode, NULL},
#ifdef __FACTORY_KEY_SUPPORT_
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
#endif
};
#elif  defined(__THREE_KEYS_)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_UP},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN3,APP_KEY_EVENT_LONGPRESS},"bt play forward key",app_bt_key, NULL},
};
#endif
#else//__HALL_CONTROL_POWER_
#if defined(__SINGLE_KEY_)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
};
#elif  defined(__TWO_KEYS_)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt play skip key",app_bt_key, NULL},
};
#elif  defined(__THREE_KEYS_)
const APP_KEY_HANDLE  app_key_handle_cfg[] = {
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGLONGPRESS},"bt function key",app_bt_key_shutdown, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_LONGPRESS},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_CLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_DOUBLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_TRIPLECLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_ULTRACLICK},"bt function key",app_bt_key, NULL},
    {{APP_KEY_CODE_PWR,APP_KEY_EVENT_RAMPAGECLICK},"bt function key",app_bt_key_enter_testmode, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_UP},"bt volume up key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN1,APP_KEY_EVENT_LONGPRESS},"bt play backward key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_UP},"bt volume down key",app_bt_key, NULL},
    {{APP_KEY_CODE_FN2,APP_KEY_EVENT_LONGPRESS},"bt play forward key",app_bt_key, NULL},
};
#endif
#endif//__HALL_CONTROL_POWER_

