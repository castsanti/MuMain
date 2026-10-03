#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// 3D model container read by BMD::Open2 (world objects, item models, players).
//
// Season 6 (version 12) and Season 21 (version 15) carry the same payload.
// Season 6 encrypts it with the map-file cipher. Season 21 encrypts it with
// LEA-256-ECB and pads the payload out to a 16-byte block. Version 10 stores
// that payload with no cipher. Version 14 is recognized and not decrypted.
//
// After the 32-byte model name and the mesh, bone, and action counts, each
// mesh is vertices (16 bytes), normals (20), texcoords (8), triangles (64),
// then a 32-byte texture name. Those strides are the Season 6 in-memory layout.
namespace Render::Models
{

constexpr unsigned char kBmdVersionPlain = 0x0A;
constexpr unsigned char kBmdVersionSeason6 = 0x0C;
constexpr unsigned char kBmdVersionSeason8 = 0x0E;
constexpr unsigned char kBmdVersionSeason21 = 0x0F;

constexpr std::size_t kBmdNameBytes = 32;
constexpr std::size_t kBmdVertexStride = 16;
constexpr std::size_t kBmdNormalStride = 20;
constexpr std::size_t kBmdTexCoordStride = 8;
constexpr std::size_t kBmdTriangleStride = 64;
constexpr std::size_t kBmdVec3Stride = 12;
constexpr std::size_t kLeaBlockBytes = 16;
constexpr std::size_t kLea256KeyBytes = 32;

enum class BmdContainerStatus
{
    Ok,
    BadHeader,
    UnknownVersion,
    Unsupported,
    BadPayloadSize,
    OutOfMemory,
};

struct BmdPlainPayload
{
    unsigned char version = 0;
    std::vector<std::uint8_t> bytes;
};

BmdContainerStatus ReadBmdPlainPayload(const std::uint8_t* file, std::size_t fileSize, BmdPlainPayload& out);

// Writes a container the reader accepts. Season 21 output is padded with zeros
// to the LEA block size; those bytes sit after the model and are not fields.
bool WriteBmdContainer(unsigned char version, const std::uint8_t* plain, std::size_t plainSize,
                       std::vector<std::uint8_t>& file);

bool DecryptLea256Ecb(const std::uint8_t* key, std::uint8_t* dst, const std::uint8_t* src, std::size_t size);
bool EncryptLea256Ecb(const std::uint8_t* key, std::uint8_t* dst, const std::uint8_t* src, std::size_t size);

} // namespace Render::Models
