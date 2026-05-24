#ifndef PLATFORM_STREAMUTILS_H
#define PLATFORM_STREAMUTILS_H

#include <string>

// Returns true if str looks like a stream URI (has a <scheme>:// prefix).
bool IsStreamUri(const std::string& str);

// Removes leading and trailing whitespace (spaces, tabs, newlines).
std::string TrimString(const std::string& str);

#endif
