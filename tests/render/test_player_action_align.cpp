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
