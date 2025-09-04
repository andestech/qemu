/*
 * RISC-V translation routines for Andes V5 instructions
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "instmap.h"
#define GP 3

#define REQUIRE_CFG(CSRNO, CSRFIELD) do {\
    RISCVException ret; \
    target_ulong value; \
    riscv_csr_operations ops; \
    riscv_get_csr_ops(CSRNO, &ops); \
    if (ops.read == NULL) { \
        return false; \
    } \
    ret = ops.read(cpu_env(ctx->cs), CSRNO, &value); \
    if (ret != RISCV_EXCP_NONE) \
        return false; \
    if (!(value & CSRFIELD)) \
        return false; \
} while (0);

#define REQUIRE_BF16CVT do { \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_BF16CVT) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_BF16CVT) \
    } \
} while (0)

#define MASK_MVEC_CFG_MFSEW (0x3 << 16)
#define REQUIRE_MFSEW REQUIRE_CFG(CSR_MVEC_CFG, MASK_MVEC_CFG_MFSEW)

#define REQUIRE_VPFH do { \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_VPFH) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_VPFH) \
    } \
} while (0)

#define REQUIRE_VL4 do { \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_VL4) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_VL4) \
    } \
} while (0)

#define REQUIRE_VDOT do { \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_VDOT) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_VDOT) \
    } \
} while (0)

#define REQUIRE_VSIH do { \
    if (cpu_env(ctx->cs)->misa_mxl == MXL_RV32) { \
        REQUIRE_CFG(CSR_MMSC_CFG2, MASK_MMSC_CFG2_VSIH) \
    } else { \
        REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_VSIH) \
    } \
} while (0)

enum andes_v5_inst_id {
    /* Code Dense Extension */
    EXEC_IT = (0x8000),

    /* V5 Performance Extension */
    /* custom 0 */
    ADDIGP = 0x01,
    LBGP = 0x00,
    LBUGP = 0x02,
    SBGP = 0x03,

    /* custom 1 */
    LHGP = 0x01,
    LHUGP = 0x05,
    LWGP = 0x02,
    LWUGP = 0x06,
    LDGP = 0x03,
    SHGP = 0x00,
    SWGP = 0x04,
    SDGP = 0x07,

    /* custom 2 */
    BBX = 0x07,
    BBX_BBC = 0x00,
    BBX_BBS = 0x01,
    BEQC = 0x05,
    BNEC = 0x06,
    BFOS = 0x03,
    BFOZ = 0x02,
    LEAF = 0x00,
    LEA_H = 0x05,
    LEA_W = 0x06,
    LEA_D = 0x07,
    LEA_B_ZE = 0x08,
    LEA_H_ZE = 0x09,
    LEA_W_ZE = 0x0a,
    LEA_D_ZE = 0x0b,
    F_FFB = 0x10,
    F_FFZMISM = 0x11,
    F_FFMISM = 0x12,
    F_FLMISM = 0x13,

    /* custom 3 */
    /* empty by now */
};

/*** Custom-2 Instructions ***/
static void andes_v5_gen_branch_tcgv(DisasContext *ctx, uint32_t opc,
                            TCGv source1, TCGv source2, target_long imm)
{
    TCGLabel *label = gen_new_label();
    target_ulong orig_pc_save = ctx->pc_save;

    switch (opc) {
    case OPC_RISC_BEQ:
        tcg_gen_brcond_tl(TCG_COND_EQ, source1, source2, label);
        break;
    case OPC_RISC_BNE:
        tcg_gen_brcond_tl(TCG_COND_NE, source1, source2, label);
        break;
    case OPC_RISC_BLT:
        tcg_gen_brcond_tl(TCG_COND_LT, source1, source2, label);
        break;
    case OPC_RISC_BGE:
        tcg_gen_brcond_tl(TCG_COND_GE, source1, source2, label);
        break;
    case OPC_RISC_BLTU:
        tcg_gen_brcond_tl(TCG_COND_LTU, source1, source2, label);
        break;
    case OPC_RISC_BGEU:
        tcg_gen_brcond_tl(TCG_COND_GEU, source1, source2, label);
        break;
    default:
        gen_exception_illegal(ctx);
        return;
    }

    gen_goto_tb(ctx, 1, ctx->cur_insn_len);
    ctx->pc_save = orig_pc_save;

    gen_set_label(label); /* branch taken */
    if (!has_ext(ctx, RVC) && !ctx->cfg_ptr->ext_zca && (imm & 0x3)) {
        /* misaligned */
        TCGv target_pc = tcg_temp_new();
        gen_pc_plus_diff(target_pc, ctx, imm);
        gen_exception_inst_addr_mis(ctx, target_pc);
    } else {
        gen_goto_tb(ctx, 0, imm);
    }

    ctx->pc_save = -1;
    ctx->base.is_jmp = DISAS_NORETURN;
}

static bool trans_bbx_bbc(DisasContext *ctx, arg_bbx_bbc *a)
{
    TCGv v0, v1, src;

    /* XXX: make it use tcg_constant_* instead */
    v0 = tcg_constant_tl(0);
    v1 = tcg_temp_new();
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    tcg_gen_andi_tl(v1, src, (target_ulong)1u << a->cimms);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BEQ, v0, v1, a->imm);
    return true;
}

static bool trans_bbx_bbs(DisasContext *ctx, arg_bbx_bbs *a)
{
    TCGv v0, v1, src;
    v0 = tcg_constant_tl(0);
    v1 = tcg_temp_new();
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    tcg_gen_andi_tl(v1, src, (target_ulong)1u << a->cimms);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BNE, v0, v1, a->imm);
    return true;
}

static bool trans_beqc(DisasContext *ctx, arg_beqc *a)
{
    TCGv v0, src;
    v0 = tcg_constant_tl(a->cimml);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BEQ, v0, src, a->imm);
    return true;
}

static bool trans_bnec(DisasContext *ctx, arg_bnec *a)
{
    TCGv v0, src;
    v0 = tcg_constant_tl(a->cimml);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    andes_v5_gen_branch_tcgv(ctx, OPC_RISC_BNE, v0, src, a->imm);
    return true;
}

static bool trans_bfos(DisasContext *ctx, arg_bfos *a)
{
    TCGv src, v1, opc;
    opc = tcg_constant_tl(ctx->opcode);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    v1 = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_andes_v5_bfo_x(v1, v1, src, opc);
    return true;
}

static bool trans_bfoz(DisasContext *ctx, arg_bfoz *a)
{
    TCGv src, v1, opc;
    opc = tcg_constant_tl(ctx->opcode);
    src = get_gpr(ctx, a->rs1, EXT_NONE);
    v1 = get_gpr(ctx, a->rd,  EXT_NONE);
    gen_helper_andes_v5_bfo_x(v1, v1, src, opc);
    return true;
}

static bool translate_andes_lea(DisasContext *ctx, int rd,
                                int rs1, int rs2, int func)
{
    TCGv src1, src2, temp;

    temp = tcg_temp_new();
    src1 = get_gpr(ctx, rs1, EXT_NONE);
    src2 = get_gpr(ctx, rs2, EXT_NONE);

    switch (func) {
    case LEA_H: /* lea.h rd, rs1, rs2 */
    case LEA_W: /* lea.w rd, rs1, rs2 */
    case LEA_D: /* lea.d rd, rs1, rs2 */
        tcg_gen_shli_tl(temp, src2, func - LEA_H + 1);
        tcg_gen_add_tl(temp, src1, temp);
        break;
#if defined(TARGET_RISCV64)
    case LEA_B_ZE: /* lea.b.ze rd, rs1, rs2 */
    case LEA_H_ZE: /* lea.h.ze rd, rs1, rs2 */
    case LEA_W_ZE: /* lea.w.ze rd, rs1, rs2 */
    case LEA_D_ZE: /* lea.d.ze rd, rs1, rs2 */
        tcg_gen_ext32u_tl(temp, src2);
        tcg_gen_shli_tl(temp, temp, func - LEA_B_ZE);
        tcg_gen_add_tl(temp, src1, temp);
        break;
#endif
    default:
        /*
         * Seems to be an opcode collision,
         * fall through for official decoder
         */
        return false;
    }
    gen_set_gpr(ctx, rd, temp);

    return true;
}
static bool trans_lea_h(DisasContext *ctx, arg_lea_h *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_H);
}
static bool trans_lea_w(DisasContext *ctx, arg_lea_w *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_W);
}
static bool trans_lea_d(DisasContext *ctx, arg_lea_d *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_D);
}
static bool trans_lea_b_ze(DisasContext *ctx, arg_lea_b_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_B_ZE);
}
static bool trans_lea_h_ze(DisasContext *ctx, arg_lea_h_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_H_ZE);
}
static bool trans_lea_w_ze(DisasContext *ctx, arg_lea_w_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_W_ZE);
}
static bool trans_lea_d_ze(DisasContext *ctx, arg_lea_d_ze *a)
{
    return translate_andes_lea(ctx, a->rd, a->rs1, a->rs2, LEA_D_ZE);
}

static bool trans_find_first_x(DisasContext *ctx, int rd,
                               int rs1, int rs2, int func)
{
    TCGv src1, src2, dest, tmp;
    src1 = get_gpr(ctx, rs1, EXT_NONE);
    src2 = get_gpr(ctx, rs2, EXT_NONE);
    dest = dest_gpr(ctx, rd);
    tmp = tcg_constant_tl(func);
    gen_helper_andes_v5_fb_x(dest, src1, src2, tmp);
    return true;
}
static bool trans_f_ffb(DisasContext *ctx, arg_f_ffb *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFB);
}
static bool trans_f_ffzmism(DisasContext *ctx, arg_f_ffzmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFZMISM);
}
static bool trans_f_ffmism(DisasContext *ctx, arg_f_ffmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FFMISM);
}
static bool trans_f_flmism(DisasContext *ctx, arg_f_flmism *a)
{
    return trans_find_first_x(ctx, a->rd, a->rs1, a->rs2, F_FLMISM);
}

/*** Custom-0 instructions ***/
static void gen_store_legacy(DisasContext *ctx, uint32_t opc, int rs1,
                             int rs2, target_long imm)
{
    TCGv t0, dat, base;
    int memop;
    const int tcg_memop_lookup_local[8] = {
        [0 ... 7] = -1,
        [0] = MO_SB,
        [1] = MO_TESW,
        [2] = MO_TESL,
    #ifdef TARGET_RISCV64
        [3] = MO_TESQ,
    #endif
    };

    memop = tcg_memop_lookup_local[(opc >> 12) & 0x7];

    if (memop < 0) {
        gen_exception_illegal(ctx);
        return;
    }

    t0 = tcg_temp_new();
    base = get_gpr(ctx, rs1, EXT_NONE);
    dat = get_gpr(ctx, rs2, EXT_NONE);
    tcg_gen_addi_tl(t0, base, imm);
    tcg_gen_qemu_st_tl(dat, t0, ctx->mem_idx, memop);
}

static void gen_load_legacy(DisasContext *ctx, uint32_t opc, int rd,
                            int rs1, target_long imm)
{
    TCGv t0, base, dest;
    int memop;
    const int tcg_memop_lookup_local[8] = {
        [0 ... 7] = -1,
        [0] = MO_SB,
        [1] = MO_TESW,
        [2] = MO_TESL,
        [4] = MO_UB,
        [5] = MO_TEUW,
    #ifdef TARGET_RISCV64
        [3] = MO_TEUQ,
        [6] = MO_TEUL,
    #endif
    };

    memop = tcg_memop_lookup_local[(opc >> 12) & 0x7];

    if (memop < 0) {
        gen_exception_illegal(ctx);
        return;
    }

    t0 = tcg_temp_new();
    base = get_gpr(ctx, rs1, EXT_NONE);
    dest = dest_gpr(ctx, rd);
    tcg_gen_addi_tl(t0, base, imm);
    tcg_gen_qemu_ld_tl(dest, t0, ctx->mem_idx, memop);
    gen_set_gpr(ctx, rd, dest);
}

static bool trans_addigp(DisasContext *ctx, arg_addigp *a)
{
    TCGv gp, dst;
    gp = get_gpr(ctx, GP, EXT_NONE);
    dst = dest_gpr(ctx, a->rd);
    tcg_gen_addi_tl(dst, gp, a->imm);
    gen_set_gpr(ctx, a->rd, dst);
    return true;
}

static bool trans_lbgp(DisasContext *ctx, arg_lbgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LB, a->rd, GP, a->imm);
    return true;
}
static bool trans_lbugp(DisasContext *ctx, arg_lbugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LBU, a->rd, GP, a->imm);
    return true;
}
static bool trans_sbgp(DisasContext *ctx, arg_sbgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SB, GP, a->rs2, a->imm);
    return true;
}

/*** Custom-1 instructions ***/
static bool trans_lhgp(DisasContext *ctx, arg_lhgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LH, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lhugp(DisasContext *ctx, arg_lhugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LHU, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lwgp(DisasContext *ctx, arg_lwgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LW, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_lwugp(DisasContext *ctx, arg_lwugp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LWU, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_ldgp(DisasContext *ctx, arg_ldgp *a)
{
    gen_load_legacy(ctx, OPC_RISC_LD, a->rd, GP, a->imm_c1);
    return true;
}
static bool trans_shgp(DisasContext *ctx, arg_shgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SH, GP, a->rs2, a->imm_c1);
    return true;
}
static bool trans_swgp(DisasContext *ctx, arg_swgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SW, GP, a->rs2, a->imm_c1);
    return true;
}
static bool trans_sdgp(DisasContext *ctx, arg_sdgp *a)
{
    gen_store_legacy(ctx, OPC_RISC_SD, GP, a->rs2, a->imm_c1);
    return true;
}

/* Andes INT4 Vector Load Extension */
/*
 *** INT4 unit stride load and store
 */
typedef void gen_helper_ldst_us_int4(TCGv_ptr, TCGv_ptr, TCGv,
                                TCGv_i32, TCGv_env, TCGv_i32);

static bool ldst_us_int4_trans(uint32_t vd, uint32_t rs1, uint32_t data,
                          gen_helper_ldst_us_int4 *fn, DisasContext *s,
                          bool is_store, bool is_sign)
{
    TCGv_ptr dest, mask;
    TCGv base;
    TCGv_i32 desc, sign;

    TCGLabel *over = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
    tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

    dest = tcg_temp_new_ptr();
    mask = tcg_temp_new_ptr();
    base = get_gpr(s, rs1, EXT_NONE);

    /*
     * As simd_desc supports at most 2048 bytes, and in this implementation,
     * the max vector group length is 4096 bytes. So split it into two parts.
     *
     * The first part is vlen in bytes, encoded in maxsz of simd_desc.
     * The second part is lmul, encoded in data of simd_desc.
     */
    desc = tcg_constant_i32(simd_desc(s->cfg_ptr->vlenb / 8,
                                      s->cfg_ptr->vlenb / 8, data));
    sign = tcg_constant_i32(is_sign);

    tcg_gen_addi_ptr(dest, tcg_env, vreg_ofs(s, vd));
    tcg_gen_addi_ptr(mask, tcg_env, vreg_ofs(s, 0));

    fn(dest, mask, base, sign, tcg_env, desc);

    if (!is_store) {
        mark_vs_dirty(s);
    }

    gen_set_label(over);
    return true;
}

static bool ld_us_int4_op(DisasContext *s, arg_rmr_andes *a,
                          uint8_t eew, uint32_t is_sign)
{
    uint32_t data = 0;
    gen_helper_ldst_us_int4 *fn;
    static gen_helper_ldst_us_int4 * const fns[2][1] = {
        /* masked unit stride load */
        { gen_helper_vln8_v_mask},
        /* unmasked unit stride load */
        { gen_helper_vln8_v}
    };

    fn =  fns[a->vm][eew];
    if (fn == NULL) {
        return false;
    }

    /*
     * Vector load/store instructions have the EEW encoded
     * directly in the instructions. The maximum vector size is
     * calculated with EMUL rather than LMUL.
     */
    uint8_t emul = vext_get_emul(s, eew);
    data = FIELD_DP32(data, VDATA, VM, a->vm);
    data = FIELD_DP32(data, VDATA, LMUL, emul);
    /* assume nf field value = 1(000) */
    data = FIELD_DP32(data, VDATA, NF, 1);
    data = FIELD_DP32(data, VDATA, VTA, s->vta);
    data = FIELD_DP32(data, VDATA, VMA, s->vma);
    return ldst_us_int4_trans(a->rd, a->rs1, data, fn, s, false, is_sign);
}

static bool ld_us_int4_check(DisasContext *s, arg_rmr_andes* a, uint8_t eew)
{
    return require_rvv(s) &&
           vext_check_isa_ill(s) &&
           require_vm(a->vm, a->rd);
}

#define GEN_VEXT_SIGN_TRANS(NAME, EEW, ARGTYPE, OP, CHECK, SIGN)    \
static bool trans_##NAME(DisasContext *ctx, arg_##ARGTYPE * a)      \
{                                                                   \
    REQUIRE_VL4;                                                    \
    if (CHECK(ctx, a, EEW)) {                                       \
        return OP(ctx, a, EEW, SIGN);                               \
    }                                                               \
    return false;                                                   \
}

GEN_VEXT_SIGN_TRANS(vln8_v,  MO_8,  rmr_andes, ld_us_int4_op,
                    ld_us_int4_check, true)
GEN_VEXT_SIGN_TRANS(vlnu8_v, MO_8,  rmr_andes, ld_us_int4_op,
                    ld_us_int4_check, false)


/* Andes Scalar BFLOAT16 Conversion Extension */
/* Like Zfbfmin's fcvt_s_bf16 */
static bool trans_nfcvt_s_bf16(DisasContext *ctx, arg_nfcvt_s_bf16 *a)
{
    REQUIRE_FPU;
    REQUIRE_BF16CVT;

    TCGv_i64 dest = dest_fpr(ctx, a->rd);
    TCGv_i64 src2 = get_fpr_hs(ctx, a->rs2);

    gen_set_rm(ctx, RISCV_FRM_DYN);
    gen_helper_andes_nfcvt_s_bf16(dest, tcg_env, src2);
    gen_set_fpr_hs(ctx, a->rd, dest);
    mark_fs_dirty(ctx);
    return true;
}

/* Like Zfbfmin's fcvt_bf16_s */
static bool trans_nfcvt_bf16_s(DisasContext *ctx, arg_nfcvt_bf16_s *a)
{
    REQUIRE_FPU;
    REQUIRE_BF16CVT;

    TCGv_i64 dest = dest_fpr(ctx, a->rd);
    TCGv_i64 src2 = get_fpr_hs(ctx, a->rs2);

    gen_set_rm(ctx, RISCV_FRM_DYN);
    gen_helper_andes_nfcvt_bf16_s(dest, tcg_env, src2);
    gen_set_fpr_hs(ctx, a->rd, dest);
    mark_fs_dirty(ctx);
    return true;
}

/* Andes Vector BFLOAT16 Conversion Extension */
/* Like ZVFBFMIN's vfwcvtbf16_f_f_v*/
static bool trans_vfwcvt_s_bf16(DisasContext *ctx, arg_vfwcvt_s_bf16 *a)
{
    REQUIRE_FPU;
    REQUIRE_MFSEW;
    REQUIRE_BF16CVT;

    /* Only support in SEW = 16 */
    if (require_rvv(ctx) && vext_check_isa_ill(ctx) &&
        vext_check_ds(ctx, a->rd, a->rs2, 1) && ctx->sew == MO_16) {
        uint32_t data = 0;
        TCGLabel *over = gen_new_label();

        gen_set_rm_chkfrm(ctx, RISCV_FRM_DYN);
        tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
        tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

        /* Set Vector Mask Encoding to 1 = unmasked */
        data = FIELD_DP32(data, VDATA, VM, 1);
        data = FIELD_DP32(data, VDATA, LMUL, ctx->lmul);
        data = FIELD_DP32(data, VDATA, VTA, ctx->vta);
        data = FIELD_DP32(data, VDATA, VMA, ctx->vma);
        tcg_gen_gvec_3_ptr(vreg_ofs(ctx, a->rd), vreg_ofs(ctx, 0),
                           vreg_ofs(ctx, a->rs2), tcg_env,
                           ctx->cfg_ptr->vlenb / 8,
                           ctx->cfg_ptr->vlenb / 8, data,
                           gen_helper_andes_vfwcvt_s_bf16);
        mark_vs_dirty(ctx);
        gen_set_label(over);
        return true;
    }
    return false;
}

/* Like ZVFBFMIN's vfncvtbf16_f_f_w */
static bool trans_vfncvt_bf16_s(DisasContext *ctx, arg_vfncvt_bf16_s *a)
{
    REQUIRE_FPU;
    REQUIRE_MFSEW;
    REQUIRE_BF16CVT;

    /* Only support in SEW = 16 */
    if (require_rvv(ctx) && vext_check_isa_ill(ctx) &&
        vext_check_sd(ctx, a->rd, a->rs2, 1) && ctx->sew == MO_16) {
        uint32_t data = 0;
        TCGLabel *over = gen_new_label();

        gen_set_rm(ctx, RISCV_FRM_DYN);
        tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
        tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

        /* Set Vector Mask Encoding to 1 = unmasked */
        data = FIELD_DP32(data, VDATA, VM, 1);
        data = FIELD_DP32(data, VDATA, LMUL, ctx->lmul);
        data = FIELD_DP32(data, VDATA, VTA, ctx->vta);
        data = FIELD_DP32(data, VDATA, VMA, ctx->vma);
        tcg_gen_gvec_3_ptr(vreg_ofs(ctx, a->rd), vreg_ofs(ctx, 0),
                           vreg_ofs(ctx, a->rs2), tcg_env,
                           ctx->cfg_ptr->vlenb / 8,
                           ctx->cfg_ptr->vlenb / 8, data,
                           gen_helper_andes_vfncvt_bf16_s);
        mark_vs_dirty(ctx);
        gen_set_label(over);
        return true;
    }
    return false;
}

/* Andes V5 Vector Packed FP16 extension */
static bool andes_vfpmad_vf(DisasContext *ctx, arg_vfpmadt_vf *a, bool top)
{
    REQUIRE_FPU;
    REQUIRE_EXT(ctx, RVV);
    REQUIRE_VPFH;

    /* Only support in SEW = 16 */
    if (ctx->sew != MO_16) {
        return false;
    }
    TCGv_ptr dest, src2, mask;
    TCGv_i32 desc;
    TCGv_i64 t1, t2;

    TCGLabel *over = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
    tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

    uint32_t data = 0;

    gen_set_rm(ctx, RISCV_FRM_DYN);
    data = FIELD_DP32(data, VDATA, VM, a->vm);
    data = FIELD_DP32(data, VDATA, LMUL, ctx->lmul);
    data = FIELD_DP32(data, VDATA, VTA, ctx->vta);
    data = FIELD_DP32(data, VDATA, VMA, ctx->vma);
    dest = tcg_temp_new_ptr();
    mask = tcg_temp_new_ptr();
    src2 = tcg_temp_new_ptr();
    desc = tcg_constant_i32(simd_desc(ctx->cfg_ptr->vlenb / 8,
                                      ctx->cfg_ptr->vlenb / 8, data));

    tcg_gen_addi_ptr(dest, tcg_env, vreg_ofs(ctx, a->rd));
    tcg_gen_addi_ptr(src2, tcg_env, vreg_ofs(ctx, a->rs2));
    tcg_gen_addi_ptr(mask, tcg_env, vreg_ofs(ctx, 0));

    t1 = get_fpr_hs(ctx, a->rs1);
    t2 = tcg_constant_i64(top);

    gen_helper_vfpmad_vf(dest, mask, t1, t2, src2, tcg_env, desc);

    mark_vs_dirty(ctx);
    gen_set_label(over);
    return true;
}

static bool trans_vfpmadt_vf(DisasContext *ctx, arg_vfpmadt_vf *a)
{
    return andes_vfpmad_vf(ctx, a, true);
}

static bool trans_vfpmadb_vf(DisasContext *ctx, arg_vfpmadb_vf *a)
{
    return andes_vfpmad_vf(ctx, a, false);
}

/* Andes V5 Vector Dot Product extension */
#define GEN_OPIVV_4_TRANS(NAME, CHECK) \
static bool trans_##NAME(DisasContext *ctx, arg_rmrr_andes *a) \
{                                                            \
    REQUIRE_VDOT;                                            \
    static gen_helper_gvec_4_ptr * const fns[2] = {          \
        gen_helper_##NAME##_w,                               \
        gen_helper_##NAME##_d                                \
    };                                                       \
    /* only support sew = 2(32), 3(64) */                    \
    if (ctx->sew >= MO_32)                                   \
        return do_opivv_widen(ctx,                           \
            (arg_rmrr *)a, fns[ctx->sew - 2], CHECK);        \
    return false;                                            \
}

GEN_OPIVV_4_TRANS(vd4dots_vv, opivv_check)
GEN_OPIVV_4_TRANS(vd4dotu_vv, opivv_check)
GEN_OPIVV_4_TRANS(vd4dotsu_vv, opivv_check)

/* Andes V5 Vector Small INT Handling extension */
bool vsih_check(DisasContext *ctx)
{
    REQUIRE_VSIH;
    return true;
}

static bool ld_small_int(DisasContext *s, arg_rr *a)
{
    TCGv_ptr dest;
    TCGv base;
    TCGv_i32 desc;
    uint32_t data = 0;
    /* Uses spec EMUL=(4/SEW)*LMUL */
    uint8_t emul = (4 / (8 << s->sew)) * s->lmul;
    /* non-mask, set vm to 1 */
    data = FIELD_DP32(data, VDATA, VM, 1);
    data = FIELD_DP32(data, VDATA, LMUL, emul);
    /* assume nf field value = 1(000) */
    data = FIELD_DP32(data, VDATA, NF, 1);
    data = FIELD_DP32(data, VDATA, VTA, s->vta);
    data = FIELD_DP32(data, VDATA, VMA, s->vma);

    TCGLabel *over = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
    tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

    dest = tcg_temp_new_ptr();
    base = get_gpr(s, a->rs1, EXT_NONE);

    /*
     * As simd_desc supports at most 2048 bytes, and in this implementation,
     * the max vector group length is 4096 bytes. So split it into two parts.
     *
     * The first part is vlen in bytes, encoded in maxsz of simd_desc.
     * The second part is lmul, encoded in data of simd_desc.
     */
    desc = tcg_constant_i32(simd_desc(s->cfg_ptr->vlenb / 8,
                                      s->cfg_ptr->vlenb / 8, data));

    tcg_gen_addi_ptr(dest, tcg_env, vreg_ofs(s, a->rd));

    gen_helper_vle4_v(dest, base, tcg_env, desc);

    mark_vs_dirty(s);

    gen_set_label(over);
    return true;
}

static bool ld_small_int_check(DisasContext *s, arg_vle4_v *a)
{
    return require_rvv(s) &&
           vext_check_isa_ill(s);
}

static bool trans_vle4_v(DisasContext *ctx, arg_vle4_v *a)
{
    REQUIRE_VSIH;
    if (ld_small_int_check(ctx, a)) {
        return ld_small_int(ctx, a);
    }
    return false;
}

static bool opfbv_check(DisasContext *ctx, arg_rmr2_andes *a)
{
    REQUIRE_VSIH;
    return require_rvv(ctx) &&
           require_scale_rvf(ctx) &&
           vext_check_isa_ill(ctx) &&
           /* OPFV widening instructions ignore vs1 check */
           vext_check_ds(ctx, a->rd, a->rs2, a->vm);
}

/* Only SEW = 16 and 32 is valid, sew=1 or sew=2 */
#define GEN_OPFBV_WIDEN_TRANS(NAME)                                \
static bool trans_##NAME(DisasContext *s, arg_rmr2_andes *a)       \
{                                                                  \
    if (opfbv_check(s, a)) {                                       \
        if (s->sew != MO_16 && s->sew != MO_32)                    \
            return false;                                          \
        uint32_t data = 0;                                         \
        static gen_helper_gvec_3_ptr * const fns[2] = {            \
            gen_helper_##NAME##16,                                 \
            gen_helper_##NAME##32,                                 \
        };                                                         \
        TCGLabel *over = gen_new_label();                          \
        gen_set_rm(s, RISCV_FRM_DYN);                              \
        tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);          \
        tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over); \
                                                                   \
        data = FIELD_DP32(data, VDATA, VM, a->vm);                 \
        data = FIELD_DP32(data, VDATA, LMUL, s->lmul);             \
        data = FIELD_DP32(data, VDATA, VTA, s->vta);               \
        data = FIELD_DP32(data, VDATA, VMA, s->vma);               \
        tcg_gen_gvec_3_ptr(vreg_ofs(s, a->rd), vreg_ofs(s, 0),     \
                           vreg_ofs(s, a->rs2), tcg_env,           \
                           s->cfg_ptr->vlenb / 8,                   \
                           s->cfg_ptr->vlenb / 8, data,             \
                           fns[s->sew - 1]);                       \
        mark_vs_dirty(s);                                          \
        gen_set_label(over);                                       \
        return true;                                               \
    }                                                              \
    return false;                                                  \
}

GEN_OPFBV_WIDEN_TRANS(vfwcvt_f_n_v)
GEN_OPFBV_WIDEN_TRANS(vfwcvt_f_nu_v)
GEN_OPFBV_WIDEN_TRANS(vfwcvt_f_b_v)
GEN_OPFBV_WIDEN_TRANS(vfwcvt_f_bu_v)

/* Andes Vector Quad-Widening Integer Multiply-Add extension */
/* OPIVV with WIDEN */
/*
 * Common check function for vector quad-widening instructions
 * of quad-width result (4*SEW).
 *
 * Rules to be checked here:
 *   1. The largest vector register group used by an instruction
 *      can not be greater than 8 vector registers :
 *      => LMUL < 4 (since dst EMUL must <= 8)
 *      => SEW < 32.
 *   2. Quad-width SEW cannot greater than ELEN.
 *   3. Destination vector register number is multiples of 4 * LMUL.
 *   4. Destination vector register group for a masked vector
 *      instruction cannot overlap the source mask register (v0).
 */

static bool vext_quad_wide_check_common(DisasContext *s, int vd, int vm)
{
    return (s->lmul < 2) &&
           (s->sew < MO_32) &&
           ((s->sew + 2) <= (s->cfg_ptr->elen >> 4)) &&
           require_align(vd, s->lmul + 2) &&
           require_vm(vm, vd);
}

static bool vext_check_qs(DisasContext *s, int vd, int vs, int vm)
{
    return vext_quad_wide_check_common(s, vd, vm) &&
        require_align(vs, s->lmul) &&
        require_noover(vd, s->lmul + 2, vs, s->lmul);
}

static bool vext_check_qss(DisasContext *s, int vd, int vs1, int vs2, int vm)
{
    return vext_check_qs(s, vd, vs2, vm) &&
        require_align(vs1, s->lmul) &&
        require_noover(vd, s->lmul + 2, vs1, s->lmul);
}

static bool opivv_quad_widen_check(DisasContext *s, arg_rmrr_andes *a)
{
    return require_rvv(s) &&
           vext_check_isa_ill(s) &&
           vext_check_qss(s, a->rd, a->rs1, a->rs2, a->vm);
}

static bool do_opivv_quad_widen(DisasContext *s, arg_rmrr_andes *a,
                           gen_helper_gvec_4_ptr *fn,
                           bool (*checkfn)(DisasContext *, arg_rmrr_andes *))
{
    if (checkfn(s, a)) {
        uint32_t data = 0;
        TCGLabel *over = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_vl, 0, over);
        tcg_gen_brcond_tl(TCG_COND_GEU, cpu_vstart, cpu_vl, over);

        data = FIELD_DP32(data, VDATA, VM, a->vm);
        data = FIELD_DP32(data, VDATA, LMUL, s->lmul);
        data = FIELD_DP32(data, VDATA, VTA, s->vta);
        data = FIELD_DP32(data, VDATA, VMA, s->vma);
        tcg_gen_gvec_4_ptr(vreg_ofs(s, a->rd), vreg_ofs(s, 0),
                           vreg_ofs(s, a->rs1),
                           vreg_ofs(s, a->rs2),
                           tcg_env, s->cfg_ptr->vlenb / 8,
                           s->cfg_ptr->vlenb / 8,
                           data, fn);
        mark_vs_dirty(s);
        gen_set_label(over);
        return true;
    }
    return false;
}

#define GEN_OPIVV_QUAD_WIDEN_TRANS(NAME, CHECK) \
static bool trans_##NAME(DisasContext *s, arg_rmrr_andes *a) \
{                                                            \
    if (s->sew >= MO_32)                                     \
        return false;                                        \
    static gen_helper_gvec_4_ptr * const fns[2] = {          \
        gen_helper_##NAME##_b,                               \
        gen_helper_##NAME##_h,                               \
    };                                                       \
    return do_opivv_quad_widen(s, a, fns[s->sew], CHECK);    \
}

/* OPIVX with QUAD-WIDEN */
static bool opivx_quad_widen_check(DisasContext *s, arg_rmrr_andes *a)
{
    return require_rvv(s) &&
           vext_check_isa_ill(s) &&
           vext_check_qs(s, a->rd, a->rs2, a->vm);
}

static bool do_opivx_quad_widen(DisasContext *s, arg_rmrr_andes *a,
                           gen_helper_opivx *fn)
{
    if (opivx_quad_widen_check(s, a)) {
        return opivx_trans(a->rd, a->rs1, a->rs2, a->vm, fn, s);
    }
    return false;
}

#define GEN_OPIVX_QUAD_WIDEN_TRANS(NAME) \
static bool trans_##NAME(DisasContext *s, arg_rmrr_andes *a) \
{                                                            \
    if (s->sew >= MO_32)                                     \
        return false;                                        \
    static gen_helper_opivx * const fns[2] = {               \
        gen_helper_##NAME##_b,                               \
        gen_helper_##NAME##_h,                               \
    };                                                       \
    return do_opivx_quad_widen(s, a, fns[s->sew]);           \
}


GEN_OPIVV_QUAD_WIDEN_TRANS(vqmaccu_vv, opivv_quad_widen_check)
GEN_OPIVV_QUAD_WIDEN_TRANS(vqmacc_vv, opivv_quad_widen_check)
GEN_OPIVV_QUAD_WIDEN_TRANS(vqmaccsu_vv, opivv_quad_widen_check)
GEN_OPIVX_QUAD_WIDEN_TRANS(vqmaccu_vx)
GEN_OPIVX_QUAD_WIDEN_TRANS(vqmacc_vx)
GEN_OPIVX_QUAD_WIDEN_TRANS(vqmaccsu_vx)
GEN_OPIVX_QUAD_WIDEN_TRANS(vqmaccus_vx)
