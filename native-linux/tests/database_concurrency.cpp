#include "data/db/database.h"
#include "data/db/task_dao.h"
#include "data/repository/operation_lock.h"

#include <glib.h>
#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

void require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

cd::CalDavTask task(std::string uid, std::string summary)
{
    cd::CalDavTask value;
    value.uid = std::move(uid);
    value.summary = std::move(summary);
    value.created = 1;
    value.last_modified = 1;
    return value;
}

} // namespace

int main()
{
    GError* error = nullptr;
    gchar* raw_dir = g_dir_make_tmp("crossdashboard-concurrency-XXXXXX", &error);
    if (!raw_dir) {
        std::string message = error ? error->message : "cannot create test directory";
        if (error) g_error_free(error);
        throw std::runtime_error(message);
    }
    std::filesystem::path const directory{raw_dir};
    g_free(raw_dir);
    g_setenv("XDG_RUNTIME_DIR", directory.c_str(), TRUE);

    std::filesystem::path const db_path = directory / "test.db";
    cd::Database first(db_path.string());
    cd::Database second(db_path.string());
    cd::TaskDao first_tasks(first);
    cd::TaskDao second_tasks(second);
    first_tasks.upsert(task("initial", "Initial"));

    // A second process/connection waits for the first writer instead of failing with SQLITE_BUSY.
    require(sqlite3_exec(first.raw(), "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) == SQLITE_OK,
        "cannot begin writer transaction");
    std::atomic_bool writer_finished{false};
    std::thread writer([&] {
        second_tasks.upsert(task("concurrent", "Concurrent"));
        writer_finished = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(!writer_finished.load(), "second writer did not wait for the database lock");
    require(sqlite3_exec(first.raw(), "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK,
        "cannot commit writer transaction");
    writer.join();
    require(writer_finished.load(), "second writer did not complete after lock release");

    // Cache replacement is one transaction: callers never observe the intermediate empty table.
    first_tasks.replace_all({task("replacement", "Replacement")});
    require(!second_tasks.get_by_uid("initial").has_value(), "replace_all retained stale task");
    require(second_tasks.get_by_uid("replacement").has_value(), "replace_all lost replacement task");

    // Remote/cache operations serialize across independently opened process-lock file descriptors.
    std::atomic_bool operation_acquired{false};
    std::thread contender;
    {
        cd::OperationLock held;
        contender = std::thread([&] {
            cd::OperationLock second_lock;
            operation_acquired = true;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        require(!operation_acquired.load(), "operation lock allowed overlapping mutation");
    }
    contender.join();
    require(operation_acquired.load(), "operation lock did not release");

    std::filesystem::remove_all(directory);
    return 0;
}
