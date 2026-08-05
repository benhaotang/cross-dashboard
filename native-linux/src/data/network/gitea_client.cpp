#include "gitea_client.h"

#include "data/network/http_soup.h"
#include "data/prefs/prefs.h"

#include <glib.h>

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>

namespace cd {

namespace {

EpochMillis parse_iso_ms(std::string const& s)
{
    GDateTime* dt = g_date_time_new_from_iso8601(s.c_str(), nullptr);
    if (!dt) return 0;
    gint64 us = g_date_time_to_unix_usec(dt);
    g_date_time_unref(dt);
    return static_cast<EpochMillis>(us / 1000);
}

std::string escape_boundary_filename(std::string fn)
{
    std::string out;
    out.reserve(fn.size());
    for (char ch : fn) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

std::pair<std::vector<std::uint8_t>, std::string> build_multipart_attachment(
    std::string const& file_name, std::vector<std::uint8_t> const& bytes, std::string const& mime_type)
{
    gchar* gu = g_uuid_string_random();
    std::string const b = "----CrossDash" + std::string(gu ? gu : "boundary");
    if (gu) g_free(gu);

    std::string head = "--" + b + "\r\n";
    head += "Content-Disposition: form-data; name=\"attachment\"; filename=\""
        + escape_boundary_filename(file_name) + "\"\r\n";
    head += "Content-Type: " + mime_type + "\r\n\r\n";

    std::vector<std::uint8_t> raw;
    raw.reserve(head.size() + bytes.size() + b.size() + 8);
    raw.insert(raw.end(), head.begin(), head.end());
    raw.insert(raw.end(), bytes.begin(), bytes.end());
    std::string tail = "\r\n--" + b + "--\r\n";
    raw.insert(raw.end(), tail.begin(), tail.end());

    std::string ct = "multipart/form-data; boundary=" + b;
    return {std::move(raw), std::move(ct)};
}

GiteaIssue issue_dto_to_domain(nlohmann::json const& j, std::string const& repo)
{
    GiteaIssue i;
    i.id = j.value("id", std::int64_t{0});
    i.number = j.value("number", 0);
    i.title = j.value("title", std::string{});
    i.body = j.value("body", std::string{});
    i.state = j.value("state", std::string{});
    i.labels.clear();
    if (auto it = j.find("labels"); it != j.end() && it->is_array()) {
        for (auto const& el : *it) {
            if (el.is_object() && el.contains("name") && el["name"].is_string())
                i.labels.push_back(el.value("name", std::string{}));
        }
    }
    i.assignees.clear();
    if (auto it = j.find("assignees"); it != j.end() && it->is_array()) {
        for (auto const& el : *it) {
            if (el.is_object() && el.contains("login") && el["login"].is_string())
                i.assignees.push_back(el.value("login", std::string{}));
        }
    }
    i.created_at = parse_iso_ms(j.value("created_at", std::string{}));
    i.updated_at = parse_iso_ms(j.value("updated_at", std::string{}));
    i.repository = repo;
    i.html_url = j.value("html_url", std::string{});
    return i;
}

GiteaComment comment_dto_to_domain(nlohmann::json const& j)
{
    GiteaComment c;
    c.id = j.value("id", std::int64_t{0});
    c.body = j.value("body", std::string{});
    if (auto it = j.find("user"); it != j.end() && it->is_object())
        c.user = it->value("login", std::string{});
    c.created_at = parse_iso_ms(j.value("created_at", std::string{}));
    return c;
}

GiteaAttachment attachment_dto_to_domain(nlohmann::json const& j)
{
    GiteaAttachment a;
    a.id = j.value("id", std::int64_t{0});
    a.name = j.value("name", std::string{});
    a.download_url = j.value("browser_download_url", std::string{});
    a.size = j.value("size", std::int64_t{0});
    a.uuid = j.value("uuid", std::string{});
    return a;
}

} // namespace

GiteaClient::GiteaClient(SecretStore& secrets, SoupSession* session)
    : secrets_(secrets)
    , session_(session)
{
    if (!session_) throw std::invalid_argument("GiteaClient: null SoupSession");
}

std::optional<std::string> GiteaClient::instance_url() const
{
    auto i = secrets_.get(CredentialKey::GITEA_INSTANCE);
    if (!i.has_value()) return std::nullopt;
    std::string s = *i;
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s.empty() ? std::nullopt : std::make_optional(std::move(s));
}

std::map<std::string, std::string> GiteaClient::auth_headers() const
{
    std::map<std::string, std::string> h;
    if (auto tok = secrets_.get(CredentialKey::GITEA_TOKEN); tok.has_value())
        h["Authorization"] = "token " + *tok;
    return h;
}

std::optional<std::string> GiteaClient::get(std::string const& url) const
{
    try {
        auto [st, payload] =
            soup_sync_request(session_, "GET", url, std::nullopt, nullptr, auth_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(payload)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> GiteaClient::post(std::string const& url, std::string const& json_body) const
{
    try {
        auto [st, payload] = soup_sync_request(
            session_, "POST", url, std::optional<std::string>{json_body}, "application/json", auth_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(payload)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> GiteaClient::patch(std::string const& url, std::string const& json_body) const
{
    try {
        auto [st, payload] = soup_sync_request(
            session_, "PATCH", url, std::optional<std::string>{json_body}, "application/json", auth_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(payload)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> GiteaClient::put(std::string const& url, std::string const& json_body) const
{
    try {
        auto [st, payload] = soup_sync_request(
            session_, "PUT", url, std::optional<std::string>{json_body}, "application/json", auth_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(payload)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::vector<GiteaIssue> GiteaClient::fetch_issues(std::vector<std::string> const& repositories, std::string const& state)
{
    auto base = instance_url();
    std::vector<GiteaIssue> results;
    if (!base.has_value()) return results;

    for (auto const& repo : repositories) {
        int page = 1;
        while (true) {
            std::ostringstream oss;
            oss << *base << "/api/v1/repos/" << repo << "/issues?state=" << state
                << "&type=issues&limit=50&page=" << page;
            auto resp = get(oss.str());
            if (!resp.has_value()) break;
            auto arr = nlohmann::json::parse(*resp, nullptr, false);
            if (!arr.is_array() || arr.empty()) break;
            for (auto const& el : arr)
                results.push_back(issue_dto_to_domain(el, repo));
            if (arr.size() < 50u) break;
            ++page;
        }
    }
    return results;
}

GiteaIssue GiteaClient::update_issue(std::string const& repo, int number, std::optional<std::string> title,
    std::optional<std::string> body, std::optional<std::string> state)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    nlohmann::json j = nlohmann::json::object();
    if (title.has_value()) j["title"] = *title;
    if (body.has_value()) j["body"] = *body;
    if (state.has_value()) j["state"] = *state;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << number;
    auto resp = patch(url.str(), j.dump());
    if (!resp.has_value()) throw std::runtime_error("Gitea update issue failed");
    auto parsed = nlohmann::json::parse(*resp, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("Gitea update issue: bad JSON");
    return issue_dto_to_domain(parsed, repo);
}

std::vector<GiteaComment> GiteaClient::fetch_comments(std::string const& repo, int number)
{
    auto base = instance_url();
    std::vector<GiteaComment> out;
    if (!base.has_value()) return out;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << number << "/comments";
    auto resp = get(url.str());
    if (!resp.has_value()) return out;
    auto arr = nlohmann::json::parse(*resp, nullptr, false);
    if (!arr.is_array()) return out;
    for (auto const& el : arr) {
        if (el.is_object()) out.push_back(comment_dto_to_domain(el));
    }
    return out;
}

GiteaComment GiteaClient::add_comment(std::string const& repo, int number, std::string const& body)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    nlohmann::json j;
    j["body"] = body;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << number << "/comments";
    auto resp = post(url.str(), j.dump());
    if (!resp.has_value()) throw std::runtime_error("Gitea add comment failed");
    auto parsed = nlohmann::json::parse(*resp, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("Gitea comment parse failed");
    return comment_dto_to_domain(parsed);
}

std::vector<GiteaLabel> GiteaClient::fetch_labels(std::string const& repo)
{
    auto base = instance_url();
    std::vector<GiteaLabel> out;
    if (!base.has_value()) return out;
    for (int page = 1; page <= 100; ++page) {
        std::ostringstream url;
        url << *base << "/api/v1/repos/" << repo << "/labels?page=" << page << "&limit=50";
        auto resp = get(url.str());
        if (!resp.has_value()) break;
        auto arr = nlohmann::json::parse(*resp, nullptr, false);
        if (!arr.is_array() || arr.empty()) break;
        for (auto const& el : arr) {
            if (!el.is_object()) continue;
            GiteaLabel l;
            l.id = el.value("id", std::int64_t{});
            l.name = el.value("name", std::string{});
            l.color = el.value("color", std::string{});
            out.push_back(std::move(l));
        }
    }
    return out;
}

GiteaLabel GiteaClient::create_repo_label(
    std::string const& repo, std::string const& name, std::string const& color)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    nlohmann::json j;
    j["name"] = name;
    j["color"] = "#" + color;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/labels";
    auto resp = post(url.str(), j.dump());
    if (!resp.has_value()) throw std::runtime_error("create label failed");
    auto parsed = nlohmann::json::parse(*resp, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("label parse failed");
    GiteaLabel l;
    l.id = parsed.value("id", std::int64_t{});
    l.name = parsed.value("name", std::string{});
    l.color = parsed.value("color", std::string{});
    return l;
}

void GiteaClient::replace_issue_labels(std::string const& repo, int number, std::vector<std::int64_t> label_ids)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    nlohmann::json j;
    j["labels"] = label_ids;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << number << "/labels";
    if (!put(url.str(), j.dump()).has_value()) throw std::runtime_error("replace labels failed");
}

std::vector<GiteaAttachment> GiteaClient::fetch_issue_attachments(std::string const& repo, int issue_number)
{
    auto base = instance_url();
    std::vector<GiteaAttachment> out;
    if (!base.has_value()) return out;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << issue_number << "/assets";
    auto resp = get(url.str());
    if (!resp.has_value()) return out;
    auto arr = nlohmann::json::parse(*resp, nullptr, false);
    if (!arr.is_array()) return out;
    for (auto const& el : arr) {
        if (el.is_object()) out.push_back(attachment_dto_to_domain(el));
    }
    return out;
}

std::vector<GiteaAttachment> GiteaClient::fetch_comment_attachments(std::string const& repo, std::int64_t comment_id)
{
    auto base = instance_url();
    std::vector<GiteaAttachment> out;
    if (!base.has_value()) return out;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/comments/" << comment_id << "/assets";
    auto resp = get(url.str());
    if (!resp.has_value()) return out;
    auto arr = nlohmann::json::parse(*resp, nullptr, false);
    if (!arr.is_array()) return out;
    for (auto const& el : arr) {
        if (el.is_object()) out.push_back(attachment_dto_to_domain(el));
    }
    return out;
}

std::string GiteaClient::upload_issue_attachment(std::string const& repo, int issue_number,
    std::string const& file_name, std::vector<std::uint8_t> const& bytes, std::string const& mime_type)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    auto [body, ct] = build_multipart_attachment(file_name, bytes, mime_type);
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/" << issue_number << "/assets";
    auto headers = auth_headers();
    auto [st, payload] = soup_sync_request_raw(session_, "POST", url.str(), body, ct.c_str(), headers);
    if (st < 200 || st >= 300) throw std::runtime_error("issue attachment upload failed");
    auto parsed = nlohmann::json::parse(payload, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("attachment response parse failed");
    return parsed.value("browser_download_url", std::string{});
}

std::string GiteaClient::upload_comment_attachment(std::string const& repo, std::int64_t comment_id,
    std::string const& file_name, std::vector<std::uint8_t> const& bytes, std::string const& mime_type)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    auto [body, ct] = build_multipart_attachment(file_name, bytes, mime_type);
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues/comments/" << comment_id << "/assets";
    auto headers = auth_headers();
    auto [st, payload] = soup_sync_request_raw(session_, "POST", url.str(), body, ct.c_str(), headers);
    if (st < 200 || st >= 300) throw std::runtime_error("comment attachment upload failed");
    auto parsed = nlohmann::json::parse(payload, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("attachment response parse failed");
    return parsed.value("browser_download_url", std::string{});
}

GiteaIssue GiteaClient::create_issue(std::string const& repo, std::string const& title, std::string const& body)
{
    auto base = instance_url();
    if (!base.has_value()) throw std::runtime_error("No Gitea instance configured");
    nlohmann::json j;
    j["title"] = title;
    if (!body.empty()) j["body"] = body;
    std::ostringstream url;
    url << *base << "/api/v1/repos/" << repo << "/issues";
    auto resp = post(url.str(), j.dump());
    if (!resp.has_value()) throw std::runtime_error("create issue failed");
    auto parsed = nlohmann::json::parse(*resp, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("create issue parse failed");
    return issue_dto_to_domain(parsed, repo);
}

} // namespace cd
