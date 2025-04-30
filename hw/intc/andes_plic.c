/*
 * Andes PLIC (Platform Level Interrupt Controller)
 *
 * Copyright (c) 2021 Andes Tech. Corp.
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
#include "hw/qdev-properties.h"
#include "target/riscv/cpu.h"
#include "hw/sysbus.h"
#include "hw/pci/msi.h"
#include "hw/intc/andes_plic.h"
#include "migration/vmstate.h"
#include "hw/irq.h"

/* #define DEBUG_ANDES_PLIC */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_PLIC
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif
#define ANDES_PLIC_TRIGGER_TYPE_READONLY 0

static inline bool addr_between(uint32_t addr, uint32_t base, uint32_t offset)
{
    return (addr >= base && addr < base + offset);
}

static uint32_t atomic_set_masked(uint32_t *a, uint32_t mask, uint32_t value)
{
    uint32_t old, new, cmp = qatomic_read(a);

    do {
        old = cmp;
        new = (old & ~mask) | (value & mask);
        cmp = qatomic_cmpxchg(a, old, new);
    } while (old != cmp);

    return old;
}

static AndesPLICMode char_to_mode(char c)
{
    switch (c) {
    case 'U': return PLICMode_U;
    case 'S': return PLICMode_S;
    case 'H': return PLICMode_H;
    case 'M': return PLICMode_M;
    default:
        error_report("plic: invalid mode '%c'", c);
        exit(1);
    }
}

/*
 * parse PLIC hart/mode address offset config
 *
 * "M"              1 hart with M mode
 * "MS,MS"          2 harts, 0-1 with M and S mode
 * "M,MS,MS,MS,MS"  5 harts, 0 with M mode, 1-5 with M and S mode
 */
static void parse_hart_config(AndesPLICState *plic)
{
    int target_cnt, hart_cnt, modes;
    int target_id, hart_id;
    const char *p;
    char c;

    /* count and validate hart/mode combinations */
    target_cnt = 0, hart_cnt = 0, modes = 0;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            target_cnt += ctpop8(modes);
            modes = 0;
            hart_cnt++;
        } else {
            int m = 1 << char_to_mode(c);
            if (modes == (modes | m)) {
                error_report("plic: duplicate mode '%c' in config: %s",
                             c, plic->hart_config);
                exit(1);
            }
            modes |= m;
        }
    }
    if (modes) {
        target_cnt += ctpop8(modes);
    }
    hart_cnt++;

    plic->num_targets = target_cnt;
    plic->num_harts = hart_cnt;

    /* store hart/mode combinations */
    plic->target_config = g_new(AndesPLICTarget, plic->num_targets);
    target_id = 0, hart_id = plic->hart_id_base;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            hart_id++;
        } else {
            plic->target_config[target_id].target_id = target_id;
            plic->target_config[target_id].hart_id = hart_id;
            plic->target_config[target_id].mode = char_to_mode(c);
            target_id++;
        }
    }
}

static uint32_t andes_plic_determine_irq_by_prio(AndesPLICState *plic,
                                                 uint32_t target_id)
{
    uint32_t highest_irq = 0;
    uint32_t highest_prio = plic->priority_threshold[target_id];
    int i, j;
    int num_irq_in_word = 32;

    for (i = 0; i < plic->num_source_in_words; i++) {
        uint32_t pending_enabled_not_claimed =
            (plic->pending[i] &
             plic->enable[target_id * plic->num_source_in_words + i] &
             ~plic->claimed[i]);

        if (!pending_enabled_not_claimed) {
            continue;
        }

        if (i == (plic->num_source_in_words - 1)) {
            /*
             * If plic->num_sources is not multiple of 32, num-of-irq in last
             * word is not 32. Compute the num-of-irq of last word to avoid
             * out-of-bound access of source_priority array.
             */
            num_irq_in_word = plic->num_sources & (32 - 1);
        }

        for (j = 0; j < num_irq_in_word; j++) {
            int irq = (i * 32) + j;
            uint32_t prio = plic->source_priority[irq];
            int enabled = pending_enabled_not_claimed & (1 << j);

            /*
             * With the same priority, lower IRQ has higher effective priority.
             * So we only update highest IRQ when the current IRQ priority is
             * higher than highest IRQ.
             */
            if (enabled && prio > highest_prio) {
                highest_irq = irq;
                highest_prio = prio;
            }
        }
    }

    return highest_irq;
}

static inline
void andes_plic_set_pending(AndesPLICState *plic, int irq, bool level)
{
    atomic_set_masked(&plic->pending[irq / 32], 1 << (irq & 31), -!!level);
}

static inline
void andes_plic_set_preempted_priority(AndesPLICState *plic, uint32_t target_id,
                                       int prio, bool level)
{
    atomic_set_masked(&plic->priority_stack[target_id *
                                    plic->num_priority_in_words + (prio / 32)],
                      1 << (prio & 31), -!!level);
}

static inline
void andes_plic_set_claimed(AndesPLICState *plic, int irq, bool level)
{
    qatomic_set(&plic->claimed[irq], level);
}

static void andes_plic_priority_push(AndesPLICState *plic,
                                     uint32_t target_id, uint32_t irq)
{
    if (!irq) {
        return;
    }

    andes_plic_set_preempted_priority(plic, target_id,
                                      plic->priority_threshold[target_id], true);
    plic->priority_threshold[target_id] = plic->source_priority[irq];
}

static void andes_plic_priority_pop(AndesPLICState *plic, uint32_t target_id)
{
    uint32_t prio_word = 0;
    uint32_t highest_prio;

    /* search the word that contains the highest priority */
    for (int i = plic->num_priority_in_words - 1; i >= 0; i--) {
        prio_word = plic->priority_stack[target_id *
                                         plic->num_priority_in_words + i];
        if (prio_word) {
            highest_prio = i * 32;
            break;
        }
    }
    if (prio_word) {
        uint32_t msb_mask = 1 << 31;
        for (int i = 31; i >= 0; i--) {
           if (prio_word & msb_mask) {
               highest_prio += i;
               break;
           }
           prio_word <<= 1;
        }
    } else {
        /* nothing to pop */
        return;
    }

    andes_plic_set_preempted_priority(plic, target_id, highest_prio, false);
    plic->priority_threshold[target_id] = highest_prio;
}

static void andes_plichw_update(void *opaque)
{
    AndesPLICState *plic = ANDES_PLIC(opaque);

    /* Update external IRQ pendings for all targets */
    for (int i = 0; i < plic->num_targets; i++) {
        uint32_t hart_id = plic->target_config[i].hart_id;
        AndesPLICMode mode = plic->target_config[i].mode;
        CPUState *cpu = qemu_get_cpu(hart_id);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            continue;
        }
        /* If no pending IRQ is qualified in this target, the IRQ is zero */
        uint32_t irq = andes_plic_determine_irq_by_prio(plic, i);

        AndesCsr *csr = &env->andes_csr;
        AndesVec *vec = &env->andes_vec;
        switch (mode) {
        case PLICMode_M:
            if (!vec->vectored_irq_m &&
                (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) &&
                (plic->feature_enable & FER_VECTORED) && irq) {

                vec->vectored_irq_m = irq;
                andes_plic_set_pending(plic, irq, false);
                andes_plic_set_claimed(plic, irq, true);

                if (plic->feature_enable & FER_PREEMPT) {
                    andes_plic_priority_push(plic, i, irq);
                }
            }
            qemu_set_irq(plic->m_external_irqs[hart_id - plic->hart_id_base],
                         irq);
            break;
        case PLICMode_S:
            if (!vec->vectored_irq_s &&
                (csr->csrno[CSR_MMISC_CTL] & (1UL << V5_MMISC_CTL_VEC_PLIC)) &&
                (plic->feature_enable & FER_VECTORED) && irq) {

                vec->vectored_irq_s = irq;
                andes_plic_set_pending(plic, irq, false);
                andes_plic_set_claimed(plic, irq, true);

                if (plic->feature_enable & FER_PREEMPT) {
                    andes_plic_priority_push(plic, i, irq);
                }
            }
            qemu_set_irq(plic->s_external_irqs[hart_id - plic->hart_id_base],
                         irq);
            break;
        default:
            break;
        }
    }
}

static void andes_plicsw_update(void *opaque)
{
    AndesPLICState *plic = ANDES_PLIC(opaque);

    /* Update external IRQ pendings for all targets */
    for (int i = 0; i < plic->num_targets; i++) {
        uint32_t hart_id = plic->target_config[i].hart_id;
        AndesPLICMode mode = plic->target_config[i].mode;
        CPUState *cpu = qemu_get_cpu(hart_id);
        CPURISCVState *env = cpu_env(cpu);
        if (!env) {
            continue;
        }
        /* If there is a pending IRQ qualified, the level is true(high) */
        bool level = !!andes_plic_determine_irq_by_prio(plic, i);

        switch (mode) {
        case PLICMode_M:
            qemu_set_irq(plic->m_external_irqs[hart_id - plic->hart_id_base],
                         level);
            break;
        case PLICMode_S:
            qemu_set_irq(plic->s_external_irqs[hart_id - plic->hart_id_base],
                         level);
            break;
        default:
            break;
        }
    }
}

static uint64_t
andes_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    AndesPLICState *plic = ANDES_PLIC(opaque);

    /* read addr must be 4 bytes aligned */
    if ((addr & 0x3) != 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register read 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }

    if (addr_between(addr, plic->priority_base, plic->num_sources * 4)) {
        /* Priority Register Block costs 4 bytes per source */
        uint32_t irq = (addr - plic->priority_base) / 4;
        /* IRQ number starts from 1 */
        irq += 1;
        return plic->source_priority[irq];
    } else if (addr_between(addr, plic->pending_base,
                            (plic->num_sources + 31) / 8)) {
        /* Pending Bit costs 1 bit per source */
        uint32_t word_idx = (addr - plic->pending_base) / 4;
        return plic->pending[word_idx];
    } else if (addr_between(addr, plic->enable_base,
                            plic->enable_stride * plic->num_targets)) {
        /* Enable Bit Register Block costs "stride" bytes per target(context) */
        uint32_t target_id = (addr - plic->enable_base) / plic->enable_stride;
        uint32_t word_idx = (addr & (plic->enable_stride - 1)) / 4;

        if (word_idx < plic->num_source_in_words) {
            return plic->enable[target_id * plic->num_source_in_words + word_idx];
        } else {
            return 0;
        }
    } else if (addr_between(addr, plic->threshold_base,
                            plic->threshold_stride * plic->num_targets)) {
        /*
        * Priority Threshold Register Block costs "stride" bytes per target.
        * Interrupt Claim Register Block occupies a byte in the same block per
        * target(context) with an offset of 0x4.
        * Preempted Priority Stack Register Block occupies 8 bytes in the same
        * block per target(context) with an offset of 0x400.
        */
        uint32_t target_id = (addr - plic->threshold_base) / plic->threshold_stride;
        uint32_t byte_idx = (addr & (plic->threshold_stride - 1));

        if (byte_idx == 0) {
            return plic->priority_threshold[target_id];
        } else if (byte_idx == 4) {
            /* read to claim an IRQ to serve */
            uint32_t highest_irq = andes_plic_determine_irq_by_prio(plic, target_id);

            if (highest_irq) {
                andes_plic_set_pending(plic, highest_irq, false);
                andes_plic_set_claimed(plic, highest_irq, true);
                if (plic->feature_enable & FER_PREEMPT) {
                    andes_plic_priority_push(plic, target_id, highest_irq);
                }
                plic->update(plic);
            }

            return highest_irq;
        } else if (byte_idx >= 0x400 && byte_idx <= 0x41c) {
            uint32_t word_idx = byte_idx - 0x400;
            if (word_idx < plic->num_priority_in_words) {
                return plic->priority_stack[target_id *
                                            plic->num_priority_in_words +
                                            word_idx];
            } else {
                return 0;
            }
        }
    } else if (addr == REG_FEATURE_ENABLE) {
        return plic->feature_enable;
    } else if (addr == REG_NUM_IRQ_TARGET) {
        return plic->num_irq_target;
    } else if (addr == REG_VER_MAX_PRIORITY) {
        return (plic->num_priorities & 0xFFFF) << 16;
    } else if (addr_between(addr, REG_TRIGGER_TYPE_BASE,
                            (plic->num_sources + 31) / 8)) {
        /* Interrupt Trigger Type Register costs 1 bit per source */
        uint32_t word_idx = (addr - REG_TRIGGER_TYPE_BASE) / 4;
        return plic->trigger_type[word_idx];
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Invalid register read 0x%" HWADDR_PRIx "\n",
                  __func__, addr);
    return 0;
}

static void
andes_plic_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AndesPLICState *plic = ANDES_PLIC(opaque);
    uint32_t val = (uint32_t)value;

    /* write addr must be 4 bytes aligned */
    if ((addr & 0x3) != 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register write 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return;
    }

    if (addr_between(addr, plic->priority_base, plic->num_sources * 4)) {
        /* Priority Register Block costs 4 bytes per source */
        uint32_t irq = (addr - plic->priority_base) / 4;
        /* IRQ number starts from 1 */
        irq += 1;

        if (val <= plic->num_priorities) {
            plic->source_priority[irq] = val;
            plic->update(plic);
        }
    } else if (addr_between(addr, plic->pending_base,
                            (plic->num_sources + 31) / 8)) {
        /* Pending Bit costs 1 bit per source */
        uint32_t word_idx = (addr - plic->pending_base) / 4;
        uint32_t xchg = plic->pending[word_idx] ^ val;
        if (xchg) {
            plic->pending[word_idx] |= val;
            plic->update(plic);
        }
    } else if (addr_between(addr, plic->enable_base,
                            plic->enable_stride * plic->num_targets)) {
        /* Enable Bit Register Block costs "stride" bytes per target(context) */
        uint32_t target_id = (addr - plic->enable_base) / plic->enable_stride;
        uint32_t word_idx = (addr & (plic->enable_stride - 1)) / 4;

        if (word_idx < plic->num_source_in_words) {
            uint32_t old = plic->enable[target_id * plic->num_source_in_words +
                                        word_idx];
            uint32_t disabled_to_enabled = (old ^ val) & val;

            /* If an IRQ is enabled from disabled, reset its latch counter */
            for (int i = 0; i < 32; i++) {
                if (disabled_to_enabled & 0x1) {
                    plic->edge_latch_cnt[word_idx * 32 + i] = 0;
                    disabled_to_enabled >>= 1;
                }
            }

            plic->enable[target_id * plic->num_source_in_words +
                         word_idx] = val;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Invalid enable write 0x%" HWADDR_PRIx "\n",
                          __func__, addr);
        }
    } else if (addr_between(addr, plic->threshold_base,
                            plic->threshold_stride * plic->num_targets)) {
        /*
        * Priority Threshold Register Block costs "stride" bytes per target.
        * Interrupt Complete Register Block occupies a byte in the same block
        * per target(context) with an offset of 0x4.
        */
        uint32_t target_id = (addr - plic->threshold_base) / plic->threshold_stride;
        uint32_t byte_idx = (addr & (plic->threshold_stride - 1));

        if (byte_idx == 0) {
            if (val <= plic->num_priorities) {
                plic->priority_threshold[target_id] = val;
                plic->update(plic);
            }
        } else if (byte_idx == 4) {
            /* write to signal the completion of IRQ serving */
            if (val < plic->num_sources) {
                uint32_t word_idx = val / 32;
                uint32_t bit_mask = 1 << (val & 31);
                uint32_t enable_word =
                    plic->enable[target_id * plic->num_source_in_words +
                                 word_idx];

                if ((!!(plic->trigger_type[word_idx] & bit_mask)) ==
                    ANDES_PLIC_TRIGGER_TYPE_LEVEL) {
                    /*
                    * Mark the level-triggered interrupt as pending if
                    * its level is still high and enabled.
                    */
                    if (plic->level[val] &&
                        (enable_word && bit_mask)) {
                        andes_plic_set_pending(plic, val, true);
                    }
                } else {
                    /*
                    * Mark the edge-triggered interrupt as pending if
                    * there are latched rising edges and it is enabled.
                    */
                    if ((plic->edge_latch_cnt[val] > 0) &&
                        (enable_word && bit_mask)) {
                        andes_plic_set_pending(plic, val, true);
                        plic->edge_latch_cnt[val]--;
                    }
                }

                if (plic->feature_enable & FER_PREEMPT) {
                    andes_plic_priority_pop(plic, target_id);
                }
                andes_plic_set_claimed(plic, val, false);
                plic->update(plic);
            }
        } else if (byte_idx >= 0x400 && byte_idx <= 0x41c) {
            uint32_t word_idx = byte_idx - 0x400;
            if (word_idx < plic->num_priority_in_words) {
                plic->priority_stack[target_id * plic->num_priority_in_words +
                                     word_idx] = val;
            }
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Invalid context write 0x%" HWADDR_PRIx "\n",
                          __func__, addr);
        }
    } else if (addr == REG_FEATURE_ENABLE) {
        plic->feature_enable = val & (FER_PREEMPT | FER_VECTORED);
    } else if (addr == REG_NUM_IRQ_TARGET || addr == REG_VER_MAX_PRIORITY) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register write at 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
    } else if (addr_between(addr, REG_TRIGGER_TYPE_BASE,
               (plic->num_sources + 31) / 8)) {
        /* Interrupt Trigger Type Register costs 1 bit per source */
#if !ANDES_PLIC_TRIGGER_TYPE_READONLY
        uint32_t word_idx = (addr - REG_TRIGGER_TYPE_BASE) / 4;
        plic->trigger_type[word_idx] = val;
#else
        qemu_log_mask(LOG_GUEST_ERROR,
            "%s: invalid trigger type write: 0x%" HWADDR_PRIx "",
            __func__, addr);
#endif
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Invalid register write 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
    }
}

static void andes_plic_irq_request(void *opaque, int irq, int level)
{
    AndesPLICState *plic = ANDES_PLIC(opaque);
    uint32_t word_idx = irq / 32;
    uint32_t bit_mask = 1 << (irq % 32);
    bool edge_triggered = !!(plic->trigger_type[word_idx] & bit_mask) ==
                          ANDES_PLIC_TRIGGER_TYPE_EDGE;

    /* Latch an rising edge and increase the latch counter */
    if (edge_triggered && !plic->level[irq] && level) {
        plic->edge_latch_cnt[irq]++;
    }

    /*
     * Keep level data for level triggered to re-generate IRQ
     * while receives complete message
     */
    plic->level[irq] = level;

    /* Check if any context has enabled the irq */
    bool irq_enabled = false;
    for (int i = 0; i < plic->num_targets; i++) {
        uint32_t enable_word = plic->enable[i * plic->num_source_in_words +
                                            word_idx];

        if (enable_word & bit_mask) {
            irq_enabled = true;
            break;
        }
    }

    if (!irq_enabled) {
        return;
    }

    if (edge_triggered) {
        /*
         * If the edge-triggered IRQ is not claimed, set the pending bit
         * of the IRQ according to the latch counter.
         * If the IRQ is claimed, the latched IRQ will be raised when the
         * claimed IRQ is completed.
         */
        if (!plic->claimed[irq]) {
            if (plic->edge_latch_cnt[irq] > 0) {
                andes_plic_set_pending(plic, irq, true);
                plic->edge_latch_cnt[irq]--;
                plic->update(plic);
            }
        }
    } else {
        /*
         * If the level-triggered IRQ is not claimed, set the pending bit
         * of the IRQ according to the level.
         * If the IRQ is claimed, an IRQ will be raised when the caaimed IRQ
         * is completed and the level remains high.
         */
        if (!plic->claimed[irq]) {
            if (level) {
                andes_plic_set_pending(plic, irq, true);
                plic->update(plic);
            }
        }
    }
}

static const MemoryRegionOps andes_plic_ops = {
    .read = andes_plic_read,
    .write = andes_plic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static void
andes_plic_realize(DeviceState *dev, Error **errp)
{
    AndesPLICState *plic = ANDES_PLIC(dev);

    memory_region_init_io(&plic->mmio, OBJECT(dev),
                          &andes_plic_ops, plic,
                          TYPE_ANDES_PLIC, plic->mmio_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &plic->mmio);

    parse_hart_config(plic);

    plic->num_source_in_words = plic->num_sources / 32 + 1;
    plic->num_priority_in_words = plic->num_priorities / 32 + 1;
    plic->num_enable_in_words = plic->num_source_in_words * plic->num_targets;
    plic->num_prio_stack_in_words = plic->num_priority_in_words *
                                    plic->num_targets;
    plic->source_priority = g_new0(uint32_t, plic->num_sources);
    plic->priority_threshold = g_new0(uint32_t, plic->num_targets);
    plic->pending = g_new0(uint32_t, plic->num_source_in_words);
    plic->enable = g_new0(uint32_t, plic->num_enable_in_words);
    plic->priority_stack = g_new0(uint32_t, plic->num_prio_stack_in_words);
    plic->trigger_type = g_new0(uint32_t, plic->num_source_in_words);
    plic->claimed = g_new0(uint8_t, plic->num_sources);
    plic->level = g_new0(uint8_t, plic->num_sources);
    plic->edge_latch_cnt = g_new0(uint8_t, plic->num_sources);

    qdev_init_gpio_in(dev, andes_plic_irq_request, plic->num_sources);

    plic->s_external_irqs = g_malloc(sizeof(qemu_irq) * plic->num_harts);
    qdev_init_gpio_out(dev, plic->s_external_irqs, plic->num_harts);

    plic->m_external_irqs = g_malloc(sizeof(qemu_irq) * plic->num_harts);
    qdev_init_gpio_out(dev, plic->m_external_irqs, plic->num_harts);

    if (strstr(plic->plic_name , "SW") != NULL) {
        plic->update = andes_plicsw_update;
    } else {
        plic->update = andes_plichw_update;
    }

    msi_nonbroken = true;
}

static const VMStateDescription vmstate_andes_plic = {
    .name = "andes_plic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
            VMSTATE_VARRAY_UINT32(source_priority, AndesPLICState,
                                  num_sources, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(priority_threshold, AndesPLICState,
                                  num_targets, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(pending, AndesPLICState,
                                  num_source_in_words, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(enable, AndesPLICState,
                                  num_enable_in_words, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(priority_stack, AndesPLICState,
                                  num_prio_stack_in_words, 0,
                                  vmstate_info_uint32, uint32_t),
            VMSTATE_VARRAY_UINT32(claimed, AndesPLICState,
                                  num_sources, 0,
                                  vmstate_info_uint8, uint8_t),
            VMSTATE_VARRAY_UINT32(level, AndesPLICState,
                                  num_sources, 0,
                                  vmstate_info_uint8, uint8_t),
            VMSTATE_VARRAY_UINT32(edge_latch_cnt, AndesPLICState,
                                  num_sources, 0,
                                  vmstate_info_uint8, uint8_t),
            VMSTATE_END_OF_LIST()
        }
};

static Property andes_plic_properties[] = {
    DEFINE_PROP_STRING("plic-name", AndesPLICState, plic_name),
    DEFINE_PROP_STRING("hart-config", AndesPLICState, hart_config),
    DEFINE_PROP_UINT32("hart-id-base", AndesPLICState, hart_id_base, 0),
    DEFINE_PROP_UINT32("num-sources", AndesPLICState, num_sources, 0),
    DEFINE_PROP_UINT32("num-priorities", AndesPLICState, num_priorities, 0),
    DEFINE_PROP_UINT32("priority-base", AndesPLICState, priority_base, 0),
    DEFINE_PROP_UINT32("pending-base", AndesPLICState, pending_base, 0),
    DEFINE_PROP_UINT32("enable-base", AndesPLICState, enable_base, 0),
    DEFINE_PROP_UINT32("enable-stride", AndesPLICState, enable_stride, 0),
    DEFINE_PROP_UINT32("threshold-base", AndesPLICState, threshold_base, 0),
    DEFINE_PROP_UINT32("threshold-stride", AndesPLICState, threshold_stride, 0),
    DEFINE_PROP_UINT32("mmio-size", AndesPLICState, mmio_size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_plic_reset(AndesPLICState *plic)
{
    memset(plic->source_priority, 0, sizeof(uint32_t) * plic->num_sources);
    memset(plic->priority_threshold, 0, sizeof(uint32_t) * plic->num_targets);
    memset(plic->pending, 0, sizeof(uint32_t) * plic->num_source_in_words);
    memset(plic->enable, 0, sizeof(uint32_t) * plic->num_enable_in_words);
    memset(plic->priority_stack, 0,
           sizeof(uint32_t) * plic->num_prio_stack_in_words);

    for (int i = 0; i < plic->num_harts; i++) {
        qemu_set_irq(plic->m_external_irqs[i], 0);
        qemu_set_irq(plic->s_external_irqs[i], 0);
    }

    memset(plic->claimed, 0, sizeof(uint8_t) * plic->num_sources);
    memset(plic->level, 0, sizeof(uint8_t) * plic->num_sources);
    memset(plic->edge_latch_cnt, 0, sizeof(uint8_t) * plic->num_sources);
    /* No reset trigger type */
}

static void andes_plic_reset_hold(Object *obj, ResetType type)
{
    AndesPLICState *andes_plic = ANDES_PLIC(obj);
    AndesPLICClass *apc = ANDES_PLIC_GET_CLASS(obj);
    andes_plic_reset(andes_plic);
    if (apc->parent_phases.hold) {
        apc->parent_phases.hold(obj, type);
    }
}

static void andes_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    AndesPLICClass *apc = ANDES_PLIC_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->vmsd = &vmstate_andes_plic;
    device_class_set_props(dc, andes_plic_properties);
    device_class_set_parent_realize(dc, andes_plic_realize,
                                    &apc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, andes_plic_reset_hold, NULL,
                                       &apc->parent_phases);
}

static const TypeInfo andes_plic_info = {
    .name          = TYPE_ANDES_PLIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AndesPLICState),
    .class_init    = andes_plic_class_init,
    .class_size    = sizeof(AndesPLICClass),
};

static void andes_plic_register_types(void)
{
    type_register_static(&andes_plic_info);
}

type_init(andes_plic_register_types)

/*
 * Create PLIC device.
 */
DeviceState *andes_plic_create(hwaddr plic_base,
    const char *plic_name, char *hart_config,
    uint32_t num_harts, uint32_t hart_id_base,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t threshold_base, uint32_t threshold_stride,
    uint32_t mmio_size)
{
    DeviceState *dev = qdev_new(TYPE_ANDES_PLIC);
    AndesPLICState *plic = ANDES_PLIC(dev);
    bool is_plic_sw = false;

    /* assert that stride values are powers of 2 and non-zero */
    assert(enable_stride == (enable_stride & -enable_stride));
    assert(threshold_stride == (threshold_stride & -threshold_stride));
    qdev_prop_set_string(dev, "plic-name", plic_name);
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "hart-id-base", hart_id_base);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "threshold-base", threshold_base);
    qdev_prop_set_uint32(dev, "threshold-stride", threshold_stride);
    qdev_prop_set_uint32(dev, "mmio-size", mmio_size);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, plic_base);

    if (strstr(plic_name, "SW") != NULL) {
        is_plic_sw = true;
    }

    plic->num_irq_target = (plic->num_targets << 16) | num_sources;

    for (int i = 0; i < plic->num_targets; i++) {
        int cpu_num = plic->target_config[i].hart_id;
        CPUState *cpu = qemu_get_cpu(cpu_num);

        if (plic->target_config[i].mode == PLICMode_M) {
            qdev_connect_gpio_out(dev, cpu_num - hart_id_base + num_harts,
                                  qdev_get_gpio_in(DEVICE(cpu),
                                  is_plic_sw ? IRQ_M_SOFT : IRQ_M_EXT));
        }
        if (plic->target_config[i].mode == PLICMode_S) {
            qdev_connect_gpio_out(dev, cpu_num - hart_id_base,
                                  qdev_get_gpio_in(DEVICE(cpu),
                                  is_plic_sw ? IRQ_S_SOFT : IRQ_S_EXT));
        }
    }

    return dev;
}
