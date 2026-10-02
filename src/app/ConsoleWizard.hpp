#pragma once

#include <string>
#include "../core/Config.hpp"

class ConsoleWizard
{
public:
    bool begin();
    void collect(AppConfig &config, const std::string &redirectUri);
    bool complete(AppConfig &config, Config &configStore);
    void detach();

private:
    bool m_ownsConsole = false;
    bool m_detached = false;
    unsigned int m_previousOutputCp = 0;
    bool m_codePageChanged = false;
    unsigned long m_previousConsoleMode = 0;
    bool m_consoleModeChanged = false;
};
