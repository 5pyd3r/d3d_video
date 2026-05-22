#include "StringUtils.h"

std::string w2s(const std::wstring& wstr) {
    int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &str[0], (int)str.size(), NULL, NULL);
    return str;
}

std::string w2u(const std::wstring& wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], (int)str.size(), NULL, NULL);
    return str;
}

std::string GetLastErrorMessage(DWORD errorCode) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr);

    if (size == 0)
        return "Unknown error (FormatMessage failed)";

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    if (!message.empty() && message.back() == '\n')
        message.pop_back();
    if (!message.empty() && message.back() == '\r')
        message.pop_back();

    return message;
}
