#include "cmsis_os.h"
#include "app_overlay.h"
#include "hal_trace.h"

osMutexDef(app_overlay_mutex);

static osMutexId app_overlay_mutex_id = NULL;
static APP_OVERLAY_ID_T app_overlay_id = APP_OVERLAY_ID_QTY;

void app_overlay_select(enum APP_OVERLAY_ID_T id)
{
	TRACE("%s id:%d",__func__, id);
	osMutexWait(app_overlay_mutex_id, osWaitForever);
	if (app_overlay_id==APP_OVERLAY_ID_QTY){
		app_overlay_load(id);
	}else if (app_overlay_id!=APP_OVERLAY_ID_QTY){
		if (app_overlay_id!=id){
			app_overlay_unload(app_overlay_id);
			app_overlay_load(id);
		}
	}
	app_overlay_id = id;
	osMutexRelease(app_overlay_mutex_id);
}


enum APP_OVERLAY_ID_T app_overlay_getload(void)
{
	enum APP_OVERLAY_ID_T id;
	
//	osMutexWait(app_overlay_mutex_id, osWaitForever);
	id = app_overlay_id;	
//	osMutexRelease(app_overlay_mutex_id);

	return id;
}


void app_overlay_unloadall(void)
{
	osMutexWait(app_overlay_mutex_id, osWaitForever);
	if (app_overlay_id != APP_OVERLAY_ID_QTY){
		app_overlay_unload(app_overlay_id);
	}
	app_overlay_id = APP_OVERLAY_ID_QTY;
	osMutexRelease(app_overlay_mutex_id);
}

void app_overlay_open(void)
{
	if (app_overlay_mutex_id == NULL)
		osMutexCreate(osMutex(app_overlay_mutex));
}

void app_overlay_close(void)
{
	app_overlay_unloadall();
	if (app_overlay_mutex_id != NULL){
		osMutexDelete(app_overlay_mutex_id);
		app_overlay_mutex_id = NULL;
	}
}


