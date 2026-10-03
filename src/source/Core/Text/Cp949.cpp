#include "Core/Text/Cp949.h"

#include <cstdint>

namespace Core::Text
{
namespace
{

constexpr unsigned int kLeadBase = 0x81;
constexpr unsigned int kLeadLast = 0xFE;
constexpr wchar_t kReplacement = 0xFFFD;

#include "Cp949Table.inc"

wchar_t FromPair(unsigned int lead, unsigned int trail)
{
    const std::uint16_t code = kCp949Lead[lead - kLeadBase][trail];
    if (code == 0)
    {
        return kReplacement;
    }
    return static_cast<wchar_t>(code);
}

} // namespace

std::wstring WideFromCp949(std::string_view bytes)
{
    std::wstring text;
    text.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        const auto lead = static_cast<unsigned int>(static_cast<unsigned char>(bytes[i]));
        if (lead < 0x80)
        {
            text.push_back(static_cast<wchar_t>(lead));
            continue;
        }
        if (lead >= kLeadBase && lead <= kLeadLast && i + 1 < bytes.size())
        {
            const auto trail = static_cast<unsigned int>(static_cast<unsigned char>(bytes[i + 1]));
            const wchar_t code = FromPair(lead, trail);
            if (code != kReplacement)
            {
                text.push_back(code);
                ++i;
                continue;
            }
        }
        text.push_back(kReplacement);
    }
    return text;
}

} // namespace Core::Text
