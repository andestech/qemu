/*
 * RISC-V translation routines for Andes VMM instructions
 *
 * Copyright (c) 2025 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define REQUIRE_AMM(ctx) \
do {                                                 \
    if (!ctx->cfg_ptr->ext_XAndesVMM) {              \
        qemu_printf("XAndesVMM is not enabled.\n");  \
        riscv_raise_exception(cpu_env(ctx->cs),      \
            RISCV_EXCP_ILLEGAL_INST, GETPC());       \
        return false;                                \
    }                                                \
    REQUIRE_CFG(CSR_MMSC_CFG3, MASK_MMSC_CFG3_AMM)   \
} while (0)

#define AMM_CHECK(ctx, in_sew) \
do {                                                 \
    if (!require_rvv(ctx) ||                         \
        !vext_check_isa_ill(ctx) ||                  \
        (ctx->sew != in_sew) ||                      \
        (ctx->lmul < 0)) {                           \
        return false;                                \
    }                                                \
} while (0)

static bool require_noover_amm(const int8_t dst, const int8_t src,
                               const int8_t src_lmul)
{
    int8_t src_range = 1 << src_lmul;
    return (dst < src) || (dst > src + src_range - 1);
}

#define AMM_ARITHMETIC_CHECK(ctx, arg) \
do {                                                 \
    if (!require_align(arg->vs1, ctx->lmul) ||       \
        !require_align(arg->vs2, ctx->lmul) ||       \
        !require_noover_amm(arg->vd, arg->vs1,       \
                            ctx->lmul) ||            \
        !require_noover_amm(arg->vd, arg->vs2,       \
                            ctx->lmul)) {            \
        return false;                                \
    }                                                \
} while (0)

#define GEN_ANDES_VQAMM_TRANS(NAME) \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)  \
{                                                                     \
    REQUIRE_AMM(ctx);                                                 \
    AMM_CHECK(ctx, MO_8);                                             \
    AMM_ARITHMETIC_CHECK(ctx, a);                                     \
                                                                      \
    uint32_t data = 0;                                                \
    data = FIELD_DP32(data, VDATA, LMUL, ctx->lmul);                  \
    tcg_gen_gvec_3_ptr(vreg_ofs(ctx, a->vd),                          \
                       vreg_ofs(ctx, a->vs1),                         \
                       vreg_ofs(ctx, a->vs2),                         \
                       tcg_env,                                       \
                       ctx->cfg_ptr->vlenb,                           \
                       ctx->cfg_ptr->vlenb,                           \
                       data,                                          \
                       gen_helper_##NAME);                            \
    finalize_rvv_inst(ctx);                                           \
    return true;                                                      \
}

GEN_ANDES_VQAMM_TRANS(andes_vqammuu_vv)
GEN_ANDES_VQAMM_TRANS(andes_vqammus_vv)
GEN_ANDES_VQAMM_TRANS(andes_vqammsu_vv)
GEN_ANDES_VQAMM_TRANS(andes_vqammss_vv)

#define GEN_ANDES_VLSE_TRANS(NAME, REG, SEW) \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)  \
{                                                           \
    REQUIRE_AMM(ctx);                                       \
    AMM_CHECK(ctx, SEW);                                    \
                                                            \
    TCGv_ptr a0;                                            \
    TCGv_i32 desc;                                          \
    uint32_t data = 0;                                      \
    data = FIELD_DP32(data, VDATA, LMUL, ctx->lmul);        \
    desc = tcg_constant_i32(simd_desc(ctx->cfg_ptr->vlenb,  \
                                      ctx->cfg_ptr->vlenb,  \
                                      data));               \
    a0 = tcg_temp_new_ptr();                                \
    tcg_gen_addi_ptr(a0, tcg_env, vreg_ofs(ctx, a->REG));   \
    gen_helper_##NAME(a0,                                   \
                      get_gpr(ctx, a->rs1, EXT_NONE),       \
                      get_gpr(ctx, a->rs2, EXT_NONE),       \
                      tcg_env, desc);                       \
    finalize_rvv_inst(ctx);                                 \
    return true;                                            \
}

GEN_ANDES_VLSE_TRANS(andes_vle8_mk, vd, MO_8);
GEN_ANDES_VLSE_TRANS(andes_vle8_nk, vd, MO_8);
GEN_ANDES_VLSE_TRANS(andes_vle8_kn, vd, MO_8);
GEN_ANDES_VLSE_TRANS(andes_vle32_mn, vd, MO_32);
GEN_ANDES_VLSE_TRANS(andes_vse8_nk, vs, MO_8);
GEN_ANDES_VLSE_TRANS(andes_vse8_kn, vs, MO_8);
GEN_ANDES_VLSE_TRANS(andes_vse32_nm, vs, MO_32);
GEN_ANDES_VLSE_TRANS(andes_vse32_mn, vs, MO_32);
