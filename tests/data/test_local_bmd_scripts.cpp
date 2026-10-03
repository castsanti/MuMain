#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Core/Utilities/Checksum.h"
#include "GameLogic/Items/ItemSetTypeScript.h"
#include "UI/NewUI/HUD/MasterSkillTreeScript.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{

void AppendChecksum(std::vector<std::uint8_t>& file, WORD key)
{
    const DWORD checksum =
        GenerateCheckSum2(reinterpret_cast<const BYTE*>(file.data()), static_cast<DWORD>(file.size()), key);
    file.push_back(static_cast<std::uint8_t>(checksum & 0xFF));
    file.push_back(static_cast<std::uint8_t>((checksum >> 8) & 0xFF));
    file.push_back(static_cast<std::uint8_t>((checksum >> 16) & 0xFF));
    file.push_back(static_cast<std::uint8_t>((checksum >> 24) & 0xFF));
}

std::vector<std::uint8_t> ItemSetFile(int itemCount, int stride, std::uint8_t option)
{
    std::vector<std::uint8_t> file(static_cast<std::size_t>(itemCount * stride), 0x11);
    for (int i = 0; i < itemCount; ++i)
    {
        std::vector<BYTE> record(static_cast<std::size_t>(stride), 0x11);
        record[0] = option;
        record[1] = 0xFF;
        record[2] = 3;
        record[3] = 0;
        BuxConvert(record.data(), stride);
        std::memcpy(file.data() + static_cast<std::size_t>(i * stride), record.data(), static_cast<std::size_t>(stride));
    }
    AppendChecksum(file, 0xE5F1);
    return file;
}

std::vector<std::uint8_t> MasterSkillFile(int count, std::uint16_t index, float value)
{
    std::vector<std::uint8_t> file(static_cast<std::size_t>(count * UI::MasterSkill::kMasterSkillRecordBytes), 0);
    for (int i = 0; i < count; ++i)
    {
        BYTE row[UI::MasterSkill::kMasterSkillRecordBytes] = {};
        const std::uint16_t skillIndex = index == 0 ? 0 : static_cast<std::uint16_t>(index + i);
        row[0] = static_cast<BYTE>(skillIndex & 0xFF);
        row[1] = static_cast<BYTE>((skillIndex >> 8) & 0xFF);
        row[2] = 2;
        row[4] = 1;
        row[6] = 20;
        std::memcpy(row + 20, &value, sizeof(value));
        BuxConvert(row, UI::MasterSkill::kMasterSkillRecordBytes);
        std::memcpy(file.data() + static_cast<std::size_t>(i * UI::MasterSkill::kMasterSkillRecordBytes), row,
                    sizeof(row));
    }
    AppendChecksum(file, 0x2BC1);
    return file;
}

} // namespace

TEST_CASE("Season 6 ItemSetType is 8192 four-byte rows")
{
    const std::vector<std::uint8_t> file = ItemSetFile(GameLogic::Items::kSeason6ItemCount, 4, 7);
    REQUIRE(file.size() == 32772);

    const GameLogic::Items::ItemSetTypeDocument document = GameLogic::Items::ParseItemSetType(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.recordBytes == 4);
    REQUIRE(document.records.size() == 8192);
    CHECK(document.records[0].option[0] == 7);
    CHECK(document.records[0].option[1] == 0xFF);
    CHECK(document.records[0].mixLevel[0] == 3);
}

TEST_CASE("Season 21 ItemSetType keeps the four-byte row and reads every section")
{
    const std::vector<std::uint8_t> file = ItemSetFile(28160, 4, 4);
    REQUIRE(file.size() == 112644);

    const GameLogic::Items::ItemSetTypeDocument document = GameLogic::Items::ParseItemSetType(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.recordBytes == 4);
    CHECK(document.records.size() == 28160);
    CHECK(document.records.front().option[0] == 4);
    CHECK(document.records.back().mixLevel[0] == 3);
}

TEST_CASE("a longer ItemSetType row is not split into four-byte records")
{
    const std::vector<std::uint8_t> file = ItemSetFile(11264, 10, 9);
    REQUIRE(file.size() == 112644);

    const GameLogic::Items::ItemSetTypeDocument document = GameLogic::Items::ParseItemSetType(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.recordBytes == 10);
    CHECK(document.records.size() == 11264);
    CHECK(document.records[10].option[0] == 9);
}

TEST_CASE("an unknown ItemSetType size fails without reading past the buffer")
{
    const std::uint8_t bytes[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const GameLogic::Items::ItemSetTypeDocument document = GameLogic::Items::ParseItemSetType(bytes, sizeof(bytes));
    CHECK_FALSE(document.ok);
    CHECK(document.records.empty());
}

TEST_CASE("Season 6 and Season 21 master skill trees share the 24-byte row")
{
    const std::vector<std::uint8_t> season6 = MasterSkillFile(512, 1, 1.5f);
    REQUIRE(season6.size() == 12292);
    const UI::MasterSkill::MasterSkillDocument season6Document =
        UI::MasterSkill::ParseMasterSkillTree(season6.data(), season6.size());
    REQUIRE(season6Document.ok);
    CHECK(season6Document.records.size() == 512);
    CHECK(season6Document.records[0].index == 1);
    CHECK(season6Document.records[0].classCode == 2);
    CHECK(season6Document.records[0].group == 1);
    CHECK(season6Document.records[0].maxLevel == 20);
    CHECK(season6Document.records[0].defValue == doctest::Approx(1.5f));
    CHECK(season6Document.records[1].index == 2);

    const std::vector<std::uint8_t> season21 = MasterSkillFile(2048, 1, 2.0f);
    REQUIRE(season21.size() == 49156);
    const UI::MasterSkill::MasterSkillDocument season21Document =
        UI::MasterSkill::ParseMasterSkillTree(season21.data(), season21.size());
    REQUIRE(season21Document.ok);
    CHECK(season21Document.records.size() == 2048);
    CHECK(season21Document.records.back().index == 2048);
    CHECK(season21Document.records.back().defValue == doctest::Approx(2.0f));
}

TEST_CASE("a truncated master skill file is rejected")
{
    std::vector<std::uint8_t> file = MasterSkillFile(512, 1, 1.f);
    file.resize(100);
    const UI::MasterSkill::MasterSkillDocument document = UI::MasterSkill::ParseMasterSkillTree(file.data(), file.size());
    CHECK_FALSE(document.ok);
    CHECK(document.records.empty());
}
