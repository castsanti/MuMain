#include "Render/Models/PlayerActionAlign.h"

#include <algorithm>
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
#include "PlayerMotionClass.inc"
#include "PlayerActionNames.inc"
#include "PlayerActionKeys.inc"

static_assert(sizeof(kSeason6PlayerBones) / sizeof(kSeason6PlayerBones[0]) == kSeason6PlayerBoneCount);
static_assert(sizeof(kSeason6PlayerPose) / sizeof(kSeason6PlayerPose[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6PlayerCoarsePose) / sizeof(kSeason6PlayerCoarsePose[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6IdleAction) / sizeof(kSeason6IdleAction[0]) == kSeason6IdleActionCount);
static_assert(sizeof(kSeason6MotionClass) / sizeof(kSeason6MotionClass[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6MotionPose) / sizeof(kSeason6MotionPose[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6ActionName) / sizeof(kSeason6ActionName[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6ActionKeys) / sizeof(kSeason6ActionKeys[0]) == kSeason6PlayerActionCount);
static_assert(sizeof(kSeason6ActionLock) / sizeof(kSeason6ActionLock[0]) == kSeason6PlayerActionCount);

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

std::uint8_t Season6MotionClass(int action)
{
    if (action < 0 || action >= kSeason6PlayerActionCount)
    {
        return static_cast<std::uint8_t>(PlayerMotionClass::GroundStill);
    }
    return kSeason6MotionClass[action];
}

void CopySeason6MotionPose(int action, std::int16_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6IdlePoseAngles || action < 0 || action >= kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6MotionPose[action], sizeof(kSeason6MotionPose[action]));
}

namespace
{

std::uint8_t ClassifyLoadedMotion(float pathLength, float meanHeight, float groundHeight)
{
    const bool air = meanHeight > groundHeight + kAirAboveGround;
    const bool still = pathLength <= kMotionStillPath;
    if (air && still)
    {
        return static_cast<std::uint8_t>(PlayerMotionClass::AirStill);
    }
    if (air)
    {
        return static_cast<std::uint8_t>(PlayerMotionClass::AirMove);
    }
    if (still)
    {
        return static_cast<std::uint8_t>(PlayerMotionClass::GroundStill);
    }
    return static_cast<std::uint8_t>(PlayerMotionClass::GroundMove);
}

float DetectGroundHeight(const std::int16_t* loadedAngles, const float* pathLength, const float* meanHeight,
                         int loadedCount)
{
    std::int16_t idlePose[kSeason6IdlePoseAngles];
    CopySeason6IdlePose(0, idlePose, kSeason6IdlePoseAngles);
    int best = -1;
    int bestDistance = 0;
    for (int loaded = 0; loaded < loadedCount; ++loaded)
    {
        if (pathLength[loaded] > kMotionStillPath)
        {
            continue;
        }
        const int distance = PlayerPoseDistance(loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                                idlePose, kSeason6IdlePoseAngles);
        if (best < 0 || distance < bestDistance)
        {
            bestDistance = distance;
            best = loaded;
        }
    }
    if (best < 0)
    {
        return kReferenceGroundHeight;
    }
    return meanHeight[best];
}

struct MotionPair
{
    int distance = 0;
    int action = 0;
    int loaded = 0;
};

bool EarlierPair(const MotionPair& left, const MotionPair& right)
{
    if (left.distance != right.distance)
    {
        return left.distance < right.distance;
    }
    if (left.action != right.action)
    {
        return left.action < right.action;
    }
    return left.loaded < right.loaded;
}

void AssignSameClass(const std::int16_t* loadedAngles, const std::uint8_t* loadedClass, int loadedCount,
                     int* season6ToLoaded, std::vector<char>& clipUsed)
{
    std::vector<MotionPair> pairs;
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        const std::int16_t* pose = kSeason6MotionPose[action];
        for (int loaded = 0; loaded < loadedCount; ++loaded)
        {
            if (loadedClass[loaded] != kSeason6MotionClass[action])
            {
                continue;
            }
            MotionPair pair;
            pair.distance = PlayerPoseDistance(pose, loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                               kSeason6IdlePoseAngles);
            pair.action = action;
            pair.loaded = loaded;
            pairs.push_back(pair);
        }
    }
    std::sort(pairs.begin(), pairs.end(), EarlierPair);

    std::vector<char> actionUsed(static_cast<std::size_t>(kSeason6PlayerActionCount), 0);
    for (const MotionPair& pair : pairs)
    {
        if (actionUsed[static_cast<std::size_t>(pair.action)] != 0 || clipUsed[static_cast<std::size_t>(pair.loaded)] != 0)
        {
            continue;
        }
        season6ToLoaded[pair.action] = pair.loaded;
        actionUsed[static_cast<std::size_t>(pair.action)] = 1;
        clipUsed[static_cast<std::size_t>(pair.loaded)] = 1;
    }
}

void AssignLeftoverClips(const std::int16_t* loadedAngles, int loadedCount, int* season6ToLoaded,
                         std::vector<char>& clipUsed)
{
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        if (season6ToLoaded[action] >= 0)
        {
            continue;
        }
        int best = -1;
        int bestDistance = 0;
        const std::int16_t* pose = kSeason6MotionPose[action];
        for (int loaded = 0; loaded < loadedCount; ++loaded)
        {
            if (clipUsed[static_cast<std::size_t>(loaded)] != 0)
            {
                continue;
            }
            const int distance = PlayerPoseDistance(pose, loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                                    kSeason6IdlePoseAngles);
            if (best < 0 || distance < bestDistance || (distance == bestDistance && loaded < best))
            {
                bestDistance = distance;
                best = loaded;
            }
        }
        if (best < 0)
        {
            return;
        }
        season6ToLoaded[action] = best;
        clipUsed[static_cast<std::size_t>(best)] = 1;
    }
}

int CountClassMatches(const std::uint8_t* loadedClass, int loadedCount, const int* season6ToLoaded)
{
    int matched = 0;
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        const int loaded = season6ToLoaded[action];
        if (loaded < 0 || loaded >= loadedCount)
        {
            continue;
        }
        if (loadedClass[loaded] == kSeason6MotionClass[action])
        {
            ++matched;
        }
    }
    return matched;
}

} // namespace

int MapSeason6PlayerMotions(const std::int16_t* loadedAngles, const float* pathLength, const float* meanHeight,
                            int loadedCount, int* season6ToLoaded)
{
    if (loadedAngles == nullptr || pathLength == nullptr || meanHeight == nullptr || season6ToLoaded == nullptr ||
        loadedCount < kSeason6PlayerActionCount)
    {
        return -1;
    }

    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        season6ToLoaded[action] = -1;
    }

    const float groundHeight = DetectGroundHeight(loadedAngles, pathLength, meanHeight, loadedCount);
    std::vector<std::uint8_t> loadedClass(static_cast<std::size_t>(loadedCount));
    for (int loaded = 0; loaded < loadedCount; ++loaded)
    {
        loadedClass[static_cast<std::size_t>(loaded)] =
            ClassifyLoadedMotion(pathLength[loaded], meanHeight[loaded], groundHeight);
    }

    std::vector<char> clipUsed(static_cast<std::size_t>(loadedCount), 0);
    AssignSameClass(loadedAngles, loadedClass.data(), loadedCount, season6ToLoaded, clipUsed);
    AssignLeftoverClips(loadedAngles, loadedCount, season6ToLoaded, clipUsed);
    return CountClassMatches(loadedClass.data(), loadedCount, season6ToLoaded);
}

const char* Season6ActionName(int action)
{
    if (action < 0 || action >= kSeason6PlayerActionCount)
    {
        return "";
    }
    return kSeason6ActionName[action];
}

void CopySeason6ActionKeys(std::uint16_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6ActionKeys, sizeof(kSeason6ActionKeys));
}

void CopySeason6ActionLocks(std::uint8_t* destination, int count)
{
    if (destination == nullptr || count != kSeason6PlayerActionCount)
    {
        return;
    }
    std::memcpy(destination, kSeason6ActionLock, sizeof(kSeason6ActionLock));
}

namespace
{

constexpr const char* kAttackEndAlias = "PLAYER_FLY_RIDE";

const char* ActionBasename(const char* name)
{
    const char* base = name;
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            base = cursor + 1;
        }
    }
    return base;
}

bool NameMatchesAction(const char* loaded, const char* action)
{
    if (loaded == nullptr || loaded[0] == '\0' || action == nullptr)
    {
        return false;
    }
    const char* base = ActionBasename(loaded);
    const std::size_t actionLength = std::strlen(action);
    if (std::strncmp(base, action, actionLength) != 0)
    {
        return false;
    }
    return base[actionLength] == '\0' || std::strcmp(base + actionLength, ".smd") == 0;
}

int FindNamedClip(const char* const* loadedNames, int loadedCount, const std::vector<char>& used, const char* action)
{
    for (int loaded = 0; loaded < loadedCount; ++loaded)
    {
        if (used[static_cast<std::size_t>(loaded)] != 0)
        {
            continue;
        }
        if (NameMatchesAction(loadedNames[loaded], action))
        {
            return loaded;
        }
    }
    return -1;
}

int PoseCostAt(const std::int16_t* loadedAngles, int loaded, int action)
{
    std::int16_t reference[kSeason6IdlePoseAngles];
    CopySeason6MotionPose(action, reference, kSeason6IdlePoseAngles);
    return PlayerPoseDistance(reference, loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                              kSeason6IdlePoseAngles);
}

bool RidingSkillClipsInserted(const std::int16_t* loadedAngles, int loadedCount)
{
    constexpr int kAnchors[] = {kSeason6DefenseAction, kSeason6DieAction, kSeason6RageIdleAction};
    if (kSeason6RageIdleAction + kSeason21RidingSkillCount >= loadedCount)
    {
        return false;
    }
    int sameCost = 0;
    int shiftedCost = 0;
    for (const int anchor : kAnchors)
    {
        sameCost += PoseCostAt(loadedAngles, anchor, anchor);
        shiftedCost += PoseCostAt(loadedAngles, anchor + kSeason21RidingSkillCount, anchor);
    }
    return shiftedCost < sameCost && shiftedCost * 2 < sameCost;
}

} // namespace

int MapSeason6PlayerActionsByNames(const char* const* loadedNames, int loadedCount, int* season6ToLoaded)
{
    if (loadedNames == nullptr || season6ToLoaded == nullptr || loadedCount <= 0)
    {
        return -1;
    }
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        season6ToLoaded[action] = -1;
    }
    std::vector<char> used(static_cast<std::size_t>(loadedCount), 0);
    int matched = 0;
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        int loaded = FindNamedClip(loadedNames, loadedCount, used, kSeason6ActionName[action]);
        if (loaded < 0 && action == kSeason6AttackEndAction)
        {
            loaded = FindNamedClip(loadedNames, loadedCount, used, kAttackEndAlias);
        }
        if (loaded < 0)
        {
            continue;
        }
        season6ToLoaded[action] = loaded;
        used[static_cast<std::size_t>(loaded)] = 1;
        ++matched;
    }
    return matched;
}

bool MapSeason6PlayerActionList(const std::int16_t* loadedAngles, int loadedCount, int* season6ToLoaded)
{
    if (season6ToLoaded == nullptr || loadedCount < kSeason6PlayerActionCount)
    {
        return false;
    }
    const bool riding = loadedAngles != nullptr && RidingSkillClipsInserted(loadedAngles, loadedCount);
    const int shift = riding ? kSeason21RidingSkillCount : 0;
    if (kSeason6RageIdleAction + shift >= loadedCount)
    {
        return false;
    }
    for (int action = 0; action < kSeason6PlayerActionCount; ++action)
    {
        season6ToLoaded[action] = action < kSeason6DefenseAction ? action : action + shift;
    }
    return true;
}

namespace
{

// A one-frame change is cheaper than skipping the clip. A different motion
// class costs more than a small pose miss and less than the wrong clip.
constexpr int kClipKeyWeight = 40;
constexpr int kClipLockWeight = 120;
constexpr int kClipClassWeight = 250;
constexpr int kClipSkipCost = 8;
constexpr std::int64_t kClipAlignInfinity = 1000000000;

std::size_t ClipCell(int action, int loaded, int loadedCount)
{
    return static_cast<std::size_t>(action) * static_cast<std::size_t>(loadedCount + 1) +
           static_cast<std::size_t>(loaded);
}

int ClipMatchCost(int action, int loaded, const std::uint16_t* loadedKeys, const std::uint8_t* loadedLocks,
                  const std::int16_t* loadedAngles, const std::uint8_t* loadedClass)
{
    int cost = 0;
    if (loadedAngles != nullptr)
    {
        cost += PlayerPoseDistance(kSeason6MotionPose[action],
                                   loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                   kSeason6IdlePoseAngles);
    }
    const int keyDelta =
        std::abs(static_cast<int>(loadedKeys[loaded]) - static_cast<int>(kSeason6ActionKeys[action]));
    cost += keyDelta * kClipKeyWeight;
    if (loadedLocks[loaded] != kSeason6ActionLock[action])
    {
        cost += kClipLockWeight;
    }
    if (loadedClass != nullptr && loadedClass[loaded] != kSeason6MotionClass[action])
    {
        cost += kClipClassWeight;
    }
    return cost;
}

void ClassifyClipMotions(const std::int16_t* loadedAngles, const float* pathLength, const float* meanHeight,
                         int loadedCount, std::vector<std::uint8_t>& loadedClass)
{
    const float groundHeight = DetectGroundHeight(loadedAngles, pathLength, meanHeight, loadedCount);
    loadedClass.resize(static_cast<std::size_t>(loadedCount));
    for (int loaded = 0; loaded < loadedCount; ++loaded)
    {
        loadedClass[static_cast<std::size_t>(loaded)] =
            ClassifyLoadedMotion(pathLength[loaded], meanHeight[loaded], groundHeight);
    }
}

bool TraceClipOrder(const std::vector<char>& take, int loadedCount, int* season6ToLoaded)
{
    int action = kSeason6PlayerActionCount;
    int loaded = loadedCount;
    while (action > 0)
    {
        if (loaded <= 0)
        {
            return false;
        }
        if (take[ClipCell(action, loaded, loadedCount)] != 0)
        {
            season6ToLoaded[action - 1] = loaded - 1;
            --action;
        }
        --loaded;
    }
    return true;
}

std::int64_t CheaperClipCost(std::int64_t prior, int added)
{
    if (prior >= kClipAlignInfinity)
    {
        return kClipAlignInfinity;
    }
    return prior + added;
}

bool AlignClipOrder(const std::uint16_t* loadedKeys, const std::uint8_t* loadedLocks,
                    const std::int16_t* loadedAngles, const std::uint8_t* loadedClass, int loadedCount,
                    int* season6ToLoaded)
{
    const int actionCount = kSeason6PlayerActionCount;
    const std::size_t cells = static_cast<std::size_t>(actionCount + 1) * static_cast<std::size_t>(loadedCount + 1);
    std::vector<std::int64_t> cost(cells, kClipAlignInfinity);
    std::vector<char> take(cells, 0);
    cost[ClipCell(0, 0, loadedCount)] = 0;
    for (int loaded = 1; loaded <= loadedCount; ++loaded)
    {
        cost[ClipCell(0, loaded, loadedCount)] = cost[ClipCell(0, loaded - 1, loadedCount)] + kClipSkipCost;
    }
    for (int action = 1; action <= actionCount; ++action)
    {
        for (int loaded = 1; loaded <= loadedCount; ++loaded)
        {
            const std::int64_t skipCost =
                CheaperClipCost(cost[ClipCell(action, loaded - 1, loadedCount)], kClipSkipCost);
            const int match =
                ClipMatchCost(action - 1, loaded - 1, loadedKeys, loadedLocks, loadedAngles, loadedClass);
            const std::int64_t takeCost =
                CheaperClipCost(cost[ClipCell(action - 1, loaded - 1, loadedCount)], match);
            if (takeCost < skipCost)
            {
                cost[ClipCell(action, loaded, loadedCount)] = takeCost;
                take[ClipCell(action, loaded, loadedCount)] = 1;
                continue;
            }
            cost[ClipCell(action, loaded, loadedCount)] = skipCost;
        }
    }
    if (cost[ClipCell(actionCount, loadedCount, loadedCount)] >= kClipAlignInfinity)
    {
        return false;
    }
    return TraceClipOrder(take, loadedCount, season6ToLoaded);
}

} // namespace

bool MapSeason6PlayerClipOrder(const std::uint16_t* loadedKeys, const std::uint8_t* loadedLocks,
                               const std::int16_t* loadedAngles, const float* pathLength, const float* meanHeight,
                               int loadedCount, int* season6ToLoaded)
{
    if (loadedKeys == nullptr || loadedLocks == nullptr || season6ToLoaded == nullptr ||
        loadedCount < kSeason6PlayerActionCount || loadedCount > kMaxSeason21PlayerActions)
    {
        return false;
    }
    if ((pathLength == nullptr) != (meanHeight == nullptr))
    {
        return false;
    }
    if (pathLength != nullptr && loadedAngles == nullptr)
    {
        return false;
    }
    std::vector<std::uint8_t> loadedClass;
    const std::uint8_t* classes = nullptr;
    if (pathLength != nullptr)
    {
        ClassifyClipMotions(loadedAngles, pathLength, meanHeight, loadedCount, loadedClass);
        classes = loadedClass.data();
    }
    return AlignClipOrder(loadedKeys, loadedLocks, loadedAngles, classes, loadedCount, season6ToLoaded);
}

namespace
{

bool IsStandingTravel(float pathLength, float maxStep)
{
    return pathLength <= kStandingPathLimit && maxStep <= kStandingStepLimit;
}

bool ClipInRange(int loaded, int loadedCount)
{
    return loaded >= 0 && loaded < loadedCount;
}

bool KeptStandingIdle(const int* season6ToLoaded, const float* pathLength, const float* maxStep, int loaded,
                      int loadedCount)
{
    if (!ClipInRange(loaded, loadedCount) || !IsStandingTravel(pathLength[loaded], maxStep[loaded]))
    {
        return false;
    }
    for (int slot = 0; slot < kSeason6IdleActionCount; ++slot)
    {
        if (season6ToLoaded[kSeason6IdleAction[slot]] == loaded)
        {
            return true;
        }
    }
    return false;
}

int FindStandingClip(const std::int16_t* loadedAngles, const float* pathLength, const float* maxStep, int loadedCount,
                     const int* season6ToLoaded, const std::int16_t* idlePose)
{
    int best = -1;
    int bestDistance = 0;
    for (int loaded = 0; loaded < loadedCount; ++loaded)
    {
        if (!IsStandingTravel(pathLength[loaded], maxStep[loaded]))
        {
            continue;
        }
        if (KeptStandingIdle(season6ToLoaded, pathLength, maxStep, loaded, loadedCount))
        {
            continue;
        }
        const int distance = PlayerPoseDistance(loadedAngles + static_cast<std::size_t>(loaded) * kSeason6IdlePoseAngles,
                                                idlePose, kSeason6IdlePoseAngles);
        if (best < 0 || distance < bestDistance)
        {
            bestDistance = distance;
            best = loaded;
        }
    }
    return best;
}

int ActionUsingClip(const int* season6ToLoaded, int loaded)
{
    for (int index = 0; index < kSeason6PlayerActionCount; ++index)
    {
        if (season6ToLoaded[index] == loaded)
        {
            return index;
        }
    }
    return -1;
}

} // namespace

int PlaceSeason6IdleClips(const std::int16_t* loadedAngles, const float* pathLength, const float* maxStep,
                          int loadedCount, int* season6ToLoaded)
{
    if (loadedAngles == nullptr || pathLength == nullptr || maxStep == nullptr || season6ToLoaded == nullptr ||
        loadedCount <= 0)
    {
        return 0;
    }

    int placed = 0;
    for (int idleSlot = 0; idleSlot < kSeason6IdleActionCount; ++idleSlot)
    {
        const int action = kSeason6IdleAction[idleSlot];
        const int current = season6ToLoaded[action];
        if (ClipInRange(current, loadedCount) && IsStandingTravel(pathLength[current], maxStep[current]))
        {
            continue;
        }

        const int best = FindStandingClip(loadedAngles, pathLength, maxStep, loadedCount, season6ToLoaded,
                                          kSeason6IdlePose[idleSlot]);
        if (best < 0)
        {
            if (current >= 0)
            {
                season6ToLoaded[action] = -1;
                ++placed;
            }
            continue;
        }

        const int owner = ActionUsingClip(season6ToLoaded, best);
        season6ToLoaded[action] = best;
        if (owner >= 0 && owner != action)
        {
            season6ToLoaded[owner] = current;
        }
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
