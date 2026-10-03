#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Data::Items
{

// One row of Data/Local/Item.bmd (Season 21). The file stores the client item
// number split by 256: item number = group * 256 + id.
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
// rejected. itemType is the client item number stored in the file.
LocalItemDocument ParseLocalItemTable(const std::uint8_t* bytes, std::size_t size);

// Keeps the rows from a table the client loaded. Empty until then.
void SetLocalItemModels(std::vector<LocalItemRow> items);

const LocalItemRow* FindLocalItemModel(int itemType);

std::span<const LocalItemRow> StoredLocalItems();

// Server equipment uses the Season 6 item number: section * 512 + index.
int ChooseEquipmentItemType(int group, int number);

} // namespace Data::Items
