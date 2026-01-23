#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"
#include "andes_v5_encodings.h"

asm(".option arch, +v");

int main() {
    register_exception_handler();
    enable_vector();
    printf("Starting XAndesVDot tests...\n");

    unsigned long vl = 1;
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vtype) : "r"(vl));

    uint32_t vs1_val = 0x04030201; // Bytes: 1, 2, 3, 4
    uint32_t vs2_val = 0x08070605; // Bytes: 5, 6, 7, 8
    uint32_t zero = 0;

    asm volatile("vle32.v v10, (%0)" : : "r"(&vs1_val));
    asm volatile("vle32.v v11, (%0)" : : "r"(&vs2_val));
    asm volatile("vle32.v v20, (%0)" : : "r"(&zero));

    // 1. VD4DOTS.VV
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VD4DOTS_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    uint32_t res;
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != 70) {
        printf("FAIL vd4dots.vv: Expected 70, got %d\n", (int)res);
    } else {
        printf("PASS vd4dots.vv\n");
    }

    // 2. VD4DOTU.VV
    asm volatile("vle32.v v20, (%0)" : : "r"(&zero));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VD4DOTU_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != 70) {
        printf("FAIL vd4dotu.vv: Expected 70, got %d\n", (int)res);
    } else {
        printf("PASS vd4dotu.vv\n");
    }

    // 3. Negative check
    uint32_t vs1_neg = 0xFF; // Byte 0 is -1 (signed) or 255 (unsigned)
    asm volatile("vle32.v v10, (%0)" : : "r"(&vs1_neg)); // 0,0,0,-1
    asm volatile("vle32.v v20, (%0)" : : "r"(&zero));
    
    // VD4DOTS
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VD4DOTS_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if ((int32_t)res != -5) {
        printf("FAIL vd4dots.vv neg: Expected -5, got %d\n", (int32_t)res);
    } else {
        printf("PASS vd4dots.vv neg\n");
    }

    // VD4DOTU
    asm volatile("vle32.v v20, (%0)" : : "r"(&zero));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VD4DOTU_VV(20, 10, 11, 1)) : "v20", "v10", "v11"
    );
    asm volatile("vse32.v v20, (%0)" : : "r"(&res));
    if (res != 1275) {
        printf("FAIL vd4dotu.vv neg: Expected 1275, got %d\n", (int)res);
    } else {
        printf("PASS vd4dotu.vv neg\n");
    }

    return 0;
}