#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Render/Models/MapFileCrypt.h"
#include "Render/Terrain/TerrainContainer.h"

#include <cstdint>
#include <cstring>
#include <vector>

using namespace Render::Terrain;

namespace
{

std::vector<std::uint8_t> Encrypt(const std::vector<std::uint8_t>& plain)
{
    std::vector<std::uint8_t> encrypted(plain.size());
    Render::Models::EncryptMapFile(encrypted.data(), plain.data(), static_cast<int>(plain.size()));
    return encrypted;
}

std::vector<std::uint8_t> WithSeason21Header(const char* magic, const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> file(static_cast<std::size_t>(kSeason21HeaderBytes), 0);
    file[0] = static_cast<std::uint8_t>(magic[0]);
    file[1] = static_cast<std::uint8_t>(magic[1]);
    file[2] = static_cast<std::uint8_t>(magic[2]);
    file[3] = 1;
    file.insert(file.end(), payload.begin(), payload.end());
    return file;
}

std::vector<std::uint8_t> MapCipher(int mapNumber, std::uint8_t layerValue)
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kMapPayloadBytes), 0);
    plain[1] = static_cast<std::uint8_t>(mapNumber);
    plain[2] = layerValue;
    return Encrypt(plain);
}

std::vector<std::uint8_t> AttributeCipher(int mapNumber, std::uint16_t wall)
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kAttributeWordPayloadBytes), 0);
    plain[0] = 0;
    plain[1] = static_cast<std::uint8_t>(mapNumber);
    plain[2] = 255;
    plain[3] = 255;
    plain[4] = static_cast<std::uint8_t>(wall & 0xFF);
    plain[5] = static_cast<std::uint8_t>((wall >> 8) & 0xFF);
    BuxConvert(reinterpret_cast<BYTE*>(plain.data()), static_cast<int>(plain.size()));
    return Encrypt(plain);
}

} // namespace

TEST_CASE("Season 6 terrain map decrypts the fixed grid")
{
    const std::vector<std::uint8_t> file = MapCipher(34, 9);
    REQUIRE(file.size() == 196610);

    const TerrainMapDocument document = DecodeTerrainMap(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK_FALSE(document.season21);
    CHECK(document.mapNumber == 34);
    CHECK(document.layer1.front() == 9);
}

TEST_CASE("Season 21 MAP header is stripped before the Season 6 cipher")
{
    const std::vector<std::uint8_t> file = WithSeason21Header("MAP", MapCipher(34, 9));
    REQUIRE(file.size() == 196648);

    const TerrainMapDocument document = DecodeTerrainMap(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.mapNumber == 34);
    CHECK(document.layer1.front() == 9);
    CHECK(document.layer2.size() == static_cast<std::size_t>(kTerrainCells));
    CHECK(document.alpha.size() == static_cast<std::size_t>(kTerrainCells));
}

TEST_CASE("Season 21 ATT header keeps wide attribute cells")
{
    const std::vector<std::uint8_t> file = WithSeason21Header("ATT", AttributeCipher(34, 5));
    REQUIRE(file.size() == 131114);

    const TerrainAttributeDocument document = DecodeTerrainAttribute(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.wideAttributes);
    CHECK(document.mapNumber == 34);
    CHECK(document.width == 255);
    CHECK(document.height == 255);
    CHECK(document.walls.front() == 5);
}

TEST_CASE("Season 21 object list stops at the end of the buffer")
{
    std::vector<std::uint8_t> plain(4 + static_cast<std::size_t>(kObjectRecordBytes), 0);
    plain[1] = 34;
    plain[2] = 5;
    plain[3] = 0;
    plain[4] = 74;
    plain[5] = 0;
    const float scale = 1.5f;
    std::memcpy(plain.data() + 4 + 2 + (sizeof(float) * 6), &scale, sizeof(scale));

    const std::vector<std::uint8_t> file = WithSeason21Header("OBJ", Encrypt(plain));
    const TerrainObjectDocument document = DecodeTerrainObjects(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.mapNumber == 34);
    REQUIRE(document.objects.size() == 1);
    CHECK(document.objects[0].type == 74);
    CHECK(document.objects[0].scale == doctest::Approx(1.5f));
    CHECK(document.error.find("extends past") != std::string::npos);
}

TEST_CASE("a Season 21 magic with the wrong payload size fails soft")
{
    const std::uint8_t bytes[] = {'M', 'A', 'P', 1, 1, 2, 3, 4};
    const TerrainMapDocument document = DecodeTerrainMap(bytes, sizeof(bytes));
    CHECK_FALSE(document.ok);
    CHECK(document.error.find("Season 21") != std::string::npos);
}
