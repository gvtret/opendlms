#!/usr/bin/env python3
"""Generate the final streebog.c C constants block."""

L_VEC = [
    0x94141c3c08421001, 0x8020080020501080,
    0x4010104000020488, 0x0800202000808802,
    0x8020880140000000, 0x0100008004000840,
    0x8080010000802010, 0x8000010002010401
]
POLY = 0x1C3

def gf_mul64(a, b):
    r = 0
    for _ in range(64):
        if b & 1: r ^= a
        hi = a >> 63
        a = (a << 1) & 0xFFFFFFFFFFFFFFFF
        if hi: a ^= POLY
        b >>= 1
    return r

def bytes_to_int_be(b):
    r = 0
    for byte in b: r = (r << 8) | byte
    return r

def to_bytes_be(n, length):
    return bytes([(n >> (8*(length-1-i))) & 0xFF for i in range(length)])

def l_transform(block):
    qws = [bytes_to_int_be(block[i*8:(i+1)*8]) for i in range(8)]
    result = 0
    for i in range(8):
        result ^= gf_mul64(qws[i], L_VEC[i])
    return result

def R_right(block):
    l_val = l_transform(block)
    new_block = bytearray(64)
    for j in range(8): new_block[j] = (l_val >> (8*(7-j))) & 0xFF
    for i in range(56): new_block[8+i] = block[i]
    return bytes(new_block)

def L_func(block):
    for _ in range(8): block = R_right(block)
    return block

def Vec_512(j):
    result = bytearray(64)
    j_bytes = to_bytes_be(j, 8)
    for i in range(8): result[56+i] = j_bytes[i]
    return bytes(result)

C = [L_func(Vec_512(i+1)) for i in range(12)]

print("static const uint8_t C[12][64] = {")
for i in range(12):
    c = C[i]
    rows = []
    for row in range(8):
        start = row * 8
        hex_str = ','.join(f'0x{c[j]:02x}' for j in range(start, start+8))
        rows.append(f'     {hex_str}')
    print(f'    {{')
    for j, row in enumerate(rows):
        if j < 7:
            print(f'{row},')
        else:
            print(f'{row}}},')
print("};")
