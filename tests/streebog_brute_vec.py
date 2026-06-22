#!/usr/bin/env python3
"""Try all possible Vec_512(1) byte positions."""
L_VEC = [0x94141c3c08421001, 0x8020080020501080, 0x4010104000020488, 0x0800202000808802,
         0x8020880140000000, 0x0100008004000840, 0x8080010000802010, 0x8000010002010401]
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

def b2i_be(b):
    r = 0
    for byte in b: r = (r << 8) | byte
    return r

def l_func(block):
    qws = [b2i_be(block[i*8:(i+1)*8]) for i in range(8)]
    r = 0
    for i in range(8): r ^= gf_mul64(qws[i], L_VEC[i])
    return r

def R(block):
    lv = l_func(block)
    nb = bytearray(64)
    for j in range(8): nb[j] = (lv >> (8*(7-j))) & 0xFF
    for i in range(56): nb[8+i] = block[i]
    return bytes(nb)

def L(block):
    for _ in range(8): block = R(block)
    return block

expected = "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094"

# Try each byte position for Vec_512(1)
for byte_pos in range(64):
    for bit_val in [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80]:
        vec = bytearray(64)
        vec[byte_pos] = bit_val
        c0 = L(bytes(vec))
        c0_hex = ''.join(f'{b:02x}' for b in c0)
        if c0_hex == expected:
            print(f"FOUND! byte_pos={byte_pos} bit=0x{bit_val:02x}")

print("Done searching")
