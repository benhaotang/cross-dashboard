#include "data/parser/ical_parser.h"
#include "data/parser/task_input_parser.h"

/** Meson smoke test (`meson test`) — parsers only, no network. */
int main()
{
    auto p = cd::TaskInputParser::parse("!!! urgent");
    if (p.priority != 1) return 1;

    cd::CalendarEvent ev{};
    ev.uid = "uid-smoke";
    ev.summary = "S";
    ev.start = 1;
    ev.end = 2;
    std::string s = cd::ICalParser::serialize_event(ev);
    if (s.find("BEGIN:VEVENT") == std::string::npos) return 2;
    if (s.find("UID:uid-smoke") == std::string::npos) return 3;

    auto p2 = cd::TaskInputParser::parse("todo #home");
    if (!p2.categories.empty()) {
        if (p2.categories[0] != "home") return 4;
    }
    else
        return 4;

    return 0;
}
