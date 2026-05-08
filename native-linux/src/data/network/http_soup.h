#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

typedef struct _SoupSession SoupSession;

namespace cd {

std::pair<int, std::string> soup_sync_request(SoupSession* session, char const* method,
    std::string const& uri, std::optional<std::string> const& body_utf8, char const* content_type_if_body,
    std::map<std::string, std::string> const& headers = {});

/** Binary request body (e.g. multipart/form-data). */
std::pair<int, std::string> soup_sync_request_raw(SoupSession* session, char const* method,
    std::string const& uri, std::vector<std::uint8_t> const& body_bytes, char const* content_type,
    std::map<std::string, std::string> const& headers = {});

} // namespace cd
