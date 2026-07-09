#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cosemlib/crypto/streebog.h"

static void hex_to_bytes(const char *hex, uint8_t *out, size_t len) {
	for (size_t i = 0; i < len; i++) {
		unsigned int b;
		sscanf(hex + 2 * i, "%02x", &b);
		out[i] = (uint8_t)b;
	}
}

int main(void) {
	uint8_t digest[32];

	streebog256(NULL, 0, digest);
	printf("Empty:  ");
	for (int i = 0; i < 32; i++)
		printf("%02x", digest[i]);
	printf("\n");
	printf("Expected: 3f5b11e2a8c30975dc351857a5f5593271c4d34499eaff0e8459894c5a896e47\n");

	const char *hash_data_hex = "00112233445566778899aabbccddeeff"
	                            "00112233445566778899aabbccddeeff"
	                            "ff00ee11dd22cc33"
	                            "bb44aa5599668877"
	                            "8899aabbccddeeff88889999aaaabbbb"
	                            "ccccddddeeeeffff89abcdeffedcba98"
	                            "00112233445566770000111122223333"
	                            "44445555666677770123456776543210";
	uint8_t hash_data[112];
	hex_to_bytes(hash_data_hex, hash_data, 112);

	streebog256(hash_data, 112, digest);
	printf("HLS9_C:  ");
	for (int i = 0; i < 32; i++)
		printf("%02x", digest[i]);
	printf("\n");
	printf("Expected: 4c375b843898b6f0a0744051f74e42f2a944581d46c495e743e97abdcd9d7c58\n");

	return 0;
}
