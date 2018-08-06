#include "tgt_hardware.h"
#include "iir_process.h"
#include "fir_process.h"
#include "dual_mic_denoise.h"
#include "coherent_denoise.h"
#include "hw_codec_iir_process.h"
#include "bt_sco_chain_webrtc.h"

const struct HAL_IOMUX_PIN_FUNCTION_MAP cfg_hw_pinmux_pwl[CFG_HW_PLW_NUM] = {
    {HAL_IOMUX_PIN_P0_0, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
    {HAL_IOMUX_PIN_P0_1, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE},
};

//adckey define
const uint16_t CFG_HW_ADCKEY_MAP_TABLE[CFG_HW_ADCKEY_NUMBER] = {
    HAL_KEY_CODE_FN9,HAL_KEY_CODE_FN8,HAL_KEY_CODE_FN7,
    HAL_KEY_CODE_FN6,HAL_KEY_CODE_FN5,HAL_KEY_CODE_FN4,
    HAL_KEY_CODE_FN3,HAL_KEY_CODE_FN2,HAL_KEY_CODE_FN1
};

//gpiokey define
#define CFG_HW_GPIOKEY_DOWN_LEVEL          (0)
#define CFG_HW_GPIOKEY_UP_LEVEL            (1)
const struct HAL_KEY_GPIOKEY_CFG_T cfg_hw_gpio_key_cfg[CFG_HW_GPIOKEY_NUM] = {
    {HAL_KEY_CODE_FN1,{HAL_IOMUX_PIN_P1_0, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE}},
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
    {TX_PA_GAIN,0x03,-11},
    {TX_PA_GAIN,0x03,-99},
    {TX_PA_GAIN,0x03,-45},
    {TX_PA_GAIN,0x03,-42},
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
    HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};

const struct HAL_IOMUX_PIN_FUNCTION_MAP app_battery_ext_charger_indicator_cfg = {
    HAL_IOMUX_PIN_NUM, HAL_IOMUX_FUNC_AS_GPIO, HAL_IOMUX_PIN_VOLTAGE_VIO, HAL_IOMUX_PIN_PULLUP_ENALBE
};

const IIR_CFG_T audio_eq_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0.15,
    .num = 0,
    .param = {
        {0.0,   50.0,   0.7},
        {0.0,   250.0,  0.7},
        {0.0,   1000.0, 0.7},
        {12.0,   4000.0, 0.7},
        {12.0,   8000.0, 0.7}
    }
};

//hardware dac iir eq
const IIR_CFG_T audio_eq_hw_dac_iir_cfg = {
    .gain0 = 0,
    .gain1 = 0,
    .num = 0,
    .param = {
        {IIR_TYPE_PEAK, 0.0,   5000.0,   0.6},
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

const DUAL_MIC_DENOISE_CFG_T dual_mic_denoise_cfg =
{
    .alpha_h            = 0.8,
    .alpha_slow         = 0.9,
    .alpha_fast         = 0.6,
    .thegma             = 0.01,
    .thre_corr          = 0.2,
    .thre_filter_diff   = 0.2,
    .cal_left_gain      = 1.0,
    .cal_right_gain     = 1.0,
    .delay_mono_sample  = 6,
};

const COHERENT_DENOISE_CFG_T coherent_denoise_cfg =
{
    // MIC L/R??辰?213ㄓ
    .left_gain              = 1.0f,
    .right_gain             = 1.0f,
    // mic L/R delay sample. 0: 那那車?車迆?車???角角??a<2cm; 1: 那那車?車迆?車???角角??a2cm℅車車辰; 2: 那那車?車迆?車???角角??a4cm℅車車辰
    .delaytaps              = 0, 
    // 1: ?米車辰??2“∩辰?a; 0: ?米車辰??2“1?㊣?; ??豕?∩辰?a       
    .freq_smooth_enable     = 1,
    // ??豕?1?㊣?
    .low_quality_enable     = 0, 
};

const AEC_CFG_T aec_cfg =
{
    .enEAecEnable       = 1,
    .enHpfEnable        = 1,
    .enAfEnable         = 0,
    .enNsEnable         = 0,
    .enNlpEnable        = 1,
    .enCngEnable        = 0,
    .shwDelayLength     = 0,
    .shwNlpTargetSupp   = (3<<10),
    .shwNlpMinOvrd      = (1<<11),
};