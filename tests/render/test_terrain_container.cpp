#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Render/Models/MapFileCrypt.h"
#include "Render/Terrain/ModulusCryptor.h"
#include "Render/Terrain/TerrainContainer.h"
#include "Render/Terrain/WorldIdMap.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Render::Terrain;

namespace
{

constexpr std::uint8_t kMasterKey[32] = {
    'w', 'e', 'b', 'z', 'e', 'n', '#', '@', '!', '0', '1', 'w', 'e', 'b', 'z', 'e',
    'n', '#', '@', '!', '0', '1', 'w', 'e', 'b', 'z', 'e', 'n', '#', '@', '!', '0'};

std::uint32_t LoadBe(const std::uint8_t* bytes)
{
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) | (std::uint32_t(bytes[2]) << 8) | bytes[3];
}

void StoreBe(std::uint8_t* bytes, std::uint32_t value)
{
    bytes[0] = static_cast<std::uint8_t>(value >> 24);
    bytes[1] = static_cast<std::uint8_t>(value >> 16);
    bytes[2] = static_cast<std::uint8_t>(value >> 8);
    bytes[3] = static_cast<std::uint8_t>(value);
}

void EncryptTeaBlock(std::uint8_t* block, const std::uint8_t* key)
{
    std::uint32_t v0 = LoadBe(block);
    std::uint32_t v1 = LoadBe(block + 4);
    const std::uint32_t k0 = LoadBe(key);
    const std::uint32_t k1 = LoadBe(key + 4);
    const std::uint32_t k2 = LoadBe(key + 8);
    const std::uint32_t k3 = LoadBe(key + 12);
    constexpr std::uint32_t kDelta = 0x9E3779B9u;
    std::uint32_t sum = 0;
    for (int round = 0; round < 32; ++round)
    {
        sum += kDelta;
        v0 += ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        v1 += ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
    }
    StoreBe(block, v0);
    StoreBe(block + 4, v1);
}

void EncryptTeaSpan(std::uint8_t* data, std::size_t offset, std::size_t length, const std::uint8_t* key)
{
    for (std::size_t i = 0; i + 8 <= length; i += 8)
    {
        EncryptTeaBlock(data + offset + i, key);
    }
}

std::vector<std::uint8_t> EncryptModulusTea(const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(kModulusHeaderBytes) + payload.size(), 0);
    std::memcpy(buffer.data() + 2, kMasterKey, sizeof(kMasterKey));
    std::memcpy(buffer.data() + kModulusHeaderBytes, payload.data(), payload.size());
    const std::size_t dataSize = payload.size();
    constexpr std::size_t kChunk = 1024;
    const std::size_t payloadChunk = dataSize - (dataSize % 8);
    EncryptTeaSpan(buffer.data(), static_cast<std::size_t>(kModulusHeaderBytes), payloadChunk, kMasterKey);
    if (dataSize > kChunk)
    {
        EncryptTeaSpan(buffer.data(), 2, kChunk, kMasterKey);
        EncryptTeaSpan(buffer.data(), buffer.size() - kChunk, kChunk, kMasterKey);
    }
    if (dataSize > (4 * kChunk))
    {
        EncryptTeaSpan(buffer.data(), 2 + (dataSize >> 1), kChunk, kMasterKey);
    }
    return buffer;
}

std::vector<std::uint8_t> WithMagic(const char* magic, const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> file;
    file.push_back(static_cast<std::uint8_t>(magic[0]));
    file.push_back(static_cast<std::uint8_t>(magic[1]));
    file.push_back(static_cast<std::uint8_t>(magic[2]));
    file.push_back(1);
    file.insert(file.end(), payload.begin(), payload.end());
    return file;
}

std::vector<std::uint8_t> EncryptSeason6(const std::vector<std::uint8_t>& plain)
{
    std::vector<std::uint8_t> encrypted(plain.size());
    Render::Models::EncryptMapFile(encrypted.data(), plain.data(), static_cast<int>(plain.size()));
    return encrypted;
}

std::vector<std::uint8_t> Season6Map(int mapNumber, std::uint8_t layerValue)
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kMapPayloadBytes), 0);
    plain[1] = static_cast<std::uint8_t>(mapNumber);
    plain[2] = layerValue;
    return EncryptSeason6(plain);
}

std::vector<std::uint8_t> TileMapPlain(int mapNumber, std::uint8_t layerValue)
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kMapPayloadBytes), 0);
    plain[1] = static_cast<std::uint8_t>(mapNumber);
    plain[2] = layerValue;
    plain[static_cast<std::size_t>(2 + kTerrainCells)] = 3;
    return plain;
}

std::vector<std::uint8_t> FromHex(const char* hex)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; hex[i] != 0 && hex[i + 1] != 0; i += 2)
    {
        const auto value = std::stoul(std::string(hex + i, 2), nullptr, 16);
        bytes.push_back(static_cast<std::uint8_t>(value));
    }
    return bytes;
}

std::string ToHex(const std::uint8_t* bytes, std::size_t size)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string hex(size * 2, '0');
    for (std::size_t i = 0; i < size; ++i)
    {
        hex[(i * 2)] = kDigits[bytes[i] >> 4];
        hex[(i * 2) + 1] = kDigits[bytes[i] & 0x0F];
    }
    return hex;
}

} // namespace

TEST_CASE("Season 6 terrain map decrypts the fixed grid")
{
    const std::vector<std::uint8_t> file = Season6Map(34, 9);
    REQUIRE(file.size() == 196610);

    const TerrainMapDocument document = DecodeTerrainMap(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK_FALSE(document.season21);
    CHECK(document.mapNumber == 34);
    CHECK(document.layer1.front() == 9);
}

TEST_CASE("modulus decrypt matches the Season 21 cipher vectors")
{
    const std::vector<std::uint8_t> small = FromHex(
        "0207212e3b4855626f7c8996a3b0bdcad7e4f1fe0b1825323f4c596673808d9aa7b4c1cedbe8f5020f1c293643505d6a7784919eabb8c5d2dfecf90613202d3a4754616e7b8895a2afbcc9d6e3f0fd0a1724");
    std::vector<std::uint8_t> plain;
    REQUIRE(DecryptModulus(small.data(), small.size(), plain));
    CHECK(ToHex(plain.data(), plain.size()) ==
          "25dce1ed717dcde73c50cd484164cf7781f83d1580bbbf9a4ed828fa78e91be7c65f676949562c710a063996a000e072");

    std::vector<std::uint8_t> big(34 + 1040);
    for (std::size_t i = 0; i < big.size(); ++i)
    {
        big[i] = static_cast<std::uint8_t>((i * 19 + 3) & 255);
    }
    big[0] = 0;
    big[1] = 0;
    REQUIRE(DecryptModulus(big.data(), big.size(), plain));
    CHECK(ToHex(plain.data(), 32) == "e6d5cb9e6e0d3665a560081180f04929fca5c9ff0afd6557d8c4b3711fd6ee34");
    CHECK(ToHex(plain.data() + 500, 16) == "c35272b26a8047152666e3fe47d90616");
    CHECK(ToHex(plain.data() + plain.size() - 16, 16) == "e6d5cb9e6e0d3665a560081180f04929");
}

TEST_CASE("Season 21 MAP magic decrypts to the tile grid")
{
    const std::vector<std::uint8_t> file = WithMagic("MAP", EncryptModulusTea(TileMapPlain(34, 9)));
    const TerrainMapDocument document = DecodeTerrainMap(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.mapNumber == 34);
    CHECK(document.layer1.front() == 9);
    CHECK(document.layer2.front() == 3);
}

TEST_CASE("a Season 21 map with a flat tile histogram is not installed")
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kMapPayloadBytes), 0);
    plain[1] = 34;
    for (int i = 0; i < kTerrainCells; ++i)
    {
        plain[static_cast<std::size_t>(2 + i)] = static_cast<std::uint8_t>(i & 255);
    }
    const std::vector<std::uint8_t> file = WithMagic("MAP", EncryptModulusTea(plain));
    const TerrainMapDocument document = DecodeTerrainMap(file.data(), file.size());
    CHECK_FALSE(document.ok);
    CHECK(document.season21);
    CHECK(document.layer1.empty());
    CHECK(document.error.find("not a tile grid") != std::string::npos);
}

TEST_CASE("Season 21 ATT magic decrypts to walls")
{
    std::vector<std::uint8_t> plain(static_cast<std::size_t>(kAttributeWordPayloadBytes), 0);
    plain[0] = 0;
    plain[1] = 34;
    plain[2] = 255;
    plain[3] = 255;
    plain[4] = 5;
    BuxConvert(reinterpret_cast<BYTE*>(plain.data()), static_cast<int>(plain.size()));
    const std::vector<std::uint8_t> file = WithMagic("ATT", EncryptModulusTea(plain));
    const TerrainAttributeDocument document = DecodeTerrainAttribute(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.mapNumber == 34);
    CHECK(document.width == 255);
    CHECK(document.height == 255);
    CHECK(document.walls.front() == 5);
}

TEST_CASE("an object file whose count does not fill the buffer is rejected")
{
    std::vector<std::uint8_t> plain(4 + static_cast<std::size_t>(kObjectRecordBytes), 0);
    plain[1] = 34;
    plain[2] = 5;
    plain[3] = 0;
    const std::vector<std::uint8_t> file = EncryptSeason6(plain);
    const TerrainObjectDocument document = DecodeTerrainObjects(file.data(), file.size());
    CHECK_FALSE(document.ok);
    CHECK(document.objects.empty());
    CHECK(document.mapNumber == -1);
}

TEST_CASE("a Season 6 object file with an exact record count loads")
{
    std::vector<std::uint8_t> plain(4 + static_cast<std::size_t>(kObjectRecordBytes), 0);
    plain[1] = 34;
    plain[2] = 1;
    plain[4] = 74;
    const float scale = 1.5f;
    std::memcpy(plain.data() + 4 + 2 + (sizeof(float) * 6), &scale, sizeof(scale));
    const std::vector<std::uint8_t> file = EncryptSeason6(plain);
    const TerrainObjectDocument document = DecodeTerrainObjects(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK_FALSE(document.season21);
    CHECK(document.mapNumber == 34);
    REQUIRE(document.objects.size() == 1);
    CHECK(document.objects[0].type == 74);
    CHECK(document.objects[0].scale == doctest::Approx(1.5f));
}

TEST_CASE("Season 21 OBJ magic loads the records inside the modulus blob")
{
    std::vector<std::uint8_t> plain(4 + static_cast<std::size_t>(kObjectRecordBytes), 0);
    plain[1] = 34;
    plain[2] = 1;
    plain[4] = 12;
    const std::vector<std::uint8_t> file = WithMagic("OBJ", EncryptModulusTea(plain));
    const TerrainObjectDocument document = DecodeTerrainObjects(file.data(), file.size());
    REQUIRE(document.ok);
    CHECK(document.season21);
    CHECK(document.mapNumber == 34);
    REQUIRE(document.objects.size() == 1);
    CHECK(document.objects[0].type == 12);
}

TEST_CASE("a Season 21 id is accepted when it is the opened world")
{
    const TerrainWorldResolution mapMatch = ResolveTerrainWorld(true, TerrainIdKind::Map, 34, 34);
    CHECK(mapMatch.accepted);
    CHECK(mapMatch.world == 34);

    const TerrainWorldResolution mapNoise = ResolveTerrainWorld(true, TerrainIdKind::Map, 167, 34);
    CHECK_FALSE(mapNoise.accepted);
    CHECK(mapNoise.world == 167);

    const TerrainWorldResolution attributeNoise = ResolveTerrainWorld(true, TerrainIdKind::Attribute, 58, 34);
    CHECK_FALSE(attributeNoise.accepted);
    CHECK(attributeNoise.world == 58);

    const TerrainWorldResolution season6Match = ResolveTerrainWorld(false, TerrainIdKind::Map, 34, 34);
    CHECK(season6Match.accepted);
    CHECK(season6Match.world == 34);

    const TerrainWorldResolution season6Mismatch = ResolveTerrainWorld(false, TerrainIdKind::Map, 33, 34);
    CHECK_FALSE(season6Mismatch.accepted);
    CHECK(season6Mismatch.world == 33);
}

TEST_CASE("a short Season 21 magic fails before a grid is installed")
{
    const std::uint8_t bytes[] = {'M', 'A', 'P', 1, 1, 2, 3, 4};
    const TerrainMapDocument document = DecodeTerrainMap(bytes, sizeof(bytes));
    CHECK_FALSE(document.ok);
    CHECK(document.season21);
    CHECK(document.layer1.empty());
}
