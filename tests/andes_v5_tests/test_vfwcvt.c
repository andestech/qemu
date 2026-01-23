#include <stdio.h>
#include <stdint.h>
#include "andes_v5_encodings.h"
#include "trap_utils.h"

asm(".option arch, +v");

static inline void set_vcfg_sew32_lmul1(unsigned long vl) {
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vtype) : "r"(vl));
}

static inline void enable_fs_vs(void) {
    unsigned long mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    // Enable FS (bits 14:13) and VS (bits 10:9)
    // 3 << 13 = 0x6000
    // 3 << 9  = 0x600
    mstatus |= 0x6600; 
    asm volatile("csrw mstatus, %0" : : "r"(mstatus));
}

int main() {
    register_exception_handler();
    enable_fs_vs();
    
    printf("Starting vfwcvt.f.n.v tests...\n");

    uint32_t src_data = 0x12; // El 0 = 2, El 1 = 1
    unsigned long vl = 2;

    // Load into v10
    unsigned long vtype_load;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vtype_load) : "r"(vl));
    register uint32_t * src_ptr asm("x10") = &src_data;
    asm volatile("vle8.v v10, (x10)" : : "r"(src_ptr) : "v10", "memory");

    // Perform conversion: Dest e32, Source e4
    set_vcfg_sew32_lmul1(vl);
    
    // vfwcvt.f.n.v v20, v10
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VFWCVT_F_N_V(20, 10, 1)) : "v20", "v10"
    );

    // Store result
    float result[2] = {0, 0};
    register float * dst_ptr asm("x10") = result;
    asm volatile("vse32.v v20, (x10)" : : "r"(dst_ptr) : "v20", "memory");

    printf("Result: %f, %f (Expected 2.0, 1.0)\n", result[0], result[1]);

    if (result[0] == 2.0f && result[1] == 1.0f) {
        printf("PASS Test vfwcvt\n");
    } else {
        printf("FAIL Test vfwcvt\n");
    }

    return 0;
}
