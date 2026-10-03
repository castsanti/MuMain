#include "GameLogic/Items/ItemSetTypeScript.h"

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Core/Utilities/Checksum.h"

#include <algorithm>
#include <vector>

namespace GameLogic::Items
{
namespace
{

constexpr WORD kChecksumKey = 0xE5F1;
constexpr int kSectionLength = 512;
constexpr int kPlausibleOptionLimit = 64;
constexpr int kCandidateStrides[] = {4, 5, 8, 10, 11, 12, 16, 20, 22};

bool PlausibleOption(std::uint8_t option)
{
    return option == 0 || option == 0xFF || option < kPlausibleOptionLimit;
}

int ScoreStride(const std::uint8_t* payload, int payloadBytes, int stride)
{
    if (stride < kItemSetRecordBytes || payloadBytes % stride != 0)
    {
        return -1;
    }
    const int count = payloadBytes / stride;
    if (count % kSectionLength != 0)
    {
        return -1;
    }

    int plausible = 0;
    std::vector<BYTE> record(static_cast<std::size_t>(stride));
    for (int i = 0; i < count; ++i)
    {
        const std::uint8_t* source = payload + static_cast<std::size_t>(i * stride);
        std::copy(source, source + stride, record.begin());
        BuxConvert(record.data(), stride);
        if (PlausibleOption(record[0]))
        {
            ++plausible;
        }
    }
    return plausible;
}

ItemSetTypeDocument ReadRecords(const std::uint8_t* payload, int payloadBytes, int stride)
{
    ItemSetTypeDocument document;
    document.recordBytes = stride;
    const int count = payloadBytes / stride;
    document.records.resize(static_cast<std::size_t>(count));
    std::vector<BYTE> record(static_cast<std::size_t>(stride));
    for (int i = 0; i < count; ++i)
    {
        const std::uint8_t* source = payload + static_cast<std::size_t>(i * stride);
        std::copy(source, source + stride, record.begin());
        BuxConvert(record.data(), stride);
        document.records[static_cast<std::size_t>(i)].option[0] = record[0];
        document.records[static_cast<std::size_t>(i)].option[1] = record[1];
        document.records[static_cast<std::size_t>(i)].mixLevel[0] = record[2];
        document.records[static_cast<std::size_t>(i)].mixLevel[1] = record[3];
    }
    document.ok = true;
    return document;
}

} // namespace

ItemSetTypeDocument ParseItemSetType(const std::uint8_t* bytes, std::size_t size)
{
    ItemSetTypeDocument document;
    if (bytes == nullptr || size < sizeof(DWORD))
    {
        document.error = "ItemSetType.bmd is too small";
        return document;
    }

    const int payloadBytes = static_cast<int>(size - sizeof(DWORD));
    DWORD storedChecksum = 0;
    storedChecksum |= bytes[size - 4];
    storedChecksum |= static_cast<DWORD>(bytes[size - 3]) << 8;
    storedChecksum |= static_cast<DWORD>(bytes[size - 2]) << 16;
    storedChecksum |= static_cast<DWORD>(bytes[size - 1]) << 24;
    const bool checksumMatches =
        GenerateCheckSum2(reinterpret_cast<const BYTE*>(bytes), static_cast<DWORD>(payloadBytes), kChecksumKey) ==
        storedChecksum;
    const bool knownSize = size == static_cast<std::size_t>(kSeason6ItemSetFileBytes) ||
                           size == static_cast<std::size_t>(kSeason21ItemSetFileBytes);
    if (!checksumMatches && !knownSize)
    {
        document.error = "ItemSetType.bmd checksum does not match";
        return document;
    }

    if (size == static_cast<std::size_t>(kSeason6ItemSetFileBytes))
    {
        if (!checksumMatches)
        {
            document.error = "ItemSetType.bmd checksum does not match";
            return document;
        }
        return ReadRecords(bytes, payloadBytes, kItemSetRecordBytes);
    }

    const int fourByteScore = ScoreStride(bytes, payloadBytes, kItemSetRecordBytes);
    const int fourByteCount = payloadBytes / kItemSetRecordBytes;
    if (fourByteScore >= 0 && fourByteScore * 10 >= fourByteCount * 9)
    {
        return ReadRecords(bytes, payloadBytes, kItemSetRecordBytes);
    }

    int bestStride = 0;
    int bestScore = 0;
    int bestCount = 1;
    for (const int stride : kCandidateStrides)
    {
        const int score = ScoreStride(bytes, payloadBytes, stride);
        if (score < 0)
        {
            continue;
        }
        const int count = payloadBytes / stride;
        if (bestStride == 0 || score * bestCount > bestScore * count)
        {
            bestStride = stride;
            bestScore = score;
            bestCount = count;
        }
    }

    if (bestStride == 0 || bestScore * 10 < bestCount * 9)
    {
        document.error = "ItemSetType.bmd record layout was not recognized";
        return document;
    }
    return ReadRecords(bytes, payloadBytes, bestStride);
}

} // namespace GameLogic::Items
