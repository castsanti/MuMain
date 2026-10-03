#include "Render/Terrain/ModulusCryptor.h"

#include <cstring>

namespace Render::Terrain
{
namespace
{

constexpr int kCipherCount = 8;
constexpr int kChunkBytes = 1024;
constexpr int kMasterKeyBytes = 32;

constexpr std::uint8_t kMasterKey[kMasterKeyBytes] = {
    'w', 'e', 'b', 'z', 'e', 'n', '#', '@', '!', '0', '1', 'w', 'e', 'b', 'z', 'e',
    'n', '#', '@', '!', '0', '1', 'w', 'e', 'b', 'z', 'e', 'n', '#', '@', '!', '0'};

struct CipherSpec
{
    int blockBytes;
    int keyBytes;
};

constexpr CipherSpec kCiphers[kCipherCount] = {
    {8, 16}, {12, 12}, {8, 16}, {8, 16}, {16, 16}, {16, 16}, {8, 16}, {8, 32},
};

#include "ModulusSBoxes.inc"

constexpr std::uint8_t kGostS[128] = {
    4,  10, 9,  2,  13, 8,  0, 14, 6,  11, 1, 12, 7,  15, 5,  3,  14, 11, 4, 12, 6,  13,
    15, 10, 2,  3,  8,  1,  0, 7,  5,  9,  5,  8,  1,  13, 10, 3,  4,  2,  14, 15, 12, 7,
    6,  0,  9,  11, 7,  13, 10, 1,  0,  8,  9, 15, 14, 4,  6,  12, 11, 2,  5,  3,  6,  12,
    7,  1,  5,  15, 13, 8,  4,  10, 9,  14, 0, 3,  11, 2,  4,  11, 10, 0,  7,  2,  1,  13,
    3,  6,  8,  5,  9,  12, 15, 14, 13, 11, 4, 1,  3,  15, 5,  9,  0,  10, 14, 7,  6,  8,
    2,  12, 1,  15, 13, 0,  5,  7,  10, 4,  9, 2,  3,  14, 6,  11, 8,  12,
};

std::uint32_t Rotl(std::uint32_t value, unsigned shift)
{
    shift &= 31u;
    if (shift == 0)
    {
        return value;
    }
    return (value << shift) | (value >> (32u - shift));
}

std::uint32_t Rotr(std::uint32_t value, unsigned shift)
{
    shift &= 31u;
    if (shift == 0)
    {
        return value;
    }
    return (value >> shift) | (value << (32u - shift));
}

std::uint32_t LoadBe(const std::uint8_t* bytes)
{
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) | (std::uint32_t(bytes[2]) << 8) |
           bytes[3];
}

std::uint32_t LoadLe(const std::uint8_t* bytes)
{
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) |
           (std::uint32_t(bytes[3]) << 24);
}

void StoreBe(std::uint8_t* bytes, std::uint32_t value)
{
    bytes[0] = static_cast<std::uint8_t>(value >> 24);
    bytes[1] = static_cast<std::uint8_t>(value >> 16);
    bytes[2] = static_cast<std::uint8_t>(value >> 8);
    bytes[3] = static_cast<std::uint8_t>(value);
}

void StoreLe(std::uint8_t* bytes, std::uint32_t value)
{
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
    bytes[2] = static_cast<std::uint8_t>(value >> 16);
    bytes[3] = static_cast<std::uint8_t>(value >> 24);
}

void DecryptTea(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t v0 = LoadBe(block);
    std::uint32_t v1 = LoadBe(block + 4);
    const std::uint32_t k0 = LoadBe(key);
    const std::uint32_t k1 = LoadBe(key + 4);
    const std::uint32_t k2 = LoadBe(key + 8);
    const std::uint32_t k3 = LoadBe(key + 12);
    constexpr std::uint32_t kDelta = 0x9E3779B9u;
    std::uint32_t sum = 0xC6EF3720u;
    for (int round = 0; round < 32; ++round)
    {
        v1 -= ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
        v0 -= ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        sum -= kDelta;
    }
    StoreBe(block, v0);
    StoreBe(block + 4, v1);
}

std::uint32_t ReverseBits(std::uint32_t value)
{
    value = ((value & 0xAAAAAAAAu) >> 1) | ((value & 0x55555555u) << 1);
    value = ((value & 0xCCCCCCCCu) >> 2) | ((value & 0x33333333u) << 2);
    return ((value & 0xF0F0F0F0u) >> 4) | ((value & 0x0F0F0F0Fu) << 4);
}

std::uint32_t ReverseBytes(std::uint32_t value)
{
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) | ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

void ThreeWayMu(std::uint32_t& a0, std::uint32_t& a1, std::uint32_t& a2)
{
    a1 = ReverseBits(a1);
    const std::uint32_t swapped = ReverseBits(a0);
    a0 = ReverseBits(a2);
    a2 = swapped;
}

void ThreeWayTheta(std::uint32_t& a0, std::uint32_t& a1, std::uint32_t& a2)
{
    std::uint32_t mixed = a0 ^ a1 ^ a2;
    mixed = Rotl(mixed, 16) ^ Rotl(mixed, 8);
    const std::uint32_t b0 = (a0 << 24) ^ (a2 >> 8) ^ (a1 << 8) ^ (a0 >> 24);
    const std::uint32_t b1 = (a1 << 24) ^ (a0 >> 8) ^ (a2 << 8) ^ (a1 >> 24);
    a0 ^= mixed ^ b0;
    a1 ^= mixed ^ b1;
    a2 ^= mixed ^ (b0 >> 16) ^ (b1 << 16);
}

void ThreeWayPiGammaPi(std::uint32_t& a0, std::uint32_t& a1, std::uint32_t& a2)
{
    const std::uint32_t b2 = Rotl(a2, 1);
    const std::uint32_t b0 = Rotl(a0, 22);
    a0 = Rotl(b0 ^ (a1 | ~b2), 1);
    a2 = Rotl(b2 ^ (b0 | ~a1), 22);
    a1 ^= b2 | ~b0;
}

void DecryptThreeWay(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t a0 = LoadLe(block);
    std::uint32_t a1 = LoadLe(block + 4);
    std::uint32_t a2 = LoadLe(block + 8);
    std::uint32_t k0 = LoadBe(key);
    std::uint32_t k1 = LoadBe(key + 4);
    std::uint32_t k2 = LoadBe(key + 8);
    ThreeWayTheta(k0, k1, k2);
    ThreeWayMu(k0, k1, k2);
    k0 = ReverseBytes(k0);
    k1 = ReverseBytes(k1);
    k2 = ReverseBytes(k2);

    std::uint32_t roundConstant = 0xB1B1u;
    ThreeWayMu(a0, a1, a2);
    constexpr int kRounds = 11;
    for (int round = 0; round < kRounds; ++round)
    {
        a0 ^= k0 ^ (roundConstant << 16);
        a1 ^= k1;
        a2 ^= k2 ^ roundConstant;
        ThreeWayTheta(a0, a1, a2);
        ThreeWayPiGammaPi(a0, a1, a2);
        roundConstant <<= 1;
        if ((roundConstant & 0x10000u) != 0)
        {
            roundConstant ^= 0x11011u;
        }
    }
    a0 ^= k0 ^ (roundConstant << 16);
    a1 ^= k1;
    a2 ^= k2 ^ roundConstant;
    ThreeWayTheta(a0, a1, a2);
    ThreeWayMu(a0, a1, a2);
    StoreLe(block, a0);
    StoreLe(block + 4, a1);
    StoreLe(block + 8, a2);
}

std::uint32_t CastByte(const std::uint32_t* words, int byteIndex)
{
    const std::uint32_t word = words[byteIndex >> 2];
    const int shift = 24 - 8 * (byteIndex & 3);
    return (word >> shift) & 0xFFu;
}

std::uint32_t CastF1(std::uint32_t value, std::uint32_t mask, std::uint32_t rotation)
{
    const std::uint32_t mixed = Rotl(mask + value, rotation);
    return ((kCastS[0][mixed >> 24] ^ kCastS[1][(mixed >> 16) & 0xFF]) - kCastS[2][(mixed >> 8) & 0xFF]) +
           kCastS[3][mixed & 0xFF];
}

std::uint32_t CastF2(std::uint32_t value, std::uint32_t mask, std::uint32_t rotation)
{
    const std::uint32_t mixed = Rotl(mask ^ value, rotation);
    return ((kCastS[0][mixed >> 24] - kCastS[1][(mixed >> 16) & 0xFF]) + kCastS[2][(mixed >> 8) & 0xFF]) ^
           kCastS[3][mixed & 0xFF];
}

std::uint32_t CastF3(std::uint32_t value, std::uint32_t mask, std::uint32_t rotation)
{
    const std::uint32_t mixed = Rotl(mask - value, rotation);
    return ((kCastS[0][mixed >> 24] + kCastS[1][(mixed >> 16) & 0xFF]) ^ kCastS[2][(mixed >> 8) & 0xFF]) -
           kCastS[3][mixed & 0xFF];
}

void CastScheduleHalf(std::uint32_t* xWords, std::uint32_t* subkeys, int base)
{
    std::uint32_t zWords[4];
    zWords[0] = xWords[0] ^ kCastS[4][CastByte(xWords, 0xD)] ^ kCastS[5][CastByte(xWords, 0xF)] ^
                kCastS[6][CastByte(xWords, 0xC)] ^ kCastS[7][CastByte(xWords, 0xE)] ^ kCastS[6][CastByte(xWords, 0x8)];
    zWords[1] = xWords[2] ^ kCastS[4][CastByte(zWords, 0x0)] ^ kCastS[5][CastByte(zWords, 0x2)] ^
                kCastS[6][CastByte(zWords, 0x1)] ^ kCastS[7][CastByte(zWords, 0x3)] ^ kCastS[7][CastByte(xWords, 0xA)];
    zWords[2] = xWords[3] ^ kCastS[4][CastByte(zWords, 0x7)] ^ kCastS[5][CastByte(zWords, 0x6)] ^
                kCastS[6][CastByte(zWords, 0x5)] ^ kCastS[7][CastByte(zWords, 0x4)] ^ kCastS[4][CastByte(xWords, 0x9)];
    zWords[3] = xWords[1] ^ kCastS[4][CastByte(zWords, 0xA)] ^ kCastS[5][CastByte(zWords, 0x9)] ^
                kCastS[6][CastByte(zWords, 0xB)] ^ kCastS[7][CastByte(zWords, 0x8)] ^ kCastS[5][CastByte(xWords, 0xB)];
    subkeys[base + 0] = kCastS[4][CastByte(zWords, 0x8)] ^ kCastS[5][CastByte(zWords, 0x9)] ^
                        kCastS[6][CastByte(zWords, 0x7)] ^ kCastS[7][CastByte(zWords, 0x6)] ^
                        kCastS[4][CastByte(zWords, 0x2)];
    subkeys[base + 1] = kCastS[4][CastByte(zWords, 0xA)] ^ kCastS[5][CastByte(zWords, 0xB)] ^
                        kCastS[6][CastByte(zWords, 0x5)] ^ kCastS[7][CastByte(zWords, 0x4)] ^
                        kCastS[5][CastByte(zWords, 0x6)];
    subkeys[base + 2] = kCastS[4][CastByte(zWords, 0xC)] ^ kCastS[5][CastByte(zWords, 0xD)] ^
                        kCastS[6][CastByte(zWords, 0x3)] ^ kCastS[7][CastByte(zWords, 0x2)] ^
                        kCastS[6][CastByte(zWords, 0x9)];
    subkeys[base + 3] = kCastS[4][CastByte(zWords, 0xE)] ^ kCastS[5][CastByte(zWords, 0xF)] ^
                        kCastS[6][CastByte(zWords, 0x1)] ^ kCastS[7][CastByte(zWords, 0x0)] ^
                        kCastS[7][CastByte(zWords, 0xC)];
    xWords[0] = zWords[2] ^ kCastS[4][CastByte(zWords, 0x5)] ^ kCastS[5][CastByte(zWords, 0x7)] ^
                kCastS[6][CastByte(zWords, 0x4)] ^ kCastS[7][CastByte(zWords, 0x6)] ^ kCastS[6][CastByte(zWords, 0x0)];
    xWords[1] = zWords[0] ^ kCastS[4][CastByte(xWords, 0x0)] ^ kCastS[5][CastByte(xWords, 0x2)] ^
                kCastS[6][CastByte(xWords, 0x1)] ^ kCastS[7][CastByte(xWords, 0x3)] ^ kCastS[7][CastByte(zWords, 0x2)];
    xWords[2] = zWords[1] ^ kCastS[4][CastByte(xWords, 0x7)] ^ kCastS[5][CastByte(xWords, 0x6)] ^
                kCastS[6][CastByte(xWords, 0x5)] ^ kCastS[7][CastByte(xWords, 0x4)] ^ kCastS[4][CastByte(zWords, 0x1)];
    xWords[3] = zWords[3] ^ kCastS[4][CastByte(xWords, 0xA)] ^ kCastS[5][CastByte(xWords, 0x9)] ^
                kCastS[6][CastByte(xWords, 0xB)] ^ kCastS[7][CastByte(xWords, 0x8)] ^ kCastS[5][CastByte(zWords, 0x3)];
}

void CastScheduleKeys(std::uint32_t* subkeys, const std::uint8_t* key)
{
    std::uint32_t xWords[4];
    for (int i = 0; i < 4; ++i)
    {
        xWords[i] = LoadBe(key + (i * 4));
    }
    for (int base = 0; base <= 16; base += 16)
    {
        CastScheduleHalf(xWords, subkeys, base);
        subkeys[base + 4] = kCastS[4][CastByte(xWords, 0x3)] ^ kCastS[5][CastByte(xWords, 0x2)] ^
                            kCastS[6][CastByte(xWords, 0xC)] ^ kCastS[7][CastByte(xWords, 0xD)] ^
                            kCastS[4][CastByte(xWords, 0x8)];
        subkeys[base + 5] = kCastS[4][CastByte(xWords, 0x1)] ^ kCastS[5][CastByte(xWords, 0x0)] ^
                            kCastS[6][CastByte(xWords, 0xE)] ^ kCastS[7][CastByte(xWords, 0xF)] ^
                            kCastS[5][CastByte(xWords, 0xD)];
        subkeys[base + 6] = kCastS[4][CastByte(xWords, 0x7)] ^ kCastS[5][CastByte(xWords, 0x6)] ^
                            kCastS[6][CastByte(xWords, 0x8)] ^ kCastS[7][CastByte(xWords, 0x9)] ^
                            kCastS[6][CastByte(xWords, 0x3)];
        subkeys[base + 7] = kCastS[4][CastByte(xWords, 0x5)] ^ kCastS[5][CastByte(xWords, 0x4)] ^
                            kCastS[6][CastByte(xWords, 0xA)] ^ kCastS[7][CastByte(xWords, 0xB)] ^
                            kCastS[7][CastByte(xWords, 0x7)];
        std::uint32_t zWords[4];
        zWords[0] = xWords[0] ^ kCastS[4][CastByte(xWords, 0xD)] ^ kCastS[5][CastByte(xWords, 0xF)] ^
                    kCastS[6][CastByte(xWords, 0xC)] ^ kCastS[7][CastByte(xWords, 0xE)] ^
                    kCastS[6][CastByte(xWords, 0x8)];
        zWords[1] = xWords[2] ^ kCastS[4][CastByte(zWords, 0x0)] ^ kCastS[5][CastByte(zWords, 0x2)] ^
                    kCastS[6][CastByte(zWords, 0x1)] ^ kCastS[7][CastByte(zWords, 0x3)] ^
                    kCastS[7][CastByte(xWords, 0xA)];
        zWords[2] = xWords[3] ^ kCastS[4][CastByte(zWords, 0x7)] ^ kCastS[5][CastByte(zWords, 0x6)] ^
                    kCastS[6][CastByte(zWords, 0x5)] ^ kCastS[7][CastByte(zWords, 0x4)] ^
                    kCastS[4][CastByte(xWords, 0x9)];
        zWords[3] = xWords[1] ^ kCastS[4][CastByte(zWords, 0xA)] ^ kCastS[5][CastByte(zWords, 0x9)] ^
                    kCastS[6][CastByte(zWords, 0xB)] ^ kCastS[7][CastByte(zWords, 0x8)] ^
                    kCastS[5][CastByte(xWords, 0xB)];
        subkeys[base + 8] = kCastS[4][CastByte(zWords, 0x3)] ^ kCastS[5][CastByte(zWords, 0x2)] ^
                            kCastS[6][CastByte(zWords, 0xC)] ^ kCastS[7][CastByte(zWords, 0xD)] ^
                            kCastS[4][CastByte(zWords, 0x9)];
        subkeys[base + 9] = kCastS[4][CastByte(zWords, 0x1)] ^ kCastS[5][CastByte(zWords, 0x0)] ^
                            kCastS[6][CastByte(zWords, 0xE)] ^ kCastS[7][CastByte(zWords, 0xF)] ^
                            kCastS[5][CastByte(zWords, 0xC)];
        subkeys[base + 10] = kCastS[4][CastByte(zWords, 0x7)] ^ kCastS[5][CastByte(zWords, 0x6)] ^
                             kCastS[6][CastByte(zWords, 0x8)] ^ kCastS[7][CastByte(zWords, 0x9)] ^
                             kCastS[6][CastByte(zWords, 0x2)];
        subkeys[base + 11] = kCastS[4][CastByte(zWords, 0x5)] ^ kCastS[5][CastByte(zWords, 0x4)] ^
                             kCastS[6][CastByte(zWords, 0xA)] ^ kCastS[7][CastByte(zWords, 0xB)] ^
                             kCastS[7][CastByte(zWords, 0x6)];
        xWords[0] = zWords[2] ^ kCastS[4][CastByte(zWords, 0x5)] ^ kCastS[5][CastByte(zWords, 0x7)] ^
                    kCastS[6][CastByte(zWords, 0x4)] ^ kCastS[7][CastByte(zWords, 0x6)] ^
                    kCastS[6][CastByte(zWords, 0x0)];
        xWords[1] = zWords[0] ^ kCastS[4][CastByte(xWords, 0x0)] ^ kCastS[5][CastByte(xWords, 0x2)] ^
                    kCastS[6][CastByte(xWords, 0x1)] ^ kCastS[7][CastByte(xWords, 0x3)] ^
                    kCastS[7][CastByte(zWords, 0x2)];
        xWords[2] = zWords[1] ^ kCastS[4][CastByte(xWords, 0x7)] ^ kCastS[5][CastByte(xWords, 0x6)] ^
                    kCastS[6][CastByte(xWords, 0x5)] ^ kCastS[7][CastByte(xWords, 0x4)] ^
                    kCastS[4][CastByte(zWords, 0x1)];
        xWords[3] = zWords[3] ^ kCastS[4][CastByte(xWords, 0xA)] ^ kCastS[5][CastByte(xWords, 0x9)] ^
                    kCastS[6][CastByte(xWords, 0xB)] ^ kCastS[7][CastByte(xWords, 0x8)] ^
                    kCastS[5][CastByte(zWords, 0x3)];
        subkeys[base + 12] = kCastS[4][CastByte(xWords, 0x8)] ^ kCastS[5][CastByte(xWords, 0x9)] ^
                             kCastS[6][CastByte(xWords, 0x7)] ^ kCastS[7][CastByte(xWords, 0x6)] ^
                             kCastS[4][CastByte(xWords, 0x3)];
        subkeys[base + 13] = kCastS[4][CastByte(xWords, 0xA)] ^ kCastS[5][CastByte(xWords, 0xB)] ^
                             kCastS[6][CastByte(xWords, 0x5)] ^ kCastS[7][CastByte(xWords, 0x4)] ^
                             kCastS[5][CastByte(xWords, 0x7)];
        subkeys[base + 14] = kCastS[4][CastByte(xWords, 0xC)] ^ kCastS[5][CastByte(xWords, 0xD)] ^
                             kCastS[6][CastByte(xWords, 0x3)] ^ kCastS[7][CastByte(xWords, 0x2)] ^
                             kCastS[6][CastByte(xWords, 0x8)];
        subkeys[base + 15] = kCastS[4][CastByte(xWords, 0xE)] ^ kCastS[5][CastByte(xWords, 0xF)] ^
                             kCastS[6][CastByte(xWords, 0x1)] ^ kCastS[7][CastByte(xWords, 0x0)] ^
                             kCastS[7][CastByte(xWords, 0xD)];
    }
    for (int i = 16; i < 32; ++i)
    {
        subkeys[i] &= 0x1Fu;
    }
}

void DecryptCast(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t subkeys[32];
    CastScheduleKeys(subkeys, key);
    std::uint32_t left = LoadBe(block);
    std::uint32_t right = LoadBe(block + 4);
    for (int round = 15; round >= 0; --round)
    {
        std::uint32_t mixed = 0;
        const int kind = round % 3;
        if (kind == 0)
        {
            mixed = CastF1(right, subkeys[round], subkeys[round + 16]);
        }
        else if (kind == 1)
        {
            mixed = CastF2(right, subkeys[round], subkeys[round + 16]);
        }
        else
        {
            mixed = CastF3(right, subkeys[round], subkeys[round + 16]);
        }
        const std::uint32_t next = left ^ mixed;
        left = right;
        right = next;
    }
    StoreBe(block, right);
    StoreBe(block + 4, left);
}

void MixRcSubkeys(std::uint32_t* subkeys, int subkeyCount, std::uint32_t* words, int wordCount)
{
    constexpr std::uint32_t kP32 = 0xB7E15163u;
    constexpr std::uint32_t kQ32 = 0x9E3779B9u;
    subkeys[0] = kP32;
    for (int i = 1; i < subkeyCount; ++i)
    {
        subkeys[i] = subkeys[i - 1] + kQ32;
    }
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    int subkeyIndex = 0;
    int wordIndex = 0;
    const int passes = 3 * (subkeyCount > wordCount ? subkeyCount : wordCount);
    for (int pass = 0; pass < passes; ++pass)
    {
        subkeys[subkeyIndex] = Rotl(subkeys[subkeyIndex] + a + b, 3);
        a = subkeys[subkeyIndex];
        subkeyIndex = (subkeyIndex + 1) % subkeyCount;
        words[wordIndex] = Rotl(words[wordIndex] + a + b, (a + b) & 31u);
        b = words[wordIndex];
        wordIndex = (wordIndex + 1) % wordCount;
    }
}

void DecryptRc5(std::uint8_t* block, const std::uint8_t* key)
{
    constexpr int kRounds = 16;
    constexpr int kSubkeyCount = 2 * (kRounds + 1);
    std::uint32_t subkeys[kSubkeyCount];
    std::uint32_t words[4];
    for (int i = 0; i < 4; ++i)
    {
        words[i] = LoadLe(key + (i * 4));
    }
    MixRcSubkeys(subkeys, kSubkeyCount, words, 4);

    std::uint32_t a = LoadLe(block);
    std::uint32_t b = LoadLe(block + 4);
    for (int round = kRounds; round >= 1; --round)
    {
        b = Rotr(b - subkeys[(2 * round) + 1], a & 31u) ^ a;
        a = Rotr(a - subkeys[2 * round], b & 31u) ^ b;
    }
    b -= subkeys[1];
    a -= subkeys[0];
    StoreLe(block, a);
    StoreLe(block + 4, b);
}

void DecryptRc6(std::uint8_t* block, const std::uint8_t* key)
{
    constexpr int kRounds = 20;
    constexpr int kSubkeyCount = 2 * (kRounds + 2);
    std::uint32_t subkeys[kSubkeyCount];
    std::uint32_t words[4];
    for (int i = 0; i < 4; ++i)
    {
        words[i] = LoadLe(key + (i * 4));
    }
    MixRcSubkeys(subkeys, kSubkeyCount, words, 4);

    std::uint32_t a = LoadLe(block);
    std::uint32_t b = LoadLe(block + 4);
    std::uint32_t c = LoadLe(block + 8);
    std::uint32_t d = LoadLe(block + 12);
    c -= subkeys[(2 * kRounds) + 3];
    a -= subkeys[(2 * kRounds) + 2];
    for (int round = 0; round < kRounds; ++round)
    {
        const std::uint32_t rotated = d;
        d = c;
        c = b;
        b = a;
        a = rotated;
        const std::uint32_t u = Rotl(d * ((2u * d) + 1u), 5);
        const std::uint32_t t = Rotl(b * ((2u * b) + 1u), 5);
        c = Rotr(c - subkeys[(2 * kRounds) - (2 * round) + 1], t & 31u) ^ u;
        a = Rotr(a - subkeys[(2 * kRounds) - (2 * round)], u & 31u) ^ t;
    }
    d -= subkeys[1];
    b -= subkeys[0];
    StoreLe(block, a);
    StoreLe(block + 4, b);
    StoreLe(block + 8, c);
    StoreLe(block + 12, d);
}

void ScheduleMars(std::uint32_t* subkeys, const std::uint8_t* key)
{
    std::uint32_t words[15] = {};
    for (int i = 0; i < 4; ++i)
    {
        words[i] = LoadLe(key + (i * 4));
    }
    words[4] = 4;
    for (int group = 0; group < 4; ++group)
    {
        for (int i = 0; i < 15; ++i)
        {
            const std::uint32_t mixed = words[(i + 8) % 15] ^ words[(i + 13) % 15];
            words[i] ^= Rotl(mixed, 3) ^ static_cast<std::uint32_t>((4 * i) + group);
        }
        for (int pass = 0; pass < 4; ++pass)
        {
            for (int i = 0; i < 15; ++i)
            {
                words[i] = Rotl(words[i] + kMarsS[words[(i + 14) % 15] & 511u], 9);
            }
        }
        for (int i = 0; i < 10; ++i)
        {
            subkeys[(10 * group) + i] = words[(4 * i) % 15];
        }
    }
    for (int i = 5; i < 37; i += 2)
    {
        std::uint32_t word = subkeys[i] | 3u;
        std::uint32_t mask = (~word ^ (word << 1)) & (~word ^ (word >> 1)) & 0x7FFFFFFEu;
        mask &= mask >> 1;
        mask &= mask >> 2;
        mask &= mask >> 4;
        mask |= mask << 1;
        mask |= mask << 2;
        mask |= mask << 4;
        mask &= 0x7FFFFFFCu;
        word ^= Rotl(kMarsS[265 + (subkeys[i] & 3u)], subkeys[i - 1]) & mask;
        subkeys[i] = word;
    }
}

void MarsForwardMix(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d, const std::uint32_t* subkeys)
{
    d += subkeys[36];
    c += subkeys[37];
    b += subkeys[38];
    a += subkeys[39];
    for (int i = 0; i < 8; ++i)
    {
        b ^= kMarsS[a & 0xFFu];
        b += kMarsS[256 + ((a >> 8) & 0xFFu)];
        c += kMarsS[(a >> 16) & 0xFFu];
        a = Rotr(a, 24);
        d ^= kMarsS[256 + (a & 0xFFu)];
        if (i % 4 == 0)
        {
            a += d;
        }
        if (i % 4 == 1)
        {
            a += b;
        }
        const std::uint32_t rotated = a;
        a = b;
        b = c;
        c = d;
        d = rotated;
    }
}

void MarsKeyedRounds(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d, const std::uint32_t* subkeys)
{
    for (int i = 0; i < 16; ++i)
    {
        const std::uint32_t rotated = Rotr(a, 13);
        const std::uint32_t product = Rotl(a * subkeys[35 - (2 * i)], 10);
        const std::uint32_t mixed = rotated + subkeys[34 - (2 * i)];
        const std::uint32_t low = Rotl(kMarsS[mixed & 0x1FFu] ^ Rotr(product, 5) ^ product, product & 31u);
        c -= Rotl(mixed, Rotr(product, 5) & 31u);
        if (i < 8)
        {
            b -= low;
            d ^= product;
        }
        else
        {
            d -= low;
            b ^= product;
        }
        a = b;
        b = c;
        c = d;
        d = rotated;
    }
}

void MarsBackwardMix(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d)
{
    for (int i = 0; i < 8; ++i)
    {
        if (i % 4 == 2)
        {
            a -= d;
        }
        if (i % 4 == 3)
        {
            a -= b;
        }
        b ^= kMarsS[256 + (a & 0xFFu)];
        c -= kMarsS[(a >> 24) & 0xFFu];
        const std::uint32_t rotated = Rotl(a, 24);
        d = (d - kMarsS[256 + ((a >> 16) & 0xFFu)]) ^ kMarsS[rotated & 0xFFu];
        a = b;
        b = c;
        c = d;
        d = rotated;
    }
}

void DecryptMars(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t subkeys[40];
    ScheduleMars(subkeys, key);
    std::uint32_t d = LoadLe(block);
    std::uint32_t c = LoadLe(block + 4);
    std::uint32_t b = LoadLe(block + 8);
    std::uint32_t a = LoadLe(block + 12);
    MarsForwardMix(a, b, c, d, subkeys);
    MarsKeyedRounds(a, b, c, d, subkeys);
    MarsBackwardMix(a, b, c, d);
    d -= subkeys[0];
    c -= subkeys[1];
    b -= subkeys[2];
    a -= subkeys[3];
    StoreLe(block, d);
    StoreLe(block + 4, c);
    StoreLe(block + 8, b);
    StoreLe(block + 12, a);
}

std::uint16_t IdeaMul(std::uint16_t left, std::uint16_t right)
{
    const std::uint32_t product = std::uint32_t(left) * std::uint32_t(right);
    if (product == 0)
    {
        return static_cast<std::uint16_t>((1 - left - right) & 0xFFFF);
    }
    const std::uint32_t folded = (product & 0xFFFFu) - (product >> 16);
    return static_cast<std::uint16_t>(((folded & 0xFFFFu) - (folded >> 16)) & 0xFFFF);
}

std::uint16_t IdeaInverse(std::uint16_t value)
{
    if (value <= 1)
    {
        return value;
    }
    std::uint16_t inverse = value;
    for (int i = 0; i < 15; ++i)
    {
        inverse = IdeaMul(inverse, inverse);
        inverse = IdeaMul(inverse, value);
    }
    return inverse;
}

std::uint16_t LoadBe16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

void StoreBe16(std::uint8_t* bytes, std::uint16_t value)
{
    bytes[0] = static_cast<std::uint8_t>(value >> 8);
    bytes[1] = static_cast<std::uint8_t>(value);
}

void DecryptIdea(std::uint8_t* block, const std::uint8_t* key)
{
    constexpr int kRounds = 8;
    constexpr int kSubkeyCount = 52;
    std::uint16_t encryptKeys[kSubkeyCount];
    for (int i = 0; i < 8; ++i)
    {
        encryptKeys[i] = LoadBe16(key + (i * 2));
    }
    for (int i = 8; i < kSubkeyCount; ++i)
    {
        const int base = ((i / 8) * 8) - 8;
        encryptKeys[i] = static_cast<std::uint16_t>((encryptKeys[base + ((i + 1) % 8)] << 9) |
                                                    (encryptKeys[base + ((i + 2) % 8)] >> 7));
    }

    std::uint16_t decryptKeys[kSubkeyCount];
    for (int round = 0; round < kRounds; ++round)
    {
        const int source = (kRounds - round) * 6;
        decryptKeys[(round * 6) + 0] = IdeaInverse(encryptKeys[source + 0]);
        const int addendSwap = round > 0 ? 1 : 0;
        decryptKeys[(round * 6) + 1] =
            static_cast<std::uint16_t>((0x10000 - encryptKeys[source + 1 + addendSwap]) & 0xFFFF);
        decryptKeys[(round * 6) + 2] =
            static_cast<std::uint16_t>((0x10000 - encryptKeys[source + 2 - addendSwap]) & 0xFFFF);
        decryptKeys[(round * 6) + 3] = IdeaInverse(encryptKeys[source + 3]);
        decryptKeys[(round * 6) + 4] = encryptKeys[((kRounds - 1 - round) * 6) + 4];
        decryptKeys[(round * 6) + 5] = encryptKeys[((kRounds - 1 - round) * 6) + 5];
    }
    decryptKeys[(kRounds * 6) + 0] = IdeaInverse(encryptKeys[0]);
    decryptKeys[(kRounds * 6) + 1] = static_cast<std::uint16_t>((0x10000 - encryptKeys[1]) & 0xFFFF);
    decryptKeys[(kRounds * 6) + 2] = static_cast<std::uint16_t>((0x10000 - encryptKeys[2]) & 0xFFFF);
    decryptKeys[(kRounds * 6) + 3] = IdeaInverse(encryptKeys[3]);

    std::uint16_t x0 = LoadBe16(block);
    std::uint16_t x1 = LoadBe16(block + 2);
    std::uint16_t x2 = LoadBe16(block + 4);
    std::uint16_t x3 = LoadBe16(block + 6);
    for (int round = 0; round < kRounds; ++round)
    {
        const std::uint16_t* roundKey = decryptKeys + (round * 6);
        x0 = IdeaMul(x0, roundKey[0]);
        x1 = static_cast<std::uint16_t>(x1 + roundKey[1]);
        x2 = static_cast<std::uint16_t>(x2 + roundKey[2]);
        x3 = IdeaMul(x3, roundKey[3]);
        std::uint16_t t0 = static_cast<std::uint16_t>(x0 ^ x2);
        t0 = IdeaMul(t0, roundKey[4]);
        std::uint16_t t1 = static_cast<std::uint16_t>(t0 + static_cast<std::uint16_t>(x1 ^ x3));
        t1 = IdeaMul(t1, roundKey[5]);
        t0 = static_cast<std::uint16_t>(t0 + t1);
        x0 = static_cast<std::uint16_t>(x0 ^ t1);
        x3 = static_cast<std::uint16_t>(x3 ^ t0);
        t0 = static_cast<std::uint16_t>(t0 ^ x1);
        x1 = static_cast<std::uint16_t>(x2 ^ t1);
        x2 = t0;
    }
    x0 = IdeaMul(x0, decryptKeys[(kRounds * 6) + 0]);
    x2 = static_cast<std::uint16_t>(x2 + decryptKeys[(kRounds * 6) + 1]);
    x1 = static_cast<std::uint16_t>(x1 + decryptKeys[(kRounds * 6) + 2]);
    x3 = IdeaMul(x3, decryptKeys[(kRounds * 6) + 3]);
    StoreBe16(block, x0);
    StoreBe16(block + 2, x2);
    StoreBe16(block + 4, x1);
    StoreBe16(block + 6, x3);
}

std::uint32_t GostSubstitute(std::uint32_t value)
{
    std::uint32_t substituted = 0;
    for (int nibble = 0; nibble < 8; ++nibble)
    {
        const std::uint32_t index = (nibble * 16) + ((value >> (nibble * 4)) & 0xFu);
        substituted |= std::uint32_t(kGostS[index]) << (nibble * 4);
    }
    return substituted;
}

void GostRound(std::uint32_t& left, std::uint32_t& right, std::uint32_t key)
{
    right ^= Rotl(GostSubstitute(left + key), 11);
    const std::uint32_t swapped = left;
    left = right;
    right = swapped;
}

void DecryptGost(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t left = LoadLe(block);
    std::uint32_t right = LoadLe(block + 4);
    std::uint32_t words[8];
    for (int i = 0; i < 8; ++i)
    {
        words[i] = LoadLe(key + (i * 4));
    }
    for (int i = 0; i < 8; ++i)
    {
        GostRound(left, right, words[i]);
    }
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int i = 7; i >= 0; --i)
        {
            GostRound(left, right, words[i]);
        }
    }
    StoreLe(block, right);
    StoreLe(block + 4, left);
}

void DecryptBlock(std::uint8_t* block, int algorithm, const std::uint8_t* key)
{
    switch (algorithm & 7)
    {
    case 0:
        DecryptTea(block, key);
        break;
    case 1:
        DecryptThreeWay(block, key);
        break;
    case 2:
        DecryptCast(block, key);
        break;
    case 3:
        DecryptRc5(block, key);
        break;
    case 4:
        DecryptRc6(block, key);
        break;
    case 5:
        DecryptMars(block, key);
        break;
    case 6:
        DecryptIdea(block, key);
        break;
    default:
        DecryptGost(block, key);
        break;
    }
}

void DecryptSpan(std::uint8_t* data, std::size_t offset, std::size_t length, int algorithm, const std::uint8_t* key)
{
    const int blockBytes = kCiphers[algorithm & 7].blockBytes;
    for (std::size_t i = 0; i + static_cast<std::size_t>(blockBytes) <= length; i += static_cast<std::size_t>(blockBytes))
    {
        DecryptBlock(data + offset + i, algorithm, key);
    }
}

int ChunkBytes(int algorithm)
{
    const int blockBytes = kCiphers[algorithm & 7].blockBytes;
    return kChunkBytes - (kChunkBytes % blockBytes);
}

} // namespace

bool DecryptModulus(const std::uint8_t* source, std::size_t size, std::vector<std::uint8_t>& plain)
{
    plain.clear();
    if (source == nullptr || size < static_cast<std::size_t>(kModulusHeaderBytes))
    {
        return false;
    }

    std::vector<std::uint8_t> buffer(source, source + size);
    const int algorithm2 = buffer[0] & 7;
    const int algorithm1 = buffer[1] & 7;
    const std::size_t dataSize = size - static_cast<std::size_t>(kModulusHeaderBytes);
    const std::size_t headerChunk = static_cast<std::size_t>(ChunkBytes(algorithm1));

    if (dataSize > (4 * headerChunk))
    {
        DecryptSpan(buffer.data(), 2 + (dataSize >> 1), headerChunk, algorithm1, kMasterKey);
    }
    if (dataSize > headerChunk)
    {
        DecryptSpan(buffer.data(), size - headerChunk, headerChunk, algorithm1, kMasterKey);
        DecryptSpan(buffer.data(), 2, headerChunk, algorithm1, kMasterKey);
    }

    const std::size_t payloadChunk = dataSize - (dataSize % static_cast<std::size_t>(kCiphers[algorithm2].blockBytes));
    DecryptSpan(buffer.data(), static_cast<std::size_t>(kModulusHeaderBytes), payloadChunk, algorithm2, buffer.data() + 2);
    plain.assign(buffer.begin() + kModulusHeaderBytes, buffer.end());
    return true;
}

} // namespace Render::Terrain
