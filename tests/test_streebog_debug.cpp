#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../cosemlib/crypto/streebog.h"

int main() {
    uint8_t block[64];
    memset(block, 0, 64);
    block[63] = 1;
    streebog256_L(block);
    printf("C[0] = ");
    for (int i = 0; i < 64; i++) printf("%02x", block[i]);
    printf("\n");
    return 0;
}
