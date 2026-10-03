#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Render::Terrain
{

constexpr int kTerrainGrid = 256;
constexpr int kTerrainCells = kTerrainGrid * kTerrainGrid;

// Season 6 EncTerrain.map ciphertext is the plain grid: version, map number, two
// layers, and one alpha byte per cell. EncTerrain.att is that grid as bytes or
// words plus a 4-byte header. Season 21 writes the same ciphertext after a short
// plaintext header whose first bytes are "MAP\x01", "ATT\x01", or "OBJ\x01".
// World34's files are 38 bytes longer than those Season 6 blobs.
constexpr int kMapPayloadBytes = 2 + (kTerrainCells * 3);
constexpr int kAttributeBytePayloadBytes = 4 + kTerrainCells;
constexpr int kAttributeWordPayloadBytes = 4 + (kTerrainCells * 2);
constexpr int kSeason21HeaderBytes = 38;
constexpr int kObjectRecordBytes = 30;

enum class TerrainFileKind
{
    Map,
    Attribute,
    Object,
};

struct TerrainObjectRecord
{
    std::int16_t type = 0;
    float position[3] = {};
    float angle[3] = {};
    float scale = 0.f;
};

struct TerrainMapDocument
{
    bool ok = false;
    bool season21 = false;
    int mapNumber = -1;
    std::vector<std::uint8_t> layer1;
    std::vector<std::uint8_t> layer2;
    std::vector<std::uint8_t> alpha;
    std::string error;
};

struct TerrainAttributeDocument
{
    bool ok = false;
    bool season21 = false;
    bool wideAttributes = false;
    int version = 0;
    int mapNumber = -1;
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> walls;
    std::string error;
};

struct TerrainObjectDocument
{
    bool ok = false;
    bool season21 = false;
    int mapNumber = -1;
    std::vector<TerrainObjectRecord> objects;
    std::string error;
};

TerrainMapDocument DecodeTerrainMap(const std::uint8_t* bytes, std::size_t size);
TerrainAttributeDocument DecodeTerrainAttribute(const std::uint8_t* bytes, std::size_t size);
TerrainObjectDocument DecodeTerrainObjects(const std::uint8_t* bytes, std::size_t size);

// Season 21 EncTerrain stores an id that is not the client world folder index
// (World34 shipped map id 167 and attribute id 58). A successful decode belongs
// to the client world that requested the file. kUnspecifiedClientWorld keeps
// the embedded id for callers that do not know the world.
constexpr int kUnspecifiedClientWorld = -1;

int AcceptedTerrainWorld(bool season21, int embeddedMapNumber, int clientWorld);

}
