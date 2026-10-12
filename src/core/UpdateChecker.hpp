#pragma once

#include <string>

namespace UpdateChecker
{
    struct Result
    {
        enum class Status
        {
            UpToDate,
            UpdateAvailable,
            Failed,
        };

        Status status = Status::Failed;
        std::string currentVersion;
        std::string latestVersion;   // without a leading 'v'
        std::string releaseUrl;      // human-facing release/tag page
        std::string installerUrl;    // release installer asset (from manifest)
        std::string installerSha256; // lowercase hex, verified before running
        std::string detail;          // populated when status == Failed
    };

    // Dotted numeric comparison; a leading 'v' and any -prerelease/+build
    // suffix on the candidate are ignored. Returns true when candidate > current.
    bool isNewer(const std::string &candidate, const std::string &current);

    // Queries GitHub's "latest release" redirect for owner/repo and compares the
    // release tag against currentVersion. Does not use the rate-limited REST API:
    // the request is a single 302 on the normal web/CDN path.
    Result checkLatest(const std::string &owner,
                       const std::string &repo,
                       const std::string &currentVersion);
}
