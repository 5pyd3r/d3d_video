#ifndef PLATFORM_STRINGUTILS_H
#define PLATFORM_STRINGUTILS_H

#include <string>
#include <windows.h>

std::string w2s(const std::wstring& wstr);
std::string w2u(const std::wstring& wstr);
std::string GetLastErrorMessage(DWORD errorCode = GetLastError());
bool IsVideoFile(const std::string& path);

#endif
