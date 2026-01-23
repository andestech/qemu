#ifndef TRAP_UTILS_H
#define TRAP_UTILS_H

#include <stdio.h>

/* CSR Access Macros */
#define read_csr(reg) ({ unsigned long __tmp; \
    asm volatile ("csrr %0, " #reg : "=r"(__tmp)); __tmp; })

#define write_csr(reg, val) ({ \
    asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

/* Trap Entry Point */
#if __riscv_xlen == 64
static inline void __attribute__((target("arch=rv64im_zicsr"))) 
#elif __riscv_xlen == 32
static inline void __attribute__((target("arch=rv32im_zicsr"))) 
#else
#error "Only support both RV32 and RV64"
#endif
__attribute__((interrupt("machine"))) exception_handler(void) {
    unsigned long epc = read_csr(mepc);
    unsigned long cause = read_csr(mcause);
    printf("\n[EXCEPTION] Trap detected! Cause: 0x%lx, Faulting PC: 0x%lx\n", cause, epc);
    while (1);
}

static inline void register_exception_handler(void) {
    /* Register the exception handler in mtvec */
    /* Mode = Direct (0) */
    write_csr(mtvec, (unsigned long)exception_handler);
}

static inline void enable_vector(void) {
    unsigned long mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    // Enable FS (bits 14:13) and VS (bits 10:9)
    mstatus |= 0x6600; 
    asm volatile("csrw mstatus, %0" : : "r"(mstatus));
}

#endif /* TRAP_UTILS_H */