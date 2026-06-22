#!/usr/bin/env python3
"""Try different GF(2^64) polynomials."""

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

def bytes_to_int_be(b):
    r = 0
    for byte in b: r = (r << 8) | byte
    return r

def to_bytes_be(n, length):
    return bytes([(n >> (8*(length-1-i))) & 0xFF for i in range(length)])

def l_transform(block, poly):
    qws = [bytes_to_int_be(block[i*8:(i+1)*8]) for i in range(8)]
    result = 0
    for i in range(8):
        result ^= gf_mul64(qws[i], L_VEC[i], poly)
    return result

def R_right(block, poly):
    l_val = l_transform(block, poly)
    new_block = bytearray(64)
    for j in range(8): new_block[j] = (l_val >> (8*(7-j))) & 0xFF
    for i in range(56): new_block[8+i] = block[i]
    return bytes(new_block)

def L_func(block, poly):
    for _ in range(8): block = R_right(block, poly)
    return block

def Vec_512(j):
    result = bytearray(64)
    j_bytes = to_bytes_be(j, 8)
    for i in range(8): result[56+i] = j_bytes[i]
    return bytes(result)

expected = "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094"
print(f"Expected C[0]: {expected}\n")

# Try many different polynomials
polys = [
    ("0x1C3", 0x1C3),
    ("0x1B", 0x1B),
    ("0x11B", 0x11B),
    ("0x11D", 0x11D),
    ("0x105", 0x105),
    ("0x107", 0x107),
    ("0x10D", 0x10D),
    ("0x11F", 0x11F),
    ("0x12D", 0x12D),
    ("0x13D", 0x13D),
    ("0x153", 0x153),
    ("0x163", 0x163),
    ("0x187", 0x187),
    ("0x18D", 0x18D),
    ("0x1A9", 0x1A9),
    ("0x1CF", 0x1CF),
    ("0x1F7", 0x1F7),
    ("0x1FB", 0x1FB),
    ("0x1FD", 0x1FD),
    ("0x1FF", 0x1FF),
]

vec = Vec_512(1)
for name, poly in polys:
    c0 = L_func(vec, poly)
    c0_hex = ''.join(f'{b:02x}' for b in c0)
    match = " *** MATCH!" if c0_hex == expected else ""
    print(f"Poly {name:6s}: {c0_hex[:40]}...{match}")
