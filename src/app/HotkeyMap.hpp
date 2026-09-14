#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace HotkeyMap
{
    UINT toVk(const std::string &name);
    std::string fromVk(UINT vk);
    std::string normalize(const std::string &name);
    bool validate(const std::string &name, std::string &reason);
}