#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Network/Server/ServerListScript.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace Network::ServerList;

namespace
{

struct PlainGroup
{
    std::uint16_t index = 0;
    std::string name;
    std::uint8_t position = 0;
    std::uint8_t sequence = 0;
    std::uint8_t marker = 0;
    std::array<std::uint8_t, kSeason6NonPvpCount> nonPvp{};
    std::vector<std::uint8_t> description;
    std::int16_t lengthField = 0;
    bool useLengthField = false;
};

void EncryptChunk(std::vector<std::uint8_t>& file, std::size_t begin, std::size_t end)
{
    if (end <= begin)
    {
        return;
    }
    BuxConvert(reinterpret_cast<BYTE*>(file.data() + begin), static_cast<int>(end - begin));
}

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void AppendName(std::vector<std::uint8_t>& out, const std::string& name)
{
    std::uint8_t field[kServerNameBytes] = {};
    const std::size_t count = name.size() < static_cast<std::size_t>(kServerNameBytes)
                                  ? name.size()
                                  : static_cast<std::size_t>(kServerNameBytes);
    std::memcpy(field, name.data(), count);
    out.insert(out.end(), field, field + kServerNameBytes);
}

void AppendRecord(std::vector<std::uint8_t>& file, const PlainGroup& group, bool withIndex, bool withNonPvp)
{
    const std::size_t headerStart = file.size();
    if (withIndex)
    {
        AppendU16(file, group.index);
    }
    AppendName(file, group.name);
    file.push_back(group.position);
    file.push_back(group.sequence);
    if (withNonPvp)
    {
        file.insert(file.end(), group.nonPvp.begin(), group.nonPvp.end());
    }
    else
    {
        file.push_back(group.marker);
    }

    const auto length = static_cast<std::uint16_t>(group.useLengthField
                                                        ? group.lengthField
                                                        : static_cast<std::int16_t>(group.description.size()));
    AppendU16(file, length);
    const std::size_t headerEnd = file.size();
    file.insert(file.end(), group.description.begin(), group.description.end());
    EncryptChunk(file, headerStart, headerEnd);
    EncryptChunk(file, headerEnd, file.size());
}

std::vector<std::uint8_t> Encode(const std::vector<PlainGroup>& groups, bool withIndex, bool withNonPvp)
{
    std::vector<std::uint8_t> file;
    for (const PlainGroup& group : groups)
    {
        AppendRecord(file, group, withIndex, withNonPvp);
    }
    return file;
}

PlainGroup Group(std::uint16_t index, const char* name, std::uint8_t position, std::uint8_t sequence,
                 std::vector<std::uint8_t> description)
{
    PlainGroup group;
    group.index = index;
    group.name = name;
    group.position = position;
    group.sequence = sequence;
    group.description = std::move(description);
    return group;
}

std::int16_t MisreadSeason6Length(const std::vector<std::uint8_t>& file)
{
    std::vector<std::uint8_t> header(file.begin(), file.begin() + static_cast<std::size_t>(kSeason6HeaderBytes));
    BuxConvert(reinterpret_cast<BYTE*>(header.data()), kSeason6HeaderBytes);
    const std::uint16_t raw = static_cast<std::uint16_t>(header[kSeason6HeaderBytes - 2] |
                                                         (header[kSeason6HeaderBytes - 1] << 8));
    return static_cast<std::int16_t>(raw);
}

} // namespace

TEST_CASE("Season 6 server list matches the 165-byte three-group file")
{
    std::vector<PlainGroup> groups = {
        Group(0, "Valhalla", 0, 1, {'V', 'a'}),
        Group(1, "Helheim", 1, 2, {'H', 'e'}),
        Group(2, "Midgard", 2, 3, {'M', 'i'}),
    };
    groups[1].nonPvp[0] = 1;
    groups[2].nonPvp[14] = 3;

    const std::vector<std::uint8_t> file = Encode(groups, true, true);
    REQUIRE(file.size() == 165);

    const ScriptDocument document = ParseScript(file.data(), file.size());
    REQUIRE(document.format == ScriptFormat::Season6);
    REQUIRE(document.records.size() == 3);
    CHECK(document.records[0].name == "Valhalla");
    CHECK(document.records[0].description == "Va");
    CHECK(document.records[0].position == 0);
    CHECK(document.records[0].sequence == 1);
    CHECK(document.records[1].name == "Helheim");
    CHECK(document.records[1].index == 1);
    CHECK(document.records[1].nonPvp[0] == 1);
    CHECK(document.records[1].description == "He");
    CHECK(document.records[2].name == "Midgard");
    CHECK(document.records[2].nonPvp[14] == 3);
    CHECK(document.records[2].description == "Mi");
}

TEST_CASE("Season 21 server list uses one marker byte and an unsigned length")
{
    std::vector<std::uint8_t> hostile(16, static_cast<std::uint8_t>('x'));
    hostile[12] = 0xF5;
    hostile[13] = 0xB8;

    std::vector<PlainGroup> groups = {
        Group(4, "Lorencia", 1, 2, hostile),
        Group(9, "Noria", 0, 3, {'N', 'o', 'r', 'i', 'a'}),
    };
    groups[0].marker = 0x5A;
    groups[1].marker = 0x11;

    const std::vector<std::uint8_t> file = Encode(groups, true, false);
    REQUIRE(MisreadSeason6Length(file) == -18187);

    const ScriptDocument document = ParseScript(file.data(), file.size());
    REQUIRE(document.format == ScriptFormat::Season21);
    REQUIRE(document.records.size() == 2);
    CHECK(document.records[0].index == 4);
    CHECK(document.records[0].name == "Lorencia");
    CHECK(document.records[0].position == 1);
    CHECK(document.records[0].sequence == 2);
    CHECK(document.records[0].season21Marker == 0x5A);
    CHECK(document.records[0].nonPvp == std::array<std::uint8_t, kSeason6NonPvpCount>{});
    CHECK(document.records[0].description.size() == hostile.size());
    CHECK(static_cast<std::uint8_t>(document.records[0].description[12]) == 0xF5);
    CHECK(document.records[1].index == 9);
    CHECK(document.records[1].name == "Noria");
    CHECK(document.records[1].description == "Noria");
    CHECK(document.records[1].season21Marker == 0x11);
}

TEST_CASE("Season 21 server list without a group index numbers groups in file order")
{
    std::vector<PlainGroup> groups = {
        Group(99, "Arena", 2, 4, {}),
        Group(99, "Market", 1, 5, {'o', 'k'}),
    };
    groups[0].marker = 7;
    groups[1].marker = 8;

    const std::vector<std::uint8_t> file = Encode(groups, false, false);
    const ScriptDocument document = ParseScript(file.data(), file.size());
    REQUIRE(document.format == ScriptFormat::Season21WithoutIndex);
    REQUIRE(document.records.size() == 2);
    CHECK(document.records[0].index == 0);
    CHECK(document.records[0].name == "Arena");
    CHECK(document.records[0].season21Marker == 7);
    CHECK(document.records[0].description.empty());
    CHECK(document.records[1].index == 1);
    CHECK(document.records[1].name == "Market");
    CHECK(document.records[1].description == "ok");
}

TEST_CASE("description length outside 0..1024 does not overrun and rejects the file")
{
    PlainGroup negative = Group(1, "Bad", 0, 0, {});
    negative.nonPvp.fill(0xFF);
    negative.sequence = 0xFF;
    negative.useLengthField = true;
    negative.lengthField = -18187;

    const std::vector<std::uint8_t> negativeFile = Encode({negative}, true, true);
    const ScriptDocument negativeDocument = ParseScript(negativeFile.data(), negativeFile.size());
    CHECK(negativeDocument.format == ScriptFormat::None);
    CHECK(negativeDocument.records.empty());
    CHECK(negativeDocument.error.find("-18187") != std::string::npos);

    PlainGroup oversized = Group(1, "Big", 0, 0, std::vector<std::uint8_t>(1025, 0xFF));
    oversized.nonPvp.fill(0xFF);
    oversized.sequence = 0xFF;
    const std::vector<std::uint8_t> oversizedFile = Encode({oversized}, true, true);
    const ScriptDocument oversizedDocument = ParseScript(oversizedFile.data(), oversizedFile.size());
    CHECK(oversizedDocument.format == ScriptFormat::None);
    CHECK(oversizedDocument.error.find("1025") != std::string::npos);

    PlainGroup exact = Group(3, "Full", 1, 2, std::vector<std::uint8_t>(kMaxDescriptionBytes, 'B'));
    exact.nonPvp.fill(0xFF);
    exact.sequence = 0xFF;
    const std::vector<std::uint8_t> exactFile = Encode({exact}, true, true);
    const ScriptDocument exactDocument = ParseScript(exactFile.data(), exactFile.size());
    REQUIRE(exactDocument.format == ScriptFormat::Season6);
    REQUIRE(exactDocument.records.size() == 1);
    CHECK(exactDocument.records[0].name == "Full");
    CHECK(exactDocument.records[0].description.size() == static_cast<std::size_t>(kMaxDescriptionBytes));
}

TEST_CASE("truncated, empty, and trailing server list bytes fail soft")
{
    const std::uint8_t truncated[] = {1, 2, 3, 4, 5};
    const ScriptDocument truncatedDocument = ParseScript(truncated, sizeof(truncated));
    CHECK(truncatedDocument.format == ScriptFormat::None);
    CHECK(truncatedDocument.error.find("truncated") != std::string::npos);

    const ScriptDocument emptyDocument = ParseScript(nullptr, 0);
    CHECK(emptyDocument.format == ScriptFormat::None);

    PlainGroup group = Group(0, "Valhalla", 0, 1, {});
    group.nonPvp.fill(0xFF);
    group.sequence = 0xFF;
    std::vector<std::uint8_t> trailing = Encode({group}, true, true);
    trailing.push_back(0);
    const ScriptDocument trailingDocument = ParseScript(trailing.data(), trailing.size());
    CHECK(trailingDocument.format == ScriptFormat::None);

    const ScriptDocument missing = ParseScript(nullptr, 4);
    CHECK(missing.format == ScriptFormat::None);
}

TEST_CASE("a 32-byte server name is not read past the field")
{
    PlainGroup group = Group(1, std::string(kServerNameBytes, 'A').c_str(), 0, 1, {'z'});
    const std::vector<std::uint8_t> file = Encode({group}, true, true);
    const ScriptDocument document = ParseScript(file.data(), file.size());
    REQUIRE(document.format == ScriptFormat::Season6);
    CHECK(document.records[0].name == std::string(kServerNameBytes, 'A'));
    CHECK(document.records[0].description == "z");
}

TEST_CASE("wide description copy stops at the UI buffer")
{
    wchar_t buffer[8];
    CopyBoundedWide(buffer, 8, L"0123456789");
    CHECK(std::wstring(buffer) == L"0123456");

    CopyBoundedWide(buffer, 8, L"Ab");
    CHECK(std::wstring(buffer) == L"Ab");

    CopyBoundedWide(nullptr, 8, L"Ab");
    CopyBoundedWide(buffer, 0, L"zzzzzzzz");
    CHECK(std::wstring(buffer) == L"Ab");
}
