#include "Render/Textures/OzjJpeg.h"

#include "Render/Terrain/ModulusCryptor.h"

namespace Render::Textures
{
namespace
{

constexpr std::size_t kClassicHeaderBytes = 24;
constexpr std::size_t kScanStart = 16;

bool IsJpegMarker(std::uint8_t marker)
{
    return marker == 0xC0 || marker == 0xC2 || marker == 0xC4 || marker == 0xDB || marker == 0xDD || marker == 0xE0 ||
           marker == 0xE1 || marker == 0xE2 || marker == 0xFE;
}

bool IsJpegStart(const std::uint8_t* data, std::size_t size, std::size_t offset)
{
    return offset + 4 <= size && data[offset] == 0xFF && data[offset + 1] == 0xD8 && data[offset + 2] == 0xFF &&
           IsJpegMarker(data[offset + 3]);
}

std::size_t FindJpegStart(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 3)
    {
        return size;
    }
    if (IsJpegStart(data, size, 0))
    {
        return 0;
    }
    const std::size_t start = size > kScanStart ? kScanStart : 0;
    for (std::size_t offset = start; offset + 2 < size; ++offset)
    {
        if (IsJpegStart(data, size, offset))
        {
            return offset;
        }
    }
    return size;
}

bool CopyJpeg(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& jpeg)
{
    const std::size_t offset = FindJpegStart(data, size);
    if (offset >= size)
    {
        return false;
    }
    jpeg.assign(data + offset, data + size);
    return true;
}

} // namespace

bool ReadOzjJpeg(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg)
{
    jpeg.clear();
    if (file == nullptr || size <= kClassicHeaderBytes)
    {
        return false;
    }
    if (CopyJpeg(file, size, jpeg))
    {
        return true;
    }

    std::vector<std::uint8_t> plain;
    if (!Render::Terrain::DecryptModulus(file, size, plain))
    {
        return false;
    }
    return CopyJpeg(plain.data(), plain.size(), jpeg);
}

} // namespace Render::Textures
