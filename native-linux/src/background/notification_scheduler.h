#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace cd {

class AppContainer;

class NotificationScheduler final {
public:
    explicit NotificationScheduler(AppContainer& app);
    ~NotificationScheduler();

    NotificationScheduler(NotificationScheduler const&) = delete;
    NotificationScheduler& operator=(NotificationScheduler const&) = delete;

    void reschedule_all();

private:
    struct AlarmTimer {
        std::uint32_t source_id{};
        std::string uid;
    };

    AppContainer& app_;
    std::unordered_map<int, AlarmTimer> timers_;
};

} // namespace cd
