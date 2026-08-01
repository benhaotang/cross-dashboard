#include "app_container.h"
#include "background/pomodoro_session.h"
#include "background/service_dbus.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/memo_dao.h"
#include "data/db/note_dao.h"
#include "data/db/task_dao.h"
#include "data/parser/task_input_parser.h"
#include "data/prefs/prefs.h"
#include "data/repository/repositories.h"

#include <glib.h>
#include <gio/gio.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using namespace cd;

std::atomic_bool interrupted{false};

void handle_interrupt(int) { interrupted = true; }

EpochMillis now_millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string join(std::vector<std::string> const& values, std::string const& separator)
{
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out += separator;
        out += values[i];
    }
    return out;
}

std::string text_from_args_or_stdin(int argc, char** argv, int first)
{
    std::string text;
    for (int i = first; i < argc; ++i) {
        if (!text.empty()) text += ' ';
        text += argv[i];
    }
    if (!text.empty()) return text;

    if (isatty(STDIN_FILENO)) return {};
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    text = buffer.str();
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    return text;
}

std::string format_epoch(std::optional<EpochMillis> value)
{
    if (!value.has_value() || *value == 0) return "-";
    GDateTime* dt = g_date_time_new_from_unix_local(*value / 1000);
    if (!dt) return "-";
    gchar* formatted = g_date_time_format(dt, "%F %R");
    std::string out = formatted ? formatted : "-";
    g_free(formatted);
    g_date_time_unref(dt);
    return out;
}

std::string one_line(std::string value)
{
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\t', ' ');
    return value;
}

std::string short_calendar(std::optional<std::string> const& href);
struct PomoTarget;

void print_usage(std::ostream& out)
{
    out << "Cross-Dashboard Linux CLI\n\n"
           "Usage:\n"
           "  cross-dashboard-cli task [SMART TEXT]        Create a task (or read stdin)\n"
           "  cross-dashboard-cli capture [TEXT]           Create a private Capture memo (or read stdin)\n"
           "  cross-dashboard-cli list TYPE [--all|--json|--fuzzel]\n"
           "                                                List tasks, events, issues, notes, or capture\n"
           "  cross-dashboard-cli sync                     Sync every configured backend\n"
           "  cross-dashboard-cli waybar                   Stream Waybar custom-module JSON\n"
           "  cross-dashboard-cli pomo TYPE TARGET [--minutes N]\n"
           "  cross-dashboard-cli pomo task -u UID [--minutes N]\n"
           "  cross-dashboard-cli pomo fuzzel [--minutes N]\n"
           "  cross-dashboard-cli pomo status|pause|resume|stop|toggle\n"
           "                                                Run a terminal timer for a task/event/issue\n\n"
           "Smart task example: echo '!!! deploy #work tomorrow morning' | cross-dashboard-cli task\n"
           "Pomodoro title search is fuzzy and asks you to choose from contextual matches.\n"
           "Use task -u UID for an exact, non-interactive-safe task selection.\n";
}

struct ServiceSyncWait {
    GMainLoop* loop{};
    bool completed{};
    bool success{};
    std::vector<std::string> errors;
};

void service_sync_completed(GDBusConnection*, char const*, char const*, char const*, char const*,
    GVariant* parameters, gpointer user_data)
{
    auto& wait = *static_cast<ServiceSyncWait*>(user_data);
    gboolean success = FALSE;
    GVariant* errors = nullptr;
    g_variant_get(parameters, "(b@as)", &success, &errors);
    GVariantIter iter;
    char const* message = nullptr;
    g_variant_iter_init(&iter, errors);
    while (g_variant_iter_next(&iter, "&s", &message)) wait.errors.emplace_back(message);
    g_variant_unref(errors);
    wait.success = success;
    wait.completed = true;
    g_main_loop_quit(wait.loop);
}

gboolean service_sync_timeout(gpointer user_data)
{
    g_main_loop_quit(static_cast<ServiceSyncWait*>(user_data)->loop);
    return G_SOURCE_REMOVE;
}

int request_service_sync()
{
    GError* error = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) {
        std::string message = error ? error->message : "session bus unavailable";
        if (error) g_error_free(error);
        throw std::runtime_error(message);
    }

    ServiceSyncWait wait;
    wait.loop = g_main_loop_new(nullptr, FALSE);
    guint const subscription = g_dbus_connection_signal_subscribe(bus, cd::service_dbus::kBusName,
        cd::service_dbus::kInterface, cd::service_dbus::kSyncCompletedSignal,
        cd::service_dbus::kObjectPath, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
        &service_sync_completed, &wait, nullptr);

    GVariant* reply = g_dbus_connection_call_sync(bus, cd::service_dbus::kBusName,
        cd::service_dbus::kObjectPath, cd::service_dbus::kInterface, cd::service_dbus::kSyncMethod,
        nullptr, G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &error);
    if (!reply) {
        g_dbus_connection_signal_unsubscribe(bus, subscription);
        g_main_loop_unref(wait.loop);
        g_object_unref(bus);
        std::string message = error ? error->message : "background service unavailable";
        if (error) g_error_free(error);
        throw std::runtime_error(message + "; run systemctl --user enable --now crossdashboard.service");
    }
    g_variant_unref(reply);

    guint const timeout = g_timeout_add_seconds(600, &service_sync_timeout, &wait);
    g_main_loop_run(wait.loop);
    if (wait.completed && timeout != 0) g_source_remove(timeout);
    g_dbus_connection_signal_unsubscribe(bus, subscription);
    g_main_loop_unref(wait.loop);
    g_object_unref(bus);

    if (!wait.completed) throw std::runtime_error("background sync timed out");
    for (auto const& item : wait.errors) std::cerr << item << '\n';
    return wait.success ? 0 : 1;
}

int create_task(AppContainer& app, std::string const& input)
{
    if (input.empty()) throw std::runtime_error("task text is empty");
    AppSettings const settings = merged_app_preferences(app.prefs());
    ParsedTask parsed = TaskInputParser::parse(input, settings.task_defaults);
    if (parsed.summary.empty()) throw std::runtime_error("task summary is empty after smart parsing");

    auto calendar = app.secrets().get(CredentialKey::CALDAV_DEFAULT_TASK_CALENDAR);
    if (!calendar.has_value() || calendar->empty())
        throw std::runtime_error("default task calendar is not configured in Settings");

    CalDavTask task;
    task.summary = parsed.summary;
    task.priority = parsed.priority;
    task.categories = parsed.categories;
    task.due = parsed.due;
    task.created = now_millis();
    task.last_modified = task.created;
    CalDavTask const saved = app.tasks().create(task, *calendar);
    std::cout << saved.uid << '\t' << saved.summary;
    if (saved.due.has_value()) std::cout << "\tdue " << format_epoch(saved.due);
    std::cout << '\n';
    return 0;
}

int create_capture(AppContainer& app, std::string const& content)
{
    if (content.empty()) throw std::runtime_error("capture text is empty");
    auto saved = app.memos_repository().create_memo(content, MemoVisibility::Private, {});
    if (!saved.has_value()) throw std::runtime_error("Memos server did not return the created memo");
    std::cout << saved->name << '\n';
    return 0;
}

int list_entities(AppContainer& app, std::string kind, bool show_all, bool json_output, bool fuzzel_output)
{
    kind = lower(std::move(kind));
    nlohmann::json output = nlohmann::json::array();

    if (kind == "task" || kind == "tasks") {
        for (auto const& task : TaskDao(app.db()).get_all()) {
            bool const active = task.status != TaskStatus::Completed && task.status != TaskStatus::Cancelled;
            if (!show_all && !active) continue;
            if (json_output) {
                output.push_back({{"uid", task.uid}, {"summary", task.summary},
                    {"status", task_status_to_ical(task.status)}, {"priority", task.priority},
                    {"due", task.due.value_or(0)}, {"categories", task.categories}});
            }
            else if (fuzzel_output) {
                std::cout << task.uid << '\t' << one_line(task.summary) << " — "
                          << task_status_to_ical(task.status) << " — due " << format_epoch(task.due)
                          << " — " << short_calendar(task.calendar_href) << '\n';
            }
            else {
                std::cout << task.uid << '\t' << task_status_to_ical(task.status) << '\t'
                          << format_epoch(task.due) << '\t' << task.summary << '\n';
            }
        }
    }
    else if (kind == "event" || kind == "events") {
        auto events = EventDao(app.db()).get_all();
        EpochMillis const now = now_millis();
        for (auto const& event : events) {
            if (!show_all && event.end < now) continue;
            if (json_output) {
                output.push_back({{"uid", event.uid}, {"summary", event.summary}, {"start", event.start},
                    {"end", event.end}, {"location", event.location.value_or("")}});
            }
            else if (fuzzel_output) {
                std::cout << event.uid << '\t' << one_line(event.summary) << " — "
                          << format_epoch(event.start) << " — " << short_calendar(event.calendar_href)
                          << '\n';
            }
            else {
                std::cout << event.uid << '\t' << format_epoch(event.start) << '\t' << event.summary << '\n';
            }
        }
    }
    else if (kind == "issue" || kind == "issues") {
        for (auto const& issue : IssueDao(app.db()).get_all()) {
            if (!show_all && issue.state != "open") continue;
            if (json_output) {
                output.push_back({{"id", issue.id}, {"number", issue.number}, {"repository", issue.repository},
                    {"title", issue.title}, {"state", issue.state}, {"labels", issue.labels},
                    {"url", issue.html_url}});
            }
            else if (fuzzel_output) {
                std::cout << issue.repository << '#' << issue.number << '\t' << one_line(issue.title)
                          << " — " << issue.repository << " — " << issue.state << '\n';
            }
            else {
                std::cout << issue.repository << '#' << issue.number << '\t' << issue.state << '\t'
                          << issue.title << '\n';
            }
        }
    }
    else if (kind == "note" || kind == "notes") {
        for (auto const& note : NoteDao(app.db()).get_all()) {
            if (json_output)
                output.push_back({{"uid", note.uid}, {"summary", note.summary}, {"body", note.body}});
            else if (fuzzel_output)
                std::cout << note.uid << '\t' << one_line(note.summary) << " — "
                          << short_calendar(note.calendar_href) << '\n';
            else
                std::cout << note.uid << '\t' << note.summary << '\n';
        }
    }
    else if (kind == "capture" || kind == "captures" || kind == "memos") {
        for (auto const& memo : MemoDao(app.db()).get_all()) {
            if (!show_all && memo.state != MemoState::Normal) continue;
            if (json_output) {
                output.push_back({{"name", memo.name}, {"state", memo_state_name(memo.state)},
                    {"visibility", memo_visibility_name(memo.visibility)}, {"content", memo.content},
                    {"tags", memo.tags}, {"created", memo.create_time}});
            }
            else if (fuzzel_output) {
                std::cout << memo.name << '\t' << one_line(memo.content) << " — "
                          << memo_visibility_name(memo.visibility) << " — "
                          << memo_state_name(memo.state) << '\n';
            }
            else {
                std::cout << memo.name << '\t' << memo_state_name(memo.state) << '\t'
                          << one_line(memo.content) << '\n';
            }
        }
    }
    else {
        throw std::runtime_error("unknown list type: " + kind);
    }

    if (json_output) std::cout << output.dump(2) << '\n';
    return 0;
}

struct PomoTarget {
    std::string kind;
    std::string id;
    std::string title;
    std::string context;
    std::optional<CalDavTask> task;
};

struct RankedPomoTarget {
    PomoTarget target;
    int score{};
};

std::size_t edit_distance(std::string const& left, std::string const& right)
{
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t j = 0; j <= right.size(); ++j) previous[j] = j;
    for (std::size_t i = 1; i <= left.size(); ++i) {
        current[0] = i;
        for (std::size_t j = 1; j <= right.size(); ++j) {
            std::size_t const substitution = previous[j - 1] + (left[i - 1] == right[j - 1] ? 0 : 1);
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
        }
        previous.swap(current);
    }
    return previous.back();
}

int title_match_score(std::string const& title, std::string const& selector)
{
    std::string const haystack = lower(title);
    std::string const needle = lower(selector);
    if (needle.empty()) return -1;
    if (haystack == needle) return 1000;
    if (auto const pos = haystack.find(needle); pos != std::string::npos)
        return 800 - static_cast<int>(std::min<std::size_t>(haystack.size() - needle.size(), 150));

    std::size_t const length = std::max(haystack.size(), needle.size());
    if (length == 0) return -1;
    double const similarity = 1.0 - static_cast<double>(edit_distance(haystack, needle)) / length;
    return similarity >= 0.55 ? static_cast<int>(similarity * 600.0) : -1;
}

std::string short_calendar(std::optional<std::string> const& href)
{
    if (!href.has_value() || href->empty()) return "calendar unknown";
    std::string value = *href;
    while (!value.empty() && value.back() == '/') value.pop_back();
    auto const slash = value.find_last_of('/');
    return "calendar " + (slash == std::string::npos ? value : value.substr(slash + 1));
}

PomoTarget choose_interactively(std::string const& kind, std::string const& selector,
    std::vector<RankedPomoTarget> matches)
{
    if (matches.empty()) throw std::runtime_error("no cached " + kind + " matches: " + selector);
    std::stable_sort(matches.begin(), matches.end(), [](auto const& a, auto const& b) {
        return a.score > b.score;
    });
    if (matches.size() > 10) matches.resize(10);

    if (!isatty(STDIN_FILENO)) {
        throw std::runtime_error("title matching needs an interactive terminal; use -u UID for a task "
            "(find it with `cross-dashboard-cli list tasks`)");
    }

    std::cerr << "Matches for \"" << selector << "\":\n";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        auto const& target = matches[i].target;
        std::cerr << "  " << i + 1 << ") " << target.title;
        if (!target.context.empty()) std::cerr << " — " << target.context;
        std::cerr << " [" << target.id << "]\n";
    }
    std::cerr << "Choose 1-" << matches.size() << " (or q to cancel): " << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer) || answer == "q" || answer == "Q")
        throw std::runtime_error("Pomodoro selection cancelled");
    std::size_t consumed = 0;
    unsigned long const selected = std::stoul(answer, &consumed);
    if (consumed != answer.size() || selected < 1 || selected > matches.size())
        throw std::runtime_error("invalid Pomodoro selection");
    return matches[selected - 1].target;
}

PomoTarget find_pomo_target(AppContainer& app, std::string kind, std::string const& selector,
    bool exact_task_uid)
{
    kind = lower(std::move(kind));
    if (kind == "task") {
        auto tasks = TaskDao(app.db()).get_all();
        if (exact_task_uid) {
            auto it = std::find_if(tasks.begin(), tasks.end(), [&](CalDavTask const& task) {
                return task.uid == selector;
            });
            if (it == tasks.end()) throw std::runtime_error("no cached task has UID: " + selector);
            return {kind, it->uid, it->summary,
                short_calendar(it->calendar_href) + ", " + task_status_to_ical(it->status)
                    + ", due " + format_epoch(it->due),
                *it};
        }
        std::vector<RankedPomoTarget> matches;
        for (auto const& task : tasks) {
            int score = title_match_score(task.summary, selector);
            if (score < 0) continue;
            if (task.status != TaskStatus::Completed && task.status != TaskStatus::Cancelled) score += 25;
            matches.push_back({{kind, task.uid, task.summary,
                short_calendar(task.calendar_href) + ", " + task_status_to_ical(task.status)
                    + ", due " + format_epoch(task.due),
                task}, score});
        }
        return choose_interactively(kind, selector, std::move(matches));
    }
    else if (kind == "event") {
        if (exact_task_uid) throw std::runtime_error("-u/--uid is currently supported for task Pomodoros");
        auto events = EventDao(app.db()).get_all();
        std::vector<RankedPomoTarget> matches;
        for (auto const& event : events) {
            int const score = title_match_score(event.summary, selector);
            if (score >= 0) matches.push_back({{kind, event.uid, event.summary,
                short_calendar(event.calendar_href) + ", starts " + format_epoch(event.start),
                std::nullopt}, score});
        }
        return choose_interactively(kind, selector, std::move(matches));
    }
    else if (kind == "issue") {
        if (exact_task_uid) throw std::runtime_error("-u/--uid is currently supported for task Pomodoros");
        auto issues = IssueDao(app.db()).get_all();
        for (auto const& issue : issues) {
            std::string const qualified = issue.repository + "#" + std::to_string(issue.number);
            if (selector == qualified)
                return {kind, qualified, issue.title, issue.state, std::nullopt};
        }
        std::vector<RankedPomoTarget> matches;
        for (auto const& issue : issues) {
            int score = title_match_score(issue.title, selector);
            if (selector == std::to_string(issue.number)) score = 950;
            if (score >= 0) matches.push_back({{kind,
                issue.repository + "#" + std::to_string(issue.number), issue.title,
                issue.repository + ", " + issue.state, std::nullopt}, score});
        }
        return choose_interactively(kind, selector, std::move(matches));
    }
    else {
        throw std::runtime_error("Pomodoro type must be task, event, or issue");
    }
    throw std::runtime_error("no cached " + kind + " matches: " + selector);
}

PomoTarget find_exact_pomo_target(AppContainer& app, std::string const& key)
{
    auto const separator = key.find(':');
    if (separator == std::string::npos) throw std::runtime_error("invalid Fuzzel selection");
    std::string const kind = key.substr(0, separator);
    std::string const id = key.substr(separator + 1);
    if (kind == "task") {
        auto task = TaskDao(app.db()).get_by_uid(id);
        if (!task) throw std::runtime_error("selected task is no longer in the cache: " + id);
        return {kind, task->uid, task->summary,
            short_calendar(task->calendar_href) + ", due " + format_epoch(task->due), *task};
    }
    if (kind == "event") {
        for (auto const& event : EventDao(app.db()).get_all()) {
            if (event.uid == id)
                return {kind, event.uid, event.summary,
                    short_calendar(event.calendar_href) + ", starts " + format_epoch(event.start),
                    std::nullopt};
        }
        throw std::runtime_error("selected event is no longer in the cache: " + id);
    }
    if (kind == "issue") {
        for (auto const& issue : IssueDao(app.db()).get_all()) {
            std::string const qualified = issue.repository + "#" + std::to_string(issue.number);
            if (qualified == id)
                return {kind, qualified, issue.title, issue.repository + ", " + issue.state,
                    std::nullopt};
        }
        throw std::runtime_error("selected issue is no longer in the cache: " + id);
    }
    throw std::runtime_error("invalid Fuzzel Pomodoro type: " + kind);
}

PomoTarget choose_pomodoro_with_fuzzel(AppContainer& app)
{
    std::ostringstream choices;
    for (auto const& task : TaskDao(app.db()).get_all()) {
        if (task.status == TaskStatus::Completed || task.status == TaskStatus::Cancelled) continue;
        choices << "task:" << task.uid << '\t' << "Task · " << one_line(task.summary)
                << " · due " << format_epoch(task.due) << " · " << short_calendar(task.calendar_href)
                << '\n';
    }
    EpochMillis const now = now_millis();
    for (auto const& event : EventDao(app.db()).get_all()) {
        if (event.end < now) continue;
        choices << "event:" << event.uid << '\t' << "Event · " << one_line(event.summary)
                << " · " << format_epoch(event.start) << " · " << short_calendar(event.calendar_href)
                << '\n';
    }
    for (auto const& issue : IssueDao(app.db()).get_all()) {
        if (issue.state != "open") continue;
        std::string const id = issue.repository + "#" + std::to_string(issue.number);
        choices << "issue:" << id << '\t' << "Issue · " << one_line(issue.title)
                << " · " << id << '\n';
    }
    std::string const input = choices.str();
    if (input.empty()) throw std::runtime_error("no active cached tasks, upcoming events, or open issues");

    GError* error = nullptr;
    GSubprocess* process = g_subprocess_new(
        static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE
            | G_SUBPROCESS_FLAGS_STDERR_SILENCE),
        &error, "fuzzel", "--dmenu", "--with-nth=2", "--prompt=Pomodoro> ", nullptr);
    if (!process) {
        std::string message = error ? error->message : "could not start fuzzel";
        if (error) g_error_free(error);
        throw std::runtime_error(message + "; install fuzzel or select with task -u UID");
    }
    gchar* selected = nullptr;
    gboolean const communicated =
        g_subprocess_communicate_utf8(process, input.c_str(), nullptr, &selected, nullptr, &error);
    bool const successful = communicated && g_subprocess_get_successful(process);
    std::string result = selected ? selected : "";
    g_free(selected);
    g_object_unref(process);
    if (!communicated) {
        std::string message = error ? error->message : "fuzzel communication failed";
        if (error) g_error_free(error);
        throw std::runtime_error(message);
    }
    if (error) g_error_free(error);
    if (!successful || result.empty()) throw std::runtime_error("Pomodoro selection cancelled");
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    auto const tab = result.find('\t');
    return find_exact_pomo_target(app, result.substr(0, tab));
}

struct ServicePomodoroState {
    bool active{};
    bool running{};
    int seconds_left{};
    int duration_seconds{};
    int completed_sessions{};
    std::string phase;
    std::string kind;
    std::string id;
    std::string title;
};

GVariant* call_service(char const* method, GVariant* parameters, GVariantType const* reply_type)
{
    GError* error = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) {
        std::string message = error ? error->message : "session bus unavailable";
        if (error) g_error_free(error);
        throw std::runtime_error(message);
    }
    GVariant* reply = g_dbus_connection_call_sync(bus, cd::service_dbus::kBusName,
        cd::service_dbus::kObjectPath, cd::service_dbus::kInterface, method, parameters,
        reply_type, G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &error);
    g_object_unref(bus);
    if (!reply) {
        std::string message = error ? error->message : "background service unavailable";
        if (error) g_error_free(error);
        throw std::runtime_error(message
            + "; run systemctl --user enable --now crossdashboard.service");
    }
    return reply;
}

ServicePomodoroState service_pomodoro_state()
{
    GVariant* reply = call_service(cd::service_dbus::kGetPomodoroStateMethod, nullptr,
        G_VARIANT_TYPE(cd::service_dbus::kPomodoroStateTupleType));
    gboolean active = FALSE;
    gboolean running = FALSE;
    gint seconds = 0;
    gint duration = 0;
    gint completed = 0;
    char const* phase = nullptr;
    char const* kind = nullptr;
    char const* id = nullptr;
    char const* title = nullptr;
    g_variant_get(reply, "(bbiii&s&s&s&s)", &active, &running, &seconds, &duration, &completed,
        &phase, &kind, &id, &title);
    ServicePomodoroState result{active != FALSE, running != FALSE, seconds, duration, completed,
        phase ? phase : "", kind ? kind : "", id ? id : "", title ? title : ""};
    g_variant_unref(reply);
    return result;
}

std::string printable_phase(std::string const& phase)
{
    if (phase == "short-break") return "Short break";
    if (phase == "long-break") return "Long break";
    return "Focus";
}

void print_pomodoro_status(ServicePomodoroState const& state, bool carriage_return = false)
{
    if (!state.active) {
        std::cout << "No active Pomodoro\n";
        return;
    }
    int const mm = state.seconds_left / 60;
    int const ss = state.seconds_left % 60;
    if (carriage_return) std::cout << '\r';
    std::cout << printable_phase(state.phase) << ' ' << std::setfill('0') << std::setw(2) << mm
              << ':' << std::setw(2) << ss << (state.running ? "" : " paused");
    if (!state.title.empty()) std::cout << " — " << state.title;
    if (!state.kind.empty()) std::cout << " (" << state.kind << ' ' << state.id << ')';
    if (carriage_return) std::cout << "        " << std::flush;
    else std::cout << '\n';
}

bool service_pomodoro_control(char const* method)
{
    GVariant* reply = call_service(method, nullptr, G_VARIANT_TYPE("(b)"));
    gboolean changed = FALSE;
    g_variant_get(reply, "(b)", &changed);
    g_variant_unref(reply);
    return changed != FALSE;
}

int run_pomodoro_target(AppContainer& app, PomoTarget target, int minutes)
{
    if (minutes <= 0) minutes = merged_app_preferences(app.prefs()).pomodoro_settings.work_minutes;
    if (minutes <= 0 || minutes > 24 * 60)
        throw std::runtime_error("Pomodoro duration must be between 1 minute and 24 hours");
    GVariant* reply = call_service(cd::service_dbus::kStartPomodoroMethod,
        g_variant_new("(ssssi)", target.kind.c_str(), target.id.c_str(), target.title.c_str(),
            "focus", minutes), G_VARIANT_TYPE("(b)"));
    gboolean started = FALSE;
    g_variant_get(reply, "(b)", &started);
    g_variant_unref(reply);
    if (!started) {
        auto const current = service_pomodoro_state();
        std::cerr << "A Pomodoro is already active:\n";
        print_pomodoro_status(current);
        return 2;
    }

    auto const initial = service_pomodoro_state();
    std::cout << "Started in background: " << target.title << " (" << target.kind << ' '
              << target.id << ")\n";
    if (!isatty(STDOUT_FILENO)) return 0;
    std::signal(SIGINT, handle_interrupt);
    std::signal(SIGTERM, handle_interrupt);
    while (!interrupted.load()) {
        auto const current = service_pomodoro_state();
        if (!current.active || current.completed_sessions > initial.completed_sessions
            || current.phase != "focus") {
            std::cout << "\rPomodoro complete: " << target.title << "                    \n";
            return 0;
        }
        print_pomodoro_status(current, true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\nPomodoro continues in the background; use `pomo stop` to stop it.\n";
    return 130;
}

int run_pomodoro(AppContainer& app, std::string const& kind, std::string const& selector, int minutes,
    bool exact_task_uid)
{
    return run_pomodoro_target(
        app, find_pomo_target(app, kind, selector, exact_task_uid), minutes);
}

std::string compact_title(std::string const& title, glong max_chars = 32)
{
    if (!g_utf8_validate(title.c_str(), title.size(), nullptr)) return one_line(title).substr(0, max_chars);
    if (g_utf8_strlen(title.c_str(), title.size()) <= max_chars) return one_line(title);
    gchar* shortened = g_utf8_substring(title.c_str(), 0, max_chars - 1);
    std::string result = shortened ? shortened : "";
    g_free(shortened);
    return one_line(result) + "…";
}

std::string compact_when(EpochMillis timestamp)
{
    GDateTime* value = g_date_time_new_from_unix_local(timestamp / 1000);
    GDateTime* now = g_date_time_new_now_local();
    if (!value || !now) {
        if (value) g_date_time_unref(value);
        if (now) g_date_time_unref(now);
        return "Soon";
    }
    bool const today = g_date_time_get_year(value) == g_date_time_get_year(now)
        && g_date_time_get_day_of_year(value) == g_date_time_get_day_of_year(now);
    gchar* formatted = g_date_time_format(value, today ? "%R" : "%a %R");
    std::string result = formatted ? formatted : "Soon";
    g_free(formatted);
    g_date_time_unref(value);
    g_date_time_unref(now);
    return result;
}

struct UpcomingStatus {
    int priority{};
    EpochMillis timestamp{};
    std::string kind;
    std::string title;
    std::string tooltip;
};

std::optional<UpcomingStatus> next_waybar_item(AppContainer& app)
{
    EpochMillis const now = now_millis();
    std::vector<UpcomingStatus> candidates;
    for (auto const& event : EventDao(app.db()).get_all()) {
        if (event.end < now) continue;
        bool const happening = event.start <= now;
        candidates.push_back({happening ? 0 : 2, happening ? now : event.start,
            happening ? "event-active" : "event", event.summary,
            std::string{happening ? "Happening now: " : "Upcoming event: "} + event.summary
                + "\n" + format_epoch(event.start) + " – " + format_epoch(event.end)
                + "\n" + short_calendar(event.calendar_href)});
    }
    for (auto const& task : TaskDao(app.db()).get_all()) {
        if (!task.due || task.status == TaskStatus::Completed || task.status == TaskStatus::Cancelled)
            continue;
        bool const overdue = *task.due < now;
        candidates.push_back({overdue ? 1 : 2, *task.due, overdue ? "task-overdue" : "task",
            task.summary, std::string{overdue ? "Overdue task: " : "Task due: "} + task.summary
                + "\n" + format_epoch(task.due) + "\n" + short_calendar(task.calendar_href)});
    }
    if (candidates.empty()) return std::nullopt;
    return *std::min_element(candidates.begin(), candidates.end(), [](auto const& a, auto const& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.timestamp < b.timestamp;
    });
}

nlohmann::json waybar_payload(AppContainer& app)
{
    if (auto session = read_pomodoro_session()) {
        int const left = current_seconds_left(*session, now_millis());
        int const mm = left / 60;
        int const ss = left % 60;
        std::ostringstream timer;
        timer << "󰔛 " << std::setfill('0') << std::setw(2) << mm << ':' << std::setw(2) << ss
              << ' ' << compact_title(session->title, 24);
        return {{"text", timer.str()},
            {"tooltip", session->phase + ": " + session->title + "\n" + session->kind + " "
                    + session->id + "\nLeft click: pause/resume · Right click: sync"},
            {"class", "pomodoro"}, {"alt", "pomodoro"}};
    }
    auto item = next_waybar_item(app);
    if (!item) return {{"text", ""}, {"tooltip", "No upcoming events or due tasks"},
        {"class", "idle"}, {"alt", "idle"}};
    std::string prefix;
    if (item->kind == "event-active") prefix = " Now";
    else if (item->kind == "task-overdue") prefix = " Overdue";
    else prefix = std::string{item->kind == "task" ? " " : " "} + compact_when(item->timestamp);
    return {{"text", prefix + " " + compact_title(item->title)},
        {"tooltip", item->tooltip + "\nLeft click: choose Pomodoro · Right click: sync"},
        {"class", item->kind}, {"alt", item->kind}};
}

int stream_waybar(AppContainer& app)
{
    std::signal(SIGINT, handle_interrupt);
    std::signal(SIGTERM, handle_interrupt);
    while (!interrupted.load()) {
        std::cout << waybar_payload(app).dump() << '\n' << std::flush;
        for (int i = 0; i < 10 && !interrupted.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || std::string{argv[1]} == "--help" || std::string{argv[1]} == "-h") {
        print_usage(std::cout);
        return argc < 2 ? 2 : 0;
    }

    try {
        cd::AppContainer app;
        std::string const command = lower(argv[1]);
        if (command == "task") return create_task(app, text_from_args_or_stdin(argc, argv, 2));
        if (command == "capture") return create_capture(app, text_from_args_or_stdin(argc, argv, 2));
        if (command == "sync") return request_service_sync();
        if (command == "waybar") {
            if (argc != 2) throw std::runtime_error("waybar does not accept arguments");
            return stream_waybar(app);
        }
        if (command == "list") {
            if (argc < 3) throw std::runtime_error("list requires an entity type");
            bool all = false;
            bool json = false;
            bool fuzzel = false;
            for (int i = 3; i < argc; ++i) {
                std::string const option = argv[i];
                if (option == "--all") all = true;
                else if (option == "--json") json = true;
                else if (option == "--fuzzel") fuzzel = true;
                else throw std::runtime_error("unknown list option: " + option);
            }
            if (json && fuzzel) throw std::runtime_error("--json and --fuzzel cannot be combined");
            return list_entities(app, argv[2], all, json, fuzzel);
        }
        if (command == "pomo" || command == "pomodoro") {
            if (argc < 3) throw std::runtime_error("pomo requires TYPE and TARGET, or `pomo fuzzel`");
            std::string const pomo_action = lower(argv[2]);
            if (pomo_action == "status") {
                if (argc != 3) throw std::runtime_error("pomo status does not accept arguments");
                print_pomodoro_status(service_pomodoro_state());
                return 0;
            }
            if (pomo_action == "toggle") {
                if (argc != 3) throw std::runtime_error("pomo toggle does not accept arguments");
                auto const current = service_pomodoro_state();
                if (!current.active)
                    return run_pomodoro_target(app, choose_pomodoro_with_fuzzel(app), 0);
                char const* method = current.running ? cd::service_dbus::kPausePomodoroMethod
                                                     : cd::service_dbus::kResumePomodoroMethod;
                if (!service_pomodoro_control(method)) return 2;
                std::cout << "Pomodoro " << (current.running ? "paused" : "resumed") << '\n';
                return 0;
            }
            if (pomo_action == "pause" || pomo_action == "resume" || pomo_action == "stop") {
                if (argc != 3) throw std::runtime_error("pomo " + pomo_action + " does not accept arguments");
                char const* method = pomo_action == "pause" ? cd::service_dbus::kPausePomodoroMethod
                    : pomo_action == "resume" ? cd::service_dbus::kResumePomodoroMethod
                                                : cd::service_dbus::kStopPomodoroMethod;
                bool const changed = service_pomodoro_control(method);
                if (!changed) {
                    std::cerr << "Pomodoro was not " << (pomo_action == "pause" ? "running"
                        : pomo_action == "resume" ? "paused" : "active") << '\n';
                    return 2;
                }
                std::cout << "Pomodoro " << (pomo_action == "pause" ? "paused"
                    : pomo_action == "resume" ? "resumed" : "stopped") << '\n';
                return 0;
            }
            if (pomo_action == "fuzzel") {
                int minutes = 0;
                for (int i = 3; i < argc; ++i) {
                    std::string const value = argv[i];
                    if (value != "--minutes" || ++i >= argc)
                        throw std::runtime_error("pomo fuzzel only accepts --minutes N");
                    minutes = std::stoi(argv[i]);
                }
                return run_pomodoro_target(app, choose_pomodoro_with_fuzzel(app), minutes);
            }
            if (argc < 4) throw std::runtime_error("pomo requires TYPE and TARGET");
            int minutes = 0;
            bool exact_task_uid = false;
            std::vector<std::string> selector_parts;
            for (int i = 3; i < argc; ++i) {
                std::string const value = argv[i];
                if (value == "--minutes") {
                    if (++i >= argc) throw std::runtime_error("--minutes requires a number");
                    minutes = std::stoi(argv[i]);
                }
                else if (value == "-u" || value == "--uid") {
                    if (exact_task_uid) throw std::runtime_error("task UID was specified more than once");
                    if (++i >= argc) throw std::runtime_error(value + " requires a UID");
                    exact_task_uid = true;
                    selector_parts.push_back(argv[i]);
                }
                else selector_parts.push_back(value);
            }
            if (selector_parts.empty()) throw std::runtime_error("pomo target is empty");
            if (exact_task_uid && selector_parts.size() != 1)
                throw std::runtime_error("-u/--uid accepts exactly one UID");
            return run_pomodoro(app, argv[2], join(selector_parts, " "), minutes, exact_task_uid);
        }

        throw std::runtime_error("unknown command: " + command);
    }
    catch (std::exception const& error) {
        std::cerr << "cross-dashboard-cli: " << error.what() << '\n';
        return 1;
    }
}
