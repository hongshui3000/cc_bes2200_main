
#ifndef _MED_PP_COMM_H_
#define _MED_PP_COMM_H_

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "codec_typedefine.h"
#include "codec_com_codec.h"
//#include "ucom_mem_dyn.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/
//#define MED_PP_GetFrameLength()                     (g_shwMedPpFrameLength)                         /* ��ȡ��ǰ����֡���� */
//#define MED_PP_SetFrameLength(shwPpFrameLen)        (g_shwMedPpFrameLength = (shwPpFrameLen))       /* ���õ�ǰ����֡���� */

//#define AUDIO_PP_GetFrameLength()                   (g_shwAudioPpFrameLength)                       /* ��ȡ��ǰ��Ƶ֡���� */
//#define AUDIO_PP_SetFrameLength(shwAudioPpFrameLen) (g_shwAudioPpFrameLength = (shwAudioPpFrameLen))  /* ���õ�ǰ��Ƶ֡���� */


/* NV���и�ģ�鳤�Ⱥ궨�� */
#define CODEC_NV_VOL_LEN                  ((VOS_INT16)2)                                     /* NV������������ */
#define CODEC_NV_PREEMP_LEN               ((VOS_INT16)7)                                     /* NV����Ԥ���س��� */
#define CODEC_NV_DEEMP_LEN                ((VOS_INT16)7)                                     /* NV����ȥ���س��� */
#define CODEC_NV_HPF_TX_LEN               ((VOS_INT16)17)                                    /* NV�������и�ͨ�˲����� */
#define CODEC_NV_HPF_RX_LEN               ((VOS_INT16)17)                                    /* NV�������и�ͨ�˲����� */
#define CODEC_NV_DEVGAIN_LEN              ((VOS_INT16)2)                                     /* NV�����豸���泤�� */
#define CODEC_NV_SIDEGAIN_LEN             ((VOS_INT16)1)                                     /* NV���в������泤�� */
#define CODEC_NV_COMP_TX_LEN              ((VOS_INT16)29)                                    /* NV�������в����˲����� */
#define CODEC_NV_COMP_RX_LEN              ((VOS_INT16)29)                                    /* NV�������в����˲����� */
#define CODEC_NV_AEC_LEN                  ((VOS_INT16)41)                                    /* NV����AEC���� */
#define CODEC_NV_EANR_1MIC_LEN            ((VOS_INT16)11)                                    /* NV����ANR_1MIC���� */
#define CODEC_NV_ANR_2MIC_LEN             ((VOS_INT16)21)                                    /* NV����ANR_2MIC���� */
#define CODEC_NV_AGC_TX_LEN               ((VOS_INT16)7)                                     /* NV��������AGC���� */
#define CODEC_NV_AGC_RX_LEN               ((VOS_INT16)7)                                     /* NV��������AGC���� */
#define CODEC_NV_MBDRC_LEN                ((VOS_INT16)151)                                   /* NV����MBDRC���� */
#define CODEC_NV_AIG_TX_LEN               ((VOS_INT16)41)                                    /* NV����AIG���� */
#define CODEC_NV_AIG_RX_LEN               ((VOS_INT16)41)                                    /* NV����AIG���� */
#define CODEC_NV_AVC_LEN                  ((VOS_INT16)11)                                    /* NV����AVC���� */
#define CODEC_NV_VAD_TX_LEN               ((VOS_INT16)4)                                     /* NV��������VAD���� */
#define CODEC_NV_VAD_RX_LEN               ((VOS_INT16)4)                                     /* NV��������VAD���� */
#define CODEC_NV_PP_RESERVE_LEN           ((VOS_INT16)2)                                    /* NV����Ԥ�����鳤�� */

#define AUDIO_NV_COMP_TX_LEN              ((VOS_INT16)150)                                    /* NV�������в����˲����� */
#define AUDIO_NV_COMP_RX_LEN              ((VOS_INT16)150)                                    /* NV�������в����˲����� */
#define AUDIO_NV_MBDRC_LEN                ((VOS_INT16)251)                                   /* NV����MBDRC���� */

#define MED_PP_SHIFT_BY_1               (1)                                     /* ���ƻ�������1λ */
#define MED_PP_SHIFT_BY_2               (2)                                     /* ���ƻ�������2λ */
#define MED_PP_SHIFT_BY_3               (3)                                     /* ���ƻ�������3λ */
#define MED_PP_SHIFT_BY_4               (4)                                     /* ���ƻ�������4λ */
#define MED_PP_SHIFT_BY_5               (5)                                     /* ���ƻ�������5λ */
#define MED_PP_SHIFT_BY_6               (6)                                     /* ���ƻ�������6λ */
#define MED_PP_SHIFT_BY_7               (7)                                     /* ���ƻ�������7λ */
#define MED_PP_SHIFT_BY_8               (8)                                     /* ���ƻ�������8λ */
#define MED_PP_SHIFT_BY_9               (9)                                     /* ���ƻ�������9λ */
#define MED_PP_SHIFT_BY_10              (10)                                    /* ���ƻ�������10λ */
#define MED_PP_SHIFT_BY_11              (11)                                    /* ���ƻ�������11λ */
#define MED_PP_SHIFT_BY_12              (12)                                    /* ���ƻ�������12λ */
#define MED_PP_SHIFT_BY_13              (13)                                    /* ���ƻ�������13λ */
#define MED_PP_SHIFT_BY_14              (14)                                    /* ���ƻ�������14λ */
#define MED_PP_SHIFT_BY_15              (15)                                    /* ���ƻ�������15λ */
#define MED_PP_SHIFT_BY_16              (16)                                    /* ���ƻ�������16λ */
#define MED_PP_SHIFT_BY_17              (17)                                    /* ���ƻ�������17λ */
#define MED_PP_SHIFT_BY_20              (20)                                    /* ���ƻ�������20λ */
#define MED_PP_SHIFT_BY_21              (21)                                    /* ���ƻ�������21λ */
#define MED_PP_SHIFT_BY_24              (24)                                    /* ���ƻ�������24λ */
#define MED_PP_SHIFT_BY_25              (25)                                    /* ���ƻ�������25λ */
#define MED_PP_SHIFT_BY_26              (26)                                    /* ���ƻ�������26λ */
#define MED_PP_SHIFT_BY_27              (27)                                    /* ���ƻ�������27λ */
#define MED_PP_SHIFT_BY_28              (28)                                    /* ���ƻ�������28λ */
#define MED_PP_SHIFT_BY_29              (29)                                    /* ���ƻ�������29λ */
#define MED_PP_SHIFT_BY_30              (30)                                    /* ���ƻ�������30λ */
#define MED_PP_SHIFT_BY_31              (31)                                    /* ���ƻ�������31λ */

#define MED_PP_MIN_PSD_VALUE            (1)                                     /* ��������Сֵ�޶� */
#define NONZERO_FRM_NUM_INSERTED        (11)                                    /* ��Ϊ��������������ȫ��֡�� */

#define MED_SAMPLE_8K                   (8000)
#define MED_FRM_LEN_8K                  (160)
#define MED_FFT_LEN_8K                  (256)

#define MED_SAMPLE_16K                  (16000)
#define MED_FRM_LEN_16K                 (320)
#define MED_FFT_LEN_16K                 (512)

#define MED_SAMPLE_48K                  (48000)
#define MED_FRM_LEN_48K                 (960)
#define MED_FFT_LEN_48K                 (1024)


/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/

/* ��˷����ö�� */
enum MED_PP_MIC_NUM_ENUM
{
    MED_PP_MIC_NUM_1                       = 1,
    MED_PP_MIC_NUM_2,
    MED_PP_MIC_NUM_BUTT
};
typedef VOS_UINT16 MED_PP_MIC_NUM_ENUM_UINT16;

/* ����ö�� */
enum AUDIO_PP_CHANNEL_ENUM
{
    AUDIO_PP_CHANNEL_LEFT                  = 0,
    AUDIO_PP_CHANNEL_RIGHT,
    AUDIO_PP_CHANNEL_ALL,
    AUDIO_PP_CHANNEL_BUTT
};
typedef VOS_UINT16 AUDIO_PP_CHANNEL_ENUM_UINT16;

/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/
extern VOS_INT16 g_shwMedPpFrameLength;
extern VOS_INT16 g_shwAudioPpFrameLength;
extern const VOS_INT16 g_ashwMedPpBandTable1[40][2];
/*****************************************************************************
  5 STRUCT����
*****************************************************************************/
#define MED_OBJ_HEADER                                          \
                CODEC_OBJ_USED_STATUS_ENUM_UINT16 enIsUsed;       \
                VOS_INT16                       shwReserved;

typedef struct MED_OBJ_HEADER_tag
{
    MED_OBJ_HEADER
}MED_OBJ_HEADER_STRU;

/*****************************************************************************
ʵ������  : MED_OBJ_INFO
��������  : �ṹ�������Ϣ
*****************************************************************************/
typedef struct
{
    VOS_VOID               *pvObjArray;                                         /* ���������ͷָ�� */
    VOS_INT32               swObjNum;                                           /* ��������ĳ���   */
    VOS_INT32               swObjLen;                                           /* ����Ĵ�С       */
}MED_OBJ_INFO;

/*****************************************************************************
  6 UNION����
*****************************************************************************/

/*****************************************************************************
  7 OTHERS����
*****************************************************************************/

/*****************************************************************************
  8 ��������
*****************************************************************************/
extern VOS_UINT32 MED_PP_CheckPtrValid(
                       MED_OBJ_INFO           *pstInfo,
                       VOS_VOID               *pvTargetObj);
extern VOS_VOID* MED_PP_Create(
                       MED_OBJ_INFO           *pstInfo,
                       VOS_VOID               *pvObjArray,
                       VOS_INT32               swObjNum,
                       VOS_INT32               swObjLen);
extern VOS_VOID MED_PP_CalcPsdBin(
                       VOS_INT16              *pshwFreqBin,
                       VOS_INT16               shwFreqBinLen,
                       VOS_INT32              *pswPsdBin);
extern VOS_VOID MED_PP_CalcBandPsd(
                        VOS_INT32             *pswPsdBin,
                        VOS_INT16              shwBandLen,
                        VOS_INT32             *pswPsdBand);
extern VOS_VOID MED_PP_CalcSmoothPsd(
                        VOS_INT32             *pswPsdLast,
                        VOS_INT16              shwLen,
                        VOS_INT16              shwAlpha,
                        VOS_INT32             *pswPsd);



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of med_aec_main.h*/

