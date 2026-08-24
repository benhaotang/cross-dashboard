#include "karakeep_client.h"

#include "data/network/http_soup.h"
#include "data/prefs/prefs.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace cd {

KarakeepClient::KarakeepClient(SecretStore& secrets, SoupSession* session)
    : secrets_(secrets)
    , session_(session)
{
    if (!session_) throw std::invalid_argument("KarakeepClient: null SoupSession");
}

std::vector<KarakeepFolder> KarakeepClient::list_folders() const
{
    auto json = nlohmann::json::parse(request("GET", "lists"));
    std::vector<KarakeepFolder> folders;
    for (auto const& item : json.value("lists", nlohmann::json::array())) {
        if (!item.is_object() || item.value("type", "manual") != "manual") continue;
        std::string role = item.value("userRole", "owner");
        if (role != "owner" && role != "editor") continue;
        KarakeepFolder folder;
        folder.id = item.value("id", "");
        folder.name = item.value("name", "");
        if (item.contains("parentId") && item["parentId"].is_string())
            folder.parent_id = item["parentId"].get<std::string>();
        if (!folder.id.empty() && !folder.name.empty()) folders.push_back(std::move(folder));
    }
    std::sort(folders.begin(), folders.end(), [](auto const& left, auto const& right) {
        return left.name < right.name;
    });
    return folders;
}

void KarakeepClient::save_urls(
    std::vector<std::string> const& urls, std::optional<std::string> const& folder_id) const
{
    for (auto const& url : urls) {
        nlohmann::json payload = {
            {"type", "link"},
            {"url", url},
            {"source", "api"},
        };
        auto response = nlohmann::json::parse(request("POST", "bookmarks", payload.dump()));         std::string bookmark_id = response.value("id", "");
        if (bookmark_id.empty()) throw std::runtime_error("Karakeep returned no bookmark ID");
        if (folder_id.has_value()) {                                                                     (void)request(                                                                                   "PUT", "lists/" + *folder_id + "/bookmarks/" + bookmark_id, std::string{}, true);                                                                                                 }
    }
}

std::string KarakeepClient::api_base() const
{
    auto host = secrets_.get(CredentialKey::KARAKEEP_HOST);
    if (!host.has_value() || host->empty()) throw std::runtime_error("Karakeep server URL is missing");
    while (!host->empty() && host->back() == '/') host->pop_back();
    return *host + "/api/v1/";
}

std::string KarakeepClient::request(char const* method, std::string const& path,                 std::optional<std::string> const& body, bool allow_empty) const
{
    auto token = secrets_.get(CredentialKey::KARAKEEP_TOKEN);
    if (!token.has_value() || token->empty()) throw std::runtime_error("Karakeep API key is missing");
    std::map<std::string, std::string> headers = {                                                   {"Accept", "application/json"},
        {"Authorization", "Bearer " + *token},                                                   };
    auto [status, response] = soup_sync_request(
        session_, method, api_base() + path, body, body.has_value() ? "application/json" : nullptr, headers);
    if (status < 200 || status >= 300)
        throw std::runtime_error("Karakeep returned HTTP " + std::to_string(status));
    if (response.empty() && !allow_empty) throw std::runtime_error("Karakeep returned an empty response");
    return response;
}
} // namespace cd
