/*
 * QEMU RISC-V IOPMP (Input Output Physical Memory Protection)
 *
 * Copyright (c) 2023-2024 Andes Tech. Corp.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "trace.h"
#include "exec/exec-all.h"
#include "exec/address-spaces.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/misc/atciopmp200.h"
#include "memory.h"
#include "hw/irq.h"
#include "hw/registerfields.h"
#include "trace.h"
#include "hw/stream.h"
#include "hw/misc/riscv_iopmp_transaction_info.h"
#include "qemu/main-loop.h"

#define TYPE_ATCIOPMP200 "atciopmp200"
OBJECT_DECLARE_SIMPLE_TYPE(Atciopmp200state, ATCIOPMP200)

typedef struct {
    uint32_t addr_reg;
    uint32_t addrh_reg;
    uint32_t cfg_reg;
} iopmp200_entry_t;

typedef struct {
    uint32_t *srcmd_en;
    uint32_t *mdcfg;
    iopmp200_entry_t *entry;
    uint32_t mdlck;
    uint32_t entrylck;
    uint32_t mdcfglck;
    volatile uint32_t mdstall;
    uint32_t err_cfg;
    uint64_t err_reqaddr;
    uint32_t err_reqid;
    uint32_t err_reqinfo;
} iopmp200_regs;

typedef struct Atciopmp200state {
    SysBusDevice parent_obj;
    iopmp_addr_t *entry_addr;
    MemoryRegion mmio;
    IOMMUMemoryRegion iommu;
    iopmp200_regs regs;
    MemoryRegion *downstream;
    MemoryRegion blocked_r, blocked_w, blocked_x, blocked_rw, blocked_rx,
                 blocked_wx, blocked_rwx;

    iopmp_transaction_state *transaction_state;
    QemuMutex iopmp_transaction_mutex;
    Iopmp_StreamSink transaction_info_sink;

    AddressSpace iopmp_sysbus_as;
    AddressSpace downstream_as;
    AddressSpace blocked_r_as, blocked_w_as, blocked_x_as, blocked_rw_as,
                 blocked_rx_as, blocked_wx_as, blocked_rwx_as;
    qemu_irq irq;
    bool enable;

    /*
     * The hardware defined parameters, which could be modified by device
     * property
     */
    /* Indicates if TOR is supported */
    bool tor_en;
    /* Device implements interrupt suppression per entry*/
    bool peis;
    /* Device implements the error suppression per entry*/
    bool pees;
    /* Indicates the supported number of RRID */
    uint32_t rrid_num;
    /* Indicates the supported number of MD */
    uint32_t md_num;
    uint32_t k;
    /*
     * Data value to be returned for all read accesses that violate the security
     * check
     */
    uint32_t err_rdata;

    /*
     * The hardware defined parameters, which are fixed or indirect modified by
     * device property
     */
    /* entry_offset is fixed at 0x2000 */
    uint32_t entry_offset;
    /* entry_num = md_num * k */
    uint32_t entry_num;
} Atciopmp200state;

#define TYPE_IOPMP200_IOMMU_MEMORY_REGION "iopmp200-iommu-memory-region"
#define TYPE_IOPMP200_TRANSACTION_INFO_SINK "iopmp200_transaction_info_sink"

DECLARE_INSTANCE_CHECKER(Iopmp_StreamSink, IOPMP_TRANSACTION_INFO_SINK,
                         TYPE_IOPMP200_TRANSACTION_INFO_SINK)

#define ATCIOPMP200_MAX_MD_NUM            31
#define ATCIOPMP200_MAX_RRID_NUM          33
#define ATCIOPMP200_MAX_K                 16

#define VENDER_ANDES                      0
#define SPECVER_1_0_0_DRAFT6              0
#define IMPID_ATCIOPMP200                 0x00302000

REG32(VERSION, 0x00)
    FIELD(VERSION, VENDOR, 0, 24)
    FIELD(VERSION, SPECVER , 24, 8)
REG32(IMP, 0x04)
    FIELD(IMP, IMPID, 0, 32)
REG32(HWCFG0, 0x08)
    FIELD(HWCFG0, MODEL, 0, 4)
    FIELD(HWCFG0, TOR_EN, 4, 1)
    FIELD(HWCFG0, SPS_EN, 5, 1)
    FIELD(HWCFG0, USER_CFG_EN, 6, 1)
    FIELD(HWCFG0, PRIENT_PROG, 7, 1)
    FIELD(HWCFG0, RRID_TRANSL_EN, 8, 1)
    FIELD(HWCFG0, RRID_TRANSL_PROG, 9, 1)
    FIELD(HWCFG0, CHK_X, 10, 1)
    FIELD(HWCFG0, NO_X, 11, 1)
    FIELD(HWCFG0, NO_W, 12, 1)
    FIELD(HWCFG0, STALL_EN, 13, 1)
    FIELD(HWCFG0, PEIS, 14, 1)
    FIELD(HWCFG0, PEES, 15, 1)
    FIELD(HWCFG0, MFR_EN, 16, 1)
    FIELD(HWCFG0, MD_NUM, 24, 7)
    FIELD(HWCFG0, ENABLE, 31, 1)
REG32(HWCFG1, 0x0C)
    FIELD(HWCFG1, RRID_NUM, 0, 16)
    FIELD(HWCFG1, ENTRY_NUM, 16, 16)
REG32(HWCFG2, 0x10)
    FIELD(HWCFG2, PRIO_ENTRY, 0, 16)
    FIELD(HWCFG2, RRID_TRANSL, 16, 16)
REG32(ENTRYOFFSET, 0x14)
    FIELD(ENTRYOFFSET, OFFSET, 0, 32)
REG32(MDSTALL, 0x30)
    FIELD(MDSTALL, EXEMPT_AND_IS_STALLED, 0, 1)
    FIELD(MDSTALL, MD, 1, 31)
REG32(MDLCK, 0x40)
    FIELD(MDLCK, L, 0, 1)
    FIELD(MDLCK, MD, 1, 31)
REG32(MDCFGLCK, 0x48)
    FIELD(MDCFGLCK, L, 0, 1)
    FIELD(MDCFGLCK, F, 1, 7)
REG32(ENTRYLCK, 0x4C)
    FIELD(ENTRYLCK, L, 0, 1)
    FIELD(ENTRYLCK, F, 1, 16)
REG32(ERR_CFG, 0x60)
    FIELD(ERR_CFG, L, 0, 1)
    FIELD(ERR_CFG, IE, 1, 1)
    FIELD(ERR_CFG, IRE, 2, 1)
    FIELD(ERR_CFG, IWE, 3, 1)
    FIELD(ERR_CFG, IXE, 4, 1)
    FIELD(ERR_CFG, RRE, 5, 1)
    FIELD(ERR_CFG, RWE, 6, 1)
    FIELD(ERR_CFG, RXE, 7, 1)
REG32(ERR_REQINFO, 0x64)
    FIELD(ERR_REQINFO, IP, 0, 1)
    FIELD(ERR_REQINFO, TTYPE, 1, 2)
    FIELD(ERR_REQINFO, ETYPE, 4, 3)
    FIELD(ERR_REQINFO, SVC, 7, 1)
REG32(ERR_REQADDR, 0x68)
    FIELD(ERR_REQADDR, ADDR, 0, 32)
REG32(ERR_REQADDRH, 0x6C)
    FIELD(ERR_REQADDRH, ADDRH, 0, 32)
REG32(ERR_REQID, 0x70)
    FIELD(ERR_REQID, RRID, 0, 16)
    FIELD(ERR_REQID, EID, 16, 16)
REG32(ERR_MFR, 0x74)
    FIELD(ERR_MFR, SVW, 0, 16)
    FIELD(ERR_MFR, SVI, 16, 12)
    FIELD(ERR_MFR, SVS, 31, 1)
REG32(MDCFG0, 0x800)
    FIELD(MDCFG0, T, 0, 16)
REG32(SRCMD_EN0, 0x1000)
    FIELD(SRCMD_EN0, L, 0, 1)
    FIELD(SRCMD_EN0, MD, 1, 31)

FIELD(ENTRY_ADDR, ADDR, 0, 32)
FIELD(ENTRY_ADDRH, ADDRH, 0, 32)

FIELD(ENTRY_CFG, R, 0, 1)
FIELD(ENTRY_CFG, W, 1, 1)
FIELD(ENTRY_CFG, X, 2, 1)
FIELD(ENTRY_CFG, A, 3, 2)
FIELD(ENTRY_CFG, SIE, 5, 3)
FIELD(ENTRY_CFG, SIRE, 5, 1)
FIELD(ENTRY_CFG, SIWE, 6, 1)
FIELD(ENTRY_CFG, SIXE, 7, 1)
FIELD(ENTRY_CFG, SEE, 8, 3)
FIELD(ENTRY_CFG, SERE, 8, 1)
FIELD(ENTRY_CFG, SEWE, 9, 1)
FIELD(ENTRY_CFG, SEXE, 10, 1)

/* Offsets to SRCMD_EN(i) */
#define SRCMD_EN_OFFSET  0x0

/* Offsets to ENTRY_ADDR(i) */
#define ENTRY_ADDR_OFFSET     0x0
#define ENTRY_ADDRH_OFFSET    0x4
#define ENTRY_CFG_OFFSET      0x8

typedef enum {
    RRE_ERROR,
    RRE_SUCCESS_VALUE,
} atciopmp200_read_reaction;

typedef enum {
    RWE_ERROR,
    RWE_SUCCESS,
} atciopmp200_write_reaction;

typedef enum {
    RXE_ERROR,
    RXE_SUCCESS_VALUE,
} iopmp_exec_reaction;

typedef enum {
    ERR_REQINFO_TTYPE_NOERROR,
    ERR_REQINFO_TTYPE_READ,
    ERR_REQINFO_TTYPE_WRITE,
    ERR_REQINFO_TTYPE_FETCH
} iopmp_err_reqinfo_ttype;

typedef enum {
    ERR_REQINFO_ETYPE_NOERROR,
    ERR_REQINFO_ETYPE_READ,
    ERR_REQINFO_ETYPE_WRITE,
    ERR_REQINFO_ETYPE_FETCH,
    ERR_REQINFO_ETYPE_PARHIT,
    ERR_REQINFO_ETYPE_NOHIT,
    ERR_REQINFO_ETYPE_RRID,
    ERR_REQINFO_ETYPE_USER
} iopmp_err_reqinfo_etype;

typedef enum {
    IOPMP_MODEL_FULL,
    IOPMP_MODEL_RAPIDK,
    IOPMP_MODEL_DYNAMICK,
    IOPMP_MODEL_ISOLATION,
    IOPMP_MODEL_COMPACTK
} iopmp_model;

typedef enum {
    IOPMP_ENTRY_NO_HIT,
    IOPMP_ENTRY_PAR_HIT,
    IOPMP_ENTRY_HIT
} iopmp_entry_hit;

typedef enum {
    IOPMP_ACCESS_READ  = 1,
    IOPMP_ACCESS_WRITE = 2,
    IOPMP_ACCESS_FETCH = 3
} iopmp_access_type;

static void iopmp_iommu_notify(Atciopmp200state *s)
{
    IOMMUTLBEvent event = {
        .entry = {
            .iova = 0,
            .translated_addr = 0,
            .addr_mask = -1ULL,
            .perm = IOMMU_NONE,
        },
        .type = IOMMU_NOTIFIER_UNMAP,
    };

    for (int i = 0; i < s->rrid_num; i++) {
        memory_region_notify_iommu(&s->iommu, i, event);
    }
}

static void iopmp_decode_napot(uint64_t a, uint64_t *sa,
                               uint64_t *ea)
{
    /*
     * aaaa...aaa0   8-byte NAPOT range
     * aaaa...aa01   16-byte NAPOT range
     * aaaa...a011   32-byte NAPOT range
     * ...
     * aa01...1111   2^XLEN-byte NAPOT range
     * a011...1111   2^(XLEN+1)-byte NAPOT range
     * 0111...1111   2^(XLEN+2)-byte NAPOT range
     *  1111...1111   Reserved
     */

    a = (a << 2) | 0x3;
    *sa = a & (a + 1);
    *ea = a | (a + 1);
}

static void iopmp_update_rule(Atciopmp200state *s, uint32_t entry_index)
{
    uint8_t this_cfg = s->regs.entry[entry_index].cfg_reg;
    uint64_t this_addr = s->regs.entry[entry_index].addr_reg |
                         ((uint64_t)s->regs.entry[entry_index].addrh_reg << 32);
    uint64_t prev_addr = 0u;
    uint64_t sa = 0u;
    uint64_t ea = 0u;

    if (entry_index >= 1u) {
        prev_addr = s->regs.entry[entry_index - 1].addr_reg |
                    ((uint64_t)s->regs.entry[entry_index - 1].addrh_reg << 32);
    }

    switch (FIELD_EX32(this_cfg, ENTRY_CFG, A)) {
    case IOPMP_AMATCH_OFF:
        sa = 0u;
        ea = -1;
        break;

    case IOPMP_AMATCH_TOR:
        sa = (prev_addr) << 2; /* shift up from [xx:0] to [xx+2:2] */
        ea = ((this_addr) << 2) - 1u;
        if (sa > ea) {
            sa = ea = 0u;
        }
        break;

    case IOPMP_AMATCH_NA4:
        sa = this_addr << 2; /* shift up from [xx:0] to [xx+2:2] */
        ea = (sa + 4u) - 1u;
        break;

    case IOPMP_AMATCH_NAPOT:
        iopmp_decode_napot(this_addr, &sa, &ea);
        break;

    default:
        sa = 0u;
        ea = 0u;
        break;
    }

    s->entry_addr[entry_index].sa = sa;
    s->entry_addr[entry_index].ea = ea;
    iopmp_iommu_notify(s);
}

static MemTxResult atciopmp200_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    Atciopmp200state *s = ATCIOPMP200(opaque);
    uint32_t rz = 0;
    uint32_t offset, idx;
    MemTxResult tx_result = MEMTX_OK;

    switch (addr) {
    case A_VERSION:
        rz = FIELD_DP32(rz, VERSION, VENDOR, VENDER_ANDES);
        rz = FIELD_DP32(rz, VERSION, SPECVER, SPECVER_1_0_0_DRAFT6);
        break;
    case A_IMP:
        rz = IMPID_ATCIOPMP200;
        break;
    case A_HWCFG0:
        rz = FIELD_DP32(rz, HWCFG0, MODEL, IOPMP_MODEL_RAPIDK);
        rz = FIELD_DP32(rz, HWCFG0, TOR_EN, s->tor_en);
        rz = FIELD_DP32(rz, HWCFG0, SPS_EN, 0);
        rz = FIELD_DP32(rz, HWCFG0, USER_CFG_EN, 0);
        rz = FIELD_DP32(rz, HWCFG0, PRIENT_PROG, 0);
        rz = FIELD_DP32(rz, HWCFG0, RRID_TRANSL_EN, 0);
        rz = FIELD_DP32(rz, HWCFG0, RRID_TRANSL_PROG, 0);
        rz = FIELD_DP32(rz, HWCFG0, CHK_X, 1);
        rz = FIELD_DP32(rz, HWCFG0, NO_X, 0);
        rz = FIELD_DP32(rz, HWCFG0, NO_W, 0);
        rz = FIELD_DP32(rz, HWCFG0, STALL_EN, 1);
        rz = FIELD_DP32(rz, HWCFG0, PEIS, s->peis);
        rz = FIELD_DP32(rz, HWCFG0, PEES, s->pees);
        rz = FIELD_DP32(rz, HWCFG0, MFR_EN, 0);
        rz = FIELD_DP32(rz, HWCFG0, MD_NUM, s->md_num);
        rz = FIELD_DP32(rz, HWCFG0, ENABLE, s->enable);
        break;
    case A_HWCFG1:
        rz = FIELD_DP32(rz, HWCFG1, RRID_NUM, s->rrid_num);
        rz = FIELD_DP32(rz, HWCFG1, ENTRY_NUM, s->entry_num);
        break;
    case A_HWCFG2:
        rz = s->entry_num;
        break;
    case A_ENTRYOFFSET:
        rz = s->entry_offset;
        break;
    case A_MDSTALL:
        rz = s->regs.mdstall;
        break;
    case A_MDLCK:
        rz = s->regs.mdlck;
        break;
        break;
    case A_MDCFGLCK:
        rz = s->regs.mdcfglck;
        break;
    case A_ENTRYLCK:
        rz = s->regs.entrylck;
        break;
    case A_ERR_CFG:
        rz = s->regs.err_cfg;
        break;
    case A_ERR_REQADDR:
        rz = s->regs.err_reqaddr & UINT32_MAX;
        break;
    case A_ERR_REQADDRH:
        rz = s->regs.err_reqaddr >> 32;
        break;
    case A_ERR_REQID:
        rz = s->regs.err_reqid;
        break;
    case A_ERR_REQINFO:
        rz = s->regs.err_reqinfo;
        break;

    default:
        if (addr >= A_MDCFG0 &&
            addr < A_MDCFG0 + 4 * (s->md_num - 1)) {
            offset = addr - A_MDCFG0;
            idx = offset >> 2;
            if (idx == 0 && offset == 0) {
                rz = s->regs.mdcfg[idx];
            } else {
                /* Only MDCFG0 is implemented in rapid-k model */
                tx_result = MEMTX_ERROR;
                qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                              (int)addr);
            }
        } else if (addr >= A_SRCMD_EN0 &&
                   addr <= A_SRCMD_EN0 + 32 * (s->rrid_num - 1)) {
            offset = addr - A_SRCMD_EN0;
            idx = offset >> 5;
            offset &= 0x1f;

            switch (offset) {
            case SRCMD_EN_OFFSET:
                rz = s->regs.srcmd_en[idx];
                break;
            default:
                tx_result = MEMTX_ERROR;
                qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                              (int)addr);
                break;
            }
        } else if (addr >= s->entry_offset &&
                   addr <= s->entry_offset + ENTRY_CFG_OFFSET +
                           16 * (s->entry_num - 1)) {
            offset = addr - s->entry_offset;
            idx = offset >> 4;
            offset &= 0xf;

            switch (offset) {
            case ENTRY_ADDR_OFFSET:
                rz = s->regs.entry[idx].addr_reg;
                break;
            case ENTRY_ADDRH_OFFSET:
                rz = s->regs.entry[idx].addrh_reg;
                break;
            case ENTRY_CFG_OFFSET:
                rz = s->regs.entry[idx].cfg_reg;
                break;
            default:
                tx_result = MEMTX_ERROR;
                qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                              (int)addr);
                break;
            }
        } else {
            tx_result = MEMTX_ERROR;
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                          (int)addr);
        }
        break;
    }
    *data = rz;
    trace_atciopmp200_read(addr, rz);
    return tx_result;
}

static MemTxResult atciopmp200_write(void *opaque, hwaddr addr, uint64_t value,
                                     unsigned size, MemTxAttrs attrs)
{
    Atciopmp200state *s = ATCIOPMP200(opaque);
    uint32_t offset, idx;
    uint32_t value32 = value;
    MemTxResult tx_result = MEMTX_OK;
    uint32_t value_f;

    trace_atciopmp200_write(addr, value32);

    switch (addr) {
    case A_VERSION: /* RO */
        break;
    case A_IMP: /* RO */
        break;
    case A_HWCFG0:
        if (FIELD_EX32(value32, HWCFG0, ENABLE)) {
            /* W1S */
            s->enable = 1;
            iopmp_iommu_notify(s);
        }
        break;
    case A_HWCFG1: /* RO */
        break;
    case A_HWCFG2:
        break;
    case A_MDSTALL:
        s->regs.mdstall = FIELD_EX32(value32, MDSTALL, EXEMPT_AND_IS_STALLED);
        iopmp_iommu_notify(s);
        break;
    case A_MDLCK:
        if (!FIELD_EX32(s->regs.mdlck, MDLCK, L)) {
            /* sticky to 1 */
            s->regs.mdlck |= value32;
            /* Mask out bits exceeding (md_num + lock) */
            s->regs.mdlck = extract32(s->regs.mdlck, 0, s->md_num + 1);
        }
        break;
    case A_MDCFGLCK:
        break;
    case A_ENTRYLCK:
        if (!(FIELD_EX32(s->regs.entrylck, ENTRYLCK, L))) {
            value_f = FIELD_EX32(value32, ENTRYLCK, F);
            if (value_f > FIELD_EX32(s->regs.entrylck, ENTRYLCK, F)) {
                s->regs.entrylck = FIELD_DP32(s->regs.entrylck, ENTRYLCK, F,
                                              value_f);
            }
            s->regs.entrylck = FIELD_DP32(s->regs.entrylck, ENTRYLCK, L,
                                          FIELD_EX32(value32, ENTRYLCK, L));
        }
        break;
    case A_ERR_CFG:
        if (!FIELD_EX32(s->regs.err_cfg, ERR_CFG, L)) {
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, L,
                FIELD_EX32(value32, ERR_CFG, L));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, IE,
                FIELD_EX32(value32, ERR_CFG, IE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, IRE,
                FIELD_EX32(value32, ERR_CFG, IRE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, RRE,
                FIELD_EX32(value32, ERR_CFG, RRE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, IWE,
                FIELD_EX32(value32, ERR_CFG, IWE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, RWE,
                FIELD_EX32(value32, ERR_CFG, RWE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, IXE,
                FIELD_EX32(value32, ERR_CFG, IXE));
            s->regs.err_cfg = FIELD_DP32(s->regs.err_cfg, ERR_CFG, RXE,
                FIELD_EX32(value32, ERR_CFG, RXE));
        }
        break;
    case A_ERR_REQADDR: /* RO */
        break;
    case A_ERR_REQADDRH: /* RO */
        break;
    case A_ERR_REQID: /* RO */
        break;
    case A_ERR_REQINFO:
        if (FIELD_EX32(value32, ERR_REQINFO, IP)) {
            s->regs.err_reqinfo = FIELD_DP32(s->regs.err_reqinfo,
                                             ERR_REQINFO, IP, 0);
            qemu_set_irq(s->irq, 0);
        }
        break;

    default:
        if (addr >= A_MDCFG0 &&
            addr < A_MDCFG0 + 4 * (s->md_num - 1)) {
            offset = addr - A_MDCFG0;
            idx = offset >> 2;
            /* RO in rapid-k model */
            if (idx > 0) {
                qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                              (int)addr);
            }
        } else if (addr >= A_SRCMD_EN0 &&
                   addr <= A_SRCMD_EN0 + 32 * (s->rrid_num - 1)) {
            offset = addr - A_SRCMD_EN0;
            idx = offset >> 5;
            offset &= 0x1f;

            if (offset % 4) {
                tx_result = MEMTX_ERROR;
                qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                              (int)addr);
            } else if (FIELD_EX32(s->regs.srcmd_en[idx], SRCMD_EN0, L)
                        == 0) {
                switch (offset) {
                case SRCMD_EN_OFFSET:
                    s->regs.srcmd_en[idx] =
                        FIELD_DP32(s->regs.srcmd_en[idx], SRCMD_EN0, L,
                                   FIELD_EX32(value32, SRCMD_EN0, L));

                    /* MD field is protected by mdlck */
                    value32 = (value32 & ~s->regs.mdlck) |
                              (s->regs.srcmd_en[idx] & s->regs.mdlck);
                    s->regs.srcmd_en[idx] =
                        FIELD_DP32(s->regs.srcmd_en[idx], SRCMD_EN0, MD,
                                   FIELD_EX32(value32, SRCMD_EN0, MD));
                    /* Mask out bits exceeding (md_num + lock) */
                    s->regs.srcmd_en[idx] = extract32(s->regs.srcmd_en[idx], 0,
                                                      s->md_num + 1);
                    break;
                default:
                    tx_result = MEMTX_ERROR;
                    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n",
                                  __func__, (int)addr);
                    break;
                }
            }
        } else if (addr >= s->entry_offset &&
                   addr <= s->entry_offset + ENTRY_CFG_OFFSET
                           + 16 * (s->entry_num - 1)) {
            offset = addr - s->entry_offset;
            idx = offset >> 4;
            offset &= 0xf;

            /* index < ENTRYLCK_F is protected */
            if (idx >= FIELD_EX32(s->regs.entrylck, ENTRYLCK, F)) {
                switch (offset) {
                case ENTRY_ADDR_OFFSET:
                    s->regs.entry[idx].addr_reg = value32;
                    break;
                case ENTRY_ADDRH_OFFSET:
                    s->regs.entry[idx].addrh_reg = value32;
                    break;
                case ENTRY_CFG_OFFSET:
                    s->regs.entry[idx].cfg_reg = value32;
                    if (!s->tor_en &&
                        FIELD_EX32(s->regs.entry[idx + 1].cfg_reg,
                                   ENTRY_CFG, A) == IOPMP_AMATCH_TOR) {
                        s->regs.entry[idx].cfg_reg =
                            FIELD_DP32(s->regs.entry[idx].cfg_reg, ENTRY_CFG, A,
                                       IOPMP_AMATCH_OFF);
                    }
                    if (!s->peis) {
                        s->regs.entry[idx].cfg_reg =
                            FIELD_DP32(s->regs.entry[idx].cfg_reg, ENTRY_CFG,
                                       SIE, 0);
                    }
                    if (!s->pees) {
                        s->regs.entry[idx].cfg_reg =
                            FIELD_DP32(s->regs.entry[idx].cfg_reg, ENTRY_CFG,
                                       SEE, 0);
                    }
                    break;
                default:
                    tx_result = MEMTX_ERROR;
                    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n",
                                  __func__, (int)addr);
                    break;
                }
                iopmp_update_rule(s, idx);
                if (idx + 1 < s->entry_num &&
                    FIELD_EX32(s->regs.entry[idx + 1].cfg_reg, ENTRY_CFG, A) ==
                    IOPMP_AMATCH_TOR) {
                    iopmp_update_rule(s, idx + 1);
                }
            }
        } else {
            tx_result = MEMTX_ERROR;
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %x\n", __func__,
                          (int)addr);
        }
        break;
    }
    return tx_result;
}

/* Match entry in memory domain */
static iopmp_entry_hit match_entry_md(Atciopmp200state *s, int md_idx,
                                      hwaddr start_addr, hwaddr end_addr,
                                      int *entry_idx,
                                      int *priority_entry_in_tlb)
{
    int entry_idx_s, entry_idx_e;
    iopmp_entry_hit result = IOPMP_ENTRY_NO_HIT;
    int i = 0;
    hwaddr tlb_sa = start_addr & ~(TARGET_PAGE_SIZE - 1);
    hwaddr tlb_ea = (end_addr & ~(TARGET_PAGE_SIZE - 1)) + TARGET_PAGE_SIZE - 1;

    entry_idx_s = md_idx * s->regs.mdcfg[0];
    entry_idx_e = (md_idx + 1) * s->regs.mdcfg[0];

    i = entry_idx_s;
    for (i = entry_idx_s; i < entry_idx_e; i++) {
        if (FIELD_EX32(s->regs.entry[i].cfg_reg, ENTRY_CFG, A) ==
            IOPMP_AMATCH_OFF) {
            continue;
        }
        if (start_addr >= s->entry_addr[i].sa &&
            start_addr <= s->entry_addr[i].ea) {
            /* Check end address */
            if (end_addr >= s->entry_addr[i].sa &&
                end_addr <= s->entry_addr[i].ea) {
                *entry_idx = i;
                return IOPMP_ENTRY_HIT;
            } else {
                *entry_idx = i;
                return IOPMP_ENTRY_PAR_HIT;
            }
        } else if (end_addr >= s->entry_addr[i].sa &&
                   end_addr <= s->entry_addr[i].ea) {
            /* Only end address matches the entry */
            *entry_idx = i;
            return IOPMP_ENTRY_PAR_HIT;
        } else if (start_addr < s->entry_addr[i].sa &&
                   end_addr > s->entry_addr[i].ea) {
            *entry_idx = i;
            return IOPMP_ENTRY_PAR_HIT;
        }
        if (priority_entry_in_tlb != NULL) {
            if ((s->entry_addr[i].sa >= tlb_sa &&
                 s->entry_addr[i].sa <= tlb_ea) ||
                (s->entry_addr[i].ea >= tlb_sa &&
                 s->entry_addr[i].ea <= tlb_ea)) {
                /*
                 * Higher priority entry in the TLB page, but it does not
                 * occupy the entire page.
                 */
                *priority_entry_in_tlb = 1;
            }
        }
    }
    return result;
}

static iopmp_entry_hit match_entry(Atciopmp200state *s, int rrid,
                                   hwaddr start_addr, hwaddr end_addr,
                                   int *match_entry_idx,
                                   int *priority_entry_in_tlb)
{
    iopmp_entry_hit cur_result = IOPMP_ENTRY_NO_HIT;
    iopmp_entry_hit result = IOPMP_ENTRY_NO_HIT;
    /* Remove lock bit */
    uint64_t srcmd_en = (uint64_t)s->regs.srcmd_en[rrid] >> 1;

    for (int md_idx = 0; md_idx < s->md_num; md_idx++) {
        if (srcmd_en & (1ULL << md_idx)) {
            cur_result = match_entry_md(s, md_idx, start_addr, end_addr,
                                        match_entry_idx, priority_entry_in_tlb);
            if (cur_result == IOPMP_ENTRY_HIT ||
                cur_result == IOPMP_ENTRY_PAR_HIT) {
                return cur_result;
            }
        }
    }
    return result;
}

static int atciopmp200_error_reaction(Atciopmp200state *s, uint32_t rrid,
                                      uint32_t eid, hwaddr addr, uint32_t etype,
                                      uint32_t ttype)
{
    uint32_t error_id = 0;
    uint32_t error_info = 0;
    int offset;
    /* interrupt enable regarding the access */
    int ie;
    /* bus error enable */
    int be;
    int error;
    if (etype >= ERR_REQINFO_ETYPE_READ && etype <= ERR_REQINFO_ETYPE_WRITE) {
        offset = etype - ERR_REQINFO_ETYPE_READ;
        ie = (extract32(s->regs.err_cfg, R_ERR_CFG_IRE_SHIFT + offset, 1) &&
              !extract32(s->regs.entry[eid].cfg_reg,
                         R_ENTRY_CFG_SIRE_SHIFT + offset, 1));
        be = (!extract32(s->regs.err_cfg, R_ERR_CFG_RRE_SHIFT + offset, 1) &&
              !extract32(s->regs.entry[eid].cfg_reg,
                         R_ENTRY_CFG_SERE_SHIFT + offset, 1));
    } else {
        offset = ttype - ERR_REQINFO_TTYPE_READ;
        ie = extract32(s->regs.err_cfg, R_ERR_CFG_IRE_SHIFT + offset, 1);
        be = !extract32(s->regs.err_cfg, R_ERR_CFG_RRE_SHIFT + offset, 1);
        if (etype != ERR_REQINFO_ETYPE_PARHIT) {
            eid = 0;
        }
    }
    error = ie | be;
    if (!FIELD_EX32(s->regs.err_reqinfo, ERR_REQINFO, IP)) {
        if (error) {
            /* Update error infomation if error is not suppressed */
            error_id = FIELD_DP32(error_id, ERR_REQID, EID, eid);
            error_id = FIELD_DP32(error_id, ERR_REQID, RRID, rrid);
            error_info = FIELD_DP32(error_info, ERR_REQINFO, ETYPE,
                                    etype);
            error_info = FIELD_DP32(error_info, ERR_REQINFO, TTYPE,
                                    ttype);
            s->regs.err_reqinfo = error_info;
            s->regs.err_reqinfo = FIELD_DP32(s->regs.err_reqinfo, ERR_REQINFO,
                                             IP, 1);
            s->regs.err_reqid = error_id;
            /* addr[LEN+2:2] */
            s->regs.err_reqaddr = addr >> 2;
        }
        /* Check global interrupt enable */
        if (ie && FIELD_EX32(s->regs.err_cfg, ERR_CFG, IE)) {
            qemu_set_irq(s->irq, 1);
        }
    }
    return be;
}

static inline IOMMUAccessFlags entry_cfg_iommu_access_flags(uint32_t cfg)
{
    /* X bit is not used in IOMMUAccessFlags */
    return cfg & IOMMU_RW;
}

static IOMMUTLBEntry atciopmp200_translate(IOMMUMemoryRegion *iommu,
                                           hwaddr addr,
                                           IOMMUAccessFlags flags,
                                           int iommu_idx)
{
    int rrid = iommu_idx;
    Atciopmp200state *s = ATCIOPMP200(container_of(iommu, Atciopmp200state,
                                                   iommu));
    hwaddr start_addr, end_addr;
    int entry_idx = -1;
    iopmp_entry_hit result;
    int priority_entry_in_tlb = 0;
    iopmp_permission iopmp_perm;
    bool lock = false;
    IOMMUTLBEntry entry = {
        .target_as = &s->downstream_as,
        .iova = addr,
        .translated_addr = addr,
        .addr_mask = 0,
        .perm = IOMMU_NONE,
    };

    if (!s->enable) {
        /* Bypass IOPMP */
        entry.addr_mask = TARGET_PAGE_SIZE - 1,
        entry.perm = IOMMU_RW;
        return entry;
    }

    /* unknown RRID */
    if (rrid >= s->rrid_num) {
        entry.target_as = &s->blocked_rwx_as;
        entry.perm = IOMMU_RW;
        return entry;
    }

    if (s->regs.mdstall) {
        if (bql_locked()) {
            bql_unlock();
            lock = true;
        }
        while (s->regs.mdstall) {
            ;
        }
        if (lock) {
            bql_lock();
        }
    }

    if (s->transaction_state[rrid].running == true) {
        start_addr = s->transaction_state[rrid].start_addr;
        end_addr = s->transaction_state[rrid].end_addr;
    } else {
        /* No transaction information, use the same address */
        start_addr = addr;
        end_addr = addr;
    }

    result = match_entry(s, rrid, start_addr, end_addr, &entry_idx,
                         &priority_entry_in_tlb);
    if (result == IOPMP_ENTRY_HIT) {
        entry.addr_mask = s->entry_addr[entry_idx].ea -
                          s->entry_addr[entry_idx].sa;
        if (entry.addr_mask > TARGET_PAGE_SIZE - 1) {
            entry.addr_mask = TARGET_PAGE_SIZE - 1;
        }
        if (priority_entry_in_tlb) {
            /*
             * Because there are entries in the same TLB, it is necessary to
             * check which entry the transaction hits on each access.
             */
            entry.addr_mask = 0;
        }
        iopmp_perm = s->regs.entry[entry_idx].cfg_reg & IOPMP_RWX;
        if (flags) {
            if ((entry_cfg_iommu_access_flags(iopmp_perm) & flags) == 0) {
                entry.target_as = &s->blocked_rw_as;
                entry.perm = IOMMU_RW;
            } else {
                entry.perm = entry_cfg_iommu_access_flags(iopmp_perm);
            }
        } else {
            /* CPU access with IOMMU_NONE flag */
            if (iopmp_perm & IOPMP_XO) {
                if ((iopmp_perm & IOPMP_RW) == IOPMP_RW) {
                    entry.target_as = &s->downstream_as;
                } else if ((iopmp_perm & IOPMP_RW) == IOPMP_RO) {
                    entry.target_as = &s->blocked_w_as;
                } else if ((iopmp_perm & IOPMP_RW) == IOPMP_WO) {
                    entry.target_as = &s->blocked_r_as;
                } else {
                    entry.target_as = &s->blocked_rw_as;
                }
            } else {
                if ((iopmp_perm & IOPMP_RW) == IOMMU_RW) {
                    entry.target_as = &s->blocked_x_as;
                } else if ((iopmp_perm & IOPMP_RW) == IOPMP_RO) {
                    entry.target_as = &s->blocked_wx_as;
                } else if ((iopmp_perm & IOPMP_RW) == IOPMP_WO) {
                    entry.target_as = &s->blocked_rx_as;
                } else {
                    entry.target_as = &s->blocked_rwx_as;
                }
            }
            entry.perm = IOMMU_RW;
        }
    } else {
         /* CPU access with IOMMU_NONE flag no_hit or par_hit*/
        entry.target_as = &s->blocked_rwx_as;
        entry.perm = IOMMU_RW;
    }
    return entry;
}

static const MemoryRegionOps atciopmp200_ops = {
    .read_with_attrs = atciopmp200_read,
    .write_with_attrs = atciopmp200_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 4, .max_access_size = 4}
};

static MemTxResult atciopmp200_permssion_write(void *opaque, hwaddr addr,
                                               uint64_t value, unsigned size,
                                               MemTxAttrs attrs)
{
    Atciopmp200state *s = ATCIOPMP200(opaque);
    return address_space_write(&s->downstream_as, addr, attrs, &value, size);
}

static MemTxResult atciopmp200_permssion_read(void *opaque, hwaddr addr,
                                             uint64_t *pdata, unsigned size,
                                             MemTxAttrs attrs)
{
    Atciopmp200state *s = ATCIOPMP200(opaque);
    return address_space_read(&s->downstream_as, addr, attrs, pdata, size);
}

static MemTxResult atciopmp200_handle_block(void *opaque, hwaddr addr,
                                            uint64_t *data, unsigned size,
                                            MemTxAttrs attrs,
                                            iopmp_access_type access_type)
{
    Atciopmp200state *s = ATCIOPMP200(opaque);
    int entry_idx = -1;
    int rrid = attrs.requester_id;
    int result;
    hwaddr start_addr, end_addr;
    iopmp_err_reqinfo_etype etype;
    iopmp_err_reqinfo_ttype ttype;
    ttype = (iopmp_err_reqinfo_ttype)access_type;
    int be;
    if (rrid > s->rrid_num) {
        etype = ERR_REQINFO_ETYPE_RRID;
        be = atciopmp200_error_reaction(s, rrid, 0, addr, etype, ttype);
        if (be) {
            return MEMTX_ERROR;
        } else {
            if (data) {
                *data = s->err_rdata;
            }
            return MEMTX_OK;
        }
    }
    if (s->transaction_state[rrid].running == true) {
        start_addr = s->transaction_state[rrid].start_addr;
        end_addr = s->transaction_state[rrid].end_addr;
    } else {
        /* No transaction information, use the same address */
        start_addr = addr;
        end_addr = addr;
    }

    result = match_entry(s, rrid, start_addr, end_addr, &entry_idx, NULL);

    if (result == IOPMP_ENTRY_HIT) {
        etype = (iopmp_err_reqinfo_etype)access_type;
    } else if (result == IOPMP_ENTRY_PAR_HIT) {
        etype = ERR_REQINFO_ETYPE_PARHIT;
    } else {
        etype = ERR_REQINFO_ETYPE_NOHIT;
    }
    be = atciopmp200_error_reaction(s, rrid, entry_idx, addr, etype, ttype);
    if (be) {
        return MEMTX_ERROR;
    } else {
        if (data) {
            *data = s->err_rdata;
        }
        return MEMTX_OK;
    }
}

static MemTxResult atciopmp200_block_write(void *opaque, hwaddr addr,
                                           uint64_t value, unsigned size,
                                           MemTxAttrs attrs)
{
    return atciopmp200_handle_block(opaque, addr, NULL, size, attrs,
                                    IOPMP_ACCESS_WRITE);
}

static MemTxResult atciopmp200_block_read(void *opaque, hwaddr addr,
                                          uint64_t *pdata, unsigned size,
                                          MemTxAttrs attrs)
{
    return atciopmp200_handle_block(opaque, addr, pdata, size, attrs,
                                    IOPMP_ACCESS_READ);
}

static MemTxResult atciopmp200_block_fetch(void *opaque, hwaddr addr,
                                           uint64_t *pdata, unsigned size,
                                           MemTxAttrs attrs)
{
    return atciopmp200_handle_block(opaque, addr, pdata, size, attrs,
                                    IOPMP_ACCESS_FETCH);
}

static const MemoryRegionOps atciopmp200_block_rw_ops = {
    .fetch_with_attrs = atciopmp200_permssion_read,
    .read_with_attrs = atciopmp200_block_read,
    .write_with_attrs = atciopmp200_block_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_w_ops = {
    .fetch_with_attrs = atciopmp200_permssion_read,
    .read_with_attrs = atciopmp200_permssion_read,
    .write_with_attrs = atciopmp200_block_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_r_ops = {
    .fetch_with_attrs = atciopmp200_permssion_read,
    .read_with_attrs = atciopmp200_block_read,
    .write_with_attrs = atciopmp200_permssion_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_rwx_ops = {
    .fetch_with_attrs = atciopmp200_block_fetch,
    .read_with_attrs = atciopmp200_block_read,
    .write_with_attrs = atciopmp200_block_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_wx_ops = {
    .fetch_with_attrs = atciopmp200_block_fetch,
    .read_with_attrs = atciopmp200_permssion_read,
    .write_with_attrs = atciopmp200_block_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_rx_ops = {
    .fetch_with_attrs = atciopmp200_block_fetch,
    .read_with_attrs = atciopmp200_block_read,
    .write_with_attrs = atciopmp200_permssion_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static const MemoryRegionOps atciopmp200_block_x_ops = {
    .fetch_with_attrs = atciopmp200_block_fetch,
    .read_with_attrs = atciopmp200_permssion_read,
    .write_with_attrs = atciopmp200_permssion_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 1, .max_access_size = 8},
};

static void atciopmp200_realize(DeviceState *dev, Error **errp)
{
    Object *obj = OBJECT(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Atciopmp200state *s = ATCIOPMP200(dev);
    uint64_t size;
    /* For atciopmp200 entry_offset is fixed 0x2000 */
    s->entry_offset = 0x2000;
    /* MDCFG has only one entry if the IOPMP model is rapid-k */
    s->regs.mdcfg = g_malloc0(1 * sizeof(uint32_t));

    /* Setting the default value of ATCIOPMP200 RAPID-K model*/
    s->regs.mdcfglck = FIELD_DP32(s->regs.mdcfglck, MDCFGLCK, F, 1);
    s->regs.mdcfglck = FIELD_DP32(s->regs.mdcfglck, MDCFGLCK, L, 1);
    s->k = MIN(s->k, ATCIOPMP200_MAX_K);
    s->rrid_num = MIN(s->rrid_num, ATCIOPMP200_MAX_RRID_NUM);
    s->md_num = MIN(s->md_num, ATCIOPMP200_MAX_MD_NUM);
    s->entry_num = s->md_num * s->k;
    s->regs.mdcfg[0] = s->k;

    s->regs.srcmd_en = g_malloc0(s->rrid_num * sizeof(uint32_t));
    s->regs.entry = g_malloc0(s->entry_num * sizeof(iopmp200_entry_t));
    s->entry_addr = g_malloc0(s->entry_num * sizeof(iopmp_addr_t));
    s->transaction_state = g_malloc0(s->rrid_num *
                                     sizeof(iopmp_transaction_state));
    qemu_mutex_init(&s->iopmp_transaction_mutex);

    s->downstream = get_system_memory();
    size = memory_region_size(s->downstream);
    address_space_init(&s->downstream_as, s->downstream,
                       "iopmp-downstream-as");

    memory_region_init_iommu(&s->iommu, sizeof(s->iommu),
                             TYPE_IOPMP200_IOMMU_MEMORY_REGION,
                             obj, "riscv-iopmp-sysbus-iommu", UINT64_MAX);
    address_space_init(&s->iopmp_sysbus_as, MEMORY_REGION(&s->iommu), "iommu");
    memory_region_init_io(&s->mmio, obj, &atciopmp200_ops,
                          s, "iopmp-regs", 0x4000);
    sysbus_init_mmio(sbd, &s->mmio);

    memory_region_init_io(&s->blocked_rw, NULL, &atciopmp200_block_rw_ops,
                          s, "iopmp-blocked-rw", size);
    memory_region_init_io(&s->blocked_w, NULL, &atciopmp200_block_w_ops,
                          s, "iopmp-blocked-w", size);
    memory_region_init_io(&s->blocked_r, NULL, &atciopmp200_block_r_ops,
                          s, "iopmp-blocked-r", size);

    memory_region_init_io(&s->blocked_rwx, NULL, &atciopmp200_block_rwx_ops,
                          s, "iopmp-blocked-rwx", size);
    memory_region_init_io(&s->blocked_wx, NULL, &atciopmp200_block_wx_ops,
                          s, "iopmp-blocked-wx", size);
    memory_region_init_io(&s->blocked_rx, NULL, &atciopmp200_block_rx_ops,
                          s, "iopmp-blocked-rx", size);
    memory_region_init_io(&s->blocked_x, NULL, &atciopmp200_block_x_ops,
                          s, "iopmp-blocked-x", size);
    address_space_init(&s->blocked_rw_as, &s->blocked_rw,
                       "iopmp-blocked-rw-as");
    address_space_init(&s->blocked_w_as, &s->blocked_w,
                       "iopmp-blocked-w-as");
    address_space_init(&s->blocked_r_as, &s->blocked_r,
                       "iopmp-blocked-r-as");

    address_space_init(&s->blocked_rwx_as, &s->blocked_rwx,
                       "iopmp-blocked-rwx-as");
    address_space_init(&s->blocked_wx_as, &s->blocked_wx,
                       "iopmp-blocked-wx-as");
    address_space_init(&s->blocked_rx_as, &s->blocked_rx,
                       "iopmp-blocked-rx-as");
    address_space_init(&s->blocked_x_as, &s->blocked_x,
                       "iopmp-blocked-x-as");

    object_initialize_child(OBJECT(s), "iopmp_transaction_info",
                        &s->transaction_info_sink,
                        TYPE_IOPMP200_TRANSACTION_INFO_SINK);
}

static void atciopmp200_reset(DeviceState *dev)
{
    Atciopmp200state *s = ATCIOPMP200(dev);

    qemu_set_irq(s->irq, 0);
    memset(s->regs.srcmd_en, 0, s->rrid_num * sizeof(uint32_t));
    memset(s->entry_addr, 0, s->entry_num * sizeof(iopmp_addr_t));

    s->regs.mdlck = 0;
    s->regs.entrylck = 0;
    s->regs.mdstall = 0;
    s->regs.err_cfg = 0;
    s->regs.err_reqaddr = 0;
    s->regs.err_reqid = 0;
    s->regs.err_reqinfo = 0;

    s->enable = 0;
}

static int atciopmp200_attrs_to_index(IOMMUMemoryRegion *iommu,
                                      MemTxAttrs attrs)
{
    return attrs.requester_id;
}

static void atciopmp200_iommu_mr_class_init(ObjectClass *klass, void *data)
{
    IOMMUMemoryRegionClass *imrc = IOMMU_MEMORY_REGION_CLASS(klass);

    imrc->translate = atciopmp200_translate;
    imrc->attrs_to_index = atciopmp200_attrs_to_index;
}

static Property atciopmp200_property[] = {
    DEFINE_PROP_UINT32("k", Atciopmp200state, k, 6),
    DEFINE_PROP_UINT32("rrid_num", Atciopmp200state, rrid_num, 8),
    DEFINE_PROP_UINT32("md_num", Atciopmp200state, md_num, 8),
    DEFINE_PROP_UINT32("err_rdata", Atciopmp200state, err_rdata, 0x0),
    DEFINE_PROP_BOOL("peis", Atciopmp200state, peis, true),
    DEFINE_PROP_BOOL("pees", Atciopmp200state, pees, true),
    DEFINE_PROP_BOOL("tor_en", Atciopmp200state, tor_en, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void atciopmp200_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_props(dc, atciopmp200_property);
    dc->realize = atciopmp200_realize;
    device_class_set_legacy_reset(dc, atciopmp200_reset);
}

static void atciopmp200_init(Object *obj)
{
    Atciopmp200state *s = ATCIOPMP200(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbd, &s->irq);
}

static const TypeInfo atciopmp200_info = {
    .name = TYPE_ATCIOPMP200,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Atciopmp200state),
    .instance_init = atciopmp200_init,
    .class_init = atciopmp200_class_init,
};

static const TypeInfo
atciopmp200_iommu_memory_region_info = {
    .name = TYPE_IOPMP200_IOMMU_MEMORY_REGION,
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .class_init = atciopmp200_iommu_mr_class_init,
};

/*
 * Alias subregions from the source memory region to the destination memory
 * region
 */
static void alias_memory_subregions(MemoryRegion *src_mr, MemoryRegion *dst_mr,
                                    const MemMapEntry *memmap,
                                    uint32_t map_entry_num)
{
    int32_t priority;
    hwaddr addr;
    MemoryRegion *alias, *subregion;
    QTAILQ_FOREACH(subregion, &src_mr->subregions, subregions_link) {
        addr = subregion->addr;
        for (int i = 0; i < map_entry_num; i++) {
            if (addr >= memmap[i].base &&
                addr < memmap[i].base + memmap[i].size) {
                priority = subregion->priority;
                alias = g_malloc0(sizeof(MemoryRegion));
                memory_region_init_alias(alias, NULL, subregion->name,
                                         subregion, 0,
                                         memory_region_size(subregion));
                memory_region_add_subregion_overlap(dst_mr, addr, alias,
                                                    priority);
                break;
            }
        }
    }
}

void iopmp200_setup_system_memory(DeviceState *dev, const MemMapEntry *memmap,
                                  uint32_t map_entry_num)
{
    Atciopmp200state *s = ATCIOPMP200(dev);
    uint32_t i;
    MemoryRegion *iommu_alias;
    MemoryRegion *target_mr = get_system_memory();
    MemoryRegion *downstream = g_malloc0(sizeof(MemoryRegion));
    memory_region_init(downstream, NULL, "iopmp_downstream",
                       memory_region_size(target_mr));
    /* Create a downstream which does not have iommu of iopmp */
    alias_memory_subregions(target_mr, downstream, memmap, map_entry_num);

    for (i = 0; i < map_entry_num; i++) {
        /* Memory access to protected regions of target are through IOPMP */
        iommu_alias = g_new(MemoryRegion, 1);
        memory_region_init_alias(iommu_alias, NULL, "iommu_alias",
                                 MEMORY_REGION(&s->iommu), memmap[i].base,
                                 memmap[i].size);
        memory_region_add_subregion_overlap(target_mr, memmap[i].base,
                                            iommu_alias, 1);
    }
    s->downstream = downstream;
    address_space_init(&s->downstream_as, s->downstream,
                       "iopmp-downstream-as");
}

Iopmp_StreamSink *iopmp200_get_sink(DeviceState *dev)
{
    Atciopmp200state *s = ATCIOPMP200(dev);
    return &s->transaction_info_sink;
}

static size_t
transaction_info_push(StreamSink *transaction_info_sink, unsigned char *buf,
                      size_t len, bool eop)
{
    Iopmp_StreamSink *ss = IOPMP_TRANSACTION_INFO_SINK(transaction_info_sink);
    Atciopmp200state *s = ATCIOPMP200(container_of(ss, Atciopmp200state,
                                      transaction_info_sink));
    iopmp_transaction_info signal;
    uint32_t sid;

    memcpy(&signal, buf, len);
    sid = signal.sid;

    if (s->transaction_state[sid].running) {
        if (eop) {
            /* Finish the transaction */
            qemu_mutex_lock(&s->iopmp_transaction_mutex);
            s->transaction_state[sid].running = 0;
            qemu_mutex_unlock(&s->iopmp_transaction_mutex);
            return 1;
        } else {
            /* Transaction is already running */
            return 0;
        }
    } else if (len == sizeof(iopmp_transaction_info)) {
        /* Get the transaction info */
        s->transaction_state[sid].supported = 1;
        qemu_mutex_lock(&s->iopmp_transaction_mutex);
        s->transaction_state[sid].running = 1;
        qemu_mutex_unlock(&s->iopmp_transaction_mutex);

        s->transaction_state[sid].start_addr = signal.start_addr;
        s->transaction_state[sid].end_addr = signal.end_addr;
        s->transaction_state[sid].error_pending = 0;
        return 1;
    }
    return 0;
}

static void iopmp_transaction_info_sink_class_init(ObjectClass *klass,
                                                   void *data)
{
    StreamSinkClass *ssc = STREAM_SINK_CLASS(klass);
    ssc->push = transaction_info_push;
}

static const TypeInfo transaction_info_sink = {
    .name = TYPE_IOPMP200_TRANSACTION_INFO_SINK,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(Iopmp_StreamSink),
    .class_init = iopmp_transaction_info_sink_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_STREAM_SINK },
        { }
    },
};

static void
atciopmp200_register_types(void)
{
    type_register_static(&atciopmp200_info);
    type_register_static(&atciopmp200_iommu_memory_region_info);
    type_register_static(&transaction_info_sink);
}

DeviceState *atciopmp200_create(hwaddr addr, qemu_irq irq)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = qdev_new("atciopmp200");
    s = SYS_BUS_DEVICE(dev);
    sysbus_realize_and_unref(s, &error_fatal);
    sysbus_mmio_map(s, 0, addr);
    sysbus_connect_irq(s, 0, irq);
    return dev;
}

type_init(atciopmp200_register_types);
