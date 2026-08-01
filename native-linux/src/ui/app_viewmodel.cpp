#include "app_viewmodel.h"

#include "app_container.h"
#include "background/service_dbus.h"
#include "data/db/issue_dao.h"
#include "data/db/task_dao.h"
#include "data/prefs/prefs.h"

#include <glib.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace cd {

AppViewModel::AppViewModel(AppContainer& app)
    : app_(app)
{
    AppSettings const settings = merged_app_preferences(app.prefs());
    pomodoro_state_.settings = settings.pomodoro_settings;
    pomodoro_state_.active = false;
    pomodoro_state_.running = false;
    pomodoro_state_.phase = PomodoroPhase::Work;
    pomodoro_state_.seconds_left = phase_duration_seconds(PomodoroPhase::Work);

    GError* error = nullptr;
    pomodoro_bus_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!pomodoro_bus_) {
        std::fprintf(stderr, "Pomodoro service bus unavailable: %s\n",
            error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return;
    }
    pomodoro_subscription_id_ = g_dbus_connection_signal_subscribe(pomodoro_bus_,
        service_dbus::kBusName, service_dbus::kInterface,
        service_dbus::kPomodoroStateChangedSignal, service_dbus::kObjectPath, nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE, &AppViewModel::pomodoro_dbus_signal_cb, this, nullptr);
    GVariant* reply = g_dbus_connection_call_sync(pomodoro_bus_, service_dbus::kBusName,
        service_dbus::kObjectPath, service_dbus::kInterface,
        service_dbus::kGetPomodoroStateMethod, nullptr,
        G_VARIANT_TYPE(service_dbus::kPomodoroStateTupleType), G_DBUS_CALL_FLAGS_NONE,
        10000, nullptr, &error);
    if (reply) {
        apply_pomodoro_variant(reply);
        g_variant_unref(reply);
    }
    else if (error) {
        std::fprintf(stderr, "Pomodoro service unavailable: %s\n", error->message);
        g_error_free(error);
    }
}

AppViewModel::~AppViewModel()
{
    if (pomodoro_bus_ && pomodoro_subscription_id_ != 0)
        g_dbus_connection_signal_unsubscribe(pomodoro_bus_, pomodoro_subscription_id_);
    if (pomodoro_bus_) g_object_unref(pomodoro_bus_);
}

void AppViewModel::trigger_capture(std::string text)
{
    capture_initial_text_ = std::move(text);
    signal_capture_initial_text.emit(capture_initial_text_);
}

void AppViewModel::trigger_new_task()
{
    signal_new_task_requested.emit();
}

void AppViewModel::set_pomodoro_modal_visible(bool visible)
{
    if (pomodoro_modal_visible_ == visible)
        return;
    pomodoro_modal_visible_ = visible;
    signal_pomodoro_modal_visibility_changed.emit(pomodoro_modal_visible_);
}

void AppViewModel::start_pomodoro(PomodoroPhase phase)
{
    if (pomodoro_state_.active) {
        if (!pomodoro_state_.running) call_pomodoro_control(service_dbus::kResumePomodoroMethod);
        return;
    }
    if (!pomodoro_bus_) return;
    char const* phase_name = phase == PomodoroPhase::ShortBreak ? "short-break"
        : phase == PomodoroPhase::LongBreak ? "long-break" : "focus";
    int const minutes = std::max(1, phase_duration_seconds(phase) / 60);
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(pomodoro_bus_, service_dbus::kBusName,
        service_dbus::kObjectPath, service_dbus::kInterface, service_dbus::kStartPomodoroMethod,
        g_variant_new("(ssssi)", active_target_kind_.c_str(), active_target_id_.c_str(),
            active_target_title_.c_str(), phase_name, minutes),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &error);
    if (reply) g_variant_unref(reply);
    if (error) {
        std::fprintf(stderr, "start Pomodoro: %s\n", error->message);
        g_error_free(error);
    }
}

void AppViewModel::pause_or_resume_pomodoro()
{
    if (!pomodoro_state_.active) return;
    call_pomodoro_control(pomodoro_state_.running
            ? service_dbus::kPausePomodoroMethod : service_dbus::kResumePomodoroMethod);
}

void AppViewModel::stop_pomodoro()
{
    call_pomodoro_control(service_dbus::kStopPomodoroMethod);
}

void AppViewModel::skip_pomodoro_phase()
{
    if (pomodoro_state_.active) call_pomodoro_control(service_dbus::kSkipPomodoroMethod);
}

void AppViewModel::set_active_task(std::string uid, std::string title)
{
    set_pomodoro_target("task", std::move(uid), std::move(title));
}

void AppViewModel::set_pomodoro_target(std::string kind, std::string id, std::string title)
{
    if (kind != "task" && kind != "issue") {
        kind = "timer";
        id.clear();
        title.clear();
    }
    active_target_kind_ = std::move(kind);
    active_target_id_ = std::move(id);
    active_target_title_ = std::move(title);
    pomodoro_state_.item_title = active_target_title_;
    emit_pomodoro_state();
}

std::string AppViewModel::pomodoro_target_key() const
{
    return active_target_kind_ + ":" + active_target_id_;
}

std::vector<PomodoroTargetOption> AppViewModel::pomodoro_target_options() const
{
    std::vector<PomodoroTargetOption> result;
    for (auto const& task : TaskDao(app_.db()).get_all()) {
        if (task.status == TaskStatus::Completed || task.status == TaskStatus::Cancelled)
            continue;
        result.push_back({"task:" + task.uid, "task", task.uid, task.summary,
            "Task · " + task.summary});
    }
    for (auto const& issue : IssueDao(app_.db()).get_all()) {
        if (issue.state != "open") continue;
        std::string const id = issue.repository + "#" + std::to_string(issue.number);
        result.push_back({"issue:" + id, "issue", id, issue.title,
            "Issue · " + issue.title + " — " + id});
    }
    std::stable_sort(result.begin(), result.end(), [](auto const& a, auto const& b) {
        if (a.kind != b.kind) return a.kind == "task";
        return a.display < b.display;
    });
    return result;
}

void AppViewModel::request_present_window()
{
    signal_present_window_requested.emit();
}

void AppViewModel::pomodoro_dbus_signal_cb(GDBusConnection*, char const*, char const*, char const*,
    char const*, GVariant* parameters, gpointer user_data)
{
    static_cast<AppViewModel*>(user_data)->apply_pomodoro_variant(parameters);
}

void AppViewModel::apply_pomodoro_variant(GVariant* value)
{
    gboolean active = FALSE;
    gboolean running = FALSE;
    gint seconds = 0;
    gint duration = 0;
    gint completed = 0;
    char const* phase = nullptr;
    char const* kind = nullptr;
    char const* id = nullptr;
    char const* title = nullptr;
    g_variant_get(value, "(bbiii&s&s&s&s)", &active, &running, &seconds, &duration, &completed,
        &phase, &kind, &id, &title);
    pomodoro_state_.active = active != FALSE;
    pomodoro_state_.running = running != FALSE;
    pomodoro_state_.completed_sessions = completed;
    pomodoro_state_.current_session = completed + 1;
    std::string const phase_name = phase ? phase : "focus";
    pomodoro_state_.phase = phase_name == "short-break" ? PomodoroPhase::ShortBreak
        : phase_name == "long-break" ? PomodoroPhase::LongBreak : PomodoroPhase::Work;
    pomodoro_state_.seconds_left = pomodoro_state_.active
        ? seconds : phase_duration_seconds(PomodoroPhase::Work);
    pomodoro_state_.item_title = title ? title : "";
    active_target_kind_ = kind && *kind ? kind : "timer";
    active_target_id_ = id ? id : "";
    active_target_title_ = title ? title : "";
    emit_pomodoro_state();
}

bool AppViewModel::call_pomodoro_control(char const* method)
{
    if (!pomodoro_bus_) return false;
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(pomodoro_bus_, service_dbus::kBusName,
        service_dbus::kObjectPath, service_dbus::kInterface, method, nullptr,
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &error);
    if (!reply) {
        std::fprintf(stderr, "Pomodoro control: %s\n", error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return false;
    }
    gboolean changed = FALSE;
    g_variant_get(reply, "(b)", &changed);
    g_variant_unref(reply);
    return changed != FALSE;
}

void AppViewModel::emit_pomodoro_state()
{
    signal_pomodoro_state_changed.emit(pomodoro_state_);
}

int AppViewModel::phase_duration_seconds(PomodoroPhase phase) const
{
    switch (phase) {
    case PomodoroPhase::Work: return pomodoro_state_.settings.work_minutes * 60;
    case PomodoroPhase::ShortBreak: return pomodoro_state_.settings.short_break_minutes * 60;
    case PomodoroPhase::LongBreak: return pomodoro_state_.settings.long_break_minutes * 60;
    }
    return 25 * 60;
}

} // namespace cd
