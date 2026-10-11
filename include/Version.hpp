#pragma once

// The build injects SVH_VERSION_STRING from CMake (derived from vcpkg.json's
// version-string, or overridden with -DSVH_VERSION=...). The fallback keeps
// editors and standalone tooling happy when the file is parsed outside a
// configured build tree.
#ifndef SVH_VERSION_STRING
#define SVH_VERSION_STRING "0.0.0-dev"
#endif

namespace Version
{
    inline constexpr const char *String = SVH_VERSION_STRING;
}
