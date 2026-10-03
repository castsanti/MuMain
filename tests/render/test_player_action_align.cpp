#include "doctest.h"

#include "Render/Models/PlayerActionAlign.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

TEST_CASE("player motion hash matches the standard CRC")
{
    const char* digits = "123456789";
    const std::uint32_t crc = Render::Models::HashPlayerMotion(0, digits, std::strlen(digits));
    CHECK(crc == 0xCBF43926u);
}

TEST_CASE("Season 6 player actions map onto themselves")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerMotion(loaded.data(), static_cast<int>(loaded.size()));
    std::vector<int> season6ToLoaded(loaded.size(), -1);
    REQUIRE(Render::Models::MapSeason6PlayerActions(loaded.data(), static_cast<int>(loaded.size()),
                                                    season6ToLoaded.data()));
    for (int i = 0; i < Render::Models::kSeason6PlayerActionCount; ++i)
    {
        CHECK(season6ToLoaded[static_cast<std::size_t>(i)] == i);
    }
}

TEST_CASE("an inserted clip shifts later Season 6 player actions")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerMotion(loaded.data(), static_cast<int>(loaded.size()));
    constexpr int kInsertedAt = 2;
    loaded.insert(loaded.begin() + kInsertedAt, 0xA11CEu);

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount), -1);
    REQUIRE(Render::Models::MapSeason6PlayerActions(loaded.data(), static_cast<int>(loaded.size()),
                                                    season6ToLoaded.data()));
    CHECK(season6ToLoaded[0] == 0);
    CHECK(season6ToLoaded[1] == 1);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kInsertedAt)] == kInsertedAt + 1);
    CHECK(season6ToLoaded.back() == Render::Models::kSeason6PlayerActionCount);
}

TEST_CASE("Season 6 player poses map onto themselves")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerPose(loaded.data(), static_cast<int>(loaded.size()));
    std::vector<int> season6ToLoaded(loaded.size(), -1);
    REQUIRE(Render::Models::MapSeason6PlayerPoses(loaded.data(), static_cast<int>(loaded.size()),
                                                 season6ToLoaded.data()));
    CHECK(season6ToLoaded[1] == 1);
}

TEST_CASE("a pose inserted ahead of idle moves the Season 6 idle clip")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerPose(loaded.data(), static_cast<int>(loaded.size()));
    loaded.insert(loaded.begin(), 0xC4A65E01u);

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount), -1);
    REQUIRE(Render::Models::MapSeason6PlayerPoses(loaded.data(), static_cast<int>(loaded.size()),
                                                 season6ToLoaded.data()));
    CHECK(season6ToLoaded[0] == 1);
    CHECK(season6ToLoaded[1] == 2);
}

TEST_CASE("player pose angles round halfway away from zero")
{
    CHECK(Render::Models::QuantizePlayerAngle(0.1f) == 1);
    CHECK(Render::Models::QuantizePlayerAngle(-0.1f) == -1);
    CHECK(Render::Models::QuantizePlayerAngle(0.05f) == 1);
    CHECK(Render::Models::QuantizePlayerAngle(-0.05f) == -1);
}

TEST_CASE("a player file that dropped a Season 6 clip is not remapped")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerMotion(loaded.data(), static_cast<int>(loaded.size()));
    loaded[4] ^= 0xFFFFFFFFu;
    std::vector<int> season6ToLoaded(loaded.size(), -1);
    CHECK_FALSE(Render::Models::MapSeason6PlayerActions(loaded.data(), static_cast<int>(loaded.size()),
                                                        season6ToLoaded.data()));
}

TEST_CASE("coarse player poses map the idle clip onto itself")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerCoarsePose(loaded.data(), static_cast<int>(loaded.size()));
    std::vector<int> season6ToLoaded(loaded.size(), -1);
    REQUIRE(Render::Models::MapSeason6PlayerCoarsePoses(loaded.data(), static_cast<int>(loaded.size()),
                                                       season6ToLoaded.data()));
    CHECK(season6ToLoaded[1] == 1);
}

TEST_CASE("a unique coarse idle pose is found after it is swapped")
{
    std::vector<std::uint32_t> loaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount));
    Render::Models::CopySeason6PlayerCoarsePose(loaded.data(), static_cast<int>(loaded.size()));
    std::swap(loaded[1], loaded[3]);

    std::vector<int> season6ToLoaded(loaded.size(), -1);
    REQUIRE(Render::Models::MapSeason6PlayerUniquePoses(loaded.data(), static_cast<int>(loaded.size()),
                                                       season6ToLoaded.data()) > 0);
    CHECK(season6ToLoaded[1] == 3);
    CHECK(season6ToLoaded[3] == 1);
}

TEST_CASE("a charge stored at idle is replaced by the standing clip")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    std::vector<std::int16_t> loaded(static_cast<std::size_t>(kActions) * kAngles, 0);
    constexpr int kStandingClip = 20;
    Render::Models::CopySeason6IdlePose(0, loaded.data() + static_cast<std::size_t>(kStandingClip) * kAngles, kAngles);

    std::vector<float> path(static_cast<std::size_t>(kActions), 200.f);
    std::vector<float> step(static_cast<std::size_t>(kActions), 40.f);
    path[static_cast<std::size_t>(kStandingClip)] = 0.07f;
    step[static_cast<std::size_t>(kStandingClip)] = 0.02f;
    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::PlaceSeason6IdleClips(loaded.data(), path.data(), step.data(), kActions,
                                                 season6ToLoaded.data()) >= 1);
    CHECK(season6ToLoaded[1] == kStandingClip);
}

TEST_CASE("a looping walk is not used as the idle clip")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    std::vector<std::int16_t> loaded(static_cast<std::size_t>(kActions) * kAngles, 0);
    constexpr int kWalkClip = 15;
    constexpr int kStandingClip = 20;
    Render::Models::CopySeason6IdlePose(0, loaded.data() + static_cast<std::size_t>(kStandingClip) * kAngles, kAngles);
    Render::Models::CopySeason6IdlePose(0, loaded.data() + static_cast<std::size_t>(kWalkClip) * kAngles, kAngles);

    std::vector<float> path(static_cast<std::size_t>(kActions), 30.f);
    std::vector<float> step(static_cast<std::size_t>(kActions), 8.f);
    path[static_cast<std::size_t>(kWalkClip)] = 245.f;
    step[static_cast<std::size_t>(kWalkClip)] = 50.f;
    path[static_cast<std::size_t>(kStandingClip)] = 0.07f;
    step[static_cast<std::size_t>(kStandingClip)] = 0.02f;

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    season6ToLoaded[1] = kWalkClip;
    season6ToLoaded[40] = kStandingClip;
    REQUIRE(Render::Models::PlaceSeason6IdleClips(loaded.data(), path.data(), step.data(), kActions,
                                                 season6ToLoaded.data()) >= 1);
    CHECK(season6ToLoaded[1] == kStandingClip);
    CHECK(season6ToLoaded[40] == kWalkClip);
}

namespace
{

float MotionPath(std::uint8_t motionClass)
{
    const auto kind = static_cast<Render::Models::PlayerMotionClass>(motionClass);
    if (kind == Render::Models::PlayerMotionClass::GroundStill || kind == Render::Models::PlayerMotionClass::AirStill)
    {
        return 0.f;
    }
    return Render::Models::kMotionStillPath + 1.f;
}

float MotionHeight(std::uint8_t motionClass)
{
    const auto kind = static_cast<Render::Models::PlayerMotionClass>(motionClass);
    if (kind == Render::Models::PlayerMotionClass::AirStill || kind == Render::Models::PlayerMotionClass::AirMove)
    {
        return Render::Models::kReferenceGroundHeight + Render::Models::kAirAboveGround + 10.f;
    }
    return Render::Models::kReferenceGroundHeight;
}

void FillReferenceMotions(std::vector<std::int16_t>& angles, std::vector<float>& path, std::vector<float>& height)
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    angles.assign(static_cast<std::size_t>(kActions) * kAngles, 0);
    path.assign(static_cast<std::size_t>(kActions), 0.f);
    height.assign(static_cast<std::size_t>(kActions), 0.f);
    for (int action = 0; action < kActions; ++action)
    {
        Render::Models::CopySeason6MotionPose(action, angles.data() + static_cast<std::size_t>(action) * kAngles, kAngles);
        const std::uint8_t motionClass = Render::Models::Season6MotionClass(action);
        path[static_cast<std::size_t>(action)] = MotionPath(motionClass);
        height[static_cast<std::size_t>(action)] = MotionHeight(motionClass);
    }
}

} // namespace

TEST_CASE("Season 6 motion classes map each clip onto itself")
{
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    std::vector<std::int16_t> angles;
    std::vector<float> path;
    std::vector<float> height;
    FillReferenceMotions(angles, path, height);
    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerMotions(angles.data(), path.data(), height.data(), kActions,
                                                   season6ToLoaded.data()) == kActions);
    for (int action = 0; action < kActions; ++action)
    {
        CHECK(season6ToLoaded[static_cast<std::size_t>(action)] == action);
    }
}

TEST_CASE("a foot walk stays on the ground and a wing idle stays in the air")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kWingIdle = 11;
    constexpr int kFootWalk = 15;
    std::vector<std::int16_t> angles;
    std::vector<float> path;
    std::vector<float> height;
    FillReferenceMotions(angles, path, height);

    for (int sample = 0; sample < kAngles; ++sample)
    {
        std::swap(angles[static_cast<std::size_t>(kWingIdle) * kAngles + sample],
                  angles[static_cast<std::size_t>(kFootWalk) * kAngles + sample]);
    }
    std::swap(path[static_cast<std::size_t>(kWingIdle)], path[static_cast<std::size_t>(kFootWalk)]);
    std::swap(height[static_cast<std::size_t>(kWingIdle)], height[static_cast<std::size_t>(kFootWalk)]);

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerMotions(angles.data(), path.data(), height.data(), kActions,
                                                   season6ToLoaded.data()) == kActions);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kFootWalk)] == kWingIdle);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdle)] == kFootWalk);
    CHECK(Render::Models::Season6MotionClass(kFootWalk) ==
          static_cast<std::uint8_t>(Render::Models::PlayerMotionClass::GroundMove));
    CHECK(Render::Models::Season6MotionClass(kWingIdle) ==
          static_cast<std::uint8_t>(Render::Models::PlayerMotionClass::AirMove));
}

void CopyPose(std::vector<std::int16_t>& angles, int loaded, int action)
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    Render::Models::CopySeason6MotionPose(action, angles.data() + static_cast<std::size_t>(loaded) * kAngles, kAngles);
}

TEST_CASE("the Main action list keeps walk, fly, and wing idle on their Season 6 indices")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kExtra = 40;
    std::vector<std::int16_t> angles(static_cast<std::size_t>(kActions + kExtra) * kAngles, 0);
    for (int action = 0; action < kActions; ++action)
    {
        CopyPose(angles, action, action);
    }
    std::swap_ranges(angles.begin() + static_cast<std::size_t>(15) * kAngles,
                     angles.begin() + static_cast<std::size_t>(16) * kAngles,
                     angles.begin() + static_cast<std::size_t>(36) * kAngles);

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerActionList(angles.data(), kActions + kExtra, season6ToLoaded.data()));
    CHECK(season6ToLoaded[11] == 11);
    CHECK(season6ToLoaded[15] == 15);
    CHECK(season6ToLoaded[34] == 34);
    CHECK(season6ToLoaded[36] == 36);
    CHECK(season6ToLoaded[13] == 13);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6DefenseAction)] ==
          Render::Models::kSeason6DefenseAction);
    CHECK(std::strcmp(Render::Models::Season6ActionName(15), "PLAYER_WALK_MALE") == 0);
    CHECK(std::strcmp(Render::Models::Season6ActionName(11), "PLAYER_STOP_FLY") == 0);
}

TEST_CASE("riding skill clips inserted before the emotes shift only the later actions")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kShift = Render::Models::kSeason21RidingSkillCount;
    constexpr int kDefense = Render::Models::kSeason6DefenseAction;
    const int loadedCount = kActions + kShift;
    std::vector<std::int16_t> angles(static_cast<std::size_t>(loadedCount) * kAngles, 0);
    for (int action = 0; action < kActions; ++action)
    {
        const int loaded = action < kDefense ? action : action + kShift;
        CopyPose(angles, loaded, action);
    }

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerActionList(angles.data(), loadedCount, season6ToLoaded.data()));
    CHECK(season6ToLoaded[11] == 11);
    CHECK(season6ToLoaded[15] == 15);
    CHECK(season6ToLoaded[25] == 25);
    CHECK(season6ToLoaded[34] == 34);
    CHECK(season6ToLoaded[36] == 36);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kDefense)] == kDefense + kShift);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6DieAction)] ==
          Render::Models::kSeason6DieAction + kShift);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6RageIdleAction)] ==
          Render::Models::kSeason6RageIdleAction + kShift);
}

TEST_CASE("named Season 21 clips map onto the Season 6 action of the same name")
{
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kShift = Render::Models::kSeason21RidingSkillCount;
    constexpr int kDefense = Render::Models::kSeason6DefenseAction;
    const char* inserted[kShift] = {
        "PLAYER_SKILL_GIGANTICSTORM_UNI", "PLAYER_SKILL_GIGANTICSTORM_DINO", "PLAYER_SKILL_GIGANTICSTORM_FENRIR",
        "PLAYER_ATTACK_SKILL_WHEEL_UNI",  "PLAYER_ATTACK_SKILL_WHEEL_DINO",  "PLAYER_ATTACK_SKILL_WHEEL_FENRIR",
    };
    std::vector<std::string> storage;
    storage.reserve(static_cast<std::size_t>(kActions + kShift));
    for (int action = 0; action < kActions; ++action)
    {
        if (action == kDefense)
        {
            for (const char* name : inserted)
            {
                storage.emplace_back(name);
            }
        }
        storage.emplace_back(Render::Models::Season6ActionName(action));
    }
    storage[static_cast<std::size_t>(Render::Models::kSeason6AttackEndAction)] = "Data/Player/PLAYER_FLY_RIDE.smd";

    std::vector<const char*> names(storage.size());
    for (std::size_t index = 0; index < storage.size(); ++index)
    {
        names[index] = storage[index].c_str();
    }
    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerActionsByNames(names.data(), static_cast<int>(names.size()),
                                                          season6ToLoaded.data()) == kActions);
    CHECK(season6ToLoaded[11] == 11);
    CHECK(season6ToLoaded[15] == 15);
    CHECK(season6ToLoaded[34] == 34);
    CHECK(season6ToLoaded[36] == 36);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kDefense)] == kDefense + kShift);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6AttackEndAction)] ==
          Render::Models::kSeason6AttackEndAction);
}

constexpr int kInsertedClipKeys = 400;
constexpr int kIdleAction = 1;
constexpr int kWingIdleAction = 11;
constexpr int kRideIdleAction = 13;
constexpr int kWalkAction = 15;
constexpr int kFlyAction = 34;
constexpr int kRideRunAction = 36;
constexpr int kAttackAction = 38;

void PlaceReferenceClip(std::vector<std::uint16_t>& keys, std::vector<std::uint8_t>& locks,
                        std::vector<std::int16_t>& angles, const std::uint16_t* seasonKeys,
                        const std::uint8_t* seasonLocks, int loaded, int action)
{
    keys[static_cast<std::size_t>(loaded)] = seasonKeys[action];
    locks[static_cast<std::size_t>(loaded)] = seasonLocks[action];
    CopyPose(angles, loaded, action);
}

void FillInsertedClip(std::vector<std::uint16_t>& keys, std::vector<std::uint8_t>& locks, int loaded)
{
    keys[static_cast<std::size_t>(loaded)] = static_cast<std::uint16_t>(kInsertedClipKeys);
    locks[static_cast<std::size_t>(loaded)] = 0;
}

TEST_CASE("clips inserted before male idle shift walk, wing, mount, and attack")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kPrefix = 17;
    const int loadedCount = kActions + kPrefix;
    std::vector<std::uint16_t> seasonKeys(static_cast<std::size_t>(kActions));
    std::vector<std::uint8_t> seasonLocks(static_cast<std::size_t>(kActions));
    Render::Models::CopySeason6ActionKeys(seasonKeys.data(), kActions);
    Render::Models::CopySeason6ActionLocks(seasonLocks.data(), kActions);

    std::vector<std::uint16_t> keys(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::int16_t> angles(static_cast<std::size_t>(loadedCount) * kAngles, 0);
    std::vector<float> path(static_cast<std::size_t>(loadedCount), 50.f);
    std::vector<float> height(static_cast<std::size_t>(loadedCount), 160.f);
    for (int inserted = 0; inserted < kPrefix; ++inserted)
    {
        FillInsertedClip(keys, locks, inserted);
    }
    for (int action = 0; action < kActions; ++action)
    {
        const int loaded = action + kPrefix;
        PlaceReferenceClip(keys, locks, angles, seasonKeys.data(), seasonLocks.data(), loaded, action);
        path[static_cast<std::size_t>(loaded)] = 0.f;
        height[static_cast<std::size_t>(loaded)] = 106.9f;
    }

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), angles.data(), path.data(),
                                                     height.data(), loadedCount, season6ToLoaded.data()));
    CHECK(season6ToLoaded[static_cast<std::size_t>(kIdleAction)] == kIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdleAction)] == kWingIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideIdleAction)] == kRideIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWalkAction)] == kWalkAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kFlyAction)] == kFlyAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideRunAction)] == kRideRunAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kAttackAction)] == kAttackAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6DefenseAction)] ==
          Render::Models::kSeason6DefenseAction + kPrefix);
}

TEST_CASE("a prefix and the riding skills shift later clips further than walk")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kPrefix = 17;
    constexpr int kRiding = Render::Models::kSeason21RidingSkillCount;
    constexpr int kDefense = Render::Models::kSeason6DefenseAction;
    const int loadedCount = kActions + kPrefix + kRiding;
    std::vector<std::uint16_t> seasonKeys(static_cast<std::size_t>(kActions));
    std::vector<std::uint8_t> seasonLocks(static_cast<std::size_t>(kActions));
    Render::Models::CopySeason6ActionKeys(seasonKeys.data(), kActions);
    Render::Models::CopySeason6ActionLocks(seasonLocks.data(), kActions);

    std::vector<std::uint16_t> keys(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::int16_t> angles(static_cast<std::size_t>(loadedCount) * kAngles, 0);
    int loaded = 0;
    for (int inserted = 0; inserted < kPrefix; ++inserted, ++loaded)
    {
        FillInsertedClip(keys, locks, loaded);
    }
    for (int action = 0; action < kActions; ++action)
    {
        if (action == kDefense)
        {
            for (int inserted = 0; inserted < kRiding; ++inserted, ++loaded)
            {
                FillInsertedClip(keys, locks, loaded);
            }
        }
        PlaceReferenceClip(keys, locks, angles, seasonKeys.data(), seasonLocks.data(), loaded, action);
        ++loaded;
    }

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), angles.data(), nullptr, nullptr,
                                                     loadedCount, season6ToLoaded.data()));
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWalkAction)] == kWalkAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdleAction)] == kWingIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideRunAction)] == kRideRunAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kAttackAction)] == kAttackAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kDefense)] == kDefense + kPrefix + kRiding);
    CHECK(season6ToLoaded[static_cast<std::size_t>(Render::Models::kSeason6RageIdleAction)] ==
          Render::Models::kSeason6RageIdleAction + kPrefix + kRiding);
}

TEST_CASE("extra clips after the Season 6 table leave walk and mount in place")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kTail = 40;
    const int loadedCount = kActions + kTail;
    std::vector<std::uint16_t> keys(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::int16_t> angles(static_cast<std::size_t>(loadedCount) * kAngles, 0);
    Render::Models::CopySeason6ActionKeys(keys.data(), kActions);
    Render::Models::CopySeason6ActionLocks(locks.data(), kActions);
    for (int action = 0; action < kActions; ++action)
    {
        CopyPose(angles, action, action);
    }
    for (int loaded = kActions; loaded < loadedCount; ++loaded)
    {
        FillInsertedClip(keys, locks, loaded);
    }

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), angles.data(), nullptr, nullptr,
                                                     loadedCount, season6ToLoaded.data()));
    CHECK(season6ToLoaded[static_cast<std::size_t>(kIdleAction)] == kIdleAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdleAction)] == kWingIdleAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWalkAction)] == kWalkAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kFlyAction)] == kFlyAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideRunAction)] == kRideRunAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kAttackAction)] == kAttackAction);
}

TEST_CASE("key counts alone move male idle off a leading charge clip")
{
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    constexpr int kPrefix = 17;
    const int loadedCount = kActions + kPrefix;
    std::vector<std::uint16_t> keys(static_cast<std::size_t>(loadedCount), 0);
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(loadedCount), 0);
    Render::Models::CopySeason6ActionKeys(keys.data() + kPrefix, kActions);
    Render::Models::CopySeason6ActionLocks(locks.data() + kPrefix, kActions);
    for (int inserted = 0; inserted < kPrefix; ++inserted)
    {
        FillInsertedClip(keys, locks, inserted);
    }

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), nullptr, nullptr, nullptr, loadedCount,
                                                     season6ToLoaded.data()));
    CHECK(season6ToLoaded[static_cast<std::size_t>(kIdleAction)] == kIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWalkAction)] == kWalkAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdleAction)] == kWingIdleAction + kPrefix);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideRunAction)] == kRideRunAction + kPrefix);
}

TEST_CASE("swapping the walk and mount poses keeps their Season 6 indices")
{
    constexpr int kAngles = Render::Models::kSeason6IdlePoseAngles;
    constexpr int kActions = Render::Models::kSeason6PlayerActionCount;
    std::vector<std::uint16_t> keys(static_cast<std::size_t>(kActions));
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(kActions));
    std::vector<std::int16_t> angles(static_cast<std::size_t>(kActions) * kAngles, 0);
    Render::Models::CopySeason6ActionKeys(keys.data(), kActions);
    Render::Models::CopySeason6ActionLocks(locks.data(), kActions);
    for (int action = 0; action < kActions; ++action)
    {
        CopyPose(angles, action, action);
    }
    std::swap_ranges(angles.begin() + static_cast<std::size_t>(kWalkAction) * kAngles,
                     angles.begin() + static_cast<std::size_t>(kWalkAction + 1) * kAngles,
                     angles.begin() + static_cast<std::size_t>(kRideRunAction) * kAngles);

    std::vector<int> season6ToLoaded(static_cast<std::size_t>(kActions), -1);
    REQUIRE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), angles.data(), nullptr, nullptr,
                                                     kActions, season6ToLoaded.data()));
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWalkAction)] == kWalkAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kRideRunAction)] == kRideRunAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kWingIdleAction)] == kWingIdleAction);
    CHECK(season6ToLoaded[static_cast<std::size_t>(kAttackAction)] == kAttackAction);
}

TEST_CASE("a player file longer than the aligner scans is left unmapped")
{
    const int loadedCount = Render::Models::kMaxSeason21PlayerActions + 1;
    std::vector<std::uint16_t> keys(static_cast<std::size_t>(loadedCount), 1);
    std::vector<std::uint8_t> locks(static_cast<std::size_t>(loadedCount), 0);
    std::vector<int> season6ToLoaded(static_cast<std::size_t>(Render::Models::kSeason6PlayerActionCount), -1);
    CHECK_FALSE(Render::Models::MapSeason6PlayerClipOrder(keys.data(), locks.data(), nullptr, nullptr, nullptr,
                                                         loadedCount, season6ToLoaded.data()));
}
