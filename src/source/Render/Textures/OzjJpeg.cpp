#include "Render/Textures/OzjJpeg.h"

#include "Render/Terrain/ModulusCryptor.h"

namespace Render::Textures
{
namespace
{

constexpr std::size_t kClassicHeaderBytes = 24;
constexpr std::size_t kScanStart = 16;
constexpr std::uint8_t kJpegPrefix = 0xFF;
constexpr std::uint8_t kJpegSoi = 0xD8;

bool IsJpegMarker(std::uint8_t marker)
{
    return marker == 0xC0 || marker == 0xC2 || marker == 0xC4 || marker == 0xDB || marker == 0xDD || marker == 0xE0 ||
           marker == 0xE1 || marker == 0xE2 || marker == 0xFE;
}

bool StartsWithJpegSoi(const std::uint8_t* data, std::size_t size, std::size_t offset)
{
    return offset + 3 <= size && data[offset] == kJpegPrefix && data[offset + 1] == kJpegSoi &&
           data[offset + 2] == kJpegPrefix;
}

bool IsJpegStart(const std::uint8_t* data, std::size_t size, std::size_t offset)
{
    return StartsWithJpegSoi(data, size, offset) && offset + 4 <= size && IsJpegMarker(data[offset + 3]);
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

bool CopyFrom(const std::uint8_t* data, std::size_t size, std::size_t offset, std::vector<std::uint8_t>& jpeg)
{
    if (offset >= size)
    {
        return false;
    }
    jpeg.assign(data + offset, data + size);
    return true;
}

bool ExtractJpeg(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& jpeg)
{
    if (StartsWithJpegSoi(data, size, kClassicHeaderBytes))
    {
        return CopyFrom(data, size, kClassicHeaderBytes, jpeg);
    }
    return CopyFrom(data, size, FindJpegStart(data, size), jpeg);
}

} // namespace

bool ReadOzjJpeg(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg)
{
    jpeg.clear();
    if (file == nullptr || size <= kClassicHeaderBytes)
    {
        return false;
    }
    if (ExtractJpeg(file, size, jpeg))
    {
        return true;
    }

    std::vector<std::uint8_t> plain;
    if (!Render::Terrain::DecryptModulus(file, size, plain))
    {
        return false;
    }
    return ExtractJpeg(plain.data(), plain.size(), jpeg);
}

bool ReadOzjJpegUnwrapped(const std::uint8_t* file, std::size_t size, std::vector<std::uint8_t>& jpeg)
{
    jpeg.clear();
    if (file == nullptr || size <= kClassicHeaderBytes)
    {
        return false;
    }
    std::vector<std::uint8_t> plain;
    if (!Render::Terrain::DecryptModulus(file, size, plain))
    {
        return false;
    }
    return ExtractJpeg(plain.data(), plain.size(), jpeg);
}

} // namespace Render::Textures
