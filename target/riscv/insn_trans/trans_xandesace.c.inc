/*
 * RISC-V translation routines for Andes ACE instructions
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define REQUIRE_ACE do { \
    REQUIRE_CFG(CSR_MMSC_CFG, MASK_MMSC_CFG_ACE) \
} while (0)

#define REQUIRE_ACES do { \
    REQUIRE_CFG(CSR_MMISC_CTL, MASK_MMISC_CTL_ACES) \
} while (0)

#include "andes_ace_helper.c.inc"
/* Original Custom-3 */
static bool trans_andes_ace(DisasContext *ctx, arg_andes_ace *a)
{
    if (!ctx->cfg_ptr->ext_XAndesAce) {
        qemu_printf("XAndesAce not set\n");
        return false;
    }
    REQUIRE_ACE;
    REQUIRE_ACES;
    TCGv opc = tcg_constant_tl(ctx->opcode);
    gen_helper_andes_ace(tcg_env, opc);
    gen_update_pc(ctx, ctx->cur_insn_len);
    lookup_and_goto_ptr(ctx);

    return true;
}

/* Custom-0 */
static bool trans_andes_ace0(DisasContext *ctx, arg_andes_ace *a)
{
    if (!ctx->cfg_ptr->ext_XAndesAce || !ctx->cfg_ptr->ext_XAndesAceExtra) {
        return false;
    }
    return trans_andes_ace(ctx, a);
}

/* Custom-1 */
static bool trans_andes_ace1(DisasContext *ctx, arg_andes_ace *a)
{
    if (!ctx->cfg_ptr->ext_XAndesAce || !ctx->cfg_ptr->ext_XAndesAceExtra) {
        return false;
    }
    return trans_andes_ace(ctx, a);
}

/* Custom-2 */
static bool trans_andes_ace2(DisasContext *ctx, arg_andes_ace *a)
{
    if (!ctx->cfg_ptr->ext_XAndesAce || !ctx->cfg_ptr->ext_XAndesAceExtra) {
        return false;
    }
    return trans_andes_ace(ctx, a);
}
