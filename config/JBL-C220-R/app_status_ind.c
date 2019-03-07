#include "cmsis_os.h"
#include "stdbool.h"
#include "hal_trace.h"
#include "app_pwl.h"
#include "app_status_ind.h"
#include "string.h"

static APP_STATUS_INDICATION_T app_status = APP_STATUS_INDICATION_NUM;
static APP_STATUS_INDICATION_T app_status_ind_filter = APP_STATUS_INDICATION_NUM;

#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
extern uint8_t app_get_fake_poweroff_flag(void);
#endif

int app_status_indication_filter_set(APP_STATUS_INDICATION_T status)
{
    app_status_ind_filter = status;
    return 0;
}

APP_STATUS_INDICATION_T app_status_indication_get(void)
{
    return app_status;
}

//Modified by ATX 
struct APP_STATUS_INDICATION_CFG_T {
    const struct APP_PWL_CFG_T pwl_cfg;    
};

#if CFG_HW_LED_PWM_FUNCTION
#define APP_STATUS_INDICATION_CFG(name, startlevel_, periodic_, dynamic_light_) \
const struct APP_STATUS_INDICATION_CFG_T app_status_indication_ ##name## _cfg = { \
                                                                            .pwl_cfg.part = (struct PWL_CYCLE_ONCE_T *)app_status_indication_  ##name##  _cfg_part, \
                                                                            .pwl_cfg.parttotal = sizeof(app_status_indication_ ##name## _cfg_part)/sizeof(struct PWL_CYCLE_ONCE_T), \
                                                                            .pwl_cfg.startlevel = startlevel_, \
                                                                            .pwl_cfg.periodic = periodic_, \
                                                                            .pwl_cfg.dynamic_light = dynamic_light_}


#define APP_STATUS_INDICATION_CFG_GET(name) (struct APP_PWL_CFG_T *)&(app_status_indication_ ##name## _cfg.pwl_cfg)

#define APP_STATUS_INDICATION_CFG_PART(name) \
const struct PWL_CYCLE_ONCE_T app_status_indication_ ##name## _cfg_part[]

#define Handover_Time         30

APP_STATUS_INDICATION_CFG_PART(connecting) = {
	{99,100},
	{0,100},
};

APP_STATUS_INDICATION_CFG(connecting, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(charge) = {
    {2  ,Handover_Time},
    {4  ,Handover_Time},
    {6  ,Handover_Time},
    {8  ,Handover_Time},
    {10 ,Handover_Time},
    {12 ,Handover_Time},
    {14 ,Handover_Time},
    {16 ,Handover_Time},
    {18 ,Handover_Time},
    {20 ,Handover_Time},
    {22 ,Handover_Time},
    {24 ,Handover_Time},
    {26 ,Handover_Time},
    {28 ,Handover_Time},
    {30 ,Handover_Time},
    {32 ,Handover_Time},
    {34 ,Handover_Time},
    {36 ,Handover_Time},
    {38 ,Handover_Time},
    {40 ,Handover_Time},
    {42 ,Handover_Time},
    {44 ,Handover_Time},
    {46 ,Handover_Time},
    {48 ,Handover_Time},
    {50 ,Handover_Time},
    {52 ,Handover_Time},
    {54 ,Handover_Time},
    {56 ,Handover_Time},
    {58 ,Handover_Time},
    {60 ,Handover_Time},
    {62 ,Handover_Time},
    {64 ,Handover_Time},
    {66 ,Handover_Time},
    {68 ,Handover_Time},
    {70 ,Handover_Time},
    {72 ,Handover_Time},
    {74 ,Handover_Time},
    {76 ,Handover_Time},
    {78 ,Handover_Time},
    {80 ,Handover_Time},
    {82 ,Handover_Time},
    {84 ,Handover_Time},
    {86 ,Handover_Time},
    {88 ,Handover_Time},
    {90 ,Handover_Time},
    {92 ,Handover_Time},
    {94 ,Handover_Time},
    {96 ,Handover_Time},
    {98 ,Handover_Time},
    {100,Handover_Time},
    {98 ,Handover_Time},
    {96 ,Handover_Time},
    {94 ,Handover_Time},
    {92 ,Handover_Time},
    {90 ,Handover_Time},
    {88 ,Handover_Time},
    {86 ,Handover_Time},
    {84 ,Handover_Time},
    {82 ,Handover_Time},
    {80 ,Handover_Time},
    {78 ,Handover_Time},
    {76 ,Handover_Time},
    {74 ,Handover_Time},
    {72 ,Handover_Time},
    {70 ,Handover_Time},
    {68 ,Handover_Time},
    {66 ,Handover_Time},
    {64 ,Handover_Time},
    {62 ,Handover_Time},
    {60 ,Handover_Time},
    {58 ,Handover_Time},
    {56 ,Handover_Time},
    {54 ,Handover_Time},
    {52 ,Handover_Time},
    {50 ,Handover_Time},
    {48 ,Handover_Time},
    {46 ,Handover_Time},
    {44 ,Handover_Time},
    {42 ,Handover_Time},
    {40 ,Handover_Time},
    {38 ,Handover_Time},
    {36 ,Handover_Time},
    {34 ,Handover_Time},
    {32 ,Handover_Time},
    {30 ,Handover_Time},
    {28 ,Handover_Time},
    {26 ,Handover_Time},
    {24 ,Handover_Time},
    {22 ,Handover_Time},
    {20 ,Handover_Time},
    {18 ,Handover_Time},
    {16 ,Handover_Time},
    {14 ,Handover_Time},
    {12 ,Handover_Time},
    {10 ,Handover_Time},
    {8  ,Handover_Time},
    {6  ,Handover_Time},
    {4  ,Handover_Time},
    {2  ,Handover_Time},
    {0  ,100},
};
APP_STATUS_INDICATION_CFG(charge, 2, true, true);

APP_STATUS_INDICATION_CFG_PART(poweron) = {
    {99,1000},
    {0,100},
};
APP_STATUS_INDICATION_CFG(poweron, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(factorytest) = {
	{0,100},
	{99,1000},
};
APP_STATUS_INDICATION_CFG(factorytest, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(pairing) = {
    {99,250},
    {0,250},
};
APP_STATUS_INDICATION_CFG(pairing, 1, true, true);

APP_STATUS_INDICATION_CFG_PART(ble_adv_mode) = {
    {99,500},
    {0,500},
};
APP_STATUS_INDICATION_CFG(ble_adv_mode, 1, true, true);

APP_STATUS_INDICATION_CFG_PART(incomingcall) = {
    {99,200},
    {0,800},
};
APP_STATUS_INDICATION_CFG(incomingcall, 1, true, true);

APP_STATUS_INDICATION_CFG_PART(testmode) = {
	{99,100},
	{0,100},
	{99,100},
	{0,100},
	{99,100},
	{0,100},
	{99,100},
};
APP_STATUS_INDICATION_CFG(testmode, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(clearsucceed) = {
    {99,100},
    {0,100},
	{99,100},
	{0,100},
	{99,100},
	{0,100},
};
APP_STATUS_INDICATION_CFG(clearsucceed, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(ota) = {
    {99,100},
    {0,100},
	{99,100},
	{0,100},
	{99,100},
	{0,100},
	{99,100},
	{0,100},
	{99,100},
	{0,100},
};
APP_STATUS_INDICATION_CFG(ota, 1, false, true);

APP_STATUS_INDICATION_CFG_PART(pagescan) = {
    {99,200},
    {0,2000},
	{99,200},
	{0,2000},

};
APP_STATUS_INDICATION_CFG(pagescan, 1, true, true);

APP_STATUS_INDICATION_CFG_PART(batterylow) = {
    {99,200},
    {0,200},
	{99,200},
	{0,4400},

};
APP_STATUS_INDICATION_CFG(batterylow, 1, true, true);

#endif

#if CFG_HW_LED_PWM_FUNCTION
int app_status_indication_set(APP_STATUS_INDICATION_T status)
{
    if (app_status == status)
        return 0;

    if (app_status_ind_filter == status)
        return 0;

	if(app_status_indication_get() == APP_STATUS_INDICATION_CHARGENEED)
		return 0;
	
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
	if(app_get_fake_poweroff_flag() == 1)
		return 0;
#endif    

	if(app_status_indication_get() == APP_STATUS_INDICATION_CHARGENEED)
		return 0;
	
	TRACE("%s %d",__func__, status);
    app_status = status;
    
    app_pwl_stop(APP_PWL_ID_0);
   
    switch (status) {     
	 case APP_STATUS_INDICATION_POWERON:
		app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(poweron));
		app_pwl_start(APP_PWL_ID_0);
      	break;
        
      case APP_STATUS_INDICATION_POWEROFF:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(poweron));
          app_pwl_start(APP_PWL_ID_0);
      break;

	 case APP_STATUS_INDICATION_CHARGENEED:
	 	app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(batterylow));
          app_pwl_start(APP_PWL_ID_0);
	 	break;
		
	  case APP_STATUS_INDICATION_SLAVE_PAIRING:		
		app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(factorytest));
		app_pwl_start(APP_PWL_ID_0);
		break;

		case APP_STATUS_INDICATION_VERSION_CHECK:		
		app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(ble_adv_mode));
		app_pwl_start(APP_PWL_ID_0);
		break;
		
	  case APP_STATUS_INDICATION_PAGESCAN:
      case APP_STATUS_INDICATION_CONNECTING:  
	  case APP_STATUS_INDICATION_TWS_SEARCH:
	  case APP_STATUS_INDICATION_TWS_PAIRING:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(pagescan));
          app_pwl_start(APP_PWL_ID_0);	  	
	  break;
	  
	  case APP_STATUS_INDICATION_BOTHSCAN:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(pairing));
          app_pwl_start(APP_PWL_ID_0);
      break;

	case APP_STATUS_INDICATION_INCOMINGCALL:
		app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(incomingcall));
		app_pwl_start(APP_PWL_ID_0);
		break;
		
	case APP_STATUS_INDICATION_HANGUPCALL:
	case APP_STATUS_INDICATION_ANSWERCALL:
	case APP_STATUS_INDICATION_REFUSECALL:
      case APP_STATUS_INDICATION_CONNECTED:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(connecting));
          app_pwl_start(APP_PWL_ID_0);
      break;
      
      case APP_STATUS_INDICATION_CHARGING:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(charge));
          app_pwl_start(APP_PWL_ID_0);
      break;
	  
      case APP_STATUS_INDICATION_TESTMODE:
//	  case APP_STATUS_INDICATION_TESTMODE1:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(testmode));
          app_pwl_start(APP_PWL_ID_0);
      break;   
	  
	  case APP_STATUS_INDICATION_OTA:
          app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(ota));
          app_pwl_start(APP_PWL_ID_0);
      break; 	 

	case APP_STATUS_INDICATION_CLEARSUCCEED:
		app_pwl_setup(APP_PWL_ID_0, APP_STATUS_INDICATION_CFG_GET(clearsucceed));
		app_pwl_start(APP_PWL_ID_0);
		break; 	 
			
      default:
      break;
    }
  
    return 0;
}
#else
int app_status_indication_set(APP_STATUS_INDICATION_T status)
{
    struct APP_PWL_CFG_T cfg0;
    struct APP_PWL_CFG_T cfg1;
    TRACE("%s %d",__func__, status);
#ifdef __DEEP_SLEEP_FAKE_POWER_OFF__
	if(app_get_fake_poweroff_flag() == 1)
		return 0;
#endif

	if(app_status_indication_get() == APP_STATUS_INDICATION_CHARGENEED)
		return 0;
	
    if (app_status == status)
        return 0;

    if (app_status_ind_filter == status)
        return 0;

    app_status = status;
    memset(&cfg0, 0, sizeof(struct APP_PWL_CFG_T));
    memset(&cfg1, 0, sizeof(struct APP_PWL_CFG_T));
    app_pwl_stop(APP_PWL_ID_0);
    app_pwl_stop(APP_PWL_ID_1);
    switch (status) {
		case APP_STATUS_INDICATION_SLAVE_PAIRING:
			cfg0.part[0].level = 1;
			cfg0.part[0].time = (1000);
			cfg0.parttotal = 1;
			cfg0.startlevel = 1;
			cfg0.periodic = true;
			app_pwl_setup(APP_PWL_ID_0, &cfg0);
			app_pwl_start(APP_PWL_ID_0);
			break;
			
		case APP_STATUS_INDICATION_POWERON:
			cfg0.part[0].level = 1;
			cfg0.part[0].time = (1000);
			cfg0.part[1].level = 0;
			cfg0.part[1].time = (100);
			cfg0.parttotal = 2;
			cfg0.startlevel = 0;
			cfg0.periodic = false;

            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);

            break;
        case APP_STATUS_INDICATION_INITIAL:
            break;
        case APP_STATUS_INDICATION_PAGESCAN:
        case APP_STATUS_INDICATION_CONNECTING:
		case APP_STATUS_INDICATION_TWS_SEARCH:
        case APP_STATUS_INDICATION_TWS_PAIRING:
			cfg1.part[0].level = 1;
            cfg1.part[0].time = (1000);
            cfg1.parttotal = 1;
            cfg1.startlevel = 1;
            cfg1.periodic = true;
            app_pwl_setup((APP_PWL_ID_1), &cfg1);
            app_pwl_start((APP_PWL_ID_1));
            break;
        case APP_STATUS_INDICATION_BOTHSCAN:
			cfg0.part[0].level = 0;
            cfg0.part[0].time = (200);
            cfg0.part[1].level = 1;
            cfg0.part[1].time = (200);
            cfg0.parttotal = 2;
            cfg0.startlevel = 0;
            cfg0.periodic = true;
            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
            break;
        case APP_STATUS_INDICATION_CONNECTED:
			cfg0.part[0].level = 1;
            cfg0.part[0].time = (1000);
            cfg0.parttotal = 1;
            cfg0.startlevel = 1;
            cfg0.periodic = true;
            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
            break;
        case APP_STATUS_INDICATION_CHARGING:
            cfg1.part[0].level = 1;
            cfg1.part[0].time = (5000);
            cfg1.parttotal = 1;
            cfg1.startlevel = 1;
            cfg1.periodic = true;
            app_pwl_setup(APP_PWL_ID_1, &cfg1);
            app_pwl_start(APP_PWL_ID_1);
            break;
        case APP_STATUS_INDICATION_FULLCHARGE:
#ifdef __BLUE_LED_ON_10S_FOR_FULLCHARGE__
            cfg0.part[0].level = 1;
            cfg0.part[0].time = (5000);
            cfg0.parttotal = 1;
            cfg0.startlevel = 1;
            cfg0.periodic = true;
            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
#endif

            break;
        case APP_STATUS_INDICATION_POWEROFF:
			cfg1.part[0].level = 1;
			cfg1.part[0].time = (1000);
			cfg1.part[1].level = 0;
			cfg1.part[1].time = (100);
			cfg1.parttotal = 2;
			cfg1.startlevel = 0;
			cfg1.periodic = false;

			app_pwl_setup(APP_PWL_ID_1, &cfg1);
			app_pwl_start(APP_PWL_ID_1);
            break;
        case APP_STATUS_INDICATION_CHARGENEED:
			cfg1.part[0].level = 1;
            cfg1.part[0].time = (400);
            cfg1.part[1].level = 0;
            cfg1.part[1].time = (2600);
            cfg1.parttotal = 2;
            cfg1.startlevel = 1;
            cfg1.periodic = true;
            app_pwl_setup(APP_PWL_ID_1, &cfg1);
            app_pwl_start(APP_PWL_ID_1);    
            break;
	   case APP_STATUS_INDICATION_CLEARSUCCEED:
		   cfg0.part[0].level =1;
            cfg0.part[0].time = (100);
            cfg0.part[1].level = 0;
            cfg0.part[1].time = (100);
            cfg0.part[2].level =1;
            cfg0.part[2].time = (100);
            cfg0.part[3].level = 0;
            cfg0.part[3].time = (100);
            cfg0.part[4].level =1;
            cfg0.part[4].time = (100);
            cfg0.part[5].level = 0;
            cfg0.part[5].time = (100);
            cfg0.parttotal = 6;
            cfg0.startlevel = 1;
            cfg0.periodic = false;

            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
		   break;
        case APP_STATUS_INDICATION_TESTMODE:
            cfg0.part[0].level =1;
            cfg0.part[0].time = (100);
            cfg0.part[1].level = 0;
            cfg0.part[1].time = (100);
            cfg0.part[2].level =1;
            cfg0.part[2].time = (100);
            cfg0.part[3].level = 0;
            cfg0.part[3].time = (100);
            cfg0.part[4].level =1;
            cfg0.part[4].time = (100);
            cfg0.part[5].level = 0;
            cfg0.part[5].time = (100);
            cfg0.parttotal = 6;
            cfg0.startlevel = 0;
            cfg0.periodic = false;

            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
            break;
        case APP_STATUS_INDICATION_TESTMODE1:
            cfg0.part[0].level =1;
            cfg0.part[0].time = (100);
            cfg0.part[1].level = 0;
            cfg0.part[1].time = (100);
            cfg0.part[2].level =1;
            cfg0.part[2].time = (100);
            cfg0.part[3].level = 0;
            cfg0.part[3].time = (100);
            cfg0.part[4].level =1;
            cfg0.part[4].time = (100);
            cfg0.part[5].level = 0;
            cfg0.part[5].time = (100);
            cfg0.parttotal = 6;
            cfg0.startlevel = 0;
            cfg0.periodic = false;

            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
            break;
        case  APP_STATUS_INDICATION_OTA://Modified by ATX : Leon.He_20180302: enable UI prompt for OTA by default
            cfg0.part[0].level =1;
            cfg0.part[0].time = (100);
            cfg0.part[1].level = 0;
            cfg0.part[1].time = (100);
            cfg0.part[2].level =1;
            cfg0.part[2].time = (100);
            cfg0.part[3].level = 0;
            cfg0.part[3].time = (100);
            cfg0.part[4].level =1;
            cfg0.part[4].time = (100);
            cfg0.part[5].level = 0;
            cfg0.part[5].time = (100);
            cfg0.part[6].level =1;
            cfg0.part[6].time = (100);
            cfg0.part[7].level = 0;
            cfg0.part[7].time = (100);
            cfg0.part[8].level =1;
            cfg0.part[8].time = (100);
            cfg0.part[9].level = 0;
            cfg0.part[9].time = (100);
            cfg0.parttotal = 10;
            cfg0.startlevel = 0;
            cfg0.periodic = false;

            app_pwl_setup(APP_PWL_ID_0, &cfg0);
            app_pwl_start(APP_PWL_ID_0);
        	break;
        default:
            break;
    }
    return 0;
}
#endif

