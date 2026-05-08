#pragma once

#include <memory>
#include <string>

struct sqlite3;

namespace cd {

/** Opens `app.db` under XDG data home, WAL mode, incremental `PRAGMA user_version` migrations. */
class Database final {
public:
    /** If `path` is empty, uses `$XDG_DATA_HOME/crossdashboard/app.db` or `~/.local/share/crossdashboard/app.db`. */
    explicit Database(std::string path = {});

    sqlite3* raw() const { return db_.get(); }

    Database(Database const&) = delete;
    Database& operator=(Database const&) = delete;

    Database(Database&&) noexcept = default;
    Database& operator=(Database&&) noexcept = default;

private:
    struct DbCloser {
        void operator()(sqlite3* p) const;
    };
    std::unique_ptr<sqlite3, DbCloser> db_;
    void migrate();
};

} // namespace cd
