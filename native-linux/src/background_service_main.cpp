#include "app_container.h"
#include "background/notification_scheduler.h"
#include "background/pomodoro_session.h"
#include "background/service_dbus.h"
#include "background/sync_runner.h"
#include "background/background_manager.h"
#include "data/db/task_dao.h"
#include "data/prefs/prefs.h"

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <libnotify/notify.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
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
    <method name='RefreshBackground'><arg type='b' name='accepted' direction='out'/></method>
    <signal name='BackgroundUpdated'><arg type='b' name='success'/><arg type='s' name='message'/></signal>
    <method name='StartPomodoro'>
      <arg type='s' name='kind' direction='in'/><arg type='s' name='id' direction='in'/>
      <arg type='s' name='title' direction='in'/><arg type='s' name='phase' direction='in'/>
      <arg type='i' name='minutes' direction='in'/><arg type='b' name='started' direction='out'/>
    </method>
    <method name='GetPomodoroState'>
      <arg type='b' name='active' direction='out'/><arg type='b' name='running' direction='out'/>
      <arg type='i' name='seconds_left' direction='out'/><arg type='i' name='duration_seconds' direction='out'/>
      <arg type='i' name='completed_sessions' direction='out'/><arg type='s' name='phase' direction='out'/>
      <arg type='s' name='kind' direction='out'/><arg type='s' name='id' direction='out'/>
      <arg type='s' name='title' direction='out'/>
    </method>
    <method name='PausePomodoro'><arg type='b' name='changed' direction='out'/></method>
    <method name='ResumePomodoro'><arg type='b' name='changed' direction='out'/></method>
    <method name='StopPomodoro'><arg type='b' name='changed' direction='out'/></method>
    <method name='SkipPomodoro'><arg type='b' name='changed' direction='out'/></method>
    <signal name='PomodoroStateChanged'>
      <arg type='b' name='active'/><arg type='b' name='running'/>
      <arg type='i' name='seconds_left'/><arg type='i' name='duration_seconds'/>
      <arg type='i' name='completed_sessions'/><arg type='s' name='phase'/>
      <arg type='s' name='kind'/><arg type='s' name='id'/><arg type='s' name='title'/>
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
    cd::BackgroundManager backgrounds{app};
    cd::NotificationScheduler notifications{app};
    GMainLoop* loop{g_main_loop_new(nullptr, FALSE)};
    GDBusConnection* connection{};
    guint registration_id{};
    guint portal_settings_subscription_id{};
    guint periodic_id{};
    guint alarm_refresh_id{};
    guint pomodoro_tick_id{};
    int interval_seconds{};
    std::atomic_bool syncing{false};
    bool shutdown_requested{};
    std::thread worker;
    bool pomodoro_active{};
    bool pomodoro_running{};
    int pomodoro_seconds_left{};
    int pomodoro_duration_seconds{};
    int pomodoro_completed_sessions{};
    std::string pomodoro_phase{"focus"};
    std::string pomodoro_kind;
    std::string pomodoro_id;
    std::string pomodoro_title;
    cd::EpochMillis pomodoro_phase_started{};

    ~ServiceState()
    {
        if (periodic_id != 0) g_source_remove(periodic_id);
        if (alarm_refresh_id != 0) g_source_remove(alarm_refresh_id);
        if (pomodoro_tick_id != 0) g_source_remove(pomodoro_tick_id);
        cd::clear_owned_pomodoro_session();
        if (worker.joinable()) worker.join();
        if (connection && registration_id != 0)
            g_dbus_connection_unregister_object(connection, registration_id);
        if (connection && portal_settings_subscription_id != 0)
            g_dbus_connection_signal_unsubscribe(connection, portal_settings_subscription_id);
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

cd::EpochMillis now_millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

char const* phase_label(std::string const& phase)
{
    if (phase == "short-break") return "Short break";
    if (phase == "long-break") return "Long break";
    return "Focus";
}

int phase_duration(ServiceState& state, std::string const& phase)
{
    auto const settings = cd::merged_app_preferences(state.app.prefs()).pomodoro_settings;
    if (phase == "short-break") return std::max(1, settings.short_break_minutes) * 60;
    if (phase == "long-break") return std::max(1, settings.long_break_minutes) * 60;
    return std::max(1, settings.work_minutes) * 60;
}

GVariant* pomodoro_state_variant(ServiceState const& state)
{
    return g_variant_new("(bbiiissss)", state.pomodoro_active, state.pomodoro_running,
        state.pomodoro_seconds_left, state.pomodoro_duration_seconds,
        state.pomodoro_completed_sessions, state.pomodoro_phase.c_str(),
        state.pomodoro_kind.c_str(), state.pomodoro_id.c_str(), state.pomodoro_title.c_str());
}

void emit_pomodoro_state(ServiceState& state)
{
    if (state.pomodoro_active) {
        cd::publish_pomodoro_session({static_cast<long long>(getpid()), state.pomodoro_kind,
            state.pomodoro_id, state.pomodoro_title, phase_label(state.pomodoro_phase),
            state.pomodoro_running, state.pomodoro_seconds_left,
            state.pomodoro_duration_seconds, now_millis()});
    }
    else {
        cd::clear_owned_pomodoro_session();
    }
    if (!state.connection) return;
    GError* error = nullptr;
    g_dbus_connection_emit_signal(state.connection, nullptr, cd::service_dbus::kObjectPath,
        cd::service_dbus::kInterface, cd::service_dbus::kPomodoroStateChangedSignal,
        pomodoro_state_variant(state), &error);
    if (error) {
        std::fprintf(stderr, "cross-dashboard-service: Pomodoro signal: %s\n", error->message);
        g_error_free(error);
    }
}

void show_notification(ServiceState& state, char const* title, std::string const& body)
{
    if (!cd::merged_app_preferences(state.app.prefs()).notifications_enabled) return;
    NotifyNotification* notification = notify_notification_new(title, body.c_str(), "alarm-symbolic");
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_show(notification, nullptr);
    g_object_unref(notification);
}

std::string hhmm(cd::EpochMillis value)
{
    GDateTime* time = g_date_time_new_from_unix_local(value / 1000);
    if (!time) return "?";
    gchar* formatted = g_date_time_format(time, "%R");
    std::string result = formatted ? formatted : "?";
    g_free(formatted);
    g_date_time_unref(time);
    return result;
}

void log_focus_session(ServiceState& state, cd::EpochMillis ended)
{
    state.app.stats().increment_pomodoro();
    if (state.pomodoro_kind != "task" || state.pomodoro_id.empty()) return;
    cd::TaskDao dao(state.app.db());
    auto task = dao.get_by_uid(state.pomodoro_id);
    if (!task) return;
    std::string const line = "🍅 Pomodoro: " + hhmm(state.pomodoro_phase_started) + "–" + hhmm(ended);
    task->description = task->description && !task->description->empty()
        ? std::optional<std::string>{*task->description + "\n" + line}
        : std::optional<std::string>{line};
    task->last_modified = ended;
    try {
        state.app.tasks().update(*task);
    }
    catch (std::exception const& error) {
        std::fprintf(stderr, "cross-dashboard-service: Pomodoro log: %s\n", error.what());
    }
}

void begin_pomodoro_phase(ServiceState& state, std::string phase, int override_minutes = 0)
{
    state.pomodoro_phase = std::move(phase);
    state.pomodoro_duration_seconds = override_minutes > 0
        ? override_minutes * 60 : phase_duration(state, state.pomodoro_phase);
    state.pomodoro_seconds_left = state.pomodoro_duration_seconds;
    state.pomodoro_phase_started = now_millis();
    state.pomodoro_active = true;
    state.pomodoro_running = true;
    emit_pomodoro_state(state);
}

void complete_pomodoro_phase(ServiceState& state)
{
    auto const settings = cd::merged_app_preferences(state.app.prefs()).pomodoro_settings;
    if (state.pomodoro_phase == "focus") {
        ++state.pomodoro_completed_sessions;
        log_focus_session(state, now_millis());
        show_notification(state, "Pomodoro complete", state.pomodoro_title + " — time for a break");
        int const cycle = std::max(1, settings.sessions_until_long_break);
        begin_pomodoro_phase(state,
            state.pomodoro_completed_sessions % cycle == 0 ? "long-break" : "short-break");
    }
    else {
        show_notification(state, "Break complete", "Ready for the next focus session");
        begin_pomodoro_phase(state, "focus");
    }
}

gboolean pomodoro_tick(gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    if (!state.pomodoro_active) {
        state.pomodoro_tick_id = 0;
        return G_SOURCE_REMOVE;
    }
    if (state.pomodoro_running && state.pomodoro_seconds_left > 0)
        --state.pomodoro_seconds_left;
    if (state.pomodoro_running && state.pomodoro_seconds_left <= 0)
        complete_pomodoro_phase(state);
    else
        emit_pomodoro_state(state);
    return G_SOURCE_CONTINUE;
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

void refresh_background(ServiceState& state)
{
    std::string message; bool const success=state.backgrounds.refresh(message);
    if(state.connection) g_dbus_connection_emit_signal(state.connection,nullptr,cd::service_dbus::kObjectPath,
        cd::service_dbus::kInterface,cd::service_dbus::kBackgroundUpdatedSignal,g_variant_new("(bs)",success,message.c_str()),nullptr);
    if(!success && message!="Background updates are disabled" && message!="No enabled background snapshot")
        std::fprintf(stderr,"cross-dashboard-service: %s\n",message.c_str());
}

gboolean refresh_background_idle(gpointer user_data)
{
    refresh_background(*static_cast<ServiceState*>(user_data));return G_SOURCE_REMOVE;
}

void portal_setting_changed(GDBusConnection*, char const*, char const*, char const*, char const*,
    GVariant* parameters, gpointer user_data)
{
    char const* setting_namespace{}; char const* key{}; GVariant* value{};
    g_variant_get(parameters,"(&s&s@v)",&setting_namespace,&key,&value);
    if(value)g_variant_unref(value);
    if(std::string{setting_namespace}=="org.freedesktop.appearance"&&std::string{key}=="color-scheme")
        g_idle_add(&refresh_background_idle,user_data);
}

gboolean finish_sync(gpointer user_data)
{
    std::unique_ptr<SyncCompletion> completion(static_cast<SyncCompletion*>(user_data));
    ServiceState& state = *completion->state;
    if (state.worker.joinable()) state.worker.join();
    state.notifications.reschedule_all();
    refresh_background(state);
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
    GVariant* parameters, GDBusMethodInvocation* invocation, gpointer user_data)
{
    auto& state = *static_cast<ServiceState*>(user_data);
    if (std::string{method} == cd::service_dbus::kSyncMethod) {
        bool const accepted = request_sync(state);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", accepted));
        return;
    }
    if (std::string{method} == cd::service_dbus::kRefreshBackgroundMethod) {
        std::string message; bool const success=state.backgrounds.refresh(message);
        if (state.connection) g_dbus_connection_emit_signal(state.connection, nullptr,
            cd::service_dbus::kObjectPath, cd::service_dbus::kInterface,
            cd::service_dbus::kBackgroundUpdatedSignal,
            g_variant_new("(bs)", success, message.c_str()), nullptr);
        g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",TRUE));
        return;
    }
    if (std::string{method} == cd::service_dbus::kGetPomodoroStateMethod) {
        g_dbus_method_invocation_return_value(invocation, pomodoro_state_variant(state));
        return;
    }
    if (std::string{method} == cd::service_dbus::kStartPomodoroMethod) {
        char const* kind = nullptr;
        char const* id = nullptr;
        char const* title = nullptr;
        char const* phase = nullptr;
        gint minutes = 0;
        g_variant_get(parameters, "(&s&s&s&si)", &kind, &id, &title, &phase, &minutes);
        bool const valid_phase = std::string{phase} == "focus" || std::string{phase} == "short-break"
            || std::string{phase} == "long-break";
        bool const started = !state.pomodoro_active && valid_phase && minutes >= 0 && minutes <= 24 * 60;
        if (started) {
            state.pomodoro_kind = kind;
            state.pomodoro_id = id;
            state.pomodoro_title = title;
            state.pomodoro_completed_sessions = 0;
            begin_pomodoro_phase(state, phase, minutes);
            if (state.pomodoro_tick_id == 0)
                state.pomodoro_tick_id = g_timeout_add_seconds(1, &pomodoro_tick, &state);
            show_notification(state, "Pomodoro started", state.pomodoro_title.empty()
                    ? phase_label(state.pomodoro_phase) : state.pomodoro_title);
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", started));
        return;
    }
    auto return_changed = [&](bool changed) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", changed));
    };
    if (std::string{method} == cd::service_dbus::kPausePomodoroMethod) {
        bool const changed = state.pomodoro_active && state.pomodoro_running;
        if (changed) {
            state.pomodoro_running = false;
            emit_pomodoro_state(state);
        }
        return_changed(changed);
        return;
    }
    if (std::string{method} == cd::service_dbus::kResumePomodoroMethod) {
        bool const changed = state.pomodoro_active && !state.pomodoro_running;
        if (changed) {
            state.pomodoro_running = true;
            state.pomodoro_phase_started = now_millis()
                - static_cast<cd::EpochMillis>(state.pomodoro_duration_seconds
                    - state.pomodoro_seconds_left) * 1000;
            emit_pomodoro_state(state);
        }
        return_changed(changed);
        return;
    }
    if (std::string{method} == cd::service_dbus::kStopPomodoroMethod) {
        bool const changed = state.pomodoro_active;
        if (changed) {
            state.pomodoro_active = false;
            state.pomodoro_running = false;
            state.pomodoro_seconds_left = 0;
            if (state.pomodoro_tick_id != 0) {
                g_source_remove(state.pomodoro_tick_id);
                state.pomodoro_tick_id = 0;
            }
            emit_pomodoro_state(state);
        }
        return_changed(changed);
        return;
    }
    if (std::string{method} == cd::service_dbus::kSkipPomodoroMethod) {
        bool const changed = state.pomodoro_active;
        if (changed) complete_pomodoro_phase(state);
        return_changed(changed);
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
    state.portal_settings_subscription_id=g_dbus_connection_signal_subscribe(connection,
        "org.freedesktop.portal.Desktop","org.freedesktop.portal.Settings","SettingChanged",
        "/org/freedesktop/portal/desktop",nullptr,G_DBUS_SIGNAL_FLAGS_NONE,
        &portal_setting_changed,&state,nullptr);
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
    else {
        emit_pomodoro_state(state);
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
        refresh_background(state);
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
