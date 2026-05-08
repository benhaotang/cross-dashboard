#include "app_container.h"
#include "data/parser/ical_parser.h"
#include "data/parser/task_input_parser.h"

/** Phase 1 demo: SQLite + parsers + Soup-backed clients/repos — no GTK. */
int main()
{
    try {
        cd::AppContainer app;

        auto test_parse = cd::TaskInputParser::parse("!! ship Linux build #cross");
        if (test_parse.priority != 5) return 2;

        cd::CalendarEvent demo_event;
        demo_event.uid = "uid-test";
        demo_event.summary = "Demo";
        demo_event.start = 0;
        demo_event.end = 3600000;
        std::string const ser = cd::ICalParser::serialize_event(demo_event);
        if (ser.find("BEGIN:VEVENT") == std::string::npos) return 3;

        (void)app.db();
        (void)app.prefs();
        (void)app.caldav();
        return 0;
    }
    catch (...) {
        return 1;
    }
}
