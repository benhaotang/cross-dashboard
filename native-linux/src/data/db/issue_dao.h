#pragma once

#include "../../domain/models.h"

#include <optional>
#include <vector>

namespace cd {

class Database;

class IssueDao final {
public:
    explicit IssueDao(Database& db);
    [[nodiscard]] std::vector<GiteaIssue> get_all() const;
    [[nodiscard]] std::optional<GiteaIssue> get_by_id(std::int64_t id) const;
    [[nodiscard]] std::vector<GiteaIssue> get_by_state(std::string const& state) const;
    void upsert(GiteaIssue const& row);
    void upsert_all(std::vector<GiteaIssue> const& rows);
    void replace_all(std::vector<GiteaIssue> const& rows);
    void delete_all();

private:
    Database& db_;
};

} // namespace cd
