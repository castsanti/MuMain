#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace UI::MasterSkill
{

constexpr int kMasterSkillRecordBytes = 24;
constexpr int kSeason6MasterSkillCount = 512;
constexpr int kSeason21MasterSkillCount = 2048;
constexpr int kSeason6MasterSkillFileBytes = (kSeason6MasterSkillCount * kMasterSkillRecordBytes) + 4;
constexpr int kSeason21MasterSkillFileBytes = (kSeason21MasterSkillCount * kMasterSkillRecordBytes) + 4;

struct MasterSkillRecord
{
    std::uint16_t index = 0;
    std::uint16_t classCode = 0;
    std::uint8_t group = 0;
    std::uint8_t requiredPoints = 0;
    std::uint8_t maxLevel = 0;
    std::uint8_t arrowDirection = 0;
    std::int32_t requireSkill[2] = {};
    std::int32_t skill = 0;
    float defValue = 0.f;
};

struct MasterSkillDocument
{
    bool ok = false;
    std::vector<MasterSkillRecord> records;
    std::string error;
};

// Season 6 is 512 packed 24-byte rows. Season 21 is 2048 rows of the same row
// plus the same trailing checksum. A row that runs past the buffer is rejected.
MasterSkillDocument ParseMasterSkillTree(const std::uint8_t* bytes, std::size_t size);

}
