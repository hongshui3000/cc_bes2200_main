
#ifndef _MED_AEC_COMM_H_
#define _MED_AEC_COMM_H_

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "codec_typedefine.h"
#include "med_fft.h"
#include "codec_com_codec.h"
#include "med_aec_main.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/
/* FFT IFFT�ص����� */
typedef VOS_VOID (*MED_AEC_FFT_CALLBACK)(VOS_INT16 *, VOS_INT16 *);

#ifdef AEC_ONLY_SUPPORT_8k
#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_FFT_LEN_NB              (256)
#else
#define MED_AEC_FFT_LEN_NB              (240)
#endif
#define MED_AEC_MAX_FFT_LEN             (MED_AEC_FFT_LEN_NB)                    /* ���FFT���� */
#else
#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_FFT_LEN_NB              (256)
#define MED_AEC_FFT_LEN_WB              (512)
#else
#define MED_AEC_FFT_LEN_NB              (240)
#define MED_AEC_FFT_LEN_WB              (480)
#endif
#define MED_AEC_MAX_FFT_LEN             (MED_AEC_FFT_LEN_WB)                    /* ���FFT���� */
#endif

#define MED_AEC_HF_AF_M                 (2)

/* ��ʱȫ�����飬��AEC��ģ��ʹ�� */
//extern VOS_INT16                        g_ashwMedAecTmp1Len640[MED_AEC_MAX_FFT_LEN];
//extern VOS_INT16                        g_ashwMedAecTmp2Len640[MED_AEC_MAX_FFT_LEN];
//extern VOS_INT32                        g_aswMedAecTmp1Len320[CODEC_FRAME_LENGTH_WB];
//extern VOS_INT32                        g_aswMedAecTmp1Len640[MED_AEC_MAX_FFT_LEN];

/* AEC �����궨��*/
//#define MED_AEC_GetshwVecTmp640Ptr1()   (&g_ashwMedAecTmp1Len640[0])            /* ��ʱȫ������ָ�� ����640 INT16 */
//#define MED_AEC_GetshwVecTmp640Ptr2()   (&g_ashwMedAecTmp2Len640[0])            /* ��ʱȫ������ָ�� ����640 INT16 */
//#define MED_AEC_GetswVecTmp320Ptr1()    (&g_aswMedAecTmp1Len320[0])             /* ��ʱȫ������ָ�� ����320 INT32 */
//#define MED_AEC_GetswVecTmp640Ptr1()    (&g_aswMedAecTmp1Len640[0])             /* ��ʱȫ������ָ�� ����640 INT32 */

#define MED_AEC_OFFSET_THD              (60)                                    /* �ӳٲ�����ֵ */
#define MED_AEC_MAX_OFFSET              (960)                                   /* ��󲹳����ȣ���λ������ Q0*/
#define MED_AEC_MAX_TAIL_LEN            (960)                                   /* ���β�˳��ȣ���λms�����֧��60msβ���ӳ� Q0*/

#define MED_AEC_MAX_DTD_ST_MODI_THD     (5000)                                  /* DTD���������������ֵ Q0*/
#define MED_AEC_MAX_DTD_DT_MODI_THD     (30000)                                 /* DTD˫�������������ֵQ15*/
#define MED_AEC_MAX_DTD_POWER_THD       (30000)                                 /* DTD�����������ֵ Q0*/
#define MED_AEC_MAX_NLP_DT2ST_THD       (20)                                    /* NLP˫���л������������ֵ Q0*/
#define MED_AEC_MAX_NLP_ST2DT_THD       (20)                                    /* NLP�����л�˫���������ֵ Q0*/
#define MED_AEC_MAX_NLP_CNG_THD         (2000)                                  /* NLP�������������������ֵ Q0*/
#define MED_AEC_MAX_NLP_NOISE_FLOOR_THD (256)                                   /* NLP���������������ֵ Q0*/
#define MED_AEC_MAX_NLP_MAX_SUPPS_LVL   (32767)                                 /* NLP��Ƶ���Ƽ������ֵ Q15*/
#define MED_AEC_MAX_NLP_NON_LINEAR_THD  (5000)                                  /* NLP��Ƶ�����������������ֵ Q0*/
#define MED_AEC_MAX_DTD_EST_POWER_THD   (30000)                                 /* ��ǿ����ʹ��ʱ������ֵ���ֵ Q0*/
#define MED_AEC_MAX_DTD_EST_AMPL_THD    (30000)                                 /* ��ǿ����ʹ��ʱ������ֵ���ֵ Q0*/
#define MED_AEC_DTD_EST_POWER_BASE      (1000)                                  /* ��ǿ����ʹ��ʱ������ֵ��λֵ Q0*/
#define MED_AEC_DTD_EST_HANG_OVER_LEN   (4)                                     /* ��ǿ����ʹ��ʱhangover֡���� Q0*/

#define MED_AEC_MAX_DTD_NLINE_NEAR_FAR_RATIO_GAIN   (32765)                     /* ������DTDԶ�˹������ܶȵ��������ֵ Q11 */
#define MED_AEC_MAX_DTD_NLINE_SP_SER_THD            (32765)                     /* ������DTD�������ڸ���Ϊ1��SER�������ֵ Q11 */
#define MED_AEC_MAX_DTD_NLINE_ECHO_SER_THD          (32765)                     /* ������DTD�������ڸ���Ϊ0��SER�������ֵ Q11 */
#define MED_AEC_MAX_DTD_NLINE_BAND_PSD_MUTE_THD     (32765)                     /* ������DTD���˹������ܶ���Ϊ�������������ֵ Q0 */

#define MED_AEC_MAX_DTD_NLINE_SP_THD_INIT           (32000)                     /* ������DTD˫���ж��������ڸ�����ֵ�����ֵ Q15 */
#define MED_AEC_MAX_DTD_NLINE_SP_THD_MAX            (32000)                     /* ������DTD˫���ж��������ڸ�����ֵ���ֵ�����ֵ Q15 */
#define MED_AEC_MAX_DTD_NLINE_SP_THD_MIN            (32000)                     /* ������DTD˫���ж��������ڸ�����ֵ��Сֵ�����ֵ Q15 */
#define MED_AEC_MAX_DTD_NLINE_SUM_PSD_THD           (32000)                     /* ������DTD˫���ж�ʣ��������ֵ���ֵ�����ֵ Q15 */

#define MED_AEC_MAX_NLP_OVERDRIVE_FAR_CNT           (30000)                     /* Զ���źż������ֵ  Q0 */
#define MED_AEC_MAX_NLP_OVERDRIVE_FAR_THD           (30000)                     /* Զ���źż����������ֵ  Q0 */
#define MED_AEC_MAX_NLP_OVERDRIVE_MAX               (32000)                     /* OVERDRIVE���ֵ  Q0 */
#define MED_AEC_MAX_NLP_STSUPPRESS_ALPH             (32000)                     /* OVERDRIVE���ֵ  Q0 */
#define MED_AEC_MAX_NLP_STSUPPRESS_POWTHD           (10000)                     /* OVERDRIVE���ֵ  Q0 */
#define MED_AEC_MAX_NLP_SMOOTH_GAIN_DOD             (32767)                     /* �������ӵ���ָ�������ֵ*/
#define MED_AEC_MAX_NLP_BANDSORT_IDX                (60)                        /*  NLP: �������������� */
/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/
/* AF FFT���� */
enum MED_AEC_AF_FFT_LEN_ENUM
{
#ifdef SPEECH_STREAM_UNIT_128
    MED_AEC_AF_FFT_LEN_NB = 256,                                                /* խ��FFT���� */
    MED_AEC_AF_FFT_LEN_WB = 512,                                                /* ���FFT���� */
#else
    MED_AEC_AF_FFT_LEN_NB = 240,                                                /* խ��FFT���� */
    MED_AEC_AF_FFT_LEN_WB = 480,                                                /* ���FFT���� */
#endif
    MED_AEC_AF_FFT_LEN_BUTT
};
typedef VOS_INT16  MED_AEC_AF_FFT_LEN_ENUM_INT16;

/* NLP FFT���� */
enum MED_AEC_NLP_FFT_LEN_ENUM
{
    MED_1MIC_AEC_NLP_FFT_LEN_NB = 256,                                          /* 1MICխ��FFT���� */
    MED_1MIC_AEC_NLP_FFT_LEN_WB = 512,                                          /* 1MIC���FFT���� */
    MED_2MIC_AEC_NLP_FFT_LEN_NB = 320,                                          /* 2MICխ��FFT���� */
    MED_2MIC_AEC_NLP_FFT_LEN_WB = 640,                                          /* 2MIC���FFT���� */
    MED_AEC_NLP_FFT_LEN_BUTT
};
typedef VOS_INT16  MED_AEC_NLP_FFT_LEN_ENUM_INT16;

/* DTD����־ */
enum MED_AEC_DTD_FLAG_ENUM
{
    MED_AEC_DTD_FLAG_ST,                                                        /* ���� */
    MED_AEC_DTD_FLAG_DT,                                                        /* ˫�� */
    MED_AEC_DTD_FLAG_PASS,                                                      /* ȫͨ */
    MED_AEC_DTD_FLAG_BUTT
};
typedef VOS_INT16 MED_AEC_DTD_FLAG_ENUM_INT16;

/* ������ö�� */
enum CODEC_SWITCH_ENUM
{
	CODEC_SWITCH_OFF = 0,
	CODEC_SWITCH_ON,
	CODEC_SWITCH_BUTT
};
typedef VOS_UINT16 CODEC_SWITCH_ENUM_UINT16;

/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/

/*****************************************************************************
  5 STRUCT����
*****************************************************************************/

/*****************************************************************************
  6 UNION����
*****************************************************************************/

/*****************************************************************************
  7 OTHERS����
*****************************************************************************/

/*****************************************************************************
  8 ��������
*****************************************************************************/
#ifdef DEBUG_AEC
static VOS_VOID fprint_vec_int32(FILE *fd, VOS_INT32 *buf, VOS_INT32 len)
{
    for (VOS_INT32 i = 0; i < len - 1; i++) {
        fprintf(fd, "%d ", buf[i]);
    }
    fprintf(fd, "%d\n", buf[len - 1]);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of med_aec_main.h*/

