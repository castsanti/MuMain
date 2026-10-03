#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Network::ServerList
{

constexpr int kServerNameBytes = 32;
constexpr int kSeason6NonPvpCount = 15;
constexpr int kMaxDescriptionBytes = 1024;
constexpr int kIndexBytes = 2;
constexpr int kLengthBytes = 2;

// Season 6: index, name[32], position, sequence, nonPVP[15], signed length.
constexpr int kSeason6HeaderBytes =
    kIndexBytes + kServerNameBytes + 1 + 1 + kSeason6NonPvpCount + kLengthBytes;

// Season 21: index, name[32], position, sequence, one marker byte, unsigned length.
// The marker is not a non-PVP flag. There is no 15-byte non-PVP array.
constexpr int kSeason21HeaderBytes = kIndexBytes + kServerNameBytes + 1 + 1 + 1 + kLengthBytes;

// Same Season 21 record without the leading group index.
constexpr int kSeason21NameFirstHeaderBytes = kServerNameBytes + 1 + 1 + 1 + kLengthBytes;

static_assert(kSeason6HeaderBytes == 53);
static_assert(kSeason21HeaderBytes == 39);
static_assert(kSeason21NameFirstHeaderBytes == 37);

enum class ScriptFormat
{
    None,
    Season6,
    Season21,
    Season21WithoutIndex,
};

struct ScriptRecord
{
    std::uint16_t index = 0;
    std::string name;
    std::uint8_t position = 0;
    std::uint8_t sequence = 0;
    std::uint8_t season21Marker = 0;
    std::array<std::uint8_t, kSeason6NonPvpCount> nonPvp{};
    std::string description;
};

struct ScriptDocument
{
    ScriptFormat format = ScriptFormat::None;
    std::vector<ScriptRecord> records;
    std::string error;
};

// Reads a whole ServerList.bmd. Season 6 is tried first, then Season 21 with a
// group index, then Season 21 without one. A description length outside 0..1024
// rejects that layout. The walk never copies more description bytes than remain
// in the buffer.
ScriptDocument ParseScript(const std::uint8_t* bytes, std::size_t size);

const char* ScriptFormatName(ScriptFormat format);

// Writes at most capacity-1 characters and a terminating NUL.
void CopyBoundedWide(wchar_t* destination, std::size_t capacity, std::wstring_view text);

}
