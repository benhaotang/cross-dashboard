#pragma once

#include <cstdint>

namespace cd {

class AppContainer;
class NotificationScheduler;

class SyncScheduler final {
public:
    SyncScheduler(AppContainer& app, NotificationScheduler& notifications);
    ~SyncScheduler();

    SyncScheduler(SyncScheduler const&) = delete;
    SyncScheduler& operator=(SyncScheduler const&) = delete;

    void start(int interval_seconds);
    void stop();
    void sync_once();

private:
    bool ensure_systemd_units(int interval_seconds);

    AppContainer& app_;
    NotificationScheduler& notifications_;
    std::uint32_t source_id_{};
};

} // namespace cd
