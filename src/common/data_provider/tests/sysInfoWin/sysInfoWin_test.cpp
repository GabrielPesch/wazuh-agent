#include "sysInfoWin_test.hpp"
#include "packages/packagesWindowsParserHelper.h"

void SysInfoWinTest::SetUp() {};

void SysInfoWinTest::TearDown() {};

TEST_F(SysInfoWinTest, test_extract_HFValue_7618)
{
    // Invalid cases
    EXPECT_EQ("", PackageWindowsHelper::extractHFValue("KB"));
    EXPECT_EQ("", PackageWindowsHelper::extractHFValue("KBAAAAAA"));
    EXPECT_EQ("", PackageWindowsHelper::extractHFValue("AABBEEKB25A34111"));
    // Valid cases
    EXPECT_EQ("KB976902", PackageWindowsHelper::extractHFValue("KB976902\\KB976932\\SUPPORT\\SSU\\SAND\\5A42A8EB"));
    EXPECT_EQ("KB976932", PackageWindowsHelper::extractHFValue("KB976932\\SAND\\87C8A3D4"));
    EXPECT_EQ("KB2534111", PackageWindowsHelper::extractHFValue("KB2534111.MSU\\8847D77D"));
    EXPECT_EQ("KB2534111", PackageWindowsHelper::extractHFValue("KBKBKBKBKB2534111"));
    EXPECT_EQ("KB2534111", PackageWindowsHelper::extractHFValue("KB2534111"));
}

TEST_F(SysInfoWinTest, testHF_Valids_Format)
{
    std::set<std::string> ret;
    constexpr auto KB_FORMAT_REGEX_OK {R"(KB[0-9]{6,})"};
    constexpr auto KB_ONLY_FORMAT_REGEX {R"(KB)"};
    constexpr auto KB_NO_NUMBERS_FORMAT_REGEX {R"(KB[a-z])"};
    constexpr auto KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX {R"(KB[0-9]{6,}[a-zA-Z])"};
    PackageWindowsHelper::getHotFixFromReg(HKEY_LOCAL_MACHINE, PackageWindowsHelper::WIN_REG_HOTFIX, ret);

    for (const auto& hf : ret)
    {
        EXPECT_TRUE(std::regex_match(hf, std::regex(KB_FORMAT_REGEX_OK)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_ONLY_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_NO_NUMBERS_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX)));
    }
}

TEST_F(SysInfoWinTest, testHF_NT_Valids_Format)
{
    std::set<std::string> ret;
    constexpr auto KB_FORMAT_REGEX_OK {R"(KB[0-9]{6,})"};
    constexpr auto KB_ONLY_FORMAT_REGEX {R"(KB)"};
    constexpr auto KB_NO_NUMBERS_FORMAT_REGEX {R"(KB[a-z])"};
    constexpr auto KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX {R"(KB[0-9]{6,}[a-zA-Z])"};
    PackageWindowsHelper::getHotFixFromRegNT(HKEY_LOCAL_MACHINE, PackageWindowsHelper::VISTA_REG_HOTFIX, ret);

    for (const auto& hf : ret)
    {
        EXPECT_TRUE(std::regex_match(hf, std::regex(KB_FORMAT_REGEX_OK)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_ONLY_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_NO_NUMBERS_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX)));
    }
}

TEST_F(SysInfoWinTest, testHF_WOW_Valids_Format)
{
    std::set<std::string> ret;
    constexpr auto KB_FORMAT_REGEX_OK {R"(KB[0-9]{6,})"};
    constexpr auto KB_ONLY_FORMAT_REGEX {R"(KB)"};
    constexpr auto KB_NO_NUMBERS_FORMAT_REGEX {R"(KB[a-z])"};
    constexpr auto KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX {R"(KB[0-9]{6,}[a-zA-Z])"};
    PackageWindowsHelper::getHotFixFromRegWOW(HKEY_LOCAL_MACHINE, PackageWindowsHelper::WIN_REG_WOW_HOTFIX, ret);

    for (const auto& hf : ret)
    {
        EXPECT_TRUE(std::regex_match(hf, std::regex(KB_FORMAT_REGEX_OK)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_ONLY_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_NO_NUMBERS_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX)));
    }
}

TEST_F(SysInfoWinTest, testHF_PRODUCT_Valids_Format)
{
    std::set<std::string> ret;
    constexpr auto KB_FORMAT_REGEX_OK {R"(KB[0-9]{6,})"};
    constexpr auto KB_ONLY_FORMAT_REGEX {R"(KB)"};
    constexpr auto KB_NO_NUMBERS_FORMAT_REGEX {R"(KB[a-z])"};
    constexpr auto KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX {R"(KB[0-9]{6,}[a-zA-Z])"};
    PackageWindowsHelper::getHotFixFromRegProduct(
        HKEY_LOCAL_MACHINE, PackageWindowsHelper::WIN_REG_PRODUCT_HOTFIX, ret);

    for (const auto& hf : ret)
    {
        EXPECT_TRUE(std::regex_match(hf, std::regex(KB_FORMAT_REGEX_OK)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_ONLY_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_NO_NUMBERS_FORMAT_REGEX)));
        EXPECT_FALSE(std::regex_match(hf, std::regex(KB_WITH_NUMBERS_AND_LETTERS_FORMAT_REGEX)));
    }
}
