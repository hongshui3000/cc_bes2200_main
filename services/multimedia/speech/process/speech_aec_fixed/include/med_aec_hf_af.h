

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "med_aec_comm.h"

#ifndef _MED_AEC_HF_AF_H_
#define _MED_AEC_HF_AF_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/
/* AF �궨��*/
#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_HF_AF_FILT_NB           (256)                                   /* խ��MDF�˲��鳤�� */
#else
#define MED_AEC_HF_AF_FILT_NB           (240)                                   /* խ��MDF�˲��鳤�� */
#endif
#define MED_AEC_HF_AF_W_QN_FILT         (13)                                    /* Filter������W ԭʼ���꾫�� */
#define MED_AEC_HF_AF_WEIGPOWER_THD     (8)                                     /* IFFT���������źŵ��������2^9 */
#define MED_AEC_HF_AF_ERRABS_QN         (9)                                     /* Ƶ������źŵľ���ֵ�Ŵ���*/
#define MED_AEC_HF_AF_BIGNORMERR_QN     (13)                                    /* �˲���ϵ�������������ʱ���꾫�� */
#define MED_AEC_HF_AF_SMALLNORMERR_QN   (20)                                    /* �˲���ϵ�������������ʱ���꾫�� */
#define MED_AEC_HF_AF_Q5                (32)                                    /* 2^5 */
/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/


/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/

/*****************************************************************************
  5 STRUCT����
*****************************************************************************/
/* AF�����ṹ�� */

/* AFϵ�����½ṹ�� */
typedef struct
{
    VOS_INT16                           shwStep;
    VOS_INT16                           shwPowTh;
    VOS_INT32                           swFrmCount;
    VOS_INT16                           shwSegmLen;
    VOS_INT16                           shwAlphFac;
    VOS_INT16                           shwMaxWIdx;
    VOS_INT16                           shwReserve;
    VOS_INT32                           aswWeight[MED_AEC_HF_AF_M][MED_AEC_MAX_FFT_LEN];
    VOS_INT32                           aswFarPow[(MED_AEC_MAX_FFT_LEN / 2) + 1];
	VOS_INT32                           aswGrad[MED_AEC_HF_AF_M][MED_AEC_MAX_FFT_LEN];
} MED_AEC_HF_COEFUPDT_STRU;

/* AF״̬ */
typedef struct
{
    MED_AEC_AF_FFT_LEN_ENUM_INT16       shwFftLen;                              /* FFT���� */
    MED_FFT_NUM_ENUM_INT16              enFftNumIndex;                          /* FFT��������ö�� */
    MED_FFT_NUM_ENUM_INT16              enIfftNumIndex;                         /* IFFT��������ö�� */
    VOS_INT16                           shwReserve;
    VOS_INT16                           ashwFar[MED_AEC_MAX_FFT_LEN];
    VOS_INT16                           ashwFarFreq[MED_AEC_HF_AF_M][MED_AEC_MAX_FFT_LEN];
    VOS_INT16                           ashwNear[MED_AEC_MAX_FFT_LEN];
    MED_AEC_HF_COEFUPDT_STRU            stCoefUpdt;
} MED_AEC_HF_AF_STRU;


/*****************************************************************************
  6 UNION����
*****************************************************************************/

/*****************************************************************************
  7 OTHERS����
*****************************************************************************/

/*****************************************************************************
  8 ��������
*****************************************************************************/
extern VOS_VOID MED_AEC_HF_AfInit(
                CODEC_SAMPLE_RATE_MODE_ENUM_INT32 enSampleRate,
                MED_AEC_HF_AF_STRU               *pstHfAf);

extern VOS_VOID MED_AEC_HF_AfMain (
                MED_AEC_HF_AF_STRU            *pstHfAf,
                VOS_INT16                     *pshwNear,
                VOS_INT16                     *pshwFar,
                VOS_INT16                     *pshwErr);

extern VOS_VOID MED_AEC_HF_AfFilt(
                VOS_INT16  ashwFarFreq[][MED_AEC_MAX_FFT_LEN],
                VOS_INT32  aswWeight[][MED_AEC_MAX_FFT_LEN],
                VOS_INT16  enFftLen,
                VOS_INT16  enIfftNumIndex,
                VOS_INT16 *pshwEcho);

extern VOS_VOID MED_AEC_HF_AfErrorCalc(
                VOS_INT16 *pshwEcho,
                VOS_INT16 *pshwNear,
                VOS_INT16  enFftLen,
                VOS_INT16  enFftNumIndex,
                VOS_INT16 *pshwErr,
                VOS_INT16 *pshwErrFreq);

extern VOS_VOID MED_AEC_HF_AfCoefUpdt(
                MED_AEC_HF_COEFUPDT_STRU *pstCoefUpdt,
                VOS_INT16 *pshwErrFreq,
                VOS_INT16  ashwFarFreq[][MED_AEC_MAX_FFT_LEN],
                VOS_INT16  enFftLen,
                VOS_INT16  enFftNumIndex,
                VOS_INT16  enIfftNumIndex);

extern VOS_VOID MED_AEC_HF_AfCoefTDMang(
                VOS_INT32  aswGrad[][MED_AEC_MAX_FFT_LEN],
                VOS_INT16  shwFftLen,
                VOS_INT16  enIfftNumIndex,
                VOS_INT16  enFftNumIndex,
                VOS_INT32  aswWeight[][MED_AEC_MAX_FFT_LEN]);

extern VOS_VOID MED_AEC_HF_AfEchoLocat(
                MED_AEC_HF_COEFUPDT_STRU   *pstCoefUpdt,
                VOS_INT16                   shwFftLen);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of med_aec_af.h*/

