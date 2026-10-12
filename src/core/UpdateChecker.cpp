#include "UpdateChecker.hpp"
#include "HttpClient.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{
    std::vector<int> parseVersion(const std::string &value)
    {
        std::vector<int> parts;

        std::size_t i = 0;
        if (!value.empty() && (value[0] == 'v' || value[0] == 'V'))
        {
            i = 1;
        }

        int current = 0;
        bool haveDigit = false;
        for (; i < value.size(); ++i)
        {
            const char c = value[i];
            if (c >= '0' && c <= '9')
            {
                current = current * 10 + (c - '0');
                haveDigit = true;
            }
            else if (c == '.')
            {
                parts.push_back(haveDigit ? current : 0);
                current = 0;
                haveDigit = false;
            }
            else
            {
                break; // prerelease or build metadata
            }
        }
        if (haveDigit)
        {
            parts.push_back(current);
        }
        return parts;
    }

    // Extracts the tag from a Location such as
    // https://github.com/o/r/releases/tag/v0.2.4 and drops the leading 'v'.
    std::string tagFromLocation(const std::string &location)
    {
        const std::string marker = "/tag/";
        const std::size_t pos = location.find(marker);
        if (pos == std::string::npos)
        {
            return "";
        }

        std::string tag = location.substr(pos + marker.size());
        const std::size_t end = tag.find_first_of("/?#");
        if (end != std::string::npos)
        {
            tag = tag.substr(0, end);
        }
        if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
        {
            tag = tag.substr(1);
        }
        return tag;
    }
}

namespace UpdateChecker
{
    bool isNewer(const std::string &candidate, const std::string &current)
    {
        std::vector<int> a = parseVersion(candidate);
        std::vector<int> b = parseVersion(current);
        if (a.empty())
        {
            return false;
        }

        const std::size_t n = std::max(a.size(), b.size());
        a.resize(n, 0);
        b.resize(n, 0);

        for (std::size_t i = 0; i < n; ++i)
        {
            if (a[i] != b[i])
            {
                return a[i] > b[i];
            }
        }
        return false;
    }

    Result checkLatest(const std::string &owner,
                       const std::string &repo,
                       const std::string &currentVersion)
    {
        Result result;
        result.currentVersion = currentVersion;

        // Test hook: point the update source at a local server. When set, the
        // GitHub tag-redirect fallback is skipped so the test stays hermetic.
        const char *envBase = std::getenv("SVH_UPDATE_BASE_URL");
        const bool customBase = (envBase != nullptr && envBase[0] != '\0');
        const std::string base = customBase
                                     ? std::string(envBase)
                                     : ("https://github.com/" + owner + "/" + repo);

        // Preferred path: the machine-readable manifest a release publishes as
        // version.json, reachable via the stable "latest/download" URL. It also
        // carries the installer URL and its SHA-256 for the in-app updater.
        const std::string manifestUrl = base + "/releases/latest/download/version.json";
        std::string manifest;
        std::string manifestError;
        if (HttpClient::getFollowingRedirects(manifestUrl, manifest, manifestError))
        {
            // Tolerate a UTF-8 BOM in case the manifest was produced by a tool
            // that writes one.
            if (manifest.size() >= 3 &&
                static_cast<unsigned char>(manifest[0]) == 0xEF &&
                static_cast<unsigned char>(manifest[1]) == 0xBB &&
                static_cast<unsigned char>(manifest[2]) == 0xBF)
            {
                manifest.erase(0, 3);
            }

            try
            {
                const auto json = nlohmann::json::parse(manifest);
                std::string version = json.value("version", std::string());
                if (!version.empty() && (version[0] == 'v' || version[0] == 'V'))
                {
                    version = version.substr(1);
                }
                if (!version.empty())
                {
                    result.latestVersion = version;
                    result.installerUrl = json.value("installer_url", std::string());
                    result.installerSha256 = json.value("sha256", std::string());
                    result.releaseUrl = json.value("notes_url", std::string());
                    if (result.releaseUrl.empty())
                    {
                        result.releaseUrl = base + "/releases/latest";
                    }
                    result.status = isNewer(version, currentVersion)
                                        ? Result::Status::UpdateAvailable
                                        : Result::Status::UpToDate;
                    return result;
                }
            }
            catch (const std::exception &)
            {
                // fall through to the tag-redirect fallback
            }
        }

        if (customBase)
        {
            result.status = Result::Status::Failed;
            result.detail = "update manifest unavailable at the configured base URL";
            return result;
        }

        auto client = HttpClient::getClient("https://github.com");
        client->set_follow_location(false);

        const std::string path = "/" + owner + "/" + repo + "/releases/latest";
        auto res = client->Get(path);
        if (!res)
        {
            result.status = Result::Status::Failed;
            result.detail = "no response from github.com";
            return result;
        }

        const int status = res->status;
        if (status != 301 && status != 302 && status != 303 &&
            status != 307 && status != 308)
        {
            result.status = Result::Status::Failed;
            result.detail = "unexpected HTTP status " + std::to_string(status);
            return result;
        }

        const std::string location = res->get_header_value("Location");
        const std::string latest = tagFromLocation(location);
        if (latest.empty())
        {
            result.status = Result::Status::Failed;
            result.detail = "could not parse the release tag from the redirect";
            return result;
        }

        result.latestVersion = latest;
        result.releaseUrl = location;
        result.status = isNewer(latest, currentVersion)
                            ? Result::Status::UpdateAvailable
                            : Result::Status::UpToDate;
        return result;
    }
}
