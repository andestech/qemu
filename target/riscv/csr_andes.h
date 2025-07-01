#ifndef RISCV_CSR_ANDES_INC_H
#define RISCV_CSR_ANDES_INC_H

#include "andes_cpu_bits.h"

#define ANDES_CSR_TABLE_SIZE 0x1000
#define ANDES_CSRIND_TABLE_SIZE 0x100

typedef struct andes_csr {
    target_ulong csrno[ANDES_CSR_TABLE_SIZE];
    target_ulong csrind[ANDES_CSRIND_TABLE_SIZE];
} AndesCsr;

typedef struct AndesVec {
    int vectored_irq_m;
    int vectored_irq_s;
} AndesVec;

typedef void (*fp_spec_csr_init_fn)(AndesCsr *);

/* mmsc_cfg */
#define V5_MMSC_CFG_ECC                     0
#define V5_MMSC_CFG_TLB_ECC                 1
#define V5_MMSC_CFG_TLB_ECC2                2
#define V5_MMSC_CFG_ECD                     3
#define V5_MMSC_CFG_PFT                     4
#define V5_MMSC_CFG_HSP                     5
#define V5_MMSC_CFG_ACE                     6
#define V5_MMSC_CFG_ADDPMC                  7
#define V5_MMSC_CFG_ADDPMC_4                (0x0 << V5_MMSC_CFG_ADDPMC)
#define V5_MMSC_CFG_ADDPMC_3                (0x1 << V5_MMSC_CFG_ADDPMC)
#define V5_MMSC_CFG_ADDPMC_2                (0x2 << V5_MMSC_CFG_ADDPMC)
#define V5_MMSC_CFG_ADDPMC_1                (0x3 << V5_MMSC_CFG_ADDPMC)
#define V5_MMSC_CFG_ADDPMC_0                (0x4 << V5_MMSC_CFG_ADDPMC)
#define V5_MMSC_CFG_VPLIC                   12
#define V5_MMSC_CFG_EV5PE                   13
#define V5_MMSC_CFG_LMSLVP                  14
#define V5_MMSC_CFG_PMNDS                   15
#define V5_MMSC_CFG_CCTLCSR                 16
#define V5_MMSC_CFG_EFHW                    17
#define V5_MMSC_CFG_VCCTL                   18
#define V5_MMSC_CFG_VCCTL2                  19
#define V5_MMSC_CFG_EXCSLVL                 20
#define V5_MMSC_CFG_NOPMC                   22
#define V5_MMSC_CFG_SPE_AFT                 23
#define V5_MMSC_CFG_ESLEEP                  24
#define V5_MMSC_CFG_PPI                     25
#define V5_MMSC_CFG_FIO                     26
#define V5_MMSC_CFG_CLIC                    27
#define V5_MMSC_CFG_ECLIC                   28
#define V5_MMSC_CFG_EDSP                    29
#define V5_MMSC_CFG_PPMA                    30
#define V5_MMSC_CFG_MSC_EXT                 31

#define V5_MMSC_CFG_BF16CVT                 32
#define V5_MMSC_CFG_ZFH                     33
#define V5_MMSC_CFG_VL4                     34
#define V5_MMSC_CFG_CRASHSAVE               35
#define V5_MMSC_CFG_VECCFG                  36
#define V5_MMSC_CFG_FINV                    37
#define V5_MMSC_CFG_PP16                    38
#define V5_MMSC_CFG_VSIH                    40
#define V5_MMSC_CFG_ECDV                    41
#define V5_MMSC_CFG_VDOT                    43
#define V5_MMSC_CFG_VPFH                    44
#define V5_MMSC_CFG_L2CMP_CFG               45
#define V5_MMSC_CFG_L2C                     46
#define V5_MMSC_CFG_IOCP                    47
#define V5_MMSC_CFG_CORE_PCLUS              48
#define V5_MMSC_CFG_RVARCH                  52
#define V5_MMSC_CFG_TLB_RAM_CMD             53
#define V5_MMSC_CFG_CCTL_FL_UL              54
#define V5_MMSC_CFG_HSPO                    55
#define V5_MMSC_CFG_ALT_FP_FMT              57
#define V5_MMSC_CFG_D_LMSLVP                58
#define V5_MMSC_CFG_I_LMSLVP                59
#define V5_MMSC_CFG_BTB_ECC                 61
#define V5_MMSC_CFG_MSC_EXT3                63

#define V5_MMSC_CFG2_BF16CVT                0
#define V5_MMSC_CFG2_ZFH                    1
#define V5_MMSC_CFG2_VL4                    2
#define V5_MMSC_CFG2_CRASHSAVE              3
#define V5_MMSC_CFG2_VECCFG                 4
#define V5_MMSC_CFG2_FINV                   5
#define V5_MMSC_CFG2_PP16                   6
#define V5_MMSC_CFG2_VSIH                   8
#define V5_MMSC_CFG2_ECDV                   9
#define V5_MMSC_CFG2_VDOT                   11
#define V5_MMSC_CFG2_VPFH                   12
#define V5_MMSC_CFG2_L2CMP_CFG              13
#define V5_MMSC_CFG2_L2C                    14
#define V5_MMSC_CFG2_IOCP                   15
#define V5_MMSC_CFG2_CORE_PCLUS             16
#define V5_MMSC_CFG2_RVARCH                 20
#define V5_MMSC_CFG2_TLB_RAM_CMD            21
#define V5_MMSC_CFG2_CCTL_FL_UL             22
#define V5_MMSC_CFG2_HSPO                   23
#define V5_MMSC_CFG2_ALT_FP_FMT             25
#define V5_MMSC_CFG2_D_LMSLVP               26
#define V5_MMSC_CFG2_I_LMSLVP               27
#define V5_MMSC_CFG2_RVARCH2                28
#define V5_MMSC_CFG2_BTB_ECC                29
#define V5_MMSC_CFG2_MSC_EXT3               31

#define V5_MMSC_CFG3_BTB_RAM_CMD            0
#define V5_MMSC_CFG3_NO_CCTL_VA             1
#define V5_MMSC_CFG3_NO_CCTL_ALL            2
#define V5_MMSC_CFG3_NO_CCTL_IX_INVWB       3
#define V5_MMSC_CFG3_HVMCSR                 4
#define V5_MMSC_CFG3_PL2C                   5
#define V5_MMSC_CFG3_PL2_CMD                6
#define V5_MMSC_CFG3_CST_CTL                7

/* mmisc_ctl */
#define V5_MMISC_CTL_VEC_PLIC               1
#define V5_MMISC_CTL_RVCOMPM                2
#define V5_MMISC_CTL_BRPE                   3
#define V5_MMISC_CTL_MSA_OR_UNA             6
#define V5_MMISC_CTL_NON_BLOCKING           8

/* mcache_ctl */
#define V5_MCACHE_CTL_IC_FIRST_WORD         11
#define V5_MCACHE_CTL_DC_FIRST_WORD         12
#define V5_MCACHE_CTL_DC_COHEN              19
#define V5_MCACHE_CTL_DC_COHSTA             20

/* micm_cfg */
#define V5_MICM_CFG_ISZ                     6
#define V5_MICM_CFG_ILMB                    12
#define V5_MICM_CFG_ILMSZ                   15
#define MASK_MICM_CFG_ISET                  (0x07)
#define MASK_MICM_CFG_IWAY                  (0x07 << 3)
#define MASK_MICM_CFG_ISZ                   (0x07 << 6)
#define MASK_MICM_CFG_ILCK                  (0x01 << 9)
#define MASK_MICM_CFG_IC_ECC                (0x03 << 10)
#define MASK_MICM_CFG_ILMB                  (0x07 << 12)
#define MASK_MICM_CFG_ILMSZ                 (0x1F << 15)
#define MASK_MICM_CFG_ULM_2BANK             (0x01 << 20)
#define MASK_MICM_CFG_ILM_ECC               (0x03 << 21)
#define MASK_MICM_CFG_ILM_XONLY             (0x01 << 23)
#define MASK_MICM_CFG_SETH                  (0x01 << 24)
#define MASK_MICM_CFG_IC_REPL               (0x03 << 25)

/* mdcm_cfg */
#define V5_MDCM_CFG_DSZ                     6
#define V5_MDCM_CFG_DLMB                    12
#define V5_MDCM_CFG_DLMSZ                   15
#define MASK_MDCM_CFG_DSET                  (0x07)
#define MASK_MDCM_CFG_DWAY                  (0x07 << 3)
#define MASK_MDCM_CFG_DSZ                   (0x07 << 6)
#define MASK_MDCM_CFG_DLCK                  (0x01 << 9)
#define MASK_MDCM_CFG_DC_ECC                (0x03 << 10)
#define MASK_MDCM_CFG_DLMB                  (0x07 << 12)
#define MASK_MDCM_CFG_DLMSZ                 (0x1F << 15)
#define MASK_MDCM_CFG_ULM_2BANK             (0x01 << 20)
#define MASK_MDCM_CFG_DLM_ECC               (0x03 << 21)
#define MASK_MDCM_CFG_SETH                  (0x01 << 24)
#define MASK_MDCM_CFG_DC_REPL               (0x03 << 25)

/* mmsc_cfg */
#define MASK_MMSC_CFG_ECC                   (0x01)
#define MASK_MMSC_CFG_TLB_ECC               (0x03 << 1)
#define MASK_MMSC_CFG_ECD                   (0x01 << 3)
#define MASK_MMSC_CFG_PFT                   (0x01 << 4)
#define MASK_MMSC_CFG_HSP                   (0x01 << 5)
#define MASK_MMSC_CFG_ACE                   (0x01 << 6)
#define MASK_MMSC_CFG_ADDPMC                (0x1F << 7)
#define MASK_MMSC_CFG_VPLIC                 (0x01 << 12)
#define MASK_MMSC_CFG_EV5PE                 (0x01 << 13)
#define MASK_MMSC_CFG_LMSLVP                (0x01 << 14)
#define MASK_MMSC_CFG_PMNDS                 (0x01 << 15)
#define MASK_MMSC_CFG_CCTLCSR               (0x01 << 16)
#define MASK_MMSC_CFG_EFHW                  (0x01 << 17)
#define MASK_MMSC_CFG_VCCTL                 (0x03 << 18)
#define MASK_MMSC_CFG_EXCSLVL               (0x03 << 20)
#define MASK_MMSC_CFG_NOPMC                 (0x01 << 22)
#define MASK_MMSC_CFG_SPE_AFT               (0x01 << 23)
#define MASK_MMSC_CFG_ESLEEP                (0x01 << 24)
#define MASK_MMSC_CFG_PPI                   (0x01 << 25)
#define MASK_MMSC_CFG_FIO                   (0x01 << 26)
#define MASK_MMSC_CFG_CLIC                  (0x01 << 27)
#define MASK_MMSC_CFG_ECLIC                 (0x01 << 28)
#define MASK_MMSC_CFG_EDSP                  (0x01 << 29)
#define MASK_MMSC_CFG_PPMA                  (0x01 << 30)
/* below is only exists in RV32 */
/* cast to uint64_t for set_field() operation */
#define MASK_MMSC_CFG_MSC_EXT               ((uint64_t)0x01 << 31)
/* for RV64 mmsc_cfg */
#define MASK_MMSC_CFG_BF16CVT               ((uint64_t)0x01 << 32)
#define MASK_MMSC_CFG_ZFH                   ((uint64_t)0x01 << 33)
#define MASK_MMSC_CFG_VL4                   ((uint64_t)0x01 << 34)
#define MASK_MMSC_CFG_CRASHSAVE             ((uint64_t)0x01 << 35)
#define MASK_MMSC_CFG_VECCFG                ((uint64_t)0x01 << 36)
#define MASK_MMSC_CFG_FINV                  ((uint64_t)0x01 << 37)
#define MASK_MMSC_CFG_PP16                  ((uint64_t)0x01 << 38)
#define MASK_MMSC_CFG_VSIH                  ((uint64_t)0x01 << 40)
#define MASK_MMSC_CFG_ECDV                  ((uint64_t)0x03 << 41)
#define MASK_MMSC_CFG_VDOT                  ((uint64_t)0x01 << 43)
#define MASK_MMSC_CFG_VPFH                  ((uint64_t)0x01 << 44)
#define MASK_MMSC_CFG_CCACHEMP_CFG          ((uint64_t)0x01 << 45)
#define MASK_MMSC_CFG_CCACHE                ((uint64_t)0x01 << 46)
#define MASK_MMSC_CFG_IO_COHP               ((uint64_t)0x01 << 47)
#define MASK_MMSC_CFG_CORE_PCLUS            ((uint64_t)0x0F << 48)
#define MASK_MMSC_CFG_RVARCH                ((uint64_t)0x01 << 52)
#define MASK_MMSC_CFG_TLB_RAM_CMD           ((uint64_t)0x01 << 53)
#define MASK_MMSC_CFG_CCTL_FL_UL            ((uint64_t)0x01 << 54)
#define MASK_MMSC_CFG_HSPO                  ((uint64_t)0x01 << 55)
#define MASK_MMSC_CFG_XCSR                  ((uint64_t)0x01 << 56)
#define MASK_MMSC_CFG_ALT_FP_FMT            ((uint64_t)0x01 << 57)
#define MASK_MMSC_CFG_D_LMSLVP              ((uint64_t)0x01 << 58)
#define MASK_MMSC_CFG_I_LMSLVP              ((uint64_t)0x01 << 59)
#define MASK_MMSC_CFG_RVARCH2               ((uint64_t)0x01 << 60)
#define MASK_MMSC_CFG_BTB_ECC               ((uint64_t)0x03 << 61)
#define MASK_MMSC_CFG_MSC_EXT3              ((uint64_t)0x01 << 63)

/* mmsc_cfg2 */
#define MASK_MMSC_CFG2_BF16CVT              (0x01)
#define MASK_MMSC_CFG2_ZFH                  (0x01 << 1)
#define MASK_MMSC_CFG2_VL4                  (0x01 << 2)
#define MASK_MMSC_CFG2_CRASHSAVE            (0x01 << 3)
#define MASK_MMSC_CFG2_VECCFG               (0x01 << 4)
#define MASK_MMSC_CFG2_FINV                 (0x01 << 5)
#define MASK_MMSC_CFG2_PP16                 (0x01 << 6)
#define MASK_MMSC_CFG2_VSIH                 (0x01 << 8)
#define MASK_MMSC_CFG2_ECDV                 (0x03 << 9)
#define MASK_MMSC_CFG2_VDOT                 (0x01 << 11)
#define MASK_MMSC_CFG2_VPFH                 (0x01 << 12)
#define MASK_MMSC_CFG2_CCACHEMP_CFG         (0x01 << 13)
#define MASK_MMSC_CFG2_CCACHE               (0x01 << 14)
#define MASK_MMSC_CFG2_IO_COHP              (0x01 << 15)
#define MASK_MMSC_CFG2_CORE_PCLUS           (0x0F << 16)
#define MASK_MMSC_CFG2_RVARCH               (0x01 << 20)
#define MASK_MMSC_CFG2_TLB_RAM_CMD          (0x01 << 21)
#define MASK_MMSC_CFG2_CCTL_FL_UL           (0x01 << 22)
#define MASK_MMSC_CFG2_HSPO                 (0x01 << 23)
#define MASK_MMSC_CFG2_XCSR                 (0x01 << 24)
#define MASK_MMSC_CFG2_ALT_FP_FMT           (0x01 << 25)
#define MASK_MMSC_CFG2_D_LMSLVP             (0x01 << 26)
#define MASK_MMSC_CFG2_I_LMSLVP             (0x01 << 27)
#define MASK_MMSC_CFG2_RVARCH2              (0x01 << 28)
#define MASK_MMSC_CFG2_BTB_ECC              (0x03 << 29)
/* cast to uint64_t for set_field() operation */
#define MASK_MMSC_CFG2_MSC_EXT3             ((uint64_t)0x01 << 31)

/* mmsc_cfg3 */
#define MASK_MMSC_CFG3_BTB_RAM_CMD          (0x1)
#define MASK_MMSC_CFG3_NO_CCTL_VA           (0x1 << 1)
#define MASK_MMSC_CFG3_NO_CCTL_ALL          (0x1 << 2)
#define MASK_MMSC_CFG3_NO_CCTL_IX_INVWB     (0x1 << 3)
#define MASK_MMSC_CFG3_HVMCSR               (0x1 << 4)
#define MASK_MMSC_CFG3_PL2C                 (0x1 << 5)
#define MASK_MMSC_CFG3_PL2_CMD              (0x1 << 6)
#define MASK_MMSC_CFG3_CST_CTL              (0x1 << 7)
#define MASK_MMSC_CFG3_SHADOW               (0x1 << 8)
#define MASK_MMSC_CFG3_ROC                  (0x1 << 9)
#define MASK_MMSC_CFG3_AMM                  (0x1 << 10)
#define MASK_MMSC_CFG3_ECC_DETECTION_ONLY   (0x1 << 12)
#define MASK_MMSC_CFG3_IC_ECC_GRAN          (0x7 << 13)
#define MASK_MMSC_CFG3_ILM_ECC_GRAN         (0x7 << 16)
#define MASK_MMSC_CFG3_DC_ECC_GRAN          (0x7 << 19)
#define MASK_MMSC_CFG3_DLM_ECC_GRAN         (0x7 << 22)
#define MASK_MMSC_CFG3_TLB_ECC_GRAN         (0x7 << 25)
#define MASK_MMSC_CFG3_BTB_ECC_GRAN         (0x7 << 28)
/* cast to uint64_t for set_field() operation */
#define MASK_MMSC_CFG3_MSC_EXT4             ((uint64_t)0x1 << 31)
/* for RV64 mmsc_cfg_3 */
#define MASK_MMSC_CFG3_PPMA_VER             ((uint64_t)0x7 << 32)

/* mmsc_cfg4 */
#define MASK_MMSC_CFG4_PPMA_VER             (0x7)

/* mcause */
#define MASK_MCAUSE_EXCEPTION_CODE_32       (~MASK_MCAUSE_INTERRUPT_32)
/* cast to uint64_t for set_field() operation */
#define MASK_MCAUSE_INTERRUPT_32            ((uint64_t)1 << 31)
#define MASK_MCAUSE_EXCEPTION_CODE_64       (~MASK_MCAUSE_INTERRUPT_64)
#define MASK_MCAUSE_INTERRUPT_64            ((uint64_t)1 << 63)

/* mmisc_ctl */
#define MASK_MMISC_CTL_ACE                  (0x1)
#define MASK_MMISC_CTL_VEC_PLIC             (0x1 << 1)
#define MASK_MMISC_CTL_RVCOMPM              (0x1 << 2)
#define MASK_MMISC_CTL_BRPE                 (0x1 << 3)
#define MASK_MMISC_CTL_ACES                 (0x3 << 4)
#define MASK_MMISC_CTL_MSA_UNA              (0x1 << 6)
#define MASK_MMISC_CTL_NBLD_EN              (0x1 << 8)
#define MASK_MMISC_CTL_NEWNMI               (0x1 << 9)
#define MASK_MMISC_CTL_VCGL1_EN             (0x1 << 10)
#define MASK_MMISC_CTL_VCGL2_EN             (0x1 << 11)
#define MASK_MMISC_CTL_VCGL3_EN             (0x1 << 12)
#define MASK_MMISC_CTL_LDX0NXP              (0x1 << 13)

/* mrvarch_cfg */
#define MASK_MRVARCH_CFG_ZBA                (0x1)
#define MASK_MRVARCH_CFG_ZBB                (0x1 << 1)
#define MASK_MRVARCH_CFG_ZBC                (0x1 << 2)
#define MASK_MRVARCH_CFG_ZBS                (0x1 << 3)
#define MASK_MRVARCH_CFG_SMEPMP             (0x1 << 4)
#define MASK_MRVARCH_CFG_SVINVAL            (0x1 << 5)
#define MASK_MRVARCH_CFG_SMSTATEEN          (0x1 << 6)
#define MASK_MRVARCH_CFG_SSCOFPMF           (0x1 << 7)
#define MASK_MRVARCH_CFG_SSTC               (0x1 << 8)
#define MASK_MRVARCH_CFG_ZICBOM             (0x1 << 9)
#define MASK_MRVARCH_CFG_ZICBOP             (0x1 << 10)
#define MASK_MRVARCH_CFG_ZICBOZ             (0x1 << 11)
#define MASK_MRVARCH_CFG_ZBK                (0x1 << 12)
#define MASK_MRVARCH_CFG_ZKN                (0x1 << 13)
#define MASK_MRVARCH_CFG_ZKS                (0x1 << 14)
#define MASK_MRVARCH_CFG_ZKT                (0x1 << 15)
#define MASK_MRVARCH_CFG_ZKR                (0x1 << 16)
#define MASK_MRVARCH_CFG_SM_VERSION         (0x7 << 17)
#define MASK_MRVARCH_CFG_SS_VERSION         (0x7 << 20)
#define MASK_MRVARCH_CFG_SVPBMT             (0x1 << 23)
#define MASK_MRVARCH_CFG_SVNAPOT            (0x1 << 24)
#define MASK_MRVARCH_CFG_ZIHINTPAUSE        (0x1 << 25)
#define MASK_MRVARCH_CFG_ZCA                (0x1 << 26)
#define MASK_MRVARCH_CFG_ZCB                (0x1 << 27)
#define MASK_MRVARCH_CFG_ZCD                (0x1 << 28)
#define MASK_MRVARCH_CFG_ZCF                (0x1 << 29)
#define MASK_MRVARCH_CFG_ZCMP               (0x1 << 30)
/* cast to uint64_t for set_field() operation */
#define MASK_MRVARCH_CFG_ZCMT               ((uint64_t)0x1 << 31)
/* for RV64 mrvarch_cfg */
#define MASK_MRVARCH_CFG_ZFBFMIN            ((uint64_t)0x1 << 32)
#define MASK_MRVARCH_CFG_ZVFBFMIN           ((uint64_t)0x1 << 33)
#define MASK_MRVARCH_CFG_ZVFBFWMA           ((uint64_t)0x1 << 34)
#define MASK_MRVARCH_CFG_ZVQMAC             ((uint64_t)0x1 << 35)
#define MASK_MRVARCH_CFG_ZVLSSEG            ((uint64_t)0x1 << 36)
#define MASK_MRVARCH_CFG_ZICOND             ((uint64_t)0x1 << 37)
#define MASK_MRVARCH_CFG_ZIHINTNTL          ((uint64_t)0x1 << 38)
#define MASK_MRVARCH_CFG_ZFA                ((uint64_t)0x1 << 39)
#define MASK_MRVARCH_CFG_ZILSD              ((uint64_t)0x1 << 40)
#define MASK_MRVARCH_CFG_ZACAS              ((uint64_t)0x1 << 41)
#define MASK_MRVARCH_CFG_ZABHA              ((uint64_t)0x1 << 42)
#define MASK_MRVARCH_CFG_ZVFHMIN            ((uint64_t)0x1 << 43)
#define MASK_MRVARCH_CFG_ZVFH               ((uint64_t)0x1 << 44)
#define MASK_MRVARCH_CFG_ZCLSD              ((uint64_t)0x1 << 45)
#define MASK_MRVARCH_CFG_ZVBB               ((uint64_t)0x1 << 47)
#define MASK_MRVARCH_CFG_ZVBC               ((uint64_t)0x1 << 48)
#define MASK_MRVARCH_CFG_ZVKB               ((uint64_t)0x1 << 49)
#define MASK_MRVARCH_CFG_ZVKG               ((uint64_t)0x1 << 50)
#define MASK_MRVARCH_CFG_ZVKN               ((uint64_t)0x1 << 51)
#define MASK_MRVARCH_CFG_ZVKNC              ((uint64_t)0x1 << 52)
#define MASK_MRVARCH_CFG_ZVKNED             ((uint64_t)0x1 << 53)
#define MASK_MRVARCH_CFG_ZVKNG              ((uint64_t)0x1 << 54)
#define MASK_MRVARCH_CFG_ZVKNHA             ((uint64_t)0x1 << 55)
#define MASK_MRVARCH_CFG_ZVKBNHB            ((uint64_t)0x1 << 56)
#define MASK_MRVARCH_CFG_ZVKS               ((uint64_t)0x1 << 57)
#define MASK_MRVARCH_CFG_ZVKSC              ((uint64_t)0x1 << 58)
#define MASK_MRVARCH_CFG_ZVKSED             ((uint64_t)0x1 << 59)
#define MASK_MRVARCH_CFG_ZVKSG              ((uint64_t)0x1 << 60)
#define MASK_MRVARCH_CFG_ZVKSH              ((uint64_t)0x1 << 61)
#define MASK_MRVARCH_CFG_ZVKT               ((uint64_t)0x1 << 62)
#define MASK_MRVARCH_CFG_MRVARCH_EXT3       ((uint64_t)0x1 << 63)

/* mrvarch_cfg2 */
#define MASK_MRVARCH_CFG2_ZFBFMIN           (0x1)
#define MASK_MRVARCH_CFG2_ZVFBFMIN          (0x1 << 1)
#define MASK_MRVARCH_CFG2_ZVFBFWMA          (0x1 << 2)
#define MASK_MRVARCH_CFG2_ZVQMAC            (0x1 << 3)
#define MASK_MRVARCH_CFG2_ZVLSSEG           (0x1 << 4)
#define MASK_MRVARCH_CFG2_ZICOND            (0x1 << 5)
#define MASK_MRVARCH_CFG2_ZIHINTNTL         (0x1 << 6)
#define MASK_MRVARCH_CFG2_ZFA               (0x1 << 7)
#define MASK_MRVARCH_CFG2_ZILSD             (0x1 << 8)
#define MASK_MRVARCH_CFG2_ZACAS             (0x1 << 9)
#define MASK_MRVARCH_CFG2_ZABHA             (0x1 << 10)
#define MASK_MRVARCH_CFG2_ZVFHMIN           (0x1 << 11)
#define MASK_MRVARCH_CFG2_ZVFH              (0x1 << 12)
#define MASK_MRVARCH_CFG2_ZCLSD             (0x1 << 13)
#define MASK_MRVARCH_CFG2_ZVBB              (0x1 << 15)
#define MASK_MRVARCH_CFG2_ZVBC              (0x1 << 16)
#define MASK_MRVARCH_CFG2_ZVKB              (0x1 << 17)
#define MASK_MRVARCH_CFG2_ZVKG              (0x1 << 18)
#define MASK_MRVARCH_CFG2_ZVKN              (0x1 << 19)
#define MASK_MRVARCH_CFG2_ZVKNC             (0x1 << 20)
#define MASK_MRVARCH_CFG2_ZVKNED            (0x1 << 21)
#define MASK_MRVARCH_CFG2_ZVKNG             (0x1 << 22)
#define MASK_MRVARCH_CFG2_ZVKNHA            (0x1 << 23)
#define MASK_MRVARCH_CFG2_ZVKBNHB           (0x1 << 24)
#define MASK_MRVARCH_CFG2_ZVKS              (0x1 << 25)
#define MASK_MRVARCH_CFG2_ZVKSC             (0x1 << 26)
#define MASK_MRVARCH_CFG2_ZVKSED            (0x1 << 27)
#define MASK_MRVARCH_CFG2_ZVKSG             (0x1 << 28)
#define MASK_MRVARCH_CFG2_ZVKSH             (0x1 << 29)
#define MASK_MRVARCH_CFG2_ZVKT              (0x1 << 30)
/* cast to uint64_t for set_field() operation */
#define MASK_MRVARCH_CFG2_MRVARCH_EXT3      ((uint64_t)0x1 << 31)

/* mrvarch_cfg3 */
#define MASK_MRVARCH_CFG3_SMAIA             (0x1)
#define MASK_MRVARCH_CFG3_SSAIA             (0x1 << 1)
#define MASK_MRVARCH_CFG3_SPMP              (0x3 << 2)
#define MASK_MRVARCH_CFG3_SMRNMI            (0x3 << 4)
#define MASK_MRVARCH_CFG3_SSNPM             (0x1 << 6)
#define MASK_MRVARCH_CFG3_SMNPM             (0x1 << 7)
#define MASK_MRVARCH_CFG3_SMMPM             (0x1 << 8)
#define MASK_MRVARCH_CFG3_ZAWRS             (0x1 << 9)
#define MASK_MRVARCH_CFG3_SMCSRIND          (0x1 << 10)
#define MASK_MRVARCH_CFG3_SSCSRIND          (0x1 << 11)
#define MASK_MRVARCH_CFG3_RERI              (0x1 << 12)
#define MASK_MRVARCH_CFG3_ZIMOP             (0x1 << 24)
#define MASK_MRVARCH_CFG3_ZCMOP             (0x1 << 25)

/* mhsp_ctl */
#define MASK_MHSP_CTL_OVF_EN                (0x1)
#define MASK_MHSP_CTL_UDF_EN                (0x1 << 1)
#define MASK_MHSP_CTL_SCHM                  (0x1 << 2)
#define MASK_MHSP_CTL_U                     (0x1 << 3)
#define MASK_MHSP_CTL_S                     (0x1 << 4)
#define MASK_MHSP_CTL_M                     (0x1 << 5)

/* umisc_ctl */
#define MASK_UMISC_CTL_FP_MODE              (0x3)
#define MASK_UMISC_CTL_CMR_CTL              (0x3 << 2)
#define V5_UMISC_CTL_FP_MODE_FP16           0
#define V5_UMISC_CTL_FP_MODE_BF16           1

/* uzobctl */
#define MASK_UZOBCTL_ACTIVE_K               (0xFF)
#define MASK_UZOBCTL_ACTIVE_N               (0xFF << 8)
#define MASK_UZOBCTL_ACTIVE_M               (0xFF << 16)

/* mvec_cfg */
#define MASK_MVEC_CFG_MINOR                 (0xF)
#define MASK_MVEC_CFG_MAJOR                 (0xF << 4)
#define MASK_MVEC_CFG_DW                    (0x7 << 8)
#define MASK_MVEC_CFG_MW                    (0x7 << 11)
#define MASK_MVEC_CFG_MISEW                 (0x3 << 14)
#define MASK_MVEC_CFG_MFSEW                 (0x3 << 16)

/* mslideleg */
#define MASK_MSLIDELEG_IMECCI               (0x1 << 16)
#define MASK_MSLIDELEG_BWEI                 (0x1 << 17)
#define MASK_MSLIDELEG_PMOVI                (0x1 << 18)
#define MASK_MSLIDELEG_IMECCDMR             (0x1 << 19)
#define MASK_MSLIDELEG_ACEERR               (0x1 << 24)

/* mhvm_cfg */
#define MASK_MHVM_CFG_SZ                    (0x3F)
#define MASK_MHVM_CFG_BANK                  (0x1  << 8)
#define MASK_MHVM_CFG_BSEL                  (0x1F << 16)
#define MASK_MHVM_CFG_SUBP                  (0x1  << 24)

/* counters */
#define MASK_COUNTER_CY                     (0x1)
#define MASK_COUNTER_TM                     (0x1 << 1)
#define MASK_COUNTER_IR                     (0x1 << 2)
#define MASK_COUNTER_HPM3                   (0x1 << 3)
#define MASK_COUNTER_HPM4                   (0x1 << 4)
#define MASK_COUNTER_HPM5                   (0x1 << 5)
#define MASK_COUNTER_HPM6                   (0x1 << 6)

/* local irqs */
#define MASK_LOCAL_IRQ_IMECCI               (0x1 << 16)
#define MASK_LOCAL_IRQ_BWEI                 (0x1 << 17)
#define MASK_LOCAL_IRQ_PMOVI                (0x1 << 18)
#define MASK_LOCAL_IRQ_IMECCDMR             (0x1 << 19)
#define MASK_LOCAL_IRQ_ACCERR               (0x1 << 24)

/* write masks */
#define WRITE_MASK_CSR_MCOUNTEREN           0x7F
#define WRITE_MASK_CSR_MECC_CODE_32         0x7F
#define WRITE_MASK_CSR_MECC_CODE_64         0xFF
#define WRITE_MASK_CSR_UITB_32              0xFFFFFFFC
#define WRITE_MASK_CSR_UITB_64              0xFFFFFFFFFFFFFFFC
#define WRITE_MASK_CSR_MPFT_CTL             0x1F0
#define WRITE_MASK_CSR_COUNTER              0x7D
#define WRITE_MASK_CSR_HPMEVENT             0x1FF
#define WRITE_MASK_CSR_SLIE                 0x10F0000
#define WRITE_MASK_CSR_SLIP                 0xF0000
#define WRITE_MASK_CSR_SLIE_AIA             0x3800000
#define WRITE_MASK_CSR_SLIP_AIA             0x2800000
#define WRITE_MASK_CSR_MPPIB_32             0xFFFFFC3F
#define WRITE_MASK_CSR_MPPIB_64             0xFFFFFFFFFFFFFC3F
#define WRITE_MASK_CSR_MFIOB_32             0xFFFFFC3F
#define WRITE_MASK_CSR_MFIOB_64             0xFFFFFFFFFFFFFC3F
#define WRITE_MASK_CSR_UCODE                0x1
#define WRITE_MASK_CSR_UZOBCTL              0xFFFFFF
#define WRITE_MASK_CSR_MVEC_CFG             0x3FFFF
#define WRITE_MASK_CSR_MXSTATUS             0x3FF
#define WRITE_MASK_CSR_SMDCAUSE             0x7F
#define WRITE_MASK_CSR_MMISC_CTL            0x3F7F
#define WRITE_MASK_CSR_SMISC_CTL            0x30
#define WRITE_MASK_CSR_MCACHE_CTL           0x7FFFFF

void andes_csr_configs(CPURISCVState *env);
void andes_csr_init(AndesCsr *);
void andes_vec_init(AndesVec *);
void andes_cpu_do_interrupt_post(CPUState *cpu);

RISCVException andes_rmw_xireg_csrind(CPURISCVState *env, int csrno,
                                      target_ulong isel,
                                      target_ulong *ret_value,
                                      target_ulong new_value,
                                      target_ulong write_mask);

#endif /* RISCV_CSR_ANDES_INC_H */
