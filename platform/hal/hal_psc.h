#ifndef __HAL_PSC_H__
#define __HAL_PSC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void hal_psc_codec_enable(void);

void hal_psc_codec_disable(void);

void hal_psc_bt_enable(void);

void hal_psc_bt_disable(void);

#ifdef __cplusplus
}
#endif

#endif

