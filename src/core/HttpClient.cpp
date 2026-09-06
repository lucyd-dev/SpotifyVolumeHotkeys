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
}
