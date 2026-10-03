#include "Render/Terrain/TerrainContainer.h"

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"
#include "Render/Models/MapFileCrypt.h"
#include "Render/Terrain/ModulusCryptor.h"

#include <array>
#include <cstring>
#include <string>

namespace Render::Terrain
{
namespace
{

constexpr int kMagicBytes = 4;
constexpr int kAttributeEdge = 255;
constexpr int kObjectVersionCount = 6;
constexpr std::array<int, kObjectVersionCount> kObjectStrides = {30, 32, 33, 45, 46, 54};

static_assert(kObjectRecordBytes == 2 + (sizeof(float) * 7));
static_assert(kObjectStrides[0] == kObjectRecordBytes);
static_assert(kMapPayloadBytes == 196610);
static_assert(kAttributeWordPayloadBytes == 131076);

bool IsMagic(const std::uint8_t* bytes, std::size_t size, char a, char b, char c)
{
    return bytes != nullptr && size >= static_cast<std::size_t>(kMagicBytes) && bytes[0] == static_cast<std::uint8_t>(a) &&
           bytes[1] == static_cast<std::uint8_t>(b) && bytes[2] == static_cast<std::uint8_t>(c) && bytes[3] == 1;
}

bool DecryptSeason21(const std::uint8_t* bytes, std::size_t size, std::vector<std::uint8_t>& plain, std::string& error)
{
    if (size < static_cast<std::size_t>(kMagicBytes + kModulusHeaderBytes))
    {
        error = "Season 21 terrain container is too small";
        return false;
    }
    if (!DecryptModulus(bytes + kMagicBytes, size - static_cast<std::size_t>(kMagicBytes), plain))
    {
        error = "Season 21 terrain container did not decrypt";
        return false;
    }
    return true;
}

std::vector<std::uint8_t> DecryptMap(const std::uint8_t* payload, std::size_t size)
{
    std::vector<std::uint8_t> plain(size);
    if (size == 0 || payload == nullptr)
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

bool LayerIsTileGrid(const std::uint8_t* layer, std::size_t count)
{
    int seen[256] = {};
    int distinct = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (seen[layer[i]]++ == 0)
        {
            ++distinct;
        }
    }
    return distinct > 0 && distinct <= kMaxMapTileIds;
}

void CopyMapLayers(const std::vector<std::uint8_t>& plain, TerrainMapDocument& document)
{
    document.mapNumber = plain[1];
    document.layer1.assign(plain.begin() + 2, plain.begin() + 2 + kTerrainCells);
    document.layer2.assign(plain.begin() + 2 + kTerrainCells, plain.begin() + 2 + (kTerrainCells * 2));
    document.alpha.assign(plain.begin() + 2 + (kTerrainCells * 2), plain.end());
}

bool FillAttributes(std::vector<std::uint8_t>& plain, bool wordGrid, TerrainAttributeDocument& document)
{
    BuxConvert(reinterpret_cast<BYTE*>(plain.data()), static_cast<int>(plain.size()));
    document.version = plain[0];
    document.mapNumber = plain[1];
    document.width = plain[2];
    document.height = plain[3];
    document.wideAttributes = wordGrid;
    if (document.season21 &&
        (document.version != 0 || document.width != kAttributeEdge || document.height != kAttributeEdge))
    {
        document.error = "Season 21 terrain attributes are not a 255x255 grid";
        document.mapNumber = -1;
        return false;
    }

    document.walls.resize(static_cast<std::size_t>(kTerrainCells));
    if (!wordGrid)
    {
        for (int i = 0; i < kTerrainCells; ++i)
        {
            document.walls[static_cast<std::size_t>(i)] = plain[static_cast<std::size_t>(4 + i)];
        }
        document.ok = true;
        return true;
    }
    for (int i = 0; i < kTerrainCells; ++i)
    {
        const std::size_t offset = static_cast<std::size_t>(4 + (i * 2));
        document.walls[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(plain[offset] | (plain[offset + 1] << 8));
    }
    document.ok = true;
    return true;
}

int ObjectStride(int version)
{
    if (version < 0 || version >= kObjectVersionCount)
    {
        return 0;
    }
    return kObjectStrides[static_cast<std::size_t>(version)];
}

bool ReadObjects(const std::vector<std::uint8_t>& plain, TerrainObjectDocument& document)
{
    if (plain.size() < 4)
    {
        return false;
    }
    const int stride = ObjectStride(plain[0]);
    const int count = ReadI16(plain.data() + 2);
    if (stride == 0 || count < 0)
    {
        return false;
    }
    const std::size_t expected = 4 + static_cast<std::size_t>(count) * static_cast<std::size_t>(stride);
    if (plain.size() != expected)
    {
        return false;
    }

    document.mapNumber = plain[1];
    document.objects.clear();
    document.objects.reserve(static_cast<std::size_t>(count));
    std::size_t offset = 4;
    for (int i = 0; i < count; ++i)
    {
        TerrainObjectRecord object;
        object.type = ReadI16(plain.data() + offset);
        std::memcpy(object.position, plain.data() + offset + 2, sizeof(object.position));
        std::memcpy(object.angle, plain.data() + offset + 2 + sizeof(object.position), sizeof(object.angle));
        std::memcpy(&object.scale, plain.data() + offset + 2 + sizeof(object.position) + sizeof(object.angle),
                    sizeof(object.scale));
        document.objects.push_back(object);
        offset += static_cast<std::size_t>(stride);
    }
    document.ok = true;
    return true;
}

void RejectObjects(TerrainObjectDocument& document)
{
    document.ok = false;
    document.objects.clear();
    document.mapNumber = -1;
    document.error = "terrain object count does not match the file";
}

} // namespace

TerrainMapDocument DecodeTerrainMap(const std::uint8_t* bytes, std::size_t size)
{
    TerrainMapDocument document;
    if (bytes == nullptr && size != 0)
    {
        document.error = "missing terrain bytes";
        return document;
    }

    std::vector<std::uint8_t> plain;
    if (IsMagic(bytes, size, 'M', 'A', 'P'))
    {
        document.season21 = true;
        if (!DecryptSeason21(bytes, size, plain, document.error))
        {
            return document;
        }
    }
    else
    {
        if (size != static_cast<std::size_t>(kMapPayloadBytes))
        {
            document.error = "terrain map payload is " + std::to_string(size) + " bytes";
            return document;
        }
        plain = DecryptMap(bytes, size);
    }

    if (plain.size() != static_cast<std::size_t>(kMapPayloadBytes))
    {
        document.error = "terrain map payload is " + std::to_string(plain.size()) + " bytes";
        return document;
    }
    if (document.season21 && !LayerIsTileGrid(plain.data() + 2, static_cast<std::size_t>(kTerrainCells)))
    {
        document.error = "Season 21 terrain map is not a tile grid";
        return document;
    }

    CopyMapLayers(plain, document);
    document.ok = true;
    return document;
}

TerrainAttributeDocument DecodeTerrainAttribute(const std::uint8_t* bytes, std::size_t size)
{
    TerrainAttributeDocument document;
    if (bytes == nullptr && size != 0)
    {
        document.error = "missing terrain bytes";
        return document;
    }

    std::vector<std::uint8_t> plain;
    if (IsMagic(bytes, size, 'A', 'T', 'T'))
    {
        document.season21 = true;
        if (!DecryptSeason21(bytes, size, plain, document.error))
        {
            return document;
        }
    }
    else
    {
        plain = DecryptMap(bytes, size);
    }

    const bool byteGrid = plain.size() == static_cast<std::size_t>(kAttributeBytePayloadBytes);
    const bool wordGrid = plain.size() == static_cast<std::size_t>(kAttributeWordPayloadBytes);
    if (!byteGrid && !wordGrid)
    {
        document.error = "terrain attribute payload is " + std::to_string(plain.size()) + " bytes";
        return document;
    }
    if (!FillAttributes(plain, wordGrid, document))
    {
        document.walls.clear();
    }
    return document;
}

TerrainObjectDocument DecodeTerrainObjects(const std::uint8_t* bytes, std::size_t size)
{
    TerrainObjectDocument document;
    if (bytes == nullptr && size != 0)
    {
        document.error = "missing terrain bytes";
        return document;
    }
    if (size < 4)
    {
        document.error = "terrain object payload is too small";
        return document;
    }

    if (IsMagic(bytes, size, 'O', 'B', 'J'))
    {
        document.season21 = true;
        std::vector<std::uint8_t> plain;
        if (!DecryptSeason21(bytes, size, plain, document.error) || !ReadObjects(plain, document))
        {
            RejectObjects(document);
        }
        return document;
    }

    if (ReadObjects(DecryptMap(bytes, size), document))
    {
        return document;
    }

    std::vector<std::uint8_t> plain;
    if (DecryptModulus(bytes, size, plain) && ReadObjects(plain, document))
    {
        document.season21 = true;
        return document;
    }

    RejectObjects(document);
    return document;
}

} // namespace Render::Terrain
