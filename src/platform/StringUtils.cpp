#include "StringUtils.h"
#include <algorithm>
#include <filesystem>

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

std::wstring u2w(const std::string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], (int)wstr.size());
    return wstr;
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

bool IsVideoFile(const std::string& path) {
    static const std::string kExtensions[] = {
        ".mp4", ".avi", ".mkv", ".mov", ".wmv",
        ".webm", ".flv", ".ts", ".m4v", ".mpg", ".mpeg"
    };

    auto dot = path.rfind('.');
    if (dot == std::string::npos) return false;

    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const auto& known : kExtensions) {
        if (ext == known) return true;
    }
    return false;
}

std::wstring TruncateFileNameForTitle(const std::string& filePath, size_t maxLen) {
    std::filesystem::path p(filePath);
    std::wstring fileName = p.filename().wstring();

    if (fileName.length() <= maxLen) return fileName;

    size_t dotPos = fileName.find_last_of(L'.');
    std::wstring ext = (dotPos != std::wstring::npos) ? fileName.substr(dotPos) : L"";
    std::wstring nameWithoutExt = (dotPos != std::wstring::npos) ? fileName.substr(0, dotPos) : fileName;

    const size_t keepEnd = 5;
    if (nameWithoutExt.length() <= keepEnd + 3) {
        return nameWithoutExt + L"..." + ext;
    }

    size_t keepStart = maxLen - 3 - keepEnd - ext.length();
    if (keepStart < 1) keepStart = 1;
    if (keepStart > nameWithoutExt.length() - keepEnd)
        keepStart = nameWithoutExt.length() - keepEnd;

    return nameWithoutExt.substr(0, keepStart)
        + L"..."
        + nameWithoutExt.substr(nameWithoutExt.length() - keepEnd)
        + ext;
}
