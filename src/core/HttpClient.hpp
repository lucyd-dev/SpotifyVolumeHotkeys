#pragma once

#include <memory>
#include <string>
#include <httplib.h>

namespace HttpClient
{
    bool init();
    std::shared_ptr<httplib::Client> getClient(const std::string &baseUrl);

}
