

#ifndef __SHA256_H__
#define __SHA256_H__

#include "gma_include.h"

/****************************** MACROS ******************************/
//#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

typedef struct {
    BYTE data[64];
    WORD datalen;
    unsigned long long bitlen;
    WORD state[8];
} SHA256_CTX;

/*********************** FUNCTION DECLARATIONS **********************/
void ali_sha256_init(SHA256_CTX *ctx);
void ali_sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void ali_sha256_final(SHA256_CTX *ctx, BYTE hash[]);

#endif /* __SHA256_H__ */


