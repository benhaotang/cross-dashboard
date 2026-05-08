#include "ical_parser.h"

#include <glib.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <regex>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace cd {

namespace {

using PropMap = std::unordered_map<std::string, std::string>;

std::vector<std::string> unfold_lines(std::string_view text_view)
{
    std::vector<std::string> lines;
    std::string cur;
    auto push_cur = [&] {
        if (!cur.empty()) {
            lines.push_back(std::move(cur));
            cur.clear();
        }
    };
    std::string buffer(text_view); // NUL-safe newline iteration
    std::istringstream iss(buffer);
    std::string ln;
    while (std::getline(iss, ln)) {
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        if (!ln.empty() && (ln[0] == ' ' || ln[0] == '\t')) {
            if (ln.size() > 1) cur.append(ln.substr(1));
        }
        else {
            push_cur();
            cur = ln;
        }
    }
    push_cur();
    return lines;
}

std::string upper_copy(std::string const& x)
{
    std::string o = x;
    for (char& ch : o)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return o;
}

std::string normalize_prop_name(std::string const& raw_before_colon)
{
    auto semi = raw_before_colon.find(';');
    if (semi != std::string::npos)
        return upper_copy(raw_before_colon.substr(0, semi));
    return upper_copy(raw_before_colon);
}

std::string unescape_ic(std::string v)
{
    for (size_t pos = 0; (pos = v.find('\\', pos)) != std::string::npos;) {
        if (pos + 1 >= v.size()) break;
        char esc = v[pos + 1];
        switch (esc) {
        case 'n':
        case 'N': v.replace(pos, 2, "\n"); break;
        case ',': v.replace(pos, 2, ","); break;
        case ';': v.replace(pos, 2, ";"); break;
        case '\\': v.replace(pos, 2, "\\"); break;
        default: pos++; break;
        }
    }
    return v;
}

std::vector<std::string> parse_categories(std::string const& raw)
{
    std::vector<std::string> out;
    std::stringstream ss(raw);
    std::string p;
    while (std::getline(ss, p, ',')) {
        auto a = p.find_first_not_of(" \t");
        auto b = p.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        out.push_back(p.substr(a, b - a + 1));
    }
    return out;
}

std::string escape_ic(std::string const& v)
{
    std::string o;
    o.reserve(v.size() + 8);
    for (char c : v) {
        switch (c) {
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case ',': o += "\\,"; break;
        case ';': o += "\\;"; break;
        default: o += c; break;
        }
    }
    return o;
}

EpochMillis parse_duration_ms(std::string const& s)
{
    std::smatch m;
    static std::regex const re(
        R"(P(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?)");
    if (!std::regex_match(s, m, re)) return 3600'000;
    auto grab = [&](int idx) -> long long {
        if (!m[idx].matched || m[idx].str().empty()) return 0;
        try {
            return std::stoll(m[idx].str());
        }
        catch (...) {
            return 0;
        }
    };
    long long ms = 0;
    ms += grab(1) * 7LL * 24 * 3600 * 1000;
    ms += grab(2) * 24LL * 3600 * 1000;
    ms += grab(3) * 3600LL * 1000;
    ms += grab(4) * 60LL * 1000;
    ms += grab(5) * 1000LL;
    return ms > 0 ? ms : 3600'000;
}

std::optional<EpochMillis> parse_dt_raw_line(std::string const& raw_line)
{
    auto colon = raw_line.find(':');
    if (colon == std::string::npos) return std::nullopt;
    std::string params = raw_line.substr(0, colon);
    std::string val = raw_line.substr(colon + 1);

    if (params.find("VALUE=DATE") != std::string::npos) {
        char const* vp = val.c_str();
        int y{};
        unsigned m{};
        unsigned d{};
        if (std::strlen(vp) >= 8 && std::sscanf(vp, "%4d%2u%2u", &y, &m, &d) == 3) {
            g_autoptr(GDateTime) gdt =
                g_date_time_new_utc(y, static_cast<gint>(m), static_cast<gint>(d), 0, 0, 0.0);
            if (!gdt) return std::nullopt;
            return g_date_time_to_unix(gdt) * 1000;
        }
        return std::nullopt;
    }

    if (!val.empty() && val.back() == 'Z') {
        int y{};
        unsigned mo{};
        unsigned d{};
        unsigned H{};
        unsigned M{};
        unsigned S{};
        if (std::sscanf(val.c_str(), "%04d%02u%02uT%02u%02u%02uZ", &y, &mo, &d, &H, &M, &S) == 6) {
            g_autoptr(GDateTime) gdt =
                g_date_time_new_utc(y, static_cast<gint>(mo), static_cast<gint>(d),
                    static_cast<gint>(H), static_cast<gint>(M), static_cast<double>(S));
            if (!gdt) return std::nullopt;
            return g_date_time_to_unix(gdt) * 1000;
        }
    }

    std::smatch zm;
    static std::regex const tzidr(R"(TZID=([^;:]+))", std::regex_constants::icase);
    std::optional<std::string> tz_name;
    if (std::regex_search(params, zm, tzidr)) tz_name = zm[1].str();

    int y{};
    unsigned mo{};
    unsigned d{};
    unsigned H{};
    unsigned M{};
    unsigned S{};
    if (std::sscanf(val.c_str(), "%04d%02u%02uT%02u%02u%02u", &y, &mo, &d, &H, &M, &S) != 6)
        return std::nullopt;

    if (tz_name.has_value()) {
        GTimeZone* tz = g_time_zone_new(tz_name->c_str());
        GDateTime* local = g_date_time_new(
            tz, y, static_cast<gint>(mo), static_cast<gint>(d), static_cast<gint>(H), static_cast<gint>(M),
            static_cast<gdouble>(S));
        g_time_zone_unref(tz);
        if (!local) return std::nullopt;
        EpochMillis ms = g_date_time_to_unix(local) * 1000;
        g_date_time_unref(local);
        return ms;
    }
    g_autoptr(GDateTime) gutc = g_date_time_new_utc(
        y, static_cast<gint>(mo), static_cast<gint>(d), static_cast<gint>(H), static_cast<gint>(M),
        static_cast<gdouble>(S));
    if (!gutc) return std::nullopt;
    return g_date_time_to_unix(gutc) * 1000;
}

std::optional<EpochMillis> parse_dt(PropMap const& props, char const* key)
{
    std::string raw_key = std::string{"__RAW_"} + key;
    auto it = props.find(raw_key);
    if (it == props.end()) return std::nullopt;
    return parse_dt_raw_line(it->second);
}

std::string format_instant_ms_utc(EpochMillis ms)
{
    gint64 secs = ms / 1000;
    g_autoptr(GDateTime) gdt = g_date_time_new_from_unix_utc(secs);
    if (!gdt) return "19700101T000000Z";
    gchar* s = g_date_time_format(gdt, "%Y%m%dT%H%M%SZ");
    std::string out = s ? s : "";
    g_free(s);
    return out;
}

EpochMillis utc_now_epoch_ms()
{
    g_autoptr(GDateTime) n = g_date_time_new_now_utc();
    return g_date_time_to_unix(n) * 1000;
}

template <typename Fn>
void for_each_component(std::string_view text, std::string const& comp_upper, Fn const& cb)
{
    auto lines = unfold_lines(text);
    bool inside{};
    PropMap props;
    for (auto const& line : lines) {
        auto up = upper_copy(line);
        if (up == "BEGIN:" + comp_upper) {
            inside = true;
            props.clear();
            continue;
        }
        if (up == "END:" + comp_upper && inside) {
            inside = false;
            cb(props);
            continue;
        }
        if (!inside) continue;
        auto colon = line.find(':');
        if (colon <= 0 || colon == std::string::npos) continue;
        std::string raw_field = line.substr(0, colon);
        std::string norm = normalize_prop_name(raw_field);
        std::string value = line.substr(colon + 1);

        bool append = norm == "RELATED-TO" || norm == "ATTENDEE";
        if (!append && props.count(norm))
            continue;
        if (append && props.count(norm)) {
            props[norm].push_back(',');
            props[norm].append(value);
        }
        else
            props[norm] = value;
        props[std::string{"__RAW_"} + norm] = line;
    }
}

std::optional<CalendarEvent> parse_event_props(PropMap const& props,
    std::optional<std::string> const cal,
    std::optional<std::string> const resh,
    std::optional<std::string> etag)
{
    auto uid_it = props.find("UID");
    auto sum_it = props.find("SUMMARY");
    if (uid_it == props.end() || sum_it == props.end()) return std::nullopt;
    auto st = parse_dt(props, "DTSTART");
    if (!st.has_value()) return std::nullopt;
    auto end = parse_dt(props, "DTEND");
    EpochMillis end_ms{};
    if (end.has_value()) {
        end_ms = *end;
    }
    else {
        auto dr = props.find("DURATION");
        if (dr != props.end()) end_ms = *st + parse_duration_ms(dr->second);
        else end_ms = *st + 3600'000;
    }
    CalendarEvent e;
    e.uid = uid_it->second;
    e.summary = unescape_ic(sum_it->second);
    e.start = *st;
    e.end = end_ms;
    if (auto di = props.find("DESCRIPTION"); di != props.end())
        e.description = unescape_ic(di->second);
    if (auto li = props.find("LOCATION"); li != props.end())
        e.location = unescape_ic(li->second);
    if (cal) e.calendar_href = cal;
    if (etag && !etag->empty()) e.etag = etag;
    if (resh && !resh->empty()) e.href = resh;
    return e;
}

std::optional<CalDavTask> parse_task_props(PropMap const& props, std::optional<std::string> const cal,
    std::optional<std::string> const resh, std::optional<std::string> etag)
{
    auto uid_it = props.find("UID");
    auto sum_it = props.find("SUMMARY");
    if (uid_it == props.end() || sum_it == props.end()) return std::nullopt;

    CalDavTask t;
    t.uid = uid_it->second;
    t.summary = unescape_ic(sum_it->second);
    if (auto ri = props.find("RELATED-TO"); ri != props.end()) {
        auto raw_it = props.find("__RAW_RELATED-TO");
        bool parent = raw_it != props.end()
            && (raw_it->second.find("RELTYPE=PARENT") != std::string::npos
                || raw_it->second.find("reltype=PARENT") != std::string::npos);
        if (parent && !ri->second.empty()) {
            auto comma = ri->second.find(',');
            t.parent_uid = comma == std::string::npos ? ri->second : ri->second.substr(0, comma);
        }
    }
    if (auto di = props.find("DESCRIPTION"); di != props.end()) t.description = unescape_ic(di->second);

    auto st_lit = props.find("STATUS");
    t.status =
        task_status_from_ical(st_lit != props.end() ? st_lit->second : std::string{"NEEDS-ACTION"});

    if (auto p = props.find("PRIORITY"); p != props.end()) {
        try {
            t.priority = std::stoi(p->second);
        }
        catch (...) {
            t.priority = 0;
        }
    }

    if (auto p = props.find("PERCENT-COMPLETE"); p != props.end()) {
        try {
            t.percent_complete = std::stoi(p->second);
        }
        catch (...) {
            t.percent_complete = 0;
        }
    }

    t.due = parse_dt(props, "DUE");
    t.dtstart = parse_dt(props, "DTSTART");
    t.completed = parse_dt(props, "COMPLETED");
    if (auto c = parse_dt(props, "CREATED"); c.has_value()) t.created = *c;
    else t.created = utc_now_epoch_ms();

    if (auto l = parse_dt(props, "LAST-MODIFIED"); l.has_value()) t.last_modified = *l;
    else t.last_modified = utc_now_epoch_ms();

    if (auto c = props.find("CATEGORIES"); c != props.end()) t.categories = parse_categories(c->second);

    if (auto loc = props.find("LOCATION"); loc != props.end()) t.location = unescape_ic(loc->second);

    if (cal) t.calendar_href = cal;
    if (etag && !etag->empty()) t.etag = etag;
    if (resh && !resh->empty()) t.href = resh;
    return t;
}

std::optional<Note> parse_note_props_clean(
    PropMap const& props, std::optional<std::string> const cal, std::optional<std::string> const resh,
    std::optional<std::string> etag)
{
    auto uid_it = props.find("UID");
    auto sum_it = props.find("SUMMARY");
    if (uid_it == props.end() || sum_it == props.end()) return std::nullopt;
    Note n;
    n.uid = uid_it->second;
    n.summary = unescape_ic(sum_it->second);
    if (auto bo = props.find("DESCRIPTION"); bo != props.end())
        n.body = unescape_ic(bo->second);
    if (auto c = props.find("CATEGORIES"); c != props.end())
        n.categories = parse_categories(c->second);
    std::optional<EpochMillis> st = parse_dt(props, "DTSTAMP");
    if (!st.has_value())
        st = parse_dt(props, "CREATED");
    n.created = st.value_or(utc_now_epoch_ms());
    n.last_modified = parse_dt(props, "LAST-MODIFIED").value_or(utc_now_epoch_ms());
    if (cal) n.calendar_href = cal;
    if (etag && !etag->empty()) n.etag = etag;
    if (resh && !resh->empty()) n.href = resh;
    return n;
}

template <typename T, typename Pf>
std::vector<T> collect_many(std::string const& txt, std::string const comp, Pf parse_one,
    std::optional<std::string> ch, std::optional<std::string> rh, std::optional<std::string> et)
{
    std::vector<T> out;
    for_each_component(txt, comp, [&](PropMap const& pv) {
        if (auto o = parse_one(pv, ch, rh, et); o.has_value())
            out.push_back(std::move(*o));
    });
    return out;
}

} // namespace anonymous

std::vector<CalendarEvent> ICalParser::parse_events(std::string const& text,
    std::optional<std::string> calendar_href,
    std::optional<std::string> resource_href,
    std::optional<std::string> resource_etag)
{
    return collect_many<CalendarEvent>(text, "VEVENT", parse_event_props, calendar_href, resource_href,
        resource_etag);
}

std::vector<CalDavTask> ICalParser::parse_tasks(std::string const& text,
    std::optional<std::string> calendar_href,
    std::optional<std::string> resource_href,
    std::optional<std::string> resource_etag)
{
    return collect_many<CalDavTask>(text, "VTODO", parse_task_props, calendar_href, resource_href,
        resource_etag);
}

std::vector<Note> ICalParser::parse_notes(std::string const& text,
    std::optional<std::string> calendar_href,
    std::optional<std::string> resource_href,
    std::optional<std::string> resource_etag)
{
    return collect_many<Note>(text, "VJOURNAL", parse_note_props_clean, calendar_href, resource_href,
        resource_etag);
}

std::string ICalParser::serialize_task(CalDavTask const& task)
{
    std::ostringstream o;
    auto line = [&](std::string const& s) { o << s << "\r\n"; };
    line("BEGIN:VCALENDAR");
    line("VERSION:2.0");
    line("PRODID:-//CrossDashboard//Linux//EN");
    line("BEGIN:VTODO");
    line(std::string("UID:") + task.uid);
    line("DTSTAMP:" + format_instant_ms_utc(utc_now_epoch_ms()));
    line("CREATED:" + format_instant_ms_utc(task.created));
    line("LAST-MODIFIED:" + format_instant_ms_utc(utc_now_epoch_ms()));
    line("SUMMARY:" + escape_ic(task.summary));
    line(std::string("STATUS:") + task_status_to_ical(task.status));
    line(std::string("PRIORITY:") + std::to_string(task.priority));
    line(std::string("PERCENT-COMPLETE:") + std::to_string(task.percent_complete));
    if (task.description.has_value()) line("DESCRIPTION:" + escape_ic(*task.description));
    if (task.due.has_value()) line("DUE:" + format_instant_ms_utc(*task.due));
    if (task.dtstart.has_value()) line("DTSTART:" + format_instant_ms_utc(*task.dtstart));
    if (task.completed.has_value()) line("COMPLETED:" + format_instant_ms_utc(*task.completed));
    if (!task.categories.empty()) {
        std::string cat;
        for (size_t i = 0; i < task.categories.size(); ++i) {
            if (i) cat += ",";
            cat += task.categories[i];
        }
        line(std::string("CATEGORIES:") + cat);
    }
    if (task.location.has_value()) line("LOCATION:" + escape_ic(*task.location));
    if (task.parent_uid.has_value() && !task.parent_uid->empty())
        line("RELATED-TO;RELTYPE=PARENT:" + *task.parent_uid);
    line("END:VTODO");
    line("END:VCALENDAR");
    return o.str();
}

std::string ICalParser::serialize_note(Note const& note)
{
    std::ostringstream o;
    auto line = [&](std::string const& s) { o << s << "\r\n"; };
    line("BEGIN:VCALENDAR");
    line("VERSION:2.0");
    line("PRODID:-//CrossDashboard//Linux//EN");
    line("BEGIN:VJOURNAL");
    line(std::string("UID:") + note.uid);
    line("DTSTAMP:" + format_instant_ms_utc(utc_now_epoch_ms()));
    line("LAST-MODIFIED:" + format_instant_ms_utc(note.last_modified));
    line("SUMMARY:" + escape_ic(note.summary));
    if (!note.body.empty()) line("DESCRIPTION:" + escape_ic(note.body));
    if (!note.categories.empty()) {
        std::string cat;
        for (size_t i = 0; i < note.categories.size(); ++i) {
            if (i) cat += ",";
            cat += note.categories[i];
        }
        line(std::string("CATEGORIES:") + cat);
    }
    line("END:VJOURNAL");
    line("END:VCALENDAR");
    return o.str();
}

std::string ICalParser::serialize_event(CalendarEvent const& ev)
{
    std::ostringstream o;
    auto line = [&](std::string const& s) { o << s << "\r\n"; };
    line("BEGIN:VCALENDAR");
    line("VERSION:2.0");
    line("PRODID:-//CrossDashboard//Linux//EN");
    line("BEGIN:VEVENT");
    line(std::string("UID:") + ev.uid);
    line("DTSTAMP:" + format_instant_ms_utc(utc_now_epoch_ms()));
    line("DTSTART:" + format_instant_ms_utc(ev.start));
    line("DTEND:" + format_instant_ms_utc(ev.end));
    line("SUMMARY:" + escape_ic(ev.summary));
    if (ev.description.has_value()) line("DESCRIPTION:" + escape_ic(*ev.description));
    if (ev.location.has_value()) line("LOCATION:" + escape_ic(*ev.location));
    line("LAST-MODIFIED:" + format_instant_ms_utc(utc_now_epoch_ms()));
    line("END:VEVENT");
    line("END:VCALENDAR");
    return o.str();
}

} // namespace cd
