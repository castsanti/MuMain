#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::Models
{

// Season 6 player.bmd action count. Season 21 appends and inserts clips, so the
// Season 6 action index no longer addresses the same clip.
constexpr int kSeason6PlayerActionCount = 284;

// Non-dummy bones of the Season 6 player, in file order. A Season 21 player
// keeps these names and appends bones; pose matching ignores the extras.
constexpr int kSeason6PlayerBoneCount = 50;

// First and last key of each of those bones, in radians, divided by this.
constexpr float kPlayerPoseStep = 0.1f;
constexpr int kPlayerPoseSamples = 2;

std::uint32_t HashPlayerMotion(std::uint32_t crc, const void* bytes, std::size_t size);

// Nearest step, halfway away from zero. Shared by the embedded table and the
// loader so a re-saved clip still lands on the same pose.
std::int16_t QuantizePlayerAngle(float radians);

// keys, lock, then kSeason6PlayerBoneCount * kPlayerPoseSamples * 3 angles
// (first key, then last key). A missing bone is 0x7FFF per component.
std::uint32_t HashPlayerPose(std::uint16_t keys, std::uint8_t lock, const std::int16_t* angles, int angleCount);

void CopySeason6PlayerMotion(std::uint32_t* destination, int count);

void CopySeason6PlayerPose(std::uint32_t* destination, int count);

const char* Season6PlayerBoneName(int index);

// Writes the loaded action index for each Season 6 action. Returns false when
// the loaded sequence is not the Season 6 sequence with extra clips inserted.
bool MapSeason6PlayerActions(const std::uint32_t* loadedMotion, int loadedCount, int* season6ToLoaded);

// Same match on the quantized pose of each clip, for a player.bmd whose bone
// bytes were re-saved and no longer share the Season 6 checksum.
bool MapSeason6PlayerPoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded);

}
