/*
 * Andes PLIC (Platform Level Interrupt Controller)
 *
 * Copyright (c) 2018 Andes Tech. Corp.
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
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "target/riscv/cpu.h"
#include "hw/sysbus.h"
#include "hw/riscv/andes_plic.h"

/* #define DEBUG_ANDES_PLIC */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR,x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_PLIC
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

enum register_names {
    REG_FEATURE_ENABLE = 0x0000,
    REG_TRIGGER_TYPE_BASE = 0x1080,
    REG_NUM_IRQ_TARGET = 0x1100,
    REG_VER_MAX_PRIORITY = 0x1104,
};

enum feature_enable_register {
    FER_PREEMPT = (1u << 0),
    FER_VECTORED = (1u << 1),
};

#define PLICSW_CONTEXT_PER_HART     0x1000
#define PLICSW_CONTEXT_CLAIM        0x4

static uint64_t
andes_plic_read(void *opaque, hwaddr addr, unsigned size);

static int
update_eip_vectored(void *plic)
{
    AndesPLICState *s = ANDES_PLIC(plic);
    SiFivePLICState *ss = SIFIVE_PLIC(s);
    int addrid;

    /* arbitrate IRQs:
     *   algorithm: lowest addrid first
     */
    for (addrid = 0; addrid < ss->num_addrs; addrid++) {
        uint32_t hartid = ss->addr_config[addrid].hartid;
        PLICMode mode = ss->addr_config[addrid].mode;
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
        if (!env) {
            continue;
        }
        CPURVAndesExt *ext = env->ext;
        int level = 0;
        int irq_id = 0;
        switch (mode) {
        case PLICMode_M:
            if (!ext->vectored_irq_m) {
                level = sifive_plic_irqs_pending(plic, addrid);
                if (level) {
                    /* claim here */
                    hwaddr addr = ss->context_base + ss->context_stride * addrid + 4;
                    irq_id = andes_plic_read(plic, addr, 4);
                    ext->vectored_irq_m = irq_id;
                    assert(irq_id);
                }
            }
            yLOG("%s: M level %d, irq_id %d\n", __func__, level, irq_id);
            riscv_cpu_update_mip(RISCV_CPU(cpu), ss->m_mode_mip_mask,
                                      BOOL_TO_MASK(level));
            break;
        case PLICMode_S:
            if (!ext->vectored_irq_s) {
                level = sifive_plic_irqs_pending(plic, addrid);
                if (level) {
                    /* claim here */
                    hwaddr addr = ss->context_base + ss->context_stride * addrid + 4;
                    irq_id = andes_plic_read(plic, addr, 4);
                    ext->vectored_irq_s = irq_id;
                    assert(irq_id);
                }
            }
            yLOG("%s: S level %d, irq_id %d\n", __func__, level, irq_id);
            riscv_cpu_update_mip(RISCV_CPU(cpu), ss->s_mode_mip_mask,
                                      BOOL_TO_MASK(level));
            break;
        default:
            break;
        }
    }

    return 0;
}

static void
push_preempt_context(AndesPLICState *s, uint32_t addrid, uint32_t priority)
{
    uint32_t word = s->priority_words * addrid;
    set_bit(priority, (void *)&s->preempted_priority[word]);
    ++s->claim_count;
}

static void
pop_preempt_context(AndesPLICState *s, uint32_t addrid)
{
    SiFivePLICState *ss = SIFIVE_PLIC(s);
    uint32_t word = s->priority_words * addrid;
    s->last_claimed_priority = find_last_bit(
        (void *)&s->preempted_priority[word], ss->num_priorities + 1);
    clear_bit(s->last_claimed_priority, (void *)&s->preempted_priority[word]);
    --s->claim_count;
}

static uint64_t
andes_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesPLICState *s = ANDES_PLIC(opaque);
    SiFivePLICState *ss = SIFIVE_PLIC(s);
    uint64_t value;
    uint32_t word;

    if ((addr & 0x3)) {
        error_report("%s: invalid register read: %08x", __func__, (uint32_t)addr);
    }

    /* pending RW */
    if (addr >= ss->pending_base &&
        addr < ss->pending_base + (ss->num_sources >> 3)) {
        word = (addr - ss->pending_base) >> 2;
        value = ss->pending[word];
        return value;
    }

    switch (addr)
    {
    case REG_FEATURE_ENABLE:
        value = s->feature_enable;
        break;
    case REG_NUM_IRQ_TARGET:
        value = (ss->num_addrs << 16) | ss->num_sources;
        break;
    case REG_VER_MAX_PRIORITY:
        value = (ss->num_priorities << 16) | 0x0007; /* PLIC ver 0.7 */
        break;
    default:
        if (addr >= REG_TRIGGER_TYPE_BASE &&
            addr < REG_TRIGGER_TYPE_BASE + (ss->bitfield_words << 2)) {
            word = (addr - REG_TRIGGER_TYPE_BASE) >> 2;
            value = s->trigger_type[word];
            break;
        }

        if (ss->m_mode_mip_mask == MIP_MSIP) {
            CPUState *cpu = current_cpu;
            CPURISCVState *env = cpu->env_ptr;
            hwaddr claim = ss->context_base + PLICSW_CONTEXT_CLAIM + PLICSW_CONTEXT_PER_HART * env->mhartid;

            if (addr == claim) {
                return 0;
            }
        }

        memory_region_dispatch_read(&s->parent_mmio, addr, &value, size,
                                    MEMTXATTRS_UNSPECIFIED);

        /* check if claimed successfully */
        if (s->feature_enable & FER_PREEMPT && addr >= ss->context_base &&
            addr < ss->context_base + ss->num_addrs * ss->context_stride) {
            uint32_t addrid = (addr - ss->context_base) / ss->context_stride;
            uint32_t contextid = (addr & (ss->context_stride - 1));
            if ((contextid == 4) && value) {
                /* an interrupt ID has claimed */
                uint32_t curr_target_priority = ss->target_priority[addrid];
                uint32_t next_target_priority = ss->source_priority[value];
                push_preempt_context(s, addrid, curr_target_priority);
                ss->target_priority[addrid] = next_target_priority;
                /* hack! trigger sifive_plic_update */
                sifive_plic_lower_irq(ss, 0);
            }
            /* TODO: read preempted priority statck registers */
        }
    }

    LOG("== %s %s:  addr %08x, size %08x, value %08x ==\n", (ss->m_mode_mip_mask==MIP_MEIP) ? "":"sw", __func__, (int)addr, size,
        (int)value);
    return value;
}

static void
andes_plic_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *s = ANDES_PLIC(opaque);
    SiFivePLICState *ss = SIFIVE_PLIC(s);
    uint32_t word, xchg;
    LOG("== %s %s: addr %08x, size %08x, value %08x ==\n", (ss->m_mode_mip_mask==MIP_MEIP) ? "":"sw", __func__, (int)addr, size, (int)value);

    if ((addr & 0x3)) {
        error_report("%s: invalid register write: %08x", __func__, (uint32_t)addr);
    }

    /* pending RW */
    if (addr >= ss->pending_base &&
        addr < ss->pending_base + (ss->num_sources >> 3)) {
        word = (addr - ss->pending_base) >> 2;

        if (ss->m_mode_mip_mask == MIP_MSIP) {
            CPUState *cpu = current_cpu;
            CPURISCVState *env = cpu->env_ptr;
            uint32_t mask = 0xff << (8 * env->mhartid);
            int hartid;

            value &= mask;
            value >>= (8 * env->mhartid);
            for (hartid = 0; hartid < 8; hartid++) {
                if (value & (1 << (7 - hartid))) {
                    CPUState *dst = qemu_get_cpu(hartid);
                    riscv_cpu_update_mip(RISCV_CPU(dst), MIP_MSIP, BOOL_TO_MASK(1));
                }
            }
        } else {
            xchg = ss->pending[word] ^ (uint32_t)value;
            if (xchg) {
                ss->pending[word] = value;
                /* hack! trigger sifive_plic_update */
                sifive_plic_lower_irq(ss, 0);
            }
        }

        return;
    }

    switch (addr)
    {
    case REG_FEATURE_ENABLE:
        s->feature_enable = value & 0x3;
        /* TODO: side effects
         *   PREEMPT: NOP, take effects from next claim/complete.
         *   VECTORED: change update_eip logic.
         */
        ss->update_eip = (s->feature_enable & FER_VECTORED)
                             ? update_eip_vectored
                             : s->parent_update_eip;
        break;
    case REG_NUM_IRQ_TARGET:   /* RO */
    case REG_VER_MAX_PRIORITY: /* RO */
        break;
    default:
        /* R/W1S */
        if (addr >= REG_TRIGGER_TYPE_BASE &&
            addr < REG_TRIGGER_TYPE_BASE + (ss->bitfield_words << 2)) {
            word = (addr - REG_TRIGGER_TYPE_BASE) >> 2;
            s->trigger_type[word] |= value;
            break;
        }

        if (ss->m_mode_mip_mask == MIP_MSIP) {
            CPUState *cpu = current_cpu;
            CPURISCVState *env = cpu->env_ptr;
            hwaddr claim = ss->context_base + PLICSW_CONTEXT_CLAIM + PLICSW_CONTEXT_PER_HART * env->mhartid;

            if (addr == claim) {
                riscv_cpu_update_mip(RISCV_CPU(cpu), MIP_MSIP, BOOL_TO_MASK(0));
                return;
            }
        }

        memory_region_dispatch_write(&s->parent_mmio, addr, value, size,
                                     MEMTXATTRS_UNSPECIFIED);

        /* check if complete */
        if (s->feature_enable & (FER_PREEMPT | FER_VECTORED) &&
            addr >= ss->context_base &&
            addr < ss->context_base + ss->num_addrs * ss->context_stride) {
            uint32_t addrid = (addr - ss->context_base) / ss->context_stride;
            uint32_t contextid = (addr & (ss->context_stride - 1));
            if ((contextid == 4) && value) {
                /* an interrupt ID has completed */
                /* handle vectored */
                uint32_t hartid = ss->addr_config[addrid].hartid;
                CPUState *cpu = qemu_get_cpu(hartid);
                CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
                if (env) {
                    CPURVAndesExt *ext = env->ext;
                    switch (env->priv) {
                    case PRV_M:
                        if (ext->vectored_irq_m) {
                            assert(ext->vectored_irq_m == value);
                            ext->vectored_irq_m = 0;
                        }
                        LOG("%s: M complete %d, vid %d\n", __func__, (int)value, ext->vectored_irq_m);
                        break;
                    case PRV_S:
                        if (ext->vectored_irq_s) {
                            assert(ext->vectored_irq_s == value);
                            ext->vectored_irq_s = 0;
                        }
                        LOG("%s: S complete %d, vid %d\n", __func__, (int)value, ext->vectored_irq_s);
                        break;
                    default:
                        g_assert_not_reached();
                    }
                }
                /* handle preemption */
                pop_preempt_context(s, addrid);
                uint32_t next_target_priority = s->last_claimed_priority;
                ss->target_priority[addrid] = next_target_priority;
                /* hack! trigger sifive_plic_update */
                sifive_plic_lower_irq(ss, 0);
            }
            /* TODO: write preempted priority statck registers */
        }
    }
}

static const MemoryRegionOps andes_plic_ops = {
    .read = andes_plic_read,
    .write = andes_plic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void
andes_plic_reset(DeviceState * dev)
{
    LOG("%s:\n", __func__);
    AndesPLICState *s = ANDES_PLIC(dev);
    AndesPLICClass *k = ANDES_PLIC_GET_CLASS(s);
    SiFivePLICState *ss = SIFIVE_PLIC(s);

    memset(ss->target_priority, 0, sizeof(uint32_t) * ss->num_addrs);
    memset(s->trigger_type, 0, sizeof(uint32_t) * ss->bitfield_words);
    memset(s->preempted_priority, 0,
           sizeof(uint32_t) * s->priority_words * ss->num_addrs);
    memset(s->preempted_id, 0,
           sizeof(uint32_t) * ss->bitfield_words * ss->num_addrs);

    if (k->parent_reset)
        k->parent_reset(dev);

    s->feature_enable = 0;
    s->claim_count = 0;
    s->last_claimed_id = 0;
    s->last_claimed_priority = 0;
}

static void
andes_plic_realize(DeviceState *dev, Error **errp)
{
    LOG("%s:\n", __func__);
    AndesPLICState *s = ANDES_PLIC(dev);
    SiFivePLICState *ss = SIFIVE_PLIC(dev);
    AndesPLICClass *k = ANDES_PLIC_GET_CLASS(s);
    Error *local_err = NULL;

    k->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    s->priority_words = (ss->num_priorities + 31) >> 5;
    s->trigger_type = g_new(uint32_t, ss->bitfield_words);
    s->preempted_priority = g_new(uint32_t, s->priority_words * ss->num_addrs);
    s->preempted_id = g_new(uint32_t, ss->bitfield_words * ss->num_addrs);

    /* override MemoryRegionOps */
    s->parent_mmio = ss->mmio;
    memory_region_init_io(&ss->mmio, OBJECT(dev), &andes_plic_ops, s,
                          TYPE_ANDES_PLIC, ss->aperture_size);

    /* interface */
    s->parent_update_eip = ss->update_eip;
}

#if 0
static Property sifive_plic_properties[] = {
    DEFINE_PROP_STRING("hart-config", SiFivePLICState, hart_config),
    DEFINE_PROP_UINT32("num-sources", SiFivePLICState, num_sources, 0),
    DEFINE_PROP_UINT32("num-priorities", SiFivePLICState, num_priorities, 0),
    DEFINE_PROP_UINT32("priority-base", SiFivePLICState, priority_base, 0),
    DEFINE_PROP_UINT32("pending-base", SiFivePLICState, pending_base, 0),
    DEFINE_PROP_UINT32("enable-base", SiFivePLICState, enable_base, 0),
    DEFINE_PROP_UINT32("enable-stride", SiFivePLICState, enable_stride, 0),
    DEFINE_PROP_UINT32("context-base", SiFivePLICState, context_base, 0),
    DEFINE_PROP_UINT32("context-stride", SiFivePLICState, context_stride, 0),
    DEFINE_PROP_UINT32("aperture-size", SiFivePLICState, aperture_size, 0),
    DEFINE_PROP_END_OF_LIST(),
};
#endif

static void andes_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    /* SiFivePLICClass *sdc = SIFIVE_PLIC_CLASS(klass); */
    AndesPLICClass *adc = ANDES_PLIC_CLASS(klass);

    /* TODO: add own properties */
    /* dc->props = andes_plic_properties; */
    device_class_set_parent_reset(dc, andes_plic_reset, &adc->parent_reset);
    device_class_set_parent_realize(dc, andes_plic_realize, &adc->parent_realize);
}

static const TypeInfo andes_plic_info = {
    .name          = TYPE_ANDES_PLIC,
    .parent        = TYPE_SIFIVE_PLIC,
    .instance_size = sizeof(AndesPLICState),
    .class_init    = andes_plic_class_init,
    .class_size    = sizeof(AndesPLICClass),
};

static void andes_plic_register_types(void)
{
    LOG("%s:\n", __func__);
    type_register_static(&andes_plic_info);
}

type_init(andes_plic_register_types)

/*
 * Create PLIC device.
 */
DeviceState *andes_plic_create(hwaddr addr, char *hart_config,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t context_base, uint32_t context_stride,
    uint32_t aperture_size)
{
    DeviceState *dev = qdev_create(NULL, TYPE_ANDES_PLIC);
    /* TODO: better inheritance, copy from sifive_plic_create() */
    assert(enable_stride == (enable_stride & -enable_stride));
    assert(context_stride == (context_stride & -context_stride));
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "context-base", context_base);
    qdev_prop_set_uint32(dev, "context-stride", context_stride);
    qdev_prop_set_uint32(dev, "aperture-size", aperture_size);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr); /* n = 1 !! */
    return dev;
}

/*
 * Create PLIC SWINT device.
 */
DeviceState *
andes_plic_swint_create(hwaddr addr, char *hart_config, uint32_t num_sources,
                        uint32_t num_priorities, uint32_t priority_base,
                        uint32_t pending_base, uint32_t enable_base,
                        uint32_t enable_stride, uint32_t context_base,
                        uint32_t context_stride, uint32_t aperture_size,
                        uint32_t m_mode_mip_mask, uint32_t s_mode_mip_mask)
{
    DeviceState *dev = qdev_create(NULL, TYPE_ANDES_PLIC);
    /* TODO: better inheritance, copy from sifive_plic_create() */
    assert(enable_stride == (enable_stride & -enable_stride));
    assert(context_stride == (context_stride & -context_stride));
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "context-base", context_base);
    qdev_prop_set_uint32(dev, "context-stride", context_stride);
    qdev_prop_set_uint32(dev, "aperture-size", aperture_size);
    qdev_prop_set_uint32(dev, "m-mode-mip-mask", m_mode_mip_mask);
    qdev_prop_set_uint32(dev, "s-mode-mip-mask", s_mode_mip_mask);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr); /* n = 1 !! */
    return dev;
}
