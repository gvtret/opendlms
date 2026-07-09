/* Check C[0] with both R directions */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static const uint64_t l_vec_ref[8] = {0x94141c3c08421001ULL, 0x8020080020501080ULL, 0x4010104000020488ULL, 0x0800202000808802ULL,
                                      0x8020880140000000ULL, 0x0100008004000840ULL, 0x8080010000802010ULL, 0x8000010002010401ULL};

static uint64_t gf_mul64(uint64_t a, uint64_t b) {
	uint64_t r = 0;
	for (int i = 0; i < 64; i++) {
		if (b & 1ULL)
			r ^= a;
		uint64_t hi = a >> 63;
		a <<= 1;
		if (hi)
			a ^= 0x1C3ULL;
		b >>= 1;
	}
	return r;
}

static uint64_t get_u64_be(const uint8_t *p) {
	uint64_t v = 0;
	for (int i = 0; i < 8; i++)
		v = (v << 8) | p[i];
	return v;
}

static void put_u64_be(uint8_t *p, uint64_t v) {
	for (int i = 7; i >= 0; i--) {
		p[i] = (uint8_t)(v & 0xFF);
		v >>= 8;
	}
}

static void compute_l(uint8_t block[64], uint8_t out[64]) {
	uint8_t v[8];
	uint64_t l_result = 0;
	for (int i = 0; i < 8; i++) {
		l_result ^= gf_mul64(get_u64_be(block + i * 8), l_vec_ref[i]);
	}
	put_u64_be(v, l_result);
	memcpy(out, block, 56);
	memmove(out + 8, out, 56);
	memcpy(out, v, 8);
}

static void R_right(uint8_t block[64]) {
	uint8_t v[8];
	uint64_t l_result = 0;
	for (int i = 0; i < 8; i++) {
		l_result ^= gf_mul64(get_u64_be(block + i * 8), l_vec_ref[i]);
	}
	put_u64_be(v, l_result);
	memmove(block + 8, block, 56);
	memcpy(block, v, 8);
}

static void R_left(uint8_t block[64]) {
	uint8_t v[8];
	uint64_t l_result = 0;
	for (int i = 0; i < 8; i++) {
		l_result ^= gf_mul64(get_u64_be(block + i * 8), l_vec_ref[i]);
	}
	put_u64_be(v, l_result);
	memmove(block, block + 8, 56);
	memcpy(block + 56, v, 8);
}

static void print_hex(const char *label, const uint8_t *data, int len) {
	printf("%s: ", label);
	for (int i = 0; i < len; i++)
		printf("%02x", data[i]);
	printf("\n");
}

int main(void) {
	/* Test R_right with Vec_512(1) */
	uint8_t vec[64];
	memset(vec, 0, 64);
	vec[63] = 0x01;
	R_right(vec);
	printf("R_right(Vec_512(1)):  ");
	for (int i = 0; i < 64; i++)
		printf("%02x", vec[i]);
	printf("\n");

	/* Test R_left with Vec_512(1) */
	memset(vec, 0, 64);
	vec[63] = 0x01;
	R_left(vec);
	printf("R_left(Vec_512(1)):   ");
	for (int i = 0; i < 64; i++)
		printf("%02x", vec[i]);
	printf("\n");

	/* Compute L with R_right */
	memset(vec, 0, 64);
	vec[63] = 0x01;
	for (int i = 0; i < 8; i++)
		R_right(vec);
	printf("L_right(Vec_512(1)):  ");
	for (int i = 0; i < 64; i++)
		printf("%02x", vec[i]);
	printf("\n");

	/* Compute L with R_left */
	memset(vec, 0, 64);
	vec[63] = 0x01;
	for (int i = 0; i < 8; i++)
		R_left(vec);
	printf("L_left(Vec_512(1)):   ");
	for (int i = 0; i < 64; i++)
		printf("%02x", vec[i]);
	printf("\n");

	printf(
	    "Expected C[0]:        35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094\n"
	);

	return 0;
}
