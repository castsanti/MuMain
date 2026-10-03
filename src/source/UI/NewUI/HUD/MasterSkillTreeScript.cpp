#include "UI/NewUI/HUD/MasterSkillTreeScript.h"

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Core/Utilities/Checksum.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace UI::MasterSkill
{
namespace
{

constexpr WORD kChecksumKey = 0x2BC1;

static_assert(kSeason6MasterSkillFileBytes == 12292);
static_assert(kSeason21MasterSkillFileBytes == 49156);
constexpr std::uint16_t kClassMask = 0x7F;
constexpr int kMaxGroup = 3;
constexpr int kMaxLevel = 30;

std::uint16_t ReadU16(const BYTE* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::int32_t ReadI32(const BYTE* bytes)
{
    std::uint32_t value = bytes[0];
    value |= static_cast<std::uint32_t>(bytes[1]) << 8;
    value |= static_cast<std::uint32_t>(bytes[2]) << 16;
    value |= static_cast<std::uint32_t>(bytes[3]) << 24;
    return static_cast<std::int32_t>(value);
}

bool Plausible(const BYTE* record)
{
    const std::uint16_t classCode = ReadU16(record + 2);
    const std::uint8_t group = record[4];
    const std::uint8_t maxLevel = record[6];
    return (classCode & ~kClassMask) == 0 && group < kMaxGroup && maxLevel <= kMaxLevel;
}

MasterSkillRecord ReadRecord(const BYTE* record)
{
    MasterSkillRecord skill;
    skill.index = ReadU16(record);
    skill.classCode = ReadU16(record + 2);
    skill.group = record[4];
    skill.requiredPoints = record[5];
    skill.maxLevel = record[6];
    skill.arrowDirection = record[7];
    skill.requireSkill[0] = ReadI32(record + 8);
    skill.requireSkill[1] = ReadI32(record + 12);
    skill.skill = ReadI32(record + 16);
    std::memcpy(&skill.defValue, record + 20, sizeof(skill.defValue));
    return skill;
}

} // namespace

MasterSkillDocument ParseMasterSkillTree(const std::uint8_t* bytes, std::size_t size)
{
    MasterSkillDocument document;
    if (bytes == nullptr || size < sizeof(DWORD) || (size - sizeof(DWORD)) % kMasterSkillRecordBytes != 0)
    {
        document.error = "MasterSkillTreeData.bmd is not a whole number of skill rows";
        return document;
    }

    const int payloadBytes = static_cast<int>(size - sizeof(DWORD));
    const int count = payloadBytes / kMasterSkillRecordBytes;
    const bool knownCount = count == kSeason6MasterSkillCount || count == kSeason21MasterSkillCount;
    DWORD storedChecksum = 0;
    storedChecksum |= bytes[size - 4];
    storedChecksum |= static_cast<DWORD>(bytes[size - 3]) << 8;
    storedChecksum |= static_cast<DWORD>(bytes[size - 2]) << 16;
    storedChecksum |= static_cast<DWORD>(bytes[size - 1]) << 24;
    const bool checksumMatches =
        GenerateCheckSum2(reinterpret_cast<const BYTE*>(bytes), static_cast<DWORD>(payloadBytes), kChecksumKey) ==
        storedChecksum;
    if (!knownCount && !checksumMatches)
    {
        document.error = "MasterSkillTreeData.bmd was not recognized";
        return document;
    }

    int plausible = 0;
    std::vector<BYTE> row(static_cast<std::size_t>(kMasterSkillRecordBytes));
    document.records.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const std::uint8_t* source = bytes + static_cast<std::size_t>(i * kMasterSkillRecordBytes);
        std::copy(source, source + kMasterSkillRecordBytes, row.begin());
        BuxConvert(row.data(), kMasterSkillRecordBytes);
        if (Plausible(row.data()))
        {
            ++plausible;
        }
        document.records.push_back(ReadRecord(row.data()));
    }

    if (!checksumMatches && plausible * 10 < count * 9)
    {
        document.records.clear();
        document.error = "MasterSkillTreeData.bmd checksum does not match";
        return document;
    }

    document.ok = true;
    return document;
}

} // namespace UI::MasterSkill
