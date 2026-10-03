#include "Data/GameData/ItemData/LocalItemTable.h"

#include "Core/Platform/WinCompat.h"
#include "Data/GameData/ItemData/ItemType.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace Data::Items
{
namespace
{

constexpr int kCountBytes = 4;
constexpr int kChecksumBytes = 4;
constexpr int kGroupOffset = 4;
constexpr int kIdOffset = 6;
constexpr int kFolderOffset = 8;
constexpr int kFolderBytes = 260;
constexpr int kModelOffset = kFolderOffset + kFolderBytes;
constexpr int kModelBytes = 260;
constexpr int kWidthOffset = 602;
constexpr int kHeightOffset = 603;
constexpr int kMinStride = kHeightOffset + 1;
constexpr int kMaxStride = 2048;
constexpr int kMaxItems = 20000;
constexpr int kGroupSpan = 256;
constexpr int kMaxInventorySpan = 8;
constexpr std::uint8_t kXorKey[3] = {0xFC, 0xCF, 0xAB};

std::vector<LocalItemRow> g_localItems;

std::uint16_t ReadU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::int32_t ReadI32(const std::uint8_t* bytes)
{
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bytes[0]) |
                                     (static_cast<std::uint32_t>(bytes[1]) << 8) |
                                     (static_cast<std::uint32_t>(bytes[2]) << 16) |
                                     (static_cast<std::uint32_t>(bytes[3]) << 24));
}

void XorRecord(std::uint8_t* bytes, int size)
{
    for (int i = 0; i < size; ++i)
    {
        bytes[i] = static_cast<std::uint8_t>(bytes[i] ^ kXorKey[i % 3]);
    }
}

std::string FieldText(const std::uint8_t* bytes, int size)
{
    int length = 0;
    while (length < size && bytes[length] != 0)
    {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes), reinterpret_cast<const char*>(bytes + length));
}

std::string JoinModelPath(std::string folder, std::string name)
{
    if (name.empty())
    {
        return {};
    }
    std::replace(folder.begin(), folder.end(), '\\', '/');
    std::replace(name.begin(), name.end(), '\\', '/');
    const auto dot = name.rfind('.');
    const bool hasExtension = dot != std::string::npos && (name.size() - dot == 4);
    if (!hasExtension)
    {
        name += ".bmd";
    }
    if (!folder.empty() && folder.back() != '/')
    {
        folder += '/';
    }
    std::string path = folder + name;
    if (path.rfind("Data/", 0) != 0 && path.rfind("data/", 0) != 0)
    {
        path = "Data/" + path;
    }
    return path;
}

bool RowFitsInventory(const std::uint8_t* record)
{
    return record[kWidthOffset] <= kMaxInventorySpan && record[kHeightOffset] <= kMaxInventorySpan;
}

} // namespace

LocalItemDocument ParseLocalItemTable(const std::uint8_t* bytes, std::size_t size)
{
    LocalItemDocument document;
    if (bytes == nullptr || size < static_cast<std::size_t>(kCountBytes + kChecksumBytes + kMinStride))
    {
        document.error = "item table is too small";
        return document;
    }

    const int count = ReadI32(bytes);
    const int body = static_cast<int>(size) - kCountBytes - kChecksumBytes;
    if (count <= 0 || count > kMaxItems || body % count != 0)
    {
        document.error = "item table count does not match the file";
        return document;
    }

    const int stride = body / count;
    if (stride < kMinStride || stride > kMaxStride)
    {
        document.error = "item table row is not a Season 21 item";
        return document;
    }

    int plausible = 0;
    document.items.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        std::vector<std::uint8_t> record(static_cast<std::size_t>(stride));
        const std::size_t offset = static_cast<std::size_t>(kCountBytes) + static_cast<std::size_t>(index) * stride;
        std::memcpy(record.data(), bytes + offset, record.size());
        XorRecord(record.data(), stride);
        if (RowFitsInventory(record.data()))
        {
            ++plausible;
        }

        LocalItemRow row;
        const int group = ReadU16(record.data() + kGroupOffset);
        const int id = ReadU16(record.data() + kIdOffset);
        row.itemType = group * kGroupSpan + id;
        row.width = record[kWidthOffset];
        row.height = record[kHeightOffset];
        row.modelFile = JoinModelPath(FieldText(record.data() + kFolderOffset, kFolderBytes),
                                      FieldText(record.data() + kModelOffset, kModelBytes));
        document.items.push_back(std::move(row));
    }

    if (plausible * 100 < count * 80)
    {
        document.items.clear();
        document.error = "item table is not Season 21 item data";
        return document;
    }
    return document;
}

void SetLocalItemModels(std::vector<LocalItemRow> items)
{
    std::sort(items.begin(), items.end(),
              [](const LocalItemRow& left, const LocalItemRow& right) { return left.itemType < right.itemType; });
    g_localItems = std::move(items);
}

const LocalItemRow* FindLocalItemModel(int itemType)
{
    const auto found = std::lower_bound(
        g_localItems.begin(), g_localItems.end(), itemType,
        [](const LocalItemRow& row, int type) { return row.itemType < type; });
    if (found == g_localItems.end() || found->itemType != itemType)
    {
        return nullptr;
    }
    return &*found;
}

std::span<const LocalItemRow> StoredLocalItems()
{
    return g_localItems;
}

int ChooseEquipmentItemType(int group, int number)
{
    return (group & 0x0F) * MAX_ITEM_INDEX + number;
}

} // namespace Data::Items
