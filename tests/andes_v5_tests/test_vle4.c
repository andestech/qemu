#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "andes_v5_encodings.h"
#include "trap_utils.h"

asm(".option arch, +v");

static inline void set_vstart(unsigned long vstart) {
    asm volatile("csrw vstart, %0" : : "r"(vstart));
}

static inline void set_vcfg_sew8_lmul1(unsigned long vl) {
    unsigned long vtype;
    asm volatile("vsetvli %0, %1, e8, m1, tu, mu" : "=r"(vtype) : "r"(vl));
}

static inline void fill_vreg(uint8_t val) {
    uint8_t buf[256];
    memset(buf, val, sizeof(buf));
    register uint8_t *ptr asm("x11") = buf;
    asm volatile(
        "vle8.v v10, (x11)\n\t"
        : : "r"(ptr) : "v10", "memory"
    );
}

static inline void read_vreg(uint8_t *buf) {
    register uint8_t *ptr asm("x11") = buf;
    asm volatile(
        "vse8.v v10, (x11)\n\t"
        : : "r"(ptr) : "memory"
    );
}

int main() {
    register_exception_handler();
    printf("Starting vle4.v tests...\n");

    uint8_t src_mem[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    uint8_t dst_mem[16];
    unsigned long vl;

    // Sanity check
    printf("Sanity check: filling v10 with 0x55...\n");
    set_vcfg_sew8_lmul1(4);
    set_vstart(0);
    fill_vreg(0x55);
    read_vreg(dst_mem);
    if (dst_mem[0] != 0x55) {
        printf("Sanity check FAILED!\n");
        return 1;
    }

    // Test 1: Odd vstart (1), Even vl (4)
    // According to AndeStar V5 spec, vstart is aligned down to 0.
    // Byte 0 (src_mem[0]=0xAA) will be loaded into El 0 and El 1.
    printf("Test 1: Odd vstart (vstart=1, vl=4)...\n");
    vl = 4;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(1);

    register uint8_t * src_ptr asm("x10") = src_mem;
    asm volatile(
        ".word %0\n\t" 
        : : "i"(ENCODE_VLE4_V(10, 10)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    printf("Result Byte 0: 0x%02X (Expected 0xAA)\n", dst_mem[0]);
    printf("Result Byte 1: 0x%02X (Expected 0xBB)\n", dst_mem[1]);

    if (dst_mem[0] != 0xAA || dst_mem[1] != 0xBB) {
        printf("FAIL Test 1\n");
    } else {
        printf("PASS Test 1\n");
    }

    // Test 2: Even vstart (0), Odd vl (3)
    printf("Test 2: Even vstart (vstart=0, vl=3)...\n");
    vl = 3;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(0);

    src_ptr = src_mem + 2; // 0xCC
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VLE4_V(10, 10)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    printf("Result Byte 0: 0x%02X (Expected 0xCC)\n", dst_mem[0]);
    printf("Result Byte 1: 0x%02X (Expected 0x5D)\n", dst_mem[1]); // 0x5D: El2(D), El3(5)

    if (dst_mem[0] != 0xCC || dst_mem[1] != 0x5D) {
        printf("FAIL Test 2\n");
    } else {
        printf("PASS Test 2\n");
    }

    // Test 3: Odd vstart (1), Odd vl (3)
    // According to AndeStar V5 spec, vstart is aligned down to 0.
    // Byte 0 (src_mem[0]=0xAA) will be loaded into El 0 and El 1.
    // Byte 1 (src_mem[1]=0xBB) will be loaded into El 2.
    printf("Test 3: Odd vstart (vstart=1, vl=3)...\n");
    vl = 3;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(1);

    src_ptr = src_mem; // 0xAA, 0xBB
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VLE4_V(10, 10)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    // Byte 0: El 0 (A), El 1 (A) -> 0xAA
    // Byte 1: El 2 (B), El 3 (5) -> 0x5B
    printf("Result Byte 0: 0x%02X (Expected 0xAA)\n", dst_mem[0]);
    printf("Result Byte 1: 0x%02X (Expected 0x5B)\n", dst_mem[1]);

    if (dst_mem[0] != 0xAA || dst_mem[1] != 0x5B) {
        printf("FAIL Test 3\n");
    } else {
        printf("PASS Test 3\n");
    }

    // Test 4: Even vstart (2), Even vl (4)
    // El 0, 1 (skipped), El 2, 3 (loaded)
    printf("Test 4: Even vstart (vstart=2, vl=4)...\n");
    vl = 4;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(2);

    src_ptr = src_mem; // 0xAA, 0xBB
    asm volatile(
        ".word %0\n\t"
        : : "i"(ENCODE_VLE4_V(10, 10)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    // Byte 0: El 0 (5), El 1 (5) -> 0x55
    // Byte 1: El 2 (B), El 3 (B) -> 0xBB
    printf("Result Byte 0: 0x%02X (Expected 0x55)\n", dst_mem[0]);
    printf("Result Byte 1: 0x%02X (Expected 0xBB)\n", dst_mem[1]);

    if (dst_mem[0] != 0x55 || dst_mem[1] != 0xBB) {
        printf("FAIL Test 4\n");
    } else {
        printf("PASS Test 4\n");
    }

    return 0;
}
