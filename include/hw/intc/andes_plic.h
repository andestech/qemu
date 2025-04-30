/*
 * Andes PLIC (Platform Level Interrupt Controller) interface
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * This provides a RISC-V PLIC device with Andes' extensions.
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

#ifndef HW_ANDES_PLIC_H
#define HW_ANDES_PLIC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define ANDES_PLIC_NAME             "ANDES_PLIC"
#define ANDES_PLIC_HART_CONFIG      "MS"
#define ANDES_PLIC_NUM_SOURCES      128
#define ANDES_PLIC_NUM_PRIORITIES   32
#define ANDES_PLIC_PRIORITY_BASE    0x4
#define ANDES_PLIC_PENDING_BASE     0x1000
#define ANDES_PLIC_ENABLE_BASE      0x2000
#define ANDES_PLIC_ENABLE_STRIDE    0x80
#define ANDES_PLIC_THRESHOLD_BASE   0x200000
#define ANDES_PLIC_THRESHOLD_STRIDE 0x1000

#define ANDES_PLICSW_NAME             "ANDES_PLICSW"
#define ANDES_PLICSW_HART_CONFIG      "M"
#define ANDES_PLICSW_NUM_SOURCES      64
#define ANDES_PLICSW_NUM_PRIORITIES   8
#define ANDES_PLICSW_PRIORITY_BASE    0x4
#define ANDES_PLICSW_PENDING_BASE     0x1000
#define ANDES_PLICSW_ENABLE_BASE      0x2000
#define ANDES_PLICSW_ENABLE_STRIDE    0x80
#define ANDES_PLICSW_THRESHOLD_BASE   0x200000
#define ANDES_PLICSW_THRESHOLD_STRIDE 0x1000


#define TYPE_ANDES_PLIC "riscv.andes.plic"
OBJECT_DECLARE_TYPE(AndesPLICState, AndesPLICClass, ANDES_PLIC)

typedef struct AndesPLICClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
    /*< public >*/
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
} AndesPLICClass;

typedef enum AndesPLICMode {
    PLICMode_U,
    PLICMode_S,
    PLICMode_H,
    PLICMode_M
} AndesPLICMode;

typedef struct AndesPLICTarget {
    uint32_t target_id;
    uint32_t hart_id;
    AndesPLICMode mode;
} AndesPLICTarget;

typedef enum AndesPLICTriggerType {
  ANDES_PLIC_TRIGGER_TYPE_LEVEL = 0,
  ANDES_PLIC_TRIGGER_TYPE_EDGE
} AndesPLICTriggerType;

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

struct AndesPLICState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    AndesPLICTarget *target_config;
    uint32_t num_targets;
    uint32_t num_harts;

    /*
     * number of words needed to record all source-related bits
     * for example, we need 2 words to record 48 pending bits
     */
    uint32_t num_source_in_words;

    /*
     * number of words needed to record all priority bits
     * for example, we need 2 words to record 48 priority bits
     */
    uint32_t num_priority_in_words;

    /*
     * number of words needed to record all enable bits of all targets
     * we need (num_source_in_words * num_targets) words
     */
    uint32_t num_enable_in_words;

    /*
     * number of words needed to record all preemptive priority stack bits
     * we need (num_priority_in_words * num_targets) words
     */
    uint32_t num_prio_stack_in_words;

    /* source_priority[source_id], one word per source */
    uint32_t *source_priority;

    /* priority_threshold[target_id], one word per target */
    uint32_t *priority_threshold;

    /* pending[word_idx], one bit per source */
    uint32_t *pending;

    /* enable[target_id * num_source_in_words + word_idx], one bit per source */
    uint32_t *enable;

    /*
     * priority_stack[target_id * num_priority_in_words + word_idx],
     * one bit per priority
     */
    uint32_t *priority_stack;

    /* trigger_type[word_idx], one bit per source */
    uint32_t *trigger_type;

    uint32_t feature_enable;
    uint32_t num_irq_target;

    /* Internal claimed[source_id], one byte per source */
    uint8_t *claimed;

    /*
     * Internal level[source_id], one byte per source.
     * Latched the latest signal level in the gateway
     */
    uint8_t *level;

    /*
     * Internal edge_latch_cnt[source_id], one byte per source.
     * A counter to record latched raising edges.
     */
    uint8_t *edge_latch_cnt;

    /* config */
    char *plic_name;
    char *hart_config;
    uint32_t hart_id_base;
    uint32_t num_sources;
    uint32_t num_priorities;
    uint32_t priority_base;
    uint32_t pending_base;
    uint32_t enable_base;
    uint32_t enable_stride;
    uint32_t threshold_base;
    uint32_t threshold_stride;
    uint32_t mmio_size;

    qemu_irq *m_external_irqs;
    qemu_irq *s_external_irqs;

    void (*update)(void *opaque);
};

DeviceState *
andes_plic_create(hwaddr addr,
    const char *plic_name, char *hart_config,
    uint32_t num_harts, uint32_t hartid_base,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t threshold_base, uint32_t threshold_stride,
    uint32_t mmio_size);

#endif /* HW_ANDES_PLIC_H */
