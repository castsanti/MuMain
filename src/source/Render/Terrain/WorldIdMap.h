#pragma once

namespace Render::Terrain
{

enum class TerrainIdKind
{
    Map,
    Attribute,
};

// A Season 21 id was not allowed for the world being opened, and the embedded
// byte itself equals that world. This is not a missing file (-1).
constexpr int kTerrainWorldRejected = -2;

struct TerrainWorldResolution
{
    bool accepted = false;
    int world = -1;
};

// After a Season 21 decrypt, the embedded id must be the client world index or a
// row in WorldIdMap.cpp. Season 6 ids are the world index and are not listed.
int ClientWorldForSeason21Id(TerrainIdKind kind, int embeddedId);

TerrainWorldResolution ResolveTerrainWorld(bool season21, TerrainIdKind kind, int embeddedId, int expectedWorld);

}
