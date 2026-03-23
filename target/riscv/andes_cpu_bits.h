/*
 * Andes custom CSRs bit definitions
 *
 * Copyright (c) 2021 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0+
 */

/* ========= AndeStar V5 machine mode CSRs ========= */
/* Configuration Registers */
#define CSR_MICM_CFG            0xfc0
#define CSR_MDCM_CFG            0xfc1
#define CSR_MMSC_CFG            0xfc2
#define CSR_MMSC_CFG2           0xfc3
#define CSR_MMSC_CFG3           0xfc4
#define CSR_MMSC_CFG4           0xfc5
#define CSR_MVEC_CFG            0xfc7
#define CSR_MRVARCH_CFG         0xfca
#define CSR_MRVARCH_CFG2        0xfcb
#define CSR_MRVARCH_CFG3        0xfcc
#define CSR_MCCACHE_CTL_BASE    0xfcf
#define CSR_MHVM_CFG            0xfd0
#define CSR_MHVMB               0xfd1

/* Crash Debug CSRs */
#define CSR_MCRASH_STATESAVE    0xfc8
#define CSR_MSTATUS_CRASHSAVE   0xfc9

/* Memory CSRs */
#define CSR_MILMB               0x7c0
#define CSR_MDLMB               0x7c1
#define CSR_MECC_CODE           0x7C2
#define CSR_MNVEC               0x7c3
#define CSR_MCACHE_CTL          0x7ca
#define CSR_MCCTLBEGINADDR      0x7cb
#define CSR_MCCTLCOMMAND        0x7cc
#define CSR_MCCTLDATA           0x7cd
#define CSR_MPPIB               0x7f0
#define CSR_MFIOB               0x7f1

/* Hardware Stack Protection & Recording */
#define CSR_MHSP_CTL            0x7c6
#define CSR_MSP_BOUND           0x7c7
#define CSR_MSP_BASE            0x7c8
#define CSR_MXSTATUS            0x7c4
#define CSR_MDCAUSE             0x7c9
#define CSR_MSLIDELEG           0x7d5
#define CSR_MSAVESTATUS         0x7d6
#define CSR_MSAVEEPC1           0x7d7
#define CSR_MSAVECAUSE1         0x7d8
#define CSR_MSAVEEPC2           0x7d9
#define CSR_MSAVECAUSE2         0x7da
#define CSR_MSAVEDCAUSE1        0x7db
#define CSR_MSAVEDCAUSE2        0x7dc

/* Control CSRs */
#define CSR_MPFT_CTL            0x7c5
#define CSR_MMISC_CTL           0x7d0
#define CSR_MCLK_CTL            0x7df

/* Counter related CSRs */
#define CSR_MCOUNTERWEN         0x7ce
#define CSR_MCOUNTERINTEN       0x7cf
#define CSR_MCOUNTERMASK_M      0x7d1
#define CSR_MCOUNTERMASK_S      0x7d2
#define CSR_MCOUNTERMASK_U      0x7d3
#define CSR_MCOUNTEROVF         0x7d4

/* Enhanced CLIC CSRs */
#define CSR_MIRQ_ENTRY          0x7ec
#define CSR_MINTSEL_JAL         0x7ed
#define CSR_PUSHMCAUSE          0x7ee
#define CSR_PUSHMEPC            0x7ef
#define CSR_PUSHMXSTATUS        0x7eb

/* Andes Physical Memory Attribute(PMA) CSRs */
#define CSR_PMACFG0             0xbc0
#define CSR_PMACFG1             0xbc1
#define CSR_PMACFG2             0xbc2
#define CSR_PMACFG3             0xbc3
#define CSR_PMACFG4             0xbc4
#define CSR_PMACFG5             0xbc5
#define CSR_PMACFG6             0xbc6
#define CSR_PMACFG7             0xbc7
#define CSR_PMACFG8             0xbc8
#define CSR_PMACFG9             0xbc9
#define CSR_PMACFG10            0xbca
#define CSR_PMACFG11            0xbcb
#define CSR_PMAADDR0            0xbd0
#define CSR_PMAADDR1            0xbd1
#define CSR_PMAADDR2            0xbd2
#define CSR_PMAADDR3            0xbd3
#define CSR_PMAADDR4            0xbd4
#define CSR_PMAADDR5            0xbd5
#define CSR_PMAADDR6            0xbd6
#define CSR_PMAADDR7            0xbd7
#define CSR_PMAADDR8            0xbd8
#define CSR_PMAADDR9            0xbd9
#define CSR_PMAADDR10           0xbda
#define CSR_PMAADDR11           0xbdb
#define CSR_PMAADDR12           0xbdc
#define CSR_PMAADDR13           0xbdd
#define CSR_PMAADDR14           0xbde
#define CSR_PMAADDR15           0xbdf
#define CSR_PMAADDR16           0xbe0
#define CSR_PMAADDR17           0xbe1
#define CSR_PMAADDR18           0xbe2
#define CSR_PMAADDR19           0xbe3
#define CSR_PMAADDR20           0xbe4
#define CSR_PMAADDR21           0xbe5
#define CSR_PMAADDR22           0xbe6
#define CSR_PMAADDR23           0xbe7
#define CSR_PMAADDR24           0xbe8
#define CSR_PMAADDR25           0xbe9
#define CSR_PMAADDR26           0xbea
#define CSR_PMAADDR27           0xbeb
#define CSR_PMAADDR28           0xbec
#define CSR_PMAADDR29           0xbed
#define CSR_PMAADDR30           0xbee
#define CSR_PMAADDR31           0xbef
#define CSR_PMAADDR32           0xbf0
#define CSR_PMAADDR33           0xbf1
#define CSR_PMAADDR34           0xbf2
#define CSR_PMAADDR35           0xbf3
#define CSR_PMAADDR36           0xbf4
#define CSR_PMAADDR37           0xbf5
#define CSR_PMAADDR38           0xbf6
#define CSR_PMAADDR39           0xbf7
#define CSR_PMAADDR40           0xbf8
#define CSR_PMAADDR41           0xbf9
#define CSR_PMAADDR42           0xbfa
#define CSR_PMAADDR43           0xbfb
#define CSR_PMAADDR44           0xbfc
#define CSR_PMAADDR45           0xbfd
#define CSR_PMAADDR46           0xbfe
#define CSR_PMAADDR47           0xbff

/* ========= AndeStar V5 supervisor mode CSRs ========= */
/* Supervisor trap registers */
#define CSR_SLIE                0x9c4
#define CSR_SLIP                0x9c5
#define CSR_SDCAUSE             0x9c9

/* Supervisor counter registers */
#define CSR_SCOUNTERINTEN       0x9cf
#define CSR_SCOUNTERMASK_M      0x9d1
#define CSR_SCOUNTERMASK_S      0x9d2
#define CSR_SCOUNTERMASK_U      0x9d3
#define CSR_SCOUNTEROVF         0x9d4
#define CSR_SCOUNTINHIBIT       0x9e0
#define CSR_SHPMEVENT3          0x9e3
#define CSR_SHPMEVENT4          0x9e4
#define CSR_SHPMEVENT5          0x9e5
#define CSR_SHPMEVENT6          0x9e6

/* Supervisor control registers */
#define CSR_SCCTLDATA           0x9cd
#define CSR_SMISC_CTL           0x9d0

/* ========= AndeStar V5 user mode CSRs ========= */
/* User mode control registers */
#define CSR_UITB                0x800
#define CSR_UCODE               0x801
#define CSR_UZOBCTL             0x808
#define CSR_UDCAUSE             0x809
#define CSR_UCCTLBEGINADDR      0x80b
#define CSR_UCCTLCOMMAND        0x80c
#define CSR_WFE                 0x810
#define CSR_SLEEPVALUE          0x811
#define CSR_TXEVT               0x812
#define CSR_UMISC_CTL           0x813

/* ========= AndeStar V5 Indirect CSRs ========= */
#define CSRIND_ISEL_MASK        0xfffffff
#define CSRIND_ISEL_SHADOW      0x1000
#define CSRIND_ISEL_IMPD01      0x2000
#define CSRIND_ISEL_IMPD23      0x2001
#define CSRIND_ISEL_IMPD4       0x2002
#define CSRIND_ISEL_PMA_FIRST   0x3000
#define CSRIND_ISEL_PMA_LAST    0x303f
#define CSRIND_IMPD0            0x0
#define CSRIND_IMPD1            0x1
#define CSRIND_IMPD2            0x2
#define CSRIND_IMPD3            0x3
#define CSRIND_IMPD4            0x4
#define CSRIND_SHADOW_CFG       0x10
#define CSRIND_SHADOW_CTL       0x11
#define CSRIND_SHADOW_DBG       0x12
#define CSRIND_IPMACFG0         0x20
#define CSRIND_IPMAADDR48       0x60

/* ========= AndeStar V5 CPUID definitions ========= */
#define ANDES_CPUID_MSB_32      (1 << 31)
#define ANDES_CPUID_MSB_64      (1ULL << 63)
#define ANDES_CPUID_A25         (ANDES_CPUID_MSB_32 | 0x0a25)
#define ANDES_CPUID_A27         (ANDES_CPUID_MSB_32 | 0x0a27)
#define ANDES_CPUID_A45         (ANDES_CPUID_MSB_32 | 0x0a45)
#define ANDES_CPUID_A46         (ANDES_CPUID_MSB_32 | 0x0a46)
#define ANDES_CPUID_D23         (ANDES_CPUID_MSB_32 | 0x0023)
#define ANDES_CPUID_N25         (ANDES_CPUID_MSB_32 | 0x0025)
#define ANDES_CPUID_N225        (ANDES_CPUID_MSB_32 | 0x50022)
#define ANDES_CPUID_N45         (ANDES_CPUID_MSB_32 | 0x0045)
#define ANDES_CPUID_AX25        (ANDES_CPUID_MSB_64 | 0x8a25)
#define ANDES_CPUID_AX27        (ANDES_CPUID_MSB_64 | 0x8a27)
#define ANDES_CPUID_AX45        (ANDES_CPUID_MSB_64 | 0x8a45)
#define ANDES_CPUID_AX46        (ANDES_CPUID_MSB_64 | 0x8a46)
#define ANDES_CPUID_AX65        (ANDES_CPUID_MSB_64 | 0x8a65)
#define ANDES_CPUID_AX66        (ANDES_CPUID_MSB_64 | 0x8a66)
#define ANDES_CPUID_NX25        (ANDES_CPUID_MSB_64 | 0x8025)
#define ANDES_CPUID_NX27V       (ANDES_CPUID_MSB_64 | 0x8027)
#define ANDES_CPUID_NX45        (ANDES_CPUID_MSB_64 | 0x8045)
