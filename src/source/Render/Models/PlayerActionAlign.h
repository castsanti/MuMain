#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::Models
{

// Season 6 player.bmd action count. Season 21 appends and inserts clips, so the
// Season 6 action index no longer addresses the same clip.
constexpr int kSeason6PlayerActionCount = 284;

std::uint32_t HashPlayerMotion(std::uint32_t crc, const void* bytes, std::size_t size);

void CopySeason6PlayerMotion(std::uint32_t* destination, int count);

// Writes the loaded action index for each Season 6 action. Returns false when
// the loaded motion is not the Season 6 sequence with extra clips inserted.
bool MapSeason6PlayerActions(const std::uint32_t* loadedMotion, int loadedCount, int* season6ToLoaded);

}
