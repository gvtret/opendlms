#!/usr/bin/env python3
"""Try reversed alpha mapping and different polynomials for C[0]."""

L_VEC = [
    0x94141c3c08421001, 0x8020080020501080,
    0x4010104000020488, 0x0800202000808802,
    0x8020880140000000, 0x0100008004000840,
    0x8080010000802010, 0x8000010002010401
]

def gf_mul64(a, b, poly):
    r = 0
    for _ in range(64):
        if b & 1: r ^= a
        hi = a >> 63
        a = (a << 1) & 0xFFFFFFFFFFFFFFFF
        if hi: a ^= poly
        b >>= 1
    return r

def to_bytes_be(n, length):
    return bytes([(n >> (8*(length-1-i))) & 0xFF for i in range(length)])

def bytes_to_int_be(b):
    r = 0
    for byte in b:
        r = (r << 8) | byte
    return r

def l_transform(block, alpha, poly):
    qws = [bytes_to_int_be(block[i*8:(i+1)*8]) for i in range(8)]
    result = 0
    for i in range(8):
        result ^= gf_mul64(qws[i], alpha[i], poly)
    return result

def R_right(block, alpha, poly):
    l_val = l_transform(block, alpha, poly)
    new_block = bytearray(64)
    for j in range(8):
        new_block[j] = (l_val >> (8*(7-j))) & 0xFF
    for i in range(56):
        new_block[8+i] = block[i]
    return bytes(new_block)

def L_transform(block, alpha, poly):
    result = block
    for _ in range(8):
        result = R_right(result, alpha, poly)
    return result

def Vec_512(j):
    result = bytearray(64)
    j_bytes = to_bytes_be(j, 8)
    for i in range(8):
        result[56+i] = j_bytes[i]
    return bytes(result)

expected = "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094"
print(f"Expected C[0]: {expected}\n")

# Try reversed alpha mapping
alpha_rev = list(reversed(L_VEC))
vec = Vec_512(1)
c0 = L_transform(vec, alpha_rev, 0x1C3)
c0_hex = ''.join(f'{b:02x}' for b in c0)
print(f"Reversed alpha: {c0_hex[:32]}... {'MATCH!' if c0_hex == expected else ''}")

# Try different polynomials
for poly_name, poly_val in [("0x1C3", 0x1C3), ("0x11B", 0x11B), ("0x1B", 0x1B)]:
    c0 = L_transform(vec, L_VEC, poly_val)
    c0_hex = ''.join(f'{b:02x}' for b in c0)
    print(f"Poly {poly_name}: {c0_hex[:32]}... {'MATCH!' if c0_hex == expected else ''}")

# Also try reversed alpha with different poly
for poly_name, poly_val in [("0x1C3", 0x1C3), ("0x11B", 0x11B)]:
    c0 = L_transform(vec, alpha_rev, poly_val)
    c0_hex = ''.join(f'{b:02x}' for b in c0)
    print(f"Rev alpha + {poly_name}: {c0_hex[:32]}... {'MATCH!' if c0_hex == expected else ''}")

# Maybe the l_vec values are in a different order? Try cyclic shifts
for shift in range(8):
    alpha_s = [L_VEC[(i+shift) % 8] for i in range(8)]
    c0 = L_transform(vec, alpha_s, 0x1C3)
    c0_hex = ''.join(f'{b:02x}' for b in c0)
    print(f"Alpha shift {shift}: {c0_hex[:32]}... {'MATCH!' if c0_hex == expected else ''}")
