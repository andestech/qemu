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
    printf("Starting vln8.v and vlnu8.v tests...\n");

    uint8_t src_mem[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    uint8_t dst_mem[16];
    unsigned long vl;

    // Test 1: vln8.v unmasked, vl=4
    // Nibbles: 2, 1, 4, 3 (from 0x12, 0x34)
    // Extended to 8-bit: 0x02, 0x01, 0x04, 0x03
    printf("Test 1: vln8.v unmasked (vl=4)...\n");
    vl = 4;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(0);

    register uint8_t * src_ptr asm("x10") = src_mem;
    asm volatile(
        ".word %0\n\t" 
        : : "i"(ENCODE_VLN8_V(10, 10, 1)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    printf("Result: 0x%02X 0x%02X 0x%02X 0x%02X\n", dst_mem[0], dst_mem[1], dst_mem[2], dst_mem[3]);
    if (dst_mem[0] == 0x02 && dst_mem[1] == 0x01 && dst_mem[2] == 0x04 && dst_mem[3] == 0x03) {
        printf("PASS Test 1\n");
    } else {
        printf("FAIL Test 1\n");
    }

    // Test 2: vln8.v signed extension check
    // Nibble 0xA (bits 3:0 of 0x9A), Nibble 0x9 (bits 7:4 of 0x9A)
    // 0xA -> 0xFA (signed), 0x9 -> 0xF9 (signed)
    printf("Test 2: vln8.v signed extension check...\n");
    vl = 2;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(0);

    src_ptr = src_mem + 4; // 0x9A
    asm volatile(
        ".word %0\n\t" 
        : : "i"(ENCODE_VLN8_V(10, 10, 1)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    printf("Result: 0x%02X 0x%02X\n", dst_mem[0], dst_mem[1]);
    if (dst_mem[0] == 0xFA && dst_mem[1] == 0xF9) {
        printf("PASS Test 2\n");
    } else {
        printf("FAIL Test 2\n");
    }

    // Test 3: vlnu8.v unsigned extension check
    // Nibble 0xA, 0x9 from 0x9A
    // 0xA -> 0x0A, 0x9 -> 0x09
    printf("Test 3: vlnu8.v unsigned extension check...\n");
    vl = 2;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(0);

    src_ptr = src_mem + 4; // 0x9A
    asm volatile(
        ".word %0\n\t" 
        : : "i"(ENCODE_VLNU8_V(10, 10, 1)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    printf("Result: 0x%02X 0x%02X\n", dst_mem[0], dst_mem[1]);
    if (dst_mem[0] == 0x0A && dst_mem[1] == 0x09) {
        printf("PASS Test 3\n");
    } else {
        printf("FAIL Test 3\n");
    }

    // Test 4: vstart check
    printf("Test 4: vln8.v with vstart=1, vl=3...\n");
    vl = 3;
    set_vcfg_sew8_lmul1(vl);
    fill_vreg(0x55);
    set_vstart(1);

    src_ptr = src_mem; // 0x12, 0x34
    asm volatile(
        ".word %0\n\t" 
        : : "i"(ENCODE_VLN8_V(10, 10, 1)), "r"(src_ptr) : "v10", "memory"
    );

    read_vreg(dst_mem);
    // Element 0: skipped (0x55)
    // Element 1: bits 7:4 of 0x12 -> 0x01
    // Element 2: bits 3:0 of 0x34 -> 0x04
    printf("Result: 0x%02X 0x%02X 0x%02X\n", dst_mem[0], dst_mem[1], dst_mem[2]);
    if (dst_mem[0] == 0x55 && dst_mem[1] == 0x01 && dst_mem[2] == 0x04) {
        printf("PASS Test 4\n");
    } else {
        printf("FAIL Test 4\n");
    }

    return 0;
}
