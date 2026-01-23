#include <stdio.h>
#include <stdint.h>
#include "trap_utils.h"

int main() {
    register_exception_handler();
    printf("Starting XAndesPerf tests...\n");

    long rd_val, taken;
    long val = 5;

    // 1. Branch: nds.bbc
    taken = 0;
    asm volatile(
        "nds.bbc %1, 1, 1f\n\t"
        "li %0, 0\n\t"
        "j 2f\n\t"
        "1: li %0, 1\n\t"
        "2:"
        : "=r"(taken) : "r"(val)
    );
    if (taken == 1) printf("PASS nds.bbc\n"); else printf("FAIL nds.bbc\n");

    // 2. LEA
    long rs1 = 10, rs2 = 5;
    asm volatile("nds.lea.h %0, %1, %2" : "=r"(rd_val) : "r"(rs1), "r"(rs2));
    if (rd_val == 20) printf("PASS nds.lea.h\n"); else printf("FAIL nds.lea.h: %ld\n", rd_val);

    // 3. Bit Field
    val = 0xF0;
    asm volatile("nds.bfos %0, %1, 7, 4" : "=r"(rd_val) : "r"(val));
    if (rd_val == -1) printf("PASS nds.bfos\n"); else printf("FAIL nds.bfos: %ld\n", rd_val);

    // 4. String
    val = 0x04030201;
    long target = 0x03;
    asm volatile("nds.ffb %0, %1, %2" : "=r"(rd_val) : "r"(val), "r"(target));
    if (rd_val == -6) printf("PASS nds.ffb\n"); else printf("FAIL nds.ffb: %ld\n", rd_val);

    return 0;
}