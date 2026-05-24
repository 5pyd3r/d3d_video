#include <gtest/gtest.h>
#include "../../src/platform/StreamUtils.h"

TEST(StreamUtils, IsStreamUri_Rtsp) {
    EXPECT_TRUE(IsStreamUri("rtsp://192.168.1.1:554/stream"));
}

TEST(StreamUtils, IsStreamUri_Rtmp) {
    EXPECT_TRUE(IsStreamUri("rtmp://live.example.com/live/stream"));
}

TEST(StreamUtils, IsStreamUri_Http) {
    EXPECT_TRUE(IsStreamUri("http://example.com/video.mp4"));
}

TEST(StreamUtils, IsStreamUri_Https) {
    EXPECT_TRUE(IsStreamUri("https://example.com/video.m3u8"));
}

TEST(StreamUtils, IsStreamUri_Udp) {
    EXPECT_TRUE(IsStreamUri("udp://239.0.0.1:1234"));
}

TEST(StreamUtils, IsStreamUri_FilePath) {
    EXPECT_FALSE(IsStreamUri("C:\\Users\\test\\video.mp4"));
    EXPECT_FALSE(IsStreamUri("/home/user/video.mp4"));
}

TEST(StreamUtils, IsStreamUri_NoScheme) {
    EXPECT_FALSE(IsStreamUri("example.com/video"));
}

TEST(StreamUtils, IsStreamUri_Empty) {
    EXPECT_FALSE(IsStreamUri(""));
}

TEST(StreamUtils, IsStreamUri_SchemeWithDigits) {
    EXPECT_FALSE(IsStreamUri("123://host/path"));
}

TEST(StreamUtils, TrimString_NoWhitespace) {
    EXPECT_EQ(TrimString("rtsp://host/path"), "rtsp://host/path");
}

TEST(StreamUtils, TrimString_LeadingSpaces) {
    EXPECT_EQ(TrimString("  rtsp://host/path"), "rtsp://host/path");
}

TEST(StreamUtils, TrimString_TrailingNewline) {
    EXPECT_EQ(TrimString("rtsp://host/path\n"), "rtsp://host/path");
}

TEST(StreamUtils, TrimString_BothSides) {
    EXPECT_EQ(TrimString("\t rtsp://host/path \r\n"), "rtsp://host/path");
}

TEST(StreamUtils, TrimString_WhitespaceOnly) {
    EXPECT_EQ(TrimString("   \t\n"), "");
}

TEST(StreamUtils, TrimString_Empty) {
    EXPECT_EQ(TrimString(""), "");
}
