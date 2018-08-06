

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "codec_typedefine.h"
#include "codec_op_cpx.h"

#ifndef __MED_FFT_H__
#define __MED_FFT_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 �궨��
*****************************************************************************/
#define MED_FFT_OBJ_NUM                  (2)                                    /*FFTʵ������ */
#define MED_IFFT_OBJ_NUM                 (2)                                    /*IFFTʵ������ */

#define MED_FFT_320
#define MED_FFT_640

#define MED_FFT_Q13                      (13)                                   /* Q13 */
#define MED_FFT_Q14                      (14)                                   /* Q14 */
#define MED_FFT_Q15                      (15)                                   /* Q15 */

#ifdef MED_FFT_1024
#define MED_FFT_1024_CMPX_LEN            (512)                                  /* 1024��FFT�����г��� */
#define MED_FFT_1024_REAL_LEN            (MED_FFT_1024_CMPX_LEN*2)              /* 1024��FFTʵ���г��� */
#endif

#ifdef MED_FFT_640
#define MED_FFT_640_CMPX_LEN             (320)                                  /* 640��FFT�����г��� */
#define MED_FFT_640_REAL_LEN             (MED_FFT_640_CMPX_LEN*2)               /* 640��FFTʵ���г��� */
#endif

#ifdef MED_FFT_512
#define MED_FFT_512_CMPX_LEN             (256)                                  /* 512��FFT�����г��� */
#define MED_FFT_512_REAL_LEN             (MED_FFT_512_CMPX_LEN*2)               /* 512��FFTʵ���г��� */
#endif

#ifdef MED_FFT_320
#define MED_FFT_320_CMPX_LEN             (160)                                  /* 320��FFT�����г��� */
#define MED_FFT_320_REAL_LEN             (MED_FFT_320_CMPX_LEN*2)               /* 320��FFTʵ���г��� */
#endif

#ifdef MED_FFT_256
#define MED_FFT_256_CMPX_LEN             (128)                                  /* 256��FFT�����г��� */
#define MED_FFT_256_REAL_LEN             (MED_FFT_256_CMPX_LEN*2)               /* 256��FFTʵ���г��� */
#endif

#ifdef MED_FFT_128
#define MED_FFT_128_CMPX_LEN             (64)                                   /* 128��FFT�����г��� */
#define MED_FFT_128_REAL_LEN             (MED_FFT_128_CMPX_LEN*2)               /* 128��FFTʵ���г��� */
#endif

#if defined(MED_FFT_1024)
#define MED_FFT_BUF_LEN                  (MED_FFT_1024_REAL_LEN)                /* FFT�����ڲ�����������*/
#elif defined(MED_FFT_640)
#define MED_FFT_BUF_LEN                  (MED_FFT_640_REAL_LEN)                /* FFT�����ڲ�����������*/
#elif defined(MED_FFT_512)
#define MED_FFT_BUF_LEN                  (MED_FFT_512_REAL_LEN)                /* FFT�����ڲ�����������*/
#elif defined(MED_FFT_320)
#define MED_FFT_BUF_LEN                  (MED_FFT_320_REAL_LEN)                /* FFT�����ڲ�����������*/
#elif defined(MED_FFT_256)
#define MED_FFT_BUF_LEN                  (MED_FFT_256_REAL_LEN)                /* FFT�����ڲ�����������*/
#elif defined(MED_FFT_128)
#define MED_FFT_BUF_LEN                  (MED_FFT_128_REAL_LEN)                /* FFT�����ڲ�����������*/
#else
error("FFT length not supported");
#endif


#define MED_FFT_MAX_FACTORS              (5)                                    /* ��������, 4 */

#ifdef MED_FFT_1024
#define MED_FFT_512_FACTOR_NUM           (5)                                    /* 512�������,4*4*4*4*2=512 */
#endif

#ifdef MED_FFT_640
#define MED_FFT_320_FACTOR_NUM           (4)                                    /* 320�������,5*4*4*4=320 */
#endif

#ifdef MED_FFT_512
#define MED_FFT_256_FACTOR_NUM           (4)                                    /* 256�������,4*4*4*4=256 */
#endif

#ifdef MED_FFT_320
#define MED_FFT_160_FACTOR_NUM           (4)                                    /* 160�������,5*4*4*2=160 */
#endif

#ifdef MED_FFT_256
#define MED_FFT_128_FACTOR_NUM           (4)                                    /* 128�������,4*4*4*2=128 */
#endif

#ifdef MED_FFT_128
#define MED_FFT_64_FACTOR_NUM            (3)                                    /* 64�������,4*4*4=64 */
#endif

#define MED_FFT_RADIX_TWO                (2)                                    /* ��2 */
#define MED_FFT_RADIX_FOUR               (4)                                    /* ��4 */
#define MED_FFT_RADIX_FIVE               (5)                                    /* ��5 */
#define MED_FFT_BFLY5_DIV                (5)                                    /* ��5��������ʱ�õ��ĳ��� */
#define MED_FFT_FIX_DIV5_PARAM           (6553)                                 /* �Ի�5��������FixDiv������������� */

#define MED_FFT_COS_L1                   (32767)
#define MED_FFT_COS_L2                   (-7651)
#define MED_FFT_COS_L3                   (8277)
#define MED_FFT_COS_L4                   (-626)

#define MED_FFT_COSNORM_MASK1            ((VOS_INT32)0x0001ffff)
#define MED_FFT_COSNORM_MASK2            ((VOS_INT32)0x00007fff)
#define MED_FFT_COSNORM_MASK3            ((VOS_INT32)0x0000ffff)
#define MED_FFT_COSNORM_THD1             ((VOS_INT32)0x10000)
#define MED_FFT_COSNORM_THD2             ((VOS_INT32)0x8000)
#define MED_FFT_COSNORM_CONST1           ((VOS_INT32)0x20000)
#define MED_FFT_COSNORM_CONST2           ((VOS_INT32)65536)

#define ROUND_ASYM_ENABLE                                                       /* �ǶԳƱ��Ͳ������������������Ǹ����������������������롣 ����߾��� */

/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/
enum MED_FFT_NUM_ENUM
{
#ifdef MED_FFT_128
     MED_FFT_NUM_128 = 0,
#endif
#ifdef MED_FFT_256
     MED_FFT_NUM_256,
#endif
#ifdef MED_FFT_320
     MED_FFT_NUM_320,
#endif
#ifdef MED_FFT_512
     MED_FFT_NUM_512,
#endif
#ifdef MED_FFT_640
     MED_FFT_NUM_640,
#endif
#ifdef MED_FFT_1024
     MED_FFT_NUM_1024,
#endif
#ifdef MED_FFT_128
     MED_IFFT_NUM_128,
#endif
#ifdef MED_FFT_256
     MED_IFFT_NUM_256,
#endif
#ifdef MED_FFT_320
     MED_IFFT_NUM_320,
#endif
#ifdef MED_FFT_512
     MED_IFFT_NUM_512,
#endif
#ifdef MED_FFT_640
     MED_IFFT_NUM_640,
#endif
#ifdef MED_FFT_1024
     MED_IFFT_NUM_1024,
#endif
     MED_FFT_NUM_BUTT
};
typedef VOS_INT16 MED_FFT_NUM_ENUM_INT16;

/*****************************************************************************
 ʵ������  : MED_FFT_CORE_STRU
 ��������  : ָ������FFT����
*****************************************************************************/
typedef struct
{
    VOS_INT16                           shwIsInverted;                          /* ��任��־ */
    VOS_INT16                           shwN;                                   /* ����FFT���� */
    CODEC_OP_CMPX_STRU                   *pstTwiddles;                            /* ����������ת���� */
    CODEC_OP_CMPX_STRU                   *pstSuperTwiddles;                       /* ʵ��������ת���� */
    VOS_INT16                          *pshwFactors;                            /* �����Ͳ���ָ�� */
    VOS_INT16                          *pshwSortTable;                          /* λ������ָ�� */
    VOS_INT16                           shwNumOfRadix;                          /* �������� */
    VOS_INT16                           shwResvered;
}MED_FFT_CORE_STRU;

/*****************************************************************************
 ʵ������  : MED_FFT_OBJ_STRU
 ��������  : FFTʵ���任ʵ��ṹ��
*****************************************************************************/
typedef struct
{
#ifdef MED_FFT_128
    MED_FFT_CORE_STRU                   stFft128;                               /* 128��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_256
    MED_FFT_CORE_STRU                   stFft256;                               /* 256��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_320
    MED_FFT_CORE_STRU                   stFft320;                               /* 320��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_512
    MED_FFT_CORE_STRU                   stFft512;                               /* 512��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_640
    MED_FFT_CORE_STRU                   stFft640;                               /* 640��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_1024
    MED_FFT_CORE_STRU                   stFft1024;                              /* 1024��ʵ��FFT�任ʵ��  */
#endif
#ifdef MED_FFT_128
    MED_FFT_CORE_STRU                   stIfft128;                              /* 128��ʵ��IFFT�任ʵ�� */
#endif
#ifdef MED_FFT_256
    MED_FFT_CORE_STRU                   stIfft256;                              /* 256��ʵ��IFFT�任ʵ�� */
#endif
#ifdef MED_FFT_320
    MED_FFT_CORE_STRU                   stIfft320;                              /* 320��ʵ��IFFT�任ʵ�� */
#endif
#ifdef MED_FFT_512
    MED_FFT_CORE_STRU                   stIfft512;                              /* 512��ʵ��IFFT�任ʵ�� */
#endif
#ifdef MED_FFT_640
    MED_FFT_CORE_STRU                   stIfft640;                              /* 640��ʵ��IFFT�任ʵ�� */
#endif
#ifdef MED_FFT_1024
    MED_FFT_CORE_STRU                   stIfft1024;                             /* 1024��ʵ��IFFT�任ʵ�� */
#endif
} MED_FFT_OBJ_STRU;

/*****************************************************************************
  4 ��Ϣͷ����
*****************************************************************************/


/*****************************************************************************
  5 ��Ϣ����
*****************************************************************************/


/*****************************************************************************
  6 STRUCT����
*****************************************************************************/



/*****************************************************************************
  7 UNION����
*****************************************************************************/


/*****************************************************************************
  8 OTHERS����
*****************************************************************************/


/*****************************************************************************
  9 ȫ�ֱ�������
*****************************************************************************/

/* �����Ż�������� */
#ifndef _MED_C89_

extern const VOS_INT16                  g_shwDigperm[];                         /* FFT�����Ĳ��ұ� */

extern const VOS_UINT16                 g_auhwMedFft1024Twiddles[];             /* 1024��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft1024Twiddles[];            /* 1024��IFFT����������ת���� */

extern const VOS_INT32                  g_aswMedFft1024Twiddles[];              /* 1024��FFT����������ת���� */

extern const VOS_INT32                  g_aswMedFft640TwiddlesDit[];               /* 640��FFT����������ת���� */

extern const VOS_INT32                  g_aswMedFft512Twiddles[];               /* 512��FFT����������ת���� */
extern const VOS_INT32                  g_aswMedFft320TwiddlesDit[];               /* 320��FFT����������ת���� */

extern const VOS_INT32                  g_aswMedFft256Twiddles[];               /* 256��FFT����������ת���� */
#else

extern const VOS_UINT16                 g_auhwMedFft1024Twiddles[];             /* 1024��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft1024Twiddles[];            /* 1024��IFFT����������ת���� */

extern const VOS_UINT16                 g_auhwMedFft640Twiddles[];              /* 640��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft640Twiddles[];             /* 640��IFFT����������ת���� */

extern const VOS_UINT16                 g_auhwMedFft512Twiddles[];              /* 512��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft512Twiddles[];             /* 512��IFFT����������ת���� */

extern const VOS_UINT16                 g_auhwMedFft320Twiddles[];              /* 320��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft320Twiddles[];             /* 320��IFFT����������ת���� */

extern const VOS_UINT16                 g_auhwMedFft256Twiddles[];              /* 256��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft256Twiddles[];             /* 256��IFFT����������ת���� */

#endif

/* 1024��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft1024SuperTwiddles[];        /* 1024��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft1024SuperTwiddles[];       /* 1024��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft512RadixAndStride[];        /* 512��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft512SortTable[];             /* 512��λ��ת���� */

/* 640��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft640SuperTwiddles[];         /* 640��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft640SuperTwiddles[];        /* 640��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft320RadixAndStride[];        /* 320��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft320SortTable[];             /* 320��λ��ת���� */

/* 512��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft512SuperTwiddles[];         /* 512��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft512SuperTwiddles[];        /* 512��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft256RadixAndStride[];        /* 256��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft256SortTable[];             /* 256��λ��ת���� */

/* 320��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft320SuperTwiddles[];         /* 320��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft320SuperTwiddles[];        /* 320��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft160RadixAndStride[];        /* 160��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft160SortTable[];             /* 160��λ��ת���� */

/* 256��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft256SuperTwiddles[];         /* 256��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft256SuperTwiddles[];        /* 256��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft128RadixAndStride[];        /* 128��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft128SortTable[];             /* 128��λ��ת���� */

/* 128��FFT��� */
extern const VOS_UINT16                 g_auhwMedFft128Twiddles[];              /* 128��FFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedFft128SuperTwiddles[];         /* 128��FFTʵ��������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft128Twiddles[];             /* 128��IFFT����������ת���� */
extern const VOS_UINT16                 g_auhwMedIfft128SuperTwiddles[];        /* 128��IFFTʵ��������ת���� */
extern const VOS_INT16                  g_ashwMedFft64RadixAndStride[];         /* 64��FFT�����Ͳ��� */
extern const VOS_INT16                  g_ashwMedFft64SortTable[];              /* 64��λ��ת���� */

/*****************************************************************************
  10 ��������
*****************************************************************************/
#ifndef _MED_C89_
extern VOS_VOID MED_FFT_cfft_dit_160_320(
                MED_FFT_CORE_STRU       *pstObj,
                VOS_INT16               *x,
                VOS_INT16               *y);

extern VOS_VOID MED_FFT_cifft_dit_160_320(
                MED_FFT_CORE_STRU       *pstObj,
                VOS_INT16               *x,
                VOS_INT16               *y);

extern VOS_VOID MED_FFT_cfft_dif_128_256(
                MED_FFT_CORE_STRU       *pstObj,
                VOS_INT16               *x,
                VOS_INT16               *y);

extern VOS_VOID MED_FFT_cifft_dif_128_256(
                MED_FFT_CORE_STRU       *pstObj,
                VOS_INT16               *x,
                VOS_INT16               *y);
#endif

extern VOS_VOID MED_FFT_Bfly2(
                       MED_FFT_CORE_STRU      *pstObj,
                       CODEC_OP_CMPX_STRU     *pstSn,
                       VOS_INT16               shwStride,
                       VOS_INT16               shwStageNum,
                       VOS_INT16               shwStageLen,
                       VOS_INT16               shwNextStageNum);
extern VOS_VOID MED_FFT_Bfly4(
                       MED_FFT_CORE_STRU      *pstObj,
                       CODEC_OP_CMPX_STRU     *pstSn,
                       VOS_INT16               shwStride,
                       VOS_INT16               shwStageNum,
                       VOS_INT16               shwStageLen,
                       VOS_INT16               shwNextStageNum);
extern VOS_VOID MED_FFT_Bfly5(
                       MED_FFT_CORE_STRU      *pstObj,
                       CODEC_OP_CMPX_STRU     *pstSn,
                       VOS_INT16               shwStride,
                       VOS_INT16               shwStageNum);
extern VOS_VOID MED_FFT_Cmpx(
                       MED_FFT_CORE_STRU        *pstObj,
                       CODEC_OP_CMPX_STRU       *pstIn,
                       CODEC_OP_CMPX_STRU       *pstOut);
extern VOS_VOID MED_FFT_Fft(VOS_INT16 shwFftLenIndex, VOS_INT16 *pshwIn, VOS_INT16 *pshwOut);
extern VOS_VOID MED_FFT_FftCore(MED_FFT_CORE_STRU *pstObj,VOS_INT16 *pshwIn, VOS_INT16 *pshwOut);
extern VOS_VOID MED_FFT_Ifft(VOS_INT16 shwIfftLenIndex, VOS_INT16 *pshwIn, VOS_INT16 *pshwOut);
extern VOS_VOID MED_FFT_IfftCore(MED_FFT_CORE_STRU *pstObj,  VOS_INT16 *pshwIn, VOS_INT16 *pshwOut);
extern VOS_VOID MED_FFT_InitAllObjs(void);
extern VOS_VOID MED_FFT_SplitFreq(
                       MED_FFT_CORE_STRU        *pstObj,
                       CODEC_OP_CMPX_STRU       *pstIn,
                       CODEC_OP_CMPX_STRU       *pstOut);
extern VOS_VOID MED_FFT_UniteFreq(
                       MED_FFT_CORE_STRU        *pstObj,
                       CODEC_OP_CMPX_STRU       *pstIn,
                       CODEC_OP_CMPX_STRU       *pstOut);




#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of med_fft.h */


