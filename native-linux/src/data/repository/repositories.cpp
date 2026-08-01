#include "repositories.h"

#include "operation_lock.h"
#include "repo_utils.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/memo_dao.h"
#include "data/db/note_dao.h"
#include "data/db/stats_dao.h"
#include "data/db/task_dao.h"
#include "data/network/caldav_client.h"
#include "data/network/gitea_client.h"
#include "data/network/memos_client.h"
#include "data/prefs/prefs.h"

#include <glib.h>

#include <chrono>
#include <initializer_list>
#include <limits>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

constexpr std::int64_t kMsPerDay64 = std::int64_t{86400} * std::int64_t{1000};

std::string gdate_format_yyyy_mm_dd(GDateTime* dt)
{
    if (!dt) return {};
    gchar* s = g_date_time_format(dt, "%F");
    std::string out = s ? s : "";
    g_free(s);
    return out;
}

std::string local_date_iso()
{
    GDateTime* now = g_date_time_new_now_local();
    std::string o = gdate_format_yyyy_mm_dd(now);
    if (now) g_date_time_unref(now);
    return o;
}

std::string local_date_minus_days(int days_back)
{
    GDateTime* now = g_date_time_new_now_local();
    GDateTime* past = now ? g_date_time_add_days(now, -days_back) : nullptr;
    std::string o = gdate_format_yyyy_mm_dd(past);
    if (past) g_date_time_unref(past);
    if (now) g_date_time_unref(now);
    return o;
}

} // namespace

// ─── Event ──────────────────────────────────────────────────────────────────

EventRepository::EventRepository(EventDao& dao, CalDavClient& client, SecretStore& secrets)
    : dao_(dao)
    , client_(client)
    , secrets_(secrets)
{
}

std::vector<std::string> EventRepository::selected_calendar_hrefs() const
{
    return calendars_from_selected_json(secrets_.get(CredentialKey::CALDAV_SELECTED_CALENDARS));
}

void EventRepository::sync_many(std::vector<std::string> const& calendar_hrefs)
{
    OperationLock operation_lock;
    using namespace std::chrono;
    if (calendar_hrefs.empty()) return;
    EpochMillis now_m = millis_now_wall();
    auto from_ms = now_m - 30LL * kMsPerDay64;
    auto to_ms = now_m + 180LL * kMsPerDay64;
    auto fresh =
        client_.fetch_events(calendar_hrefs, milliseconds(from_ms), milliseconds(to_ms));

    auto upcoming = dao_.get_upcoming(0, 1);
    if (!fresh.empty() || !upcoming.empty()) {
        dao_.replace_all(fresh);
    }
}

std::vector<CalendarEvent> EventRepository::get_upcoming(int limit)
{
    return dao_.get_upcoming(millis_now_wall(), limit);
}

CalendarEvent EventRepository::create(CalendarEvent const& event, std::string const& calendar_href)
{
    OperationLock operation_lock;
    auto saved = client_.create_event(event, calendar_href);
    dao_.upsert_all(std::vector<CalendarEvent>{saved});
    return saved;
}

void EventRepository::remove(CalendarEvent const& event)
{
    OperationLock operation_lock;
    client_.delete_event(event);
    dao_.delete_all();
}

// ─── Task ────────────────────────────────────────────────────────────────────

TaskRepository::TaskRepository(TaskDao& dao, DailyStatsDao& stats, CalDavClient& client)
    : dao_(dao)
    , stats_(stats)
    , client_(client)
{
}

void TaskRepository::sync_many(std::vector<std::string> const& calendar_hrefs)
{
    OperationLock operation_lock;
    if (calendar_hrefs.empty()) return;
    auto fresh = client_.fetch_tasks(calendar_hrefs);
    auto duepeek = dao_.get_due_soon(std::numeric_limits<EpochMillis>::max(), 1);
    if (!fresh.empty() || !duepeek.empty()) {
        dao_.replace_all(fresh);
    }
}

CalDavTask TaskRepository::create(CalDavTask const& task, std::string const& calendar_href)
{
    OperationLock operation_lock;
    auto saved = client_.create_task(task, calendar_href);
    dao_.upsert_all(std::vector<CalDavTask>{saved});
    return saved;
}

void TaskRepository::update(CalDavTask const& task)
{
    OperationLock operation_lock;
    client_.update_task(task);
    dao_.upsert_all(std::vector<CalDavTask>{task});
}

void TaskRepository::remove(CalDavTask const& task)
{
    OperationLock operation_lock;
    client_.delete_task(task);
    dao_.delete_by_uid(task.uid);
}

CalDavTask TaskRepository::toggle_complete(CalDavTask const& task)
{
    bool was_completed = task.status == TaskStatus::Completed;
    CalDavTask updated = task;
    if (was_completed) {
        updated.status = TaskStatus::NeedsAction;
        updated.completed = std::nullopt;
        updated.percent_complete = 0;
    }
    else {
        updated.status = TaskStatus::Completed;
        updated.completed = millis_now_wall();
        updated.percent_complete = 100;
    }
    update(updated);
    if (!was_completed) stats_.increment(local_date_iso(), StatField::TasksCompleted);
    return updated;
}

// ─── Note ────────────────────────────────────────────────────────────────────

NoteRepository::NoteRepository(NoteDao& dao, CalDavClient& client)
    : dao_(dao)
    , client_(client)
{
}

void NoteRepository::sync_many(std::vector<std::string> const& calendar_hrefs)
{
    OperationLock operation_lock;
    if (calendar_hrefs.empty()) return;
    auto fresh = client_.fetch_notes(calendar_hrefs);
    bool no_sentinel = !dao_.get_by_uid("_").has_value();
    if (!fresh.empty() || no_sentinel) {
        dao_.replace_all(fresh);
    }
}

Note NoteRepository::create(Note const& note, std::string const& calendar_href)
{
    OperationLock operation_lock;
    auto saved = client_.create_note(note, calendar_href);
    dao_.upsert_all(std::vector<Note>{saved});
    return saved;
}

void NoteRepository::update(Note const& note)
{
    OperationLock operation_lock;
    client_.update_note(note);
    dao_.upsert_all(std::vector<Note>{note});
}

void NoteRepository::remove(Note const& note)
{
    OperationLock operation_lock;
    client_.delete_note(note);
    dao_.delete_by_uid(note.uid);
}

// ─── Issue ───────────────────────────────────────────────────────────────────

IssueRepository::IssueRepository(IssueDao& dao, DailyStatsDao& stats, GiteaClient& client)
    : dao_(dao)
    , stats_(stats)
    , client_(client)
{
}

void IssueRepository::sync_many(std::vector<std::string> const& repositories)
{
    OperationLock operation_lock;
    if (repositories.empty()) return;
    auto open = client_.fetch_issues(repositories, "open");
    auto closed = client_.fetch_issues(repositories, "closed");
    std::vector<GiteaIssue> all;
    all.reserve(open.size() + closed.size());
    all.insert(all.end(), open.begin(), open.end());
    all.insert(all.end(), closed.begin(), closed.end());
    if (!all.empty()) {
        dao_.replace_all(all);
    }
}

GiteaIssue IssueRepository::update_issue(std::string const& repo, int number,
    std::optional<std::string> title, std::optional<std::string> body, std::optional<std::string> state)
{
    OperationLock operation_lock;
    auto updated = client_.update_issue(repo, number, title, body, state);
    dao_.upsert_all(std::vector<GiteaIssue>{updated});
    if (state.has_value() && *state == "closed")
        stats_.increment(local_date_iso(), StatField::IssuesClosed);
    return updated;
}

std::vector<GiteaComment> IssueRepository::fetch_comments(std::string const& repo, int number)
{
    return client_.fetch_comments(repo, number);
}

void IssueRepository::add_comment(std::string const& repo, int number, std::string const& body)
{
    OperationLock operation_lock;
    (void)client_.add_comment(repo, number, body);
}

void IssueRepository::replace_labels(std::string const& repo, int number, std::vector<std::string> label_names)
{
    OperationLock operation_lock;
    auto existing = client_.fetch_labels(repo);
    std::vector<std::int64_t> ids;
    ids.reserve(label_names.size());
    for (auto const& name : label_names) {
        std::int64_t id = 0;
        bool found = false;
        for (auto const& el : existing) {
            if (el.name == name) {
                id = el.id;
                found = true;
                break;
            }
        }
        if (!found) {
            auto created = client_.create_repo_label(repo, name, "0075ca");
            id = created.id;
        }
        ids.push_back(id);
    }
    client_.replace_issue_labels(repo, number, ids);

    auto fresh_open = client_.fetch_issues(std::vector<std::string>{repo}, "open");
    auto fresh_closed = client_.fetch_issues(std::vector<std::string>{repo}, "closed");
    for (auto const& i : fresh_open) {
        if (i.number == number) {
            dao_.upsert_all(std::vector<GiteaIssue>{i});
            return;
        }
    }
    for (auto const& i : fresh_closed) {
        if (i.number == number) {
            dao_.upsert_all(std::vector<GiteaIssue>{i});
            return;
        }
    }
}

GiteaIssue IssueRepository::create_issue(std::string const& repo, std::string const& title, std::string const& body)
{
    OperationLock operation_lock;
    auto issue = client_.create_issue(repo, title, body);
    dao_.upsert_all(std::vector<GiteaIssue>{issue});
    return issue;
}

// ─── Memos ─────────────────────────────────────────────────────────────────

MemoRepository::MemoRepository(MemoDao& dao, MemosClient& client)
    : dao_(dao)
    , client_(client)
{
}

void MemoRepository::sync_all()
{
    OperationLock operation_lock;
    if (!client_.base_url_opt().has_value()) return;

    std::vector<MemosMemo> all_fetched;

    std::initializer_list<MemoState> kinds{MemoState::Normal, MemoState::Archived};
    for (auto st : kinds) {
        std::optional<std::string> page{};
        std::optional<std::string> next{};
        do {
            auto pair = client_.list_memos(page, std::nullopt, st);
            auto const& slice = pair.first;
            next = pair.second;
            all_fetched.insert(all_fetched.end(), slice.begin(), slice.end());
            page = next;
        } while (next.has_value());
    }

    if (!all_fetched.empty()) {
        dao_.replace_all(all_fetched);
    }
}

std::optional<MemosMemo> MemoRepository::create_memo(
    std::string const& content, MemoVisibility visibility, std::vector<PendingAttachment> const& attachments)
{
    OperationLock operation_lock;
    std::vector<std::string> names;
    for (auto const& pa : attachments) {
        auto att =
            client_.create_attachment(pa.file_name, pa.mime_type, pa.bytes);
        if (att.has_value()) names.push_back(att->name);
    }
    auto memo = client_.create_memo(content, visibility, names);
    if (memo.has_value()) dao_.upsert(*memo);
    return memo;
}

void MemoRepository::delete_memo(std::string const& memo_name, bool force)
{
    OperationLock operation_lock;
    if (!client_.delete_memo(memo_name, force))
        return;
    dao_.delete_by_name(memo_name);
}

std::optional<MemosMemo> MemoRepository::archive_memo(std::string const& memo_name)
{
    OperationLock operation_lock;
    auto updated = client_.update_memo(memo_name, std::nullopt, MemoState::Archived, std::nullopt);
    if (!updated.has_value()) return std::nullopt;
    dao_.upsert(*updated);
    return updated;
}

std::optional<MemosMemo> MemoRepository::restore_memo(std::string const& memo_name)
{
    OperationLock operation_lock;
    auto updated = client_.update_memo(memo_name, std::nullopt, MemoState::Normal, std::nullopt);
    if (!updated.has_value()) return std::nullopt;
    dao_.upsert(*updated);
    return updated;
}

std::optional<MemosMemo> MemoRepository::update_memo_content(
    std::string const& memo_name, std::string const& content, MemoVisibility visibility)
{
    OperationLock operation_lock;
    auto updated = client_.update_memo(memo_name, content, std::nullopt, visibility);
    if (!updated.has_value()) return std::nullopt;
    dao_.upsert(*updated);
    return updated;
}

void MemoRepository::add_memo_comment(
    std::string const& memo_name, std::string const& body, MemoVisibility visibility)
{
    OperationLock operation_lock;
    (void)client_.create_memo_comment(memo_name, body, visibility);
}

std::optional<std::string> MemoRepository::create_share(std::string const& memo_id)
{
    OperationLock operation_lock;
    return client_.create_memo_share(memo_id);
}

// ─── Stats ─────────────────────────────────────────────────────────────────

StatsRepository::StatsRepository(DailyStatsDao& dao)
    : dao_(dao)
{
}

std::vector<DailyStats> StatsRepository::range_starting_days_ago(int start_days_inclusive)
{
    std::string start = local_date_minus_days(start_days_inclusive);
    return dao_.get_range(start);
}

void StatsRepository::increment_pomodoro()
{
    dao_.increment(local_date_iso(), StatField::PomodoroSessions);
}

} // namespace cd
