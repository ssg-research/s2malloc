#ifndef haesni
#define haesni

#include "defines.h"
#include <string.h>     //for memcmp

#ifdef __x86_64__
#include <wmmintrin.h>  //for intrinsics for AES-NI
//compile using gcc and following arguments: -g;-O0;-Wall;-msse2;-msse;-march=native;-maes
#else
#include "sse2neon.h"
#endif


//internal stuff

//macros
#define DO_ENC_BLOCK(m,k) \
	do{\
        m = _mm_xor_si128       (m, k[ 0]); \
        m = _mm_aesenc_si128    (m, k[ 1]); \
        m = _mm_aesenc_si128    (m, k[ 2]); \
        m = _mm_aesenc_si128    (m, k[ 3]); \
        m = _mm_aesenc_si128    (m, k[ 4]); \
        m = _mm_aesenc_si128    (m, k[ 5]); \
        m = _mm_aesenc_si128    (m, k[ 6]); \
        m = _mm_aesenc_si128    (m, k[ 7]); \
        m = _mm_aesenc_si128    (m, k[ 8]); \
        m = _mm_aesenc_si128    (m, k[ 9]); \
        m = _mm_aesenclast_si128(m, k[10]);\
    }while(0)

//#define AES_128_key_exp(k, rcon) aes_128_key_expansion(k, _mm_aeskeygenassist_si128(k, rcon))

static unsigned char const_Rb[16] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

static __m128i key_schedule[11];//the expanded key

//static inline __m128i aes_128_key_expansion(__m128i key, __m128i keygened) {
//    keygened = _mm_shuffle_epi32(keygened, _MM_SHUFFLE(3,3,3,3));
//    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
//    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
//    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
//    return _mm_xor_si128(key, keygened);
//}

//public API
//static inline void aes128_load_key(unsigned char* enc_key) {
//    key_schedule[0] = _mm_loadu_si128((const __m128i*) enc_key);
//    key_schedule[1]  = AES_128_key_exp(key_schedule[0], 0x01);
//    key_schedule[2]  = AES_128_key_exp(key_schedule[1], 0x02);
//    key_schedule[3]  = AES_128_key_exp(key_schedule[2], 0x04);
//    key_schedule[4]  = AES_128_key_exp(key_schedule[3], 0x08);
//    key_schedule[5]  = AES_128_key_exp(key_schedule[4], 0x10);
//    key_schedule[6]  = AES_128_key_exp(key_schedule[5], 0x20);
//    key_schedule[7]  = AES_128_key_exp(key_schedule[6], 0x40);
//    key_schedule[8]  = AES_128_key_exp(key_schedule[7], 0x80);
//    key_schedule[9]  = AES_128_key_exp(key_schedule[8], 0x1B);
//    key_schedule[10] = AES_128_key_exp(key_schedule[9], 0x36);
//}


static inline void xor_128(const unsigned char *a, const unsigned char *b, unsigned char *out) {
    int i;
    for (i = 0; i < AES128_BYTE; i++) {
        out[i] = a[i] ^ b[i];
    }
}


static inline void aes128_enc(unsigned char* plainText, unsigned char* cipherText) {
    __m128i m = _mm_loadu_si128((__m128i *) plainText);

    DO_ENC_BLOCK(m,key_schedule);

    _mm_storeu_si128((__m128i *) cipherText, m);
}


static inline void padding (const unsigned char *lastb, unsigned char *pad) {
    size_t j;
    for (j = 0; j < ADDR_BYTE; j++) {
        pad[j] = lastb[j];
    }

    pad[ADDR_BYTE] = 0x80;

    for (j = ADDR_BYTE + 1; j < AES128_BYTE; j++) {
        pad[j] = 0x00;
    }
}


static inline void leftshift_onebit(const unsigned char *input,unsigned char *output) {
    int i;
    unsigned char overflow = 0;

    for (i = AES128_BYTE - 1; i >= 0; i--) {
        output[i] = input[i] << 1;
        output[i] |= overflow;
        overflow = (input[i] & 0x80)?1:0;
    }
}


static inline void generate_subkey(unsigned char *key, unsigned char *K1, unsigned
char *K2) {
    unsigned char L[AES128_BYTE];
    unsigned char Z[AES128_BYTE];
    unsigned char tmp[AES128_BYTE];
    int i;

    for (i = 0; i < AES128_BYTE; i++ ) Z[i] = 0;

    aes128_enc( Z, L);

    if ((L[0] & 0x80) == 0 ) { /* If MSB(L) = 0, then K1 = L << 1 */
        leftshift_onebit(L,K1);
    } else {    /* Else K1 = ( L << 1 ) (+) Rb */
        leftshift_onebit(L,tmp);
        xor_128(tmp,const_Rb,K1);
    }

    if ((K1[0] & 0x80) == 0 ) {
        leftshift_onebit(K1,K2);
    } else {
        leftshift_onebit(K1,tmp);
        xor_128(tmp,const_Rb,K2);
    }
}


static inline void aes128_hmac(unsigned char* K1, unsigned char* K2, unsigned char* input, unsigned char* mac) {
    unsigned char X[AES128_BYTE], Y[AES128_BYTE], M_last[AES128_BYTE], padded[AES128_BYTE];
//    unsigned char K1[AES128_BYTE], K2[AES128_BYTE];
//    generate_subkey(key, K1, K2);

    padding(input, padded);
    xor_128(padded,K2,M_last);
    for (size_t i = 0; i < AES128_BYTE; i++) X[i] = 0;

    xor_128(X,M_last,Y);

    aes128_enc(Y, X);

    for (size_t i = 0; i < AES128_BYTE; i++) {
        mac[i] = X[i];
    }
}

static inline void hash1(unsigned char* K1, unsigned char* K2, uint64_t input, unsigned char* canary) {
    unsigned char input_c[ADDR_BYTE];
    input_c[0] = (input >> 56) & 0xff;
    input_c[1] = (input >> 48) & 0xff;
    input_c[2] = (input >> 40) & 0xff;
    input_c[3] = (input >> 32) & 0xff;
    input_c[4] = (input >> 24) & 0xff;
    input_c[5] = (input >> 16) & 0xff;
    input_c[6] = (input >> 8)  & 0xff;
    input_c[7] =  input        & 0xff;

    unsigned char mac[AES128_BYTE];
    aes128_hmac(K1, K2, input_c, mac);

    *canary = mac[0];
    for (size_t i = 1; i < AES128_BYTE; i++) {
        *canary ^= mac[i];
    }

//    Debug("key: %02x%02x%02x%02x%02x%02x%02x%02x", key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
//    Debug("%02x%02x%02x%02x%02x%02x%02x%02x\n", key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
//    Debug("input: %p, canary %02x\n", input, *canary);
}


static inline void hashn(unsigned char* K1, unsigned char* K2, uint64_t input, unsigned char* canary) {
    unsigned char input_c[ADDR_BYTE];
    input_c[0] = (input >> 56) & 0xff;
    input_c[1] = (input >> 48) & 0xff;
    input_c[2] = (input >> 40) & 0xff;
    input_c[3] = (input >> 32) & 0xff;
    input_c[4] = (input >> 24) & 0xff;
    input_c[5] = (input >> 16) & 0xff;
    input_c[6] = (input >> 8)  & 0xff;
    input_c[7] =  input        & 0xff;

    unsigned char mac[AES128_BYTE];
    aes128_hmac(K1, K2, input_c, mac);

    size_t i = 0;

#if EMPTY_CANARY_BYTE_LENGTH <= AES128_BYTE
    for (; i < EMPTY_CANARY_BYTE_LENGTH; i++) {
        canary[i] = mac[i];
    }

    for (; i < AES128_BYTE; i++) {
        canary[i % EMPTY_CANARY_BYTE_LENGTH] ^= mac[i];
    }
#else
    while (i < EMPTY_CANARY_BYTE_LENGTH) {
        canary[i] = mac[i % AES128_BYTE];
        i++;
    }
#endif
}

#endif
