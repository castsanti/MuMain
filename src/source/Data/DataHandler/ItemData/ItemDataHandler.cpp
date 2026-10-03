#include "stdafx.h"
#include "ItemDataHandler.h"
#include "ItemJsonStorage.h"
#include "Core/Globals/_struct.h"
#include "Core/Globals/_define.h"
#include "Core/Text/Utf8.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameData/ItemData/ItemAttributeConversion.h"
#include "Data/GameData/EffectData/GlowColorList.h"
#include "Data/GameData/ItemData/ItemDataValidation.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Data/GameData/ItemData/ItemModelDatabase.h"
#include "Data/GameData/ItemData/LocalItemTable.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#ifdef _EDITOR
#include "Data/DataHandler/CommonDataSaver.h"
#include "ItemDataSaver.h"
#include "ItemDataExportS6E3.h"
#include "ItemDataExportAsCSV.h"
#endif

// External references
extern ITEM_ATTRIBUTE* ItemAttribute;

using namespace Data::Items;

void ApplySeason21ItemTable();

namespace
{
// How many errors the player sees; all of them go to the log.
constexpr size_t MaxErrorsInMessage = 10;

double MillisecondsSince(std::chrono::steady_clock::time_point start)
{
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

void LogIssues(const std::vector<ItemDataIssue>& issues)
{
    const auto logger = mu::log::Get("data");
    for (const ItemDataIssue& issue : issues)
    {
        if (issue.severity == ItemDataIssueSeverity::Error)
        {
            MU_LOG_ERROR(logger, "Item data {}", issue.ToString());
        }
        else
        {
            MU_LOG_WARN(logger, "Item data {}", issue.ToString());
        }
    }
}

std::string DescribeErrors(const std::filesystem::path& directory, const std::vector<ItemDataIssue>& issues)
{
    std::string message = "The item data in " + directory.string() + " has errors:\n";
    size_t errorCount = 0;
    for (const ItemDataIssue& issue : issues)
    {
        if (issue.severity != ItemDataIssueSeverity::Error)
        {
            continue;
        }
        if (++errorCount <= MaxErrorsInMessage)
        {
            message += "\n" + issue.ToString();
        }
    }
    if (errorCount > MaxErrorsInMessage)
    {
        message += "\n... and " + std::to_string(errorCount - MaxErrorsInMessage) + " more";
    }
    return message + "\n\nAll problems are listed in MuError.log.";
}

// ITEM_ATTRIBUTE records with the item names of `locale`.
std::unique_ptr<ITEM_ATTRIBUTE[]> BuildItemAttributes(std::string_view locale)
{
    auto attributes = std::make_unique<ITEM_ATTRIBUTE[]>(MAX_ITEM);
    for (const ItemDefinition& slot : g_ItemDatabase.GetAllSlots())
    {
        if (!slot.Exists())
        {
            continue;
        }
        ItemDefinition definition = slot;
        definition.name = Core::Text::FromUtf8(definition.names.Get(locale));
        ToItemAttribute(definition, attributes[MakeItemType(slot.group, slot.number)]);
    }
    return attributes;
}
} // namespace

CItemDataHandler::CItemDataHandler()
{
}

CItemDataHandler& CItemDataHandler::GetInstance()
{
    static CItemDataHandler instance;
    return instance;
}

ITEM_ATTRIBUTE* CItemDataHandler::GetItemAttributes()
{
    return ItemAttribute;
}

ITEM_ATTRIBUTE* CItemDataHandler::GetItemAttribute(int index)
{
    if (index >= 0 && index < MAX_ITEM)
        return &ItemAttribute[index];
    return nullptr;
}

int CItemDataHandler::GetItemCount() const
{
    return MAX_ITEM;
}

std::wstring CItemDataHandler::GetItemFilePath(const std::wstring& language)
{
    return L"Data\\Local\\" + language + L"\\Item_" + language + L".bmd";
}

bool CItemDataHandler::Load(std::string& errorMessage)
{
    const auto loadStart = std::chrono::steady_clock::now();
    ItemDataLoadResult result = LoadItemDataDirectory(GetItemDataDirectory());
    LogIssues(result.issues);
    if (HasErrors(result.issues))
    {
        errorMessage = DescribeErrors(GetItemDataDirectory(), result.issues);
        return false;
    }
    const double loadMilliseconds = MillisecondsSince(loadStart);

    const auto buildStart = std::chrono::steady_clock::now();
    g_ItemDatabase.SetDisplayLocale(I18N::GetCurrentLocale());
    g_ItemDatabase.Build(result.items);
    FillItemAttributes();
    ApplySeason21ItemTable();
    RegisterLocaleObserver();

    MU_LOG_INFO(mu::log::Get("data"), "Loaded {} items from {} in {:.1f} ms (item database build {:.2f} ms)",
                g_ItemDatabase.GetExistingItemCount(), GetItemDataDirectory().string(), loadMilliseconds,
                MillisecondsSince(buildStart));
    return true;
}

bool CItemDataHandler::LoadModels(std::string& errorMessage)
{
    const auto loadStart = std::chrono::steady_clock::now();
    GlowColorsLoadResult colors = LoadGlowColorsFile(GetGlowColorsFile());
    ItemModelDataLoadResult result = LoadItemModelDataDirectory(GetItemModelDataDirectory());
    result.issues.insert(result.issues.begin(), colors.issues.begin(), colors.issues.end());
    if (!HasErrors(colors.issues))
    {
        ValidateItemModelGlowColors(result.models, colors.colors, result.issues);
    }
    LogIssues(result.issues);
    if (HasErrors(result.issues))
    {
        errorMessage = DescribeErrors(GetItemModelDataDirectory(), result.issues);
        return false;
    }

    g_GlowColors.Build(colors.colors);
    g_ItemModelDatabase.Build(result.models, g_GlowColors);
    MU_LOG_INFO(mu::log::Get("data"), "Loaded {} item models from {} in {:.1f} ms", g_ItemModelDatabase.GetModelCount(),
                GetItemModelDataDirectory().string(), MillisecondsSince(loadStart));
    return true;
}

static bool ReadWholeFile(const wchar_t* path, std::vector<std::uint8_t>& bytes)
{
    FILE* file = _wfopen(path, L"rb");
    if (file == nullptr)
    {
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::fclose(file);
        return false;
    }
    const long fileSize = std::ftell(file);
    if (fileSize <= 0 || std::fseek(file, 0, SEEK_SET) != 0)
    {
        std::fclose(file);
        return false;
    }
    bytes.resize(static_cast<std::size_t>(fileSize));
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    return read == bytes.size();
}

void ApplySeason21ItemTable()
{
    if (ItemAttribute == nullptr)
    {
        return;
    }

    std::vector<std::uint8_t> bytes;
    if (!ReadWholeFile(L"Data\\Local\\Item.bmd", bytes))
    {
        return;
    }

    LocalItemDocument document = ParseLocalItemTable(bytes.data(), bytes.size());
    if (!document.error.empty())
    {
        MU_LOG_WARN(mu::log::Get("data"), "Data/Local/Item.bmd was not used: {}", document.error);
        return;
    }

    int sized = 0;
    for (const LocalItemRow& item : document.items)
    {
        if (!IsValidItemType(item.itemType) || item.width <= 0 || item.height <= 0)
        {
            continue;
        }
        ItemAttribute[item.itemType].Width = static_cast<BYTE>(item.width);
        ItemAttribute[item.itemType].Height = static_cast<BYTE>(item.height);
        ++sized;
    }
    const int rows = static_cast<int>(document.items.size());
    SetLocalItemModels(std::move(document.items));
    MU_LOG_INFO(mu::log::Get("data"), "Loaded {} Season 21 items from Data/Local/Item.bmd ({} with a size)", rows,
                sized);
}

void CItemDataHandler::FillItemAttributes()
{
    for (int itemType = 0; itemType < MAX_ITEM; ++itemType)
    {
        const ItemDefinition* definition = g_ItemDatabase.Find(itemType);
        if (definition != nullptr)
        {
            ToItemAttribute(*definition, ItemAttribute[itemType]);
        }
        else
        {
            // Empty slots stay all zero, as they were with item.bmd.
            ItemAttribute[itemType] = ITEM_ATTRIBUTE{};
        }
    }
}

void CItemDataHandler::RegisterLocaleObserver()
{
    if (m_localeObserverRegistered)
    {
        return;
    }
    I18N::RegisterLocaleObserver(&CItemDataHandler::OnLocaleChanged, this);
    m_localeObserverRegistered = true;
}

void CItemDataHandler::OnLocaleChanged(void* context) noexcept
{
    auto* handler = static_cast<CItemDataHandler*>(context);
    g_ItemDatabase.SetDisplayLocale(I18N::GetCurrentLocale());
    handler->FillItemAttributes();
    if (ItemAttribute == nullptr)
    {
        return;
    }
    for (const LocalItemRow& item : StoredLocalItems())
    {
        if (!IsValidItemType(item.itemType) || item.width <= 0 || item.height <= 0)
        {
            continue;
        }
        ItemAttribute[item.itemType].Width = static_cast<BYTE>(item.width);
        ItemAttribute[item.itemType].Height = static_cast<BYTE>(item.height);
    }
}

#ifdef _EDITOR
void CItemDataHandler::OnItemEdited(int itemType)
{
    if (!IsValidItemType(itemType))
    {
        return;
    }

    ITEM_ATTRIBUTE& attribute = ItemAttribute[itemType];
    ItemDefinition definition = g_ItemDatabase.GetAllSlots()[itemType];
    definition.group = GetItemGroup(itemType);
    definition.number = GetItemNumber(itemType);
    CopyItemAttributeStats(attribute, definition);

    // Only a changed name is stored; otherwise every stat edit would turn the
    // shown English fallback into a translation. ITEM_ATTRIBUTE holds a cut
    // name, so compare against the cut name, or a stat edit would store the
    // cut version of a long name.
    const std::wstring editedName = ReadItemAttributeName(attribute);
    if (editedName != CutToItemAttributeName(definition.name))
    {
        const std::string editedNameUtf8 = Core::Text::ToUtf8(editedName.c_str());
        definition.names.Set(g_ItemDatabase.GetDisplayLocale(), editedNameUtf8);
        if (definition.names.GetNeutral().empty())
        {
            // A new item needs an English name.
            definition.names.Set(Data::LocalizedString::NeutralLocale, editedNameUtf8);
        }
    }

    g_ItemDatabase.Set(definition);

    // A removed translation shows the English name again.
    const ItemDefinition* updated = g_ItemDatabase.Find(itemType);
    if (updated != nullptr && CutToItemAttributeName(updated->name) != editedName)
    {
        ToItemAttribute(*updated, attribute);
    }
}

void CItemDataHandler::OnItemsSwapped(int firstItemType, int secondItemType)
{
    g_ItemDatabase.Swap(firstItemType, secondItemType);
}

ItemDataSaveResult CItemDataHandler::Save(std::vector<ItemDataIssue>& issues)
{
    return SaveItemDataDirectory(GetItemDataDirectory(), g_ItemDatabase.GetAllSlots(), issues);
}

ItemBmdImportResult CItemDataHandler::ImportFromBmd()
{
    ItemBmdImportResult result = ImportItemBmdFiles();
    if (HasErrors(result.issues))
    {
        return result;
    }

    result.keptEnglishNameCount = KeepCurrentEnglishNames(result.items);
    KeepFieldsNotInBmd(result.items);
    ValidateItems(result.items, result.validationIssues);

    g_ItemDatabase.Build(result.items);
    FillItemAttributes();
    return result;
}

int CItemDataHandler::KeepCurrentEnglishNames(std::vector<ItemDefinition>& items)
{
    int keptCount = 0;
    for (ItemDefinition& item : items)
    {
        const ItemDefinition* current = g_ItemDatabase.Find(item.group, item.number);
        if (!item.names.GetNeutral().empty() || current == nullptr || current->names.GetNeutral().empty())
        {
            continue;
        }
        item.names.Set(Data::LocalizedString::NeutralLocale, current->names.GetNeutral());
        ++keptCount;
    }
    return keptCount;
}

void CItemDataHandler::KeepFieldsNotInBmd(std::vector<ItemDefinition>& items)
{
    for (ItemDefinition& item : items)
    {
        if (const ItemDefinition* current = g_ItemDatabase.Find(item.group, item.number))
        {
            CopyFieldsNotInItemAttribute(*current, item);
        }
    }
}

bool CItemDataHandler::ExportAsBmd(std::string& changeLog)
{
    bool success = true;
    for (const ItemBmdLanguage& language : GetItemBmdLanguages())
    {
        const auto attributes = BuildItemAttributes(language.locale);
        std::string languageChangeLog;
        const bool saved =
            ItemDataSaver::Save(GetItemFilePath(language.folder).c_str(), attributes.get(), &languageChangeLog);
        // An unchanged file is not a failure.
        const bool unchanged = !saved && languageChangeLog == CommonDataSaver::NoChangesMessage;
        success = (saved || unchanged) && success;
        changeLog += languageChangeLog;
    }
    return success;
}

bool CItemDataHandler::ExportAsS6E3(wchar_t* fileName)
{
    return ItemDataExportS6E3::SaveLegacy(fileName);
}

bool CItemDataHandler::ExportToCsv(wchar_t* fileName)
{
    return ItemDataExportAsCSV::ExportToCsv(fileName);
}
#endif
