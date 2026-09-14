#include "HotkeyMap.hpp"

namespace HotkeyMap
{
    std::string normalize(const std::string &name)
    {
        std::string out;
        out.reserve(name.size());
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            char c = name[i];
            if (c >= 'a' && c <= 'z')
            {
                c = (char)(c - ('a' - 'A'));
            }
            out += c;
        }
        return out;
    }

    UINT toVk(const std::string &name)
    {
        std::string upper = normalize(name);

        if (upper == "SPACE") return VK_SPACE;
        if (upper == "UP")    return VK_UP;
        if (upper == "DOWN")  return VK_DOWN;
        if (upper == "LEFT")  return VK_LEFT;
        if (upper == "RIGHT") return VK_RIGHT;

        if (upper.size() == 1 && upper[0] >= 'A' && upper[0] <= 'Z')
        {
            return (UINT)upper[0];
        }
        if (upper.size() == 1 && upper[0] >= '0' && upper[0] <= '9')
        {
            return (UINT)upper[0];
        }

        if (upper.size() >= 2 && upper[0] == 'F')
        {
            int num = 0;
            std::size_t i;
            for (i = 1; i < upper.size(); ++i)
            {
                if (upper[i] < '0' || upper[i] > '9') break;
                num = num * 10 + (upper[i] - '0');
            }
            if (i == upper.size() && num >= 1 && num <= 24)
            {
                return (UINT)(VK_F1 + num - 1);
            }
        }

        return 0;
    }

    std::string fromVk(UINT vk)
    {
        if (vk == VK_SPACE) return "Space";
        if (vk == VK_UP)    return "Up";
        if (vk == VK_DOWN)  return "Down";
        if (vk == VK_LEFT)  return "Left";
        if (vk == VK_RIGHT) return "Right";
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        {
            return std::string(1, (char)vk);
        }
        if (vk >= VK_F1 && vk <= VK_F24)
        {
            return "F" + std::to_string(vk - VK_F1 + 1);
        }
        return "";
    }

    bool validate(const std::string &name, std::string &outReason)
    {
        if (name.empty())
        {
            outReason = "Key name is empty.";
            return false;
        }
        if (toVk(name) == 0)
        {
            outReason = "'" + name +
                     "' is not a supported key. Use A-Z, 0-9, F1-F24, Space, or an arrow key.";
            return false;
        }
        return true;
    }
}
