#include "Render/Terrain/WorldIdMap.h"

namespace Render::Terrain
{
namespace
{

struct Season21TerrainWorld
{
    int clientWorld;
    int mapId;
    int attributeId;
};

// Embedded payload ids read from Season 21 EncTerrain files.
// Checked-in Data/World*/EncTerrain*.map files are Season 6: the decrypted id
// byte already equals the WorldN index, so they are not rows in this table.
// World34's Season 21 overlay (play log) is map id 167 and attribute id 58.
constexpr Season21TerrainWorld kSeason21TerrainWorlds[] = {
    {34, 167, 58},
};

constexpr int kSeason21TerrainWorldCount =
    static_cast<int>(sizeof(kSeason21TerrainWorlds) / sizeof(kSeason21TerrainWorlds[0]));

constexpr bool Season21TerrainIdsAreUnique()
{
    for (int i = 0; i < kSeason21TerrainWorldCount; ++i)
    {
        for (int j = i + 1; j < kSeason21TerrainWorldCount; ++j)
        {
            const Season21TerrainWorld& left = kSeason21TerrainWorlds[i];
            const Season21TerrainWorld& right = kSeason21TerrainWorlds[j];
            if (left.clientWorld == right.clientWorld || left.mapId == right.mapId
                || left.attributeId == right.attributeId)
            {
                return false;
            }
        }
    }
    return true;
}

static_assert(Season21TerrainIdsAreUnique());
static_assert(kSeason21TerrainWorlds[0].clientWorld == 34);
static_assert(kSeason21TerrainWorlds[0].mapId == 167);
static_assert(kSeason21TerrainWorlds[0].attributeId == 58);

int IdForKind(const Season21TerrainWorld& entry, TerrainIdKind kind)
{
    if (kind == TerrainIdKind::Map)
    {
        return entry.mapId;
    }
    return entry.attributeId;
}

} // namespace

int ClientWorldForSeason21Id(TerrainIdKind kind, int embeddedId)
{
    for (const Season21TerrainWorld& entry : kSeason21TerrainWorlds)
    {
        if (IdForKind(entry, kind) == embeddedId)
        {
            return entry.clientWorld;
        }
    }
    return -1;
}

TerrainWorldResolution ResolveTerrainWorld(bool season21, TerrainIdKind kind, int embeddedId, int expectedWorld)
{
    if (!season21)
    {
        const bool accepted = expectedWorld < 0 || embeddedId == expectedWorld;
        return TerrainWorldResolution{accepted, embeddedId};
    }

    const int mapped = ClientWorldForSeason21Id(kind, embeddedId);
    if (mapped >= 0 && (expectedWorld < 0 || mapped == expectedWorld))
    {
        return TerrainWorldResolution{true, mapped};
    }

    const int reported = embeddedId == expectedWorld ? kTerrainWorldRejected : embeddedId;
    return TerrainWorldResolution{false, reported};
}

} // namespace Render::Terrain
