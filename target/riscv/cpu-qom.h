/*
 * QEMU RISC-V CPU QOM header (target agnostic)
 *
 * Copyright (c) 2023 Ventana Micro Systems Inc.
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

#ifndef RISCV_CPU_QOM_H
#define RISCV_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_RISCV_CPU "riscv-cpu"
#define TYPE_RISCV_DYNAMIC_CPU "riscv-dynamic-cpu"
#define TYPE_RISCV_VENDOR_CPU "riscv-vendor-cpu"
#define TYPE_RISCV_BARE_CPU "riscv-bare-cpu"

#define RISCV_CPU_TYPE_SUFFIX "-" TYPE_RISCV_CPU
#define RISCV_CPU_TYPE_NAME(name) (name RISCV_CPU_TYPE_SUFFIX)

#define TYPE_RISCV_CPU_ANDES_A25        RISCV_CPU_TYPE_NAME("andes-a25")
#define TYPE_RISCV_CPU_ANDES_A25MP      RISCV_CPU_TYPE_NAME("andes-a25mp")
#define TYPE_RISCV_CPU_ANDES_A27        RISCV_CPU_TYPE_NAME("andes-a27")
#define TYPE_RISCV_CPU_ANDES_A45        RISCV_CPU_TYPE_NAME("andes-a45")
#define TYPE_RISCV_CPU_ANDES_A45MP      RISCV_CPU_TYPE_NAME("andes-a45mp")
#define TYPE_RISCV_CPU_ANDES_A46MP      RISCV_CPU_TYPE_NAME("andes-a46mp")
#define TYPE_RISCV_CPU_ANDES_A46MPV     RISCV_CPU_TYPE_NAME("andes-a46mpv")
#define TYPE_RISCV_CPU_ANDES_AX25       RISCV_CPU_TYPE_NAME("andes-ax25")
#define TYPE_RISCV_CPU_ANDES_AX25MP     RISCV_CPU_TYPE_NAME("andes-ax25mp")
#define TYPE_RISCV_CPU_ANDES_AX27       RISCV_CPU_TYPE_NAME("andes-ax27")
#define TYPE_RISCV_CPU_ANDES_AX45       RISCV_CPU_TYPE_NAME("andes-ax45")
#define TYPE_RISCV_CPU_ANDES_AX45MP     RISCV_CPU_TYPE_NAME("andes-ax45mp")
#define TYPE_RISCV_CPU_ANDES_AX45MPV    RISCV_CPU_TYPE_NAME("andes-ax45mpv")
#define TYPE_RISCV_CPU_ANDES_AX46MP     RISCV_CPU_TYPE_NAME("andes-ax46mp")
#define TYPE_RISCV_CPU_ANDES_AX46MPV    RISCV_CPU_TYPE_NAME("andes-ax46mpv")
#define TYPE_RISCV_CPU_ANDES_AX65       RISCV_CPU_TYPE_NAME("andes-ax65")
#define TYPE_RISCV_CPU_ANDES_AX66       RISCV_CPU_TYPE_NAME("andes-ax66")
#define TYPE_RISCV_CPU_ANDES_D23        RISCV_CPU_TYPE_NAME("andes-d23")
#define TYPE_RISCV_CPU_ANDES_D25        RISCV_CPU_TYPE_NAME("andes-d25")
#define TYPE_RISCV_CPU_ANDES_D25F       RISCV_CPU_TYPE_NAME("andes-d25f")
#define TYPE_RISCV_CPU_ANDES_D45        RISCV_CPU_TYPE_NAME("andes-d45")
#define TYPE_RISCV_CPU_ANDES_N25        RISCV_CPU_TYPE_NAME("andes-n25")
#define TYPE_RISCV_CPU_ANDES_N25F       RISCV_CPU_TYPE_NAME("andes-n25f")
#define TYPE_RISCV_CPU_ANDES_N25F_SE    RISCV_CPU_TYPE_NAME("andes-n25f-se")
#define TYPE_RISCV_CPU_ANDES_N225       RISCV_CPU_TYPE_NAME("andes-n225")
#define TYPE_RISCV_CPU_ANDES_N45        RISCV_CPU_TYPE_NAME("andes-n45")
#define TYPE_RISCV_CPU_ANDES_NX25       RISCV_CPU_TYPE_NAME("andes-nx25")
#define TYPE_RISCV_CPU_ANDES_NX25F      RISCV_CPU_TYPE_NAME("andes-nx25f")
#define TYPE_RISCV_CPU_ANDES_NX27V      RISCV_CPU_TYPE_NAME("andes-nx27v")
#define TYPE_RISCV_CPU_ANDES_NX45       RISCV_CPU_TYPE_NAME("andes-nx45")
#define TYPE_RISCV_CPU_ANDES_NX45V      RISCV_CPU_TYPE_NAME("andes-nx45v")
#define TYPE_RISCV_CPU_MAX              RISCV_CPU_TYPE_NAME("max")
#define TYPE_RISCV_CPU_MAX32            RISCV_CPU_TYPE_NAME("max32")
#define TYPE_RISCV_CPU_BASE32           RISCV_CPU_TYPE_NAME("rv32")
#define TYPE_RISCV_CPU_BASE64           RISCV_CPU_TYPE_NAME("rv64")
#define TYPE_RISCV_CPU_BASE128          RISCV_CPU_TYPE_NAME("x-rv128")
#define TYPE_RISCV_CPU_RV32I            RISCV_CPU_TYPE_NAME("rv32i")
#define TYPE_RISCV_CPU_RV32E            RISCV_CPU_TYPE_NAME("rv32e")
#define TYPE_RISCV_CPU_RV64I            RISCV_CPU_TYPE_NAME("rv64i")
#define TYPE_RISCV_CPU_RV64E            RISCV_CPU_TYPE_NAME("rv64e")
#define TYPE_RISCV_CPU_RVA22U64         RISCV_CPU_TYPE_NAME("rva22u64")
#define TYPE_RISCV_CPU_RVA22S64         RISCV_CPU_TYPE_NAME("rva22s64")
#define TYPE_RISCV_CPU_IBEX             RISCV_CPU_TYPE_NAME("lowrisc-ibex")
#define TYPE_RISCV_CPU_SHAKTI_C         RISCV_CPU_TYPE_NAME("shakti-c")
#define TYPE_RISCV_CPU_SIFIVE_E31       RISCV_CPU_TYPE_NAME("sifive-e31")
#define TYPE_RISCV_CPU_SIFIVE_E34       RISCV_CPU_TYPE_NAME("sifive-e34")
#define TYPE_RISCV_CPU_SIFIVE_E51       RISCV_CPU_TYPE_NAME("sifive-e51")
#define TYPE_RISCV_CPU_SIFIVE_U34       RISCV_CPU_TYPE_NAME("sifive-u34")
#define TYPE_RISCV_CPU_SIFIVE_U54       RISCV_CPU_TYPE_NAME("sifive-u54")
#define TYPE_RISCV_CPU_THEAD_C906       RISCV_CPU_TYPE_NAME("thead-c906")
#define TYPE_RISCV_CPU_VEYRON_V1        RISCV_CPU_TYPE_NAME("veyron-v1")
#define TYPE_RISCV_CPU_HOST             RISCV_CPU_TYPE_NAME("host")

OBJECT_DECLARE_CPU_TYPE(RISCVCPU, RISCVCPUClass, RISCV_CPU)

#endif /* RISCV_CPU_QOM_H */
