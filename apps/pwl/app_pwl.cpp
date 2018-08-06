#include "cmsis_os.h"
#include "tgt_hardware.h"
#include "hal_gpio.h"
#include "hal_iomux.h"
#include "hal_pwm.h"//Modified by ATX 
#include "hal_trace.h"
#include "pmu.h"
#include "app_pwl.h"
#include "string.h"

#define APP_PWL_TRACE(s,...)
//TRACE(s, ##__VA_ARGS__)

static void app_pwl_timehandler(void const *param);
 
osTimerDef (APP_PWL_TIMER0, app_pwl_timehandler);                      // define timers
#if	(CFG_HW_PLW_NUM == 2)
osTimerDef (APP_PWL_TIMER1, app_pwl_timehandler);
#endif
#if	(CFG_HW_PLW_NUM == 3)//Modified by ATX : Parke.Wei_20180322
osTimerDef (APP_PWL_TIMER1, app_pwl_timehandler);
osTimerDef (APP_PWL_TIMER2, app_pwl_timehandler);
#endif


struct APP_PWL_T {
    enum APP_PWL_ID_T id;
    struct APP_PWL_CFG_T config;
    uint8_t partidx;
    osTimerId timer;
};

static struct APP_PWL_T app_pwl[APP_PWL_ID_QTY];

//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
static int app_pwl_set_level(enum APP_PWL_ID_T id, uint8_t  level)
{
    struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_pinmux = {HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE};

    struct APP_PWL_T *pwl = NULL;
    struct HAL_PWM_CFG_T cfg;
    bool use_as_gpio = true;
	
    APP_PWL_TRACE("%s id:%d, level:%d",__func__, id, level);

    pwl = &app_pwl[id];
    if (pwl->config.dynamic_light){
        if((!level) || (level == 100)){
            use_as_gpio = true;
        }else{
            APP_PWL_TRACE("PWM");
            use_as_gpio = false;
            cfg.freq = 1000;
            cfg.ratio = level;
            cfg.inv = false;
            cfg.sleep_on = true;
            hal_pwm_enable((enum HAL_PWM_ID_T)id, &cfg);            
            cfg_hw_pinmux.pin = cfg_hw_pinmux_pwl[id].pin;
            cfg_hw_pinmux.function = cfg_hw_pinmux_pwl[id].function;
            hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&cfg_hw_pinmux, 1);
        }
    }
    if (use_as_gpio){
        APP_PWL_TRACE("GPIO");
        if(level){
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
            pmu_viorise_req(id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, true);
#endif
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin, HAL_GPIO_DIR_OUT, 1);
        }else{
            hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin, HAL_GPIO_DIR_OUT, 0);
			hal_pwm_disable((enum HAL_PWM_ID_T)id);
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
            pmu_viorise_req(id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, false);
#endif
        }          
        cfg_hw_pinmux.pin = cfg_hw_pinmux_pwl[id].pin;
        cfg_hw_pinmux.function= HAL_IOMUX_FUNC_AS_GPIO;
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&cfg_hw_pinmux, 1);
    }
    return 0;
}
#endif
static void app_pwl_timehandler(void const *param)
{
    struct APP_PWL_T *pwl = (struct APP_PWL_T *)param;
    struct APP_PWL_CFG_T *cfg = &(pwl->config);
    APP_PWL_TRACE("%s %x",__func__, param);

    osTimerStop(pwl->timer);

    pwl->partidx++;
    if (cfg->periodic){
        if (pwl->partidx >= cfg->parttotal){
            pwl->partidx = 0;
        }
    }else{
        if (pwl->partidx >= cfg->parttotal){
            return;
        }
    }

    APP_PWL_TRACE("idx:%d pin:%d lvl:%d", pwl->partidx, cfg_hw_pinmux_pwl[pwl->id].pin, cfg->part[pwl->partidx].level);
//Modified by ATX 	
#if CFG_HW_LED_PWM_FUNCTION	
	app_pwl_set_level(pwl->id, cfg->part[pwl->partidx].level);
#else	
    if(cfg->part[pwl->partidx].level){
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
        pmu_viorise_req(pwl->id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, true);
#endif
#ifdef CHIP_BEST2000
        if((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED1 || (enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED2)
        {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
        else
#endif    
        {
            hal_gpio_pin_set((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
    }else{
#ifdef CHIP_BEST2000
        if((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED1 || (enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED2)
        {
            hal_gpio_pin_set((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
        else
#endif   
        {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
        pmu_viorise_req(pwl->id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, false);
#endif
    }
#endif
    osTimerStart(pwl->timer, cfg->part[pwl->partidx].time);
}

int app_pwl_open(void)
{
    uint8_t i;
    APP_PWL_TRACE("%s",__func__);
    for (i=0;i<APP_PWL_ID_QTY;i++){
        app_pwl[i].id = APP_PWL_ID_QTY;
        memset(&(app_pwl[i].config), 0, sizeof(struct APP_PWL_CFG_T));
        hal_iomux_init((struct HAL_IOMUX_PIN_FUNCTION_MAP *)&cfg_hw_pinmux_pwl[i], 1);
//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION		
//Modified by ATX : Leon.He_20180119: don't set dir when open for led1 led2, avoid to lighting led1 and led2 without any action
        if (cfg_hw_pinmux_pwl[i].pin == HAL_GPIO_PIN_LED1 || cfg_hw_pinmux_pwl[i].pin == HAL_GPIO_PIN_LED2)
        {
        	 APP_PWL_TRACE("ignore set dir when open for led1 led2\n");
        }
        else
#endif		
        	hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[i].pin, HAL_GPIO_DIR_OUT, 0);	
    }
    app_pwl[APP_PWL_ID_0].timer = osTimerCreate (osTimer(APP_PWL_TIMER0), osTimerOnce, &app_pwl[APP_PWL_ID_0]);
#if (CFG_HW_PLW_NUM == 2)
    app_pwl[APP_PWL_ID_1].timer = osTimerCreate (osTimer(APP_PWL_TIMER1), osTimerOnce, &app_pwl[APP_PWL_ID_1]);
#endif
//Modified by ATX : Parke.Wei_20180322
#if (CFG_HW_PLW_NUM == 3)
	app_pwl[APP_PWL_ID_1].timer = osTimerCreate (osTimer(APP_PWL_TIMER1), osTimerOnce, &app_pwl[APP_PWL_ID_1]);
	app_pwl[APP_PWL_ID_2].timer = osTimerCreate (osTimer(APP_PWL_TIMER2), osTimerOnce, &app_pwl[APP_PWL_ID_2]);

#endif

    return 0;
}

int app_pwl_start(enum APP_PWL_ID_T id)
{
    struct APP_PWL_T *pwl = NULL;
    struct APP_PWL_CFG_T *cfg = NULL;


    if (id >= APP_PWL_ID_QTY) {
        return -1;
    }

    APP_PWL_TRACE("%s %d",__func__, id);

    pwl = &app_pwl[id];
    cfg = &(pwl->config);

    if (pwl->id == APP_PWL_ID_QTY){
        return -1;
    }

    pwl->partidx = 0;
    if (pwl->partidx >= cfg->parttotal){
        return -1;
    }

    osTimerStop(pwl->timer);

    APP_PWL_TRACE("idx:%d pin:%d lvl:%d", pwl->partidx, cfg_hw_pinmux_pwl[pwl->id].pin, cfg->part[pwl->partidx].level);
//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
	app_pwl_set_level(pwl->id, cfg->part[pwl->partidx].level);
#else
	if(cfg->part[pwl->partidx].level){
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
        pmu_viorise_req(pwl->id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, false);
#endif
#ifdef CHIP_BEST2000
        if((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED1 || (enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED2)
        {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
        else
#endif            
        {
            hal_gpio_pin_set((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
    }else{
#ifdef CHIP_BEST2000
        if((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED1 || (enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin == HAL_GPIO_PIN_LED2)
        {
             hal_gpio_pin_set((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
        else
#endif            
        {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[pwl->id].pin);
        }
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
        pmu_viorise_req(pwl->id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, false);
#endif
    }
#endif
    osTimerStart(pwl->timer, cfg->part[pwl->partidx].time);
    return 0;
}

int app_pwl_setup(enum APP_PWL_ID_T id, struct APP_PWL_CFG_T *cfg)
{

    if (cfg == NULL || id >= APP_PWL_ID_QTY) {
        return -1;
    }
    APP_PWL_TRACE("%s %d",__func__, id);
//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
	app_pwl_set_level(id, 0);
#else
    hal_gpio_pin_set_dir((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin, HAL_GPIO_DIR_OUT, cfg->startlevel?1:0);
#endif    
	app_pwl[id].id = id;
    memcpy(&(app_pwl[id].config), cfg, sizeof(struct APP_PWL_CFG_T));

    osTimerStop(app_pwl[id].timer);

    return 0;
}

int app_pwl_stop(enum APP_PWL_ID_T id)
{

    if (id >= APP_PWL_ID_QTY) {
        return -1;
    }

    osTimerStop(app_pwl[id].timer);
//Modified by ATX 
#if CFG_HW_LED_PWM_FUNCTION
    app_pwl_set_level(id, 0);
#if defined(__PMU_VIO_DYNAMIC_CTRL_MODE__)
    if (app_pwl[id].id != APP_PWL_ID_QTY)
            pmu_viorise_req(app_pwl[id].id == APP_PWL_ID_0 ? PMU_VIORISE_REQ_USER_PWL0 : PMU_VIORISE_REQ_USER_PWL1, false);
#endif

#else
#ifdef CHIP_BEST2000
        if((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin == HAL_GPIO_PIN_LED1 || (enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin == HAL_GPIO_PIN_LED2)
        {
             hal_gpio_pin_set((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin);
        }
        else
#endif
        {
            hal_gpio_pin_clr((enum HAL_GPIO_PIN_T)cfg_hw_pinmux_pwl[id].pin);
        }
#endif
    return 0;
}

int app_pwl_close(void)
{
    uint8_t i;
    for (i=0;i<APP_PWL_ID_QTY;i++){
        if (app_pwl[i].id != APP_PWL_ID_QTY)
            app_pwl_stop((enum APP_PWL_ID_T)i);
    }
    return 0;
}


