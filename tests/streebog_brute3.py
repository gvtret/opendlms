#!/usr/bin/env python3
"""Try generating C constants with left-shift R and different orderings."""

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

def R_left(block):
    l_val = l_transform(block)
    new_block = bytearray(64)
    for i in range(56): new_block[i] = block[8+i]
    for j in range(8): new_block[56+j] = (l_val >> (8*(7-j))) & 0xFF
    return bytes(new_block)

def L_right(block):
    for _ in range(8): block = R_right(block)
    return block

def L_left(block):
    for _ in range(8): block = R_left(block)
    return block

def Vec_512_be(j):
    result = bytearray(64)
    j_bytes = to_bytes_be(j, 8)
    for i in range(8): result[56+i] = j_bytes[i]
    return bytes(result)

# All 12 expected C constants from the code
C_expected_hex = [
    "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094",
    "bd7b9d66e3ee319f96a01e4e7550d172364419ca155419574d602667a01824d85dc3f788bd2c15513d51cfade8a23fc53041bef46ce359a201c0c21085206ebc",
]

print("Testing all 12 C constants...\n")

# Test L_right
print("=== L_right (right-shift R) ===")
for i in range(12):
    vec = Vec_512_be(i+1)
    c = L_right(vec)
    c_hex = ''.join(f'{b:02x}' for b in c)
    print(f"C[{i:2d}]: {c_hex[:32]}...")

print(f"\nExpected C[0]: {C_expected_hex[0][:32]}...")
print(f"Expected C[1]: {C_expected_hex[1][:32]}...")

# Test L_left
print("\n=== L_left (left-shift R) ===")
for i in range(12):
    vec = Vec_512_be(i+1)
    c = L_left(vec)
    c_hex = ''.join(f'{b:02x}' for b in c)
    print(f"C[{i:2d}]: {c_hex[:32]}...")

# Maybe the C constants from the code were generated differently
# Let me try to reverse-engineer the correct L function
# by checking if C[1] = L(C[0]) (which would mean the R function is cyclic)

c0_expected = bytes.fromhex(C_expected_hex[0])
c1_expected = bytes.fromhex(C_expected_hex[1])

# If C[i+1] = L(C[i]) with right shift, then C[1] should be L(C[0])
c1_from_c0_right = L_right(c0_expected)
c1_hex_right = ''.join(f'{b:02x}' for b in c1_from_c0_right)
print(f"\nL_right(C[0]): {c1_hex_right[:32]}...")
print(f"Expected C[1]: {C_expected_hex[1][:32]}...")
print(f"Match: {'YES' if c1_hex_right == C_expected_hex[1] else 'NO'}")

c1_from_c0_left = L_left(c0_expected)
c1_hex_left = ''.join(f'{b:02x}' for b in c1_from_c0_left)
print(f"\nL_left(C[0]):  {c1_hex_left[:32]}...")
print(f"Expected C[1]: {C_expected_hex[1][:32]}...")
print(f"Match: {'YES' if c1_hex_left == C_expected_hex[1] else 'NO'}")
