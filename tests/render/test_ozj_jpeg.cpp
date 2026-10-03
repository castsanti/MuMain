#include "doctest.h"

#include "Render/Textures/OzjJpeg.h"

#include <cstdint>
#include <vector>

TEST_CASE("OZJ JPEG follows a header that is not 24 bytes")
{
    std::vector<std::uint8_t> file(40, 0);
    file[20] = 0xFF;
    file[21] = 0xD8;
    file[22] = 0xFF;
    file[23] = 0xE0;
    file[24] = 0x11;

    std::vector<std::uint8_t> jpeg;
    REQUIRE(Render::Textures::ReadOzjJpeg(file.data(), file.size(), jpeg));
    CHECK(jpeg.size() == 20);
    CHECK(jpeg[0] == 0xFF);
    CHECK(jpeg[1] == 0xD8);
    CHECK(jpeg[4] == 0x11);
}

TEST_CASE("OZJ JPEG at the Season 6 offset is kept")
{
    std::vector<std::uint8_t> file(30, 0xAB);
    file[24] = 0xFF;
    file[25] = 0xD8;
    file[26] = 0xFF;
    file[27] = 0xE0;

    std::vector<std::uint8_t> jpeg;
    REQUIRE(Render::Textures::ReadOzjJpeg(file.data(), file.size(), jpeg));
    CHECK(jpeg.size() == 6);
    CHECK(jpeg[0] == 0xFF);
}

TEST_CASE("an OZJ without a JPEG marker is rejected")
{
    std::vector<std::uint8_t> file(40, 0x11);
    std::vector<std::uint8_t> jpeg;
    CHECK_FALSE(Render::Textures::ReadOzjJpeg(file.data(), file.size(), jpeg));
}
