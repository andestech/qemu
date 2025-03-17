/*
 * RISC-V Andes Extension Helpers for QEMU
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/host-utils.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"
#include "internals.h"
#include "vector_internals.h"

typedef int (*test_function)(uint8_t a, uint8_t b);

target_ulong helper_andes_v5_bfo_x(target_ulong rd, target_ulong rs1,
                                   target_ulong insn)
{
    int msb, lsb, is_se;
    int lsbp1, msbm1, lsbm1, lenm1;
    uint64_t se;
    uint64_t nxrd = rd; /* for safety sake */

    msb = extract64(insn, 26, 6);
    lsb = extract64(insn, 20, 6);
    is_se = 0x3 == (0x7 & (insn >> 12)); /* BFOS */
    lsbp1 = lsb + 1;
    msbm1 = msb - 1;
    lsbm1 = lsb - 1;

    if (msb == 0) {
        nxrd = deposit64(nxrd, lsb, 1, 1 & rs1);
        if (lsb > 0) {
            nxrd = deposit64(nxrd, 0, lsbm1 + 1, 0);
        }
        if (lsb < 63) {
            se = (is_se && (1 & rs1)) ? -1LL : 0;
            nxrd = deposit64(nxrd, lsbp1, 64 - lsbp1, se);
        }
    } else if (msb < lsb) {
        lenm1 = lsb - msb;
        nxrd = deposit64(nxrd, msb, lenm1 + 1, rs1 >> 0);
        if (lsb < 63) {
            se = (is_se && (1 & (rs1 >> lenm1))) ? -1LL : 0;
            nxrd = deposit64(nxrd, lsbp1, 64 - lsbp1, se);
        }
        nxrd = deposit64(nxrd, 0, msbm1 + 1, 0);
    } else { /* msb >= lsb */
        lenm1 = msb - lsb;
        nxrd = deposit64(nxrd, 0, lenm1 + 1, rs1 >> lsb);
        se = (is_se && (1 & (rs1 >> msb))) ? -1LL : 0;
        nxrd = deposit64(nxrd, lenm1 + 1, 63 - lenm1, se);
    }

    return (target_long)nxrd;
}

static int andes_v5_fb_x_internal(uint8_t *bytes1, uint8_t *bytes2, int size,
                             int is_little_endian, test_function test)
{
    int i, found;

    found = 0;

    if (is_little_endian) {
        for (i = 0; i < size; ++i) {
            if (test(bytes1[i], bytes2[i])) {
                found = i - size;
                break;
            }
        }
    } else { /* is_big_endian */
        for (i = size - 1; i >= 0; --i) {
            if (test(bytes1[i], bytes2[i])) {
                found = i - size;
                break;
            }
        }
    }

    return found;
}

static int andes_v5_test_match(uint8_t a, uint8_t b)
{
    return (a == b);
}

static int andes_v5_test_mismatch(uint8_t a, uint8_t b)
{
    return (a != b);
}

static int andes_v5_test_zero_mismatch(uint8_t a, uint8_t b)
{
    return (a == 0) || (a != b);
}

target_ulong helper_andes_v5_fb_x(target_ulong rs1, target_ulong rs2,
                                  target_ulong op)
{
    target_ulong rd;
    uint8_t *pa, *pb;
    unsigned int size;

    size = sizeof(target_ulong);
    pa = (uint8_t *)&rs1;
    pb = (uint8_t *)&rs2;
    rd = 0;

    switch (op) {
    case 0x10: /* FFB */
        /* Each byte in Rs1 is matched with the value in Rs2[7:0].  */
        memset(pb, *pb, size);
        rd = andes_v5_fb_x_internal(pa, pb, size, 1, andes_v5_test_match);
        break;
    case 0x11: /* FFZMISM */
        rd = andes_v5_fb_x_internal(pa, pb, size, 1,
                                    andes_v5_test_zero_mismatch);
        break;
    case 0x12: /* FFMISM */
        rd = andes_v5_fb_x_internal(pa, pb, size, 1, andes_v5_test_mismatch);
        break;
    case 0x13: /* FLMISM */
        /*
         * tricky!
         *   # reverse endian to find last
         *   # patch result
         */
        rd = andes_v5_fb_x_internal(pa, pb, size, 0, andes_v5_test_mismatch);
        break;
    default:
        /* helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST); */
        break;
    }

    return rd;
}

uint64_t helper_andes_nfcvt_bf16_s(CPURISCVState *env, uint64_t rs2)
{
    float32 frs = check_nanbox_s(env, rs2);
    return nanbox_h(env, float32_to_bfloat16(frs, &env->fp_status));
}

uint64_t helper_andes_nfcvt_s_bf16(CPURISCVState *env, uint64_t rs2)
{
    float16 frs = check_nanbox_h_bf16(env, rs2);
    return nanbox_s(env, bfloat16_to_float32(frs, &env->fp_status));
}

void helper_andes_v5_hsp_check(CPURISCVState *env, target_ulong val)
{
    if (csr_ops[CSR_MHSP_CTL].predicate(env, CSR_MHSP_CTL) == RISCV_EXCP_NONE) {
        target_ulong mhsp_ctl;
        csr_ops[CSR_MHSP_CTL].read(env, CSR_MHSP_CTL, &mhsp_ctl);

#ifdef CONFIG_USER_ONLY
        if ((mhsp_ctl & MASK_MHSP_CTL_U) == 0) {
            return;
        }
#else
        if ((env->priv == PRV_M && (mhsp_ctl & MASK_MHSP_CTL_M) == 0)
                || (env->priv == PRV_S && (mhsp_ctl & MASK_MHSP_CTL_S) == 0)
                || (env->priv == PRV_U && (mhsp_ctl & MASK_MHSP_CTL_U) == 0)) {
            return;
        }
#endif

        target_ulong msp_base;
        target_ulong msp_bound;
        csr_ops[CSR_MSP_BASE].read(env, CSR_MSP_BASE, &msp_base);
        csr_ops[CSR_MSP_BOUND].read(env, CSR_MSP_BOUND, &msp_bound);

        if ((mhsp_ctl & MASK_MHSP_CTL_SCHM) != 0) {
            if ((mhsp_ctl & MASK_MHSP_CTL_OVF_EN) != 0) {
                /* recording mode */
                if (val < msp_bound) {
                    csr_ops[CSR_MSP_BOUND].write(env, CSR_MSP_BOUND, val);
                }
                return;
            }
        } else {
            if ((mhsp_ctl & MASK_MHSP_CTL_OVF_EN) != 0) {
                /* overflow mode */
                if (val < msp_bound) {
                    mhsp_ctl = set_field(mhsp_ctl,
                        MASK_MHSP_CTL_OVF_EN | MASK_MHSP_CTL_UDF_EN, 0);
                    csr_ops[CSR_MHSP_CTL].write(env, CSR_MHSP_CTL, mhsp_ctl);
                    riscv_raise_exception(env, RISCV_EXCP_ANDES_STACK_OVERFLOW,
                                          GETPC());
                }
            }
            if ((mhsp_ctl & MASK_MHSP_CTL_UDF_EN) != 0) {
                /* underflow mode */
                if (val > msp_base) {
                    mhsp_ctl = set_field(mhsp_ctl,
                        MASK_MHSP_CTL_OVF_EN | MASK_MHSP_CTL_UDF_EN, 0);
                    csr_ops[CSR_MHSP_CTL].write(env, CSR_MHSP_CTL, mhsp_ctl);
                    riscv_raise_exception(env, RISCV_EXCP_ANDES_STACK_UNDERFLOW,
                                          GETPC());
                }
            }
        }
    }
    return;
}

#include "andes_ace_helper.h"
void helper_andes_ace(CPURISCVState *env, target_ulong opcode)
{
    /* Save current function ra for TCG TB lookup when run ACE insn. */
    env->ace_ra = GETPC();
    int ret = qemu_ace_agent_run_insn(env, opcode);
    if (ret != 0) {
        /* wrong ACE instruction seems return RESERVED_INSN(=1), not ILL Insn */
        qemu_printf("Run ace instruction result = %d\n", ret);
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }
}

#define GEN_ANDES_AMM_ARITHMETIC_HELPER(NAME, VS1_T, VS1_W_T, VS2_T, VS2_W_T, VD_T) \
void HELPER(NAME)(void *vd, void *vs1, void *vs2,                                   \
                  CPURISCVState *env, uint32_t desc)                                \
{                                                                                   \
    uint32_t vlenb = simd_maxsz(desc);                                              \
    uint32_t LMUL = 1 << vext_lmul(desc);                                           \
    uint32_t M = 2;                                                                 \
    uint32_t K = 8;                                                                 \
    uint32_t N = vlenb / 8;                                                         \
    uint32_t active_m, active_k, active_n;                                          \
                                                                                    \
    if (LMUL == 1) {                                                                \
        target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];                   \
        active_m = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_M);                       \
        active_k = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);                       \
        active_n = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);                       \
    } else {                                                                        \
        active_m = M;                                                               \
        active_k = K;                                                               \
        active_n = N;                                                               \
    }                                                                               \
                                                                                    \
    for (uint32_t g = 0; g < LMUL; g++) {                                           \
        for (uint32_t m = 0; m < active_m; m++) {                                   \
            for (uint32_t n = 0; n < active_n; n++) {                               \
                for (uint32_t k = 0; k < active_k; k++) {                           \
                    VS1_W_T vs1_w = *(((VS1_T *)vs1) + (g * N * K + m * K + k));    \
                    VS2_W_T vs2_w = *(((VS2_T *)vs2) + (g * N * K + n * K + k));    \
                    VD_T *vd_w = ((VD_T *)vd) + (m * N + n);                        \
                    *vd_w += vs1_w * vs2_w;                                         \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    }                                                                               \
}                                                                                   \

GEN_ANDES_AMM_ARITHMETIC_HELPER(andes_vqammuu_vv,
                                uint8_t, uint32_t, uint8_t, uint32_t, uint32_t)
GEN_ANDES_AMM_ARITHMETIC_HELPER(andes_vqammus_vv,
                                uint8_t, uint32_t, int8_t, int32_t, int32_t)
GEN_ANDES_AMM_ARITHMETIC_HELPER(andes_vqammsu_vv,
                                int8_t, int32_t, uint8_t, uint32_t, int32_t)
GEN_ANDES_AMM_ARITHMETIC_HELPER(andes_vqammss_vv,
                                int8_t, int32_t, int8_t, int32_t, int32_t)

#define GEN_VEXT_LD_ELEM(NAME, ETYPE, H, LDSUF)             \
static inline QEMU_ALWAYS_INLINE                            \
void NAME##_tlb(CPURISCVState *env, abi_ptr addr,           \
                uint32_t idx, void *vd, uintptr_t retaddr)  \
{                                                           \
    ETYPE *cur = ((ETYPE *)vd + H(idx));                    \
    *cur = cpu_##LDSUF##_data_ra(env, addr, retaddr);       \
}
GEN_VEXT_LD_ELEM(lde_b, uint8_t,  H1, ldub)
GEN_VEXT_LD_ELEM(lde_w, uint32_t, H4, ldl)
                                                           \
#define GEN_VEXT_ST_ELEM(NAME, ETYPE, H, STSUF)             \
static inline QEMU_ALWAYS_INLINE                            \
void NAME##_tlb(CPURISCVState *env, abi_ptr addr,           \
                uint32_t idx, void *vd, uintptr_t retaddr)  \
{                                                           \
    ETYPE data = *((ETYPE *)vd + H(idx));                   \
    cpu_##STSUF##_data_ra(env, addr, data, retaddr);        \
}
GEN_VEXT_ST_ELEM(ste_b, uint8_t,  H1, stb)
GEN_VEXT_ST_ELEM(ste_w, uint32_t, H4, stl)

void HELPER(andes_vle8_mk)(void *vd, target_ulong rs1, target_ulong rs2,
                           CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t M = 2;
    uint32_t K = 8;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_M = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_M);
    uint32_t active_K = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t m = (i % velements) / K;
        uint32_t k = (i % velements) % K;
        bool is_active = false;

        if ((LMUL > 1 && m < M && k < K) ||
            (LMUL == 1 && m < active_M && k < active_K)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + m * rs2 + (g * K + k) * SEW / 8;
            lde_b_tlb(env, adjust_addr(env, addr), i, vd, GETPC());
        } else {
            /* TODO: vd[i] = get_inactive_value(vtype.vta, vtype.vma, vd[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vle8_nk)(void *vd, target_ulong rs1, target_ulong rs2,
                           CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t N = VLEN / 64;
    uint32_t K = 8;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);
    uint32_t active_K = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t n = (i % velements) / K;
        uint32_t k = (i % velements) % K;
        bool is_active = false;

        if ((LMUL > 1 && n < N && k < K) ||
            (LMUL == 1 && n < active_N && k < active_K)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + n * rs2 + (g * K + k) * SEW / 8;
            lde_b_tlb(env, adjust_addr(env, addr), i, vd, GETPC());
        } else {
            /* TODO: vd[i] = get_inactive_value(vtype.vta, vtype.vma, vd[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vle8_kn)(void *vd, target_ulong rs1, target_ulong rs2,
                           CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t N = VLEN / 64;
    uint32_t K = 8;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);
    uint32_t active_K = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t n = (i % velements) / K;
        uint32_t k = (i % velements) % K;
        bool is_active = false;

        if ((LMUL > 1 && n < N && k < K) ||
            (LMUL == 1 && n < active_N && k < active_K)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + (g * K + k) * rs2 + n * SEW / 8;
            lde_b_tlb(env, adjust_addr(env, addr), i, vd, GETPC());
        } else {
            /* TODO: vd[i] = get_inactive_value(vtype.vta, vtype.vma, vd[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vle32_mn)(void *vd, target_ulong rs1, target_ulong rs2,
                            CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t M = 2;
    uint32_t N = VLEN / 64;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_M = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_M);
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t m = (i % velements) / N;
        uint32_t n = (i % velements) % N;
        bool is_active = false;

        if ((LMUL > 1 && m < M && n < N) ||
            (LMUL == 1 && m < active_M && n < active_N)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + (g * M + m) * rs2 + n * SEW / 8;
            lde_w_tlb(env, adjust_addr(env, addr), i, vd, GETPC());
        } else {
            /* TODO: vd[i] = get_inactive_value(vtype.vta, vtype.vma, vd[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vse8_nk)(void *vs, target_ulong rs1, target_ulong rs2,
                           CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t N = VLEN / 64;
    uint32_t K = 8;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);
    uint32_t active_K = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t n = (i % velements) / K;
        uint32_t k = (i % velements) % K;
        bool is_active = false;

        if ((LMUL > 1 && n < N && k < K) ||
            (LMUL == 1 && n < active_N && k < active_K)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + n * rs2 + (g * K + k) * SEW / 8;
            ste_b_tlb(env, adjust_addr(env, addr), i, vs, GETPC());
        } else {
            /* TODO: vs[i] = get_inactive_value(vtype.vta, vtype.vma, vs[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vse8_kn)(void *vs, target_ulong rs1, target_ulong rs2,
                           CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t N = VLEN / 64;
    uint32_t K = 8;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);
    uint32_t active_K = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_K);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t n = (i % velements) / K;
        uint32_t k = (i % velements) % K;
        bool is_active = false;

        if ((LMUL > 1 && n < N && k < K) ||
            (LMUL == 1 && n < active_N && k < active_K)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + (g * K + k) * rs2 + n * SEW / 8;
            ste_b_tlb(env, adjust_addr(env, addr), i, vs, GETPC());
        } else {
            /* TODO: vs[i] = get_inactive_value(vtype.vta, vtype.vma, vs[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vse32_nm)(void *vs, target_ulong rs1, target_ulong rs2,
                            CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t M = 2;
    uint32_t N = VLEN / 64;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_M = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_M);
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t m = (i % velements) / N;
        uint32_t n = (i % velements) % N;
        bool is_active = false;

        if ((LMUL > 1 && n < N && m < M) ||
            (LMUL == 1 && n < active_N && m < active_M)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + n * rs2 + (g * M + m) * SEW / 8;
            ste_w_tlb(env, adjust_addr(env, addr), i, vs, GETPC());
        } else {
            /* TODO: vs[i] = get_inactive_value(vtype.vta, vtype.vma, vs[i]); */
        }
    }
    env->vstart = 0;
}

void HELPER(andes_vse32_mn)(void *vs, target_ulong rs1, target_ulong rs2,
                            CPURISCVState *env, uint32_t desc)
{
    uint32_t VLEN = simd_maxsz(desc) * 8;
    uint32_t LMUL = 1 << vext_lmul(desc);
    uint32_t SEW = 1 << (FIELD_EX64(env->vtype, VTYPE, VSEW) + 3);
    uint32_t velements = VLEN / SEW;

    uint32_t M = 2;
    uint32_t N = VLEN / 64;
    target_ulong uzobctl = env->andes_csr.csrno[CSR_UZOBCTL];
    uint32_t active_M = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_M);
    uint32_t active_N = get_field(uzobctl, MASK_UZOBCTL_ACTIVE_N);

    for (target_ulong i = env->vstart; i < LMUL * velements; i++) {
        uint32_t g = i / velements;
        uint32_t m = (i % velements) / N;
        uint32_t n = (i % velements) % N;
        bool is_active = false;

        if ((LMUL > 1 && n < N && m < M) ||
            (LMUL == 1 && n < active_N && m < active_M)) {
            is_active = true;
        }

        if (is_active) {
            target_ulong addr = rs1 + (g * M + m) * rs2 + n * SEW / 8;
            ste_w_tlb(env, adjust_addr(env, addr), i, vs, GETPC());
        } else {
            /* TODO: vs[i] = get_inactive_value(vtype.vta, vtype.vma, vs[i]); */
        }
    }
    env->vstart = 0;
}
