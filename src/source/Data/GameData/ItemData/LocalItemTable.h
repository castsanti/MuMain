#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Data::Items
{

// One row of Data/Local/Item.bmd (Season 21). The file's group is not the
// item section: section * 512 + index == group * 256 + id.
struct LocalItemRow
{
    int itemType = -1;
    int width = 0;
    int height = 0;
    std::string modelFile;
};

struct LocalItemDocument
{
    std::vector<LocalItemRow> items;
    std::string error;
};

// Reads the count-prefixed, per-record XOR table. A Season 6 item file is
// rejected. itemType is group * 256 + id.
LocalItemDocument ParseLocalItemTable(const std::uint8_t* bytes, std::size_t size);

// Keeps the rows from a table the client loaded. Empty until then.
void SetLocalItemModels(std::vector<LocalItemRow> items);

const LocalItemRow* FindLocalItemModel(int itemType);

// Equipment packets name an item as a group and a number. Season 6 packs that
// as section * 512 + index. Season 21's local table packs the same item as
// group * 256 + id. When only one of those slots has an inventory size, that
// is the item. When both do, the section packing stays.
int ChooseEquipmentItemType(int group, int number, bool sectionHasSize, bool packedHasSize);

} // namespace Data::Items
