#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"
#include "andes_v5_encodings.h"

asm(".option arch, +v");
asm(".option arch, +d");
asm(".option arch, +f");

int main() {
    register_exception_handler();
    enable_vector();
    printf("Starting XAndesVPackFPH tests...\n");

    unsigned long vl = 1;
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vtype) : "r"(vl));

    uint16_t vs2_val = 0x4000; // 2.0
    asm volatile("vle16.v v10, (%0)" : : "r"(&vs2_val) : "v10");

    uint64_t rs1_val = 0x40004400; // Top 2.0, Bottom 4.0
    // Load rs1 into FA0
    asm volatile("fmv.d.x fa0, %0" : : "r"(rs1_val) : "fa0");

    // 1. VFPMADT.VF
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VFPMADT_VF(20, 10, 10, 1)) : "v20", "v10", "fa0"
    );
    uint16_t res;
    asm volatile("vse16.v v20, (%0)" : : "r"(&res) : "memory");
    if (res != 0x4800) {
        printf("FAIL vfpmadt.vf: Expected 0x4800 (8.0), got 0x%x\n", res);
    } else {
        printf("PASS vfpmadt.vf\n");
    }

    // 2. VFPMADB.VF
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VFPMADB_VF(20, 10, 10, 1)) : "v20", "v10", "fa0"
    );
    asm volatile("vse16.v v20, (%0)" : : "r"(&res) : "memory");
    if (res != 0x4900) {
        printf("FAIL vfpmadb.vf: Expected 0x4900 (10.0), got 0x%x\n", res);
    } else {
        printf("PASS vfpmadb.vf\n");
    }

    return 0;
}