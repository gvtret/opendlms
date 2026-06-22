#!/usr/bin/env python3
"""Complete Streebog-256 reference implementation for verifying constants."""

# S-box (same as Kuznyechik)
SB = [
    0xFC, 0xEE, 0xDD, 0x11, 0xCF, 0x6E, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0x93, 0x2E, 0x99, 0xBA,
    0x17, 0x36, 0xF1, 0xBB, 0x14, 0xCD, 0x5F, 0xC1,
    0xF9, 0x18, 0x65, 0x5A, 0xE2, 0x5C, 0xEF, 0x21,
    0x81, 0x1C, 0x3C, 0x42, 0x8B, 0x01, 0x8E, 0x4F,
    0x05, 0x84, 0x02, 0xAE, 0xE3, 0x6A, 0x8F, 0xA0,
    0x06, 0x0B, 0xED, 0x98, 0x7F, 0xD4, 0xD3, 0x1F,
    0xEB, 0x34, 0x2C, 0x51, 0xEA, 0xC8, 0x48, 0xAB,
    0xF2, 0x2A, 0x68, 0xA2, 0xFD, 0x3A, 0xCE, 0xCC,
    0xB5, 0x70, 0x0E, 0x56, 0x08, 0x0C, 0x76, 0x12,
    0xBF, 0x72, 0x13, 0x47, 0x9C, 0xB7, 0x5D, 0x87,
    0x15, 0xA1, 0x96, 0x29, 0x10, 0x7B, 0x9A, 0xC7,
    0xF3, 0x91, 0x78, 0x6F, 0x9D, 0x9E, 0xB2, 0xB1,
    0x32, 0x75, 0x19, 0x3D, 0xFF, 0x35, 0x8A, 0x7E,
    0x6D, 0x54, 0xC6, 0x80, 0xC3, 0xBD, 0x0D, 0x57,
    0xDF, 0xF5, 0x24, 0xA9, 0x3E, 0xA8, 0x43, 0xC9,
    0xD7, 0x79, 0xD6, 0xF6, 0x7C, 0x22, 0xB9, 0x03,
    0xE0, 0x0F, 0xEC, 0xDE, 0x7A, 0x94, 0xB0, 0xBC,
    0xDC, 0xE8, 0x28, 0x50, 0x4E, 0x33, 0x0A, 0x4A,
    0xA7, 0x97, 0x60, 0x73, 0x1E, 0x00, 0x62, 0x44,
    0x1A, 0xB8, 0x38, 0x82, 0x64, 0x9F, 0x26, 0x41,
    0xAD, 0x45, 0x46, 0x92, 0x27, 0x5E, 0x55, 0x2F,
    0x8C, 0xA3, 0xA5, 0x7D, 0x69, 0xD5, 0x95, 0x3B,
    0x07, 0x58, 0xB3, 0x40, 0x86, 0xAC, 0x1D, 0xF7,
    0x30, 0x37, 0x6B, 0xE4, 0x88, 0xD9, 0xE7, 0x89,
    0xE1, 0x1B, 0x83, 0x49, 0x4C, 0x3F, 0xF8, 0xFE,
    0x8D, 0x53, 0xAA, 0x90, 0xCA, 0xD8, 0x85, 0x61,
    0x20, 0x71, 0x67, 0xA4, 0x2D, 0x2B, 0x09, 0x5B,
    0xCB, 0x9B, 0x25, 0xD0, 0xBE, 0xE5, 0x6C, 0x52,
    0x59, 0xA6, 0x74, 0xD2, 0xE6, 0xF4, 0xB4, 0xC0,
    0xD1, 0x66, 0xAF, 0xC2, 0x39, 0x4B, 0x63, 0xB6
]

# l_vec constants (α values for linear transformation l)
L_VEC = [
    0x94141c3c08421001, 0x8020080020501080,
    0x4010104000020488, 0x0800202000808802,
    0x8020880140000000, 0x0100008004000840,
    0x8080010000802010, 0x8000010002010401
]

# GF(2^64) polynomial: x^64 + x^8 + x^7 + x^6 + x + 1
POLY = 0x1C3

def gf_mul64(a, b):
    """Multiply in GF(2^64) with the given irreducible polynomial."""
    r = 0
    for _ in range(64):
        if b & 1:
            r ^= a
        hi = a >> 63
        a = (a << 1) & 0xFFFFFFFFFFFFFFFF
        if hi:
            a ^= POLY
        b >>= 1
    return r

def to_bytes_le(n, length):
    """Convert integer to little-endian byte array."""
    return bytes([(n >> (8*i)) & 0xFF for i in range(length)])

def to_bytes_be(n, length):
    """Convert integer to big-endian byte array."""
    return bytes([(n >> (8*(length-1-i))) & 0xFF for i in range(length)])

def bytes_to_int_le(b):
    """Convert little-endian bytes to integer."""
    r = 0
    for i in range(len(b)-1, -1, -1):
        r = (r << 8) | b[i]
    return r

def bytes_to_int_be(b):
    """Convert big-endian bytes to integer."""
    r = 0
    for byte in b:
        r = (r << 8) | byte
    return r

# Block is 64 bytes, treated as 8 big-endian qwords for the l function
def get_qwords_be(block):
    """Get 8 qwords from block (big-endian byte order)."""
    return [bytes_to_int_be(block[i*8:(i+1)*8]) for i in range(8)]

def put_qwords_be(qws):
    """Convert 8 qwords to 64 bytes (big-endian byte order)."""
    result = bytearray(64)
    for i in range(8):
        for j in range(8):
            result[i*8+j] = (qws[i] >> (8*(7-j))) & 0xFF
    return bytes(result)

def l_transform(block):
    """Linear transformation l: 64 bytes -> 8 bytes."""
    qws = get_qwords_be(block)
    result = 0
    for i in range(8):
        result ^= gf_mul64(qws[i], L_VEC[i])
    # Return as 8 big-endian bytes
    return result

def R(block):
    """One round of the linear transformation R (right shift)."""
    block = bytearray(block)
    l_val = l_transform(block)
    # Shift right by 8 bytes: [l_val, block[0..55]]
    new_block = bytearray(64)
    # Put l_val as first 8 bytes (big-endian)
    for j in range(8):
        new_block[j] = (l_val >> (8*(7-j))) & 0xFF
    # Copy old bytes 0..55 to positions 8..63
    for i in range(56):
        new_block[8+i] = block[i]
    return bytes(new_block)

def L_transform(block):
    """Linear transformation L = R^8."""
    result = block
    for _ in range(8):
        result = R(result)
    return result

def Vec_512(j):
    """Create Vec_512(j) - a 64-byte block representing integer j."""
    result = bytearray(64)
    j_bytes = to_bytes_be(j, 8)
    # Place j in the last 8 bytes (big-endian)
    for i in range(8):
        result[56+i] = j_bytes[i]
    return bytes(result)

# Generate C constants
def gen_C():
    C = []
    for i in range(12):
        vec = Vec_512(i+1)
        c = L_transform(vec)
        C.append(c)
    return C

# Verify C[0]
C = gen_C()
c0_hex = ''.join(f'{b:02x}' for b in C[0])
print(f"C[0] (computed): {c0_hex}")
print(f"C[0] (expected): 35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094")
print()

# Full Streebog-256 implementation
def streebog_g(h, m, n):
    """G(N, M, h) - core transformation."""
    # Key = h XOR N
    k = bytes(a ^ b for a, b in zip(h, n))
    # Data = h XOR M
    u = bytes(a ^ b for a, b in zip(h, m))

    for r in range(12):
        # Key = L(S(Key XOR C[r]))
        w = bytes(a ^ b for a, b in zip(k, C[r]))
        w = apply_S(w)
        w = L_transform(w)
        k = w

        # Data = L(S(Data XOR C[r]))
        v = bytes(a ^ b for a, b in zip(u, C[r]))
        v = apply_S(v)
        v = L_transform(v)
        u = v

        # Rotate key left by (12-r-1)*8 bits
        rot = (11 - r) * 8
        rot_bytes = rot // 8
        new_k = bytearray(64)
        for i in range(64):
            new_k[i] = k[(i + rot_bytes) % 64]
        k = bytes(new_k)

    # Result = h XOR Data XOR Key
    return bytes(a ^ b ^ c for a, b, c in zip(h, u, k))

def apply_S(block):
    """Apply S-box substitution (byte-wise)."""
    return bytes(SB[b] for b in block)

def add512(a, b):
    """512-bit big-endian addition (mod 2^512)."""
    result = bytearray(64)
    carry = 0
    for i in range(63, -1, -1):
        s = a[i] + b[i] + carry
        result[i] = s & 0xFF
        carry = s >> 8
    return bytes(result)

def streebog256(data):
    """Streebog-256 hash."""
    # IV for Streebog-256
    h = bytes([0x01] * 64)
    sigma = bytes(64)
    byte_count = 0

    # Process full 64-byte blocks
    offset = 0
    while offset + 64 <= len(data):
        m = data[offset:offset+64]
        byte_count += 64

        # Compute N = byte_count * 8 (128-bit LE in 64 bytes)
        bit_count = byte_count * 8
        n = bytearray(64)
        n_bytes = to_bytes_le(bit_count, 16)
        for i in range(16):
            n[i] = n_bytes[i]

        sigma = add512(sigma, m)
        h = streebog_g(h, m, bytes(n))
        offset += 64

    # Pad last block
    remaining = data[offset:]
    buf_len = len(remaining)
    byte_count += buf_len  # total bytes received

    padded = bytearray(64)
    padded[:buf_len] = remaining
    padded[buf_len] = 0x01  # padding marker

    # Compute final N
    bit_count = byte_count * 8
    n = bytearray(64)
    n_bytes = to_bytes_le(bit_count, 16)
    for i in range(16):
        n[i] = n_bytes[i]

    if buf_len < 48:
        # Length fits in the block
        padded[48:56] = n_bytes[0:8]
        padded[56:64] = n_bytes[8:16]
        sigma = add512(sigma, bytes(padded))
        h = streebog_g(h, bytes(padded), bytes(n))
    else:
        # Need two blocks
        sigma = add512(sigma, bytes(padded))
        h = streebog_g(h, bytes(padded), bytes(n))
        padded2 = bytearray(64)
        padded2[48:56] = n_bytes[0:8]
        padded2[56:64] = n_bytes[8:16]
        sigma = add512(sigma, bytes(padded2))
        h = streebog_g(h, bytes(padded2), bytes(n))

    # Final step 1: g(h, N, zeros, h)
    zeros = bytes(64)
    h = streebog_g(h, zeros, bytes(n))

    # Final step 2: g(h, zeros, sigma, h)
    n_zero = bytes(64)
    h = streebog_g(h, sigma, n_zero)

    return h[:32]

# Test empty hash
empty_hash = streebog256(b'')
print(f"Empty hash:  {''.join(f'{b:02x}' for b in empty_hash)}")
print(f"Expected:    3f5b11e2a8c30975dc351857a5f5593271c4d34499eaff0e8459894c5a896e47")
print()

# Test HLS9_C
data = bytes.fromhex(
    "00112233445566778899aabbccddeeff"
    "00112233445566778899aabbccddeeff"
    "ff00ee11dd22cc33"
    "bb44aa5599668877"
    "8899aabbccddeeff88889999aaaabbbb"
    "ccccddddeeeeffff89abcdeffedcba98"
    "00112233445566770000111122223333"
    "44445555666677770123456776543210"
)
h = streebog256(data)
print(f"HLS9_C:      {''.join(f'{b:02x}' for b in h)}")
print(f"Expected:    4c375b843898b6f0a0744051f74e42f2a944581d46c495e743e97abdcd9d7c58")
