#ifndef __HAL_IOMUX_BEST1000_H__
#define __HAL_IOMUX_BEST1000_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "plat_types.h"

enum HAL_IOMUX_PIN_T {
    HAL_IOMUX_PIN_P0_0 = 0,
    HAL_IOMUX_PIN_P0_1,
    HAL_IOMUX_PIN_P0_2,
    HAL_IOMUX_PIN_P0_3,
    HAL_IOMUX_PIN_P0_4,
    HAL_IOMUX_PIN_P0_5,
    HAL_IOMUX_PIN_P0_6,
    HAL_IOMUX_PIN_P0_7,

    HAL_IOMUX_PIN_P1_0,
    HAL_IOMUX_PIN_P1_1,
    HAL_IOMUX_PIN_P1_2,
    HAL_IOMUX_PIN_P1_3,
    HAL_IOMUX_PIN_P1_4,
    HAL_IOMUX_PIN_P1_5,
    HAL_IOMUX_PIN_P1_6,
    HAL_IOMUX_PIN_P1_7,

    HAL_IOMUX_PIN_P2_0,
    HAL_IOMUX_PIN_P2_1,
    HAL_IOMUX_PIN_P2_2,
    HAL_IOMUX_PIN_P2_3,
    HAL_IOMUX_PIN_P2_4,
    HAL_IOMUX_PIN_P2_5,
    HAL_IOMUX_PIN_P2_6,
    HAL_IOMUX_PIN_P2_7,

    HAL_IOMUX_PIN_P3_0,
    HAL_IOMUX_PIN_P3_1,
    HAL_IOMUX_PIN_P3_2,
    HAL_IOMUX_PIN_P3_3,
    HAL_IOMUX_PIN_P3_4,
    HAL_IOMUX_PIN_P3_5,
    HAL_IOMUX_PIN_P3_6,
    HAL_IOMUX_PIN_P3_7,

    HAL_IOMUX_PIN_NUM,
};

enum HAL_IOMUX_FUNCTION_T {
    HAL_IOMUX_FUNC_AS_GPIO = 0,
    HAL_IOMUX_FUNC_UART0_RX,
    HAL_IOMUX_FUNC_UART0_TX,
    HAL_IOMUX_FUNC_UART0_CTS,
    HAL_IOMUX_FUNC_UART0_RTS,
    HAL_IOMUX_FUNC_UART1_RX,
    HAL_IOMUX_FUNC_UART1_TX,
    HAL_IOMUX_FUNC_UART1_CTS,
    HAL_IOMUX_FUNC_UART1_RTS,
    HAL_IOMUX_FUNC_BT_UART_RX,
    HAL_IOMUX_FUNC_BT_UART_TX,
    HAL_IOMUX_FUNC_BT_UART_CTS,
    HAL_IOMUX_FUNC_BT_UART_RTS,
    HAL_IOMUX_FUNC_I2C_SCL,
    HAL_IOMUX_FUNC_I2C_SDA,
    HAL_IOMUX_FUNC_I2S_SCK,
    HAL_IOMUX_FUNC_I2S_WS,
    HAL_IOMUX_FUNC_I2S_SDI0,
    HAL_IOMUX_FUNC_I2S_SDO,
    HAL_IOMUX_FUNC_SPDIF0_DI,
    HAL_IOMUX_FUNC_SPDIF0_DO,
    HAL_IOMUX_FUNC_SPI_CS,
    HAL_IOMUX_FUNC_SPI_DI,
    HAL_IOMUX_FUNC_SPI_DIO,
    HAL_IOMUX_FUNC_SPI_CLK,
    HAL_IOMUX_FUNC_SPILCD_CS,
    HAL_IOMUX_FUNC_SPILCD_DI,
    HAL_IOMUX_FUNC_SPILCD_DIO,
    HAL_IOMUX_FUNC_SPILCD_DCN,
    HAL_IOMUX_FUNC_SPILCD_CLK,
    HAL_IOMUX_FUNC_SDIO_CLK,
    HAL_IOMUX_FUNC_SDIO_CMD,
    HAL_IOMUX_FUNC_SDIO_DATA0,
    HAL_IOMUX_FUNC_SDIO_DATA1,
    HAL_IOMUX_FUNC_SDIO_DATA2,
    HAL_IOMUX_FUNC_SDIO_DATA3,
    HAL_IOMUX_FUNC_SDIO_INT_M,
    HAL_IOMUX_FUNC_SDMMC_CLK,
    HAL_IOMUX_FUNC_SDMMC_CMD,
    HAL_IOMUX_FUNC_SDMMC_DATA0,
    HAL_IOMUX_FUNC_SDMMC_DATA1,
    HAL_IOMUX_FUNC_SDMMC_DATA2,
    HAL_IOMUX_FUNC_SDMMC_DATA3,
    HAL_IOMUX_FUNC_SDMMC_DATA4,
    HAL_IOMUX_FUNC_SDMMC_DATA5,
    HAL_IOMUX_FUNC_SDMMC_DATA6,
    HAL_IOMUX_FUNC_SDMMC_DATA7,
    HAL_IOMUX_FUNC_SDEMMC_RST_N,
    HAL_IOMUX_FUNC_SDEMMC_DT_N,
    HAL_IOMUX_FUNC_SDEMMC_WP,
    HAL_IOMUX_FUNC_PWM0,
    HAL_IOMUX_FUNC_PWM1,
    HAL_IOMUX_FUNC_PWM2,
    HAL_IOMUX_FUNC_PWM3,
    HAL_IOMUX_FUNC_PDM0_D,
    HAL_IOMUX_FUNC_PDM0_CK,
    HAL_IOMUX_FUNC_CLK_32K_IN,
    HAL_IOMUX_FUNC_CLK_OUT,
    HAL_IOMUX_FUNC_CLK_REQ_IN,
    HAL_IOMUX_FUNC_CLK_REQ_OUT,
    HAL_IOMUX_FUNC_PTA_TX_CONF,
    HAL_IOMUX_FUNC_PTA_STATUS,
    HAL_IOMUX_FUNC_PTA_RF_ACT,
    HAL_IOMUX_FUNC_SMP0,
    HAL_IOMUX_FUNC_SMP1,
    HAL_IOMUX_FUNC_SMP2,
    HAL_IOMUX_FUNC_SMP3,
    HAL_IOMUX_FUNC_SMP4,
    HAL_IOMUX_FUNC_SMP5,
    HAL_IOMUX_FUNC_SMP6,
    HAL_IOMUX_FUNC_SMP7,
    HAL_IOMUX_FUNC_RF_ANA_SWTX,
    HAL_IOMUX_FUNC_RF_ANA_SWRX,

    HAL_IOMUX_FUNC_END = 0xFFFFFFFF,
};

enum HAL_IOMUX_ISPI_ACCESS_T {
    HAL_IOMUX_ISPI_BT_RF            = (1 << 0),
    HAL_IOMUX_ISPI_BT_PMU           = (1 << 1),
    HAL_IOMUX_ISPI_BT_ANA           = (1 << 2),
    HAL_IOMUX_ISPI_MCU_RF           = (1 << 3),
    HAL_IOMUX_ISPI_MCU_PMU          = (1 << 4),
    HAL_IOMUX_ISPI_MCU_ANA          = (1 << 5),
};

#ifdef __cplusplus
}
#endif

#endif
