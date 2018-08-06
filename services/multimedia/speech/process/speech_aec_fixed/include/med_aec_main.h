

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "v_typdef.h"

#ifndef _MED_AEC_MAIN_H_
#define _MED_AEC_MAIN_H_


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/

/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/

/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/

/*****************************************************************************
  5 STRUCT����
*****************************************************************************/
typedef VOS_VOID (*NS_HANDLER_T)(VOS_VOID *stNs, VOS_INT16 *shwBuf, VOS_INT32 swFrameLength);

/* AEC������ʼ���ӿ� */
typedef struct
{
    VOS_BOOL                            enEAecEnable;                           /* AEC: ��ǿ����AECʹ�ܿ��� */
    VOS_BOOL                            enHpfEnable;                            /* HPF: ��ͨ�˲����� */
    VOS_BOOL                            enAfEnable;                             /* AF : ����Ӧ�˲����� */
    VOS_BOOL                            enNsEnable;                             /* NS : �������ƿ��� */
    VOS_BOOL                            enNlpEnable;                            /* NLP: ���������ƿ��� */
    VOS_BOOL                            enCngEnable;                            /* CNG: ������������ */
    VOS_INT16                           shwDelayLength;                         /* DELAY: �̶���ʱ */
    VOS_INT16                           shwNlpRefCnt;                           /* NLP: Զ���źż������������Ƶ�һ������ */
    VOS_INT16                           shwNlpRefAmp1;                          /* NLP: Զ���ź���������1���������Ƶ�һ������ */
    VOS_INT16                           shwNlpExOverdrive;                      /* NLP: ��ǿ��Overdrive���������Ƶ�һ������ */
    VOS_INT16                           shwReserve4;                            /* ����4 */
    VOS_INT16                           shwNlpResdPowAlph;                      /* NLP: �в��ź������˲�ϵ�� */
    VOS_INT16                           shwNlpResdPowThd;                       /* NLP: �в��ź��������� */
    VOS_INT16                           shwNlpSmoothGainDod;                    /* NLP: �������ӵ���ָ��"���С�ڵ���0�������ʵ�ʵ���ָ����������ָ������ָ��" */
    VOS_INT16                           shwNlpBandSortIdx;                      /* NLP: խ������ĸ� λ��MED_AEC_HF_ECHO_BIN_RANGE*(3/4)or(1/2)-1 */
    VOS_INT16                           shwNlpBandSortIdxLow;                   /* NLP: խ������ĵ� λ��MED_AEC_HF_ECHO_BIN_RANGE*(1/2)or(3/10)-1*/
    VOS_INT16                           shwNlpTargetSupp;                       /* NLP: Ŀ�������� */
    VOS_INT16                           shwNlpMinOvrd;                          /* NLP: ��С������ */
    VOS_INT16                           shwNlpOvrdHigh;                         /* NLP: �����½�ʱ��ƽ�����ӣ�ֵԽС���½�Խ�죬һ�� 0.9 ~ 1*/
    VOS_INT16                           shwNlpOvrdLow;                          /* NLP: ��������ʱ��ƽ�����ӣ�ֵԽС������Խ�죬һ�� 0.9 ~ 1������ shwNlpOvrdLow < shwNlpOvrdHigh */
} MED_AEC_NV_STRU;

/*****************************************************************************
  6 UNION����
*****************************************************************************/

/*****************************************************************************
  7 OTHERS����
*****************************************************************************/

/*****************************************************************************
  8 ��������
*****************************************************************************/
extern VOS_UINT32  MED_AEC_Main(
	                   VOS_VOID               *pAecInstance,
                       VOS_INT16              *pshwMicIn,
					   VOS_INT16              *pshwSpkIn,
                       VOS_INT16              *pshwLineOut);
extern VOS_VOID* MED_AEC_Create(void);
extern VOS_UINT32  MED_AEC_Destroy(VOS_VOID  **ppAecObj);
extern VOS_UINT32 MED_AEC_SetPara (
                       VOS_VOID                        *pAecInstance,
					   MED_AEC_NV_STRU                 *pstNv,
                       VOS_INT32                       enSampleRate);
extern VOS_UINT32  MED_AEC_SetExtenalNsHandle(
                       VOS_VOID               *pAecInstance,
                       VOS_VOID               *pstNs,
                       NS_HANDLER_T           swNsHandler);
extern VOS_UINT32  MED_AEC_GetNlpGain(
                       VOS_VOID               *pAecInstance,
                       VOS_INT16              *pshwNlpGain);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* end of med_aec_main.h*/

