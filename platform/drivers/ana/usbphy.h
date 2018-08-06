#ifndef __USBPHY_H__
#define __USBPHY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "hal_cmu.h"
#include "plat_addr_map.h"
#include CHIP_SPECIFIC_HDR(usbphy)

void usbphy_open(void);

void usbphy_reset(void);

void usbphy_sleep(void);

void usbphy_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif

