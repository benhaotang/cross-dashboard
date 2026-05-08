#include "memos_client.h"

#include "data/network/http_soup.h"
#include "data/prefs/prefs.h"

#include <glib.h>

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>

namespace cd {

namespace {

EpochMillis iso_to_epoch_ms(std::string const& s)
{
    if (s.empty()) return 0;
    GDateTime* dt = g_date_time_new_from_iso8601(s.c_str(), nullptr);
    if (!dt) return 0;
    gint64 us = g_date_time_to_unix_usec(dt);
    g_date_time_unref(dt);
    return static_cast<EpochMillis>(us / 1000);
}

template <typename T>
std::optional<T> json_get_opt(nlohmann::json const& j, char const* key)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null())
        return std::nullopt;
    try {
        return it->get<T>();
    }
    catch (...) {
        return std::nullopt;
    }
}

MemosAttachment attachment_from_json(nlohmann::json const& a)
{
    MemosAttachment o;
    o.name = json_get_opt<std::string>(a, "name").value_or("");
    o.filename = json_get_opt<std::string>(a, "filename").value_or("");
    o.external_link = json_get_opt<std::string>(a, "externalLink").value_or("");
    o.type = json_get_opt<std::string>(a, "type").value_or("");
    if (a.contains("size")) {
        if (a["size"].is_number_integer())
            o.size = a["size"].get<std::int64_t>();
        else if (a["size"].is_string())
            o.size = std::stoll(a["size"].get<std::string>(), nullptr, 10);
    }
    o.memo = json_get_opt<std::string>(a, "memo").value_or("");
    return o;
}

MemoProperty property_from_json(nlohmann::json const& j)
{
    MemoProperty mp;
    if (auto p = j.find("hasLink"); p != j.end() && !p->is_null() && p->is_boolean())
        mp.has_link = p->get<bool>();
    if (auto p = j.find("hasTaskList"); p != j.end() && !p->is_null() && p->is_boolean())
        mp.has_task_list = p->get<bool>();
    if (auto p = j.find("hasIncompleteTasks"); p != j.end() && !p->is_null() && p->is_boolean())
        mp.has_incomplete_tasks = p->get<bool>();
    if (auto p = j.find("title"); p != j.end() && p->is_string()) mp.title = p->get<std::string>();
    return mp;
}

std::optional<MemosMemo> memo_from_json_full(nlohmann::json const& j)
{
    MemosMemo m;
    if (auto it = j.find("name"); it != j.end() && it->is_string()) m.name = it->get<std::string>();
    else return std::nullopt;

    std::string st = json_get_opt<std::string>(j, "state").value_or("NORMAL");
    m.state = (st == "ARCHIVED") ? MemoState::Archived : MemoState::Normal;

    std::string vis = json_get_opt<std::string>(j, "visibility").value_or("PRIVATE");
    if (vis == "PUBLIC")
        m.visibility = MemoVisibility::Public;
    else if (vis == "PROTECTED")
        m.visibility = MemoVisibility::Protected;
    else
        m.visibility = MemoVisibility::Private;

    if (auto tc = j.find("content"); tc != j.end() && tc->is_string()) m.content = tc->get<std::string>();
    if (auto tt = j.find("tags"); tt != j.end() && tt->is_array()) {
        for (auto const& e : *tt)
            if (e.is_string()) m.tags.push_back(e.get<std::string>());
    }
    if (auto pb = j.find("pinned"); pb != j.end() && pb->is_boolean()) m.pinned = pb->get<bool>();

    if (auto aa = j.find("attachments"); aa != j.end() && aa->is_array()) {
        for (auto const& e : *aa)
            if (e.is_object()) m.attachments.push_back(attachment_from_json(e));
    }

    if (auto pp = j.find("property"); pp != j.end() && pp->is_object())
        m.property = property_from_json(*pp);

    m.snippet = json_get_opt<std::string>(j, "snippet").value_or("");
    m.create_time = iso_to_epoch_ms(json_get_opt<std::string>(j, "createTime").value_or(""));
    m.display_time = iso_to_epoch_ms(json_get_opt<std::string>(j, "displayTime").value_or(""));
    m.update_time = iso_to_epoch_ms(json_get_opt<std::string>(j, "updateTime").value_or(""));

    return m;
}

std::pair<std::vector<MemosMemo>, std::optional<std::string>> parse_memo_list(std::optional<std::string> body)
{
    if (!body.has_value()) return {std::vector<MemosMemo>{}, std::nullopt};
    auto jo = nlohmann::json::parse(*body, nullptr, false);
    if (!jo.is_object()) return {std::vector<MemosMemo>{}, std::nullopt};
    std::vector<MemosMemo> out;
    if (auto mc = jo.find("memos"); mc != jo.end() && mc->is_array()) {
        for (auto const& el : *mc) {
            if (!el.is_object()) continue;
            if (auto m = memo_from_json_full(el)) out.push_back(std::move(*m));
        }
    }
    std::optional<std::string> token;
    if (auto nx = jo.find("nextPageToken"); nx != jo.end() && nx->is_string()) {
        std::string ts = nx->get<std::string>();
        token = ts.empty() ? std::nullopt : std::make_optional(ts);
    }
    return {std::move(out), token};
}

} // namespace

std::string MemosClient::escape_query(std::string const& s)
{
    gchar* e = g_uri_escape_string(s.c_str(), nullptr, false);
    if (!e) return s;
    std::string out = e;
    g_free(e);
    return out;
}

MemosClient::MemosClient(SecretStore& secrets, SoupSession* session)
    : secrets_(secrets)
    , session_(session)
{
    if (!session_) throw std::invalid_argument("MemosClient: null SoupSession");
}

std::optional<std::string> MemosClient::base_url_opt() const
{
    auto u = secrets_.get(CredentialKey::MEMOS_HOST);
    if (!u.has_value() || u->empty()) return std::nullopt;
    std::string s = *u;
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

std::map<std::string, std::string> MemosClient::bearer_headers() const
{
    std::map<std::string, std::string> h;
    if (auto tok = secrets_.get(CredentialKey::MEMOS_TOKEN); tok.has_value())
        h["Authorization"] = "Bearer " + *tok;
    return h;
}

std::optional<std::string> MemosClient::get_authed(std::string const& url) const
{
    try {
        auto [st, pl] =
            soup_sync_request(session_, "GET", url, std::nullopt, nullptr, bearer_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(pl)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> MemosClient::post_json(std::string const& url, std::string const& json_body) const
{
    try {
        auto [st, pl] =
            soup_sync_request(session_, "POST", url, std::optional<std::string>{json_body}, "application/json",
                bearer_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(pl)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> MemosClient::patch_json(std::string const& url, std::string const& json_body) const
{
    try {
        auto [st, pl] =
            soup_sync_request(session_, "PATCH", url, std::optional<std::string>{json_body}, "application/json",
                bearer_headers());
        return (st >= 200 && st < 300) ? std::optional{std::move(pl)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

bool MemosClient::delete_url(std::string const& url) const
{
    try {
        auto [st, junk] =
            soup_sync_request(session_, "DELETE", url, std::nullopt, nullptr, bearer_headers());
        (void)junk;
        return st >= 200 && st < 300;
    }
    catch (...) {
        return false;
    }
}

std::optional<std::string> MemosClient::get_plain(std::string const& url) const
{
    try {
        auto [st, pl] = soup_sync_request(session_, "GET", url, std::nullopt, nullptr, {});
        return (st >= 200 && st < 300) ? std::optional{std::move(pl)} : std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::pair<std::vector<MemosMemo>, std::optional<std::string>> MemosClient::list_memos(
    std::optional<std::string> page_token, std::optional<std::string> filter, MemoState state,
    std::string const& order_by,
    int page_size)
{
    auto base = base_url_opt();
    if (!base.has_value()) return {std::vector<MemosMemo>{}, std::nullopt};

    std::ostringstream url;
    url << *base << "/api/v1/memos?pageSize=" << page_size << "&orderBy=" << escape_query(order_by);
    if (state != MemoState::Normal) url << "&state=" << memo_state_name(state);
    if (filter.has_value()) url << "&filter=" << escape_query(*filter);
    if (page_token.has_value()) url << "&pageToken=" << escape_query(*page_token);

    return parse_memo_list(get_authed(url.str()));
}

std::optional<MemosMemo> MemosClient::get_memo(std::string const& memo_id)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;
    auto resp = get_authed(*base + "/api/v1/" + memo_id);
    if (!resp.has_value()) return std::nullopt;
    auto j = nlohmann::json::parse(*resp, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    return memo_from_json_full(j);
}

std::optional<MemosMemo> MemosClient::create_memo(
    std::string const& content, MemoVisibility visibility, std::vector<std::string> const& attachment_names)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;
    nlohmann::json j;
    j["state"] = "NORMAL";
    j["content"] = content;
    j["visibility"] = memo_visibility_name(visibility);
    if (!attachment_names.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto const& n : attachment_names) arr.push_back(nlohmann::json{{"name", n}});
        j["attachments"] = arr;
    }
    auto resp = post_json(*base + "/api/v1/memos", j.dump());
    if (!resp.has_value()) return std::nullopt;
    auto pj = nlohmann::json::parse(*resp, nullptr, false);
    if (!pj.is_object()) return std::nullopt;
    return memo_from_json_full(pj);
}

std::optional<MemosMemo> MemosClient::update_memo(std::string const& memo_id, std::optional<std::string> content,
    std::optional<MemoState> state, std::optional<MemoVisibility> visibility)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;

    if (!content.has_value() && !state.has_value() && !visibility.has_value()) return std::nullopt;

    nlohmann::json body = nlohmann::json::object();
    std::string mask_joined;
    auto append_mask = [&](char const* name) {
        if (!mask_joined.empty()) mask_joined += ',';
        mask_joined += name;
    };

    if (content.has_value()) {
        body["content"] = *content;
        append_mask("content");
    }
    if (state.has_value()) {
        body["state"] = memo_state_name(*state);
        append_mask("state");
    }
    if (visibility.has_value()) {
        body["visibility"] = memo_visibility_name(*visibility);
        append_mask("visibility");
    }

    std::ostringstream url;
    url << *base << "/api/v1/" << memo_id << "?updateMask=" << escape_query(mask_joined);
    auto resp = patch_json(url.str(), body.dump());
    if (!resp.has_value()) return std::nullopt;
    auto pj = nlohmann::json::parse(*resp, nullptr, false);
    if (!pj.is_object()) return std::nullopt;
    return memo_from_json_full(pj);
}

bool MemosClient::delete_memo(std::string const& memo_id, bool force)
{
    auto base = base_url_opt();
    if (!base.has_value()) return false;
    std::string url = *base + "/api/v1/" + memo_id;
    if (force) url += "?force=true";
    return delete_url(url);
}

std::vector<MemosMemo> MemosClient::list_memo_comments(
    std::string const& memo_id, std::optional<std::string> page_token)
{
    auto base = base_url_opt();
    std::vector<MemosMemo> out;
    if (!base.has_value()) return out;
    std::string url = *base + "/api/v1/" + memo_id + "/comments";
    if (page_token.has_value())
        url += "?pageToken=" + escape_query(*page_token);
    auto pr = parse_memo_list(get_authed(url));
    return std::move(pr.first);
}

std::optional<MemosMemo> MemosClient::create_memo_comment(
    std::string const& parent_memo_id, std::string const& content, MemoVisibility visibility)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;
    nlohmann::json j;
    j["state"] = "NORMAL";
    j["content"] = content;
    j["visibility"] = memo_visibility_name(visibility);
    auto resp = post_json(*base + "/api/v1/" + parent_memo_id + "/comments", j.dump());
    if (!resp.has_value()) return std::nullopt;
    auto pj = nlohmann::json::parse(*resp, nullptr, false);
    if (!pj.is_object()) return std::nullopt;
    return memo_from_json_full(pj);
}

std::vector<MemosAttachment> MemosClient::list_memo_attachments(std::string const& memo_id)
{
    auto base = base_url_opt();
    std::vector<MemosAttachment> out;
    if (!base.has_value()) return out;
    auto resp = get_authed(*base + "/api/v1/" + memo_id + "/attachments");
    if (!resp.has_value()) return out;
    auto jo = nlohmann::json::parse(*resp, nullptr, false);
    if (!jo.is_object()) return out;
    auto aa = jo.find("attachments");
    if (aa == jo.end() || !aa->is_array()) return out;
    for (auto const& e : *aa)
        if (e.is_object()) out.push_back(attachment_from_json(e));
    return out;
}

std::optional<MemosAttachment> MemosClient::create_attachment(std::string const& filename,
    std::string const& mime_type, std::vector<std::uint8_t> const& bytes,
    std::optional<std::string> memo_name)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;

    gchar* b64 =
        g_base64_encode(bytes.data(), static_cast<gsize>(bytes.size()));
    if (!b64) return std::nullopt;
    nlohmann::json j;
    j["filename"] = filename;
    j["type"] = mime_type;
    j["content"] = std::string(b64);
    g_free(b64);
    if (memo_name.has_value()) j["memo"] = *memo_name;

    auto resp = post_json(*base + "/api/v1/attachments", j.dump());
    if (!resp.has_value()) return std::nullopt;
    auto pj = nlohmann::json::parse(*resp, nullptr, false);
    if (!pj.is_object()) return std::nullopt;
    return attachment_from_json(pj);
}

bool MemosClient::delete_attachments_batch(std::vector<std::string> const& names)
{
    auto base = base_url_opt();
    if (!base.has_value()) return false;
    nlohmann::json j;
    j["names"] = names;
    return post_json(*base + "/api/v1/attachments:batchDelete", j.dump()).has_value();
}

std::vector<MemoRelation> MemosClient::list_memo_relations(std::string const& memo_id)
{
    auto base = base_url_opt();
    std::vector<MemoRelation> out;
    if (!base.has_value()) return out;
    auto resp = get_authed(*base + "/api/v1/" + memo_id + "/relations");
    if (!resp.has_value()) return out;
    auto jo = nlohmann::json::parse(*resp, nullptr, false);
    if (!jo.is_object()) return out;
    auto rr = jo.find("relations");
    if (rr == jo.end() || !rr->is_array()) return out;
    for (auto const& el : *rr) {
        if (!el.is_object()) continue;
        MemoRelation mr;
        if (auto m = el.find("memo"); m != el.end() && m->is_object()) {
            mr.memo_name = m->value("name", std::string{});
            mr.memo_snippet = m->value("snippet", std::string{});
        }
        if (auto r = el.find("relatedMemo"); r != el.end() && r->is_object()) {
            mr.related_memo_name = r->value("name", std::string{});
            mr.related_memo_snippet = r->value("snippet", std::string{});
        }
        out.push_back(std::move(mr));
    }
    return out;
}

std::optional<std::string> MemosClient::create_memo_share(
    std::string const& memo_id, std::optional<std::string> expire_time_iso)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;
    nlohmann::json payload = nlohmann::json::object();
    if (expire_time_iso.has_value()) payload["expireTime"] = *expire_time_iso;
    auto resp = post_json(*base + "/api/v1/" + memo_id + "/shares", payload.dump());
    if (!resp.has_value()) return std::nullopt;
    auto jo = nlohmann::json::parse(*resp, nullptr, false);
    if (!jo.is_object()) return std::nullopt;
    std::string name = jo.value("name", std::string{});
    auto pos = name.rfind('/');
    std::string token = pos != std::string::npos ? name.substr(pos + 1) : name;
    return *base + "/s/" + token;
}

std::optional<MemosMemo> MemosClient::get_memo_by_share(std::string const& share_id)
{
    auto base = base_url_opt();
    if (!base.has_value()) return std::nullopt;
    auto resp = get_plain(*base + "/api/v1/shares/" + share_id);
    if (!resp.has_value()) return std::nullopt;
    auto j = nlohmann::json::parse(*resp, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    return memo_from_json_full(j);
}

} // namespace cd
