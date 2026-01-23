#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"
#include "andes_v5_encodings.h"

asm(".option arch, +v");

int main() {
    register_exception_handler();
    enable_vector();
    printf("Starting XAndesVQMac tests...\n");

    unsigned long vl = 1;
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vtype) : "r"(vl));

    int8_t vs1_val = -2;
    int8_t vs2_val = 3;
    int32_t vd_val = 0;

    asm volatile("vle8.v v10, (%0)" : : "r"(&vs1_val));
    asm volatile("vle8.v v11, (%0)" : : "r"(&vs2_val));
    asm volatile("vle32.v v20, (%0)" : : "r"(&vd_val));

    // 1. VQMACC.VV
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VQMACC_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    int32_t res;
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != -6) {
        printf("FAIL vqmacc.vv: Expected -6, got %d\n", (int)res);
    } else {
        printf("PASS vqmacc.vv\n");
    }

    // 2. VQMACCU.VV
    asm volatile("vle32.v v20, (%0)" : : "r"(&vd_val));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VQMACCU_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != 762) {
        printf("FAIL vqmaccu.vv: Expected 762, got %d\n", (int)res);
    } else {
        printf("PASS vqmaccu.vv\n");
    }

    // 3. VQMACCSU.VV
    asm volatile("vle32.v v20, (%0)" : : "r"(&vd_val));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VQMACCSU_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != -6) {
        printf("FAIL vqmaccsu.vv: Expected -6, got %d\n", (int)res);
    } else {
        printf("PASS vqmaccsu.vv\n");
    }

    return 0;
}
