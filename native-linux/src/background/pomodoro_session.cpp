#include "pomodoro_session.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <string>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <nlohmann/json.hpp>

namespace cd {
namespace {

std::string session_path()
{
    char const* runtime = g_get_user_runtime_dir();
    std::string directory;
    if (runtime && *runtime) directory = std::string{runtime} + "/cross-dashboard";
    else directory = std::string{g_get_tmp_dir()} + "/cross-dashboard-" + std::to_string(getuid());
    if (g_mkdir_with_parents(directory.c_str(), 0700) != 0) return {};
    return directory + "/pomodoro.json";
}

std::optional<PublishedPomodoroSession> read_unchecked()
{
    std::string const path = session_path();
    gchar* contents = nullptr;
    gsize length = 0;
    if (path.empty() || !g_file_get_contents(path.c_str(), &contents, &length, nullptr))
        return std::nullopt;
    auto const value = nlohmann::json::parse(contents, contents + length, nullptr, false);
    g_free(contents);
    if (!value.is_object()) return std::nullopt;
    PublishedPomodoroSession result;
    result.owner_pid = value.value("pid", -1LL);
    result.kind = value.value("kind", std::string{});
    result.id = value.value("id", std::string{});
    result.title = value.value("title", std::string{});
    result.phase = value.value("phase", std::string{"Focus"});
    result.running = value.value("running", true);
    result.seconds_left = value.value("secondsLeft", 0);
    result.duration_seconds = value.value("durationSeconds", 0);
    result.updated_epoch = value.value("updatedEpoch", 0LL);
    return result;
}

} // namespace

void publish_pomodoro_session(PublishedPomodoroSession const& session)
{
    std::string const path = session_path();
    if (path.empty()) return;
    nlohmann::json const value = {{"version", 1}, {"pid", session.owner_pid},
        {"kind", session.kind}, {"id", session.id}, {"title", session.title},
        {"phase", session.phase}, {"running", session.running},
        {"secondsLeft", session.seconds_left}, {"durationSeconds", session.duration_seconds},
        {"updatedEpoch", session.updated_epoch}};
    std::string const contents = value.dump();
    g_file_set_contents(path.c_str(), contents.c_str(), contents.size(), nullptr);
}

void clear_owned_pomodoro_session()
{
    auto const session = read_unchecked();
    if (!session || session->owner_pid != static_cast<long long>(getpid())) return;
    std::string const path = session_path();
    if (!path.empty()) g_remove(path.c_str());
}

int current_seconds_left(PublishedPomodoroSession const& session, EpochMillis now_epoch)
{
    if (!session.running) return std::max(0, session.seconds_left);
    int const elapsed = static_cast<int>(std::max<EpochMillis>(0, now_epoch - session.updated_epoch) / 1000);
    return std::max(0, session.seconds_left - elapsed);
}

std::optional<PublishedPomodoroSession> read_pomodoro_session()
{
    auto session = read_unchecked();
    if (!session || session->owner_pid <= 0) return std::nullopt;
    if (::kill(static_cast<pid_t>(session->owner_pid), 0) != 0 && errno != EPERM)
        return std::nullopt;
    if (current_seconds_left(*session, [] {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }()) <= 0)
        return std::nullopt;
    return session;
}

} // namespace cd
