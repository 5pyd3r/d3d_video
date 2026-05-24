#include "StreamUtils.h"
#include <algorithm>

bool IsStreamUri(const std::string& str) {
    auto pos = str.find("://");
    if (pos == std::string::npos || pos == 0) return false;

    // Scheme must be alphabetic characters only
    for (size_t i = 0; i < pos; ++i) {
        char c = str[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

std::string TrimString(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n'))
        ++start;

    auto end = str.end();
    while (end != start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n'))
        --end;

    return std::string(start, end);
}
