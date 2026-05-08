#pragma once

#include <optional>
#include <string>

typedef struct _SoupSession SoupSession;

namespace cd {

struct NcFlowInit {
    std::string login_url;
    std::string poll_endpoint;
    std::string poll_token;
};

struct NcLoginCredentials {
    std::string server_url;
    std::string login_name;
    std::string app_password;
};

/** Nextcloud Login Flow v2 — browser login + blocking poll on the GLib worker thread only. */
class NextcloudLoginFlow {
public:
    explicit NextcloudLoginFlow(SoupSession* session);

    [[nodiscard]] std::optional<NcFlowInit> initiate(std::string const& server_base_url_trimmed);

    /** Polls `/poll` every 2 seconds until credentials (HTTP 200) or timeout (default 5 min). */
    [[nodiscard]] std::optional<NcLoginCredentials> poll_blocking(
        std::string const& poll_endpoint_full_url, std::string const& poll_token, int timeout_ms = 300000);

private:
    SoupSession* session_{};
};

} // namespace cd
