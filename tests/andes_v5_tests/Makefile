CC = riscv64-elf-gcc
CFLAGS = -mvh -static -O2 -I.

TESTS = test_perf test_bf16 test_vpack_fp16 test_vdot test_vsmall_int test_vqmac test_codense test_vfwcvt test_vle4 test_vln8

all: $(TESTS)

test_perf: test_perf.c trap_utils.h
	$(CC) $(CFLAGS) -o $@ $<

test_bf16: test_bf16.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vpack_fp16: test_vpack_fp16.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vdot: test_vdot.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vsmall_int: test_vsmall_int.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vqmac: test_vqmac.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_codense: test_codense.c trap_utils.h
	$(CC) $(CFLAGS) -o $@ $<

test_vfwcvt: test_vfwcvt.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vle4: test_vle4.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

test_vln8: test_vln8.c trap_utils.h andes_v5_encodings.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TESTS)
