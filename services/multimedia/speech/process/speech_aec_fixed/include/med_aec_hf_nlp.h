

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
//#include "codec_com_codec.h"
#include "med_fft.h"

//#include "codec_op_etsi.h"
//#include "codec_op_lib.h"
//#include "codec_op_netsi.h"
//#include "codec_op_float.h"
//#include "codec_com_codec.h"
#include "med_pp_comm.h"
#include "med_aec_comm.h"

#ifndef __MED_AEC_HF_NLP_H__
#define __MED_AEC_HF_NLP_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/
/* ����ȫ�ֱ��� */
#define MED_AEC_HF_GetNlpHannWin()               (&g_ashwMedAecHfNlpHannWinNb[0])/* խ����NLPʱ������ */
#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_HF_NLP_FRM_LEN_NB                (128)                          /* խ��֡�� */
#define MED_AEC_HF_NLP_FFT_LEN_NB                (256)                          /* խ��FFT���� */
#define MED_AEC_HF_NLP_FRM_LEN_WB                (256)                          /* ���֡�� */
#else
#define MED_AEC_HF_NLP_FRM_LEN_NB                (120)                          /* խ��֡�� */
#define MED_AEC_HF_NLP_FFT_LEN_NB                (240)                          /* խ��FFT���� */
#define MED_AEC_HF_NLP_FRM_LEN_WB                (240)                          /* ���֡�� */
#endif
#define MED_AEC_HF_NLP_CN_ALPHA                  (8192)                         /* ���˹����׵��˲�ϵ��, Q15, 0.25 */
#define MED_AEC_HF_NLP_CN_ONE_SUB_ALPHA          (29491)                        /* ���˹����׵��˲�ϵ��, Q15, 0.9 */
#define MED_AEC_HF_NLP_CN_STEP                   (9830)                         /* �������ʹ��Ƶĵ�������, Q15, 0.3 */
#define MED_AEC_HF_NLP_CN_RAMP                   (33)                           /* �������ʹ��Ƶ�����ϵ��, Q15, 0.001 */
#define MED_AEC_HF_NLP_CN_NEARPSD_INIT           (1)                            /* ���˹����׳�ʼֵ */
#define MED_AEC_HF_NLP_CN_PSD_INIT_NB            (98)                           /* խ�������׳�ʼֵ */
#define MED_AEC_HF_NLP_CN_PSD_INIT_WB            (24)                           /* ��������׳�ʼֵ */

#define MED_AEC_HF_CANCEL_COUNT_THD              (25)                           /* ֡���������� */

/* SmoothPara */
#define MED_AEC_HF_NLP_SMOOTH_PARA_POW_THD       (1465)                         /* ƽ��������ֵ 150000000/FFT_Len^2*/
#define MED_AEC_HF_NLP_SMOOTH_PARA_FAST          (13107)                        /* �����˲�ϵ��  0.4 Q15*/
#define MED_AEC_HF_NLP_SMOOTH_PARA_SLOW          (29491)                        /* �����˲�ϵ�� 0.9 Q15*/
#define MED_AEC_HF_NLP_SMOOTH_PARA_SLOW_SUB      (3276)                         /* �����˲�ϵ����1�Ĳ� 0.1 Q15*/
#define MED_AEC_HF_NLP_SMOOTH_PARA_GAMMA_CNT_MIN (0)                            /* Gamma������Сֵ */
#define MED_AEC_HF_NLP_SMOOTH_PARA_GAMMA_CNT_MAX (5)                            /* Gamma�������ֵ */

#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_HF_ECHO_BIN_START                (10)                           /* խ������Ļ�������ʼλ�� */
#define MED_AEC_HF_ECHO_BIN_RANGE_NB             (50)                           /* խ����������Ļ����鳤�� */
#define MED_AEC_HF_ECHO_BIN_RANGE_WB             (98)                          /* �����������Ļ����鳤�� */
#else
#define MED_AEC_HF_ECHO_BIN_START                (9)                           /* խ������Ļ�������ʼλ�� */
#define MED_AEC_HF_ECHO_BIN_RANGE_NB             (47)                           /* խ����������Ļ����鳤�� */
#define MED_AEC_HF_ECHO_BIN_RANGE_WB             (91)                          /* �����������Ļ����鳤�� */
#endif
#define MED_AEC_HF_SORT_MIN                      (24575)                        /* SortQ����С���� Q15*/
#define MED_AEC_HF_SORT_HIGH                     (29490)                        /* SortQ�ĸ����� Q15*/
#define MED_AEC_HF_SORT_LOW                      (26214)                        /* SortQ�������� Q15*/

#define MED_AEC_HF_OVRD_MINCTR_MAX               (65534)
#define MED_AEC_HF_COHED_MEAN_HIGH               (32112)                        /* cohedMean�ĸ����� Q15*/
#define MED_AEC_HF_COHED_MEAN_LOW                (31129)                        /* cohedMean�ĵ����� Q15*/

#define MED_AEC_HF_PREAVG_LOW                    (19660)                        /* PrefAvgLow������ Q15*/
//#define MED_AEC_HF_OVRD_MIN                      (6141)                         /* ovrd����Сֵ Q11*/
#define MED_AEC_HF_OVRD_SUB                      (4096)                         /* ovrd�ļ�2 Q11*/
//#define MED_AEC_HF_OVRD_LOW                      (29490)                        /* ovrd�˲������� Q15*/
//#define MED_AEC_HF_OVRD_HIGH                     (31130)//(32439)                        /* ovrd�˲������� Q15*/
#define MED_AEC_HF_OVRD_MULT                     (209709)                       /* Mult��������ֵ Q15*/
#define MED_AEC_HF_OVRD_HALF_MULT                (MED_AEC_HF_OVRD_MULT>>1)      /* Mult��������ֵ Q15*/
#define MED_AEC_HF_OVRD_LOG_CONST                (705)                          /* �������� Q10*/
//#define MED_AEC_HF_OVRD_LOG_DIVED                (5120)                         /* �����ı��������� Q10*/

#define MED_AEC_HF_NLP_ERRPOW_FAC                (17203)                        /* 1.05->17203(Q14),�в��ź���������ϵ�� */
#define MED_AEC_HF_NLP_NEARPOW_FAC               (20429)                        /* 19.95->20429(Q10),�����ź���������ϵ�� */
#define MED_AEC_HF_NLP_DYNAMICSHIFT_TH           (536870912)                    /* 2^29=536870912,��̬��λ����1073741824 */

#define MED_AEC_HF_NLP_SMOOTHGAIN_GAMMA          (13107)                        /* 0.4->13107(Q15)*/

#define MED_AEC_HF_NLP_INSERTCN_Q30_MAX          (1073676289)                   /* (1073741824) */
#define MED_AEC_HF_NLP_INSERTCN_Q13_MAX          (8192)
#define MED_AEC_HF_NLP_INSERTCN_DYNAMICSHIFT_TH  (536870912)                    /* 2^29=536870912,��̬��λ����1073741824*/
#define MED_AEC_HF_NLP_INSERTCN_NORMALIZE_NB     (204)                          /* 1/161->204(Q15),������һ�� */
#define MED_AEC_HF_NLP_INSERTCN_NORMALIZE_WB     (102)                          /* 1/321->102(Q15),������һ�� */

#define MED_AEC_HF_SHW_MAX                       (32767)                        /* 16s�����ֵ Q15*/
#define MED_AEC_HF_SW_MAX                        (2147483647)                   /* 32s�����ֵ Q15*/

#ifdef SPEECH_STREAM_UNIT_128
#define MED_AEC_HF_NLP_RESDPOW_LOWBAND_8k        (13)                           /* խ���в��ź�����ͳ����ʼƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_HIGHBAND_8k       (34)                           /* խ���в��ź�����ͳ�ƽ�ֹƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_LOWBAND_16k       (24)                           /* ����в��ź�����ͳ����ʼƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_HIGHBAND_16k      (66)                           /* ����в��ź�����ͳ�ƽ�ֹƵ�� */
#define MED_AEC_HF_NLP_REFRPOW_THD               (16384)                        /* Զ�˲ο��ź��������� */
#else
#define MED_AEC_HF_NLP_RESDPOW_LOWBAND_8k        (12)                           /* խ���в��ź�����ͳ����ʼƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_HIGHBAND_8k       (31)                           /* խ���в��ź�����ͳ�ƽ�ֹƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_LOWBAND_16k       (22)                           /* ����в��ź�����ͳ����ʼƵ�� */
#define MED_AEC_HF_NLP_RESDPOW_HIGHBAND_16k      (61)                           /* ����в��ź�����ͳ�ƽ�ֹƵ�� */
#define MED_AEC_HF_NLP_REFRPOW_THD               (16384)                        /* Զ�˲ο��ź��������� */
#endif

#define MED_AEC_HF_NLP_COMP_LOW                  (6554)                         /* 0.2(Q15) */
#define MED_AEC_HF_NLP_VAD_FAR_HANGOUVER         (3)

#define MED_AEC_HF_SMOOTH_GAIN_TH                (1000)
#define MED_AEC_HF_SMOOTH_HIGH_FREQ_TH           (20)
#define MED_AEC_HF_SMOOTH_HI_GAIN_FREQ_TH        (25)
#define MED_AEC_HF_SMOOTH_HI_GAIN_FREQ_NUM       (40)
/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/

/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/
/* խ����NLPʱ������ */
extern VOS_INT16 g_ashwMedAecHfNlpHannWinNb[MED_AEC_HF_NLP_FRM_LEN_NB * 2];

/* �����NLPʱ������ */
extern VOS_INT16 g_ashwMedAecHfNlpHannWinWb[MED_AEC_HF_NLP_FRM_LEN_WB * 2];

extern VOS_INT16 g_ashwWeightNB[];
extern VOS_INT16 g_ashwWeightWB[];
extern VOS_INT16 g_ashwFreqNB[];
extern VOS_INT16 g_ashwFreqWB[];
extern VOS_INT16 g_ashwCoeff[];

/*****************************************************************************
  5 STRUCT����
*****************************************************************************/
/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_SMOOTH_GAIN_STRU
 ��������  : ����ʩ������ṹ��
*****************************************************************************/
typedef struct
{
    VOS_INT16                           shwRefCntThd;                           /* NLP: Զ���źż������� */
    VOS_INT16                           shwRefCnt;                              /* NLP: Զ���źż��� */
    VOS_INT16                           shwRefAmp1;                             /* NLP: Զ���ź���������1���������Ƶ�һ������ */
    VOS_INT16                           shwExOverdrive;                         /* NLP: ��ǿ��Overdrive */
    VOS_INT16                           shwOvrdod;                              /* �������ӵ���ָ��  Q10*/
    VOS_INT16                           shwReserve;                            
    VOS_INT16                          *pashwWeight;
    VOS_INT16                          *pashwFreq;
}MED_AEC_HF_NLP_SMOOTH_GAIN_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_TRANS_STRU
 ��������  : ����ʱƵ�任��Ƶʱ�任״̬�ṹ��
*****************************************************************************/
typedef struct
{
    VOS_INT16                          *pshwWin;                                /* ��������ָ�� */
    VOS_INT16                           shwFrmLen;                              /* ֡�� */
    VOS_INT16                           shwFftLen;                              /* NLP FFT/IFFT�ĳ��� */
    MED_FFT_NUM_ENUM_INT16              enFftNumIndex;                          /* FFT��������ö�� */
    MED_FFT_NUM_ENUM_INT16              enIfftNumIndex;                         /* IFFT��������ö�� */
    VOS_INT16                           ashwLastResdFrm[MED_AEC_HF_NLP_FRM_LEN_WB];/* ��һ֡�в�ʱ���ź� */
    VOS_INT16                           ashwNearFreq[MED_AEC_MAX_FFT_LEN];      /* ����Ƶ���ź� */
    VOS_INT16                           ashwResdFreq[MED_AEC_MAX_FFT_LEN];      /* �в�Ƶ���ź� */
    VOS_INT16                           ashwRefrFreq[MED_AEC_MAX_FFT_LEN];      /* Զ��Ƶ���ź� */
    VOS_INT16                           ashwResdFrm[MED_AEC_MAX_FFT_LEN];       /* ��һ֡�͵�ǰ֡�в�ʱ���ź� */
} MED_AEC_HF_NLP_TRANS_STRU;


typedef struct
{
    VOS_INT16                           ashwRefrWFreq[MED_AEC_HF_AF_M][MED_AEC_MAX_FFT_LEN];
} MED_AEC_HF_NLP_PART_DELAY_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_NLP_SMOOTH_PSD_STRU
 ��������  : ���������׺����ϵ���Ľṹ��
*****************************************************************************/
typedef struct
{
    VOS_INT32                           aswSee[1 + (MED_AEC_MAX_FFT_LEN / 2)];   /* �в��Թ����� */
    VOS_INT32                           aswSdd[1 + (MED_AEC_MAX_FFT_LEN / 2)];   /* �����ź��Թ����� */
	VOS_INT32                           aswSddOrg[1 + (MED_AEC_MAX_FFT_LEN / 2)];/* �˲�ǰ�����ź��Թ����� */
    VOS_INT32                           aswSxx[1 + (MED_AEC_MAX_FFT_LEN / 2)];   /* Զ���ź��Թ����� */
    VOS_INT32                           aswSxd[MED_AEC_MAX_FFT_LEN];             /* Զ���źźͽ����źŻ������� */
    VOS_INT32                           aswSed[MED_AEC_MAX_FFT_LEN];             /* �в�ͽ����źŻ������� */
    VOS_INT16                           ashwcohed[1 + (MED_AEC_MAX_FFT_LEN / 2)];/* �в�ͽ����ź����ϵ�� */
    VOS_INT16                           ashwcohxd[1 + (MED_AEC_MAX_FFT_LEN / 2)];/* Զ�˺ͽ����ź����ϵ�� */
    VOS_INT16                           ashwhnled[1 + (MED_AEC_MAX_FFT_LEN / 2)];/* ���������ϵ�� */
    VOS_INT16                           shwFFTLenth;                             /* FFT���� */
    VOS_INT16                           shwGammaCount;
    VOS_INT16                           shwGammaT;
    VOS_INT16                           shwMaxFarSmooth;
    VOS_INT16                           shwReserve;
}MED_AEC_HF_NLP_SMOOTH_PSD_STRU;


typedef struct
{
	VOS_BOOL                           shwRefVad;
	VOS_INT16                          swVadThr;
	VOS_INT16                          shwSpeCnt;
	VOS_INT16                          shwSilCnt;
	VOS_INT16                          shwSpe2Sil;
	VOS_INT16                          shwSil2Spe;
}MED_AEC_HF_NLP_REF_VAD_STRU;

typedef struct
{
	VOS_BOOL                           shwDtd;
	VOS_INT16                          shwBinStart;
	VOS_INT16                          shwEchoRange;
	VOS_INT16                          shwHangoverInMax;
	VOS_INT16                          shwHangoverOutMax;
	VOS_INT16                          shwHangoverOutMax2;
	VOS_INT16                          shwDtdCountMax;
	VOS_INT16                          shwHangoverIn;
	VOS_INT16                          shwHangoverOut;
	VOS_INT16                          shwDtdCount;
	VOS_FLOAT                          shwGammaUp;
	VOS_FLOAT                          shwGammaDown;
	VOS_FLOAT                          shwThrRatioMax;
	VOS_FLOAT                          shwThrAbs;
	VOS_FLOAT                          shwThrMin;
}MED_AEC_HF_NLP_DTD_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_BAND_SORT_STRU
 ��������  : ����MED_AEC_HFģ��BandSort�Ľṹ��
*****************************************************************************/
typedef struct
{

    VOS_INT16                           shwFFTLenth;
    VOS_INT16                           shwEchoLeng;                            /* ����Ļ����鳤�� */
    VOS_INT16                           shwEchoRange;                           /* ����Ļ����鳤�� */
    VOS_INT16                           shwCohedMean;                           /* �Ӵ��в�ͼӴ����˵����ϵ��adhnled��ָ��Ƶ�㷶Χ�ڵľ�ֵQ15*/
    VOS_INT16                           shwHnlSortQ;                            /* �Ӵ�Զ�˺ͼӴ����˵Ĳ����ϵ��(1-adcohxd)��ָ��Ƶ�㷶Χ�ڵľ�ֵ  Q15*/
    VOS_INT16                           shwHnlPrefAvg;                          /* ���������ϵ��adhnled�������������һ��ֶε��Ӧ��ֵ  Q15*/
    VOS_INT16                           shwHnlPrefAvgLow;                       /* ���������ϵ��adhnled������������ĵͷֶε��Ӧ��ֵ  Q15*/
    VOS_INT16                           shwHnlIdx;                              /* ���������ϵ��adhnled�������������һ��ֶε�Idx*/
    VOS_INT16                           shwHnlIdxLow;                           /* ���������ϵ��adhnled������������ĵͷֶε�Idx*/
    VOS_INT16                           shwBinStart;                            /* ����Ļ�����ĳ�ʼIdx */
} MED_AEC_HF_NLP_BAND_SORT_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_OVER_DRIVE_STRU
 ��������  : ����MED_AEC_HFģ��OverDrive�Ľṹ��
*****************************************************************************/
typedef struct
{
    VOS_INT16                           shwCohxdLocalMin;                       /* dhnlSortQ�ľֲ�����  Q15*/                           /*   */
    VOS_INT16                           shwSuppState;                           /* ���������жϣ�0��ʾ�����ƣ�1��ʾ����  Q15*/
    VOS_INT16                           shwHnlLocalMin;                         /* dhnlPrefAvgLow����  Q15*/
    VOS_INT16                           shwHnlNewMin;                           /* ���������ϵ��������������ƣ�����ֵ��0/1  Q15*/
    VOS_INT16                           shwMult;                                /* 10�ı���  Q0*/
    VOS_INT16                           shwDivergeState;                        /* ������ɢָʾ������ֵ��0/1��0��ʾ����ɢ��1��ʾ��ɢ�����в��������ڵͶ�����  Q0*/
    VOS_INT16                           shwOvrd;                                /* ��ƹ�����  Q11*/
    VOS_INT16                           shwOvrdSm;                              /* ʵ�ʹ�����  Q11*/
    VOS_INT32                           swHnlMinCtr;                            /* ���������ϵ��������������ƣ�����ֵ��0/1/2  Q*/
    VOS_INT32                           swHnlMin;                               /* ���������ϵ��������������ƣ�����ֵ��0/1  Q*/
    VOS_INT16                           shwTargetSupp;                          /* �����Ի��������� Q10 */
    VOS_INT16                           shwMinOvrd;                             /* ��С������ Q11 */
    VOS_INT16                           shwOvrdHigh;
    VOS_INT16                           shwOvrdLow;
}MED_AEC_HF_NLP_OVER_DRIVE_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_CN_STRU
 ��������  : ����������������״̬�ṹ��
*****************************************************************************/
typedef struct
{
    VOS_BOOL                            enCnFlag;                               /* ������������ʹ��ָʾ */
    VOS_INT16                           shwBinLen;                              /* Ƶ���� */
    VOS_INT32                           aswNearPsdBin[1 + (MED_AEC_MAX_FFT_LEN / 2)]; /* �������ܶ� */
    VOS_INT32                           aswPsdBin[1 + (MED_AEC_MAX_FFT_LEN / 2)];/* �������ܶ� */
    VOS_INT32                           swCancelCount;                          /* ֡������ */
    VOS_INT16                           shwNormalize;                           /* ����������һ��ϵ�� */
    VOS_INT16                           shwReserved;
} MED_AEC_HF_NLP_CN_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_STSUPPRE_STRU
 ��������  : ����������һ������״̬�ṹ��
*****************************************************************************/
typedef struct
{
    VOS_INT32                           swSumRefr;                              /* Զ�˲ο��ź����� */
    VOS_INT32                           swSumResd;                              /* NLP�в��ź����� */
    VOS_INT16                           shwResdPowAlph;                         /* NLP�����в��ź���������Alpha�˲�ϵ�� */
    VOS_INT16                           shwResdPowThd;                          /* NLP�����в��ź��������ޣ����ڴ����޽�һ������*/
    VOS_INT16                           shwResdPowBandLow;                      /* �в��ź�����ͳ����ʼƵ�� */
    VOS_INT16                           shwResdPowBandHi;                       /* �в��ź�����ͳ�ƽ�ֹƵ�� */
    VOS_BOOL                            swVadfr;                                /* Զ�������źŴ��ڱ�־ */
    VOS_INT16                           swVadCount;                             /* Զ�������źŴ���֡�� */
} MED_AEC_HF_NLP_STSUPPRE_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_CLIP_STRU
 ��������  : �����������޼�״̬�ṹ��
*****************************************************************************/
typedef struct
{
	VOS_FLOAT                          swGamma;
	VOS_FLOAT                          swSuppThr;
	VOS_FLOAT                          shwResdPow;
	VOS_FLOAT                          shwRefrPow;
	VOS_FLOAT                          swGainM;
} MED_AEC_HF_NLP_CLIP_STRU;

/*****************************************************************************
 ʵ������  : MED_AEC_HF_NLP_CN_STRU
 ��������  : ����NLP״̬�ṹ��
*****************************************************************************/
typedef struct
{
    MED_AEC_HF_NLP_TRANS_STRU           stTrans;                                /* NLPʱƵ�任��Ƶʱ�任״̬�ṹ�� */
#ifdef MED_AEC_DTD_ENABLE
    MED_AEC_HF_NLP_REF_VAD_STRU         stRefVad;
#endif
    MED_AEC_HF_NLP_PART_DELAY_STRU      stPartDelay;                            /* �ο��ź���ʱ�ṹ�� */
    MED_AEC_HF_NLP_SMOOTH_PSD_STRU      stSmoothPSD;                            /* ���������׺����ϵ���Ľṹ�� */
#ifdef MED_AEC_DTD_ENABLE
    MED_AEC_HF_NLP_DTD_STRU             stDtd;
#endif
    MED_AEC_HF_NLP_BAND_SORT_STRU       stBandSort;                             /* ����MED_AEC_HFģ��BandSort�Ľṹ�� */
    MED_AEC_HF_NLP_OVER_DRIVE_STRU      stOverdrive;                            /* ����MED_AEC_HFģ��OverDrive�Ľṹ�� */
    MED_AEC_HF_NLP_CN_STRU              stCn;                                   /* NLP������������״̬�ṹ�� */
    MED_AEC_HF_NLP_SMOOTH_GAIN_STRU     stSmoothGain;                           /* ��������ṹ�� */
    MED_AEC_HF_NLP_STSUPPRE_STRU        stSTSuppress;                           /* ������ǿ���ƽṹ�� */
	MED_AEC_HF_NLP_CLIP_STRU            stClip;                                 /* �������޼��ṹ�� */
} MED_AEC_HF_NLP_STRU;

/*****************************************************************************
  6 UNION����
*****************************************************************************/

/*****************************************************************************
  7 OTHERS����
*****************************************************************************/

/*****************************************************************************
  8 ��������
*****************************************************************************/
extern VOS_VOID MED_AEC_HF_NlpInit(
                MED_AEC_NV_STRU                  *pstAecParam,
                CODEC_SAMPLE_RATE_MODE_ENUM_INT32 enSampleRate,
                MED_AEC_HF_NLP_STRU              *pstHfNlp);
extern VOS_VOID MED_AEC_HF_NlpMain(
                MED_AEC_HF_NLP_STRU              *pstNlp,
                VOS_INT16                         shwDelayIdx, 
                VOS_INT32                        *pswWeight,
                VOS_INT16                        *pshwNearFrm,
                VOS_INT16                        *pshwRefrFrm,
                VOS_INT16                        *pshwResdFrm,
                VOS_INT16                        *pshwOut);
extern VOS_VOID MED_AEC_HF_NlpTime2Freq(
                MED_AEC_HF_NLP_TRANS_STRU        *pstTransObj,
                VOS_INT16                        *pshwNearFrm,
                VOS_INT16                        *pshwRefrFrm,
                VOS_INT16                        *pshwResdFrm);
extern VOS_VOID MED_AEC_HF_PartitionDelay(
                MED_AEC_HF_NLP_PART_DELAY_STRU *pstPartDelay,
                MED_INT16                       shwDelayIdx,
                MED_INT16                      *pshwRefrFreq,
                MED_INT16                       shwFftLen);
extern VOS_VOID MED_AEC_HF_NlpGamma(
                MED_AEC_HF_NLP_SMOOTH_PSD_STRU   *pstSmoothPSD,
                MED_INT32                        *paswSx);

extern VOS_VOID MED_AEC_HF_NlpSmoothPSD(
                MED_AEC_HF_NLP_SMOOTH_PSD_STRU   *pstSmoothPSD,
                MED_AEC_HF_NLP_TRANS_STRU        *pstTime2Freq, 
				VOS_INT16                         shwMaxFar);
extern VOS_VOID MED_AEC_HF_NlpBandSort(
                VOS_INT16                        *pshwCohed,
                VOS_INT16                        *pshwCohxd,
                VOS_INT16                        *pshwHnled,
                MED_AEC_HF_NLP_BAND_SORT_STRU    *pstBandSort);

extern VOS_VOID MED_AEC_HF_NlpSwap(
                VOS_INT16                        *pshwTempa,
                VOS_INT16                        *pshwTempb);

extern VOS_VOID MED_AEC_HF_NlpShortSort(
                VOS_INT16                        *pshwLow,
                VOS_INT16                        *pshwHigh);
extern VOS_VOID  MED_AEC_HF_NlpOverdrive(
                VOS_INT16                        *pshwCohxd,
                VOS_INT16                        *pshwCohed,
                MED_AEC_HF_NLP_BAND_SORT_STRU    *pstBandSort,
                MED_AEC_HF_NLP_OVER_DRIVE_STRU   *pstOverDrive,
                VOS_INT16                        *pshwHnled);
extern VOS_VOID MED_AEC_HF_NlpDivergeProc(
                VOS_INT32                        *pswErrPSD,
                VOS_INT32                        *pswNearPSD,
                VOS_INT16                        *pshwNearFreq,
                VOS_INT16                        *pshwErrFreq,
                VOS_INT16                        *shwDivergeState,
                VOS_INT32                        *pswWeight,
                VOS_INT16                         shwFftLen );
extern  VOS_VOID MED_AEC_HF_NlpSmoothGain(
                MED_AEC_HF_NLP_SMOOTH_GAIN_STRU  *pstSmoothGain,
                VOS_INT16                         shwPrefAvg,
                VOS_INT16                        *pashwAdhnled,
                VOS_INT16                         shwDovrdSm,
                VOS_INT16                         shwFftLen,
                VOS_INT16                         shwGamma,
                VOS_INT16                        *pshwErrFreq,
                VOS_INT16                         shwMaxFar,
                VOS_INT16                         shwMinOvrd);
extern  VOS_VOID MED_AEC_HF_NlpInsertCN(
                MED_AEC_HF_NLP_CN_STRU           *pstCnObj,
                VOS_INT16                         shwFftLen,
                VOS_INT16                        *pshwErrFreq,
                VOS_INT16                        *pashwAdhnled );
extern VOS_VOID MED_AEC_HF_NlpFreq2Time(
                MED_AEC_HF_NLP_TRANS_STRU        *pstTransObj,
                VOS_INT16                        *pshwResdFreq,
                VOS_INT16                        *pshwOut);
extern VOS_VOID MED_AEC_HF_NlpNoisePower(
                    MED_AEC_HF_NLP_CN_STRU       *pstCnObj,
                    VOS_INT32                    *pswSddOrg);

extern VOS_VOID MED_AEC_HF_STSuppress(
                MED_AEC_HF_NLP_STSUPPRE_STRU     *pstSTSuppress,
                VOS_INT16                         shwFftLen,
                VOS_INT16                        *pshwRefrFrm,
                VOS_INT16                        *pshwResdFreq);

extern VOS_VOID MED_AEC_HF_NlpClip(
	           MED_AEC_HF_NLP_CLIP_STRU          *pstClip,
	           VOS_INT16                         *pshwRefrFreq,
	           VOS_INT16                         *pshwResdFreq);

extern VOS_VOID MED_AEC_HF_NlpRefVad(
	           MED_AEC_HF_NLP_REF_VAD_STRU       *pstRefVad,
	           VOS_INT16                          shwFftLen,
	           VOS_INT16                         *pshwRefrFrm);

extern VOS_VOID MED_AEC_HF_NlpDtd(
               MED_AEC_HF_NLP_DTD_STRU           *pstDtd,
               VOS_BOOL                           shwVad,
               VOS_INT16                         *pshwcohed,
               VOS_INT32                         *pswSdd,
               VOS_INT32                         *pswSee);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of med_aec_hf_nlp.h*/
