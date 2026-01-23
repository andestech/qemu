#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"

static uint32_t insn_table[256] __attribute__((aligned(4))) = {
    0x00150513, // [0] addi x10, x10, 1
    0x00250513, // [1] addi x10, x10, 2
};

int main() {
    register_exception_handler();
    printf("Starting XAndesCoDense tests...\n");

    write_csr(0x800, (unsigned long)insn_table); 

    unsigned long mmsc_cfg = read_csr(0xfc2); 
    int ecdv = (mmsc_cfg >> 41) & 3;
    printf("CoDense version (ECDV): %d\n", ecdv);

    long res;
    if (ecdv == 0) {
        printf("Testing nds.exec.it 0...\n");
        asm volatile(
            "li x10, 10\n\t"
            ".short 0x8000\n\t"
            "mv %0, x10"
            : "=r"(res) : : "x10"
        );
    } else {
        printf("Testing nds.nexec.it 0...\n");
        asm volatile(
            "li x10, 10\n\t"
            ".short 0x9000\n\t"
            "mv %0, x10"
            : "=r"(res) : : "x10"
        );
    }
    
    if (res == 11) {
        printf("PASS CoDense execution\n");
    } else {
        printf("FAIL CoDense execution: Expected 11, got %ld\n", res);
    }

    return 0;
}
