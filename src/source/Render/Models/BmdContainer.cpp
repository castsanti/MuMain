#include "Render/Models/BmdContainer.h"

#include "Render/Models/MapFileCrypt.h"

#include <array>
#include <cstring>
#include <limits>
#include <new>

namespace Render::Models
{
namespace
{

constexpr std::size_t kPlainHeaderBytes = 4;
constexpr std::size_t kEncryptedHeaderBytes = 8;
constexpr int kLea256Rounds = 32;
constexpr int kLea256RoundKeyWords = 6;

constexpr std::uint32_t kLeaDelta[8] = {0xc3efe9dbu, 0x44626b02u, 0x79e27c8au, 0x78df30ecu,
                                        0x715ea49eu, 0xc785da0au, 0xe04ef22au, 0xe5c40957u};

// Season 21 BMD key, little-endian key words. Public model viewers use this key.
constexpr std::uint8_t kSeason21LeaKey[kLea256KeyBytes] = {
    0xCC, 0x50, 0x45, 0x13, 0xC2, 0xA6, 0x57, 0x4E, 0xD6, 0x9A, 0x45, 0x89, 0xBF, 0x2F, 0xBC, 0xD9,
    0x39, 0xB3, 0xB3, 0xBD, 0x50, 0xBD, 0xCC, 0xB6, 0x85, 0x46, 0xD1, 0xD6, 0x16, 0x54, 0xE0, 0x87};

constexpr std::uint32_t Rotl(std::uint32_t value, unsigned shift)
{
    shift &= 31u;
    if (shift == 0)
    {
        return value;
    }
    return (value << shift) | (value >> (32u - shift));
}

constexpr std::uint32_t Rotr(std::uint32_t value, unsigned shift)
{
    shift &= 31u;
    if (shift == 0)
    {
        return value;
    }
    return (value >> shift) | (value << (32u - shift));
}

using RoundKeys = std::array<std::uint32_t, kLea256Rounds * kLea256RoundKeyWords>;

RoundKeys ScheduleLea256(const std::uint8_t* key)
{
    std::uint32_t words[8] = {};
    for (int word = 0; word < 8; ++word)
    {
        const std::size_t offset = static_cast<std::size_t>(word) * 4;
        words[word] = static_cast<std::uint32_t>(key[offset]) |
                      (static_cast<std::uint32_t>(key[offset + 1]) << 8) |
                      (static_cast<std::uint32_t>(key[offset + 2]) << 16) |
                      (static_cast<std::uint32_t>(key[offset + 3]) << 24);
    }

    RoundKeys roundKeys = {};
    for (int round = 0; round < kLea256Rounds; ++round)
    {
        const std::uint32_t delta = kLeaDelta[round & 7];
        const int slot = (round * kLea256RoundKeyWords) & 7;
        words[(slot + 0) & 7] = Rotl(words[(slot + 0) & 7] + Rotl(delta, static_cast<unsigned>(round)), 1);
        words[(slot + 1) & 7] = Rotl(words[(slot + 1) & 7] + Rotl(delta, static_cast<unsigned>(round + 1)), 3);
        words[(slot + 2) & 7] = Rotl(words[(slot + 2) & 7] + Rotl(delta, static_cast<unsigned>(round + 2)), 6);
        words[(slot + 3) & 7] = Rotl(words[(slot + 3) & 7] + Rotl(delta, static_cast<unsigned>(round + 3)), 11);
        words[(slot + 4) & 7] = Rotl(words[(slot + 4) & 7] + Rotl(delta, static_cast<unsigned>(round + 4)), 13);
        words[(slot + 5) & 7] = Rotl(words[(slot + 5) & 7] + Rotl(delta, static_cast<unsigned>(round + 5)), 17);

        const std::size_t base = static_cast<std::size_t>(round) * kLea256RoundKeyWords;
        for (int piece = 0; piece < kLea256RoundKeyWords; ++piece)
        {
            roundKeys[base + static_cast<std::size_t>(piece)] = words[(slot + piece) & 7];
        }
    }
    return roundKeys;
}

void DecryptBlock(const RoundKeys& roundKeys, const std::uint8_t* src, std::uint8_t* dst)
{
    std::uint32_t state[4] = {};
    std::memcpy(state, src, kLeaBlockBytes);

    for (int round = 0; round < kLea256Rounds; ++round)
    {
        const std::uint32_t* roundKey =
            roundKeys.data() + static_cast<std::size_t>(kLea256Rounds - 1 - round) * kLea256RoundKeyWords;
        std::uint32_t next[4] = {};
        next[0] = state[3];
        next[1] = (Rotr(state[0], 9) - (next[0] ^ roundKey[0])) ^ roundKey[1];
        next[2] = (Rotl(state[1], 5) - (next[1] ^ roundKey[2])) ^ roundKey[3];
        next[3] = (Rotl(state[2], 3) - (next[2] ^ roundKey[4])) ^ roundKey[5];
        std::memcpy(state, next, sizeof(state));
    }

    std::memcpy(dst, state, kLeaBlockBytes);
}

void EncryptBlock(const RoundKeys& roundKeys, const std::uint8_t* src, std::uint8_t* dst)
{
    std::uint32_t state[4] = {};
    std::memcpy(state, src, kLeaBlockBytes);

    for (int round = 0; round < kLea256Rounds; ++round)
    {
        const std::uint32_t* roundKey = roundKeys.data() + static_cast<std::size_t>(round) * kLea256RoundKeyWords;
        std::uint32_t next[4] = {};
        next[3] = state[0];
        next[0] = Rotl((state[1] ^ roundKey[1]) + (next[3] ^ roundKey[0]), 9);
        next[1] = Rotr((state[2] ^ roundKey[3]) + (state[1] ^ roundKey[2]), 5);
        next[2] = Rotr((state[3] ^ roundKey[5]) + (state[2] ^ roundKey[4]), 3);
        std::memcpy(state, next, sizeof(state));
    }

    std::memcpy(dst, state, kLeaBlockBytes);
}

bool RunLea256(const std::uint8_t* key, std::uint8_t* dst, const std::uint8_t* src, std::size_t size, bool encrypt)
{
    if (key == nullptr || dst == nullptr || src == nullptr)
    {
        return false;
    }
    if (size % kLeaBlockBytes != 0)
    {
        return false;
    }

    const RoundKeys roundKeys = ScheduleLea256(key);
    for (std::size_t offset = 0; offset < size; offset += kLeaBlockBytes)
    {
        if (encrypt)
        {
            EncryptBlock(roundKeys, src + offset, dst + offset);
        }
        else
        {
            DecryptBlock(roundKeys, src + offset, dst + offset);
        }
    }
    return true;
}

bool ReadSize(const std::uint8_t* file, std::size_t fileSize, std::int32_t& size)
{
    if (fileSize < kEncryptedHeaderBytes)
    {
        return false;
    }
    std::memcpy(&size, file + kPlainHeaderBytes, sizeof(size));
    return true;
}

BmdContainerStatus ReadEncryptedPayload(unsigned char version, const std::uint8_t* file, std::size_t fileSize,
                                        std::vector<std::uint8_t>& plain)
{
    std::int32_t encryptedSize = 0;
    if (!ReadSize(file, fileSize, encryptedSize) || encryptedSize <= 0)
    {
        return BmdContainerStatus::BadPayloadSize;
    }
    if (static_cast<std::size_t>(encryptedSize) > fileSize - kEncryptedHeaderBytes)
    {
        return BmdContainerStatus::BadPayloadSize;
    }
    if (version == kBmdVersionSeason21 && static_cast<std::size_t>(encryptedSize) % kLeaBlockBytes != 0)
    {
        return BmdContainerStatus::BadPayloadSize;
    }

    try
    {
        plain.resize(static_cast<std::size_t>(encryptedSize));
    }
    catch (const std::bad_alloc&)
    {
        return BmdContainerStatus::OutOfMemory;
    }

    const std::uint8_t* encrypted = file + kEncryptedHeaderBytes;
    if (version == kBmdVersionSeason6)
    {
        DecryptMapFile(plain.data(), encrypted, encryptedSize);
        return BmdContainerStatus::Ok;
    }

    if (!DecryptLea256Ecb(kSeason21LeaKey, plain.data(), encrypted, plain.size()))
    {
        plain.clear();
        return BmdContainerStatus::BadPayloadSize;
    }
    return BmdContainerStatus::Ok;
}

std::size_t PaddedPlainSize(unsigned char version, std::size_t plainSize)
{
    if (version != kBmdVersionSeason21)
    {
        return plainSize;
    }
    const std::size_t remainder = plainSize % kLeaBlockBytes;
    if (remainder == 0)
    {
        return plainSize;
    }
    return plainSize + (kLeaBlockBytes - remainder);
}

} // namespace

bool DecryptLea256Ecb(const std::uint8_t* key, std::uint8_t* dst, const std::uint8_t* src, std::size_t size)
{
    return RunLea256(key, dst, src, size, false);
}

bool EncryptLea256Ecb(const std::uint8_t* key, std::uint8_t* dst, const std::uint8_t* src, std::size_t size)
{
    return RunLea256(key, dst, src, size, true);
}

BmdContainerStatus ReadBmdPlainPayload(const std::uint8_t* file, std::size_t fileSize, BmdPlainPayload& out)
{
    out = {};
    if (file == nullptr || fileSize < kPlainHeaderBytes)
    {
        return BmdContainerStatus::BadHeader;
    }
    if (file[0] != 'B' || file[1] != 'M' || file[2] != 'D')
    {
        return BmdContainerStatus::BadHeader;
    }

    out.version = file[3];
    if (out.version == kBmdVersionSeason8)
    {
        return BmdContainerStatus::Unsupported;
    }
    if (out.version == kBmdVersionPlain)
    {
        try
        {
            out.bytes.assign(file + kPlainHeaderBytes, file + fileSize);
        }
        catch (const std::bad_alloc&)
        {
            return BmdContainerStatus::OutOfMemory;
        }
        return BmdContainerStatus::Ok;
    }
    if (out.version != kBmdVersionSeason6 && out.version != kBmdVersionSeason21)
    {
        return BmdContainerStatus::UnknownVersion;
    }
    return ReadEncryptedPayload(out.version, file, fileSize, out.bytes);
}

bool WriteBmdContainer(unsigned char version, const std::uint8_t* plain, std::size_t plainSize,
                       std::vector<std::uint8_t>& file)
{
    if (plain == nullptr || plainSize == 0 ||
        plainSize > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    {
        return false;
    }

    if (version == kBmdVersionPlain)
    {
        file.resize(kPlainHeaderBytes + plainSize);
        file[0] = 'B';
        file[1] = 'M';
        file[2] = 'D';
        file[3] = version;
        std::memcpy(file.data() + kPlainHeaderBytes, plain, plainSize);
        return true;
    }
    if (version != kBmdVersionSeason6 && version != kBmdVersionSeason21)
    {
        return false;
    }

    const std::size_t storedSize = PaddedPlainSize(version, plainSize);
    std::vector<std::uint8_t> stored(storedSize, 0);
    std::memcpy(stored.data(), plain, plainSize);

    file.resize(kEncryptedHeaderBytes + storedSize);
    file[0] = 'B';
    file[1] = 'M';
    file[2] = 'D';
    file[3] = version;
    const auto sizeField = static_cast<std::int32_t>(storedSize);
    std::memcpy(file.data() + kPlainHeaderBytes, &sizeField, sizeof(sizeField));

    std::uint8_t* encrypted = file.data() + kEncryptedHeaderBytes;
    if (version == kBmdVersionSeason6)
    {
        EncryptMapFile(encrypted, stored.data(), static_cast<int>(storedSize));
        return true;
    }
    return EncryptLea256Ecb(kSeason21LeaKey, encrypted, stored.data(), storedSize);
}

} // namespace Render::Models
