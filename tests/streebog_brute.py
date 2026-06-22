#!/usr/bin/env python3
"""Try all byte ordering combinations for C[0]."""

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

def to_bytes_le(n, length):
    return bytes([(n >> (8*i)) & 0xFF for i in range(length)])

def to_bytes_be(n, length):
    return bytes([(n >> (8*(length-1-i))) & 0xFF for i in range(length)])

def bytes_to_int_le(b):
    r = 0
    for i in range(len(b)-1, -1, -1):
        r = (r << 8) | b[i]
    return r

def bytes_to_int_be(b):
    r = 0
    for byte in b:
        r = (r << 8) | byte
    return r

def get_qwords(block, be):
    if be:
        return [bytes_to_int_be(block[i*8:(i+1)*8]) for i in range(8)]
    else:
        return [bytes_to_int_le(block[i*8:(i+1)*8]) for i in range(8)]

def put_qwords_be(qws):
    result = bytearray(64)
    for i in range(8):
        for j in range(8):
            result[i*8+j] = (qws[i] >> (8*(7-j))) & 0xFF
    return bytes(result)

def put_qwords_le(qws):
    result = bytearray(64)
    for i in range(8):
        for j in range(8):
            result[i*8+j] = (qws[i] >> (8*j)) & 0xFF
    return bytes(result)

def l_transform(block, qword_be):
    qws = get_qwords(block, qword_be)
    result = 0
    for i in range(8):
        result ^= gf_mul64(qws[i], L_VEC[i])
    return result

def R_right(block, qword_be, out_be):
    l_val = l_transform(block, qword_be)
    new_block = bytearray(64)
    if out_be:
        for j in range(8):
            new_block[j] = (l_val >> (8*(7-j))) & 0xFF
    else:
        for j in range(8):
            new_block[j] = (l_val >> (8*j)) & 0xFF
    for i in range(56):
        new_block[8+i] = block[i]
    return bytes(new_block)

def R_left(block, qword_be, out_be):
    l_val = l_transform(block, qword_be)
    new_block = bytearray(64)
    for i in range(56):
        new_block[i] = block[8+i]
    if out_be:
        for j in range(8):
            new_block[56+j] = (l_val >> (8*(7-j))) & 0xFF
    else:
        for j in range(8):
            new_block[56+j] = (l_val >> (8*j)) & 0xFF
    return bytes(new_block)

def L_transform(block, r_func, qword_be, out_be):
    result = block
    for _ in range(8):
        result = r_func(result, qword_be, out_be)
    return result

def Vec_512(j, be):
    result = bytearray(64)
    if be:
        j_bytes = to_bytes_be(j, 8)
        for i in range(8):
            result[56+i] = j_bytes[i]
    else:
        j_bytes = to_bytes_le(j, 8)
        for i in range(8):
            result[i] = j_bytes[i]
    return bytes(result)

expected = "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094"
print(f"Expected C[0]: {expected}\n")

# Try all 8 combinations
for vec_be in [True, False]:
    for qword_be in [True, False]:
        for r_func_name, r_func in [("R_right", R_right), ("R_left", R_left)]:
            vec = Vec_512(1, vec_be)
            c0 = L_transform(vec, r_func, qword_be, qword_be)
            c0_hex = ''.join(f'{b:02x}' for b in c0)
            match = "MATCH!" if c0_hex == expected else ""
            print(f"vec_be={vec_be} qword_be={qword_be} {r_func_name}: {c0_hex[:32]}... {match}")
