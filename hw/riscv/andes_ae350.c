/*
 * Andes RISC-V AE350 Board
 *
 * Copyright (c) 2021 Andes Tech. Corp.
 *
 * Andes AE350 Board supports ns16550a UART and VirtIO MMIO.
 * The interrupt controllers are andes PLIC and andes PLICSW.
 * Timer is Andes PLMT.
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
#include "qemu/units.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/nmi.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/char/serial-mm.h"
#include "hw/misc/unimp.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/numa.h"
#include "kvm/kvm_riscv.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "hw/pci/pci.h"
#include "hw/pci-host/gpex.h"
#include "elf.h"

#include "hw/intc/andes_plic.h"
#include "hw/timer/andes_plmt.h"
#include "hw/timer/atcpit100.h"
#include "hw/riscv/andes_ae350.h"
#include "hw/misc/atcbmc200.h"
#include "hw/misc/atcbmc300.h"
#include "hw/misc/andes_atcsmu.h"
#include "hw/sd/atfsdc010.h"
#include "hw/rtc/atcrtc100.h"
#include "hw/watchdog/atcwdt200.h"
#include "hw/misc/atciopmp300.h"
#include "hw/misc/atciopmp200.h"
#include "hw/misc/atciopmp100.h"
#include "hw/misc/riscv_iopmp_dispatcher.h"
#include "hw/intc/riscv_aplic.h"
#include "sysemu/tcg.h"
#include "sysemu/kvm.h"

#define BIOS_FILENAME ""

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} andes_ae350_memmap[] = {
    [ANDES_AE350_DRAM]              = { 0x00000000,   0x80000000 },
    [ANDES_AE350_MROM]              = { 0x80000000,    0x8000000 },
    [ANDES_AE350_NOR]               = { 0x88000000,    0x4000000 },
    [ANDES_AE350_SUBPORT0_ILM]      = { 0xa0000000,     0x200000 },
    [ANDES_AE350_SUBPORT0_DLM]      = { 0xa0200000,     0x200000 },
    [ANDES_AE350_SUBPORT1_ILM]      = { 0xa0400000,     0x200000 },
    [ANDES_AE350_SUBPORT1_DLM]      = { 0xa0600000,     0x200000 },
    [ANDES_AE350_SUBPORT2_ILM]      = { 0xa0800000,     0x200000 },
    [ANDES_AE350_SUBPORT2_DLM]      = { 0xa0a00000,     0x200000 },
    [ANDES_AE350_SUBPORT3_ILM]      = { 0xa0c00000,     0x200000 },
    [ANDES_AE350_SUBPORT3_DLM]      = { 0xa0e00000,     0x200000 },
    [ANDES_AE350_SUBPORT4_ILM]      = { 0xa1000000,     0x200000 },
    [ANDES_AE350_SUBPORT4_DLM]      = { 0xa1200000,     0x200000 },
    [ANDES_AE350_SUBPORT5_ILM]      = { 0xa1400000,     0x200000 },
    [ANDES_AE350_SUBPORT5_DLM]      = { 0xa1600000,     0x200000 },
    [ANDES_AE350_SUBPORT6_ILM]      = { 0xa1800000,     0x200000 },
    [ANDES_AE350_SUBPORT6_DLM]      = { 0xa1a00000,     0x200000 },
    [ANDES_AE350_SUBPORT7_ILM]      = { 0xa1c00000,     0x200000 },
    [ANDES_AE350_SUBPORT7_DLM]      = { 0xa1e00000,     0x200000 },
    [ANDES_AE350_BMC]               = { 0xc0000000,     0x100000 },
    [ANDES_AE350_IMSIC_M]           = { 0xc4000000,     0x100000 },
    [ANDES_AE350_IMSIC_S]           = { 0xc4100000,     0x100000 },
    [ANDES_AE350_MAC]               = { 0xe0100000,     0x100000 },
    [ANDES_AE350_LCD]               = { 0xe0200000,     0x100000 },
    [ANDES_AE350_SMC]               = { 0xe0400000,     0x100000 },
    [ANDES_AE350_L2C]               = { 0xe0500000,     0x100000 },
    [ANDES_AE350_CLIC]              = { 0xe2000000,    0x2000000 },
    [ANDES_AE350_PLIC]              = { 0xe4000000,     0x400000 },
    [ANDES_AE350_APLIC]             = { 0xe4400000,     0x100000 },
    [ANDES_AE350_PLMT]              = { 0xe6000000,     0x100000 },
    [ANDES_AE350_PLICSW]            = { 0xe6400000,     0x400000 },
    [ANDES_AE350_SMU]               = { 0xf0100000,     0x100000 },
    [ANDES_AE350_UART1]             = { 0xf0200000,     0x100000 },
    [ANDES_AE350_UART2]             = { 0xf0300000,     0x100000 },
    [ANDES_AE350_PIT]               = { 0xf0400000,     0x100000 },
    [ANDES_AE350_WDT]               = { 0xf0500000,     0x100000 },
    [ANDES_AE350_RTC]               = { 0xf0600000,     0x100000 },
    [ANDES_AE350_GPIO]              = { 0xf0700000,     0x100000 },
    [ANDES_AE350_I2C]               = { 0xf0a00000,     0x100000 },
    [ANDES_AE350_SPI]               = { 0xf0b00000,     0x100000 },
    [ANDES_AE350_DMAC]              = { 0xf0c00000,     0x100000 },
    [ANDES_AE350_SND]               = { 0xf0d00000,     0x100000 },
    [ANDES_AE350_SDC]               = { 0xf0e00000,     0x100000 },
    [ANDES_AE350_SPI2]              = { 0xf0f00000,     0x100000 },
    [ANDES_AE350_IOPMP_APB]         = { 0xf1000000,       0x4000 },
    [ANDES_AE350_IOPMP_RAM]         = { 0xf1004000,       0x4000 },
    [ANDES_AE350_IOPMP_SLP]         = { 0xf1008000,       0x4000 },
    [ANDES_AE350_IOPMP_ROM]         = { 0xf100c000,       0x4000 },
    [ANDES_AE350_IOPMP_IOCP]        = { 0xf1010000,       0x4000 },
    [ANDES_AE350_IOPMP_DFS]         = { 0xf1014000,       0x4000 },
    [ANDES_AE350_IOPMP_ILMAP]       = { 0xf1018000,       0x4000 },
    [ANDES_AE350_IOPMP_DLMAP]       = { 0xf101c000,       0x4000 },
    [ANDES_AE350_IOPMP100]          = { 0xf1024000,       0x4000 },
    [ANDES_AE350_UART3]             = { 0xf1100000,     0x100000 },
    [ANDES_AE350_DTROM]             = { 0xf2000000,     0x100000 },
    [ANDES_AE350_VIRTIO]            = { 0xfe000000,       0x1000 },
    [ANDES_AE350_UNCACHEABLE_ALIAS] = { 0x100000000, 0x100000000 },
};

const int iopmp300_memmap_idx_list[AE350_IOPMP300_NUM] = {
    ANDES_AE350_IOPMP_APB, ANDES_AE350_IOPMP_RAM, ANDES_AE350_IOPMP_SLP,
    ANDES_AE350_IOPMP_ROM, ANDES_AE350_IOPMP_IOCP, ANDES_AE350_IOPMP_DFS
};
const int iopmp200_memmap_idx_list[AE350_IOPMP200_NUM] = {
    ANDES_AE350_IOPMP_APB, ANDES_AE350_IOPMP_RAM, ANDES_AE350_IOPMP_ROM,
    ANDES_AE350_IOPMP_ILMAP, ANDES_AE350_IOPMP_DLMAP
};

/* KVM AIA only supports APLIC MSI. APLIC Wired is always emulated by QEMU. */
static bool andes_ae350_use_kvm_aia(AndesAe350BoardState *s)
{
    return kvm_irqchip_in_kernel() &&
           s->aia_type == ANDES_AE350_AIA_TYPE_APLIC_IMSIC;
}

static void create_fdt_iopmp(AndesAe350BoardState *bs,
                             const struct MemmapEntry *memmap,
                             uint32_t irq_mmio_phandle)
{
    g_autofree char *name = NULL;
    MachineState *ms = MACHINE(bs);
    const struct MemmapEntry *entry;

    if (bs->soc.has_iopmp300) {
        for (int i = 0; i < AE350_IOPMP300_NUM; i++) {
            entry = &memmap[iopmp300_memmap_idx_list[i]];
            name = g_strdup_printf("/soc/atciopmp300@%lx", (long)entry->base);
            qemu_fdt_add_subnode(ms->fdt, name);
            qemu_fdt_setprop_string(ms->fdt, name, "compatible", "atciopmp300");
            qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0, entry->base,
                                   0x0, entry->size);
            qemu_fdt_setprop_cell(ms->fdt, name, "interrupt-parent",
                                  irq_mmio_phandle);
            qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                                   ANDES_AE350_IOPMP_IRQ, 0x4);
        }
    } else if (bs->soc.has_iopmp200) {
        for (int i = 0; i < AE350_IOPMP200_NUM; i++) {
            entry = &memmap[iopmp200_memmap_idx_list[i]];
            name = g_strdup_printf("/soc/atciopmp200@%lx", (long)entry->base);
            qemu_fdt_add_subnode(ms->fdt, name);
            qemu_fdt_setprop_string(ms->fdt, name, "compatible", "atciopmp200");
            qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0, entry->base,
                                   0x0, entry->size);
            qemu_fdt_setprop_cell(ms->fdt, name, "interrupt-parent",
                                  irq_mmio_phandle);
            qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                                   ANDES_AE350_IOPMP_IRQ, 0x4);
        }
    }
    if (bs->soc.has_iopmp100) {
        entry = &memmap[ANDES_AE350_IOPMP100];
        name = g_strdup_printf("/soc/atciopmp100@%lx", (long)entry->base);
        qemu_fdt_add_subnode(ms->fdt, name);
        qemu_fdt_setprop_string(ms->fdt, name, "compatible", "atciopmp100");
        qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0, entry->base,
                               0x0, entry->size);
    }
}

static uint32_t imsic_num_bits(uint32_t count)
{
    uint32_t ret = 0;

    while (BIT(ret) < count) {
        ret++;
    }

    return ret;
}

static void create_fdt_one_imsic(AndesAe350BoardState *bs, hwaddr base_addr,
                                 uint32_t *intc_phandles, uint32_t msi_phandle,
                                 bool m_mode, uint32_t imsic_guest_bits)
{
    int cpu, socket;
    g_autofree char *imsic_name = NULL;
    MachineState *ms = MACHINE(bs);
    AndesAe350SocState *s = &bs->soc;
    int socket_count = riscv_socket_count(ms);
    uint32_t imsic_max_hart_per_socket, imsic_addr, imsic_size;
    g_autofree uint32_t *imsic_cells = NULL;
    g_autofree uint32_t *imsic_regs = NULL;
    static const char * const imsic_compat[2] = {
        "qemu,imsics", "riscv,imsics"
    };

    imsic_cells = g_new0(uint32_t, ms->smp.cpus * 2);
    imsic_regs = g_new0(uint32_t, socket_count * 4);

    for (cpu = 0; cpu < ms->smp.cpus; cpu++) {
        imsic_cells[cpu * 2 + 0] = cpu_to_be32(intc_phandles[cpu]);
        imsic_cells[cpu * 2 + 1] = cpu_to_be32(m_mode ? IRQ_M_EXT : IRQ_S_EXT);
    }

    imsic_max_hart_per_socket = 0;
    for (socket = 0; socket < socket_count; socket++) {
        imsic_addr = base_addr + socket * ANDES_AE350_IMSIC_GROUP_MAX_SIZE;
        imsic_size = IMSIC_HART_SIZE(imsic_guest_bits) *
                     s->cpus.num_harts;
        imsic_regs[socket * 4 + 0] = 0;
        imsic_regs[socket * 4 + 1] = cpu_to_be32(imsic_addr);
        imsic_regs[socket * 4 + 2] = 0;
        imsic_regs[socket * 4 + 3] = cpu_to_be32(imsic_size);
        if (imsic_max_hart_per_socket < s->cpus.num_harts) {
            imsic_max_hart_per_socket = s->cpus.num_harts;
        }
    }

    imsic_name = g_strdup_printf("/soc/interrupt-controller@%lx",
                                 (unsigned long)base_addr);
    qemu_fdt_add_subnode(ms->fdt, imsic_name);
    qemu_fdt_setprop_string_array(ms->fdt, imsic_name, "compatible",
                                  (char **)&imsic_compat,
                                  ARRAY_SIZE(imsic_compat));

    qemu_fdt_setprop_cell(ms->fdt, imsic_name, "#interrupt-cells",
                          0);
    qemu_fdt_setprop(ms->fdt, imsic_name, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop(ms->fdt, imsic_name, "msi-controller", NULL, 0);
    qemu_fdt_setprop(ms->fdt, imsic_name, "interrupts-extended",
                     imsic_cells, ms->smp.cpus * sizeof(uint32_t) * 2);
    qemu_fdt_setprop(ms->fdt, imsic_name, "reg", imsic_regs,
                     socket_count * sizeof(uint32_t) * 4);
    qemu_fdt_setprop_cell(ms->fdt, imsic_name, "riscv,num-ids",
                     ANDES_AE350_IRQCHIP_NUM_MSIS);

    if (imsic_guest_bits) {
        qemu_fdt_setprop_cell(ms->fdt, imsic_name, "riscv,guest-index-bits",
                              imsic_guest_bits);
    }

    if (socket_count > 1) {
        qemu_fdt_setprop_cell(ms->fdt, imsic_name, "riscv,hart-index-bits",
                              imsic_num_bits(imsic_max_hart_per_socket));
        qemu_fdt_setprop_cell(ms->fdt, imsic_name, "riscv,group-index-bits",
                              imsic_num_bits(socket_count));
        qemu_fdt_setprop_cell(ms->fdt, imsic_name, "riscv,group-index-shift",
                              IMSIC_MMIO_GROUP_MIN_SHIFT);
    }
    qemu_fdt_setprop_cell(ms->fdt, imsic_name, "phandle", msi_phandle);
}

static void create_fdt_imsic(AndesAe350BoardState *s,
                             const struct MemmapEntry *memmap,
                             uint32_t *phandle, uint32_t *intc_phandles,
                             uint32_t *msi_m_phandle, uint32_t *msi_s_phandle)
{
    *msi_m_phandle = (*phandle)++;
    *msi_s_phandle = (*phandle)++;

    if (!kvm_enabled()) {
        /* M-level IMSIC node */
        create_fdt_one_imsic(s, memmap[ANDES_AE350_IMSIC_M].base,
                             intc_phandles, *msi_m_phandle, true, 0);
    }

    /* S-level IMSIC node */
    create_fdt_one_imsic(s, memmap[ANDES_AE350_IMSIC_S].base, intc_phandles,
                         *msi_s_phandle, false,
                         imsic_num_bits(s->aia_guests + 1));

}

/* Caller must free string after use */
static char *fdt_get_aplic_nodename(unsigned long aplic_addr)
{
    return g_strdup_printf("/soc/interrupt-controller@%lx", aplic_addr);
}

static void create_fdt_one_aplic(AndesAe350BoardState *s, int socket,
                                 unsigned long aplic_addr, uint32_t aplic_size,
                                 uint32_t msi_phandle,
                                 uint32_t *intc_phandles,
                                 uint32_t aplic_phandle,
                                 uint32_t aplic_child_phandle,
                                 bool m_mode, int num_harts)
{
    int cpu;
    g_autofree char *aplic_name = fdt_get_aplic_nodename(aplic_addr);
    g_autofree uint32_t *aplic_cells = g_new0(uint32_t, num_harts * 2);
    MachineState *ms = MACHINE(s);
    static const char * const aplic_compat[2] = {
        "qemu,aplic", "riscv,aplic"
    };

    for (cpu = 0; cpu < num_harts; cpu++) {
        aplic_cells[cpu * 2 + 0] = cpu_to_be32(intc_phandles[cpu]);
        aplic_cells[cpu * 2 + 1] = cpu_to_be32(m_mode ? IRQ_M_EXT : IRQ_S_EXT);
    }

    qemu_fdt_add_subnode(ms->fdt, aplic_name);
    qemu_fdt_setprop_string_array(ms->fdt, aplic_name, "compatible",
                                  (char **)&aplic_compat,
                                  ARRAY_SIZE(aplic_compat));
    qemu_fdt_setprop_cell(ms->fdt, aplic_name, "#address-cells", 0);
    qemu_fdt_setprop_cell(ms->fdt, aplic_name,
                          "#interrupt-cells", 2);
    qemu_fdt_setprop(ms->fdt, aplic_name, "interrupt-controller", NULL, 0);

    if (s->aia_type == ANDES_AE350_AIA_TYPE_APLIC) {
        qemu_fdt_setprop(ms->fdt, aplic_name, "interrupts-extended",
                         aplic_cells, num_harts * sizeof(uint32_t) * 2);
    } else {
        qemu_fdt_setprop_cell(ms->fdt, aplic_name, "msi-parent", msi_phandle);
    }

    qemu_fdt_setprop_cells(ms->fdt, aplic_name, "reg",
                           0x0, aplic_addr, 0x0, aplic_size);
    qemu_fdt_setprop_cell(ms->fdt, aplic_name, "riscv,num-sources",
                          ANDES_AE350_IRQCHIP_NUM_SOURCES);

    if (aplic_child_phandle) {
        qemu_fdt_setprop_cell(ms->fdt, aplic_name, "riscv,children",
                              aplic_child_phandle);
        qemu_fdt_setprop_cells(ms->fdt, aplic_name, "riscv,delegation",
                               aplic_child_phandle, 0x1,
                               ANDES_AE350_IRQCHIP_NUM_SOURCES);
        /*
         * DEPRECATED_9.1: Compat property kept temporarily
         * to allow old firmwares to work with AIA. Do *not*
         * use 'riscv,delegate' in new code: use
         * 'riscv,delegation' instead.
         */
        qemu_fdt_setprop_cells(ms->fdt, aplic_name, "riscv,delegate",
                               aplic_child_phandle, 0x1,
                               ANDES_AE350_IRQCHIP_NUM_SOURCES);
    }

    riscv_socket_fdt_write_id(ms, aplic_name, socket);
    qemu_fdt_setprop_cell(ms->fdt, aplic_name, "phandle", aplic_phandle);
}

static void create_fdt_socket_aplic(AndesAe350BoardState *s,
                                    const struct MemmapEntry *memmap,
                                    int socket,
                                    uint32_t msi_m_phandle,
                                    uint32_t msi_s_phandle,
                                    uint32_t *phandle,
                                    uint32_t *intc_phandles,
                                    uint32_t *aplic_phandles,
                                    int num_harts)
{
    unsigned long aplic_addr;
    uint32_t aplic_m_phandle, aplic_s_phandle;
    uint32_t phandle_bk = *phandle;
    uint32_t i;

    if (!kvm_enabled()) {
        /* M-level APLIC node */
        for (i = 0; i < ANDES_AE350_APLIC_M_DOMAINS; i++) {
            aplic_m_phandle = (*phandle)++;
            aplic_s_phandle = (*phandle)++;
            aplic_addr = memmap[ANDES_AE350_APLIC].base +
                ANDES_AE350_APLIC_M_BASE + (ANDES_AE350_APLIC_STRIDE * i);
            create_fdt_one_aplic(s, socket, aplic_addr,
                                 ANDES_AE350_APLIC_SIZE_PER_DOMAIN,
                                 msi_m_phandle, intc_phandles,
                                 aplic_m_phandle, aplic_s_phandle,
                                 true, num_harts);
        }
    }

    *phandle = phandle_bk;
    /* S-level APLIC node */
    for (i = 0; i < ANDES_AE350_APLIC_M_DOMAINS; i++) {
        aplic_m_phandle = (*phandle)++;
        aplic_s_phandle = (*phandle)++;
        aplic_addr = memmap[ANDES_AE350_APLIC].base +
            ANDES_AE350_APLIC_S_BASE + (ANDES_AE350_APLIC_STRIDE * i);
        create_fdt_one_aplic(s, socket, aplic_addr,
                         ANDES_AE350_APLIC_SIZE_PER_DOMAIN,
                         msi_s_phandle, intc_phandles,
                         aplic_s_phandle, 0,
                         false, num_harts);
    }


    aplic_phandles[socket] = aplic_s_phandle;
}

static void
create_fdt(AndesAe350BoardState *bs, const struct MemmapEntry *memmap,
    uint64_t mem_size)
{
    AndesAe350SocState *s = &bs->soc;
    MachineState *ms = MACHINE(bs);
    void *fdt;
    int cpu, i;
    uint64_t mem_addr;
    uint32_t *plic_irq_ext, *plicsw_irq_ext, *plmt_irq_ext;
    unsigned long plic_addr, plicsw_addr, plmt_addr;
    char *plic_name, *plicsw_name, *plmt_name;
    uint32_t intc_phandle = 0, plic_phandle = 0;
    uint32_t phandle = 1;
    char *isa_name, *mem_name, *cpu_name, *intc_name, *uart_name, *virtio_name;
    uint32_t msi_m_phandle = 0, msi_s_phandle = 0;
    int phandle_pos;
    g_autofree uint32_t *intc_phandles = NULL;

    intc_phandles = g_new0(uint32_t, ms->smp.cpus);

    fdt = ms->fdt = create_device_tree(&bs->fdt_size);
    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    qemu_fdt_setprop_string(fdt, "/", "model", "Andes AE350 Board");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "andestech,ae350");
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "timebase-frequency",
                          ANDES_PLMT_TIMEBASE_FREQ);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_add_subnode(fdt, "/cpus/cpu-map");

    plic_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 4);
    plicsw_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 2);
    plmt_irq_ext = g_new0(uint32_t, s->cpus.num_harts * 2);

    for (cpu = 0; cpu < s->cpus.num_harts; cpu++) {
        intc_phandle = phandle++;

        cpu_name = g_strdup_printf("/cpus/cpu@%d",
            s->cpus.hartid_base + cpu);
        qemu_fdt_add_subnode(fdt, cpu_name);
#if defined(TARGET_RISCV32)
        qemu_fdt_setprop_string(fdt, cpu_name, "mmu-type", "riscv,sv32");
#else
        qemu_fdt_setprop_string(fdt, cpu_name, "mmu-type", "riscv,sv39");
#endif
        isa_name = riscv_isa_string(&s->cpus.harts[cpu]);
        qemu_fdt_setprop_string(fdt, cpu_name, "riscv,isa", isa_name);
        g_free(isa_name);
        qemu_fdt_setprop_string(fdt, cpu_name, "compatible", "riscv");
        qemu_fdt_setprop_string(fdt, cpu_name, "status", "okay");
        qemu_fdt_setprop_cell(fdt, cpu_name, "reg",
            s->cpus.hartid_base + cpu);
        qemu_fdt_setprop_string(fdt, cpu_name, "device_type", "cpu");

        intc_name = g_strdup_printf("%s/interrupt-controller", cpu_name);
        qemu_fdt_add_subnode(fdt, intc_name);
        qemu_fdt_setprop_cell(fdt, intc_name, "phandle", intc_phandle);
        qemu_fdt_setprop_string(fdt, intc_name, "compatible",
            "riscv,cpu-intc");
        qemu_fdt_setprop(fdt, intc_name, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(fdt, intc_name, "#interrupt-cells", 1);

        if (bs->aia_type == ANDES_AE350_AIA_TYPE_NONE) {
            plic_irq_ext[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
            plic_irq_ext[cpu * 4 + 1] = cpu_to_be32(IRQ_M_EXT);
            plic_irq_ext[cpu * 4 + 2] = cpu_to_be32(intc_phandle);
            plic_irq_ext[cpu * 4 + 3] = cpu_to_be32(IRQ_S_EXT);
        }

        if (bs->aia_type != ANDES_AE350_AIA_TYPE_APLIC_IMSIC) {
            plicsw_irq_ext[cpu * 2 + 0] = cpu_to_be32(intc_phandle);
            plicsw_irq_ext[cpu * 2 + 1] = cpu_to_be32(IRQ_M_SOFT);
        } else {
            intc_phandles[cpu] = intc_phandle;
        }

        plmt_irq_ext[cpu * 2 + 0] = cpu_to_be32(intc_phandle);
        plmt_irq_ext[cpu * 2 + 1] = cpu_to_be32(IRQ_M_TIMER);

        g_free(intc_name);
    }

    if (bs->aia_type == ANDES_AE350_AIA_TYPE_APLIC_IMSIC) {
        create_fdt_imsic(bs, memmap, &phandle, intc_phandles,
                         &msi_m_phandle, &msi_s_phandle);
    }

    /* KVM AIA only has one APLIC instance */
    /* AE350 only pass 1 socket(idx=0), and one share plic_phandle pointer */
    if (kvm_enabled() && andes_ae350_use_kvm_aia(bs)) {
        create_fdt_socket_aplic(bs, memmap, 0,
                                msi_m_phandle, msi_s_phandle, &phandle,
                                &intc_phandles[0], &plic_phandle,
                                ms->smp.cpus);
    } else if (bs->aia_type != ANDES_AE350_AIA_TYPE_NONE) {
        phandle_pos = ms->smp.cpus;
        phandle_pos -= s->cpus.num_harts;
        create_fdt_socket_aplic(bs, memmap, 0,
                                msi_m_phandle, msi_s_phandle, &phandle,
                                &intc_phandles[phandle_pos],
                                &plic_phandle,
                                s->cpus.num_harts);
    }

    mem_addr = memmap[ANDES_AE350_DRAM].base;
    mem_name = g_strdup_printf("/memory@%lx", (long)mem_addr);
    qemu_fdt_add_subnode(fdt, mem_name);
    qemu_fdt_setprop_cells(fdt, mem_name, "reg",
        mem_addr >> 32, mem_addr, mem_size >> 32, mem_size);
    qemu_fdt_setprop_string(fdt, mem_name, "device_type", "memory");
    g_free(mem_name);

    /* If AIA type is set to none, use PLIC */
    if (bs->aia_type == ANDES_AE350_AIA_TYPE_NONE) {
        /* create plic */
        plic_phandle = phandle++;
        plic_addr = memmap[ANDES_AE350_PLIC].base;
        plic_name = g_strdup_printf("/soc/interrupt-controller@%lx",
                                    (long)plic_addr);
        qemu_fdt_add_subnode(fdt, plic_name);
        qemu_fdt_setprop_cell(fdt, plic_name, "#address-cells", 0x2);
        qemu_fdt_setprop_cell(fdt, plic_name, "#interrupt-cells", 0x2);
        qemu_fdt_setprop_string(fdt, plic_name, "compatible", "riscv,plic0");
        qemu_fdt_setprop(fdt, plic_name, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop(fdt, plic_name, "interrupts-extended",
            plic_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 4);
        qemu_fdt_setprop_cells(fdt, plic_name, "reg",
            0x0, plic_addr, 0x0, memmap[ANDES_AE350_PLIC].size);
        qemu_fdt_setprop_cell(fdt, plic_name, "riscv,ndev", 0x47);
        qemu_fdt_setprop_cell(fdt, plic_name, "phandle", plic_phandle);
        g_free(plic_name);
        g_free(plic_irq_ext);
    }

    /* If AIA type is set to NONE or APLIC only, use PLIC_SW */
    if (bs->aia_type != ANDES_AE350_AIA_TYPE_APLIC_IMSIC) {
        /* create plicsw */
        plicsw_addr = memmap[ANDES_AE350_PLICSW].base;
        plicsw_name = g_strdup_printf("/soc/interrupt-controller@%lx",
            (long)plicsw_addr);
        qemu_fdt_add_subnode(fdt, plicsw_name);
        qemu_fdt_setprop_cell(fdt, plicsw_name, "#address-cells", 0x2);
        qemu_fdt_setprop_cell(fdt, plicsw_name, "#interrupt-cells", 0x2);
        qemu_fdt_setprop_string(fdt, plicsw_name,
                                "compatible", "andestech,plicsw");
        qemu_fdt_setprop(fdt, plicsw_name, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop(fdt, plicsw_name, "interrupts-extended",
            plicsw_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 2);
        qemu_fdt_setprop_cells(fdt, plicsw_name, "reg",
            0x0, plicsw_addr, 0x0, memmap[ANDES_AE350_PLICSW].size);
        qemu_fdt_setprop_cell(fdt, plicsw_name, "riscv,ndev", 0x1);
        g_free(plicsw_name);
        g_free(plicsw_irq_ext);
    }

    /* create plmt */
    plmt_addr = memmap[ANDES_AE350_PLMT].base;
    plmt_name = g_strdup_printf("/soc/plmt0@%lx", (long)plmt_addr);
    qemu_fdt_add_subnode(fdt, plmt_name);
    qemu_fdt_setprop_string(fdt, plmt_name, "compatible", "andestech,plmt0");
    qemu_fdt_setprop(fdt, plmt_name, "interrupts-extended",
        plmt_irq_ext, s->cpus.num_harts * sizeof(uint32_t) * 2);
    qemu_fdt_setprop_cells(fdt, plmt_name, "reg",
        0x0, plmt_addr, 0x0, memmap[ANDES_AE350_PLMT].size);
    g_free(plmt_name);
    g_free(plmt_irq_ext);

    uart_name = g_strdup_printf("/serial@%lx",
                                (long)memmap[ANDES_AE350_UART1].base);
    qemu_fdt_add_subnode(fdt, uart_name);
    qemu_fdt_setprop_string(fdt, uart_name, "compatible", "ns16550a");
    qemu_fdt_setprop_cells(fdt, uart_name, "reg",
        0x0, memmap[ANDES_AE350_UART1].base,
        0x0, memmap[ANDES_AE350_UART1].size);
    qemu_fdt_setprop_cell(fdt, uart_name, "clock-frequency", 3686400);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-shift", ANDES_UART_REG_SHIFT);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-offset", ANDES_UART_REG_OFFSET);
    qemu_fdt_setprop_cell(fdt, uart_name, "interrupt-parent", plic_phandle);
    qemu_fdt_setprop_cells(fdt, uart_name, "interrupts",
                            ANDES_AE350_UART1_IRQ, 0x4);

    uart_name = g_strdup_printf("/serial@%lx",
                                (long)memmap[ANDES_AE350_UART2].base);
    qemu_fdt_add_subnode(fdt, uart_name);
    qemu_fdt_setprop_string(fdt, uart_name, "compatible", "ns16550a");
    qemu_fdt_setprop_cells(fdt, uart_name, "reg",
        0x0, memmap[ANDES_AE350_UART2].base,
        0x0, memmap[ANDES_AE350_UART2].size);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-shift", ANDES_UART_REG_SHIFT);
    qemu_fdt_setprop_cell(fdt, uart_name, "reg-offset", ANDES_UART_REG_OFFSET);
    qemu_fdt_setprop_cell(fdt, uart_name, "clock-frequency", 3686400);
    qemu_fdt_setprop_cell(fdt, uart_name, "interrupt-parent", plic_phandle);
    qemu_fdt_setprop_cells(fdt, uart_name, "interrupts",
                            ANDES_AE350_UART2_IRQ, 0x4);

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
            "console=ttyS0,38400n8 earlycon=sbi debug loglevel=7");
    qemu_fdt_setprop_string(fdt, "/chosen", "stdout-path", uart_name);
    g_free(uart_name);

    for (i = 0; i < ANDES_AE350_VIRTIO_COUNT; i++) {
        virtio_name = g_strdup_printf("/virtio_mmio@%lx",
            (long)(memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size)));
        qemu_fdt_add_subnode(fdt, virtio_name);
        qemu_fdt_setprop_string(fdt, virtio_name, "compatible", "virtio,mmio");
        qemu_fdt_setprop_cells(fdt, virtio_name, "reg",
            0x0,
            memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size),
            0x0,
            memmap[ANDES_AE350_VIRTIO].size);
        qemu_fdt_setprop_cell(fdt, virtio_name, "interrupt-parent",
                                plic_phandle);
        qemu_fdt_setprop_cells(fdt, virtio_name, "interrupts",
                                ANDES_AE350_VIRTIO_IRQ + i, 0x4);
        g_free(virtio_name);
    }

    create_fdt_iopmp(bs, memmap, plic_phandle);
}

static DeviceState *andes_ae350_create_aia(AndesAe350AIAType aia_type,
                                           int aia_guests,
                                           const struct MemmapEntry *memmap,
                                           int socket, int base_hartid,
                                           int hart_count)
{
    int i;
    hwaddr addr;
    unsigned long aplic_addr;
    uint32_t guest_bits;
    DeviceState *aplic_ms[ANDES_AE350_APLIC_M_DOMAINS];
    DeviceState *aplic_ss[ANDES_AE350_APLIC_S_DOMAINS];
    bool msimode = aia_type == ANDES_AE350_AIA_TYPE_APLIC_IMSIC;

    if (msimode) {
        if (!kvm_enabled()) {
            /* Per-socket M-level IMSICs */
            addr = memmap[ANDES_AE350_IMSIC_M].base +
                   socket * ANDES_AE350_IMSIC_GROUP_MAX_SIZE;
            for (i = 0; i < hart_count; i++) {
                riscv_imsic_create(addr + i * IMSIC_HART_SIZE(0),
                                   base_hartid + i, true, 1,
                                   ANDES_AE350_IRQCHIP_NUM_MSIS);
            }
        }

        /* Per-socket S-level IMSICs */
        guest_bits = imsic_num_bits(aia_guests + 1);
        addr = memmap[ANDES_AE350_IMSIC_S].base +
               socket * ANDES_AE350_IMSIC_GROUP_MAX_SIZE;
        for (i = 0; i < hart_count; i++) {
            riscv_imsic_create(addr + i * IMSIC_HART_SIZE(guest_bits),
                               base_hartid + i, false, 1 + aia_guests,
                               ANDES_AE350_IRQCHIP_NUM_MSIS);
        }
    }

    if (!kvm_enabled()) {
        /*
         * First M-level parent is NULL
         * Second~N M-level parent is Root domain (Level 0 domain)
         */
        for (i = 0; i < ANDES_AE350_APLIC_M_DOMAINS; i++) {
            aplic_addr = memmap[ANDES_AE350_APLIC].base +
                ANDES_AE350_APLIC_M_BASE + (ANDES_AE350_APLIC_STRIDE * i);
            aplic_ms[i] = riscv_aplic_create(aplic_addr,
                                         ANDES_AE350_APLIC_SIZE_PER_DOMAIN,
                                         (msimode) ? 0 : base_hartid,
                                         (msimode) ? 0 : hart_count,
                                         ANDES_AE350_IRQCHIP_NUM_SOURCES,
                                         ANDES_AE350_IRQCHIP_NUM_PRIO_BITS,
                                         msimode, true,
                                         i == 0 ? NULL : aplic_ms[0]);
#if ANDES_AE350_MSICFGADDR_HARDWIRE
            if ( i == 0 ) { /* Only root domain */
                RISCVAPLICState *aplic = RISCV_APLIC(aplic_ms[i]);
                /* LBPPN should shift 12 bits (page) */
                aplic->mmsicfgaddr = memmap[ANDES_AE350_IMSIC_M].base >> 12;
                /* HBPPN should shift 12 bits (page) + 32 bits */
                /* Doesn't set bit 31 to lock config */
                aplic->mmsicfgaddrH = memmap[ANDES_AE350_IMSIC_M].base >> 44;
                aplic->smsicfgaddr = memmap[ANDES_AE350_IMSIC_S].base >> 12;
                /* Doesn't set bit 31 to lock config */
                aplic->smsicfgaddrH = memmap[ANDES_AE350_IMSIC_S].base >> 44;
            }
#endif
        }
    }

    if (kvm_enabled() && ANDES_AE350_APLIC_S_DOMAINS == 0) {
        error_report("Supervisor mode for APLIC must be enabled "
                     "in the KVM environment");
        exit(1);
    }
    for (i = 0; i < ANDES_AE350_APLIC_S_DOMAINS; i++) {
        aplic_addr = memmap[ANDES_AE350_APLIC].base +
            ANDES_AE350_APLIC_S_BASE + (ANDES_AE350_APLIC_STRIDE * i);
        /*
         * First S-level parent is Root domain(Machine) (Level 0 domain)
         * Second~N S-level parent is Machine child id 1~n (Level 1 domain)
         */
        aplic_ss[i] = riscv_aplic_create(aplic_addr,
                                     ANDES_AE350_APLIC_SIZE_PER_DOMAIN,
                                     (msimode) ? 0 : base_hartid,
                                     (msimode) ? 0 : hart_count,
                                     ANDES_AE350_IRQCHIP_NUM_SOURCES,
                                     ANDES_AE350_IRQCHIP_NUM_PRIO_BITS,
                                     msimode, false,
                                     aplic_ms[i]);
    }

    return kvm_enabled() ? aplic_ss[0] : aplic_ms[0];
}

static bool andes_ae350_get_load_default_mrom(Object *obj, Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);

    return s->load_default_mrom;
}

static void andes_ae350_set_load_default_mrom(Object *obj, bool val, Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);

    s->load_default_mrom = val;
}

static char *andes_ae350_get_aia_guests(Object *obj, Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);

    return g_strdup_printf("%d", s->aia_guests);
}

static void andes_ae350_set_aia_guests(Object *obj, const char *val,
                                       Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);

    s->aia_guests = atoi(val);
    if (s->aia_guests < 0 || s->aia_guests > ANDES_AE350_IRQCHIP_MAX_GUESTS) {
        error_setg(errp, "Invalid number of AIA IMSIC guests");
        error_append_hint(errp, "Valid values be between 0 and %d.\n",
                          ANDES_AE350_IRQCHIP_MAX_GUESTS);
    }
}

static char *andes_ae350_get_aia(Object *obj, Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);
    const char *val;

    switch (s->aia_type) {
    case ANDES_AE350_AIA_TYPE_APLIC:
        val = "aplic";
        break;
    case ANDES_AE350_AIA_TYPE_APLIC_IMSIC:
        val = "aplic-imsic";
        break;
    default:
        val = "none";
        break;
    };

    return g_strdup(val);
}

static void andes_ae350_set_aia(Object *obj, const char *val, Error **errp)
{
    AndesAe350BoardState *s = ANDES_AE350_MACHINE(obj);

    if (!strcmp(val, "none")) {
        s->aia_type = ANDES_AE350_AIA_TYPE_NONE;
    } else if (!strcmp(val, "aplic")) {
        s->aia_type = ANDES_AE350_AIA_TYPE_APLIC;
    } else if (!strcmp(val, "aplic-imsic")) {
        s->aia_type = ANDES_AE350_AIA_TYPE_APLIC_IMSIC;
    } else {
        error_setg(errp, "Invalid AIA interrupt controller type");
        error_append_hint(errp, "Valid values are none, aplic, and "
                          "aplic-imsic.\n");
    }
}

static char *init_hart_config(const char *hart_config, int num_harts)
{
    int length = 0, i = 0;
    char *result;

    length = (strlen(hart_config) + 1) * num_harts;
    result = g_malloc0(length);
    for (i = 0; i < num_harts; i++) {
        if (i != 0) {
            strncat(result, ",", length);
        }
        strncat(result, hart_config, length);
        length -= (strlen(hart_config) + 1);
    }

    return result;
}

static const MemMapEntry iopmp300_protect_regions[] = {
    [IOPMP300_AHB_PROTECT_REGION]      = { 0xf0000000, 0x10000000 },
    [IOPMP300_RAM_PROTECT_REGION]      = { 0x00000000, 0x80000000 },
    [IOPMP300_SLP_PROTECT_REGION]      = { 0xa0000000,  0x1000000 },
    [IOPMP300_ROM_PROTECT_REGION]      = { 0x80000000,  0x8000000 },
    [IOPMP300_IOCP_PROTECT_REGION]     = {        0x0,        0x0 },
    [IOPMP300_DFS_PROTECT_REGION]      = {        0x0,        0x0 },
};

static const struct MemMapEntry iopmp200_protect_regions[] = {
    [IOPMP200_AHB_PROTECT_REGION]      = { 0xf0000000, 0x10000000 },
    [IOPMP200_RAM_PROTECT_REGION]      = { 0x00000000, 0x80000000 },
    [IOPMP200_ROM_PROTECT_REGION]      = { 0x80000000,  0x8000000 },
    [IOPMP200_ILMAP_PROTECT_REGION]    = { 0xa0000000,   0x200000 },
    [IOPMP200_DLMAP_PROTECT_REGION]    = { 0xa0200000,   0x200000 },
};

static const struct MemMapEntry iopmp100_protect_regions[] = {
    [IOPMP100_PORT_SMU_PROTECT_REGION]              = { 0xf0100000, 0x100000 },
    [IOPMP100_PORT_UART1_PROTECT_REGION]            = { 0xf0200000, 0x100000 },
    [IOPMP100_PORT_UART2_PROTECT_REGION]            = { 0xf0300000, 0x100000 },
    [IOPMP100_PORT_PIT_PROTECT_REGION]              = { 0xf0400000, 0x100000 },
    [IOPMP100_PORT_WDT_PROTECT_REGION]              = { 0xf0500000, 0x100000 },
    [IOPMP100_PORT_RTC_PROTECT_REGION]              = { 0xf0600000, 0x100000 },
    [IOPMP100_PORT_GPIO_PROTECT_REGION]             = { 0xf0700000, 0x100000 },
    [IOPMP100_PORT_I2C_PROTECT_REGION]              = { 0xf0a00000, 0x100000 },
    [IOPMP100_PORT_SPI1_PROTECT_REGION]             = { 0xf0b00000, 0x100000 },
    [IOPMP100_PORT_SPI2_PROTECT_REGION]             = { 0xf0f00000, 0x100000 },
    [IOPMP100_PORT_SDC_PROTECT_REGION]              = { 0xf0e00000, 0x100000 },
    [IOPMP100_PORT_DMAC_PROTECT_REGION]             = { 0xf0c00000, 0x100000 },
    [IOPMP100_PORT_SSP_PROTECT_REGION]              = { 0xf0d00000, 0x100000 },
    [IOPMP100_PORT_DTROM_PROTECT_REGION]            = { 0xf2000000, 0x100000 },
    [IOPMP100_PORT_IOPMP_PROTECT_REGION]            = { 0xf1000000, 0x100000 },
    [IOPMP100_PORT_ABPBRG_APBDEC_PROTECT_REGION]    = { 0xf0000000, 0x100000 }
};

static void iopmp_setup_cpus(RISCVHartArrayState *cpus, uint32_t rrid)
{
    RISCVCPU *cpu;
    for (int i = 0; i < cpus->num_harts; i++) {
        cpu = &cpus->harts[i];
        cpu->cfg.iopmp = true;
        cpu->cfg.iopmp_rrid = rrid;
    }
}

static void andes_ae350_soc_realize(DeviceState *dev_soc, Error **errp)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;
    MachineState *machine = MACHINE(qdev_get_machine());
    MemoryRegion *system_memory = get_system_memory();
    AndesAe350SocState *s = ANDES_AE350_SOC(dev_soc);
    AndesAe350BoardState *bs = ANDES_AE350_MACHINE(machine);
    char *plic_hart_config, *plicsw_hart_config;
    Iopmp_StreamSink *iopmp_sink;
    Object *obj = OBJECT(dev_soc);
    char *token;
    g_autofree char *secure_platform_dup = g_strdup(s->secure_platform);

    if (s->ilm_size) {
        if (s->ilm_size < ANDES_LM_SIZE_MIN || s->ilm_size > ANDES_LM_SIZE_MAX
            || s->ilm_size != 1 << (31 - __builtin_clz(s->ilm_size))) {
            error_report("Cannot set instruction local memory size to 0x%x. "
                         "Valid value are 0(unconnected ILM) or power "
                         "of 2 values between 0x%x and 0x%x.",
                         s->ilm_size, ANDES_LM_SIZE_MIN, ANDES_LM_SIZE_MAX);
            exit(1);
        }
        if (s->ilm_base & (s->ilm_size - 1)) {
            error_report("Cannot set instruction local memory base to 0x%lx. "
                         "It must be aligned to instruction local memory size "
                         "0x%x.",
                         (long)s->ilm_base, s->ilm_size);
            exit(1);
        }
    }
    if (s->dlm_size) {
        if (s->dlm_size < ANDES_LM_SIZE_MIN || s->dlm_size > ANDES_LM_SIZE_MAX
            || s->dlm_size != 1 << (31 - __builtin_clz(s->dlm_size))) {
            error_report("Cannot set data local memory size to 0x%x. "
                         "Valid value are 0(unconnected DLM) or "
                         "power of 2 values between 0x%x and 0x%x.",
                         s->dlm_size, ANDES_LM_SIZE_MIN, ANDES_LM_SIZE_MAX);
            exit(1);
        }
        if (s->dlm_base & (s->dlm_size - 1)) {
            error_report("Cannot set data local memory base to 0x%lx. "
                         "It must be aligned to data local memory size 0x%x.",
                          (long)s->dlm_base, s->dlm_size);
            exit(1);
        }
    }

    sysbus_realize(SYS_BUS_DEVICE(&s->cpus), &error_abort);

    if (!s->bmc_type) {
        object_property_set_str(obj, "bmc_type", "axi", &error_abort);
    }
    if (strncmp(s->bmc_type, "axi", 3) == 0) {
        andes_atcbmc300_create(memmap[ANDES_AE350_BMC].base,
                               memmap[ANDES_AE350_BMC].size);
    } else if (strncmp(s->bmc_type, "ahb", 3) == 0) {
        andes_atcbmc200_create(memmap[ANDES_AE350_BMC].base,
                               memmap[ANDES_AE350_BMC].size);
    } else {
        error_report("%s BMC type is not supported.", s->bmc_type);
        exit(1);
    }

    /* Optional Uncacheable Alias */
    if (s->uncacheable_alias_enable) {
        MemoryRegion *mask_alias = g_new(MemoryRegion, 1);
        memory_region_init_alias(mask_alias, NULL,
                                 "riscv.andes.ae350.uncacheable_alias",
                                 system_memory,
                                 0, memmap[ANDES_AE350_UNCACHEABLE_ALIAS].size);
        memory_region_add_subregion(system_memory,
                                    memmap[ANDES_AE350_UNCACHEABLE_ALIAS].base,
                                    mask_alias);
    }

    andes_plmt_create(memmap[ANDES_AE350_PLMT].base,
                      memmap[ANDES_AE350_PLMT].size,
                      32,
                      ANDES_PLMT_TIME_BASE,
                      ANDES_PLMT_TIMECMP_BASE,
                      ANDES_PLMT_TIMEBASE_FREQ,
                      ANDES_PLMT_HART_BASE);

    /* APLIC only and AIA equal none will need PLICSW as IPI */
    if (bs->aia_type != ANDES_AE350_AIA_TYPE_APLIC_IMSIC) {
        /* APLIC only and AIA equal none will need PLICSW as IPI */
        plicsw_hart_config =
            init_hart_config(ANDES_PLICSW_HART_CONFIG, machine->smp.cpus);

        /* Per-socket SW-PLIC */
        s->plic_sw = andes_plic_create(
            memmap[ANDES_AE350_PLICSW].base,
            ANDES_PLICSW_NAME,
            plicsw_hart_config, machine->smp.cpus,
            0, /* hartid_base */
            ANDES_PLICSW_NUM_SOURCES,
            ANDES_PLICSW_NUM_PRIORITIES,
            ANDES_PLICSW_PRIORITY_BASE,
            ANDES_PLICSW_PENDING_BASE,
            ANDES_PLICSW_ENABLE_BASE,
            ANDES_PLICSW_ENABLE_STRIDE,
            ANDES_PLICSW_THRESHOLD_BASE,
            ANDES_PLICSW_THRESHOLD_STRIDE,
            memmap[ANDES_AE350_PLICSW].size);

        g_free(plicsw_hart_config);
    }

    /* AIA is none will use legacy PLIC */
    if (bs->aia_type == ANDES_AE350_AIA_TYPE_NONE) {
        plic_hart_config =
            init_hart_config(ANDES_PLIC_HART_CONFIG, machine->smp.cpus);

        /* Per-socket PLIC */
        s->plic = andes_plic_create(
            memmap[ANDES_AE350_PLIC].base,
            ANDES_PLIC_NAME,
            plic_hart_config, machine->smp.cpus,
            0, /* hartid_base */
            ANDES_PLIC_NUM_SOURCES,
            ANDES_PLIC_NUM_PRIORITIES,
            ANDES_PLIC_PRIORITY_BASE,
            ANDES_PLIC_PENDING_BASE,
            ANDES_PLIC_ENABLE_BASE,
            ANDES_PLIC_ENABLE_STRIDE,
            ANDES_PLIC_THRESHOLD_BASE,
            ANDES_PLIC_THRESHOLD_STRIDE,
            memmap[ANDES_AE350_PLIC].size);

        g_free(plic_hart_config);
        s->irqchip = s->plic;
    } else {
        s->irqchip = andes_ae350_create_aia(bs->aia_type, bs->aia_guests,
                                            memmap, 0, 0, machine->smp.cpus);
    }

    if (kvm_enabled() && andes_ae350_use_kvm_aia(bs)) {
        kvm_riscv_aia_create(machine, IMSIC_MMIO_GROUP_MIN_SHIFT,
                             ANDES_AE350_IRQCHIP_NUM_SOURCES,
                             ANDES_AE350_IRQCHIP_NUM_MSIS,
                             memmap[ANDES_AE350_APLIC].base +
                             ANDES_AE350_APLIC_S_BASE,
                             memmap[ANDES_AE350_IMSIC_S].base,
                             bs->aia_guests);
    }

    /* VIRTIO */
    for (int i = 0; i < ANDES_AE350_VIRTIO_COUNT; i++) {
        sysbus_create_simple("virtio-mmio",
            (memmap[ANDES_AE350_VIRTIO].base +
                (i * memmap[ANDES_AE350_VIRTIO].size)),
            qdev_get_gpio_in(DEVICE(s->irqchip), (ANDES_AE350_VIRTIO_IRQ + i)));
    }

    /* SMU */
    andes_atcsmu_create(&s->atcsmu, memmap[ANDES_AE350_SMU].base,
                        memmap[ANDES_AE350_SMU].size,
                        machine->smp.cpus);

    /* SMC */
    create_unimplemented_device("riscv.andes.ae350.smc",
        memmap[ANDES_AE350_SMC].base, memmap[ANDES_AE350_SMC].size);

    /* SPI */
    create_unimplemented_device("riscv.andes.ae350.spi",
        memmap[ANDES_AE350_SPI].base, memmap[ANDES_AE350_SPI].size);

    /* RTC */
    atcrtc100_create(memmap[ANDES_AE350_RTC].base,
                     qdev_get_gpio_in(DEVICE(s->irqchip),
                     ANDES_AE350_RTC_PERIOD_IRQ),
                     qdev_get_gpio_in(DEVICE(s->irqchip),
                     ANDES_AE350_RTC_ALARM_IRQ));

    /* GPIO */
    create_unimplemented_device("riscv.andes.ae350.gpio",
        memmap[ANDES_AE350_GPIO].base, memmap[ANDES_AE350_GPIO].size);

    /* I2C */
    create_unimplemented_device("riscv.andes.ae350.i2c",
        memmap[ANDES_AE350_I2C].base, memmap[ANDES_AE350_I2C].size);

    /* LCD */
    create_unimplemented_device("riscv.andes.ae350.lcd",
        memmap[ANDES_AE350_LCD].base, memmap[ANDES_AE350_LCD].size);

    /* SND */
    create_unimplemented_device("riscv.andes.ae350.snd",
        memmap[ANDES_AE350_SND].base, memmap[ANDES_AE350_SND].size);

    /* DMAC */
    atcdmac300_create(&s->dma, "atcdmac300",
                memmap[ANDES_AE350_DMAC].base,
                memmap[ANDES_AE350_DMAC].size,
                qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_DMAC_IRQ));

    /* NIC */
    atfmac100_create(&s->atfmac100, "atfmac100",
                 memmap[ANDES_AE350_MAC].base,
                 qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_MAC_IRQ));

    /* PIT */
    atcpit100_create(memmap[ANDES_AE350_PIT].base,
                qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_PIT_IRQ));

    /* SDC */
    atfsdc010_create(memmap[ANDES_AE350_SDC].base,
                qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_SDC_IRQ));

    /* SPI2 */
    create_unimplemented_device("riscv.andes.ae350.spi2",
        memmap[ANDES_AE350_SPI2].base, memmap[ANDES_AE350_SPI2].size);

    /* WDT */
    atcwdt200_create(memmap[ANDES_AE350_WDT].base);

    /* UART */
    serial_mm_init(system_memory,
        memmap[ANDES_AE350_UART1].base + ANDES_UART_REG_OFFSET,
        ANDES_UART_REG_SHIFT,
        qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_UART1_IRQ),
        38400, serial_hd(1), DEVICE_LITTLE_ENDIAN);

    serial_mm_init(system_memory,
        memmap[ANDES_AE350_UART2].base + ANDES_UART_REG_OFFSET,
        ANDES_UART_REG_SHIFT,
        qdev_get_gpio_in(DEVICE(s->irqchip), ANDES_AE350_UART2_IRQ),
        38400, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    s->has_iopmp300 = false;
    s->has_iopmp200 = false;
    s->has_iopmp100 = false;
    if (s->secure_platform &&
        strcmp(s->secure_platform, ANDES_SECURE_PLATFORM_NONE) != 0) {
        iopmp_setup_cpus(&s->cpus, 0);
        /* Parsing config of secure platform */
        token = strtok(secure_platform_dup, "+");
        while (token) {
            if (strcmp(token, "atciopmp300") == 0) {
                s->has_iopmp300 = true;
            } else if (strcmp(token, "atciopmp200") == 0) {
                s->has_iopmp200 = true;
            } else if (strcmp(token, "atciopmp100") == 0) {
                s->has_iopmp100 = true;
            } else {
                error_report("Secure platform %s is not supported.",
                             s->secure_platform);
                exit(1);
            }
            token = strtok(NULL, "+");
        }
    } else {
        object_property_set_str(obj, "secure_platform",
                                ANDES_SECURE_PLATFORM_NONE, &error_abort);
    }

    if (s->has_iopmp300 && s->has_iopmp200) {
        error_report("Secure platform %s is not supported.",
                     s->secure_platform);
        exit(1);
    }

    if (s->has_iopmp300) {
        object_initialize_child(obj, "iopmp_dispatcher",
                                &(s->iopmp_dispatcher),
                                TYPE_IOPMP_DISPATCHER);
        qdev_prop_set_uint32(DEVICE(&s->iopmp_dispatcher), "target-num",
                             AE350_IOPMP300_NUM);
        sysbus_realize(SYS_BUS_DEVICE(&s->iopmp_dispatcher), NULL);
        for (int i = 0; i < AE350_IOPMP300_NUM; i++) {
            s->iopmp_dev[i] =
                atciopmp300_create(memmap[iopmp300_memmap_idx_list[i]].base,
                                   qdev_get_gpio_in(DEVICE(s->irqchip),
                                                    ANDES_AE350_IOPMP_IRQ));
            iopmp_sink = &(ATCIOPMP300(s->iopmp_dev[i])->transaction_info_sink);
            iopmp_dispatcher_add_target(DEVICE(&s->iopmp_dispatcher),
                                        (StreamSink *)iopmp_sink,
                                        iopmp300_protect_regions[i].base,
                                        iopmp300_protect_regions[i].size, i);
        }
        /* DMA connect to iopmp */
        atcdmac300_connect_iopmp(DEVICE(&s->dma), &address_space_memory,
            (StreamSink *)&(s->iopmp_dispatcher.transaction_info_sink),
            ANDES_AE350_DMAC_INF0_IOPMP_SID, ANDES_AE350_DMAC_INF1_IOPMP_SID);
    }

    if (s->has_iopmp200) {
        object_initialize_child(obj, "iopmp_dispatcher",
                                &(s->iopmp_dispatcher),
                                TYPE_IOPMP_DISPATCHER);
        qdev_prop_set_uint32(DEVICE(&s->iopmp_dispatcher), "target-num",
                             AE350_IOPMP200_NUM);
        sysbus_realize(SYS_BUS_DEVICE(&s->iopmp_dispatcher), NULL);
        for (int i = 0; i < AE350_IOPMP200_NUM; i++) {
            s->iopmp_dev[i] =
                atciopmp200_create(memmap[iopmp200_memmap_idx_list[i]].base,
                                   qdev_get_gpio_in(DEVICE(s->irqchip),
                                                    ANDES_AE350_IOPMP_IRQ));
            iopmp_sink = iopmp200_get_sink(s->iopmp_dev[i]);
            iopmp_dispatcher_add_target(DEVICE(&s->iopmp_dispatcher),
                                        (StreamSink *)iopmp_sink,
                                        iopmp200_protect_regions[i].base,
                                        iopmp200_protect_regions[i].size, i);
        }
        /* DMA connect to iopmp */
        atcdmac300_connect_iopmp(DEVICE(&s->dma), &address_space_memory,
            (StreamSink *)&(s->iopmp_dispatcher.transaction_info_sink),
            ANDES_AE350_DMAC_INF0_IOPMP_SID, ANDES_AE350_DMAC_INF1_IOPMP_SID);
    }

    if (s->has_iopmp100) {
        s->iopmp100_dev = atciopmp100_create(memmap[ANDES_AE350_IOPMP100].base);
    }
}

static void andes_ae350_soc_instance_init(Object *obj)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;
    MachineState *machine = MACHINE(qdev_get_machine());
    AndesAe350SocState *s = ANDES_AE350_SOC(obj);

    object_initialize_child(obj, "atcdmac300", &s->dma,
                                TYPE_ATCDMAC300);

    object_initialize_child(obj, "atfmac100", &s->atfmac100,
                                TYPE_ATFMAC100);

    object_initialize_child(obj, "atcsmu", &s->atcsmu,
                            TYPE_ANDES_ATCSMU);

    object_initialize_child(obj, "cpus", &s->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_str(OBJECT(&s->cpus), "cpu-type",
                            machine->cpu_type, &error_abort);
    object_property_set_int(OBJECT(&s->cpus), "num-harts",
                            machine->smp.cpus, &error_abort);
    qdev_prop_set_uint64(DEVICE(&s->cpus), "resetvec",
                            memmap[ANDES_AE350_MROM].base);
}

static int andes_load_elf(MachineState *machine,
                          const char *default_machine_firmware)
{
    char *firmware_filename = NULL;
    bool elf_is64;
    union {
        Elf32_Ehdr h32;
        Elf64_Ehdr h64;
    } elf_header;
    Error *err = NULL;

    firmware_filename = riscv_find_firmware(machine->firmware,
                                            default_machine_firmware);

    /* If not "none" load the firmware */
    if (firmware_filename) {
        load_elf_hdr(firmware_filename, &elf_header, &elf_is64, &err);

        if (err) {
            error_report("could not load firmware '%s', only support ELF format", firmware_filename);
            error_free(err);
            exit(1);
        }

        if (elf_is64) {
            return elf_header.h64.e_entry;
        } else {
            return elf_header.h32.e_entry;
        }
    }

    return 0;
}

typedef struct Subport_status {
    int hart_id;
    bool dlm;
} Subport_status;
static uint64_t subport_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t ret = 0;
    Subport_status *s = (Subport_status *)opaque;
    CPUState *cs = qemu_get_cpu(s->hart_id);
    if (!cs) {
        return ret;
    }
    CPURISCVState *env = &RISCV_CPU(cs)->env;
    if (s->dlm) {
        memory_region_dispatch_read(env->mask_dlm, addr, &ret,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    } else {
        memory_region_dispatch_read(env->mask_ilm, addr, &ret,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    }
    return ret;
}

static void subport_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    Subport_status *s = (Subport_status *)opaque;
    CPUState *cs = qemu_get_cpu(s->hart_id);
    if (!cs) {
        return;
    }
    CPURISCVState *env = &RISCV_CPU(cs)->env;
    if (s->dlm) {
        memory_region_dispatch_write(env->mask_dlm, addr, value,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    } else {
        memory_region_dispatch_write(env->mask_ilm, addr, value,
            size_memop(size) | MO_LE, (MemTxAttrs) { .memory = 1 });
    }
}

static const MemoryRegionOps subport_ops = {
    .read = subport_read,
    .write = subport_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
};

static void subport_create(int hart_id, bool dlm, hwaddr base, hwaddr size)
{
    Subport_status *s = g_new(Subport_status, 1);
    s->hart_id = hart_id;
    s->dlm = dlm;
    MemoryRegion *subport_mr = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();
    char *name;
    if (dlm) {
        name = g_strdup_printf("%s%d_%s", "riscv.andes.ae350.subport",
                               hart_id, "dlm");
    } else {
        name = g_strdup_printf("%s%d_%s", "riscv.andes.ae350.subport",
                               hart_id, "ilm");
    }
    memory_region_init_io(subport_mr, NULL, &subport_ops, s,
                          name, size);
    memory_region_add_subregion(system_memory, base, subport_mr);
}

static void andes_ae350_machine_init(MachineState *machine)
{
    const struct MemmapEntry *memmap = andes_ae350_memmap;

    AndesAe350BoardState *bs = ANDES_AE350_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_nor = g_new(MemoryRegion, 1);
    MemoryRegion *mask_hvm = g_new(MemoryRegion, 1);
    MemoryRegion *mask_l2c = g_new(MemoryRegion, 1);
    MemoryRegion *mask_dtrom = g_new(MemoryRegion, 1);
    target_ulong start_addr = memmap[ANDES_AE350_DRAM].base;
    target_ulong firmware_end_addr, kernel_start_addr;
    uint64_t kernel_entry;

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc",
                    &bs->soc, TYPE_ANDES_AE350_SOC);
    qdev_realize(DEVICE(&bs->soc), NULL, &error_abort);

    /* Check ram size is validate */
    if (machine->ram_size > memmap[ANDES_AE350_DRAM].size) {
        error_report("Cannot model more than %ldGB RAM",
            (long)memmap[ANDES_AE350_DRAM].size / (1024 * 1024 * 1024));
        exit(1);
    }

    /* register system main memory (actual RAM) */
    memory_region_init_ram(main_mem, NULL, "riscv.andes.ae350.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_DRAM].base,
        main_mem);

    /* NOR FLASH */
    memory_region_init_rom(mask_nor, NULL, "riscv.andes.ae350.nor",
                           memmap[ANDES_AE350_NOR].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_NOR].base,
                                mask_nor);

    /* HVM */
    memory_region_init_ram(mask_hvm, NULL, "riscv.andes.ae350.hvm",
                           1 << bs->soc.hvm_size_pow_2, &error_fatal);
    memory_region_add_subregion(system_memory, bs->soc.hvm_base, mask_hvm);

    /* HVM subport */
    MemoryRegion *mask_hvm_subport = g_new(MemoryRegion, 1);
    memory_region_init_alias(mask_hvm_subport, NULL,
                             "riscv.andes.ae350.hvm_subport",
                             mask_hvm, 0, 1 << bs->soc.hvm_size_pow_2);
    memory_region_add_subregion(system_memory, bs->soc.hvm_subport_base,
                                mask_hvm_subport);

    /* L2C */
    memory_region_init_ram(mask_l2c, NULL, "riscv.andes.ae350.l2c",
                           memmap[ANDES_AE350_L2C].size, &error_fatal);
    memory_region_set_readonly(mask_l2c, false);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_L2C].base,
                                mask_l2c);

    for (int i = 0 ; i < ANDES_LM_SUBPORTS_MAX; i++) {
        struct MemmapEntry silm_map =
            memmap[ANDES_AE350_SUBPORT0_ILM + i * 2];
        struct MemmapEntry sdlm_map =
             memmap[ANDES_AE350_SUBPORT0_ILM + i * 2 + 1];
        subport_create(i, 0, silm_map.base, silm_map.size);
        subport_create(i, 1, sdlm_map.base, sdlm_map.size);
    }

    /* load/create device tree */
    if (machine->dtb) {
        machine->fdt = load_device_tree(machine->dtb, &bs->fdt_size);
        if (!machine->fdt) {
            error_report("load_device_tree() failed");
            exit(1);
        }
    } else {
        create_fdt(bs, memmap, machine->ram_size);
    }

    if (machine->kernel_cmdline && *machine->kernel_cmdline) {
        qemu_fdt_setprop_string(machine->fdt, "/chosen", "bootargs",
                                machine->kernel_cmdline);
    }

    /* boot rom */
    MemoryRegion *mrom_mr;
    /*
     * Try to resolve an external memory backend object for the boot ROM.
     * If the user specified a backend via -object (e.g.,
     * -object memory-backend-file,id=flash-backend,mem-path=boot.img,size=...),
     * use it as the boot ROM. This enables preloaded flash content or
     * persistent storage.
     */
    Object *backend_obj = object_resolve_path_type(FLASH_OBJECT_PATH,
                                                   TYPE_MEMORY_BACKEND_FILE,
                                                   NULL);
    if (backend_obj) {
        /* Use the user-provided flash backend as the boot ROM memory */
        mrom_mr = host_memory_backend_get_memory(MEMORY_BACKEND(backend_obj));
    } else {
        /* No backend found, use default ROM region */
        mrom_mr = g_new(MemoryRegion, 1);
        memory_region_init_rom(mrom_mr, NULL, "riscv.andes.ae350.mrom",
                               memmap[ANDES_AE350_MROM].size, &error_fatal);
    }
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_MROM].base,
                                mrom_mr);

    start_addr = andes_load_elf(machine, BIOS_FILENAME);
    firmware_end_addr =
        riscv_find_and_load_firmware(machine, BIOS_FILENAME,
                                     (hwaddr *)&start_addr, NULL);
    if (machine->kernel_filename) {
        kernel_start_addr = riscv_calc_kernel_start_addr(&bs->soc.cpus,
                                                         firmware_end_addr);

        kernel_entry = riscv_load_kernel(machine, &bs->soc.cpus,
                                         kernel_start_addr, true,
                                         NULL);
    } else {
       /*
        * If dynamic firmware is used, it doesn't know where is the next mode
        * if kernel argument is not set.
        */
        kernel_entry = 0;
    }

    /* dtb rom */
    memory_region_init_rom(mask_dtrom, NULL, "riscv.andes.ae350.dtrom",
                           memmap[ANDES_AE350_DTROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[ANDES_AE350_DTROM].base,
                                mask_dtrom);
    riscv_load_fdt(memmap[ANDES_AE350_DTROM].base, machine->fdt, true);

    /* load the reset vector */
    if (bs->load_default_mrom) {
        riscv_setup_rom_reset_vec(machine, &bs->soc.cpus, start_addr,
                                memmap[ANDES_AE350_MROM].base,
                                memmap[ANDES_AE350_MROM].size,
                                kernel_entry, memmap[ANDES_AE350_DTROM].base);
    }

    if (bs->soc.has_iopmp100) {
        iopmp100_setup_system_memory_range(bs->soc.iopmp100_dev,
                                           iopmp100_protect_regions,
                                           IOPMP100_PORT_NUM);
    }
    if (bs->soc.has_iopmp200) {
        for (int i = 0; i < AE350_IOPMP200_NUM; i++) {
            iopmp200_setup_system_memory(bs->soc.iopmp_dev[i],
                                         &iopmp200_protect_regions[i], 1);
        }
    }
    if (bs->soc.has_iopmp300) {
        for (int i = 0; i < AE350_IOPMP300_NUM; i++) {
            iopmp300_setup_system_memory(bs->soc.iopmp_dev[i],
                                         &iopmp300_protect_regions[i], 1);
        }
    }
}

static void ae350_do_nmi_on_cpu(CPUState *cs, run_on_cpu_data arg)
{
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    env->mcause = 0x1;
    env->mepc = env->pc;
    env->pc = env->resetvec;
}

static void ae350_nmi(NMIState *n, int cpu_index, Error **errp)
{
    CPUState *cs = qemu_get_cpu(cpu_index);
    async_run_on_cpu(cs, ae350_do_nmi_on_cpu, RUN_ON_CPU_NULL);
}

static void andes_ae350_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    NMIClass *nc = NMI_CLASS(oc);
    mc->desc = "RISC-V Board compatible with Andes AE350";
    mc->init = andes_ae350_machine_init;
    mc->max_cpus = ANDES_CPUS_MAX;
    mc->default_cpu_type = VIRT_CPU;
    mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;
    mc->cpu_index_to_instance_props = riscv_numa_cpu_index_to_props;
    mc->get_default_cpu_node_id = riscv_numa_get_default_cpu_node_id;
    mc->numa_mem_supported = false;
    object_class_property_add_str(oc, "aia", andes_ae350_get_aia,
                                  andes_ae350_set_aia);
    object_class_property_set_description(oc, "aia",
                                          "Set type of AIA interrupt "
                                          "controller. Valid values are "
                                          "none, aplic, and aplic-imsic.");

    object_class_property_add_str(oc, "aia-guests",
                                  andes_ae350_get_aia_guests,
                                  andes_ae350_set_aia_guests);
    {
        g_autofree char *str =
            g_strdup_printf("Set number of guest MMIO pages for AIA IMSIC. "
                            "Valid value should be between 0 and %d.",
                            ANDES_AE350_IRQCHIP_MAX_GUESTS);
        object_class_property_set_description(oc, "aia-guests", str);
    }

    object_class_property_add_bool(oc, "load-default-mrom",
                                   andes_ae350_get_load_default_mrom,
                                   andes_ae350_set_load_default_mrom);
    object_class_property_set_description(oc, "load-default-mrom",
                                          "If true, QEMU will load related "
                                          "boot code and fw_dynamic_info to "
                                          "the MROM region(0x8000_0000).");

    nc->nmi_monitor_handler = ae350_nmi;

}

static void andes_ae350_machine_instance_init(Object *obj)
{
    AndesAe350BoardState *bs = ANDES_AE350_MACHINE(obj);
    bs->load_default_mrom = true;
}

static const TypeInfo andes_ae350_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("andes_ae350"),
    .parent     = TYPE_MACHINE,
    .class_init = andes_ae350_machine_class_init,
    .instance_init = andes_ae350_machine_instance_init,
    .instance_size = sizeof(AndesAe350BoardState),
    .interfaces = (InterfaceInfo[]) {
         { TYPE_NMI },
         { }
    },
};

static void andes_ae350_machine_init_register_types(void)
{
    type_register_static(&andes_ae350_machine_typeinfo);
}

type_init(andes_ae350_machine_init_register_types)


static Property andes_ae350_soc_property[] = {
    /* Defaults for standard extensions */
    DEFINE_PROP_BOOL("uncacheable_alias_enable", AndesAe350SocState,
                      uncacheable_alias_enable, false),
    DEFINE_PROP_STRING("bmc_type", AndesAe350SocState, bmc_type),
    DEFINE_PROP_UINT64("ilm_base", AndesAe350SocState, ilm_base, 0),
    DEFINE_PROP_UINT64("dlm_base", AndesAe350SocState, dlm_base, 0x200000),
    DEFINE_PROP_UINT32("ilm_size", AndesAe350SocState, ilm_size, 0x200000),
    DEFINE_PROP_UINT32("dlm_size", AndesAe350SocState, dlm_size, 0x200000),
    DEFINE_PROP_BOOL("ilm_default_enable", AndesAe350SocState,
                      ilm_default_enable, false),
    DEFINE_PROP_BOOL("dlm_default_enable", AndesAe350SocState,
                      dlm_default_enable, false),
    DEFINE_PROP_UINT64("hvm_base", AndesAe350SocState, hvm_base,
                       ANDES_HVM_BASE_DEFAULT),
    DEFINE_PROP_UINT64("hvm_size_pow_2", AndesAe350SocState, hvm_size_pow_2,
                       ANDES_HVM_SIZE_POW_2_DEFAULT),
    DEFINE_PROP_UINT64("hvm_subport_base", AndesAe350SocState, hvm_subport_base,
                       ANDES_HVM_SUBPORT_BASE_DEFAULT),
    DEFINE_PROP_STRING("secure_platform", AndesAe350SocState, secure_platform),
    DEFINE_PROP_END_OF_LIST(),
};

static void andes_ae350_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    device_class_set_props(dc, andes_ae350_soc_property);
    dc->realize = andes_ae350_soc_realize;
    dc->user_creatable = false;
}

static const TypeInfo andes_ae350_soc_type_info = {
    .name       = TYPE_ANDES_AE350_SOC,
    .parent     = TYPE_DEVICE,
    .instance_init = andes_ae350_soc_instance_init,
    .instance_size = sizeof(AndesAe350SocState),
    .class_init = andes_ae350_soc_class_init,
};

static void andes_ae350_soc_init_register_types(void)
{
    type_register_static(&andes_ae350_soc_type_info);
}

type_init(andes_ae350_soc_init_register_types)
