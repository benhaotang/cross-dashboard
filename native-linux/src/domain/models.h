#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cd {

inline constexpr std::array<const char*, 8> kAllScreens = {
    "Dashboard", "Inbox", "Events", "Tasks", "Notes", "Issues", "Views", "Capture"};

/** Default sidebar order: `kAllScreens` (matches Android `ALL_SCREENS`) plus Settings (not reorderable with the eight). */
inline std::vector<std::string> default_visible_screens()
{
    std::vector<std::string> v{kAllScreens.begin(), kAllScreens.end()};
    v.emplace_back("Settings");
    return v;
}

[[nodiscard]] inline bool is_primary_screen_name(std::string const& s)
{
    for (auto* p : kAllScreens) {
        if (s == p) return true;
    }
    return false;
}

inline constexpr std::array<const char*, 4> kDefaultKanbanColumns = {
    "backlog", "planned", "inprogress", "done"};

/** Covey quadrant category tags (Android `CoveyTag`). */
inline constexpr std::array<const char*, 4> kCoveyQuadrantTags = {"do", "delay", "delegate", "eliminate"};

/** Milliseconds since UNIX epoch — matches Kotlin `Instant.toEpochMilli()`. */
using EpochMillis = int64_t;

// ─── CalDAV ────────────────────────────────────────────────────────────────

struct CalDavCalendar final {
    std::string href;
    std::string display_name;
    std::optional<std::string> color; // #RRGGBB
    std::optional<std::string> ctag;
    std::vector<std::string> components;
};

struct CalendarEvent final {
    std::string uid;
    std::string summary;
    EpochMillis start{}, end{};
    std::optional<std::string> description;
    std::optional<std::string> location;
    std::optional<std::string> calendar_href;
    std::optional<std::string> etag;
    std::optional<std::string> href;
};

enum class TaskStatus : std::uint8_t {
    NeedsAction,
    InProcess,
    Completed,
    Cancelled,
};

inline const char* task_status_to_ical(TaskStatus s)
{
    switch (s) {
    case TaskStatus::NeedsAction: return "NEEDS-ACTION";
    case TaskStatus::InProcess: return "IN-PROCESS";
    case TaskStatus::Completed: return "COMPLETED";
    case TaskStatus::Cancelled: return "CANCELLED";
    }
    return "NEEDS-ACTION";
}

inline TaskStatus task_status_from_ical(std::string const& raw)
{
    std::string u = raw;
    for (char& ch : u)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (u == "IN-PROCESS") return TaskStatus::InProcess;
    if (u == "COMPLETED") return TaskStatus::Completed;
    if (u == "CANCELLED") return TaskStatus::Cancelled;
    if (u == "NEEDS-ACTION") return TaskStatus::NeedsAction;
    return TaskStatus::NeedsAction;
}

struct CalDavTask final {
    std::string uid;
    std::string summary;
    std::optional<std::string> description;
    TaskStatus status{TaskStatus::NeedsAction};
    int priority{};
    int percent_complete{};
    std::optional<EpochMillis> due;
    std::optional<EpochMillis> dtstart;
    std::optional<EpochMillis> completed;
    EpochMillis created{};
    EpochMillis last_modified{};
    std::vector<std::string> categories;
    std::optional<std::string> location;
    std::optional<std::string> parent_uid;
    std::optional<std::string> calendar_href;
    std::optional<std::string> etag;
    std::optional<std::string> href;
};

struct Note final {
    std::string uid;
    std::string summary;
    std::string body;
    std::vector<std::string> categories;
    EpochMillis created{};
    EpochMillis last_modified{};
    std::optional<std::string> calendar_href;
    std::optional<std::string> etag;
    std::optional<std::string> href;
};

// ─── Gitea ─────────────────────────────────────────────────────────────────

struct GiteaIssue final {
    std::int64_t id{};
    int number{};
    std::string title;
    std::string body;
    std::string state;
    std::vector<std::string> labels;
    std::vector<std::string> assignees;
    EpochMillis created_at{};
    EpochMillis updated_at{};
    std::string repository;
    std::string html_url;
    std::optional<std::int64_t> milestone_id;
    std::optional<std::string> milestone_title;
    std::optional<EpochMillis> milestone_due_on;
};

struct GiteaComment final {
    std::int64_t id{};
    std::string body;
    std::string user;
    EpochMillis created_at{};
};

struct GiteaLabel final {
    std::int64_t id{};
    std::string name;
    std::string color;
};

struct GiteaMilestone final {
    std::int64_t id{};
    std::string title;
    std::optional<EpochMillis> due_on;
    int open_issues{};
    int closed_issues{};
};

struct GiteaAttachment final {
    std::int64_t id{};
    std::string name;
    std::string download_url;
    std::int64_t size{};
    std::string uuid;
};

// ─── Stats ─────────────────────────────────────────────────────────────────

struct DailyStats final {
    std::string date_iso; // YYYY-MM-DD
    int tasks_completed{};
    int pomodoro_sessions{};
    int issues_closed{};
};

enum class CalDavAuthMethod : std::uint8_t {
    NextcloudSso,
    LoginFlowV2,
    Manual,
};

struct CalDavCredentials final {
    CalDavAuthMethod auth_method{};
    std::string server_url;
    std::string username;
    std::optional<std::string> password;
    std::optional<std::string> sso_account_name;
};

struct InboxEvent final {
    CalendarEvent event;
    int duration_minutes{};
};
struct InboxTask final {
    CalDavTask task;
    std::optional<int> estimated_minutes;
};
struct InboxIssue final {
    GiteaIssue issue;
    std::optional<int> estimated_minutes;
};
using InboxItem = std::variant<InboxEvent, InboxTask, InboxIssue>;

// ─── Pomodoro ──────────────────────────────────────────────────────────────

struct PomodoroSettings final {
    int work_minutes{25};
    int short_break_minutes{5};
    int long_break_minutes{15};
    int sessions_until_long_break{4};
};

enum class PomodoroPhase : std::uint8_t { Work, ShortBreak, LongBreak };

inline const char* pomodoro_phase_label(PomodoroPhase p)
{
    switch (p) {
    case PomodoroPhase::Work: return "Focus";
    case PomodoroPhase::ShortBreak: return "Short break";
    case PomodoroPhase::LongBreak: return "Long break";
    }
    return "Focus";
}

struct PomodoroState final {
    PomodoroPhase phase{PomodoroPhase::Work};
    int seconds_left{25 * 60};
    bool running{};
    int current_session{1};
    int completed_sessions{};
    std::string item_title;
    bool active{};
    PomodoroSettings settings;
};

struct TaskDefaults final {
    int morning_hour{8};
    int afternoon_hour{13};
    int night_hour{21};
    int default_hour{10};
};

struct ParsedTask final {
    std::string summary;
    int priority{}; // 0, 1 (high), 5 (med), 9 (low)
    std::vector<std::string> categories;
    std::optional<EpochMillis> due;
};

enum class ThemePreference : std::uint8_t { System, Light, Dark };

struct AppSettings final {
    ThemePreference theme{ThemePreference::System};
    std::vector<std::string> visible_screens{default_visible_screens()};
    std::vector<std::string> kanban_columns{kDefaultKanbanColumns.begin(), kDefaultKanbanColumns.end()};
    PomodoroSettings pomodoro_settings;
    TaskDefaults task_defaults;
    bool notifications_enabled{true};
    int notification_minutes_before{15};
    int widget_sync_interval_minutes{60};
    bool biometric_lock_enabled{};
};

enum class MemoState : std::uint8_t { Normal, Archived };
enum class MemoVisibility : std::uint8_t { Private, Protected, Public };

inline const char* memo_state_name(MemoState s)
{
    return s == MemoState::Archived ? "ARCHIVED" : "NORMAL";
}

inline MemoState memo_state_from_name(std::string const& raw)
{
    return raw == "ARCHIVED" ? MemoState::Archived : MemoState::Normal;
}

inline const char* memo_visibility_name(MemoVisibility v)
{
    switch (v) {
    case MemoVisibility::Private: return "PRIVATE";
    case MemoVisibility::Protected: return "PROTECTED";
    case MemoVisibility::Public: return "PUBLIC";
    }
    return "PRIVATE";
}

inline MemoVisibility memo_visibility_from_name(std::string const& raw)
{
    if (raw == "PROTECTED") return MemoVisibility::Protected;
    if (raw == "PUBLIC") return MemoVisibility::Public;
    return MemoVisibility::Private;
}

struct MemoProperty final {
    bool has_link{};
    bool has_task_list{};
    bool has_incomplete_tasks{};
    std::string title;
};

struct MemosAttachment final {
    std::string name;
    std::string filename;
    std::string external_link;
    std::string type;
    std::int64_t size{};
    std::string memo;
};

struct MemosMemo final {
    std::string name;
    MemoState state{MemoState::Normal};
    std::string content;
    MemoVisibility visibility{MemoVisibility::Private};
    std::vector<std::string> tags;
    bool pinned{};
    std::vector<MemosAttachment> attachments;
    MemoProperty property;
    std::string snippet;
    EpochMillis create_time{};
    EpochMillis display_time{};
    EpochMillis update_time{};
};

/** Memos relation list entry (Kotlin `MemoRelation`). */
struct MemoRelation final {
    std::string memo_name;
    std::string memo_snippet;
    std::string related_memo_name;
    std::string related_memo_snippet;
};

/** Bytes prepared for memo upload (`PendingAttachment`). */
struct PendingAttachment final {
    std::string file_name;
    std::string mime_type;
    std::vector<std::uint8_t> bytes;
};

} // namespace cd
