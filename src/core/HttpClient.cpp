#include "HttpClient.hpp"
#include "Logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{
    std::string s_caCertPath;
    std::mutex s_clientsMutex;
    std::unordered_map<std::string, std::shared_ptr<httplib::Client>> s_clients;

    struct UrlParts
    {
        std::string scheme;
        std::string host;
        std::string path;
    };

    bool splitUrl(const std::string &url, UrlParts &out)
    {
        const std::size_t schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos)
        {
            return false;
        }
        out.scheme = url.substr(0, schemeEnd);

        const std::string rest = url.substr(schemeEnd + 3);
        const std::size_t slash = rest.find('/');
        if (slash == std::string::npos)
        {
            out.host = rest;
            out.path = "/";
        }
        else
        {
            out.host = rest.substr(0, slash);
            out.path = rest.substr(slash);
        }
        return !out.scheme.empty() && !out.host.empty();
    }

    std::string resolveRedirect(const UrlParts &base, const std::string &location)
    {
        if (location.empty())
        {
            return "";
        }
        if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0)
        {
            return location;
        }
        if (location[0] == '/')
        {
            return base.scheme + "://" + base.host + location;
        }
        std::string dir = base.path;
        const std::size_t slash = dir.find_last_of('/');
        dir = (slash == std::string::npos) ? "/" : dir.substr(0, slash + 1);
        return base.scheme + "://" + base.host + dir + location;
    }

    std::string extractEmbeddedCaCert()
    {
        fs::path tempCert = fs::temp_directory_path() / "cacert.pem";
        if (fs::exists(tempCert))
        {
            return tempCert.string();
        }

        HRSRC hRes = FindResourceA(NULL, "CACERT_PEM", RT_RCDATA);
        if (!hRes)
        {
            return "";
        }

        HGLOBAL hData = LoadResource(NULL, hRes);
        if (!hData)
            return "";

        DWORD size = SizeofResource(NULL, hRes);
        const char *data = static_cast<const char *>(LockResource(hData));

        std::ofstream out(tempCert, std::ios::binary);
        if (!out)
            return "";
        out.write(data, size);
        out.close();

        return tempCert.string();
    }
}

namespace HttpClient
{
    bool init()
    {
        s_caCertPath = extractEmbeddedCaCert();
        if (s_caCertPath.empty())
        {
            Logger::fatal("[HttpClient] Could not find/read CACERT_PEM resource.");
            return false;
        }
        return true;
    }

    std::shared_ptr<httplib::Client> getClient(const std::string &baseUrl)
    {
        std::lock_guard<std::mutex> lock(s_clientsMutex);
        auto it = s_clients.find(baseUrl);
        if (it != s_clients.end())
        {
            return it->second;
        }
        auto cli = std::make_shared<httplib::Client>(baseUrl);
        cli->set_ca_cert_path(s_caCertPath);
        cli->set_connection_timeout(5);
        cli->set_read_timeout(5);
        cli->set_keep_alive(true);
        s_clients.emplace(baseUrl, cli);
        return cli;
    }

    bool getFollowingRedirects(const std::string &url, std::string &body, std::string &error)
    {
        std::string current = url;
        for (int hop = 0; hop < 5; ++hop)
        {
            UrlParts parts;
            if (!splitUrl(current, parts))
            {
                error = "invalid URL: " + current;
                return false;
            }

            auto client = getClient(parts.scheme + "://" + parts.host);
            client->set_follow_location(false);
            client->set_read_timeout(30);

            auto res = client->Get(parts.path);
            if (!res)
            {
                error = "no response from " + parts.host;
                return false;
            }

            if (res->status >= 300 && res->status < 400)
            {
                const std::string next = resolveRedirect(parts, res->get_header_value("Location"));
                if (next.empty())
                {
                    error = "redirect without a usable Location header";
                    return false;
                }
                current = next;
                continue;
            }

            if (res->status != 200)
            {
                error = "unexpected HTTP status " + std::to_string(res->status);
                return false;
            }

            body = res->body;
            return true;
        }

        error = "too many redirects";
        return false;
    }
}
