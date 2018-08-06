#ifndef __APP_BATTERY_H__
#define __APP_BATTERY_H__

#define APP_BATTERY_SET_MESSAGE(appevt, status, volt) (appevt = (((uint32_t)status&0xffff)<<16)|(volt&0xffff))
#define APP_BATTERY_GET_STATUS(appevt, status) (status = (appevt>>16)&0xffff)
#define APP_BATTERY_GET_VOLT(appevt, volt) (volt = appevt&0xffff)

enum APP_BATTERY_STATUS_T {
    APP_BATTERY_STATUS_NORMAL,
    APP_BATTERY_STATUS_CHARGING,
    APP_BATTERY_STATUS_OVERVOLT,
    APP_BATTERY_STATUS_UNDERVOLT,
    APP_BATTERY_STATUS_PDVOLT,
    APP_BATTERY_STATUS_INVALID,

    APP_BATTERY_STATUS_QTY
};

typedef uint16_t APP_BATTERY_MV_T;
typedef void (*APP_BATTERY_EVENT_CB_T)(enum APP_BATTERY_STATUS_T, APP_BATTERY_MV_T volt);

int app_battery_open(void);

int app_battery_start(void);

int app_battery_stop(void);

int app_battery_close(void);

int app_battery_charger_indication_open(void);

int ntc_capture_open(void);

int ntc_capture_start(void);

#endif

