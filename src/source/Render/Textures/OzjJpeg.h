#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::Textures
{

// Season 6 and Interface OZJ files store a JPEG after a 24-byte header.
// That offset is used whenever it starts with a JPEG SOI. A marker scan and
// a ModulusCryptor unwrap are only used when byte 24 is not a JPEG.
// The returned bytes start at the JPEG SOI.
bool ReadOzjJpeg(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg);

// Decrypts the whole file with ModulusCryptor, then reads the JPEG. Used when
// the bytes at the classic offset are not a JPEG TurboJPEG can read.
bool ReadOzjJpegUnwrapped(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg);

} // namespace Render::Textures
