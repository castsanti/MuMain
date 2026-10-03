#pragma once

#include <string>
#include <string_view>

namespace Core::Text
{

// Season 21 client text (server-list names, item titles) is CP949. The UI
// holds wide characters, so this is the conversion those bytes need.
// ASCII passes through. A byte that is not a character becomes U+FFFD.
[[nodiscard]] std::wstring WideFromCp949(std::string_view bytes);

} // namespace Core::Text
