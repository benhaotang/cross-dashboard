#include "database.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <glib.h>
#include <sqlite3.h>

namespace cd {

namespace {

std::string resolve_path(std::string explicit_path)
{
    if (!explicit_path.empty()) return explicit_path;
    const gchar* xdg = g_getenv("XDG_DATA_HOME");
    std::filesystem::path dir;
    if (xdg != nullptr && xdg[0] != '\0') {
        dir = std::filesystem::path{xdg} / "crossdashboard";
    }
    else {
        const gchar* home = g_get_home_dir();
        dir = std::filesystem::path(home ? home : ".") / ".local" / "share" / "crossdashboard";
    }
    std::filesystem::create_directories(dir);
    return (dir / "app.db").string();
}

void exec_sql(sqlite3* db, char const* sql)
{
    char* err{};
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string message = err ? err : "sqlite3_exec failed";
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
}

void set_user_version(sqlite3* db, int v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "PRAGMA user_version=%d;", v);
    exec_sql(db, buf);
}

int get_user_version(sqlite3* db)
{
    int ver{};
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    if (sqlite3_step(st) == SQLITE_ROW) {
        ver = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return ver;
}

} // namespace

void Database::DbCloser::operator()(sqlite3* p) const
{
    if (p != nullptr) sqlite3_close(p);
}

Database::Database(std::string path)
{
    auto const full = resolve_path(std::move(path));
    sqlite3* raw{};
    if (sqlite3_open(full.c_str(), &raw) != SQLITE_OK) {
        std::string message = raw ? sqlite3_errmsg(raw) : "sqlite3_open failed";
        sqlite3_close(raw);
        throw std::runtime_error(message);
    }
    db_.reset(raw);
    if (sqlite3_busy_timeout(db_.get(), 5000) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_.get()));
    exec_sql(db_.get(), "PRAGMA journal_mode=WAL;");
    exec_sql(db_.get(), "PRAGMA foreign_keys=ON;");
    migrate();
}

void Database::migrate()
{
    sqlite3* db = db_.get();
    int v = get_user_version(db);
    if (v < 1) {
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS events (\n"
            "  uid TEXT PRIMARY KEY NOT NULL,\n"
            "  summary TEXT NOT NULL,\n"
            "  start_epoch INTEGER NOT NULL,\n"
            "  end_epoch INTEGER NOT NULL,\n"
            "  description TEXT,\n"
            "  location TEXT,\n"
            "  calendar_href TEXT,\n"
            "  etag TEXT,\n"
            "  href TEXT\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS tasks (\n"
            "  uid TEXT PRIMARY KEY NOT NULL,\n"
            "  summary TEXT NOT NULL,\n"
            "  description TEXT,\n"
            "  status TEXT NOT NULL,\n"
            "  priority INTEGER NOT NULL,\n"
            "  percent_complete INTEGER NOT NULL,\n"
            "  due_epoch INTEGER,\n"
            "  dtstart_epoch INTEGER,\n"
            "  completed_epoch INTEGER,\n"
            "  created_epoch INTEGER NOT NULL,\n"
            "  last_modified_epoch INTEGER NOT NULL,\n"
            "  categories_json TEXT NOT NULL,\n"
            "  location TEXT,\n"
            "  parent_uid TEXT,\n"
            "  calendar_href TEXT,\n"
            "  etag TEXT,\n"
            "  href TEXT\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS notes (\n"
            "  uid TEXT PRIMARY KEY NOT NULL,\n"
            "  summary TEXT NOT NULL,\n"
            "  body TEXT NOT NULL,\n"
            "  categories_json TEXT NOT NULL,\n"
            "  created_epoch INTEGER NOT NULL,\n"
            "  last_modified_epoch INTEGER NOT NULL,\n"
            "  calendar_href TEXT,\n"
            "  etag TEXT,\n"
            "  href TEXT\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS issues (\n"
            "  id INTEGER PRIMARY KEY NOT NULL,\n"
            "  number INTEGER NOT NULL,\n"
            "  title TEXT NOT NULL,\n"
            "  body TEXT NOT NULL,\n"
            "  state TEXT NOT NULL,\n"
            "  labels_json TEXT NOT NULL,\n"
            "  assignees_json TEXT NOT NULL,\n"
            "  created_at_epoch INTEGER NOT NULL,\n"
            "  updated_at_epoch INTEGER NOT NULL,\n"
            "  repository TEXT NOT NULL,\n"
            "  html_url TEXT NOT NULL\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS issue_comments (\n"
            "  id INTEGER PRIMARY KEY NOT NULL,\n"
            "  issue_id INTEGER NOT NULL REFERENCES issues(id) ON DELETE CASCADE,\n"
            "  body TEXT NOT NULL,\n"
            "  user TEXT NOT NULL,\n"
            "  created_at_epoch INTEGER NOT NULL\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS issue_attachments (\n"
            "  id INTEGER PRIMARY KEY NOT NULL,\n"
            "  issue_id INTEGER NOT NULL REFERENCES issues(id) ON DELETE CASCADE,\n"
            "  name TEXT NOT NULL,\n"
            "  download_url TEXT NOT NULL,\n"
            "  size INTEGER NOT NULL,\n"
            "  uuid TEXT NOT NULL\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS comment_attachments (\n"
            "  id INTEGER PRIMARY KEY NOT NULL,\n"
            "  comment_id INTEGER NOT NULL REFERENCES issue_comments(id) ON DELETE CASCADE,\n"
            "  name TEXT NOT NULL,\n"
            "  download_url TEXT NOT NULL,\n"
            "  size INTEGER NOT NULL,\n"
            "  uuid TEXT NOT NULL\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS daily_stats (\n"
            "  date TEXT PRIMARY KEY NOT NULL,\n"
            "  tasks_completed INTEGER NOT NULL DEFAULT 0,\n"
            "  pomodoro_sessions INTEGER NOT NULL DEFAULT 0,\n"
            "  issues_closed INTEGER NOT NULL DEFAULT 0\n"
            ");");
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS memos (\n"
            "  name TEXT PRIMARY KEY NOT NULL,\n"
            "  state TEXT NOT NULL,\n"
            "  content TEXT NOT NULL,\n"
            "  visibility TEXT NOT NULL,\n"
            "  tags_json TEXT NOT NULL,\n"
            "  pinned INTEGER NOT NULL,\n"
            "  attachments_json TEXT NOT NULL,\n"
            "  property_has_link INTEGER NOT NULL,\n"
            "  property_has_task_list INTEGER NOT NULL,\n"
            "  property_has_incomplete_tasks INTEGER NOT NULL,\n"
            "  property_title TEXT NOT NULL,\n"
            "  snippet TEXT NOT NULL,\n"
            "  create_time_epoch INTEGER NOT NULL,\n"
            "  display_time_epoch INTEGER NOT NULL,\n"
            "  update_time_epoch INTEGER NOT NULL\n"
            ");");
        exec_sql(db, "CREATE INDEX IF NOT EXISTS idx_tasks_parent ON tasks(parent_uid);");
        exec_sql(db, "CREATE INDEX IF NOT EXISTS idx_events_cal ON events(calendar_href);");
        exec_sql(db, "CREATE INDEX IF NOT EXISTS idx_comments_issue ON issue_comments(issue_id);");
        set_user_version(db, 1);
    }
    if (v < 2) {
        exec_sql(db, "ALTER TABLE issues ADD COLUMN milestone_id INTEGER;");
        exec_sql(db, "ALTER TABLE issues ADD COLUMN milestone_title TEXT;");
        exec_sql(db, "ALTER TABLE issues ADD COLUMN milestone_due_on_epoch INTEGER;");
        set_user_version(db, 2);
    }
}

} // namespace cd
