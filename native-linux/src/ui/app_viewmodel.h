#pragma once

#include "domain/models.h"

#include <glib.h>
#include <sigc++/sigc++.h>

#include <optional>
#include <string>

namespace cd {

class AppContainer;

/** Lightweight cross-screen triggers (captures GObject-style notify pattern via sigc++). */
class AppViewModel {
public:
    explicit AppViewModel(AppContainer& app);
    ~AppViewModel();

    void trigger_capture(std::string text);
    void trigger_new_task();
    void set_pomodoro_modal_visible(bool visible);
    void start_pomodoro(PomodoroPhase phase = PomodoroPhase::Work);
    void pause_or_resume_pomodoro();
    void stop_pomodoro();
    void skip_pomodoro_phase();
    void set_active_task(std::string uid, std::string title);
    void request_present_window();

    [[nodiscard]] std::string capture_initial_text() const { return capture_initial_text_; }
    [[nodiscard]] PomodoroState const& pomodoro_state() const { return pomodoro_state_; }
    [[nodiscard]] bool pomodoro_modal_visible() const { return pomodoro_modal_visible_; }

    sigc::signal<void(std::string const&)> signal_capture_initial_text;
    sigc::signal<void()> signal_new_task_requested;
    sigc::signal<void(PomodoroState const&)> signal_pomodoro_state_changed;
    sigc::signal<void(bool)> signal_pomodoro_modal_visibility_changed;
    sigc::signal<void()> signal_present_window_requested;

private:
    static gboolean pomodoro_tick_cb(gpointer user_data);
    gboolean on_pomodoro_tick();
    void emit_pomodoro_state();
    void start_phase(PomodoroPhase phase);
    void complete_current_phase();
    int phase_duration_seconds(PomodoroPhase phase) const;
    void append_pomodoro_session_log();

    std::string capture_initial_text_;
    PomodoroState pomodoro_state_{};
    bool pomodoro_modal_visible_{false};
    unsigned int pomodoro_timer_id_{0};
    std::optional<std::string> active_task_uid_;
    std::string active_task_title_;
    std::optional<EpochMillis> active_phase_started_at_;
    AppContainer& app_;
};

} // namespace cd
