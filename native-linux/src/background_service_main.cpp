#include "app_container.h"
#include "background/notification_scheduler.h"
#include "background/service_dbus.h"
#include "background/sync_runner.h"
#include "data/prefs/prefs.h"

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <libnotify/notify.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr char kIntrospectionXml[] = R"XML(
<node>
  <interface name='com.crossdashboard.Service1'>
    <method name='Sync'>
      <arg type='b' name='accepted' direction='out'/>
    </method>
    <signal name='SyncCompleted'>
      <arg type='b' name='success'/>
      <arg type='as' name='errors'/>
    </signal>
  </interface>
</node>
)XML";

struct ServiceState;

struct SyncCompletion {
    ServiceState* state{};
    std::vector<std::string> errors;
};

struct ServiceState {
    cd::AppContainer app;
    cd::NotificationScheduler notifications{app};
    GMainLoop* loop{g_main_loop_new(nullptr, FALSE)};
    GDBusConnection* connection{};
    guint registration_id{};
    guint periodic_id{};
    guint alarm_refresh_id{};
    int interval_seconds{};
    std::atomic_bool syncing{false};
    bool shutdown_requested{};
    std::thread worker;

    ~ServiceState()
    {
        if (periodic_id != 0) g_source_remove(periodic_id);
        if (alarm_refresh_id != 0) g_source_remove(alarm_refresh_id);
        if (worker.joinable()) worker.join();
        if (connection && registration_id != 0)
            g_dbus_connection_unregister_object(connection, registration_id);
        if (connection) g_object_unref(connection);
        if (loop) g_main_loop_unref(loop);
    }
};

gboolean periodic_sync(gpointer user_data);

gboolean refresh_alarms(gpointer user_data)
{
    static_cast<ServiceState*>(user_data)->notifications.reschedule_all();
    return G_SOURCE_CONTINUE;
}

void schedule_periodic(ServiceState& state)
{
    cd::AppSettings const settings = cd::merged_app_preferences(state.app.prefs());
    int const interval = std::max(60, settings.widget_sync_interval_minutes * 60);
    if (state.periodic_id != 0 && state.interval_seconds == interval) return;
    if (state.periodic_id != 0) g_source_remove(state.periodic_id);
    state.interval_seconds = interval;
    state.periodic_id = g_timeout_add_seconds(interval, &periodic_sync, &state);
}

void emit_sync_completed(ServiceState& state, std::vector<std::string> const& errors)
{
    if (!state.connection) return;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (auto const& error : errors) g_variant_builder_add(&builder, "s", error.c_str());
    GError* error = nullptr;
    g_dbus_connection_emit_signal(state.connection, nullptr, cd::service_dbus::kObjectPath,
        cd::service_dbus::kInterface, cd::service_dbus::kSyncCompletedSignal,
        g_variant_new("(b@as)", errors.empty() ? TRUE : FALSE, g_variant_builder_end(&builder)), &error);
    if (error) {
        std::fprintf(stderr, "cross-dashboard-service: emit signal: %s\n", error->message);
        g_error_free(error);
    }
}

gboolean finish_sync(gpointer user_data)
{
    std::unique_ptr<SyncCompletion> completion(static_cast<SyncCompletion*>(user_data));
    ServiceState& state = *completion->state;
    if (state.worker.joinable()) state.worker.join();
    state.notifications.reschedule_all();
    schedule_periodic(state);
    for (auto const& error : completion->errors)
        std::fprintf(stderr, "cross-dashboard-service: %s\n", error.c_str());
    emit_sync_completed(state, completion->errors);
    state.syncing = false;
    if (state.shutdown_requested) g_main_loop_quit(state.loop);
    return G_SOURCE_REMOVE;
}

bool request_sync(ServiceState& state)
{
    bool expected = false;
    if (!state.syncing.compare_exchange_strong(expected, true)) return false;
    if (state.worker.joinable()) state.worker.join();
    state.worker = std::thread([&state] {
        std::vector<std::string> errors;
        try {
            errors = cd::sync_all(state.app);
        }
        catch (std::exception const& error) {
            errors.emplace_back(std::string{"sync: "} + error.what());
        }
        catch (...) {
            errors.emplace_back("sync: unknown fatal error");
        }
        auto* completion = new SyncCompletion{&state, std::move(errors)};
        g_main_context_invoke(nullptr, &finish_sync, completion);
    });
    return true;
}

gboolean periodic_sync(gpointer user_data)
{
    request_sync(*static_cast<ServiceState*>(user_data));
    return G_SOURCE_CONTINUE;
}

gboolean initial_sync(gpointer user_data)
{
    request_sync(*static_cast<ServiceState*>(user_data));
    return G_SOURCE_REMOVE;
}

gboolean request_shutdown(gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    state.shutdown_requested = true;
    if (!state.syncing.load()) g_main_loop_quit(state.loop);
    return G_SOURCE_REMOVE;
}

void handle_method_call(GDBusConnection*, char const*, char const*, char const*, char const* method,
    GVariant*, GDBusMethodInvocation* invocation, gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    if (std::string{method} == cd::service_dbus::kSyncMethod) {
        bool const accepted = request_sync(state);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", accepted));
        return;
    }
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
        "Unknown Cross-Dashboard service method");
}

GDBusInterfaceVTable const kInterfaceVtable = {&handle_method_call, nullptr, nullptr, {0}};

void bus_acquired(GDBusConnection* connection, char const*, gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    state.connection = G_DBUS_CONNECTION(g_object_ref(connection));
    GError* error = nullptr;
    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(kIntrospectionXml, &error);
    if (!info) {
        std::fprintf(stderr, "cross-dashboard-service: D-Bus introspection: %s\n",
            error ? error->message : "unknown error");
        if (error) g_error_free(error);
        g_main_loop_quit(state.loop);
        return;
    }
    state.registration_id = g_dbus_connection_register_object(connection,
        cd::service_dbus::kObjectPath, info->interfaces[0], &kInterfaceVtable, &state, nullptr, &error);
    g_dbus_node_info_unref(info);
    if (state.registration_id == 0) {
        std::fprintf(stderr, "cross-dashboard-service: D-Bus registration: %s\n",
            error ? error->message : "unknown error");
        if (error) g_error_free(error);
        g_main_loop_quit(state.loop);
    }
}

void name_lost(GDBusConnection*, char const*, gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    std::fprintf(stderr, "cross-dashboard-service: could not own %s\n", cd::service_dbus::kBusName);
    g_main_loop_quit(state.loop);
}

} // namespace

int main()
{
    try {
        ServiceState state;
        (void)notify_init("cross-dashboard");
        state.notifications.reschedule_all();
        state.alarm_refresh_id = g_timeout_add_seconds(60, &refresh_alarms, &state);

        schedule_periodic(state);
        g_idle_add(&initial_sync, &state);
        g_unix_signal_add(SIGTERM, &request_shutdown, &state);
        g_unix_signal_add(SIGINT, &request_shutdown, &state);

        guint const owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, cd::service_dbus::kBusName,
            G_BUS_NAME_OWNER_FLAGS_NONE, &bus_acquired, nullptr, &name_lost, &state, nullptr);
        g_main_loop_run(state.loop);
        g_bus_unown_name(owner_id);
        if (notify_is_initted()) notify_uninit();
        return state.registration_id == 0 ? 1 : 0;
    }
    catch (std::exception const& error) {
        std::fprintf(stderr, "cross-dashboard-service: %s\n", error.what());
        return 1;
    }
}
