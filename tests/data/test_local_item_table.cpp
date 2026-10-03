#include "doctest.h"

#include "Data/GameData/ItemData/LocalItemTable.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{

constexpr int kStride = 604;

void XorRecord(std::vector<std::uint8_t>& record)
{
    const std::uint8_t key[3] = {0xFC, 0xCF, 0xAB};
    for (std::size_t i = 0; i < record.size(); ++i)
    {
        record[i] = static_cast<std::uint8_t>(record[i] ^ key[i % 3]);
    }
}

std::vector<std::uint8_t> ItemFile(std::uint16_t group, std::uint16_t id, std::uint8_t width, std::uint8_t height,
                                   const char* folder, const char* model)
{
    std::vector<std::uint8_t> record(kStride, 0);
    record[4] = static_cast<std::uint8_t>(group & 0xFF);
    record[5] = static_cast<std::uint8_t>((group >> 8) & 0xFF);
    record[6] = static_cast<std::uint8_t>(id & 0xFF);
    record[7] = static_cast<std::uint8_t>((id >> 8) & 0xFF);
    std::memcpy(record.data() + 8, folder, std::strlen(folder));
    std::memcpy(record.data() + 268, model, std::strlen(model));
    record[602] = width;
    record[603] = height;
    XorRecord(record);

    std::vector<std::uint8_t> file;
    file.push_back(1);
    file.push_back(0);
    file.push_back(0);
    file.push_back(0);
    file.insert(file.end(), record.begin(), record.end());
    file.insert(file.end(), {0, 0, 0, 0});
    return file;
}

} // namespace

TEST_CASE("Season 21 item group and id are one client item type")
{
    const std::vector<std::uint8_t> file = ItemFile(2, 0, 2, 4, "Item", "Axe01.bmd");
    const Data::Items::LocalItemDocument document = Data::Items::ParseLocalItemTable(file.data(), file.size());
    REQUIRE(document.error.empty());
    REQUIRE(document.items.size() == 1);
    CHECK(document.items[0].itemType == 512);
    CHECK(document.items[0].width == 2);
    CHECK(document.items[0].height == 4);
    CHECK(document.items[0].modelFile == "Data/Item/Axe01.bmd");
}

TEST_CASE("a row that cannot be an inventory item is not a Season 21 table")
{
    const std::vector<std::uint8_t> file = ItemFile(0, 1, 200, 200, "Item", "Sword01.bmd");
    const Data::Items::LocalItemDocument document = Data::Items::ParseLocalItemTable(file.data(), file.size());
    CHECK_FALSE(document.error.empty());
    CHECK(document.items.empty());
}

TEST_CASE("equipment uses the local packing when the section slot has no size")
{
    CHECK(Data::Items::ChooseEquipmentItemType(2, 0, true, true) == 1024);
    CHECK(Data::Items::ChooseEquipmentItemType(2, 0, false, true) == 512);
    CHECK(Data::Items::ChooseEquipmentItemType(1, 0, true, false) == 512);
}
