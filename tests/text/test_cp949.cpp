#include "doctest.h"

#include "Core/Text/Cp949.h"

TEST_CASE("CP949 Korean server names become Unicode")
{
    const std::string cp949("\xC7\xD1\xB1\xDB");
    CHECK(Core::Text::WideFromCp949(cp949) == L"\uD55C\uAE00");
}

TEST_CASE("CP949 leaves ASCII server names unchanged")
{
    CHECK(Core::Text::WideFromCp949("Valhalla") == L"Valhalla");
}

TEST_CASE("UTF-8 Korean is not a CP949 name")
{
    const std::string utf8("\xED\x95\x9C\xEA\xB8\x80");
    CHECK(Core::Text::WideFromCp949(utf8) != L"\uD55C\uAE00");
}
