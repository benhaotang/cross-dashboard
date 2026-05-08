#pragma once

#include "../../domain/models.h"

#include <optional>
#include <vector>

namespace cd {

class Database;

class TaskDao final {
public:
    explicit TaskDao(Database& db);
    [[nodiscard]] std::vector<CalDavTask> get_all() const;
    [[nodiscard]] std::optional<CalDavTask> get_by_uid(std::string const& uid) const;
    [[nodiscard]] std::vector<CalDavTask> get_by_parent_uid(std::optional<std::string> const& parent_uid) const;
    [[nodiscard]] std::vector<CalDavTask> get_due_soon(EpochMillis deadline_epoch, int limit) const;
    void upsert(CalDavTask const& row);
    void upsert_all(std::vector<CalDavTask> const& rows);
    void delete_all();
    void delete_by_uid(std::string const& uid);

private:
    Database& db_;
};

} // namespace cd
