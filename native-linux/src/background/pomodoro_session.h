#pragma once

#include "domain/models.h"

#include <optional>
#include <string>

namespace cd {

struct PublishedPomodoroSession {
    long long owner_pid{};
    std::string kind;
    std::string id;
    std::string title;
    std::string phase{"Focus"};
    bool running{true};
    int seconds_left{};
    int duration_seconds{};
    EpochMillis updated_epoch{};
};

void publish_pomodoro_session(PublishedPomodoroSession const& session);
void clear_owned_pomodoro_session();
[[nodiscard]] std::optional<PublishedPomodoroSession> read_pomodoro_session();
[[nodiscard]] int current_seconds_left(PublishedPomodoroSession const& session,
    EpochMillis now_epoch);

} // namespace cd
