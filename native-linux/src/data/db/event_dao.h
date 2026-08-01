#pragma once

#include "../../domain/models.h"

#include <vector>

namespace cd {

class Database;

class EventDao final {
public:
    explicit EventDao(Database& db);
    [[nodiscard]] std::vector<CalendarEvent> get_all() const;
    [[nodiscard]] std::vector<CalendarEvent> get_upcoming(EpochMillis now_epoch,
        int limit) const;
    [[nodiscard]] std::vector<CalendarEvent> get_by_calendar(std::string const& calendar_href) const;
    void upsert(CalendarEvent const& row);
    void upsert_all(std::vector<CalendarEvent> const& rows);
    void replace_all(std::vector<CalendarEvent> const& rows);
    void delete_all();

private:
    Database& db_;
};

} // namespace cd
