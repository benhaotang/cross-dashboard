#include "database.h"
#include "stats_dao.h"

#include <sqlite3.h>
#include <stdexcept>

namespace cd {

DailyStatsDao::DailyStatsDao(Database& db)
    : db_(db)
{
}

static DailyStats row_to_daily(sqlite3_stmt* st)
{
    DailyStats d;
    auto const t0 = reinterpret_cast<char const*>(sqlite3_column_text(st, 0));
    d.date_iso = std::string{t0 ? t0 : ""};
    d.tasks_completed = sqlite3_column_int(st, 1);
    d.pomodoro_sessions = sqlite3_column_int(st, 2);
    d.issues_closed = sqlite3_column_int(st, 3);
    return d;
}

std::optional<DailyStats> DailyStatsDao::get_for_date(std::string const& yyyy_mm_dd)
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(), "SELECT date, tasks_completed, pomodoro_sessions, "
                                     "issues_closed FROM daily_stats WHERE date = ?",
            -1,
            &st,
            nullptr)
        != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    sqlite3_bind_text(st, 1, yyyy_mm_dd.c_str(), static_cast<int>(yyyy_mm_dd.size()), SQLITE_TRANSIENT);
    std::optional<DailyStats> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = row_to_daily(st);
    sqlite3_finalize(st);
    return out;
}

std::vector<DailyStats> DailyStatsDao::get_range(std::string const& start_iso) const
{
    sqlite3_stmt* st{};
    if (sqlite3_prepare_v2(db_.raw(),
            "SELECT date, tasks_completed, pomodoro_sessions, issues_closed FROM daily_stats WHERE "
            "date >= ? ORDER BY date ASC",
            -1,
            &st,
            nullptr)
        != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    sqlite3_bind_text(st, 1, start_iso.c_str(), static_cast<int>(start_iso.size()), SQLITE_TRANSIENT);
    std::vector<DailyStats> out;
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(row_to_daily(st));
    sqlite3_finalize(st);
    return out;
}

void DailyStatsDao::upsert(DailyStats const& row)
{
    sqlite3_stmt* st{};
    char const* sql =
        "INSERT OR REPLACE INTO daily_stats (date, tasks_completed, pomodoro_sessions, issues_closed) "
        "VALUES (?,?,?,?)";
    if (sqlite3_prepare_v2(db_.raw(), sql, -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    sqlite3_bind_text(st, 1, row.date_iso.c_str(), static_cast<int>(row.date_iso.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, row.tasks_completed);
    sqlite3_bind_int(st, 3, row.pomodoro_sessions);
    sqlite3_bind_int(st, 4, row.issues_closed);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        throw std::runtime_error(sqlite3_errmsg(db_.raw()));
    }
    sqlite3_finalize(st);
}

void DailyStatsDao::increment(std::string const& date_iso, StatField field)
{
    DailyStats row = get_for_date(date_iso).value_or(DailyStats{.date_iso = date_iso});
    switch (field) {
    case StatField::TasksCompleted: row.tasks_completed += 1; break;
    case StatField::PomodoroSessions: row.pomodoro_sessions += 1; break;
    case StatField::IssuesClosed: row.issues_closed += 1; break;
    }
    upsert(row);
}

} // namespace cd
