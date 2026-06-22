#!/usr/bin/env python3
"""Use the C constants from the code (user-verified) and test."""

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

# C constants from the code
C_hex = [
    "35bd7b9d66e3ee13ae96a01e4e7550028d364419ca1554f0484d602667a0185a0f5dc3f788bd2c1e833d51cfade8a2dc1e3041bef46ce34f9401c0c210852094",
    "bd7b9d66e3ee319f96a01e4e7550d172364419ca155419574d602667a01824d85dc3f788bd2c15513d51cfade8a23fc53041bef46ce359a201c0c21085206ebc",
    "7b9d66e3ee315207a01e4e7550d18b3a4419ca15541977da602667a01824a017c3f788bd2c15352751cfade8a23fe98741bef46ce3598650c0c21085206ebdc3",
    "9d66e3ee31525fcd1e4e7550d18be3e019ca15541977adcc2667a01824a05fa0f788bd2c1535b405cfade8a23fe9ea33bef46ce359862103c21085206ebd5240",
    "66e3ee31525f38674e7550d18be3f6a2ca15541977ad dfeb67a01824a05f9cea88bd2c1535b4b559ade8a23fe9eaa911f46ce3598621cc421085206ebd52d2c6",
    "e3ee31525f382b2a7550d18be3f6d85215541977addf77dea01824a05f9ca7e9bd2c1535b4b5b698e8a23fe9eaa9c0376ce3598621ccf01c85206ebd52d2da30",
    "ee31525f382b816550d18be3f6d8005f541977ad df77c4ff1824a05f9ca730e02c1535b4b5b65507a23fe9eaa9c09038e3598621ccf05db4206ebd52d2dad74f",
    "31525f382b81c6b2d18be3f6d80033f01977addf77c494fc24a05f9ca730f2281535b4b5b65536743fe9eaa9c090e266598621ccf05d1f656ebd52d2dad77760",
    "47035d39a126b76ed2ecb559cfa9204c0041e7caeae7e96b2e0729681f4c4aab7b3c5afe5ca258658d65595c8b8e6712277a551b6d5977e33209401c0c2108520",
    "035d39a126b71b89ecb559cfa920c2f641e7caeaeae7e96b200729681f4c4aad220c5afe5ca25869f955595c8b8e671abf2aa551b6d5977e62d99401c0c21085201860",
    "5d39a126b71b8b45b559cfa920c25634e7caeaeae7e96b271039681f4c4aad27716afe5ca25869f028f95c8b8e671ab903f51b6d5977e62062f01c0c2108520189abc",
    "39a126b71b8b212759cfa920c256f71fcaeaeae7e96b271af6981f4c4aad277ef3ae5ca25869f025db1c8b8e671ab90c47fb6d5977e62064233c0c2108520189abdc3",
]

C = [bytes.fromhex(h) for h in C_hex]

def to_bytes_le(n, length):
    return bytes([(n >> (8*i)) & 0xFF for i in range(length)])

def apply_S(block):
    return bytes(SB[b] for b in block)

def rotate_left(block, bits):
    rot = bits // 8
    return bytes(block[(i + rot) % 64] for i in range(64))

def streebog_g_LS(h, m, n):
    """L(S(x)) order: XOR C, then S, then L - but we don't have L, so just test XOR + S"""
    k = bytes(a ^ b for a, b in zip(h, n))
    u = bytes(a ^ b for a, b in zip(h, m))
    for r in range(12):
        k = bytes(a ^ b for a, b in zip(k, C[r]))
        k = apply_S(k)
        # No L - just test if the compression works without L
        u = bytes(a ^ b for a, b in zip(u, C[r]))
        u = apply_S(u)
        k = rotate_left(k, (11 - r) * 8)
    return bytes(a ^ b ^ c for a, b, c in zip(h, u, k))

# Actually, I need to figure out what L function the C constants correspond to.
# The C constants from the code are the ones used in the gostcrypto reference.
# The gostcrypto reference applies L BEFORE S (i.e., S(L(x))).
# But I don't have the correct L function.

# Wait - let me check if maybe the C constants already incorporate L,
# and the round function is just: x = S(x XOR C[i]) (no additional L)?

def streebog_g_noL(h, m, n):
    """No L in rounds - C already includes L."""
    k = bytes(a ^ b for a, b in zip(h, n))
    u = bytes(a ^ b for a, b in zip(h, m))
    for r in range(12):
        k = bytes(a ^ b for a, b in zip(k, C[r]))
        k = apply_S(k)
        u = bytes(a ^ b for a, b in zip(u, C[r]))
        u = apply_S(u)
        k = rotate_left(k, (11 - r) * 8)
    return bytes(a ^ b ^ c for a, b, c in zip(h, u, k))

def add512(a, b):
    result = bytearray(64)
    carry = 0
    for i in range(63, -1, -1):
        s = a[i] + b[i] + carry
        result[i] = s & 0xFF
        carry = s >> 8
    return bytes(result)

def streebog256(data, g_func):
    h = bytes([0x01] * 64)
    sigma = bytes(64)
    byte_count = 0
    offset = 0
    while offset + 64 <= len(data):
        m = data[offset:offset+64]
        byte_count += 64
        n = bytearray(64)
        n[:16] = to_bytes_le(byte_count * 8, 16)
        sigma = add512(sigma, m)
        h = g_func(h, m, bytes(n))
        offset += 64
    remaining = data[offset:]
    buf_len = len(remaining)
    byte_count += buf_len
    padded = bytearray(64)
    padded[:buf_len] = remaining
    padded[buf_len] = 0x01
    n_bytes = to_bytes_le(byte_count * 8, 16)
    n = bytearray(64)
    n[:16] = n_bytes
    if buf_len < 48:
        padded[48:56] = n_bytes[0:8]
        padded[56:64] = n_bytes[8:16]
        sigma = add512(sigma, bytes(padded))
        h = g_func(h, bytes(padded), bytes(n))
    else:
        sigma = add512(sigma, bytes(padded))
        h = g_func(h, bytes(padded), bytes(n))
        padded2 = bytearray(64)
        padded2[48:56] = n_bytes[0:8]
        padded2[56:64] = n_bytes[8:16]
        sigma = add512(sigma, bytes(padded2))
        h = g_func(h, bytes(padded2), bytes(n))
    h = g_func(h, bytes(64), bytes(n))
    h = g_func(h, sigma, bytes(64))
    return h[:32]

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

h = streebog256(b'', streebog_g_noL)
print(f"No-L Empty:  {''.join(f'{b:02x}' for b in h)}")
h = streebog256(data, streebog_g_noL)
print(f"No-L HLS9_C: {''.join(f'{b:02x}' for b in h)}")

print()
print("Expected Empty:  3f5b11e2a8c30975dc351857a5f5593271c4d34499eaff0e8459894c5a896e47")
print("Expected HLS9_C: 4c375b843898b6f0a0744051f74e42f2a944581d46c495e743e97abdcd9d7c58")
