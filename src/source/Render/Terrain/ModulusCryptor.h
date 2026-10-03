#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::Terrain
{

// Season 21 MAP\x01 and ATT\x01 files are 4 magic bytes followed by this
// container. The plaintext is the bytes after the 34-byte modulus header.
constexpr int kModulusHeaderBytes = 34;

bool DecryptModulus(const std::uint8_t* source, std::size_t size, std::vector<std::uint8_t>& plain);

}
