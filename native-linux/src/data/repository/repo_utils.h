#pragma once

#include <optional>
#include <string>
#include <vector>

namespace cd {

/** Decodes CALDAV_SELECTED_CALENDARS JSON (array of `{ "href": "..." }` or string hrefs). */
std::vector<std::string> calendars_from_selected_json(std::optional<std::string> raw_json);

/** GITEA_REPOS — JSON array or comma-separated repos. */
std::vector<std::string> repos_from_cred_string(std::optional<std::string> raw);

} // namespace cd
