#include "plat_types.h"
#include "hal_location.h"
#include "resample_coef.h"

#define COEF_DEF(n)                         FLASH_RODATA_DEF(n)

static const int16_t COEF_DEF(filter_50p7k_to_48k)[] = {
    #include "resample_50p7k_to_48k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_50p7k_to_48k) = {
    .upsample_factor = 69,
    .downsample_factor = 73,
    .phase_coef_num = 6,
    .total_coef_num = ARRAY_SIZE(filter_50p7k_to_48k),
    .coef_group = filter_50p7k_to_48k,
};

static const int16_t COEF_DEF(filter_50p7k_to_44p1k)[] = {
    #include "resample_50p7k_to_44p1k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_50p7k_to_44p1k) = {
    .upsample_factor = 33,
    .downsample_factor = 38,
    .phase_coef_num = 6,
    .total_coef_num = ARRAY_SIZE(filter_50p7k_to_44p1k),
    .coef_group = filter_50p7k_to_44p1k,
};

static const int16_t COEF_DEF(filter_44p1k_to_48k)[] = {
    #include "resample_44p1k_to_48k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_44p1k_to_48k) = {
    .upsample_factor = 160,
    .downsample_factor = 147,
    .phase_coef_num = 8,
    .total_coef_num = ARRAY_SIZE(filter_44p1k_to_48k),
    .coef_group = filter_44p1k_to_48k,
};

static const int16_t COEF_DEF(filter_48k_to_44p1k)[] = {
    #include "resample_48k_to_44p1k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_48k_to_44p1k) = {
    .upsample_factor = 147,
    .downsample_factor = 160,
    .phase_coef_num = 8,
    .total_coef_num = ARRAY_SIZE(filter_48k_to_44p1k),
    .coef_group = filter_48k_to_44p1k,
};

static const int16_t COEF_DEF(filter_32k_to_50p7k)[] = {
    #include "resample_32k_to_50p7k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_32k_to_50p7k) = {
    .upsample_factor = 73,
    .downsample_factor = 46,
    .phase_coef_num = 24,
    .total_coef_num = ARRAY_SIZE(filter_32k_to_50p7k),
    .coef_group = filter_32k_to_50p7k,
};

static const int16_t COEF_DEF(filter_44p1k_to_50p7k)[] = {
    #include "resample_44p1k_to_50p7k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_44p1k_to_50p7k) = {
    .upsample_factor = 38,
    .downsample_factor = 33,
    .phase_coef_num = 24,
    .total_coef_num = ARRAY_SIZE(filter_44p1k_to_50p7k),
    .coef_group = filter_44p1k_to_50p7k,
};

static const int16_t COEF_DEF(filter_48k_to_50p7k)[] = {
    #include "resample_48k_to_50p7k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_48k_to_50p7k) = {
    .upsample_factor = 73,
    .downsample_factor = 69,
    .phase_coef_num = 24,
    .total_coef_num = ARRAY_SIZE(filter_48k_to_50p7k),
    .coef_group = filter_48k_to_50p7k,
};

static const int16_t COEF_DEF(filter_8k_to_8p4k)[] = {
    #include "resample_8k_to_8p4k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_8k_to_8p4k) = {
    .upsample_factor = 73,
    .downsample_factor = 69,
    .phase_coef_num = 18,
    .total_coef_num = ARRAY_SIZE(filter_8k_to_8p4k),
    .coef_group = filter_8k_to_8p4k,
};

static const int16_t COEF_DEF(filter_8p4k_to_8k)[] = {
    #include "resample_8p4k_to_8k_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_8p4k_to_8k) = {
    .upsample_factor = 69,
    .downsample_factor = 73,
    .phase_coef_num = 16,
    .total_coef_num = ARRAY_SIZE(filter_8p4k_to_8k),
    .coef_group = filter_8p4k_to_8k,
};

static const int16_t COEF_DEF(filter_any_up64)[] = {
    #include "resample_any_up64_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_any_up64) = {
    .upsample_factor = 64,
    .downsample_factor = 0,
    .phase_coef_num = 32,
    .total_coef_num = ARRAY_SIZE(filter_any_up64),
    .coef_group = filter_any_up64,
};

static const int16_t COEF_DEF(filter_any_up256)[] = {
    #include "resample_any_up256_filter.txt"
};
const struct RESAMPLE_COEF_T COEF_DEF(resample_coef_any_up256) = {
    .upsample_factor = 256,
    .downsample_factor = 0,
    .phase_coef_num = 24,
    .total_coef_num = ARRAY_SIZE(filter_any_up256),
    .coef_group = filter_any_up256,
};

