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

TEST(StringUtilsTest, IsVideoFile_KnownExtensions) {
    EXPECT_TRUE(IsVideoFile("video.mp4"));
    EXPECT_TRUE(IsVideoFile("video.avi"));
    EXPECT_TRUE(IsVideoFile("video.mkv"));
    EXPECT_TRUE(IsVideoFile("video.MOV"));
    EXPECT_TRUE(IsVideoFile("video.WMV"));
    EXPECT_TRUE(IsVideoFile("video.webm"));
    EXPECT_TRUE(IsVideoFile("video.flv"));
    EXPECT_TRUE(IsVideoFile("video.ts"));
    EXPECT_TRUE(IsVideoFile("video.m4v"));
    EXPECT_TRUE(IsVideoFile("video.mpg"));
    EXPECT_TRUE(IsVideoFile("video.mpeg"));
}

TEST(StringUtilsTest, IsVideoFile_NonVideo) {
    EXPECT_FALSE(IsVideoFile("readme.txt"));
    EXPECT_FALSE(IsVideoFile("video.mp3"));
    EXPECT_FALSE(IsVideoFile("noextension"));
    EXPECT_FALSE(IsVideoFile(""));
}

TEST(StringUtilsTest, IsVideoFile_PathWithDots) {
    EXPECT_TRUE(IsVideoFile("C:\\videos\\movie.2024.mp4"));
    EXPECT_FALSE(IsVideoFile(".gitignore"));
}
