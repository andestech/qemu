/*
 * Andes ACE helper functions
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "andes_ace_helper.h"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

AceAgentFuncPtr ace_agent_cb_func_table[] = {
    (AceAgentFuncPtr)qemu_get_XRF, (AceAgentFuncPtr)qemu_set_XRF,
    (AceAgentFuncPtr)qemu_get_FRF, (AceAgentFuncPtr)qemu_set_FRF,
    (AceAgentFuncPtr)qemu_get_VRF, (AceAgentFuncPtr)qemu_set_VRF,
    (AceAgentFuncPtr)qemu_get_MEM, (AceAgentFuncPtr)qemu_set_MEM,
    (AceAgentFuncPtr)qemu_get_CSR, (AceAgentFuncPtr)qemu_set_CSR,
    (AceAgentFuncPtr)qemu_set_CSR_masked,
    (AceAgentFuncPtr)qemu_get_ACM, (AceAgentFuncPtr)qemu_set_ACM,
    (AceAgentFuncPtr)qemu_get_PC,
    (AceAgentFuncPtr)qemu_get_hart_id,
    (AceAgentFuncPtr)qemu_get_cpu_priv,
    (AceAgentFuncPtr)qemu_get_ACES, (AceAgentFuncPtr)qemu_set_ACES,
    (AceAgentFuncPtr)qemu_ace_add_reg_desc,
    };

/* QEMU ACE agent handler */
void *qemu_ace_agent_handle;

int32_t qemu_ace_agent_load(const char *filename)
{
    /* Check whether handler already loaded? */
    if (qemu_ace_agent_handle != NULL) {
        return 0;
    }
    qemu_ace_agent_handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if (!qemu_ace_agent_handle) {
        return -1;
    }
    return 0;
}

int32_t qemu_ace_agent_load_symbol(const char *symbol_name, void **func_ptr)
{
    if (qemu_ace_agent_handle == NULL) {
            return -1;
    }
    /* Clear any existing error */
    dlerror();
    *(void **)(func_ptr) = dlsym(qemu_ace_agent_handle, symbol_name);
    if (*func_ptr == NULL) {
        return -1;
    }
    return 0;
}

int32_t qemu_ace_agent_register(CPURISCVState *env, const char *extlibpath,
                                int32_t multi)
{
    AceAgentVersion ace_agent_version;
    target_ulong hartid;
#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif
    if (qemu_ace_agent_load_symbol("ace_agent_version",
        (void **)&ace_agent_version) != 0) {
        return -1;
    }
    /*
     * The QEMU ACE Agent version can be higher than the internal QEMU agent
     * version, as newer versions of the ACE Agent support additional features
     * that may not be required by the internal QEMU implementation
     */
    if (ace_agent_version(env) < ACE_AGENT_VERSION) {
        qemu_printf("QEMU and ACE agent version don't match\n");
        qemu_printf("QEMU ACE version=%d, ACE Agent version=%d\n",
                    ACE_AGENT_VERSION, ace_agent_version(env));
        return -1;
    }
    if (env->ace_agent_register == NULL) {
        if (qemu_ace_agent_load_symbol("ace_agent_register",
            (void **)&env->ace_agent_register) != 0) {
            return -1;
        }
    }
    /*
     * register with direct function tables and also pass number of
     * total callback functions
     */
    return env->ace_agent_register(env, ace_agent_cb_func_table,
                                   ACE_CB_NAME_MAX, extlibpath, hartid, multi);
}

int32_t qemu_ace_agent_run_insn(CPURISCVState *env, uint32_t opcode)
{
    target_ulong hartid;
    if (env->ace_agent_run_insn == NULL) {
        if (qemu_ace_agent_load_symbol("ace_agent_run_insn",
            (void **)&env->ace_agent_run_insn) != 0) {
            return -1;
        }
    }
#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif
    return env->ace_agent_run_insn(env, opcode, hartid);
}

uint64_t *qemu_ace_agent_get_register(CPURISCVState *env, uint32_t g,
                                    uint32_t i, uint32_t *value_len)
{
    target_ulong hartid;
    if (env->ace_agent_get_register == NULL) {
        if (qemu_ace_agent_load_symbol("ace_agent_get_register",
            (void **)&env->ace_agent_get_register) != 0) {
            return NULL;
        }
    }
#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif
    return env->ace_agent_get_register(env, hartid, g, i, value_len);
}

void qemu_ace_agent_set_register(CPURISCVState *env, uint32_t g,
                                    uint32_t i, const uint64_t *value,
                                    uint32_t value_len)
{
    target_ulong hartid;
    if (env->ace_agent_set_register == NULL) {
        if (qemu_ace_agent_load_symbol("ace_agent_set_register",
            (void **)&env->ace_agent_set_register) != 0) {
            return;
        }
    }
#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif
    env->ace_agent_set_register(env, hartid, g, i, value, value_len);
}

const unsigned char *qemu_ace_agent_get_packet(CPURISCVState *env,
                                               uint32_t *len)
{
    target_ulong hartid;
    if (env->ace_agent_get_packet == NULL) {
        if (qemu_ace_agent_load_symbol("ace_agent_get_packet",
            (void **)&env->ace_agent_get_packet) != 0) {
            return NULL;
        }
    }
#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif
    return env->ace_agent_get_packet(env, hartid, len);
}

uint64_t qemu_get_XRF(CPURISCVState *env, uint32_t index)
{
    if (index == 0) {
        return 0;
    }
    /* Not support RV128 */
    return env->gpr[index];
}

void qemu_set_XRF(CPURISCVState *env, uint32_t index, uint64_t value)
{
    if (index == 0) {
        return;
    }
    env->gpr[index] = value;
}

uint64_t qemu_get_FRF(CPURISCVState *env, uint32_t index)
{
    /* Should we check FP enable? */
    return env->fpr[index];
}

void qemu_set_FRF(CPURISCVState *env, uint32_t index, uint64_t value)
{
    /* Should we check FP enable? */
    /* only updates mstatus, doesn't update ctx->mstatus_fs */
#if !defined(CONFIG_USER_ONLY)
    /* mstatus.FS doesn't disable (init/clean/dirty) */
    if (env->mstatus & MSTATUS_FS) {
        env->mstatus |= MSTATUS_FS;
    }
#endif
    env->fpr[index] = value;
}

unsigned char *qemu_get_VRF(CPURISCVState *env, uint32_t index)
{
    /* Should we check Vector enable? */
    /* div 8, check cpu.h */
    uint16_t vlenq = env_archcpu(env)->cfg.vlenb >> 3;
    /* get v register index pointer */
    uint64_t *vreg = &env->vreg[index * vlenq];
    return (unsigned char *)vreg;
}

void qemu_set_VRF(CPURISCVState *env, uint32_t index, unsigned char *value)
{
    /* Should we check Vector enable? */
    /* div 8, check cpu.h */
    uint16_t vlenq = env_archcpu(env)->cfg.vlenb >> 3;
    /* only update mstatus, doesn't update ctx->mstatus_vs */
#if !defined(CONFIG_USER_ONLY)
    /* mstatus.VS doesn't disable (init/clean/dirty) */
    if (env->mstatus & MSTATUS_VS) {
        env->mstatus |= MSTATUS_VS;
    }
#endif
    /* memcpy use byte size = vlenb  */
    memcpy(&env->vreg[index * vlenq], value, env_archcpu(env)->cfg.vlenb);
}

/*
 * get_MEM/set_MEM don't support non 1,2,4,8 length,
 * and return/set data always small than or equal to 8
 */
uint64_t qemu_get_MEM(CPURISCVState *env, uint64_t vaddr, uint32_t size)
{
    uint64_t data = 0;
    switch (size) {
    case 1:
#ifndef CONFIG_USER_ONLY
        data = cpu_ldub_data_ra(env, vaddr, env->ace_ra);
#else
        data = cpu_ldub_data(env, vaddr);
#endif
        break;
    case 2:
#ifndef CONFIG_USER_ONLY
        data = cpu_lduw_data_ra(env, vaddr, env->ace_ra);
#else
        data = cpu_lduw_data(env, vaddr);
#endif
        break;
    case 4:
#ifndef CONFIG_USER_ONLY
        data = cpu_ldl_data_ra(env, vaddr, env->ace_ra);
#else
        data = cpu_ldl_data(env, vaddr);
#endif
        break;
    case 8:
#ifndef CONFIG_USER_ONLY
        data = cpu_ldq_data_ra(env, vaddr, env->ace_ra);
#else
        data = cpu_ldq_data(env, vaddr);
#endif
        break;
    default: /* Unknown right size? */
        break;
    }
    return data;
}

void qemu_set_MEM(CPURISCVState *env, uint64_t vaddr,
                  uint64_t value, uint32_t size)
{
    switch (size) {
    case 1:
#ifndef CONFIG_USER_ONLY
        cpu_stb_data_ra(env, vaddr, value, env->ace_ra);
#else
        cpu_stb_data(env, vaddr, value);
#endif
        break;
    case 2:
#ifndef CONFIG_USER_ONLY
        cpu_stw_data_ra(env, vaddr, value, env->ace_ra);
#else
        cpu_stw_data(env, vaddr, value);
#endif
        break;
    case 4:
#ifndef CONFIG_USER_ONLY
        cpu_stl_data_ra(env, vaddr, value, env->ace_ra);
#else
        cpu_stl_data(env, vaddr, value);
#endif
        break;
    case 8:
#ifndef CONFIG_USER_ONLY
        cpu_stq_data_ra(env, vaddr, value, env->ace_ra);
#else
        cpu_stq_data(env, vaddr, value);
#endif
        break;
    default: /* Unknown right size? */
        break;
    }
}

static int CTZLL(unsigned long long x)
{
    if (0 == x) {
        return 0;
    }
    return __builtin_ctzll(x);
}

uint64_t qemu_get_CSR(CPURISCVState *env, uint32_t index, uint64_t mask)
{
    target_ulong value;
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.read != NULL) {
        ops.read(env, index, &value);
        return value & mask;
    }
    return 0;
}

void qemu_set_CSR(CPURISCVState *env, uint32_t index,
                  uint64_t mask, uint64_t value)
{
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.read != NULL && ops.write != NULL) {
        target_ulong old;
        ops.read(env, index, &old);
        target_ulong new = (value  << CTZLL(mask) & mask) | (old & ~mask);
        ops.write(env, index, new);
    }
}

void qemu_set_CSR_masked(CPURISCVState *env, uint32_t index, uint64_t value)
{
    riscv_csr_operations ops;
    riscv_get_csr_ops(index , &ops);
    if (ops.write != NULL) {
        ops.write(env, index, value);
    }
}

/* get memory without via mmu */
AcmStatus qemu_get_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data)
{
#ifndef CONFIG_USER_ONLY
    cpu_physical_memory_read(addr, data, size);
#endif
    return ACM_OK;
}

AcmStatus qemu_set_ACM(CPURISCVState *env, uint64_t addr,
                        uint32_t size, char *data)
{
#ifndef CONFIG_USER_ONLY
    cpu_physical_memory_write(addr, data, size);
#endif
    return ACM_OK;
}

uint32_t qemu_get_ACES(CPURISCVState *env)
{
    return qemu_get_CSR(env, CSR_MMISC_CTL, MASK_MMISC_CTL_ACES);
}

void qemu_set_ACES(CPURISCVState *env, uint32_t value)
{
    qemu_set_CSR(env, CSR_MMISC_CTL, MASK_MMISC_CTL_ACES, value);
}

uint64_t qemu_get_PC(CPURISCVState *env)
{
    return env->pc;
}

uint64_t qemu_get_hart_id(CPURISCVState *env)
{
#ifndef CONFIG_USER_ONLY
    return env->mhartid;
#else
    return 0;
#endif
}

uint32_t qemu_get_cpu_priv(CPURISCVState *env)
{
#ifndef CONFIG_USER_ONLY
    return env->priv;
#else
    return 0;
#endif
}

int32_t qemu_ace_get_filename_for_gdb(CPUState *cs, char *filename)
{
    CPURISCVState *env = cpu_env(cs);
    int32_t ret = 0;
    uint32_t len = 0;
    const unsigned char *data;
    uint32_t hartid;

#ifndef CONFIG_USER_ONLY
    hartid = env->mhartid;
#else
    hartid = 0;
#endif

    /* Get libacetool information */
    data = qemu_ace_agent_get_packet(env, &len);
    if (len > 0 && data != NULL) {
        /* use size as filename to do fopen */
        sprintf(filename, "%u", len);

        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            qemu_printf("Failed to create temporary file for ACE "
                        "debugger information (Hartid=%d)\n", hartid);
            ret = -2;
        } else {
            if (fwrite(data, sizeof(char), len, fp) != len) {
                qemu_printf("Failed to write temporary file for ACE "
                            "debugger information (Hartid=%d)\n", hartid);
                ret = -3;
            }
            fclose(fp);
        }
    } else {
        qemu_printf("Cannot get debugging information for ACE (Hartid=%d)\n",
                    hartid);
        ret = -4;
    }

    return ret;
}

void qemu_ace_add_reg_desc(CPURISCVState *env, const char *GroupName,
                           uint32_t GroupIdx, uint32_t NumOfEntry,
                           uint32_t EntryWidth, int TypeIn)
{
    AceAcrInfo *s;
    /* ACE simulator should add reg description by group ordering */
    if (GroupIdx != env->ace_acracm_reg_number) {
        qemu_printf("ACE add reg description failed for group idx = %d\n",
                    GroupIdx);
        return;
    }
    env->ace_acr_info_list = g_realloc(env->ace_acr_info_list,
        (env->ace_acracm_reg_number + 1) * sizeof(AceAcrInfo));
    if (env->ace_acr_info_list == NULL) {
        qemu_printf("ACE add reg description failed since realloc memory\n");
        return;
    }
    AceAcrInfo *acr_info_list = env->ace_acr_info_list;
    s = &acr_info_list[env->ace_acracm_reg_number];
    memset(s, 0, sizeof(AceAcrInfo));
    s->width = EntryWidth;
    s->num = NumOfEntry;
    s->idx = GroupIdx;
    strncpy(s->name, GroupName, sizeof(s->name) - 1);
    env->ace_acracm_reg_number++;
}
