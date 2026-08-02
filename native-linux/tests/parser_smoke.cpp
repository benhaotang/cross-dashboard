#include "data/parser/ical_parser.h"
#include "data/parser/task_input_parser.h"

#include <glib.h>
#include <ctime>

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

    auto berlin = cd::ICalParser::parse_events(
        "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nUID:tz\r\nSUMMARY:Interview\r\n"
        "DTSTART;TZID=Europe/Berlin:20260714T163000\r\n"
        "DTEND;TZID=Europe/Berlin:20260714T173000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
    if (berlin.size() != 1 || berlin[0].start != 1784039400000LL) return 5;

    g_setenv("TZ", "Europe/Berlin", TRUE);
    tzset();
    auto floating = cd::ICalParser::parse_events(
        "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nUID:floating\r\nSUMMARY:Floating\r\n"
        "DTSTART:20260714T163000\r\nDTEND:20260714T173000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
    if (floating.size() != 1 || floating[0].start != 1784039400000LL) return 6;

    return 0;
}
