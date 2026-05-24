#ifndef PLATFORM_STRINGUTILS_H
#define PLATFORM_STRINGUTILS_H

#include <string>
#include <windows.h>

std::string w2s(const std::wstring& wstr);
std::string w2u(const std::wstring& wstr);
std::wstring u2w(const std::string& str);
std::string GetLastErrorMessage(DWORD errorCode = GetLastError());
bool IsVideoFile(const std::string& path);
std::wstring TruncateFileNameForTitle(const std::string& filePath, size_t maxLen = 50);

#endif
