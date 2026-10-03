#include "doctest.h"

#include "Render/Models/BmdContainer.h"
#include "Render/Models/MapFileCrypt.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Render::Models;

namespace
{

std::vector<std::uint8_t> FromHex(const char* hex)
{
    std::vector<std::uint8_t> bytes;
    auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        return -1;
    };

    for (std::size_t i = 0; hex[i] != '\0' && hex[i + 1] != '\0'; i += 2)
    {
        const int high = nibble(hex[i]);
        const int low = nibble(hex[i + 1]);
        REQUIRE(high >= 0);
        REQUIRE(low >= 0);
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

void AppendBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

void AppendI16(std::vector<std::uint8_t>& out, std::int16_t value)
{
    AppendBytes(out, &value, sizeof(value));
}

void AppendF32(std::vector<std::uint8_t>& out, float value)
{
    AppendBytes(out, &value, sizeof(value));
}

void AppendName(std::vector<std::uint8_t>& out, const char* name)
{
    std::uint8_t field[kBmdNameBytes] = {};
    const std::size_t length = std::strlen(name);
    std::memcpy(field, name, length < kBmdNameBytes ? length : kBmdNameBytes - 1);
    AppendBytes(out, field, kBmdNameBytes);
}

// One static mesh, one bone, one locked-off action. The byte count is not a
// multiple of the LEA block, so a Season 21 container has to pad it.
std::vector<std::uint8_t> Object74Payload()
{
    std::vector<std::uint8_t> payload;
    AppendName(payload, "Object74.smd");
    AppendI16(payload, 1);
    AppendI16(payload, 1);
    AppendI16(payload, 1);

    AppendI16(payload, 1);
    AppendI16(payload, 1);
    AppendI16(payload, 1);
    AppendI16(payload, 1);
    AppendI16(payload, 0);

    AppendI16(payload, 0);
    AppendI16(payload, 0);
    AppendF32(payload, 1.f);
    AppendF32(payload, 2.f);
    AppendF32(payload, 3.f);

    AppendI16(payload, 0);
    AppendI16(payload, 0);
    AppendF32(payload, 0.f);
    AppendF32(payload, 1.f);
    AppendF32(payload, 0.f);
    AppendI16(payload, 0);
    AppendI16(payload, 0);

    AppendF32(payload, 0.5f);
    AppendF32(payload, 0.25f);

    std::vector<std::uint8_t> triangle(kBmdTriangleStride, 0);
    triangle[0] = 3;
    AppendBytes(payload, triangle.data(), triangle.size());
    AppendName(payload, "Object74.jpg");

    AppendI16(payload, 1);
    payload.push_back(0);

    payload.push_back(0);
    AppendName(payload, "Root");
    AppendI16(payload, -1);
    AppendF32(payload, 4.f);
    AppendF32(payload, 5.f);
    AppendF32(payload, 6.f);
    AppendF32(payload, 0.f);
    AppendF32(payload, 0.f);
    AppendF32(payload, 0.f);
    return payload;
}

std::int16_t ReadI16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    std::int16_t value = 0;
    REQUIRE(offset + sizeof(value) <= bytes.size());
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float ReadF32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    float value = 0.f;
    REQUIRE(offset + sizeof(value) <= bytes.size());
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::string ReadName(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    REQUIRE(offset + kBmdNameBytes <= bytes.size());
    const char* text = reinterpret_cast<const char*>(bytes.data() + offset);
    std::size_t length = 0;
    while (length < kBmdNameBytes && text[length] != '\0')
    {
        ++length;
    }
    return std::string(text, length);
}

void CheckObject74(const std::vector<std::uint8_t>& payload)
{
    CHECK(ReadName(payload, 0) == "Object74.smd");
    CHECK(ReadI16(payload, kBmdNameBytes) == 1);
    CHECK(ReadI16(payload, kBmdNameBytes + 2) == 1);
    CHECK(ReadI16(payload, kBmdNameBytes + 4) == 1);

    const std::size_t mesh = kBmdNameBytes + 6;
    CHECK(ReadI16(payload, mesh) == 1);
    CHECK(ReadI16(payload, mesh + 2) == 1);
    CHECK(ReadI16(payload, mesh + 4) == 1);
    CHECK(ReadI16(payload, mesh + 6) == 1);

    const std::size_t vertex = mesh + 10;
    CHECK(ReadF32(payload, vertex + 4) == doctest::Approx(1.f));
    CHECK(ReadF32(payload, vertex + 8) == doctest::Approx(2.f));
    CHECK(ReadF32(payload, vertex + 12) == doctest::Approx(3.f));

    const std::size_t textureName =
        vertex + kBmdVertexStride + kBmdNormalStride + kBmdTexCoordStride + kBmdTriangleStride;
    CHECK(ReadName(payload, textureName) == "Object74.jpg");
    CHECK(ReadI16(payload, textureName + kBmdNameBytes) == 1);
    CHECK(payload[textureName + kBmdNameBytes + sizeof(std::int16_t)] == 0);
}

const std::uint8_t kPublishedSeason21Key[kLea256KeyBytes] = {
    0xCC, 0x50, 0x45, 0x13, 0xC2, 0xA6, 0x57, 0x4E, 0xD6, 0x9A, 0x45, 0x89, 0xBF, 0x2F, 0xBC, 0xD9,
    0x39, 0xB3, 0xB3, 0xBD, 0x50, 0xBD, 0xCC, 0xB6, 0x85, 0x46, 0xD1, 0xD6, 0x16, 0x54, 0xE0, 0x87};

} // namespace

TEST_CASE("LEA-256 ECB matches the reference vectors [models][bmd]")
{
    const auto keyBytes = FromHex("4F6779E2BD1E9319C63015ACFFEFD7A791F0ED59DF1B700769FE82E2F0668C35");
    REQUIRE(keyBytes.size() == kLea256KeyBytes);

    const auto plain = FromHex("DC31CAE3DA5E0A11C966B020D7CFFEDE");
    const auto cipher = FromHex("EDA2042098F667E857A02DB8CAA7DFF2");
    std::vector<std::uint8_t> decrypted(plain.size());
    std::vector<std::uint8_t> encrypted(plain.size());
    REQUIRE(DecryptLea256Ecb(keyBytes.data(), decrypted.data(), cipher.data(), cipher.size()));
    REQUIRE(EncryptLea256Ecb(keyBytes.data(), encrypted.data(), plain.data(), plain.size()));
    CHECK(decrypted == plain);
    CHECK(encrypted == cipher);

    const auto plainBlocks = FromHex("66D127137801A9970F0C5472232169778CC13649AFD1DD125CEE5677F700B7CB");
    const auto cipherBlocks = FromHex("16BFF149DFA234BF7FBE2C59AE88A1E99A1BF8D91910A7F67D088432E1C6D790");
    const auto keyBlocks = FromHex("E7FE92FD374D30C43F5DC204DCAE9D4EAD6C0663BD8CF5EC6318196B67C71B72");
    decrypted.assign(plainBlocks.size(), 0);
    REQUIRE(DecryptLea256Ecb(keyBlocks.data(), decrypted.data(), cipherBlocks.data(), cipherBlocks.size()));
    CHECK(decrypted == plainBlocks);
}

TEST_CASE("Season 6 and Season 21 containers open the same object payload [models][bmd]")
{
    const std::vector<std::uint8_t> payload = Object74Payload();
    REQUIRE(payload.size() % kLeaBlockBytes != 0);

    std::vector<std::uint8_t> season6;
    std::vector<std::uint8_t> season21;
    REQUIRE(WriteBmdContainer(kBmdVersionSeason6, payload.data(), payload.size(), season6));
    REQUIRE(WriteBmdContainer(kBmdVersionSeason21, payload.data(), payload.size(), season21));
    CHECK(season6[3] == kBmdVersionSeason6);
    CHECK(season21[3] == kBmdVersionSeason21);
    CHECK(season6 != season21);

    BmdPlainPayload opened6;
    BmdPlainPayload opened21;
    CHECK(ReadBmdPlainPayload(season6.data(), season6.size(), opened6) == BmdContainerStatus::Ok);
    CHECK(ReadBmdPlainPayload(season21.data(), season21.size(), opened21) == BmdContainerStatus::Ok);
    CHECK(opened6.version == kBmdVersionSeason6);
    CHECK(opened21.version == kBmdVersionSeason21);
    CHECK(opened6.bytes == payload);

    const std::size_t padded = payload.size() + (kLeaBlockBytes - payload.size() % kLeaBlockBytes);
    REQUIRE(opened21.bytes.size() == padded);
    CHECK(std::equal(payload.begin(), payload.end(), opened21.bytes.begin()));
    CHECK(std::all_of(opened21.bytes.begin() + static_cast<std::ptrdiff_t>(payload.size()), opened21.bytes.end(),
                      [](std::uint8_t byte) { return byte == 0; }));

    CheckObject74(opened6.bytes);
    CheckObject74(opened21.bytes);

    std::vector<std::uint8_t> paddedPlain(padded, 0);
    std::memcpy(paddedPlain.data(), payload.data(), payload.size());
    std::vector<std::uint8_t> expectedCipher(padded);
    REQUIRE(EncryptLea256Ecb(kPublishedSeason21Key, expectedCipher.data(), paddedPlain.data(), padded));
    CHECK(std::equal(expectedCipher.begin(), expectedCipher.end(), season21.begin() + 8));
}

TEST_CASE("Plain version 10 containers keep the payload after the header [models][bmd]")
{
    const std::vector<std::uint8_t> payload = Object74Payload();
    std::vector<std::uint8_t> file;
    REQUIRE(WriteBmdContainer(kBmdVersionPlain, payload.data(), payload.size(), file));

    BmdPlainPayload opened;
    CHECK(ReadBmdPlainPayload(file.data(), file.size(), opened) == BmdContainerStatus::Ok);
    CHECK(opened.version == kBmdVersionPlain);
    CHECK(opened.bytes == payload);
}

TEST_CASE("Map-file cipher round-trips and rejects a short Season 6 container [models][bmd]")
{
    const std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5, 9, 8, 7};
    std::vector<std::uint8_t> encrypted(payload.size());
    std::vector<std::uint8_t> decrypted(payload.size());
    EncryptMapFile(encrypted.data(), payload.data(), static_cast<int>(payload.size()));
    DecryptMapFile(decrypted.data(), encrypted.data(), static_cast<int>(encrypted.size()));
    CHECK(decrypted == payload);
    CHECK(encrypted == FromHex("8EBEAC1B8F4F5F84"));
    CHECK(EncryptMapFile(nullptr, payload.data(), static_cast<int>(payload.size())) == static_cast<int>(payload.size()));

    std::vector<std::uint8_t> truncated = {'B', 'M', 'D', kBmdVersionSeason6, 8, 0, 0, 0};
    BmdPlainPayload opened;
    CHECK(ReadBmdPlainPayload(truncated.data(), truncated.size(), opened) == BmdContainerStatus::BadPayloadSize);

    const std::uint8_t unknown[] = {'B', 'M', 'D', 0x0B, 0, 0, 0, 0};
    CHECK(ReadBmdPlainPayload(unknown, sizeof(unknown), opened) == BmdContainerStatus::UnknownVersion);
    CHECK(opened.version == 0x0B);

    const std::uint8_t season8[] = {'B', 'M', 'D', kBmdVersionSeason8, 1, 2, 3, 4};
    CHECK(ReadBmdPlainPayload(season8, sizeof(season8), opened) == BmdContainerStatus::Unsupported);
    CHECK(opened.version == kBmdVersionSeason8);
    CHECK(opened.bytes.empty());

    const std::uint8_t badMagic[] = {'X', 'M', 'D', kBmdVersionSeason6};
    CHECK(ReadBmdPlainPayload(badMagic, sizeof(badMagic), opened) == BmdContainerStatus::BadHeader);

    std::vector<std::uint8_t> unaligned(8 + 20, 0);
    unaligned[0] = 'B';
    unaligned[1] = 'M';
    unaligned[2] = 'D';
    unaligned[3] = kBmdVersionSeason21;
    const std::int32_t size = 20;
    std::memcpy(unaligned.data() + 4, &size, sizeof(size));
    CHECK(ReadBmdPlainPayload(unaligned.data(), unaligned.size(), opened) == BmdContainerStatus::BadPayloadSize);
}
