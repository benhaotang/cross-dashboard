#pragma once

#include <cstdint>
#include <string>

namespace cd {

enum class BackgroundSource { Inbox, Views };

struct BackgroundTemplate final {
    bool enabled{true};
    BackgroundSource source{BackgroundSource::Inbox};
    std::string inbox_type{"all"};
    std::string inbox_date{"all"};
    std::string views_type{"all"};
    std::string views_date{"all"};
    std::string views_mode{"kanban"};
    std::int64_t captured_at{};
};

std::string background_template_json(BackgroundTemplate const& value);
bool parse_background_template(std::string const& json, BackgroundTemplate& out);
std::string background_template_summary(BackgroundTemplate const& value);

} // namespace cd
