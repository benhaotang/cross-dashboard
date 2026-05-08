#pragma once

#include "../../domain/models.h"

#include <optional>
#include <string>
#include <vector>

namespace cd {

class Database;

enum class StatField : std::uint8_t { TasksCompleted, PomodoroSessions, IssuesClosed };

/** Persists aggregated daily counters (mirrors Android `daily_stats`). */
class DailyStatsDao final {
public:
    explicit DailyStatsDao(Database& db);

    [[nodiscard]] std::optional<DailyStats> get_for_date(std::string const& yyyy_mm_dd);
    [[nodiscard]] std::vector<DailyStats> get_range(std::string const& start_iso) const;

    void upsert(DailyStats const& row);
    void increment(std::string const& date_iso, StatField field);

private:
    Database& db_;
};

} // namespace cd
