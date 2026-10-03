#pragma once

#include <cstdint>

// Season 6 map and model cipher. The encrypted size equals the plain size.
namespace Render::Models
{

inline int EncryptMapFile(std::uint8_t* dst, const std::uint8_t* src, int size)
{
    if (!dst)
    {
        return size;
    }

    const std::uint8_t xorKey[16] = {0xD1, 0x73, 0x52, 0xF6, 0xD2, 0x9A, 0xCB, 0x27,
                                     0x3E, 0xAF, 0x59, 0x31, 0x37, 0xB3, 0xE7, 0xA2};

    std::uint16_t rollingKey = 0x5E;
    for (int i = 0; i < size; ++i)
    {
        dst[i] = static_cast<std::uint8_t>((src[i] + static_cast<std::uint8_t>(rollingKey)) ^ xorKey[i % 16]);
        rollingKey = static_cast<std::uint16_t>((dst[i] + 0x3D) & 0xFF);
    }
    return size;
}

inline int DecryptMapFile(std::uint8_t* dst, const std::uint8_t* src, int size)
{
    if (!dst)
    {
        return size;
    }

    const std::uint8_t xorKey[16] = {0xD1, 0x73, 0x52, 0xF6, 0xD2, 0x9A, 0xCB, 0x27,
                                     0x3E, 0xAF, 0x59, 0x31, 0x37, 0xB3, 0xE7, 0xA2};

    std::uint16_t rollingKey = 0x5E;
    for (int i = 0; i < size; ++i)
    {
        dst[i] = static_cast<std::uint8_t>((src[i] ^ xorKey[i % 16]) - static_cast<std::uint8_t>(rollingKey));
        rollingKey = static_cast<std::uint16_t>((src[i] + 0x3D) & 0xFF);
    }
    return size;
}

} // namespace Render::Models
