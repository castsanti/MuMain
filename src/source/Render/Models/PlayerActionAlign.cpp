#include "Render/Models/PlayerActionAlign.h"

#include <cstring>
#include <vector>

namespace Render::Models
{
namespace
{

#include "PlayerMotion.inc"

constexpr std::uint32_t kCrcPolynomial = 0xEDB88320u;

bool MotionsMatch(std::uint32_t left, std::uint32_t right)
{
    return left == right;
}

} // namespace

void CopySeason6PlayerMotion(std::uint32_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6PlayerMotion, sizeof(kSeason6PlayerMotion));
}

std::uint32_t HashPlayerMotion(std::uint32_t crc, const void* bytes, std::size_t size)
{
    crc = ~crc;
    const auto* data = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (kCrcPolynomial & mask);
        }
    }
    return ~crc;
}

bool MapSeason6PlayerActions(const std::uint32_t* loadedMotion, int loadedCount, int* season6ToLoaded)
{
    if (loadedMotion == nullptr || season6ToLoaded == nullptr || loadedCount < kSeason6PlayerActionCount)
    {
        return false;
    }

    const int referenceCount = kSeason6PlayerActionCount;
    std::vector<char> can(static_cast<std::size_t>(referenceCount + 1) * static_cast<std::size_t>(loadedCount + 1), 0);
    const auto at = [&](int referenceIndex, int loadedIndex) -> char& {
        return can[(static_cast<std::size_t>(referenceIndex) * static_cast<std::size_t>(loadedCount + 1)) +
                   static_cast<std::size_t>(loadedIndex)];
    };
    for (int loadedIndex = 0; loadedIndex <= loadedCount; ++loadedIndex)
    {
        at(referenceCount, loadedIndex) = 1;
    }
    for (int referenceIndex = referenceCount - 1; referenceIndex >= 0; --referenceIndex)
    {
        for (int loadedIndex = loadedCount - 1; loadedIndex >= 0; --loadedIndex)
        {
            const bool skip = at(referenceIndex, loadedIndex + 1) != 0;
            const bool take = MotionsMatch(loadedMotion[loadedIndex], kSeason6PlayerMotion[referenceIndex]) &&
                              at(referenceIndex + 1, loadedIndex + 1) != 0;
            at(referenceIndex, loadedIndex) = static_cast<char>(skip || take);
        }
    }
    if (at(0, 0) == 0)
    {
        return false;
    }

    int referenceIndex = 0;
    int loadedIndex = 0;
    while (referenceIndex < referenceCount)
    {
        const bool take = MotionsMatch(loadedMotion[loadedIndex], kSeason6PlayerMotion[referenceIndex]) &&
                          at(referenceIndex + 1, loadedIndex + 1) != 0;
        if (take)
        {
            season6ToLoaded[referenceIndex] = loadedIndex;
            ++referenceIndex;
        }
        ++loadedIndex;
    }
    return true;
}

} // namespace Render::Models
