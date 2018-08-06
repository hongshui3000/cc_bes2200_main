#ifndef __SBCACC_HEADER__
#define __SBCACC_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

int Synthesis1_init(const char *coef, int coef_len, int channel_num);
int Synthesis2_init(const char *coef, int coef_len, int channel_num);
int Synthesis_init(const char *coef1, int coef_len1, const char *coef2, int coef_len2, int channel_num);

#ifdef __cplusplus
}
#endif

#endif /* __SBCACC_HEADER__ */
