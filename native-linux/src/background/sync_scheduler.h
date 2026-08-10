#pragma once

#include <gio/gio.h>
#include <sigc++/signal.h>

#include <cstdint>
#include <string>

namespace cd {

class SyncScheduler final {
public:
    SyncScheduler();
    ~SyncScheduler();

    SyncScheduler(SyncScheduler const&) = delete;
    SyncScheduler& operator=(SyncScheduler const&) = delete;

    void start(int interval_seconds);
    void stop();
    void sync_once();
    void refresh_background();

    /** Fired on the GTK main thread when the service reports that its sync has completed. */
    sigc::signal<void()> signal_sync_completed;
    sigc::signal<void(bool, std::string const&)> signal_background_updated;

private:
    static void sync_signal_cb(GDBusConnection* connection, char const* sender_name,
        char const* object_path, char const* interface_name, char const* signal_name,
        GVariant* parameters, void* user_data);
    static void background_signal_cb(GDBusConnection* connection, char const* sender_name,
        char const* object_path, char const* interface_name, char const* signal_name,
        GVariant* parameters, void* user_data);
    static void sync_requested_cb(GObject* source, GAsyncResult* result, void* user_data);
    static void background_requested_cb(GObject* source, GAsyncResult* result, void* user_data);

    GDBusConnection* connection_{};
    std::uint32_t subscription_id_{};
    std::uint32_t background_subscription_id_{};
};

} // namespace cd
