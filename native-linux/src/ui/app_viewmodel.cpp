#include "app_viewmodel.h"

#include "app_container.h"
#include "data/db/task_dao.h"
#include "data/prefs/prefs.h"
#include "data/repository/repositories.h"

#include <glib.h>

#include <chrono>
#include <ctime>
#include <cstdio>
#include <utility>

namespace cd {

namespace {

EpochMillis now_epoch_millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace

AppViewModel::AppViewModel(AppContainer& app)
    : app_(app)
{
    AppSettings const settings = merged_app_preferences(app_.prefs());
    pomodoro_state_.settings = settings.pomodoro_settings;
    pomodoro_state_.active = false;
    pomodoro_state_.running = false;
    pomodoro_state_.phase = PomodoroPhase::Work;
    pomodoro_state_.seconds_left = phase_duration_seconds(PomodoroPhase::Work);
}

AppViewModel::~AppViewModel()
{
    if (pomodoro_timer_id_ != 0) {
        g_source_remove(pomodoro_timer_id_);
        pomodoro_timer_id_ = 0;
    }
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
    if (!pomodoro_state_.active || pomodoro_state_.phase != phase)
        start_phase(phase);
    pomodoro_state_.running = true;
    if (pomodoro_timer_id_ == 0)
        pomodoro_timer_id_ = g_timeout_add_seconds(1, &AppViewModel::pomodoro_tick_cb, this);
    emit_pomodoro_state();
}

void AppViewModel::pause_or_resume_pomodoro()
{
    if (!pomodoro_state_.active)
        return;
    pomodoro_state_.running = !pomodoro_state_.running;
    emit_pomodoro_state();
}

void AppViewModel::stop_pomodoro()
{
    pomodoro_state_.active = false;
    pomodoro_state_.running = false;
    pomodoro_state_.phase = PomodoroPhase::Work;
    pomodoro_state_.seconds_left = phase_duration_seconds(PomodoroPhase::Work);
    pomodoro_state_.current_session = 1;
    active_phase_started_at_.reset();
    if (pomodoro_timer_id_ != 0) {
        g_source_remove(pomodoro_timer_id_);
        pomodoro_timer_id_ = 0;
    }
    emit_pomodoro_state();
}

void AppViewModel::skip_pomodoro_phase()
{
    if (!pomodoro_state_.active)
        return;
    complete_current_phase();
}

void AppViewModel::set_active_task(std::string uid, std::string title)
{
    active_task_uid_ = std::move(uid);
    active_task_title_ = std::move(title);
    pomodoro_state_.item_title = active_task_title_;
    emit_pomodoro_state();
}

void AppViewModel::request_present_window()
{
    signal_present_window_requested.emit();
}

gboolean AppViewModel::pomodoro_tick_cb(gpointer user_data)
{
    return static_cast<AppViewModel*>(user_data)->on_pomodoro_tick();
}

gboolean AppViewModel::on_pomodoro_tick()
{
    if (!pomodoro_state_.active || !pomodoro_state_.running)
        return G_SOURCE_CONTINUE;

    if (pomodoro_state_.seconds_left > 0)
        --pomodoro_state_.seconds_left;

    if (pomodoro_state_.seconds_left <= 0)
        complete_current_phase();
    else
        emit_pomodoro_state();

    return G_SOURCE_CONTINUE;
}

void AppViewModel::emit_pomodoro_state()
{
    signal_pomodoro_state_changed.emit(pomodoro_state_);
}

void AppViewModel::start_phase(PomodoroPhase phase)
{
    pomodoro_state_.phase = phase;
    pomodoro_state_.seconds_left = phase_duration_seconds(phase);
    pomodoro_state_.active = true;
    active_phase_started_at_ = now_epoch_millis();
}

void AppViewModel::complete_current_phase()
{
    PomodoroPhase finished = pomodoro_state_.phase;
    if (finished == PomodoroPhase::Work) {
        ++pomodoro_state_.completed_sessions;
        ++pomodoro_state_.current_session;
        app_.stats().increment_pomodoro();
        append_pomodoro_session_log();
        if (pomodoro_state_.completed_sessions % pomodoro_state_.settings.sessions_until_long_break == 0)
            start_phase(PomodoroPhase::LongBreak);
        else
            start_phase(PomodoroPhase::ShortBreak);
    }
    else {
        start_phase(PomodoroPhase::Work);
    }
    pomodoro_state_.running = true;
    emit_pomodoro_state();
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

void AppViewModel::append_pomodoro_session_log()
{
    if (!active_task_uid_.has_value() || !active_phase_started_at_.has_value())
        return;

    EpochMillis const end_ms = now_epoch_millis();
    auto make_hhmm = [](EpochMillis ms) -> std::string {
        std::time_t const tt = static_cast<std::time_t>(ms / 1000LL);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char buf[16]{};
        std::strftime(buf, sizeof(buf), "%H:%M", &tm);
        return std::string(buf);
    };

    TaskDao dao(app_.db());
    auto task_opt = dao.get_by_uid(*active_task_uid_);
    if (!task_opt.has_value())
        return;

    CalDavTask task = *task_opt;
    std::string const line =
        "🍅 Pomodoro: " + make_hhmm(*active_phase_started_at_) + "–" + make_hhmm(end_ms);
    if (task.description.has_value() && !task.description->empty())
        task.description = *task.description + "\n" + line;
    else
        task.description = line;
    task.last_modified = end_ms;

    try {
        app_.tasks().update(task);
    }
    catch (std::exception const& err) {
        std::fprintf(stderr, "pomodoro log update failed: %s\n", err.what());
    }
}

} // namespace cd
