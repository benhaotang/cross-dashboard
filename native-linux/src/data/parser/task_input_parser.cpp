#include "task_input_parser.h"

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <regex>
#include <sstream>

namespace cd {

namespace {

int gdate_weekday_mon1_sun7(GDateTime* dt)
{
    return static_cast<int>(g_date_time_get_day_of_week(dt));
}

std::string to_lower(std::string s)
{
    for (char& ch : s)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

EpochMillis epoch_ms_from_local(gint year, gint month, gint day, gint hour, gint minute)
{
    g_autoptr(GDateTime) naive = g_date_time_new_local(year, month, day, hour, minute, 0.0);
    if (!naive) return 0;
    return g_date_time_to_unix(naive) * 1000;
}

EpochMillis weekday_future(GDateTime* today_local, int dow_target_mon1, int hour_pick)
{
    gint y = g_date_time_get_year(today_local);
    gint m = g_date_time_get_month(today_local);
    gint d = g_date_time_get_day_of_month(today_local);

    for (gint step = 0; step < 370; ++step) {
        g_autoptr(GDateTime) cand = g_date_time_add_days(today_local, step);
        if (gdate_weekday_mon1_sun7(cand) == dow_target_mon1) {
            bool same_cal = g_date_time_get_year(cand) == y && g_date_time_get_month(cand) == m
                && g_date_time_get_day_of_month(cand) == d;
            if (same_cal)
                cand = g_date_time_add_days(cand, 7);
            return epoch_ms_from_local(g_date_time_get_year(cand), g_date_time_get_month(cand),
                g_date_time_get_day_of_month(cand), hour_pick, 0);
        }
    }
    return 0;
}

EpochMillis extract_due(std::string const& lower, TaskDefaults const& def, GDateTime* today_local)
{
    gint ty = g_date_time_get_year(today_local);
    gint tm = g_date_time_get_month(today_local);
    gint td = g_date_time_get_day_of_month(today_local);

    if (lower.find("tomorrow morning") != std::string::npos) {
        g_autoptr(GDateTime) nx = g_date_time_add_days(today_local, 1);
        return epoch_ms_from_local(
            g_date_time_get_year(nx), g_date_time_get_month(nx), g_date_time_get_day_of_month(nx), def.morning_hour, 0);
    }
    if (lower.find("tomorrow afternoon") != std::string::npos) {
        g_autoptr(GDateTime) nx = g_date_time_add_days(today_local, 1);
        return epoch_ms_from_local(
            g_date_time_get_year(nx), g_date_time_get_month(nx), g_date_time_get_day_of_month(nx), def.afternoon_hour,
            0);
    }
    if (lower.find("tomorrow night") != std::string::npos) {
        g_autoptr(GDateTime) nx = g_date_time_add_days(today_local, 1);
        return epoch_ms_from_local(g_date_time_get_year(nx), g_date_time_get_month(nx),
            g_date_time_get_day_of_month(nx), def.night_hour, 0);
    }
    if (lower.find("tomorrow") != std::string::npos) {
        g_autoptr(GDateTime) nx = g_date_time_add_days(today_local, 1);
        return epoch_ms_from_local(g_date_time_get_year(nx), g_date_time_get_month(nx),
            g_date_time_get_day_of_month(nx), def.default_hour, 0);
    }
    if (lower.find("tonight") != std::string::npos)
        return epoch_ms_from_local(ty, tm, td, def.night_hour, 0);
    if (lower.find("today") != std::string::npos)
        return epoch_ms_from_local(ty, tm, td, def.default_hour, 0);
    if (lower.find("next week") != std::string::npos) {
        g_autoptr(GDateTime) nx = g_date_time_add_days(today_local, 7);
        return epoch_ms_from_local(g_date_time_get_year(nx), g_date_time_get_month(nx),
            g_date_time_get_day_of_month(nx), def.default_hour, 0);
    }

    struct KW {
        char const* w;
        int dow;
    };
    static KW const kws[] = {{"monday", 1}, {"tuesday", 2}, {"wednesday", 3}, {"thursday", 4}, {"friday", 5},
        {"saturday", 6}, {"sunday", 7}};
    for (auto const& kw : kws) {
        if (lower.find(kw.w) != std::string::npos)
            return weekday_future(today_local, kw.dow, def.default_hour);
    }
    return 0;
}

std::string strip_time_keywords_insensitive(std::string text)
{
    static char const* kws[] = {"tomorrow morning", "tomorrow afternoon", "tomorrow night", "tomorrow", "tonight",
        "today", "next week", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
    auto lower_copy = text;
    for (char& ch : lower_copy)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    auto erase_ci_substr = [&](std::string needle) {
        for (;;) {
            auto pos = lower_copy.find(needle);
            if (pos == std::string::npos) break;
            text.erase(pos, needle.size());
            lower_copy.erase(pos, needle.size());
        }
    };
    for (char const* kw : kws) erase_ci_substr(std::string(kw));

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();

    std::string coll;
    bool space = false;
    for (char c : text) {
        bool ws = std::isspace(static_cast<unsigned char>(c)) != 0;
        if (ws) {
            if (!coll.empty()) {
                if (!space)
                    coll += ' ';
                space = true;
            }
        }
        else {
            coll += c;
            space = false;
        }
    }
    while (!coll.empty() && std::isspace(static_cast<unsigned char>(coll.front()))) coll.erase(coll.begin());
    while (!coll.empty() && std::isspace(static_cast<unsigned char>(coll.back()))) coll.pop_back();
    return coll;
}

} // namespace

ParsedTask TaskInputParser::parse(
    std::string const& input, TaskDefaults const& defaults, std::chrono::system_clock::time_point now)
{
    std::string text = input;
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();

    int priority = 0;
    if (text.size() >= 3 && text[0] == '!' && text[1] == '!' && text[2] == '!') {
        priority = 1;
        text.erase(0, 3);
    }
    else if (text.size() >= 2 && text[0] == '!' && text[1] == '!') {
        priority = 5;
        text.erase(0, 2);
    }
    else if (!text.empty() && text.front() == '!') {
        priority = 9;
        text.erase(0, 1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());

    std::vector<std::string> tags;
    {
        std::regex tag_regex(R"(#(\w+))");
        for (auto it = std::sregex_iterator(text.begin(), text.end(), tag_regex); it != std::sregex_iterator(); ++it) {
            tags.push_back(to_lower((*it)[1].str()));
        }
        text = std::regex_replace(text, tag_regex, std::string{});
        text = std::regex_replace(text, std::regex("\\s{2,}"), " ");
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
    }

    auto const tt = std::chrono::system_clock::to_time_t(now);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif

    g_autoptr(GDateTime) today_local =
        g_date_time_new_local(static_cast<int>(lt.tm_year + 1900), static_cast<int>(lt.tm_mon + 1),
            static_cast<int>(lt.tm_mday), static_cast<int>(lt.tm_hour), static_cast<int>(lt.tm_min),
            static_cast<gdouble>(lt.tm_sec));

    std::string const lower = to_lower(text);
    EpochMillis due_ms = extract_due(lower, defaults, today_local);
    if (due_ms != 0)
        text = strip_time_keywords_insensitive(std::move(text));

    std::optional<EpochMillis> due_opt = due_ms == 0 ? std::nullopt : std::optional<EpochMillis>(due_ms);
    return ParsedTask{std::move(text), priority, std::move(tags), due_opt};
}

} // namespace cd
