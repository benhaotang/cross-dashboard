#include "background_definition.h"

#include <nlohmann/json.hpp>

namespace cd {

std::string background_template_json(BackgroundTemplate const& v)
{
    return nlohmann::json{{"enabled", v.enabled}, {"source", v.source == BackgroundSource::Inbox ? "inbox" : "views"},
        {"inboxType", v.inbox_type}, {"inboxDate", v.inbox_date}, {"viewsType", v.views_type},
        {"viewsDate", v.views_date}, {"viewsMode", v.views_mode}, {"capturedAt", v.captured_at}}.dump();
}

bool parse_background_template(std::string const& raw, BackgroundTemplate& out)
{
    try {
        auto j = nlohmann::json::parse(raw);
        out.enabled = j.value("enabled", true);
        out.source = j.value("source", "inbox") == "views" ? BackgroundSource::Views : BackgroundSource::Inbox;
        out.inbox_type = j.value("inboxType", "all"); out.inbox_date = j.value("inboxDate", "all");
        out.views_type = j.value("viewsType", "all"); out.views_date = j.value("viewsDate", "all");
        out.views_mode = j.value("viewsMode", "kanban"); out.captured_at = j.value("capturedAt", std::int64_t{});
        return true;
    }
    catch (...) { return false; }
}

std::string background_template_summary(BackgroundTemplate const& v)
{
    if (v.source == BackgroundSource::Inbox) return "Inbox · " + v.inbox_type + " · " + v.inbox_date;
    return "Views · " + v.views_mode + " · " + v.views_type + " · " + v.views_date;
}

} // namespace cd
