#ifndef __BT_DRV_H__
#define  __BT_DRV_H__

#ifdef __cplusplus
extern "C" {
#endif


#include "cmsis_os.h"

#include "hal_timer.h"

#include "hal_analogif.h"
#include "hal_trace.h"

#define btdrv_read_rf_reg(reg,val)  hal_analogif_reg_read(reg,val)
#define btdrv_write_rf_reg(reg,val) hal_analogif_reg_write(reg,val)


#define btdrv_delay(ms) hal_sys_timer_delay(MS_TO_TICKS(ms))


#define BTDIGITAL_REG(a) (*(volatile uint32_t *)(a))
#define BTDIGITAL_REG_SET_FIELD(reg, mask, shift, v)\
                                                do{ \
                                                    volatile unsigned int tmp = *(volatile unsigned int *)(reg); \
                                                    tmp &= ~(mask<<shift); \
                                                    tmp |= (v<<shift); \
                                                    *(volatile unsigned int *)(reg) = tmp; \
                                                }while(0)

#define BTDIGITAL_REG_GET_FIELD(reg, mask, shift, v)\
                                                do{ \
                                                    volatile unsigned int tmp = *(volatile unsigned int *)(reg); \
                                                    v = (tmp>>shift)&mask; \
                                                }while(0)

#define DRV_DEBUG  0
#if DRV_DEBUG
#define DRV_PRINT(fmt, ...) TRACE(fmt, ##__VA_ARGS__)
#define DRV_DUMP(s,buff,len) DUMP8(s,buff,len)
#else
#define DRV_PRINT(fmt, ...)
#define DRV_DUMP(s,buff,len)
#endif

#define BT_CONTROLLER_CRASH_DUMP_ADDR_BASE  (0xc00064cc)

//#define __PASS_CI_TEST_SETTING__

#define  BT_LOW_POWER_MODE  1
#define  BT_HIGH_PERFORMANCE_MODE   2

#define BT_RFMODE BT_LOW_POWER_MODE
//#define BT_RFMODE BT_HIGH_PERFORMANCE_MODE

#define BT_POWERON            1
#define BT_POWEROFF           0



/// 8 bit access types
#define _8_Bit                              8
/// 16 bit access types
#define _16_Bit                             16
/// 32 bit access types
#define _32_Bit                             32

#define BTDRV_PATCH_WRITING  0x0
#define BTDRV_PATCH_DONE         0x1


#define BTDRV_CFG_WRITING   0x0
#define BTDRV_CFG_DONE      0x1


#define HCI_DBG_RD_MEM_CMD_OPCODE                     0xFC01
#define HCI_DBG_WR_MEM_CMD_OPCODE                     0xFC02
#define HCI_DBG_DEL_PAR_CMD_OPCODE                    0xFC03
#define HCI_DBG_FLASH_ID_CMD_OPCODE                   0xFC05
#define HCI_DBG_FLASH_ER_CMD_OPCODE                   0xFC06
#define HCI_DBG_FLASH_WR_CMD_OPCODE                   0xFC07
#define HCI_DBG_FLASH_RD_CMD_OPCODE                   0xFC08
#define HCI_DBG_RD_PAR_CMD_OPCODE                     0xFC09
#define HCI_DBG_WR_PAR_CMD_OPCODE                     0xFC0A
#define HCI_DBG_WLAN_COEX_CMD_OPCODE                  0xFC0B
#define HCI_DBG_ENTER_TEST_MODE_CMD_OPCODE            0xFC0C
#define HCI_DBG_WLAN_COEXTST_SCEN_CMD_OPCODE          0xFC0D
#define HCI_DBG_SEND_LMP_CMD_OPCODE                   0xFC0E
#define HCI_DBG_WR_SYNC_DATA_CFG_CMD_OPCODE           0xFC0F
#define HCI_DBG_RD_KE_STATS_CMD_OPCODE                0xFC10
#define HCI_DBG_PLF_RESET_CMD_OPCODE                  0xFC11
#define HCI_DBG_RD_MEM_INFO_CMD_OPCODE                0xFC12
#define HCI_DBG_EMUL_TESTER_CMD_OPCODE                0xFC2D
#define HCI_DBG_SCATT_IMPROV_CMD_OPCODE               0xFC2E
#define HCI_DBG_RF_REG_RD_CMD_OPCODE                  0xFC39
#define HCI_DBG_RF_REG_WR_CMD_OPCODE                  0xFC3A
#define HCI_DBG_HW_REG_RD_CMD_OPCODE                  0xFC30
#define HCI_DBG_HW_REG_WR_CMD_OPCODE                  0xFC31
#define HCI_DBG_SET_BD_ADDR_CMD_OPCODE                0xFC32
#define HCI_DBG_SET_TYPE_PUB_CMD_OPCODE               0xFC33
#define HCI_DBG_SET_TYPE_RAND_CMD_OPCODE              0xFC34
#define HCI_DBG_SET_CRC_CMD_OPCODE                    0xFC35
#define HCI_DBG_LLCP_DISCARD_CMD_OPCODE               0xFC36
#define HCI_DBG_RESET_RX_CNT_CMD_OPCODE               0xFC37
#define HCI_DBG_RESET_TX_CNT_CMD_OPCODE               0xFC38
#define HCI_DBG_SET_TX_PW_CMD_OPCODE                  0xFC3B
#define HCI_DBG_SET_SYNCWORD_CMD_OPCODE       0xfc3c


//bt address not ble address
#define HCI_DBG_SET_BT_ADDR_CMD_OPCODE    0xfc40
//encrytion min and max key size
#define HCI_DBG_SET_ENCRYPTION_KEY_SIZE_CMD_OPCODE    0xfc41
//preferred key type default :combine key
#define HCI_DBG_SET_PREFERRED_KEY_TYPE_CMD_OPCODE   0xfc42
//creat unit key
#define HCI_DBG_CREAT_UNIT_KEY_CMD_OPCODE   0xfc43
//set clk drift and jitter
#define HCI_DBG_SET_LPCLK_DRIFT_JITTER_CMD_OPCODE        0xfc44
//set exernal wake up time oscillater wakeup time and radio wakeup time
#define HCI_DBG_SET_WAKEUP_TIME_CMD_OPCODE   0xfc45

//change uart buadrate
#define HCI_DBG_CHANGE_UART_BAUDRATE_CMD_OPCODE  0xfc46

//set sleep enable and external wakeup enable
#define HCI_DBG_SET_SLEEP_EXWAKEUP_EN_CMD_OPCODE  0xfc47

//set private key
#define HCI_DBG_SET_SP_PRIVATE_KEY_CMD_OPCODE    0xfc48

//set public key
#define HCI_DBG_SET_SP_PUBLIC_KEY_CMD_OPCODE    0xfc49

//set errdata adopted
#define HCI_DBG_SET_ERRDATA_ADOPTED_CMD_OPCODE  0xfc4a

//set basic threshold
#define HCI_DBG_SET_BASIC_THRESHOLD_CMD_OPCODE  0xfc4b

//set edr threshold
#define HCI_DBG_SET_EDR_THRESHOLD_CMD_OPCODE   0xfc4c

//set basic algorithm
#define HCI_DBG_SET_BASIC_ALGORITHM_CMD_OPCODE 0xfc4d

//set edr alorithm
#define HCI_DBG_SET_EDR_ALGORITHM_CMD_OPCODE   0xfc4e

//set basic packet lut
#define HCI_DBG_SET_BASIC_PKT_LUT_CMD_OPCODE   0xfc4f

//set edr packet lut
#define HCI_DBG_SET_EDR_PKT_LUT_CMD_OPCODE   0xfc50

//set sco path
#define HCI_DBG_SET_SYNC_CONFIG_CMD_OPCODE  0xfc51

//set pcm setting
#define HCI_DBG_SET_PCM_SETTING_CMD_OPCODE  0xfc52

//set sync buff size
#define HCI_DBG_SET_SYNC_BUF_SIZE_CMD_OPCODE  0xfc53

//set diag_bt_hw
#define HCI_DBG_SET_DIAG_BT_HW_CMD_OPCODE    0xfc54
//set diag ble hw
#define HCI_DBG_SET_DIAG_BLE_HW_CMD_OPCODE  0xfc55
//set diag sw
#define HCI_DBG_SET_DIAG_SW_CMD_OPCODE    0xfc56

//set ble channel assessment parameter
#define HCI_DBG_SET_BLE_CA_PARA_CMD_OPCODE  0xfc57

//set afh algorithm
#define HCI_DBG_SET_AFH_ALGORITHM_CMD_OPCODE  0xfc58

//set local feature
#define HCI_DBG_SET_LOCAL_FEATURE_CMD_OPCODE   0xfc59

//set local extend feature
#define HCI_DBG_SET_LOCAL_EX_FEATURE_CMD_OPCODE  0xfc5a

//set  ble rf timing
#define HCI_DBG_SET_BT_RF_TIMING_CMD_OPCODE   0xfc5b

//set ble rf timig
#define HCI_DBG_SET_BLE_RF_TIMING_CMD_OPCODE  0xfc5c

//set ble rl size
#define HCI_DBG_SET_RL_SIZE_CMD_OPCODE   0xfc5d

///set hostwake
#define HCI_DBG_SET_HOSTWAKE_CMD_OPCODE  0xfc5e

#define HCI_DBG_SET_BT_SETTING_CMD_OPCODE  0xfc5f

// xiao add for nonsignaling test mode
#define HCI_DBG_NONSIG_TESTER_SETUP_CMD_OPCODE        0xFC60

/* xiao add for custom set param*/
#define HCI_DBG_SET_CUSTOM_PARAM_CMD_OPCODE        0xFC61


#ifdef __cplusplus
}
#endif

#endif

