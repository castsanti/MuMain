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

// Ids that are not the client world index. 167 and 58 are not listed: those bytes
// came from the Season 6 map cipher on a ModulusCryptor payload and are not worlds.
constexpr int kSeason21TerrainWorldCount = 0;
constexpr Season21TerrainWorld kSeason21TerrainWorlds[1] = {};

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
    for (int i = 0; i < kSeason21TerrainWorldCount; ++i)
    {
        const Season21TerrainWorld& entry = kSeason21TerrainWorlds[i];
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
    if (mapped >= 0)
    {
        if (expectedWorld < 0 || mapped == expectedWorld)
        {
            return TerrainWorldResolution{true, mapped};
        }
        const int reported = embeddedId == expectedWorld ? kTerrainWorldRejected : embeddedId;
        return TerrainWorldResolution{false, reported};
    }

    if (expectedWorld < 0 || embeddedId == expectedWorld)
    {
        return TerrainWorldResolution{true, embeddedId};
    }
    return TerrainWorldResolution{false, embeddedId};
}

} // namespace Render::Terrain
