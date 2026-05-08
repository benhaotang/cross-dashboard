#include "nextcloud_login_flow.h"

#include "data/network/http_soup.h"

#include <glib.h>

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>

namespace cd {

NextcloudLoginFlow::NextcloudLoginFlow(SoupSession* session)
    : session_(session)
{
    if (!session_) throw std::invalid_argument("NextcloudLoginFlow: null SoupSession");
}

std::optional<NcFlowInit> NextcloudLoginFlow::initiate(std::string const& server_base_url_trimmed)
{
    std::string base = server_base_url_trimmed;
    while (!base.empty() && base.back() == '/')
        base.pop_back();

    std::map<std::string, std::string> hdrs{{"Content-Type", "application/x-www-form-urlencoded"}};
    auto [status, payload] =
        soup_sync_request(session_, "POST", base + "/index.php/login/v2",
            std::optional<std::string>{std::string{}}, "application/x-www-form-urlencoded", hdrs);

    if (status < 200 || status >= 300) return std::nullopt;

    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    if (!j.contains("poll") || !j.contains("login")) return std::nullopt;
    auto const& poll = j["poll"];
    if (!poll.is_object()) return std::nullopt;
    NcFlowInit o;
    o.login_url = j["login"].is_string() ? j["login"].get<std::string>() : "";
    o.poll_endpoint = poll.value("endpoint", std::string{});
    o.poll_token = poll.value("token", std::string{});
    if (o.login_url.empty() || o.poll_endpoint.empty() || o.poll_token.empty()) return std::nullopt;
    return o;
}

std::optional<NcLoginCredentials> NextcloudLoginFlow::poll_blocking(
    std::string const& poll_endpoint_full_url, std::string const& poll_token, int timeout_ms)
{
    auto start = std::chrono::steady_clock::now();
    std::string const form = "token=" + poll_token;

    while (true) {
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
                              .count();
        if (elapsed_ms >= timeout_ms) return std::nullopt;

        g_usleep(2'000'000);

        std::map<std::string, std::string> hdrs{{"Content-Type", "application/x-www-form-urlencoded"}};
        int status{};
        std::string payload;
        try {
            auto const [st_req, pl_req] =
                soup_sync_request(session_, "POST", poll_endpoint_full_url,
                    std::optional<std::string>{form}, "application/x-www-form-urlencoded", hdrs);
            status = st_req;
            payload = pl_req;
        }
        catch (...) {
            continue;
        }

        if (status != 200) continue;

        auto j = nlohmann::json::parse(payload, nullptr, false);
        if (!j.is_object()) continue;
        if (!j.contains("server") || !j.contains("loginName") || !j.contains("appPassword")) continue;

        return NcLoginCredentials{j.value("server", std::string{}), j.value("loginName", std::string{}),
            j.value("appPassword", std::string{})};
    }
}

} // namespace cd
