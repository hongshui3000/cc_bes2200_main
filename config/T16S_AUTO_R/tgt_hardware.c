#include "tgt_hardware.h"
#include "iir_process.h"
#include "fir_process.h"
#include "hw_codec_iir_process.h"
#include "coherent_denoise.h"

const struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_pinmux_pwl[CFG_HW_PLW_NUM] = {
#ifdef __HW_PWM_CONTROL_LED__
	{HAL_GPIO_PIN_P2_6, HAL_IOMUX_FUNC_PWM2, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
#else
	{HAL_GPIO_PIN_LED2, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
	{HAL_GPIO_PIN_LED1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
#endif
};

//adckey define
const uint16_t CFG_HW_ADCKEY_MAP_TABLE[CFG_HW_ADCKEY_NUMBER];

//gpiokey define
#define CFG_HW_GPIOKEY_DOWN_LEVEL          (0)
#define CFG_HW_GPIOKEY_UP_LEVEL            (1)
const struct HAL_KEY_GPIOKEY_CFG_T cfg_hw_gpio_key_cfg[CFG_HW_GPIOKEY_NUM] = {
    {HAL_KEY_CODE_FN1,{HAL_IOMUX_PIN_P2_1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
    {HAL_KEY_CODE_FN2,{HAL_IOMUX_PIN_P1_1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
};

//bt config
const char *BT_LOCAL_NAME = TO_STRING(BT_DEV_NAME) "\0";
uint8_t ble_addr[6] = {
#ifdef BLE_DEV_ADDR
	BLE_DEV_ADDR
#else
	0xBE,0x99,0x34,0x45,0x56,0x67
#endif
};
uint8_t bt_addr[6] = {
#ifdef BT_DEV_ADDR
	BT_DEV_ADDR
#else
	0x00,0x22,0x34,0x33,0x22,0x11
#endif
};

//audio config
//freq bands range {[0k:2.5K], [2.5k:5K], [5k:7.5K], [7.5K:10K], [10K:12.5K], [12.5K:15K], [15K:17.5K], [17.5K:20K]}
//gain range -12~+12
int8_t cfg_hw_aud_eq_band_settings[CFG_HW_AUD_EQ_NUM_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0};
const int8_t cfg_aud_eq_sbc_band_settings[CFG_HW_AUD_EQ_NUM_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0};

#define TX_PA_GAIN                          CODEC_TX_PA_GAIN_DEFAULT

const struct CODEC_DAC_VOL_T codec_dac_vol[TGT_VOLUME_LEVEL_QTY] = {
    {TX_PA_GAIN,0x03,-18},
    {TX_PA_GAIN,0x03,-99},
    {TX_PA_GAIN,0x03,-70},
    {TX_PA_GAIN,0x03,-45},
    {TX_PA_GAIN,0x03,-39},
    {TX_PA_GAIN,0x03,-36},
    {TX_PA_GAIN,0x03,-33},
    {TX_PA_GAIN,0x03,-30},
    {TX_PA_GAIN,0x03,-27},
    {TX_PA_GAIN,0x03,-24},
    {TX_PA_GAIN,0x03,-21},
    {TX_PA_GAIN,0x03,-18},
    {TX_PA_GAIN,0x03,-15},
    {TX_PA_GAIN,0x03,-12},
    {TX_PA_GAIN,0x03, -9},
    {TX_PA_GAIN,0x03, -6},
    {TX_PA_GAIN,0x03, -3},
    {TX_PA_GAIN,0x03,  0},  //0dBm
};

const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_detecter_cfg = {
	HAL_IOMUX_PIN_P1_5, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};

const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_indicator_cfg = {
    HAL_IOMUX_PIN_P1_5, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};

const IIR_CFG_T audio_eq_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 5,
    .param = {
        {IIR_TYPE_PEAK, .0,   200,   2},
        {IIR_TYPE_PEAK, .0,   600,  2},
        {IIR_TYPE_PEAK, .0,   2000.0, 2},
        {IIR_TYPE_PEAK, .0,  6000.0, 2},
        {IIR_TYPE_PEAK, .0,  12000.0, 2}
    }
};

//hardware dac iir eq
const IIR_CFG_T audio_eq_hw_dac_iir_cfg = {
#ifdef __TWS_CHANNEL_RIGHT__
	.gain0 = -1,
	.gain1 = -1,
#else
 	.gain0 = -1,
 	.gain1 = -1,
#endif
    .num = 3,
    .param = {
		{IIR_TYPE_PEAK, 2.0,   80.0,   0.4},
		{IIR_TYPE_PEAK, -4.0,   240.0,   0.9},
		{IIR_TYPE_PEAK, -7.0,  8000.0,   4},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_dac_iir_cfg_list[EQ_DAC_IIR_LIST_NUM]={
    &audio_eq_hw_dac_iir_cfg,
};

//hardware adc iir eq
const IIR_CFG_T audio_eq_hw_adc_iir_adc_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 0,
    .param = {
        {IIR_TYPE_PEAK, 0.0,   50.0,   0.7},
    }
};

const IIR_CFG_T * const POSSIBLY_UNUSED audio_eq_hw_adc_iir_cfg_list[EQ_ADC_IIR_LIST_NUM]={
    &audio_eq_hw_adc_iir_adc_cfg,
};

const FIR_CFG_T audio_eq_fir_cfg = {
    .gain0 = 6,
    .gain1 = 6,
    .len = 128,
    .coef = {
        #include "res/eq/EQ_COEF.txt"
    }
};

const FIR_CFG_T speech_spk_eq_16k_cfg = {
    .gain0 = 6,
    .gain1 = 6,
    .len = 99,
    .coef = {
        8589,15275,1642,-9312,-6090,-3375,-3148,-2896,-2284,-1742,-1330,-968,-650,-381,-157,26,171,284,368,427,465,485,491,484,468,445,416,384,350,315,280,245,212,181,153,127,102,81,62,45,31,19,8,0,-7,-12,-16,-19,-20,-21,-21,-21,-21,-20,-18,-17,-15,-14,-12,-11,-10,-8,-7,-6,-5,-4,-3,-2,-2,-1,-1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
    }
};

const FIR_CFG_T speech_spk_eq_8k_cfg = {
    .gain0 = 6,
    .gain1 = 6,
    .len = 44,
    .coef = {
        26233,-11516,-8568,-5989,-3854,-2175,-924,-47,520,841,979,990,917,796,655,512,379,263,168,93,38,-1,-25,-39,-45,-45,-41,-36,-29,-23,-17,-12,-7,-4,-2,0,1,2,2,2,2,2,1,1,
    }
};

const COHERENT_DENOISE_CFG_T coherent_denoise_cfg =
{
    .left_gain              = 1.0f,
    .right_gain             = 1.0f,
    .delaytaps              = 0, 
    .freq_smooth_enable     = 1,
    .low_quality_enable     = 0, 
};