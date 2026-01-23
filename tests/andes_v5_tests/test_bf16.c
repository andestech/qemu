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
    printf("Starting XAndesBFHCvt tests...\n");

    // 1. Scalar BF16 -> Float32
    // BF16: 1.0 = 0x3F80.
    // Nan-boxing: upper bits must be all 1s.
    uint64_t bf16_val = 0xFFFFFFFFFFFF3F80ULL; 
    
    uint32_t res_u;
    asm volatile("fmv.d.x fa0, %1\n\t" 
                 ".word %2\n\t"
                 "fmv.x.w %0, fa1"
                 : "=r"(res_u) : "r"(bf16_val), "i"(ENCODE_NFCVT_S_BF16(11, 10)) : "fa0", "fa1");
    
    if (res_u != 0x3F800000) {
        printf("FAIL nds.nfcvt.s.bf16: Expected 0x3F800000, got 0x%x\n", res_u);
    } else {
        printf("PASS nds.nfcvt.s.bf16\n");
    }

    // 2. Vector BF16 -> Float32
    unsigned long vl = 1;
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vtype) : "r"(vl));
    
    uint16_t src_bf16 = 0x3F80;
    asm volatile("vle16.v v10, (%0)" : : "r"(&src_bf16) : "v10");

    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VFWCVT_S_BF16(20, 10)) : "v20", "v10"
    );

    uint32_t dst_f32;
    asm volatile("vse32.v v20, (%0)" : : "r"(&dst_f32) : "memory");

    if (dst_f32 != 0x3F800000) {
        printf("FAIL vfwcvt.s.bf16: Expected 0x3F800000, got 0x%x\n", dst_f32);
    } else {
        printf("PASS vfwcvt.s.bf16\n");
    }

    return 0;
}
