#pragma once

#include <gtk/gtk.h>

namespace cd {

class AppViewModel;
struct PomodoroState;

class PomodoroModal final {
public:
    explicit PomodoroModal(AppViewModel& vm);
    ~PomodoroModal() = default;

    void present(GtkWindow* parent);

private:
    void update(PomodoroState const& state);
    static gboolean delete_event_cb(GtkWidget* dlg, GdkEventAny* event, gpointer user_data);
    static void start_focus_cb(GtkButton*, gpointer user_data);
    static void start_short_break_cb(GtkButton*, gpointer user_data);
    static void start_long_break_cb(GtkButton*, gpointer user_data);
    static void pause_cb(GtkButton*, gpointer user_data);
    static void stop_cb(GtkButton*, gpointer user_data);
    static void skip_cb(GtkButton*, gpointer user_data);

    GtkWidget* dialog_{};
    GtkWidget* phase_label_{};
    GtkWidget* time_label_{};
    GtkWidget* session_label_{};
    GtkWidget* task_label_{};
    GtkWidget* idle_section_{};
    GtkWidget* active_section_{};
    GtkWidget* pause_btn_{};
    AppViewModel& vm_;
};

} // namespace cd
