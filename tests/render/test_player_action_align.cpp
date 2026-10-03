#include "doctest.h"

#include "Render/Models/PlayerActionAlign.h"

#include <cstdint>
#include <cstring>
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
