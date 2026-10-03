#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::Textures
{

// Season 6 OZJ stores the JPEG 24 bytes in. Season 21 files keep the same
// marker but not always at that offset, and some wrap the container with
// ModulusCryptor the way OZD does. The returned bytes start at the JPEG SOI.
bool ReadOzjJpeg(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg);

} // namespace Render::Textures
