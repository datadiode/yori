// Modified from https://github.com/albertony
// SPDX-License-Identifier: MIT

typedef struct CPUFeatures_ {
    int has_neon; // ARM specific (Advanced SIMD extension for ARM)
    int has_armcrypto;
    int has_sse2;
    int has_sse3;
    int has_ssse3;
    int has_sse41;
    int has_sse42;
    int has_avx;
    int has_avx2;
    int has_avx512f;
    int has_pclmul;
    int has_aesni;
    int has_rdrand;
} CPUFeatures;

#ifdef __cplusplus
extern "C"
#endif
int get_cpu_features(CPUFeatures *cpu_features);
