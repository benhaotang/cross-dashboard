#include "caldav_client.h"

#include "data/network/http_soup.h"
#include "data/parser/ical_parser.h"

#include <glib.h>

#include <cctype>
#include <chrono>
#include <ctime>
#include <regex>
#include <stdexcept>
#include <tuple>

namespace cd {

namespace {

std::string iso_utc_z(std::chrono::milliseconds ms)
{
    using namespace std::chrono;
    auto const sec = duration_cast<std::chrono::seconds>(ms).count();
    std::time_t tt = static_cast<std::time_t>(sec);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%dT%H%M%SZ", &tm);
    return buf;
}

std::optional<std::string> abs_resource_url(std::string const& server, std::optional<std::string> const& rh)
{
    if (!rh.has_value()) return std::nullopt;
    if (rh->rfind("http", 0) == 0) return rh;
    return server + *rh;
}

} // namespace

CalDavClient::CalDavClient(SecretStore& secrets, SoupSession* session)
    : secrets_(secrets)
    , session_(session)
{
    if (!session_) throw std::invalid_argument("CalDavClient: null SoupSession");
}

CalDavClient::~CalDavClient() = default;

std::optional<std::string> CalDavClient::server_url_opt() const
{
    auto s = secrets_.get(CredentialKey::CALDAV_SERVER);
    if (!s.has_value() || s->empty()) return std::nullopt;
    return normalize_base_url(*s);
}

std::string CalDavClient::normalize_base_url(std::string const& u) const
{
    std::string s = u;
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

std::optional<std::string> CalDavClient::caldav_base_opt() const
{
    auto server = server_url_opt();
    auto username = secrets_.get(CredentialKey::CALDAV_USERNAME);
    if (!server.has_value() || !username.has_value()) return std::nullopt;
    if (server->find("/dav/calendars") != std::string::npos) return normalize_base_url(*server) + "/";
    return *server + "/remote.php/dav/calendars/" + *username + "/";
}

std::optional<std::string> CalDavClient::basic_auth_header() const
{
    auto u = secrets_.get(CredentialKey::CALDAV_USERNAME);
    auto p = secrets_.get(CredentialKey::CALDAV_PASSWORD).value_or("");
    if (!u.has_value()) return std::nullopt;
    std::string cred = *u + ":" + p;
    gchar* b64 = g_base64_encode(reinterpret_cast<guchar const*>(cred.data()), static_cast<guint>(cred.size()));
    if (!b64) return std::nullopt;
    std::string out = "Basic ";
    out += b64;
    g_free(b64);
    return out;
}

std::optional<std::string> CalDavClient::execute(std::string const& method, std::string const& url,
    std::optional<std::string> const& body_utf8, std::map<std::string, std::string> extra_headers) const
{
    try {
        auto auth = basic_auth_header();
        if (auth.has_value()) extra_headers["Authorization"] = *auth;

        auto [status, payload] =
            soup_sync_request(session_, method.c_str(), url, body_utf8,
                body_utf8.has_value() ? "application/xml; charset=utf-8" : nullptr, extra_headers);
        if (status >= 200 && (status < 300 || status == 207)) return payload;
        return std::nullopt;
    }
    catch (...) {
        return std::nullopt;
    }
}

bool CalDavClient::test_connection()
{
    auto server = secrets_.get(CredentialKey::CALDAV_SERVER);
    if (!server.has_value() || server->empty()) return false;
    try {
        std::map<std::string, std::string> hdrs;
        if (auto auth = basic_auth_header(); auth.has_value()) hdrs["Authorization"] = *auth;
        auto [status, junk] =
            soup_sync_request(session_, "HEAD", normalize_base_url(*server), std::nullopt, nullptr, hdrs);
        return (status >= 200 && status < 300) || status == 207 || status == 401;
    }
    catch (...) {
        return false;
    }
}

std::optional<std::string> CalDavClient::extract_xml_value(std::string const& xml_block, std::string const& tag)
{
    try {
        std::regex pattern(
            std::string(R"(<[^:]*:?)") + tag + R"([^>]*>([\s\S]*?)</[^:]*:?)" + tag + ">",
            std::regex::icase | std::regex::ECMAScript);
        std::smatch m;
        if (std::regex_search(xml_block, m, pattern) && m.size() > 1) {
            std::string inner = m[1].str();
            // trim whitespace
            size_t start = 0;
            while (start < inner.size()
                && std::isspace(static_cast<unsigned char>(inner[start])))
                ++start;
            while (!inner.empty() && std::isspace(static_cast<unsigned char>(inner.back())))
                inner.pop_back();
            if (inner.empty()) return std::nullopt;
            return inner;
        }
    }
    catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> CalDavClient::normalize_color(std::optional<std::string> raw)
{
    if (!raw.has_value() || raw->empty()) return std::nullopt;
    std::string hex = raw->front() == '#' ? raw->substr(1) : *raw;
    while (!hex.empty() && std::isspace(static_cast<unsigned char>(hex.front())))
        hex.erase(hex.begin());
    if (hex.size() < 6) return std::nullopt;
    std::string six = hex.substr(0, 6);
    for (char& ch : six)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return "#" + six;
}

std::vector<CalDavCalendar> CalDavClient::parse_calendars_from_propfind(std::string const& xml, std::string const& base)
{
    (void)base;
    std::vector<CalDavCalendar> out;
    try {
        std::regex response_re(R"(<[^:]*:?response\b[^>]*>([\s\S]*?)</[^:]*:?response>)",
            std::regex::icase | std::regex::ECMAScript);
        auto resp_begin =
            std::sregex_iterator(xml.begin(), xml.end(), response_re);
        auto resp_end = std::sregex_iterator();

        std::regex comp_re(R"rx(<[^:]*:?comp\s+name="([^"]+)")rx", std::regex::icase);

        for (auto i = resp_begin; i != resp_end; ++i) {
            std::string content = (*i)[1].str();

            auto href = extract_xml_value(content, "href");
            if (!href.has_value()) continue;

            std::string cl = content;
            for (char& ch : cl)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (cl.find("calendar") == std::string::npos) continue;
            if (cl.find("addressbook") != std::string::npos) continue;

            std::string display = extract_xml_value(content, "displayname").value_or(*href);
            auto color = extract_xml_value(content, "calendar-color");
            if (!color.has_value()) color = extract_xml_value(content, "cal:calendar-color");
            if (!color.has_value()) color = extract_xml_value(content, "a:calendar-color");

            std::vector<std::string> components;
            auto comp_it = std::sregex_iterator(content.begin(), content.end(), comp_re);
            auto comp_end = std::sregex_iterator();
            for (auto it = comp_it; it != comp_end; ++it) {
                std::string name = (*it)[1].str();
                for (char& ch : name)
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                components.push_back(name);
            }
            if (components.empty()) components.push_back("VEVENT");

            CalDavCalendar c;
            c.href = *href;
            c.display_name = display;
            c.color = normalize_color(color);
            c.components = std::move(components);
            out.push_back(std::move(c));
        }
    }
    catch (...) {
        return {};
    }
    return out;
}

std::vector<std::tuple<std::optional<std::string>, std::optional<std::string>, std::string>>
CalDavClient::extract_calendar_resources(std::string const& multi_status_xml)
{
    std::vector<std::tuple<std::optional<std::string>, std::optional<std::string>, std::string>> out;
    try {
        std::regex response_re(R"(<[^:]*:?response\b[^>]*>([\s\S]*?)</[^:]*:?response>)",
            std::regex::icase | std::regex::ECMAScript);
        std::regex caldata_re(
            R"(<[^:]*:?calendar-data[^>]*>([\s\S]*?)</[^:]*:?calendar-data>)", std::regex::icase | std::regex::ECMAScript);
        auto it = std::sregex_iterator(multi_status_xml.begin(), multi_status_xml.end(), response_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            std::string content = (*it)[1].str();
            std::smatch dm;
            if (!std::regex_search(content, dm, caldata_re)) continue;
            std::string ical = dm[1].str();
            // trim XML CDATA-ish
            auto lead = ical.find_first_not_of(" \t\r\n");
            if (lead != std::string::npos)
                ical.erase(0, lead);
            while (!ical.empty()
                && std::isspace(static_cast<unsigned char>(ical.back())))
                ical.pop_back();
            if (ical.empty()) continue;
            auto href = extract_xml_value(content, "href");
            auto etag_val = extract_xml_value(content, "getetag");
            if (etag_val.has_value()) {
                std::string& e = *etag_val;
                if (!e.empty() && (e.front() == '"' || e.front() == '\'')) e.erase(e.begin());
                if (!e.empty() && (e.back() == '"' || e.back() == '\'')) e.pop_back();
            }
            out.emplace_back(std::move(href), std::move(etag_val), std::move(ical));
        }
    }
    catch (...) {
        return {};
    }
    return out;
}

std::string CalDavClient::calendar_query_report(std::chrono::milliseconds const* from,
    std::chrono::milliseconds const* to,
    std::string const& component_type)
{
    std::string time_filter;
    if (from && to)
        time_filter =
            "<c:time-range start=\"" + iso_utc_z(*from) + "\" end=\"" + iso_utc_z(*to) + "\"/>";

    return std::string("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n") + "<c:calendar-query "
        "xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">\n"
        "  <d:prop>\n"
        "    <d:getetag/>\n"
        "    <c:calendar-data/>\n"
        "  </d:prop>\n"
        "  <c:filter>\n"
        "    <c:comp-filter name=\"VCALENDAR\">\n"
        "      <c:comp-filter name=\"" + component_type + "\">\n"
        + time_filter +
        "\n"
        "      </c:comp-filter>\n"
        "    </c:comp-filter>\n"
        "  </c:filter>\n"
        "</c:calendar-query>\n";
}

std::string CalDavClient::todo_query_report()
{
    return R"(<?xml version="1.0" encoding="utf-8"?>
<c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
  <d:prop>
    <d:getetag/>
    <c:calendar-data/>
  </d:prop>
  <c:filter>
    <c:comp-filter name="VCALENDAR">
      <c:comp-filter name="VTODO"/>
    </c:comp-filter>
  </c:filter>
</c:calendar-query>
)";
}

std::vector<CalDavCalendar> CalDavClient::discover_calendars()
{
    auto base = caldav_base_opt();
    if (!base.has_value()) return {};

    auto const body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\"\n"
        "           xmlns:a=\"http://apple.com/ns/ical/\">\n"
        "  <d:prop>\n"
        "    <d:displayname/>\n"
        "    <d:resourcetype/>\n"
        "    <c:calendar-description/>\n"
        "    <c:supported-calendar-component-set/>\n"
        "    <a:calendar-color/>\n"
        "    <d:getctag xmlns:d=\"http://calendarserver.org/ns/\"/>\n"
        "  </d:prop>\n"
        "</d:propfind>\n";

    auto resp =
        execute("PROPFIND", *base, body, std::map<std::string, std::string>{{"Depth", "1"}});
    if (!resp.has_value()) return {};
    return parse_calendars_from_propfind(*resp, *base);
}

std::vector<CalendarEvent> CalDavClient::fetch_events(std::vector<std::string> const& calendar_hrefs,
    std::chrono::milliseconds from_epoch,
    std::chrono::milliseconds to_epoch)
{
    auto server = server_url_opt();
    std::vector<CalendarEvent> results;
    if (!server.has_value()) return results;

    auto const report = calendar_query_report(&from_epoch, &to_epoch, "VEVENT");

    for (auto const& href : calendar_hrefs) {
        std::string url = href.rfind("http", 0) == 0 ? href : (*server + href);
        auto response = execute("REPORT", url, report, std::map<std::string, std::string>{
                                                       {"Depth", "1"}, {"Content-Type", "application/xml"}});
        if (!response.has_value()) continue;
        for (auto const& tup : extract_calendar_resources(*response)) {
            auto [rh, etag, ical_data] = tup;
            auto abs = abs_resource_url(*server, rh);
            auto evts = ICalParser::parse_events(ical_data, href, abs, etag);
            results.insert(results.end(), evts.begin(), evts.end());
        }
    }
    return results;
}

std::vector<CalDavTask> CalDavClient::fetch_tasks(std::vector<std::string> const& calendar_hrefs)
{
    auto server = server_url_opt();
    std::vector<CalDavTask> results;
    if (!server.has_value()) return results;

    auto const report = todo_query_report();
    for (auto const& href : calendar_hrefs) {
        std::string url = href.rfind("http", 0) == 0 ? href : (*server + href);
        auto response = execute("REPORT", url, report, std::map<std::string, std::string>{
                                                       {"Depth", "1"}, {"Content-Type", "application/xml"}});
        if (!response.has_value()) continue;
        for (auto const& tup : extract_calendar_resources(*response)) {
            auto [rh, etag, ical_data] = tup;
            auto abs = abs_resource_url(*server, rh);
            auto tasks = ICalParser::parse_tasks(ical_data, href, abs, etag);
            results.insert(results.end(), tasks.begin(), tasks.end());
        }
    }
    return results;
}

std::vector<Note> CalDavClient::fetch_notes(std::vector<std::string> const& calendar_hrefs)
{
    auto server = server_url_opt();
    std::vector<Note> results;
    if (!server.has_value()) return results;

    auto const report = calendar_query_report(nullptr, nullptr, "VJOURNAL");
    for (auto const& href : calendar_hrefs) {
        std::string url = href.rfind("http", 0) == 0 ? href : (*server + href);
        auto response = execute("REPORT", url, report, std::map<std::string, std::string>{
                                                       {"Depth", "1"}, {"Content-Type", "application/xml"}});
        if (!response.has_value()) continue;
        for (auto const& tup : extract_calendar_resources(*response)) {
            auto [rh, etag, ical_data] = tup;
            auto abs = abs_resource_url(*server, rh);
            auto parsed = ICalParser::parse_notes(ical_data, href, abs, etag);
            results.insert(results.end(), parsed.begin(), parsed.end());
        }
    }
    return results;
}

void CalDavClient::put_resource(std::string const& url, std::string const& ical_text,
    std::optional<std::string> const& etag_if_match) const
{
    std::map<std::string, std::string> hdrs{{"Content-Type", "text/calendar; charset=utf-8"}};
    if (auto auth = basic_auth_header(); auth.has_value()) hdrs["Authorization"] = *auth;
    if (etag_if_match.has_value()) hdrs["If-Match"] = *etag_if_match;

    auto [status, body] = soup_sync_request(session_, "PUT", url,
        std::optional<std::string>{ical_text}, "text/calendar; charset=utf-8", hdrs);
    if (((status >= 200 && status < 300) || status == 201 || status == 204)) return;
    std::string err = body.size() > 500 ? body.substr(0, 500) : body;
    throw std::runtime_error("CalDAV PUT failed: HTTP " + std::to_string(status) + " — " + err);
}

void CalDavClient::delete_resource(std::string const& url, std::optional<std::string> const& etag_if_match) const
{
    std::map<std::string, std::string> hdrs;
    if (auto auth = basic_auth_header(); auth.has_value()) hdrs["Authorization"] = *auth;
    if (etag_if_match.has_value()) hdrs["If-Match"] = *etag_if_match;

    auto [status, junk] = soup_sync_request(session_, "DELETE", url, std::nullopt, nullptr, hdrs);
    if (((status >= 200 && status < 300) || status == 204)) return;
    (void)junk;
    throw std::runtime_error("CalDAV DELETE failed: HTTP " + std::to_string(status));
}

std::string CalDavClient::resolve_href(std::optional<std::string> const& href, std::string const& uid,
    std::optional<std::string> const& calendar_href) const
{
    if (href.has_value()) return *href;
    auto server = server_url_opt();
    if (!server.has_value()) throw std::runtime_error("CalDAV resolve_href: no server");
    if (!calendar_href.has_value()) throw std::runtime_error("CalDAV resolve_href: no calendar href for " + uid);
    std::string base =
        calendar_href->rfind("http", 0) == 0 ? *calendar_href : (*server + *calendar_href);
    return base + uid + ".ics";
}

CalDavTask CalDavClient::create_task(CalDavTask const& task, std::string const& calendar_href)
{
    auto server = server_url_opt();
    if (!server.has_value()) throw std::runtime_error("No CalDAV server configured");
    std::string base =
        calendar_href.rfind("http", 0) == 0 ? calendar_href : (*server + calendar_href);
    gchar* gu = g_uuid_string_random();
    std::string uid = task.uid.empty() ? (gu ? gu : "uid") : task.uid;
    if (gu) g_free(gu);
    std::string resource_url = base + uid + ".ics";
    CalDavTask with_uid = task;
    with_uid.uid = uid;
    put_resource(resource_url, ICalParser::serialize_task(with_uid), std::nullopt);
    with_uid.href = resource_url;
    return with_uid;
}

void CalDavClient::update_task(CalDavTask const& task)
{
    std::string url = resolve_href(task.href, task.uid, task.calendar_href);
    put_resource(url, ICalParser::serialize_task(task), std::nullopt);
}

void CalDavClient::delete_task(CalDavTask const& task)
{
    std::string url = resolve_href(task.href, task.uid, task.calendar_href);
    delete_resource(url, task.etag);
}

cd::Note CalDavClient::create_note(Note const& note, std::string const& calendar_href)
{
    auto server = server_url_opt();
    if (!server.has_value()) throw std::runtime_error("No CalDAV server configured");
    std::string base =
        calendar_href.rfind("http", 0) == 0 ? calendar_href : (*server + calendar_href);
    gchar* gu = g_uuid_string_random();
    std::string uid = note.uid.empty() ? (gu ? gu : "uid") : note.uid;
    if (gu) g_free(gu);
    std::string resource_url = base + uid + ".ics";
    Note with_uid = note;
    with_uid.uid = uid;
    put_resource(resource_url, ICalParser::serialize_note(with_uid), std::nullopt);
    with_uid.href = resource_url;
    return with_uid;
}

void CalDavClient::update_note(Note const& note)
{
    std::string url = resolve_href(note.href, note.uid, note.calendar_href);
    put_resource(url, ICalParser::serialize_note(note), std::nullopt);
}

void CalDavClient::delete_note(Note const& note)
{
    std::string url = resolve_href(note.href, note.uid, note.calendar_href);
    delete_resource(url, note.etag);
}

CalendarEvent CalDavClient::create_event(CalendarEvent const& event, std::string const& calendar_href)
{
    auto server = server_url_opt();
    if (!server.has_value()) throw std::runtime_error("No CalDAV server configured");
    std::string base =
        calendar_href.rfind("http", 0) == 0 ? calendar_href : (*server + calendar_href);
    gchar* gu = g_uuid_string_random();
    std::string uid = event.uid.empty() ? (gu ? gu : "uid") : event.uid;
    if (gu) g_free(gu);
    std::string resource_url = base + uid + ".ics";
    CalendarEvent with_uid = event;
    with_uid.uid = uid;
    put_resource(resource_url, ICalParser::serialize_event(with_uid), std::nullopt);
    with_uid.href = resource_url;
    return with_uid;
}

void CalDavClient::delete_event(CalendarEvent const& event)
{
    std::string url = resolve_href(event.href, event.uid, event.calendar_href);
    delete_resource(url, event.etag);
}

} // namespace cd
