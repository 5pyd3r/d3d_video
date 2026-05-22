#include <gtest/gtest.h>
#include "../../src/platform/StringUtils.h"

TEST(StringUtilsTest, GetLastErrorMessage_FileNotFound) {
    std::string msg = GetLastErrorMessage(2);  // ERROR_FILE_NOT_FOUND
    EXPECT_FALSE(msg.empty());
}

TEST(StringUtilsTest, GetLastErrorMessage_Success) {
    std::string msg = GetLastErrorMessage(0);  // ERROR_SUCCESS
    EXPECT_FALSE(msg.empty());
}

TEST(StringUtilsTest, W2U_BasicRoundTrip) {
    std::wstring input = L"Hello World";
    std::string utf8 = w2u(input);
    EXPECT_FALSE(utf8.empty());
    EXPECT_EQ(utf8, "Hello World");
}

TEST(StringUtilsTest, W2S_Basic) {
    std::wstring input = L"Test";
    std::string ansi = w2s(input);
    EXPECT_FALSE(ansi.empty());
}
