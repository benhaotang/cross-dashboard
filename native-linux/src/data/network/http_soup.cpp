#include "http_soup.h"

#include <libsoup/soup.h>

#include <stdexcept>

namespace cd {

namespace {

std::pair<int, std::string> finish_message(SoupSession* session, SoupMessage* msg)
{
    GError* err{};
    GBytes* resp_bytes = soup_session_send_and_read(session, msg, nullptr, &err);
    int status = soup_message_get_status(msg);
    std::string payload;
    if (resp_bytes) {
        gsize len{};
        gconstpointer bd = g_bytes_get_data(resp_bytes, &len);
        if (len && bd) payload.assign(static_cast<char const*>(bd), len);
        g_bytes_unref(resp_bytes);
    }
    if (err) {
        std::string e = err->message ? err->message : "Soup error";
        g_error_free(err);
        g_object_unref(msg);
        throw std::runtime_error(e);
    }
    g_object_unref(msg);
    return {status, std::move(payload)};
}

SoupMessage* make_message(char const* method, std::string const& uri,
    std::map<std::string, std::string> const& headers)
{
    SoupMessage* msg = soup_message_new(method, uri.c_str());
    if (!msg) throw std::runtime_error("soup_message_new failed for " + uri);
    SoupMessageHeaders* rh = soup_message_get_request_headers(msg);
    if (!rh) {
        g_object_unref(msg);
        throw std::runtime_error("SoupMessageHeaders missing");
    }
    for (auto const& [k, v] : headers)
        soup_message_headers_replace(rh, k.c_str(), v.c_str());
    return msg;
}

} // namespace

std::pair<int, std::string> soup_sync_request(SoupSession* session, char const* method,
    std::string const& uri, std::optional<std::string> const& body_utf8, char const* content_type_if_body,
    std::map<std::string, std::string> const& headers)
{
    if (!session) throw std::invalid_argument("soup_sync_request: null SoupSession");
    SoupMessage* msg = make_message(method, uri, headers);
    if (body_utf8.has_value()) {
        char const* ct = content_type_if_body ? content_type_if_body : "application/octet-stream";
        GBytes* gb = g_bytes_new(body_utf8->data(), body_utf8->size());
        soup_message_set_request_body_from_bytes(msg, ct, gb);
        g_bytes_unref(gb);
    }
    return finish_message(session, msg);
}

std::pair<int, std::string> soup_sync_request_raw(SoupSession* session, char const* method,
    std::string const& uri, std::vector<std::uint8_t> const& body_bytes, char const* content_type,
    std::map<std::string, std::string> const& headers)
{
    if (!session) throw std::invalid_argument("soup_sync_request_raw: null SoupSession");
    SoupMessage* msg = make_message(method, uri, headers);
    GBytes* gb = g_bytes_new(body_bytes.data(), body_bytes.size());
    soup_message_set_request_body_from_bytes(msg, content_type, gb);
    g_bytes_unref(gb);
    return finish_message(session, msg);
}

} // namespace cd
