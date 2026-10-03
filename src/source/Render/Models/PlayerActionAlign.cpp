#include "Render/Models/PlayerActionAlign.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace Render::Models
{
namespace
{

#include "PlayerMotion.inc"
#include "PlayerPose.inc"

static_assert(sizeof(kSeason6PlayerBones) / sizeof(kSeason6PlayerBones[0]) == kSeason6PlayerBoneCount);
static_assert(sizeof(kSeason6PlayerPose) / sizeof(kSeason6PlayerPose[0]) == kSeason6PlayerActionCount);

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

void CopySeason6PlayerPose(std::uint32_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6PlayerPose, sizeof(kSeason6PlayerPose));
}

const char* Season6PlayerBoneName(int index)
{
    if (index < 0 || index >= kSeason6PlayerBoneCount)
    {
        return "";
    }
    return kSeason6PlayerBones[index];
}

std::int16_t QuantizePlayerAngle(float radians)
{
    const float scaled = radians / kPlayerPoseStep;
    const float rounded = scaled >= 0.f ? scaled + 0.5f : scaled - 0.5f;
    return static_cast<std::int16_t>(rounded);
}

std::uint32_t HashPlayerPose(std::uint16_t keys, std::uint8_t lock, const std::int16_t* angles, int angleCount)
{
    const std::uint8_t header[4] = {
        static_cast<std::uint8_t>(keys & 0xFF),
        static_cast<std::uint8_t>((keys >> 8) & 0xFF),
        lock,
        0,
    };
    std::uint32_t crc = HashPlayerMotion(0, header, sizeof(header));
    if (angles == nullptr || angleCount <= 0)
    {
        return crc;
    }
    return HashPlayerMotion(crc, angles, static_cast<std::size_t>(angleCount) * sizeof(std::int16_t));
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

bool MapActionSequence(const std::uint32_t* reference, const std::uint32_t* loaded, int loadedCount,
                       int* season6ToLoaded)
{
    if (reference == nullptr || loaded == nullptr || season6ToLoaded == nullptr ||
        loadedCount < kSeason6PlayerActionCount)
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
            const bool take = MotionsMatch(loaded[loadedIndex], reference[referenceIndex]) &&
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
        const bool take = MotionsMatch(loaded[loadedIndex], reference[referenceIndex]) &&
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

bool MapSeason6PlayerActions(const std::uint32_t* loadedMotion, int loadedCount, int* season6ToLoaded)
{
    return MapActionSequence(kSeason6PlayerMotion, loadedMotion, loadedCount, season6ToLoaded);
}

bool MapSeason6PlayerPoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded)
{
    return MapActionSequence(kSeason6PlayerPose, loadedPose, loadedCount, season6ToLoaded);
}

} // namespace Render::Models
