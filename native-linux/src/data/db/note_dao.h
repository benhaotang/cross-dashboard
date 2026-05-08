#pragma once

#include "../../domain/models.h"

#include <optional>
#include <vector>

namespace cd {

class Database;

class NoteDao final {
public:
    explicit NoteDao(Database& db);
    [[nodiscard]] std::vector<Note> get_all() const;
    [[nodiscard]] std::optional<Note> get_by_uid(std::string const& uid) const;
    void upsert(Note const& row);
    void upsert_all(std::vector<Note> const& rows);
    void delete_all();
    void delete_by_uid(std::string const& uid);

private:
    Database& db_;
};

} // namespace cd
