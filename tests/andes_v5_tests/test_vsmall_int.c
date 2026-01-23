#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"
#include "andes_v5_encodings.h"

asm(".option arch, +v");

int main() {
    register_exception_handler();
    enable_vector();
    printf("Starting XAndesVSIntH tests...\n");

    unsigned long vl = 2;
    unsigned long vtype;
    
    // 1. VFWCVT.F.NU.V
    uint32_t src_data = 0xFB;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vtype) : "r"(vl));
    asm volatile("vle8.v v10, (%0)" : : "r"(&src_data));

    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vtype) : "r"(vl));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VFWCVT_F_NU_V(20, 10, 1)) : "v20", "v10"
    );
    float res[2];
    asm volatile("vse32.v v20, (%0)" : : "r"(res));
    if (res[0] != 11.0f || res[1] != 15.0f) {
        printf("FAIL vfwcvt.f.nu.v: Expected 11.0, 15.0. Got %f, %f\n", res[0], res[1]);
    } else {
        printf("PASS vfwcvt.f.nu.v\n");
    }

    // 2. VZEXT.VF2
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vtype) : "r"(vl));
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VZEXT_VF2(20, 10, 1)) : "v20", "v10"
    );
    uint8_t res_u8[2];
    asm volatile("vse8.v v20, (%0)" : : "r"(res_u8));
    if (res_u8[0] != 0x0B || res_u8[1] != 0x0F) {
        printf("FAIL vzext.vf2: Expected 0B, 0F. Got %02x, %02x\n", res_u8[0], res_u8[1]);
    } else {
        printf("PASS vzext.vf2\n");
    }

    return 0;
}
