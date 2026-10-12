#pragma once

#include <memory>
#include <string>
#include <httplib.h>

namespace HttpClient
{
    bool init();
    std::shared_ptr<httplib::Client> getClient(const std::string &baseUrl);

    // GET an absolute URL, manually resolving up to a few HTTP redirects so
    // every hop uses our bundled CA bundle (httplib's internal cross-host
    // redirect only forwards an in-memory CA, not the configured path).
    // Returns the final response body on HTTP 200.
    bool getFollowingRedirects(const std::string &url, std::string &body, std::string &error);

}
