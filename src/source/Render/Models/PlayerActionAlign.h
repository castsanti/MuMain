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
constexpr float kPlayerCoarsePoseStep = 0.5f;
constexpr int kPlayerPoseSamples = 2;

// Unarmed idles used on character select, plus the Rage Fighter idle.
// A re-saved player.bmd can fail the full-sequence match and still be fixed
// by placing these clips.
constexpr int kSeason6IdleActionCount = 4;
constexpr int kSeason6IdlePoseAngles = kSeason6PlayerBoneCount * kPlayerPoseSamples * 3;
// Path length of the root bone. Male idle is 0.07. Female and summoner sway
// stays under 5. A walk covers about 245, including a loop that ends where
// it started.
constexpr float kStandingPathLimit = 5.f;
// Largest single root step of those idles. A walk step is about 50.
constexpr float kStandingStepLimit = 1.f;

std::uint32_t HashPlayerMotion(std::uint32_t crc, const void* bytes, std::size_t size);

// Nearest step, halfway away from zero. Shared by the embedded table and the
// loader so a re-saved clip still lands on the same pose.
std::int16_t QuantizePlayerAngle(float radians);

std::int16_t QuantizePlayerAngleAt(float radians, float step);

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

void CopySeason6PlayerCoarsePose(std::uint32_t* destination, int count);

// The 0.5-radian pose, without the key count. A re-export that changes the
// key count or nudges a bone still matches.
bool MapSeason6PlayerCoarsePoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded);

// Assigns a Season 6 action only when its coarse pose occurs once in the
// reference and once in the file. Unmatched entries are left at -1.
// Returns how many actions were assigned.
int MapSeason6PlayerUniquePoses(const std::uint32_t* loadedPose, int loadedCount, int* season6ToLoaded);

int Season6IdleActionIndex(int idleSlot);

void CopySeason6IdlePose(int idleSlot, std::int16_t* destination, int count);

int PlayerPoseDistance(const std::int16_t* left, const std::int16_t* right, int count);

// Puts a standing clip on idle actions 1, 2, 3, and 283. pathLength is the
// sum of root steps and maxStep is the longest one. A clip already assigned
// there is kept when it stands. A walk is swapped for the standing clip
// whose pose is nearest that Season 6 idle. loadedAngles is loadedCount
// poses of kSeason6IdlePoseAngles. Returns how many idle slots changed.
int PlaceSeason6IdleClips(const std::int16_t* loadedAngles, const float* pathLength, const float* maxStep,
                          int loadedCount, int* season6ToLoaded);

// Fills -1 entries. A free Season 6 index keeps the clip already stored there.
void CompleteSeason6ActionMap(int* season6ToLoaded, int loadedCount);

// Root height above the standing idle. Ride and wing clips sit above this.
constexpr float kAirAboveGround = 12.f;
// Path length at or under this is a still pose. A hover is longer.
constexpr float kMotionStillPath = 8.f;
// Season 6 male idle root height, used when no still clip matches that pose.
constexpr float kReferenceGroundHeight = 106.9f;

// Ground or air, and still or moving. A foot walk cannot take a ride clip,
// and a wing idle cannot take a ground stand.
enum class PlayerMotionClass : std::uint8_t
{
    GroundStill = 0,
    GroundMove = 1,
    AirStill = 2,
    AirMove = 3,
};

std::uint8_t Season6MotionClass(int action);

void CopySeason6MotionPose(int action, std::int16_t* destination, int count);

// Writes the loaded clip for each Season 6 action. Clips are matched inside
// the same motion class by pose, then leftover actions take the nearest
// unused clip. Returns how many actions received a clip of their own class,
// or -1 when the inputs cannot be mapped.
int MapSeason6PlayerMotions(const std::int16_t* loadedAngles, const float* pathLength, const float* meanHeight,
                            int loadedCount, int* season6ToLoaded);

}
