#include "cmsis.h"
#include "cmsis_os.h"
#include "hal_trace.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "hal_sleep.h"
#include "app_thread.h"
#include "app_utils.h"

static uint32_t app_wdt_ping_time = 0;

int app_sysfreq_req(enum APP_SYSFREQ_USER_T user, enum APP_SYSFREQ_FREQ_T freq)
{
    int ret;

//alway use audio pll by xiao 02-28
#if 0//def ULTRA_LOW_POWER
    uint32_t lock = int_lock();

    enum HAL_CMU_FREQ_T old_sysfreq = hal_sysfreq_get();

    // Enable PLL if required
    if (old_sysfreq <= HAL_CMU_FREQ_52M && freq > APP_SYSFREQ_52M) {
        hal_cmu_low_freq_mode_disable();
    }
#endif

    ret = hal_sysfreq_req((enum HAL_SYSFREQ_USER_T)user, (enum HAL_CMU_FREQ_T)freq);

//alway use audio pll by xiao 02-28
#if 0//def ULTRA_LOW_POWER
    // Disable PLL if capable
    if (old_sysfreq > HAL_CMU_FREQ_52M && hal_sysfreq_get() <= HAL_CMU_FREQ_52M) {
        hal_cmu_low_freq_mode_enable();
    }

    int_unlock(lock);
#endif

    return ret;
}

static void app_wdt_kick(void)
{
    hal_wdt_ping(HAL_WDT_ID_0);
}

static int app_wdt_handle_process(APP_MESSAGE_BODY *msg_body)
{
    app_wdt_kick();
    app_wdt_thread_check_handle();

    return 0;
}

static void app_wdt_irq_handle(enum HAL_WDT_ID_T id, uint32_t status)
{
    ASSERT(0, "%s id:%d status:%d",__func__, id, status);
}

void app_wdt_ping(void)
{
    APP_MESSAGE_BLOCK msg;
    uint32_t time = hal_sys_timer_get();
    if  ((time - app_wdt_ping_time)>MS_TO_TICKS(3000)){
        app_wdt_ping_time = time;
        msg.mod_id = APP_MODUAL_WATCHDOG;
        app_mailbox_put(&msg);
    }
}

int app_wdt_open(int seconds)
{
    hal_wdt_set_irq_callback(HAL_WDT_ID_0, app_wdt_irq_handle);
    hal_wdt_set_timeout(HAL_WDT_ID_0, seconds);
    hal_wdt_start(HAL_WDT_ID_0);    
    app_wdt_thread_check_open();
    hal_sleep_set_lowpower_hook(HAL_SLEEP_HOOK_USER_0, app_wdt_kick);
    app_set_threadhandle(APP_MODUAL_WATCHDOG, app_wdt_handle_process);
    return 0;
}

int app_wdt_close(void)
{
    hal_wdt_stop(HAL_WDT_ID_0);
    return 0;
}

typedef struct {
    uint32_t kick_time;
    uint32_t timeout;
    bool enable;
}APP_WDT_THREAD_CHECK;

APP_WDT_THREAD_CHECK app_wdt_thread_check[APP_WDT_THREAD_CHECK_USER_QTY];

uint32_t __inline__ app_wdt_thread_tickdiff_calc(uint32_t curr_ticks, uint32_t prev_ticks)
{
    if(curr_ticks < prev_ticks)
        return ((0xffffffff  - prev_ticks + 1) + curr_ticks);
    else
        return (curr_ticks - prev_ticks);
}

int app_wdt_thread_check_open(void)
{
    uint8_t i=0;

    for (i=0; i<APP_WDT_THREAD_CHECK_USER_QTY; i++){
        app_wdt_thread_check[i].kick_time = 0;
        app_wdt_thread_check[i].timeout = 0;
        app_wdt_thread_check[i].enable = false;
    }
    
    return 0;
}

int app_wdt_thread_check_enable(enum APP_WDT_THREAD_CHECK_USER_T user, int seconds)
{
    ASSERT(user < APP_WDT_THREAD_CHECK_USER_QTY, "%s user:%d seconds:%d",__func__, user, seconds);

    app_wdt_thread_check[user].kick_time = hal_sys_timer_get();    
    app_wdt_thread_check[user].timeout = MS_TO_TICKS(1000*seconds);
    app_wdt_thread_check[user].enable = true;

    return 0;
}

int app_wdt_thread_check_disable(enum APP_WDT_THREAD_CHECK_USER_T user)
{
    app_wdt_thread_check[user].kick_time = 0;
    app_wdt_thread_check[user].enable = false;

    return 0;
}

int app_wdt_thread_check_ping(enum APP_WDT_THREAD_CHECK_USER_T user)
{
    if (app_wdt_thread_check[user].enable){
        app_wdt_thread_check[user].kick_time = hal_sys_timer_get();
    }
    return 0;
}

int app_wdt_thread_check_handle(void)
{
    uint32_t curtime = hal_sys_timer_get();
    uint32_t diff, i;

    for (i=0; i<APP_WDT_THREAD_CHECK_USER_QTY; i++){
        if (app_wdt_thread_check[i].enable){
            diff = app_wdt_thread_tickdiff_calc(curtime, app_wdt_thread_check[i].kick_time);
            if (diff >= app_wdt_thread_check[i].timeout){
                ASSERT(0, "%s user:%d",__func__, i, diff);
            }
        }
    }

    return 0;
}
