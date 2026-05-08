#pragma once

#include "../../domain/models.h"

#include <optional>
#include <string>
#include <vector>

namespace cd {

class EventDao;
class TaskDao;
class NoteDao;
class IssueDao;
class MemoDao;
class DailyStatsDao;
class SecretStore;
class CalDavClient;
class GiteaClient;
class MemosClient;

/** Mirrors Android `EventRepository`. */
class EventRepository {
public:
    EventRepository(EventDao& dao, CalDavClient& client, SecretStore& secrets);

    void sync_many(std::vector<std::string> const& calendar_hrefs);
    std::vector<CalendarEvent> get_upcoming(int limit = 5);
    CalendarEvent create(CalendarEvent const& event, std::string const& calendar_href);
    void remove(CalendarEvent const& event);

    [[nodiscard]] std::vector<std::string> selected_calendar_hrefs() const;

private:
    EventDao& dao_;
    CalDavClient& client_;
    SecretStore& secrets_;
};

class TaskRepository {
public:
    TaskRepository(TaskDao& dao, DailyStatsDao& stats, CalDavClient& client);

    void sync_many(std::vector<std::string> const& calendar_hrefs);
    CalDavTask create(CalDavTask const& task, std::string const& calendar_href);
    void update(CalDavTask const& task);
    void remove(CalDavTask const& task);

    /** Toggle completed state; persists via CalDAV and increments stats when marking complete. */
    CalDavTask toggle_complete(CalDavTask const& task);

private:
    TaskDao& dao_;
    DailyStatsDao& stats_;
    CalDavClient& client_;
};

class NoteRepository {
public:
    NoteRepository(NoteDao& dao, CalDavClient& client);

    void sync_many(std::vector<std::string> const& calendar_hrefs);
    Note create(Note const& note, std::string const& calendar_href);
    void update(Note const& note);
    void remove(Note const& note);

private:
    NoteDao& dao_;
    CalDavClient& client_;
};

class IssueRepository {
public:
    IssueRepository(IssueDao& dao, DailyStatsDao& stats, GiteaClient& client);

    void sync_many(std::vector<std::string> const& repositories);

    GiteaIssue update_issue(std::string const& repo, int number,
        std::optional<std::string> title, std::optional<std::string> body, std::optional<std::string> state);

    std::vector<GiteaComment> fetch_comments(std::string const& repo, int number);

    void add_comment(std::string const& repo, int number, std::string const& body);

    void replace_labels(std::string const& repo, int number, std::vector<std::string> label_names);

    GiteaIssue create_issue(std::string const& repo, std::string const& title, std::string const& body);

private:
    IssueDao& dao_;
    DailyStatsDao& stats_;
    GiteaClient& client_;
};

class MemoRepository {
public:
    MemoRepository(MemoDao& dao, MemosClient& client);

    void sync_all();

    std::optional<MemosMemo> create_memo(
        std::string const& content, MemoVisibility visibility, std::vector<PendingAttachment> const& attachments);

    void delete_memo(std::string const& memo_name, bool force = false);

    [[nodiscard]] std::optional<MemosMemo> archive_memo(std::string const& memo_name);

    [[nodiscard]] std::optional<MemosMemo> restore_memo(std::string const& memo_name);

    [[nodiscard]] std::optional<std::string> create_share(std::string const& memo_id);

    [[nodiscard]] std::optional<MemosMemo> update_memo_content(
        std::string const& memo_name, std::string const& content, MemoVisibility visibility);

    void add_memo_comment(std::string const& memo_name, std::string const& body, MemoVisibility visibility);

private:
    MemoDao& dao_;
    MemosClient& client_;
};

class StatsRepository {
public:
    explicit StatsRepository(DailyStatsDao& dao);

    [[nodiscard]] std::vector<DailyStats> range_starting_days_ago(int start_days_inclusive);

    void increment_pomodoro();

private:
    DailyStatsDao& dao_;
};

} // namespace cd
