#ifndef ANDES_V5_ENCODINGS_H
#define ANDES_V5_ENCODINGS_H

/* XAndesVSIntLoad */
/* vln8.v vd, (rs1), vm */
/* 000001 . 00010 rs1 100 vd 1011011 */
#define MATCH_VLN8_V (0x0420405b)
#define ENCODE_VLN8_V(vd, rs1, vm) \
    (MATCH_VLN8_V | ((vm) << 25) | ((rs1) << 15) | ((vd) << 7))

/* vlnu8.v vd, (rs1), vm */
/* 000001 . 00011 rs1 100 vd 1011011 */
#define MATCH_VLNU8_V (0x0430405b)
#define ENCODE_VLNU8_V(vd, rs1, vm) \
    (MATCH_VLNU8_V | ((vm) << 25) | ((rs1) << 15) | ((vd) << 7))

/* XAndesVPackFPH */
/* vfpmadt.vf vd, rs1, vs2, vm */
/* 000010 . vs2 rs1 100 vd 1011011 */
#define MATCH_VFPMADT_VF (0x0800405b)
#define ENCODE_VFPMADT_VF(vd, rs1, vs2, vm) \
    (MATCH_VFPMADT_VF | ((vm) << 25) | ((vs2) << 20) | ((rs1) << 15) | ((vd) << 7))

/* vfpmadb.vf vd, rs1, vs2, vm */
/* 000011 . vs2 rs1 100 vd 1011011 */
#define MATCH_VFPMADB_VF (0x0c00405b)
#define ENCODE_VFPMADB_VF(vd, rs1, vs2, vm) \
    (MATCH_VFPMADB_VF | ((vm) << 25) | ((vs2) << 20) | ((rs1) << 15) | ((vd) << 7))

/* XAndesVDot */
/* vd4dots.vv vd, vs1, vs2, vm */
/* 000100 . vs2 vs1 100 vd 1011011 */
#define MATCH_VD4DOTS_VV (0x1000405b)
#define ENCODE_VD4DOTS_VV(vd, vs1, vs2, vm) \
    (MATCH_VD4DOTS_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* vd4dotu.vv vd, vs1, vs2, vm */
/* 000111 . vs2 vs1 100 vd 1011011 */
#define MATCH_VD4DOTU_VV (0x1c00405b)
#define ENCODE_VD4DOTU_VV(vd, vs1, vs2, vm) \
    (MATCH_VD4DOTU_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* vd4dotsu.vv vd, vs1, vs2, vm */
/* 000101 . vs2 vs1 100 vd 1011011 */
#define MATCH_VD4DOTSU_VV (0x1400405b)
#define ENCODE_VD4DOTSU_VV(vd, vs1, vs2, vm) \
    (MATCH_VD4DOTSU_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* XAndesVSIntH */
/* vle4.v vd, (rs1) */
/* 000001 1 00000 rs1 100 vd 1011011 */
#define MATCH_VLE4_V (0x0600405b)
#define ENCODE_VLE4_V(vd, rs1) \
    (MATCH_VLE4_V | ((rs1) << 15) | ((vd) << 7))

/* vfwcvt.f.n.v vd, vs2, vm */
/* 000000 . 00100 vs2 100 vd 1011011 */
#define MATCH_VFWCVT_F_N_V (0x0002405b)
#define ENCODE_VFWCVT_F_N_V(vd, vs2, vm) \
    (MATCH_VFWCVT_F_N_V | ((vm) << 25) | ((vs2) << 20) | ((vd) << 7))

/* vfwcvt.f.nu.v vd, vs2, vm */
/* 000000 . 00101 vs2 100 vd 1011011 */
#define MATCH_VFWCVT_F_NU_V (0x0002c05b)
#define ENCODE_VFWCVT_F_NU_V(vd, vs2, vm) \
    (MATCH_VFWCVT_F_NU_V | ((vm) << 25) | ((vs2) << 20) | ((vd) << 7))

/* vfwcvt.f.b.v vd, vs2, vm */
/* 000000 . 00110 vs2 100 vd 1011011 */
#define MATCH_VFWCVT_F_B_V (0x0003405b)
#define ENCODE_VFWCVT_F_B_V(vd, vs2, vm) \
    (MATCH_VFWCVT_F_B_V | ((vm) << 25) | ((vs2) << 20) | ((vd) << 7))

/* vfwcvt.f.bu.v vd, vs2, vm */
/* 000000 . 00111 vs2 100 vd 1011011 */
#define MATCH_VFWCVT_F_BU_V (0x0003c05b)
#define ENCODE_VFWCVT_F_BU_V(vd, vs2, vm) \
    (MATCH_VFWCVT_F_BU_V | ((vm) << 25) | ((vs2) << 20) | ((vd) << 7))

/* Standard Extensions for VSIntH (EEW=4) */
/* vzext.vf2 vd, vs2, vm (f6=18, f5=6, f3=2, op=0x57) */
#define MATCH_VZEXT_VF2 (0x48032057)
#define ENCODE_VZEXT_VF2(vd, vs2, vm) \
    (MATCH_VZEXT_VF2 | ((vm) << 25) | ((vs2) << 20) | ((vd) << 7))

/* XAndesVQMac */
/* vqmaccu.vv (f6=60, f3=0, op=0x57) */
#define MATCH_VQMACCU_VV (0xf0000057)
#define ENCODE_VQMACCU_VV(vd, vs1, vs2, vm) \
    (MATCH_VQMACCU_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* vqmacc.vv (f6=61, f3=0, op=0x57) */
#define MATCH_VQMACC_VV (0xf4000057)
#define ENCODE_VQMACC_VV(vd, vs1, vs2, vm) \
    (MATCH_VQMACC_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* vqmaccsu.vv (f6=63, f3=0, op=0x57) */
#define MATCH_VQMACCSU_VV (0xfc000057)
#define ENCODE_VQMACCSU_VV(vd, vs1, vs2, vm) \
    (MATCH_VQMACCSU_VV | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7))

/* XAndesVBFHCvt */
/* vfwcvt.s.bf16 (rs1 field is 0) */
#define MATCH_VFWCVT_S_BF16 (0x0000405b)
#define ENCODE_VFWCVT_S_BF16(vd, vs2) \
    (MATCH_VFWCVT_S_BF16 | ((vs2) << 20) | ((vd) << 7))

/* vfncvt.bf16.s (rs1 field is 1) */
#define MATCH_VFNCVT_BF16_S (0x0000c05b)
#define ENCODE_VFNCVT_BF16_S(vd, vs2) \
    (MATCH_VFNCVT_BF16_S | ((vs2) << 20) | ((vd) << 7))

/* Scalar BF16 */
/* nfcvt.s.bf16 rd, rs2 (rs1 field is 2) */
#define MATCH_NFCVT_S_BF16 (0x0001405b)
#define ENCODE_NFCVT_S_BF16(rd, rs2) \
    (MATCH_NFCVT_S_BF16 | ((rs2) << 20) | ((rd) << 7))

/* nfcvt.bf16.s rd, rs2 (rs1 field is 3) */
#define MATCH_NFCVT_BF16_S (0x0001c05b)
#define ENCODE_NFCVT_BF16_S(rd, rs2) \
    (MATCH_NFCVT_BF16_S | ((rs2) << 20) | ((rd) << 7))

#endif
