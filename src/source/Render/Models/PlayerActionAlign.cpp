#include "Render/Models/PlayerActionAlign.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Render::Models
{
namespace
{

#include "PlayerMotion.inc"
#include "PlayerPose.inc"
#include "PlayerCoarsePose.inc"

static_assert(sizeof(kSeason6PlayerBones) / sizeof(kSeason6PlayerBones[0]) == kSeason6PlayerBoneCount);
static_assert(sizeof(kSeason6PlayerPose) / sizeof(kSeason6PlayerPose[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6PlayerCoarsePose) / sizeof(kSeason6PlayerCoarsePose[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6IdleAction) / sizeof(kSeason6IdleAction[0]) == kSeason6IdleActionCount);

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

std::int16_t QuantizePlayerAngleAt(float radians, float step)
{
    if (step == 0.f)
    {
        return 0;
    }
    const float scaled = radians / step;
    const float rounded = scaled >= 0.f ? scaled + 0.5f : scaled - 0.5f;
    return static_cast<std::int16_t>(rounded);
}

std::int16_t QuantizePlayerAngle(float radians)
{
    return QuantizePlayerAngleAt(radians, kPlayerPoseStep);
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

void CopySeason6PlayerCoarsePose(std::uint32_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6PlayerCoarsePose, sizeof(kSeason6PlayerCoarsePose));
}

bool MapSeason6PlayerCoarsePoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded)
{
    return MapActionSequence(kSeason6PlayerCoarsePose, loadedPose, loadedCount, season6ToLoaded);
}

namespace
{

int CountPose(const std::uint32_t* poses, int count, std::uint32_t pose)
{
    int found = 0;
    for (int index = 0; index < count; ++index)
    {
        if (poses[index] == pose)
        {
            ++found;
        }
    }
    return found;
}

} // namespace

int MapSeason6PlayerUniquePoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded)
{
    if (loadedPose == nullptr || season6ToLoaded == nullptr || loadedCount < kSeason6PlayerActionCount)
    {
        return 0;
    }

    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        season6ToLoaded[index] = -1;
    }

    int assigned = 0;
    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        const std::uint32_t pose = kSeason6PlayerCoarsePose[index];
        if (CountPose(kSeason6PlayerCoarsePose, kSeason6PlayerActionCount, pose) != 1 ||
            CountPose(loadedPose, loadedCount, pose) != 1)
        {
            continue;
        }
        for (int loaded = 0; loaded < loadedCount; ++loaded)
        {
            if (loadedPose[loaded] == pose)
            {
                season6ToLoaded[index] = loaded;
                ++assigned;
                break;
            }
        }
    }
    return assigned;
}

int Season6IdleActionIndex(int idleSlot)
{
    if (idleSlot < 0 || idleSlot >= kSeason6IdleActionCount)
    {
        return -1;
    }
    return kSeason6IdleAction[idleSlot];
}

void CopySeason6IdlePose(int idleSlot, std::int16_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6IdlePoseAngles || idleSlot < 0 || idleSlot >= kSeason6IdleActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6IdlePose[idleSlot], sizeof(kSeason6IdlePose[idleSlot]));
}

int PlayerPoseDistance(const std::int16_t* left, const std::int16_t* right, int count)
{
    if (left == nullptr || right == nullptr || count <= 0)
    {
        return 0;
    }
    int distance = 0;
    for (int index = 0; index < count; ++index)
    {
        if (left[index] == 0x7FFF || right[index] == 0x7FFF)
        {
            continue;
        }
        distance += std::abs(static_cast<int>(left[index]) - static_cast<int>(right[index]));
    }
    return distance;
}

int PlaceSeason6IdleClips(const std::int16_t* loadedAngles, const float* travel, int loadedCount, int* season6ToLoaded)
{
    if (loadedAngles == nullptr || travel == nullptr || season6ToLoaded == nullptr || loadedCount <= 0)
    {
        return 0;
    }

    std::vector<char> claimed(static_cast<std::size_t>(loadedCount), 0);
    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        const int loaded = season6ToLoaded[index];
        if (loaded >= 0 && loaded < loadedCount)
        {
            claimed[static_cast<std::size_t>(loaded)] = 1;
        }
    }

    int placed = 0;
    for (int idleSlot = 0; idleSlot < kSeason6IdleActionCount; ++idleSlot)
    {
        const int action = kSeason6IdleAction[idleSlot];
        if (season6ToLoaded[action] >= 0)
        {
            continue;
        }
        int best = -1;
        int bestDistance = kIdlePoseDistanceLimit + 1;
        for (int loaded = 0; loaded < loadedCount; ++loaded)
        {
            if (claimed[static_cast<std::size_t>(loaded)] != 0 || travel[loaded] > kIdleTravelLimit)
            {
                continue;
            }
            const int distance = PlayerPoseDistance(loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                                    kSeason6IdlePose[idleSlot], kSeason6IdlePoseAngles);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = loaded;
            }
        }
        if (best < 0)
        {
            continue;
        }
        season6ToLoaded[action] = best;
        claimed[static_cast<std::size_t>(best)] = 1;
        ++placed;
    }
    return placed;
}

void CompleteSeason6ActionMap(int* season6ToLoaded, int loadedCount)
{
    if (season6ToLoaded == nullptr || loadedCount < kSeason6PlayerActionCount)
    {
        return;
    }

    std::vector<char> used(static_cast<std::size_t>(loadedCount), 0);
    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        const int loaded = season6ToLoaded[index];
        if (loaded >= 0 && loaded < loadedCount)
        {
            used[static_cast<std::size_t>(loaded)] = 1;
        }
    }

    int cursor = 0;
    auto nextFree = [&]() {
        while (cursor < loadedCount && used[static_cast<std::size_t>(cursor)] != 0)
        {
            ++cursor;
        }
        if (cursor >= loadedCount)
        {
            return -1;
        }
        const int found = cursor;
        used[static_cast<std::size_t>(found)] = 1;
        ++cursor;
        return found;
    };

    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        if (season6ToLoaded[index] >= 0)
        {
            continue;
        }
        if (index < loadedCount && used[static_cast<std::size_t>(index)] == 0)
        {
            season6ToLoaded[index] = index;
            used[static_cast<std::size_t>(index)] = 1;
            continue;
        }
        season6ToLoaded[index] = nextFree();
    }
}

} // namespace Render::Models
