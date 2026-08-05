#include "background/sync_scheduler.h"

#include "background/service_dbus.h"

#include <gio/gio.h>

namespace cd {

SyncScheduler::SyncScheduler()
{
}

SyncScheduler::~SyncScheduler()
{
    stop();
}

void SyncScheduler::start(int interval_seconds)
{
    (void)interval_seconds;
    stop();
    GError* error = nullptr;
    connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!connection_) {
        if (error) {
            g_warning("Cross-Dashboard service bus unavailable: %s", error->message);
            g_error_free(error);
        }
        return;
    }
    subscription_id_ = g_dbus_connection_signal_subscribe(connection_, service_dbus::kBusName,
        service_dbus::kInterface, service_dbus::kSyncCompletedSignal, service_dbus::kObjectPath,
        nullptr, G_DBUS_SIGNAL_FLAGS_NONE, &SyncScheduler::sync_signal_cb, this, nullptr);
    background_subscription_id_ = g_dbus_connection_signal_subscribe(connection_, service_dbus::kBusName,
        service_dbus::kInterface, service_dbus::kBackgroundUpdatedSignal, service_dbus::kObjectPath,
        nullptr, G_DBUS_SIGNAL_FLAGS_NONE, &SyncScheduler::background_signal_cb, this, nullptr);
}

void SyncScheduler::stop()
{
    if (connection_) {
        if (subscription_id_ != 0)
            g_dbus_connection_signal_unsubscribe(connection_, subscription_id_);
        if (background_subscription_id_ != 0)
            g_dbus_connection_signal_unsubscribe(connection_, background_subscription_id_);
        subscription_id_ = 0;
        background_subscription_id_ = 0;
        g_object_unref(connection_);
        connection_ = nullptr;
    }
}

void SyncScheduler::sync_once()
{
    if (!connection_) start(0);
    if (!connection_) return;
    g_dbus_connection_call(connection_, service_dbus::kBusName, service_dbus::kObjectPath,
        service_dbus::kInterface, service_dbus::kSyncMethod, nullptr, G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &SyncScheduler::sync_requested_cb, this);
}

void SyncScheduler::refresh_background()
{
    if (!connection_) start(0);
    if (!connection_) return;
    g_dbus_connection_call(connection_, service_dbus::kBusName, service_dbus::kObjectPath,
        service_dbus::kInterface, service_dbus::kRefreshBackgroundMethod, nullptr,
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &SyncScheduler::background_requested_cb, this);
}

void SyncScheduler::background_requested_cb(GObject* source, GAsyncResult* result, void* user_data)
{
    GError* error{}; GVariant* reply=g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),result,&error);
    auto* self=static_cast<SyncScheduler*>(user_data);
    if(!reply){std::string message=error?error->message:"Background service unavailable";if(error)g_error_free(error);self->signal_background_updated.emit(false,message);return;}
    gboolean accepted{};g_variant_get(reply,"(b)",&accepted);g_variant_unref(reply);
    if(!accepted)self->signal_background_updated.emit(false,"Background update request was rejected");
}

void SyncScheduler::sync_signal_cb(GDBusConnection*, char const*, char const*, char const*,
    char const*, GVariant*, void* user_data)
{
    static_cast<SyncScheduler*>(user_data)->signal_sync_completed.emit();
}

void SyncScheduler::background_signal_cb(GDBusConnection*, char const*, char const*, char const*,
    char const*, GVariant* parameters, void* user_data)
{
    gboolean success{}; char const* message{};
    g_variant_get(parameters, "(b&s)", &success, &message);
    static_cast<SyncScheduler*>(user_data)->signal_background_updated.emit(success != FALSE,
        message ? message : "");
}

void SyncScheduler::sync_requested_cb(GObject* source, GAsyncResult* result, void* user_data)
{
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_finish(
        G_DBUS_CONNECTION(source), G_ASYNC_RESULT(result), &error);
    if (reply) {
        g_variant_unref(reply);
        return;
    }
    if (error) {
        g_warning("Could not request Cross-Dashboard service sync: %s", error->message);
        g_error_free(error);
    }
    // Let views reload their current cache and leave the application responsive on service failure.
    static_cast<SyncScheduler*>(user_data)->signal_sync_completed.emit();
}

} // namespace cd
