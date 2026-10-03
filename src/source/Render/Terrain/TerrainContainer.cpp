#include "Render/Terrain/TerrainContainer.h"

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Render/Models/MapFileCrypt.h"

#include <cstring>
#include <string>

namespace Render::Terrain
{
namespace
{

constexpr int kMaxSeason21HeaderBytes = 64;

static_assert(kObjectRecordBytes == 2 + (sizeof(float) * 7));
static_assert(kMapPayloadBytes == 196610);
static_assert(kAttributeWordPayloadBytes == 131076);

bool IsSeason21Magic(const std::uint8_t* bytes, std::size_t size)
{
    if (bytes == nullptr || size < 4 || bytes[3] != 1)
    {
        return false;
    }

    const bool map = bytes[0] == 'M' && bytes[1] == 'A' && bytes[2] == 'P';
    const bool attribute = bytes[0] == 'A' && bytes[1] == 'T' && bytes[2] == 'T';
    const bool object = bytes[0] == 'O' && bytes[1] == 'B' && bytes[2] == 'J';
    return map || attribute || object;
}

std::size_t Season21HeaderSize(const std::uint8_t* bytes, std::size_t size, TerrainFileKind kind)
{
    if (!IsSeason21Magic(bytes, size))
    {
        return 0;
    }

    if (kind == TerrainFileKind::Map && size > static_cast<std::size_t>(kMapPayloadBytes))
    {
        const std::size_t header = size - static_cast<std::size_t>(kMapPayloadBytes);
        if (header >= 4 && header <= static_cast<std::size_t>(kMaxSeason21HeaderBytes))
        {
            return header;
        }
    }

    if (kind == TerrainFileKind::Attribute)
    {
        if (size > static_cast<std::size_t>(kAttributeWordPayloadBytes))
        {
            const std::size_t header = size - static_cast<std::size_t>(kAttributeWordPayloadBytes);
            if (header >= 4 && header <= static_cast<std::size_t>(kMaxSeason21HeaderBytes))
            {
                return header;
            }
        }
        if (size > static_cast<std::size_t>(kAttributeBytePayloadBytes))
        {
            const std::size_t header = size - static_cast<std::size_t>(kAttributeBytePayloadBytes);
            if (header >= 4 && header <= static_cast<std::size_t>(kMaxSeason21HeaderBytes))
            {
                return header;
            }
        }
    }

    if (kind == TerrainFileKind::Object && size > static_cast<std::size_t>(kSeason21HeaderBytes))
    {
        return static_cast<std::size_t>(kSeason21HeaderBytes);
    }

    return 0;
}

bool PayloadBytes(const std::uint8_t* bytes, std::size_t size, TerrainFileKind kind, const std::uint8_t*& payload,
                  std::size_t& payloadSize, bool& season21, std::string& error)
{
    payload = bytes;
    payloadSize = size;
    season21 = false;
    if (bytes == nullptr && size != 0)
    {
        error = "missing terrain bytes";
        return false;
    }

    if (!IsSeason21Magic(bytes, size))
    {
        return true;
    }

    const std::size_t header = Season21HeaderSize(bytes, size, kind);
    if (header == 0 || header >= size)
    {
        error = "unrecognized Season 21 terrain header";
        return false;
    }

    payload = bytes + header;
    payloadSize = size - header;
    season21 = true;
    return true;
}

std::vector<std::uint8_t> DecryptMap(const std::uint8_t* payload, std::size_t size)
{
    std::vector<std::uint8_t> plain(size);
    if (size == 0)
    {
        return plain;
    }
    Render::Models::DecryptMapFile(plain.data(), payload, static_cast<int>(size));
    return plain;
}

std::int16_t ReadI16(const std::uint8_t* bytes)
{
    return static_cast<std::int16_t>(bytes[0] | (bytes[1] << 8));
}

} // namespace

TerrainMapDocument DecodeTerrainMap(const std::uint8_t* bytes, std::size_t size)
{
    TerrainMapDocument document;
    const std::uint8_t* payload = nullptr;
    std::size_t payloadSize = 0;
    if (!PayloadBytes(bytes, size, TerrainFileKind::Map, payload, payloadSize, document.season21, document.error))
    {
        return document;
    }
    if (payloadSize != static_cast<std::size_t>(kMapPayloadBytes))
    {
        document.error = "terrain map payload is " + std::to_string(payloadSize) + " bytes";
        return document;
    }

    const std::vector<std::uint8_t> plain = DecryptMap(payload, payloadSize);
    document.mapNumber = plain[1];
    document.layer1.assign(plain.begin() + 2, plain.begin() + 2 + kTerrainCells);
    document.layer2.assign(plain.begin() + 2 + kTerrainCells, plain.begin() + 2 + (kTerrainCells * 2));
    document.alpha.assign(plain.begin() + 2 + (kTerrainCells * 2), plain.end());
    document.ok = true;
    return document;
}

TerrainAttributeDocument DecodeTerrainAttribute(const std::uint8_t* bytes, std::size_t size)
{
    TerrainAttributeDocument document;
    const std::uint8_t* payload = nullptr;
    std::size_t payloadSize = 0;
    if (!PayloadBytes(bytes, size, TerrainFileKind::Attribute, payload, payloadSize, document.season21, document.error))
    {
        return document;
    }

    const bool byteGrid = payloadSize == static_cast<std::size_t>(kAttributeBytePayloadBytes);
    const bool wordGrid = payloadSize == static_cast<std::size_t>(kAttributeWordPayloadBytes);
    if (!byteGrid && !wordGrid)
    {
        document.error = "terrain attribute payload is " + std::to_string(payloadSize) + " bytes";
        return document;
    }

    std::vector<std::uint8_t> plain = DecryptMap(payload, payloadSize);
    BuxConvert(reinterpret_cast<BYTE*>(plain.data()), static_cast<int>(plain.size()));

    document.version = plain[0];
    document.mapNumber = plain[1];
    document.width = plain[2];
    document.height = plain[3];
    document.wideAttributes = wordGrid;
    document.walls.resize(static_cast<std::size_t>(kTerrainCells));
    if (byteGrid)
    {
        for (int i = 0; i < kTerrainCells; ++i)
        {
            document.walls[static_cast<std::size_t>(i)] = plain[static_cast<std::size_t>(4 + i)];
        }
    }
    else
    {
        for (int i = 0; i < kTerrainCells; ++i)
        {
            const std::size_t offset = static_cast<std::size_t>(4 + (i * 2));
            document.walls[static_cast<std::size_t>(i)] =
                static_cast<std::uint16_t>(plain[offset] | (plain[offset + 1] << 8));
        }
    }
    document.ok = true;
    return document;
}

TerrainObjectDocument DecodeTerrainObjects(const std::uint8_t* bytes, std::size_t size)
{
    TerrainObjectDocument document;
    const std::uint8_t* payload = nullptr;
    std::size_t payloadSize = 0;
    if (!PayloadBytes(bytes, size, TerrainFileKind::Object, payload, payloadSize, document.season21, document.error))
    {
        return document;
    }
    if (payloadSize < 4)
    {
        document.error = "terrain object payload is too small";
        return document;
    }

    const std::vector<std::uint8_t> plain = DecryptMap(payload, payloadSize);
    document.mapNumber = plain[1];
    const int count = ReadI16(plain.data() + 2);
    if (count < 0)
    {
        document.error = "terrain object count is negative";
        return document;
    }

    std::size_t offset = 4;
    document.objects.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        if (plain.size() - offset < static_cast<std::size_t>(kObjectRecordBytes))
        {
            document.error = "terrain object " + std::to_string(i) + " extends past end of file";
            document.ok = !document.objects.empty();
            return document;
        }

        TerrainObjectRecord object;
        object.type = ReadI16(plain.data() + offset);
        offset += 2;
        std::memcpy(object.position, plain.data() + offset, sizeof(object.position));
        offset += sizeof(object.position);
        std::memcpy(object.angle, plain.data() + offset, sizeof(object.angle));
        offset += sizeof(object.angle);
        std::memcpy(&object.scale, plain.data() + offset, sizeof(object.scale));
        offset += sizeof(object.scale);
        document.objects.push_back(object);
    }

    document.ok = true;
    return document;
}

int AcceptedTerrainWorld(bool season21, int embeddedMapNumber, int clientWorld)
{
    if (!season21 || clientWorld == kUnspecifiedClientWorld)
    {
        return embeddedMapNumber;
    }
    return clientWorld;
}

} // namespace Render::Terrain
