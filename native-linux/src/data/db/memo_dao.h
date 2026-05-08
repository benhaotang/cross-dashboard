#pragma once

#include "../../domain/models.h"

#include <optional>
#include <vector>

namespace cd {

class Database;

class MemoDao final {
public:
    explicit MemoDao(Database& db);
    [[nodiscard]] std::vector<MemosMemo> get_all() const;
    [[nodiscard]] std::vector<MemosMemo> get_by_state(MemoState state) const;
    [[nodiscard]] std::optional<MemosMemo> get_by_name(std::string const& name) const;
    void upsert(MemosMemo const& row);
    void upsert_all(std::vector<MemosMemo> const& rows);
    void delete_all();
    void delete_by_name(std::string const& name);

private:
    Database& db_;
};

} // namespace cd
