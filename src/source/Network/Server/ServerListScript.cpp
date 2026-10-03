#include "Network/Server/ServerListScript.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Globals/_crypt.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

namespace Network::ServerList
{
namespace
{

struct Layout
{
    ScriptFormat format;
    int headerBytes;
    bool hasIndex;
    bool hasNonPvp;
    bool signedLength;
};

std::uint16_t ReadU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
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

bool ReadLength(const std::uint8_t* header, const Layout& layout, int& length, std::string& error)
{
    const int offset = layout.headerBytes - kLengthBytes;
    const std::uint16_t raw = ReadU16(header + offset);
    if (layout.signedLength)
    {
        const auto signedLength = static_cast<std::int16_t>(raw);
        if (signedLength < 0 || signedLength > kMaxDescriptionBytes)
        {
            error = "description length " + std::to_string(signedLength) + " is outside 0.." +
                    std::to_string(kMaxDescriptionBytes);
            return false;
        }
        length = signedLength;
        return true;
    }

    if (raw > kMaxDescriptionBytes)
    {
        error = "description length " + std::to_string(raw) + " is outside 0.." +
                std::to_string(kMaxDescriptionBytes);
        return false;
    }
    length = static_cast<int>(raw);
    return true;
}

ScriptRecord MakeRecord(const std::uint8_t* header, const std::uint8_t* description, int descriptionLength,
                        const Layout& layout, std::uint16_t fallbackIndex)
{
    ScriptRecord record;
    int cursor = 0;
    record.index = fallbackIndex;
    if (layout.hasIndex)
    {
        record.index = ReadU16(header);
        cursor = kIndexBytes;
    }

    record.name = FieldText(header + cursor, kServerNameBytes);
    cursor += kServerNameBytes;
    record.position = header[cursor];
    ++cursor;
    record.sequence = header[cursor];
    ++cursor;

    if (layout.hasNonPvp)
    {
        for (int i = 0; i < kSeason6NonPvpCount; ++i)
        {
            record.nonPvp[static_cast<std::size_t>(i)] = header[cursor + i];
        }
    }
    else
    {
        record.season21Marker = header[cursor];
    }

    record.description = FieldText(description, descriptionLength);
    return record;
}

bool ReadRecord(const std::uint8_t* bytes, std::size_t size, std::size_t& offset, const Layout& layout,
               std::uint16_t fallbackIndex, ScriptRecord& record, std::string& error)
{
    if (size - offset < static_cast<std::size_t>(layout.headerBytes))
    {
        error = "truncated record header";
        return false;
    }

    std::vector<std::uint8_t> header(bytes + offset, bytes + offset + layout.headerBytes);
    BuxConvert(reinterpret_cast<BYTE*>(header.data()), layout.headerBytes);
    offset += static_cast<std::size_t>(layout.headerBytes);

    int length = 0;
    if (!ReadLength(header.data(), layout, length, error))
    {
        return false;
    }
    if (size - offset < static_cast<std::size_t>(length))
    {
        error = "description extends past end of file";
        return false;
    }

    std::vector<std::uint8_t> description(static_cast<std::size_t>(length));
    if (length > 0)
    {
        std::copy(bytes + offset, bytes + offset + length, description.begin());
        BuxConvert(reinterpret_cast<BYTE*>(description.data()), length);
    }
    offset += static_cast<std::size_t>(length);
    record = MakeRecord(header.data(), description.data(), length, layout, fallbackIndex);
    return true;
}

bool Walk(const std::uint8_t* bytes, std::size_t size, const Layout& layout, std::vector<ScriptRecord>& records,
          std::string& error)
{
    records.clear();
    std::size_t offset = 0;
    std::uint16_t nextIndex = 0;

    while (offset < size)
    {
        ScriptRecord record;
        if (!ReadRecord(bytes, size, offset, layout, nextIndex, record, error))
        {
            records.clear();
            return false;
        }
        records.push_back(std::move(record));
        ++nextIndex;
    }

    if (records.empty())
    {
        error = "empty server list";
        return false;
    }
    return true;
}

const Layout kLayouts[] = {
    {ScriptFormat::Season6, kSeason6HeaderBytes, true, true, true},
    {ScriptFormat::Season21, kSeason21HeaderBytes, true, false, false},
    {ScriptFormat::Season21WithoutIndex, kSeason21NameFirstHeaderBytes, false, false, false},
};

} // namespace

const char* ScriptFormatName(ScriptFormat format)
{
    switch (format)
    {
    case ScriptFormat::Season6:
        return "Season 6";
    case ScriptFormat::Season21:
        return "Season 21";
    case ScriptFormat::Season21WithoutIndex:
        return "Season 21 without group index";
    case ScriptFormat::None:
        return "unrecognized";
    }
    return "unrecognized";
}

ScriptDocument ParseScript(const std::uint8_t* bytes, std::size_t size)
{
    ScriptDocument document;
    if (size > 0 && bytes == nullptr)
    {
        document.error = "missing server list bytes";
        return document;
    }

    std::string errors;
    for (const Layout& layout : kLayouts)
    {
        std::vector<ScriptRecord> records;
        std::string error;
        if (Walk(bytes, size, layout, records, error))
        {
            document.format = layout.format;
            document.records = std::move(records);
            return document;
        }

        if (!errors.empty())
        {
            errors += "; ";
        }
        errors += ScriptFormatName(layout.format);
        errors += ": ";
        errors += error;
    }

    document.error = std::move(errors);
    return document;
}

void CopyBoundedWide(wchar_t* destination, std::size_t capacity, std::wstring_view text)
{
    if (destination == nullptr || capacity == 0)
    {
        return;
    }

    const std::size_t count = text.size() < capacity - 1 ? text.size() : capacity - 1;
    if (count > 0)
    {
        std::wmemcpy(destination, text.data(), count);
    }
    destination[count] = L'\0';
}

} // namespace Network::ServerList
