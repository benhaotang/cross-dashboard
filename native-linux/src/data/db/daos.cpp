#include "database.h"
#include "event_dao.h"
#include "issue_dao.h"
#include "memo_dao.h"
#include "note_dao.h"
#include "task_dao.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <stdexcept>

namespace cd {

namespace {

void exec_sql_cd(sqlite3* db, char const* sql)
{
    char* err{};
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string message = err ? err : "sqlite3_exec failed";
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
}

nlohmann::json categories_to_json(std::vector<std::string> const& c) { return nlohmann::json(c); }

std::vector<std::string> categories_from_json(std::string const& blob)
{
    if (blob.empty()) return {};
    auto j = nlohmann::json::parse(blob, nullptr, false);
    if (j.is_discarded() || !j.is_array()) return {};
    try {
        return j.get<std::vector<std::string>>();
    }
    catch (nlohmann::json::exception const&) {
        return {};
    }
}

std::string attachments_to_json(std::vector<MemosAttachment> const& attachments)
{
    nlohmann::json ja = nlohmann::json::array();
    for (auto const& a : attachments) {
        ja.push_back({{"name", a.name},
            {"filename", a.filename},
            {"externalLink", a.external_link},
            {"type", a.type},
            {"size", a.size},
            {"memo", a.memo}});
    }
    return ja.dump();
}

std::vector<MemosAttachment> attachments_from_json(std::string const& blob)
{
    if (blob.empty()) return {};
    auto outer = nlohmann::json::parse(blob, nullptr, false);
    if (outer.is_discarded() || !outer.is_array()) return {};
    std::vector<MemosAttachment> out;
    out.reserve(outer.size());
    for (auto const& o : outer) {
        MemosAttachment a;
        if (auto p = o.find("name"); p != o.end() && p->is_string()) a.name = *p;
        if (auto p = o.find("filename"); p != o.end() && p->is_string()) a.filename = *p;
        if (auto p = o.find("externalLink"); p != o.end() && p->is_string()) a.external_link = *p;
        if (auto p = o.find("type"); p != o.end() && p->is_string()) a.type = *p;
        if (auto p = o.find("size"); p != o.end() && p->is_number_integer()) a.size = *p;
        if (auto p = o.find("memo"); p != o.end() && p->is_string()) a.memo = *p;
        out.push_back(std::move(a));
    }
    return out;
}

EpochMillis epoch_col(sqlite3_stmt* st, int i)
{
    return sqlite3_column_type(st, i) == SQLITE_NULL ? EpochMillis{}
        : sqlite3_column_int64(st, i);
}

std::optional<EpochMillis> epoch_opt(sqlite3_stmt* st, int i)
{
    if (sqlite3_column_type(st, i) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_int64(st, i);
}

std::optional<std::string> text_opt(sqlite3_stmt* st, int i)
{
    if (sqlite3_column_type(st, i) == SQLITE_NULL) return std::nullopt;
    auto const* t = reinterpret_cast<char const*>(sqlite3_column_text(st, i));
    return std::string{t ? t : ""};
}

std::string text_req(sqlite3_stmt* st, int i)
{
    auto const* t = reinterpret_cast<char const*>(sqlite3_column_text(st, i));
    return std::string{t ? t : ""};
}

CalendarEvent row_to_event(sqlite3_stmt* st)
{
    CalendarEvent e;
    e.uid = text_req(st, 0);
    e.summary = text_req(st, 1);
    e.start = epoch_col(st, 2);
    e.end = epoch_col(st, 3);
    e.description = text_opt(st, 4);
    e.location = text_opt(st, 5);
    e.calendar_href = text_opt(st, 6);
    e.etag = text_opt(st, 7);
    e.href = text_opt(st, 8);
    return e;
}

CalDavTask row_to_task(sqlite3_stmt* st)
{
    CalDavTask t;
    t.uid = text_req(st, 0);
    t.summary = text_req(st, 1);
    t.description = text_opt(st, 2);
    t.status = task_status_from_ical(text_req(st, 3));
    t.priority = sqlite3_column_int(st, 4);
    t.percent_complete = sqlite3_column_int(st, 5);
    t.due = epoch_opt(st, 6);
    t.dtstart = epoch_opt(st, 7);
    t.completed = epoch_opt(st, 8);
    t.created = epoch_col(st, 9);
    t.last_modified = epoch_col(st, 10);
    t.categories = categories_from_json(text_req(st, 11));
    t.location = text_opt(st, 12);
    t.parent_uid = text_opt(st, 13);
    t.calendar_href = text_opt(st, 14);
    t.etag = text_opt(st, 15);
    t.href = text_opt(st, 16);
    return t;
}

Note row_to_note(sqlite3_stmt* st)
{
    Note n;
    n.uid = text_req(st, 0);
    n.summary = text_req(st, 1);
    n.body = text_req(st, 2);
    n.categories = categories_from_json(text_req(st, 3));
    n.created = epoch_col(st, 4);
    n.last_modified = epoch_col(st, 5);
    n.calendar_href = text_opt(st, 6);
    n.etag = text_opt(st, 7);
    n.href = text_opt(st, 8);
    return n;
}

GiteaIssue row_to_issue(sqlite3_stmt* st)
{
    GiteaIssue i;
    i.id = sqlite3_column_int64(st, 0);
    i.number = sqlite3_column_int(st, 1);
    i.title = text_req(st, 2);
    i.body = text_req(st, 3);
    i.state = text_req(st, 4);
    i.labels = categories_from_json(text_req(st, 5));
    i.assignees = categories_from_json(text_req(st, 6));
    i.created_at = epoch_col(st, 7);
    i.updated_at = epoch_col(st, 8);
    i.repository = text_req(st, 9);
    i.html_url = text_req(st, 10);
    if (sqlite3_column_type(st, 11) != SQLITE_NULL)
        i.milestone_id = sqlite3_column_int64(st, 11);
    i.milestone_title = text_opt(st, 12);
    i.milestone_due_on = epoch_opt(st, 13);
    return i;
}

MemosMemo row_to_memo(sqlite3_stmt* st)
{
    MemosMemo m;
    m.name = text_req(st, 0);
    m.state = memo_state_from_name(text_req(st, 1));
    m.content = text_req(st, 2);
    m.visibility = memo_visibility_from_name(text_req(st, 3));
    m.tags = categories_from_json(text_req(st, 4));
    m.pinned = sqlite3_column_int(st, 5) != 0;
    m.attachments = attachments_from_json(text_req(st, 6));
    MemoProperty mp;
    mp.has_link = sqlite3_column_int(st, 7) != 0;
    mp.has_task_list = sqlite3_column_int(st, 8) != 0;
    mp.has_incomplete_tasks = sqlite3_column_int(st, 9) != 0;
    mp.title = text_req(st, 10);
    m.property = mp;
    m.snippet = text_req(st, 11);
    m.create_time = epoch_col(st, 12);
    m.display_time = epoch_col(st, 13);
    m.update_time = epoch_col(st, 14);
    return m;
}

void bind_opt_text(sqlite3_stmt* st, int idx, std::optional<std::string> const& v)
{
    if (!v.has_value()) sqlite3_bind_null(st, idx);
    else sqlite3_bind_text(st, idx, v->c_str(), static_cast<int>(v->size()), SQLITE_TRANSIENT);
}

void bind_opt_epoch(sqlite3_stmt* st, int idx, std::optional<EpochMillis> const& v)
{
    if (!v.has_value()) sqlite3_bind_null(st, idx);
    else sqlite3_bind_int64(st, idx, *v);
}

template <typename F>
auto with_stmt(Database& db, char const* sql, F&& f)
{
    sqlite3_stmt* stmt{};
    if (sqlite3_prepare_v2(db.raw(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db.raw()));
    }
    try {
        auto r = f(stmt);
        sqlite3_finalize(stmt);
        return r;
    }
    catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }
}

} // namespace

// ─── EventDao ────────────────────────────────────────────────────────────────

EventDao::EventDao(Database& db)
    : db_(db)
{
}

std::vector<CalendarEvent> EventDao::get_all() const
{
    return with_stmt(db_, "SELECT uid, summary, start_epoch, end_epoch, description, "
                        "location, calendar_href, etag, href FROM events ORDER BY start_epoch ASC",
        [](sqlite3_stmt* st) {
            std::vector<CalendarEvent> out;
            while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_event(st));
            return out;
        });
}

std::vector<CalendarEvent> EventDao::get_by_calendar(std::string const& calendar_href) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT uid, summary, start_epoch, end_epoch, description, location, calendar_href, "
            "etag, href FROM events WHERE calendar_href = ? ORDER BY start_epoch ASC",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(
        st, 1, calendar_href.c_str(), static_cast<int>(calendar_href.size()), SQLITE_TRANSIENT);
    std::vector<CalendarEvent> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_event(st));
    sqlite3_finalize(st);
    return out;
}

std::vector<CalendarEvent> EventDao::get_upcoming(EpochMillis now_epoch, int limit) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT uid, summary, start_epoch, end_epoch, description, location, calendar_href, "
            "etag, href FROM events WHERE start_epoch >= ? ORDER BY start_epoch ASC LIMIT ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_int64(st, 1, now_epoch);
    sqlite3_bind_int(st, 2, limit);
    std::vector<CalendarEvent> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_event(st));
    sqlite3_finalize(st);
    return out;
}

void EventDao::upsert(CalendarEvent const& row)
{
    upsert_all({row});
}

void EventDao::upsert_all(std::vector<CalendarEvent> const& rows)
{
    if (rows.empty()) return;
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO events (uid, summary, start_epoch, end_epoch, description, "
        "location, calendar_href, etag, href) VALUES (?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    for (auto const& row : rows) {
        sqlite3_bind_text(st, 1, row.uid.c_str(), static_cast<int>(row.uid.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, row.summary.c_str(), static_cast<int>(row.summary.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 3, row.start);
        sqlite3_bind_int64(st, 4, row.end);
        bind_opt_text(st, 5, row.description);
        bind_opt_text(st, 6, row.location);
        bind_opt_text(st, 7, row.calendar_href);
        bind_opt_text(st, 8, row.etag);
        bind_opt_text(st, 9, row.href);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            throw std::runtime_error(sqlite3_errmsg(db_.raw()));
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
}

void EventDao::replace_all(std::vector<CalendarEvent> const& rows)
{
    exec_sql_cd(db_.raw(), "BEGIN IMMEDIATE;");
    try {
        delete_all();
        upsert_all(rows);
        exec_sql_cd(db_.raw(), "COMMIT;");
    }
    catch (...) {
        try { exec_sql_cd(db_.raw(), "ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void EventDao::delete_all()
{
    char* err{};
    if (sqlite3_exec(db_.raw(), "DELETE FROM events", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
    sqlite3_free(err);
}

// ─── TaskDao ─────────────────────────────────────────────────────────────────

TaskDao::TaskDao(Database& db)
    : db_(db)
{
}

std::vector<CalDavTask> TaskDao::get_all() const
{
    return with_stmt(db_,
        "SELECT uid, summary, description, status, priority, percent_complete, due_epoch, "
        "dtstart_epoch, completed_epoch, created_epoch, last_modified_epoch, categories_json, "
        "location, parent_uid, calendar_href, etag, href FROM tasks ORDER BY due_epoch ASC NULLS "
        "LAST, created_epoch DESC",
        [](sqlite3_stmt* st) {
            std::vector<CalDavTask> out;
            while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_task(st));
            return out;
        });
}

std::optional<CalDavTask> TaskDao::get_by_uid(std::string const& uid) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT uid, summary, description, status, priority, percent_complete, due_epoch, "
            "dtstart_epoch, completed_epoch, created_epoch, last_modified_epoch, categories_json, "
            "location, parent_uid, calendar_href, etag, href FROM tasks WHERE uid = ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, uid.c_str(), static_cast<int>(uid.size()), SQLITE_TRANSIENT);
    std::optional<CalDavTask> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = row_to_task(st);
    sqlite3_finalize(st);
    return out;
}

std::vector<CalDavTask> TaskDao::get_by_parent_uid(std::optional<std::string> const& parent_uid) const
{
    sqlite3_stmt* st{};
    char const* sql = parent_uid.has_value()
        ? "SELECT uid, summary, description, status, priority, percent_complete, due_epoch, "
          "dtstart_epoch, completed_epoch, created_epoch, last_modified_epoch, categories_json, "
          "location, parent_uid, calendar_href, etag, href FROM tasks WHERE parent_uid = ? ORDER BY "
          "due_epoch ASC NULLS LAST, created_epoch DESC"
        : "SELECT uid, summary, description, status, priority, percent_complete, due_epoch, "
          "dtstart_epoch, completed_epoch, created_epoch, last_modified_epoch, categories_json, "
          "location, parent_uid, calendar_href, etag, href FROM tasks WHERE parent_uid IS NULL "
          "ORDER BY due_epoch ASC NULLS LAST, created_epoch DESC";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    if (parent_uid.has_value()) {
        sqlite3_bind_text(
            st, 1, parent_uid->c_str(), static_cast<int>(parent_uid->size()), SQLITE_TRANSIENT);
    }
    std::vector<CalDavTask> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_task(st));
    sqlite3_finalize(st);
    return out;
}

std::vector<CalDavTask> TaskDao::get_due_soon(EpochMillis deadline_epoch, int limit) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT uid, summary, description, status, priority, percent_complete, due_epoch, "
            "dtstart_epoch, completed_epoch, created_epoch, last_modified_epoch, categories_json, "
            "location, parent_uid, calendar_href, etag, href FROM tasks WHERE "
            "due_epoch IS NOT NULL AND due_epoch <= ? AND status NOT IN ('COMPLETED','CANCELLED') "
            "ORDER BY due_epoch ASC LIMIT ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_int64(st, 1, deadline_epoch);
    sqlite3_bind_int(st, 2, limit);
    std::vector<CalDavTask> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_task(st));
    sqlite3_finalize(st);
    return out;
}

void TaskDao::upsert(CalDavTask const& row)
{
    upsert_all({row});
}

void TaskDao::upsert_all(std::vector<CalDavTask> const& rows)
{
    if (rows.empty()) return;
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO tasks (uid, summary, description, status, priority, "
        "percent_complete, due_epoch, dtstart_epoch, completed_epoch, created_epoch, "
        "last_modified_epoch, categories_json, location, parent_uid, calendar_href, etag, href) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    std::string cat_json;
    for (auto const& row : rows) {
        cat_json = categories_to_json(row.categories).dump();
        sqlite3_bind_text(st, 1, row.uid.c_str(), static_cast<int>(row.uid.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(
            st, 2, row.summary.c_str(), static_cast<int>(row.summary.size()), SQLITE_TRANSIENT);
        bind_opt_text(st, 3, row.description);
        std::string const stv = task_status_to_ical(row.status);
        sqlite3_bind_text(st, 4, stv.c_str(), static_cast<int>(stv.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 5, row.priority);
        sqlite3_bind_int(st, 6, row.percent_complete);
        bind_opt_epoch(st, 7, row.due);
        bind_opt_epoch(st, 8, row.dtstart);
        bind_opt_epoch(st, 9, row.completed);
        sqlite3_bind_int64(st, 10, row.created);
        sqlite3_bind_int64(st, 11, row.last_modified);
        sqlite3_bind_text(st, 12, cat_json.c_str(), static_cast<int>(cat_json.size()), SQLITE_TRANSIENT);
        bind_opt_text(st, 13, row.location);
        bind_opt_text(st, 14, row.parent_uid);
        bind_opt_text(st, 15, row.calendar_href);
        bind_opt_text(st, 16, row.etag);
        bind_opt_text(st, 17, row.href);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            throw std::runtime_error(sqlite3_errmsg(db_.raw()));
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
}

void TaskDao::replace_all(std::vector<CalDavTask> const& rows)
{
    exec_sql_cd(db_.raw(), "BEGIN IMMEDIATE;");
    try {
        delete_all();
        upsert_all(rows);
        exec_sql_cd(db_.raw(), "COMMIT;");
    }
    catch (...) {
        try { exec_sql_cd(db_.raw(), "ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void TaskDao::delete_all()
{
    char* err{};
    if (sqlite3_exec(db_.raw(), "DELETE FROM tasks", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
    sqlite3_free(err);
}

void TaskDao::delete_by_uid(std::string const& uid)
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(), "DELETE FROM tasks WHERE uid = ?", -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, uid.c_str(), static_cast<int>(uid.size()), SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

// ─── NoteDao ─────────────────────────────────────────────────────────────────

NoteDao::NoteDao(Database& db)
    : db_(db)
{
}

std::vector<Note> NoteDao::get_all() const
{
    return with_stmt(db_,
        "SELECT uid, summary, body, categories_json, created_epoch, last_modified_epoch, "
        "calendar_href, etag, href FROM notes ORDER BY last_modified_epoch DESC",
        [](sqlite3_stmt* st) {
            std::vector<Note> out;
            while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_note(st));
            return out;
        });
}

std::optional<Note> NoteDao::get_by_uid(std::string const& uid) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT uid, summary, body, categories_json, created_epoch, last_modified_epoch, "
            "calendar_href, etag, href FROM notes WHERE uid = ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, uid.c_str(), static_cast<int>(uid.size()), SQLITE_TRANSIENT);
    std::optional<Note> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = row_to_note(st);
    sqlite3_finalize(st);
    return out;
}

void NoteDao::upsert(Note const& row)
{
    upsert_all({row});
}

void NoteDao::upsert_all(std::vector<Note> const& rows)
{
    if (rows.empty()) return;
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO notes (uid, summary, body, categories_json, created_epoch, "
        "last_modified_epoch, calendar_href, etag, href) VALUES (?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    std::string cat_json;
    for (auto const& row : rows) {
        cat_json = categories_to_json(row.categories).dump();
        sqlite3_bind_text(st, 1, row.uid.c_str(), static_cast<int>(row.uid.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(
            st, 2, row.summary.c_str(), static_cast<int>(row.summary.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, row.body.c_str(), static_cast<int>(row.body.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, cat_json.c_str(), static_cast<int>(cat_json.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 5, row.created);
        sqlite3_bind_int64(st, 6, row.last_modified);
        bind_opt_text(st, 7, row.calendar_href);
        bind_opt_text(st, 8, row.etag);
        bind_opt_text(st, 9, row.href);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            throw std::runtime_error(sqlite3_errmsg(db_.raw()));
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
}

void NoteDao::replace_all(std::vector<Note> const& rows)
{
    exec_sql_cd(db_.raw(), "BEGIN IMMEDIATE;");
    try {
        delete_all();
        upsert_all(rows);
        exec_sql_cd(db_.raw(), "COMMIT;");
    }
    catch (...) {
        try { exec_sql_cd(db_.raw(), "ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void NoteDao::delete_all()
{
    char* err{};
    if (sqlite3_exec(db_.raw(), "DELETE FROM notes", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
    sqlite3_free(err);
}

void NoteDao::delete_by_uid(std::string const& uid)
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(), "DELETE FROM notes WHERE uid = ?", -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, uid.c_str(), static_cast<int>(uid.size()), SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

// ─── IssueDao ────────────────────────────────────────────────────────────────

IssueDao::IssueDao(Database& db)
    : db_(db)
{
}

std::vector<GiteaIssue> IssueDao::get_all() const
{
    return with_stmt(db_,
        "SELECT id, number, title, body, state, labels_json, assignees_json, created_at_epoch, "
        "updated_at_epoch, repository, html_url, milestone_id, milestone_title, milestone_due_on_epoch "
        "FROM issues ORDER BY updated_at_epoch DESC",
        [](sqlite3_stmt* st) {
            std::vector<GiteaIssue> out;
            while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_issue(st));
            return out;
        });
}

std::optional<GiteaIssue> IssueDao::get_by_id(std::int64_t id) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT id, number, title, body, state, labels_json, assignees_json, created_at_epoch, "
            "updated_at_epoch, repository, html_url, milestone_id, milestone_title, milestone_due_on_epoch "
            "FROM issues WHERE id = ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_int64(st, 1, id);
    std::optional<GiteaIssue> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = row_to_issue(st);
    sqlite3_finalize(st);
    return out;
}

std::vector<GiteaIssue> IssueDao::get_by_state(std::string const& state) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT id, number, title, body, state, labels_json, assignees_json, created_at_epoch, "
            "updated_at_epoch, repository, html_url, milestone_id, milestone_title, milestone_due_on_epoch "
            "FROM issues WHERE state = ? ORDER BY "
            "updated_at_epoch DESC",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, state.c_str(), static_cast<int>(state.size()), SQLITE_TRANSIENT);
    std::vector<GiteaIssue> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_issue(st));
    sqlite3_finalize(st);
    return out;
}

void IssueDao::upsert(GiteaIssue const& row)
{
    upsert_all({row});
}

void IssueDao::upsert_all(std::vector<GiteaIssue> const& rows)
{
    if (rows.empty()) return;
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO issues (id, number, title, body, state, labels_json, "
        "assignees_json, created_at_epoch, updated_at_epoch, repository, html_url, milestone_id, "
        "milestone_title, milestone_due_on_epoch) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    std::string lj, aj;
    for (auto const& row : rows) {
        lj = categories_to_json(row.labels).dump();
        aj = categories_to_json(row.assignees).dump();
        sqlite3_bind_int64(st, 1, row.id);
        sqlite3_bind_int(st, 2, row.number);
        sqlite3_bind_text(st, 3, row.title.c_str(), static_cast<int>(row.title.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, row.body.c_str(), static_cast<int>(row.body.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, row.state.c_str(), static_cast<int>(row.state.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 6, lj.c_str(), static_cast<int>(lj.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 7, aj.c_str(), static_cast<int>(aj.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 8, row.created_at);
        sqlite3_bind_int64(st, 9, row.updated_at);
        sqlite3_bind_text(
            st, 10, row.repository.c_str(), static_cast<int>(row.repository.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 11, row.html_url.c_str(), static_cast<int>(row.html_url.size()),
            SQLITE_TRANSIENT);
        if (row.milestone_id)
            sqlite3_bind_int64(st, 12, *row.milestone_id);
        else
            sqlite3_bind_null(st, 12);
        if (row.milestone_title)
            sqlite3_bind_text(st, 13, row.milestone_title->c_str(),
                static_cast<int>(row.milestone_title->size()), SQLITE_TRANSIENT);
        else
            sqlite3_bind_null(st, 13);
        if (row.milestone_due_on)
            sqlite3_bind_int64(st, 14, *row.milestone_due_on);
        else
            sqlite3_bind_null(st, 14);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            throw std::runtime_error(sqlite3_errmsg(db_.raw()));
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
}

void IssueDao::replace_all(std::vector<GiteaIssue> const& rows)
{
    exec_sql_cd(db_.raw(), "BEGIN IMMEDIATE;");
    try {
        delete_all();
        upsert_all(rows);
        exec_sql_cd(db_.raw(), "COMMIT;");
    }
    catch (...) {
        try { exec_sql_cd(db_.raw(), "ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void IssueDao::delete_all()
{
    exec_sql_cd(db_.raw(),
        "DELETE FROM comment_attachments; DELETE FROM issue_attachments; DELETE FROM "
        "issue_comments; DELETE FROM issues;");
}

// ─── MemoDao ─────────────────────────────────────────────────────────────────

MemoDao::MemoDao(Database& db)
    : db_(db)
{
}

std::vector<MemosMemo> MemoDao::get_all() const
{
    return with_stmt(db_,
        "SELECT name, state, content, visibility, tags_json, pinned, attachments_json, "
        "property_has_link, property_has_task_list, property_has_incomplete_tasks, property_title, "
        "snippet, create_time_epoch, display_time_epoch, update_time_epoch FROM memos ORDER BY "
        "display_time_epoch DESC",
        [](sqlite3_stmt* st) {
            std::vector<MemosMemo> out;
            while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_memo(st));
            return out;
        });
}

std::vector<MemosMemo> MemoDao::get_by_state(MemoState state) const
{
    std::string const stn = memo_state_name(state);
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT name, state, content, visibility, tags_json, pinned, attachments_json, "
            "property_has_link, property_has_task_list, property_has_incomplete_tasks, property_title, "
            "snippet, create_time_epoch, display_time_epoch, update_time_epoch FROM memos WHERE state = ? "
            "ORDER BY display_time_epoch DESC",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, stn.c_str(), static_cast<int>(stn.size()), SQLITE_TRANSIENT);
    std::vector<MemosMemo> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_memo(st));
    sqlite3_finalize(st);
    return out;
}

std::optional<MemosMemo> MemoDao::get_by_name(std::string const& name) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT name, state, content, visibility, tags_json, pinned, attachments_json, "
            "property_has_link, property_has_task_list, property_has_incomplete_tasks, property_title, "
            "snippet, create_time_epoch, display_time_epoch, update_time_epoch FROM memos WHERE name = ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, name.c_str(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    std::optional<MemosMemo> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = row_to_memo(st);
    sqlite3_finalize(st);
    return out;
}

void MemoDao::upsert(MemosMemo const& row)
{
    upsert_all({row});
}

void MemoDao::upsert_all(std::vector<MemosMemo> const& rows)
{
    if (rows.empty()) return;
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO memos (name, state, content, visibility, tags_json, pinned, "
        "attachments_json, property_has_link, property_has_task_list, property_has_incomplete_tasks, "
        "property_title, snippet, create_time_epoch, display_time_epoch, update_time_epoch) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    std::string tags_j, att_j;
    for (auto const& row : rows) {
        tags_j = categories_to_json(row.tags).dump();
        att_j = attachments_to_json(row.attachments);
        sqlite3_bind_text(st, 1, row.name.c_str(), static_cast<int>(row.name.size()), SQLITE_TRANSIENT);
        std::string const sn = memo_state_name(row.state);
        sqlite3_bind_text(st, 2, sn.c_str(), static_cast<int>(sn.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, row.content.c_str(), static_cast<int>(row.content.size()), SQLITE_TRANSIENT);
        std::string const vn = memo_visibility_name(row.visibility);
        sqlite3_bind_text(st, 4, vn.c_str(), static_cast<int>(vn.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, tags_j.c_str(), static_cast<int>(tags_j.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 6, row.pinned ? 1 : 0);
        sqlite3_bind_text(st, 7, att_j.c_str(), static_cast<int>(att_j.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 8, row.property.has_link ? 1 : 0);
        sqlite3_bind_int(st, 9, row.property.has_task_list ? 1 : 0);
        sqlite3_bind_int(st, 10, row.property.has_incomplete_tasks ? 1 : 0);
        sqlite3_bind_text(st, 11, row.property.title.c_str(), static_cast<int>(row.property.title.size()),
            SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 12, row.snippet.c_str(), static_cast<int>(row.snippet.size()),
            SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 13, row.create_time);
        sqlite3_bind_int64(st, 14, row.display_time);
        sqlite3_bind_int64(st, 15, row.update_time);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            throw std::runtime_error(sqlite3_errmsg(db_.raw()));
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
}

void MemoDao::replace_all(std::vector<MemosMemo> const& rows)
{
    exec_sql_cd(db_.raw(), "BEGIN IMMEDIATE;");
    try {
        delete_all();
        upsert_all(rows);
        exec_sql_cd(db_.raw(), "COMMIT;");
    }
    catch (...) {
        try { exec_sql_cd(db_.raw(), "ROLLBACK;"); } catch (...) {}
        throw;
    }
}

void MemoDao::delete_all()
{
    char* err{};
    if (sqlite3_exec(db_.raw(), "DELETE FROM memos", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
    sqlite3_free(err);
}

void MemoDao::delete_by_name(std::string const& name)
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(), "DELETE FROM memos WHERE name = ?", -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_bind_text(st, 1, name.c_str(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace cd
