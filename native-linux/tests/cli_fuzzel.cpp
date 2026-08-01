#include "data/db/database.h"
#include "data/db/task_dao.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include <glib.h>

namespace {
void require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv)
{
    require(argc == 2, "CLI path is required");
    GError* error = nullptr;
    gchar* raw_dir = g_dir_make_tmp("crossdashboard-fuzzel-XXXXXX", &error);
    require(raw_dir != nullptr, error ? error->message : "cannot create test directory");
    std::filesystem::path const directory{raw_dir};
    g_free(raw_dir);
    g_setenv("XDG_DATA_HOME", directory.c_str(), TRUE);
    g_setenv("XDG_CONFIG_HOME", directory.c_str(), TRUE);

    cd::Database database;
    cd::TaskDao tasks(database);
    cd::CalDavTask task;
    task.uid = "stable-task-uid";
    task.summary = "Deploy\thotfix\nnow";
    task.status = cd::TaskStatus::NeedsAction;
    task.created = 1;
    task.last_modified = 1;
    task.calendar_href = "/calendars/user/work/";
    tasks.upsert(task);

    char* child_argv[] = {argv[1], const_cast<char*>("list"), const_cast<char*>("task"),
        const_cast<char*>("--fuzzel"), nullptr};
    gchar* stdout_text = nullptr;
    gchar* stderr_text = nullptr;
    gint exit_status = 0;
    require(g_spawn_sync(nullptr, child_argv, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr,
                &stdout_text, &stderr_text, &exit_status, &error),
        error ? error->message : "could not run CLI");
    std::string const output = stdout_text ? stdout_text : "";
    g_free(stdout_text);
    g_free(stderr_text);
    require(g_spawn_check_wait_status(exit_status, &error),
        error ? error->message : "CLI failed");
    require(output.starts_with("stable-task-uid\tDeploy hotfix now — "),
        "Fuzzel output did not preserve stable ID and sanitize display text");
    require(output.find('\t', output.find('\t') + 1) == std::string::npos,
        "Fuzzel output contains more than two columns");

    std::filesystem::remove_all(directory);
    return 0;
}
