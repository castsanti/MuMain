#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GameLogic::Items
{

constexpr int kItemSetRecordBytes = 4;
constexpr int kSeason6ItemCount = 8192;
constexpr int kSeason6ItemSetFileBytes = (kSeason6ItemCount * kItemSetRecordBytes) + 4;
constexpr int kSeason21ItemSetFileBytes = 112644;

struct ItemSetTypeRecord
{
    std::array<std::uint8_t, 2> option{};
    std::array<std::uint8_t, 2> mixLevel{};
};

struct ItemSetTypeDocument
{
    bool ok = false;
    int recordBytes = 0;
    std::vector<ItemSetTypeRecord> records;
    std::string error;
};

// Season 6 is 8192 records of 4 bytes plus a checksum. Season 21 is 112644 bytes.
// The checksum covers the whole payload. Record stride is chosen from layouts
// whose decrypted option byte is a real set id (0, 0xFF, or below 64).
ItemSetTypeDocument ParseItemSetType(const std::uint8_t* bytes, std::size_t size);

}
