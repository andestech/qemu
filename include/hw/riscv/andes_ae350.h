/*
 * Andes RISC-V AE350 Board
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

#ifndef HW_RISCV_ANDES_AE350_H
#define HW_RISCV_ANDES_AE350_H

#include "hw/riscv/riscv_hart.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "qom/object.h"

#include "hw/dma/atcdmac300.h"
#include "hw/net/atfmac100.h"
#include "hw/sd/atfsdc010.h"
#include "hw/misc/andes_atcsmu.h"
#include "hw/misc/riscv_iopmp_dispatcher.h"
#include "hw/intc/riscv_imsic.h"

#define ANDES_CPUS_MAX 8
#define ANDES_AE350_CPUS_MAX_BITS     3
#define ANDES_AE350_CPUS_MAX    (1 << ANDES_AE350_CPUS_MAX_BITS)
#define ANDES_AE350_SOCKETS_MAX_BITS          0
#define ANDES_AE350_SOCKETS_MAX               (1 << \
    ANDES_AE350_SOCKETS_MAX_BITS)

#define TYPE_ANDES_AE350_SOC "riscv.andes.ae350.soc"
#define ANDES_AE350_SOC(obj) \
    OBJECT_CHECK(AndesAe350SocState, (obj), TYPE_ANDES_AE350_SOC)

#define AE350_GENERIC_IOPMP_MAX_NUM 6
#define AE350_IOPMP300_NUM 6
#define AE350_IOPMP200_NUM 5

#define ANDES_SECURE_PLATFORM_NONE "none"

#define FLASH_OBJECT_PATH "/objects/flash-backend"

typedef enum AndesAe350AIAType {
    ANDES_AE350_AIA_TYPE_NONE = 0,
    ANDES_AE350_AIA_TYPE_APLIC,
    ANDES_AE350_AIA_TYPE_APLIC_IMSIC,
} AndesAe350AIAType;

typedef struct AndesAe350SocState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    RISCVHartArrayState cpus;
    DeviceState *plic;
    DeviceState *plic_sw;
    DeviceState *irqchip;

    char *bmc_type;

    ATCDMAC300State dma;
    ATFMAC100State atfmac100;
    AndesATCSMUState atcsmu;

    uint64_t ilm_base;
    uint64_t dlm_base;
    uint32_t ilm_size;
    uint32_t dlm_size;
    bool ilm_default_enable;
    bool dlm_default_enable;
    bool uncacheable_alias_enable;

    uint64_t hvm_base;
    uint64_t hvm_size_pow_2;
    uint64_t hvm_subport_base;

    DeviceState *iopmp_dev[AE350_GENERIC_IOPMP_MAX_NUM];
    Iopmp_Dispatcher_State iopmp_dispatcher;
    char *secure_platform;
    bool has_iopmp300, has_iopmp200, has_iopmp100;

    DeviceState *iopmp100_dev;
} AndesAe350SocState;

#define TYPE_ANDES_AE350_MACHINE MACHINE_TYPE_NAME("andes_ae350")
#define ANDES_AE350_MACHINE(obj) \
    OBJECT_CHECK(AndesAe350BoardState, (obj), TYPE_ANDES_AE350_MACHINE)

typedef struct AndesAe350BoardState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    AndesAe350SocState soc;
    int fdt_size;
    AndesAe350AIAType aia_type;
    int aia_guests;
    bool load_default_mrom;
} AndesAe350BoardState;

enum {
    ANDES_AE350_DRAM,
    ANDES_AE350_MROM,
    ANDES_AE350_NOR,
    ANDES_AE350_SUBPORT0_ILM,
    ANDES_AE350_SUBPORT0_DLM,
    ANDES_AE350_SUBPORT1_ILM,
    ANDES_AE350_SUBPORT1_DLM,
    ANDES_AE350_SUBPORT2_ILM,
    ANDES_AE350_SUBPORT2_DLM,
    ANDES_AE350_SUBPORT3_ILM,
    ANDES_AE350_SUBPORT3_DLM,
    ANDES_AE350_SUBPORT4_ILM,
    ANDES_AE350_SUBPORT4_DLM,
    ANDES_AE350_SUBPORT5_ILM,
    ANDES_AE350_SUBPORT5_DLM,
    ANDES_AE350_SUBPORT6_ILM,
    ANDES_AE350_SUBPORT6_DLM,
    ANDES_AE350_SUBPORT7_ILM,
    ANDES_AE350_SUBPORT7_DLM,
    ANDES_AE350_BMC,
    ANDES_AE350_IMSIC_M,
    ANDES_AE350_IMSIC_S,
    ANDES_AE350_MAC,
    ANDES_AE350_LCD,
    ANDES_AE350_SMC,
    ANDES_AE350_L2C,
    ANDES_AE350_CLIC,
    ANDES_AE350_PLIC,
    ANDES_AE350_APLIC,
    ANDES_AE350_PLMT,
    ANDES_AE350_PLICSW,
    ANDES_AE350_SMU,
    ANDES_AE350_UART1,
    ANDES_AE350_UART2,
    ANDES_AE350_PIT,
    ANDES_AE350_WDT,
    ANDES_AE350_RTC,
    ANDES_AE350_GPIO,
    ANDES_AE350_I2C,
    ANDES_AE350_SPI,
    ANDES_AE350_DMAC,
    ANDES_AE350_SND,
    ANDES_AE350_SDC,
    ANDES_AE350_SPI2,
    ANDES_AE350_IOPMP,
    ANDES_AE350_IOPMP_APB,
    ANDES_AE350_IOPMP_RAM,
    ANDES_AE350_IOPMP_SLP,
    ANDES_AE350_IOPMP_ROM,
    ANDES_AE350_IOPMP_IOCP,
    ANDES_AE350_IOPMP_DFS,
    ANDES_AE350_IOPMP_ILMAP,
    ANDES_AE350_IOPMP_DLMAP,
    ANDES_AE350_IOPMP100,
    ANDES_AE350_UART3,
    ANDES_AE350_DTROM,
    ANDES_AE350_VIRTIO,
    ANDES_AE350_UNCACHEABLE_ALIAS,
};

enum {
    IOPMP300_AHB_PROTECT_REGION,
    IOPMP300_RAM_PROTECT_REGION,
    IOPMP300_SLP_PROTECT_REGION,
    IOPMP300_ROM_PROTECT_REGION,
    IOPMP300_IOCP_PROTECT_REGION, /* not impl */
    IOPMP300_DFS_PROTECT_REGION /* not impl */
};

enum {
    IOPMP200_AHB_PROTECT_REGION,
    IOPMP200_RAM_PROTECT_REGION,
    IOPMP200_ROM_PROTECT_REGION,
    IOPMP200_ILMAP_PROTECT_REGION,
    IOPMP200_DLMAP_PROTECT_REGION
};

enum {
    IOPMP100_PORT_SMU_PROTECT_REGION = 1,
    IOPMP100_PORT_UART1_PROTECT_REGION = 2,
    IOPMP100_PORT_UART2_PROTECT_REGION = 3,
    IOPMP100_PORT_PIT_PROTECT_REGION = 4,
    IOPMP100_PORT_WDT_PROTECT_REGION = 5,
    IOPMP100_PORT_RTC_PROTECT_REGION = 6,
    IOPMP100_PORT_GPIO_PROTECT_REGION = 7,
    IOPMP100_PORT_I2C_PROTECT_REGION = 8,
    IOPMP100_PORT_SPI1_PROTECT_REGION = 9,
    IOPMP100_PORT_SPI2_PROTECT_REGION = 10,
    IOPMP100_PORT_SDC_PROTECT_REGION = 11,
    IOPMP100_PORT_DMAC_PROTECT_REGION = 12,
    IOPMP100_PORT_SSP_PROTECT_REGION = 13,
    IOPMP100_PORT_DTROM_PROTECT_REGION = 14,
    IOPMP100_PORT_IOPMP_PROTECT_REGION = 15,
    IOPMP100_PORT_ABPBRG_APBDEC_PROTECT_REGION = 16,
    IOPMP100_PORT_UART3_PROTECT_REGION = 17,
    IOPMP100_PORT_NUM,
};

enum {
    ANDES_AE350_RTC_PERIOD_IRQ = 1,
    ANDES_AE350_RTC_ALARM_IRQ = 2,
    ANDES_AE350_PIT_IRQ = 3,
    ANDES_AE350_UART1_IRQ = 8,
    ANDES_AE350_UART2_IRQ = 9,
    ANDES_AE350_DMAC_IRQ = 10,
    ANDES_AE350_UART3_IRQ = 12,
    ANDES_AE350_SDC_IRQ = 18,
    ANDES_AE350_MAC_IRQ = 19,
    ANDES_AE350_VIRTIO_COUNT = 8,
    ANDES_AE350_VIRTIO_IRQ = 16, /* 16 to 23 */
    ANDES_AE350_IOPMP_IRQ = 21,
};

#define ANDES_UART_REG_SHIFT    0x2
#define ANDES_UART_REG_OFFSET   0x20

/* IDLM maximum subordinate port*/
#define ANDES_LM_SUBPORTS_MAX 8

/* LM size range in AndeStar_V5_SPA v1.6. Size 0 for unconnected LM */
#define ANDES_LM_SIZE_MIN 0x400
#define ANDES_LM_SIZE_MAX 0x20000000

/* HVM defalut configs */
#define ANDES_HVM_BASE_DEFAULT       0x90000000
#define ANDES_HVM_SIZE_POW_2_DEFAULT 0x0
#define ANDES_HVM_SUBPORT_BASE_DEFAULT      ANDES_HVM_BASE_DEFAULT

/* AIA/APLIC address map offset */
#define ANDES_AE350_APLIC_DOMAIN_NUM        1 /* this can be config by hw */
#define ANDES_AE350_APLIC_SUPERVISOR        1 /* this can be config by hw */
#define ANDES_AE350_APLIC_M_BASE            0x0
#define ANDES_AE350_APLIC_S_BASE            0x8000
#define ANDES_AE350_APLIC_STRIDE            0x10000
#define ANDES_AE350_APLIC_SIZE_PER_DOMAIN   0x8000
#define ANDES_AE350_APLIC_M_DOMAINS         ANDES_AE350_APLIC_DOMAIN_NUM
#if (ANDES_AE350_APLIC_SUPERVISOR != 0)
    #define ANDES_AE350_APLIC_S_DOMAINS         ANDES_AE350_APLIC_DOMAIN_NUM
#else
    #define ANDES_AE350_APLIC_S_DOMAINS         0
#endif

#define ANDES_AE350_IRQCHIP_NUM_MSIS 255
#define ANDES_AE350_IRQCHIP_NUM_SOURCES 63
#define ANDES_AE350_IRQCHIP_NUM_PRIO_BITS 3
#define ANDES_AE350_IRQCHIP_MAX_GUESTS_BITS 3
#define ANDES_AE350_IRQCHIP_MAX_GUESTS \
    ((1U << ANDES_AE350_IRQCHIP_MAX_GUESTS_BITS) - 1U)

/* if define hardwire, ae350 will set mmsicfgaddr to IMSIC address */
#define ANDES_AE350_MSICFGADDR_HARDWIRE 1

#define ANDES_AE350_IMSIC_GROUP_MAX_SIZE    (1U << IMSIC_MMIO_GROUP_MIN_SHIFT)
#if ANDES_AE350_IMSIC_GROUP_MAX_SIZE < \
    IMSIC_GROUP_SIZE(ANDES_AE350_CPUS_MAX_BITS, \
    ANDES_AE350_IRQCHIP_MAX_GUESTS_BITS)
#error "Can't accommodate single IMSIC group in address space"
#endif

#define ANDES_AE350_IMSIC_MAX_SIZE  (ANDES_AE350_SOCKETS_MAX * \
                                     ANDES_AE350_IMSIC_GROUP_MAX_SIZE)
#if 0x4000000 < ANDES_AE350_IMSIC_MAX_SIZE
#error "Can't accommodate all IMSIC groups in address space"
#endif

/* SID for IOPMP */
#define ANDES_AE350_CPU_IOPMP_SID         0
#define ANDES_AE350_DMAC_INF0_IOPMP_SID   6
#define ANDES_AE350_DMAC_INF1_IOPMP_SID   7

#if defined(TARGET_RISCV32)
#define VIRT_CPU TYPE_RISCV_CPU_BASE32
#elif defined(TARGET_RISCV64)
#define VIRT_CPU TYPE_RISCV_CPU_BASE64
#endif

#endif /* HW_RISCV_ANDES_AE350_H */
