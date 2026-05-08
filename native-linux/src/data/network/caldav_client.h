#pragma once

#include "data/prefs/prefs.h"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

typedef struct _SoupSession SoupSession;

namespace cd {

class CalDavClient {
public:
    CalDavClient(SecretStore& secrets, SoupSession* session);
    ~CalDavClient();

    CalDavClient(CalDavClient const&) = delete;
    CalDavClient& operator=(CalDavClient const&) = delete;
    CalDavClient(CalDavClient&&) = delete;
    CalDavClient& operator=(CalDavClient&&) = delete;

    [[nodiscard]] bool test_connection();
    [[nodiscard]] std::vector<CalDavCalendar> discover_calendars();

    [[nodiscard]] std::vector<CalendarEvent> fetch_events(std::vector<std::string> const& calendar_hrefs,
        std::chrono::milliseconds from_epoch, std::chrono::milliseconds to_epoch);

    [[nodiscard]] std::vector<CalDavTask> fetch_tasks(std::vector<std::string> const& calendar_hrefs);

    [[nodiscard]] std::vector<Note> fetch_notes(std::vector<std::string> const& calendar_hrefs);

    CalDavTask create_task(CalDavTask const& task, std::string const& calendar_href);

    void update_task(CalDavTask const& task);
    void delete_task(CalDavTask const& task);

    Note create_note(Note const& note, std::string const& calendar_href);
    void update_note(Note const& note);
    void delete_note(Note const& note);

    CalendarEvent create_event(CalendarEvent const& event, std::string const& calendar_href);
    void delete_event(CalendarEvent const& event);

private:
    SecretStore& secrets_;
    SoupSession* session_{};

    [[nodiscard]] std::optional<std::string> execute(std::string const& method, std::string const& url,
        std::optional<std::string> const& body_utf8,
        std::map<std::string, std::string> extra_headers = {}) const;

    void put_resource(std::string const& url, std::string const& ical_text,
        std::optional<std::string> const& etag_if_match) const;
    void delete_resource(std::string const& url, std::optional<std::string> const& etag_if_match) const;

    [[nodiscard]] std::string normalize_base_url(std::string const& u) const;
    [[nodiscard]] std::optional<std::string> server_url_opt() const;
    [[nodiscard]] std::optional<std::string> caldav_base_opt() const;
    [[nodiscard]] std::optional<std::string> basic_auth_header() const;
    [[nodiscard]] std::string resolve_href(std::optional<std::string> const& href,
        std::string const& uid, std::optional<std::string> const& calendar_href) const;

    [[nodiscard]] static std::optional<std::string> extract_xml_value(std::string const& xml_block,
        std::string const& tag_name);

    [[nodiscard]] static std::vector<CalDavCalendar> parse_calendars_from_propfind(std::string const& xml,
        std::string const& propfind_base_url);

    [[nodiscard]] static std::vector<std::tuple<std::optional<std::string>, std::optional<std::string>, std::string>>
    extract_calendar_resources(std::string const& multi_status_xml);

    [[nodiscard]] static std::optional<std::string> normalize_color(std::optional<std::string> raw);

    [[nodiscard]] static std::string calendar_query_report(std::chrono::milliseconds const* from,
        std::chrono::milliseconds const* to,
        std::string const& component_type);

    [[nodiscard]] static std::string todo_query_report();
};

} // namespace cd
