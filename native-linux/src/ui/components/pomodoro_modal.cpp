#include "pomodoro_modal.h"

#include "app_viewmodel.h"
#include "domain/models.h"

#include <cstdio>

namespace cd {

namespace {

void fmt_time(int seconds, char* out, std::size_t out_size)
{
    int const mm = seconds / 60;
    int const ss = seconds % 60;
    std::snprintf(out, out_size, "%02d:%02d", mm, ss);
}

} // namespace

PomodoroModal::PomodoroModal(AppViewModel& vm)
    : vm_(vm)
{
    dialog_ = gtk_dialog_new_with_buttons("Pomodoro", nullptr, GTK_DIALOG_MODAL,
        "_Close", GTK_RESPONSE_CLOSE, nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 420, 180);

    GtkWidget* box = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);

    phase_label_ = gtk_label_new("Focus");
    time_label_ = gtk_label_new("25:00");
    session_label_ = gtk_label_new("Session 1");
    gtk_box_pack_start(GTK_BOX(box), phase_label_, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), time_label_, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), session_label_, FALSE, FALSE, 4);

    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(box), controls, FALSE, FALSE, 8);

    GtkWidget* start_focus = gtk_button_new_with_label("Start Focus");
    GtkWidget* start_short = gtk_button_new_with_label("Start Short Break");
    GtkWidget* start_long = gtk_button_new_with_label("Start Long Break");
    pause_btn_ = gtk_button_new_with_label("Pause");
    GtkWidget* stop_btn = gtk_button_new_with_label("Stop");
    GtkWidget* skip_btn = gtk_button_new_with_label("Skip");

    gtk_box_pack_start(GTK_BOX(controls), start_focus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), start_short, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), start_long, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), pause_btn_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), stop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), skip_btn, FALSE, FALSE, 0);

    g_signal_connect(dialog_, "response", G_CALLBACK(&PomodoroModal::response_cb), this);
    g_signal_connect(start_focus, "clicked", G_CALLBACK(&PomodoroModal::start_focus_cb), this);
    g_signal_connect(start_short, "clicked", G_CALLBACK(&PomodoroModal::start_short_break_cb), this);
    g_signal_connect(start_long, "clicked", G_CALLBACK(&PomodoroModal::start_long_break_cb), this);
    g_signal_connect(pause_btn_, "clicked", G_CALLBACK(&PomodoroModal::pause_cb), this);
    g_signal_connect(stop_btn, "clicked", G_CALLBACK(&PomodoroModal::stop_cb), this);
    g_signal_connect(skip_btn, "clicked", G_CALLBACK(&PomodoroModal::skip_cb), this);

    vm_.signal_pomodoro_state_changed.connect([this](PomodoroState const& state) { update(state); });
    update(vm_.pomodoro_state());
}

void PomodoroModal::present(GtkWindow* parent)
{
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent);
    vm_.set_pomodoro_modal_visible(true);
    gtk_widget_show_all(dialog_);
    gtk_window_present(GTK_WINDOW(dialog_));
}

void PomodoroModal::update(PomodoroState const& state)
{
    gtk_label_set_text(GTK_LABEL(phase_label_), pomodoro_phase_label(state.phase));
    char time_buf[16]{};
    fmt_time(state.seconds_left, time_buf, sizeof(time_buf));
    gtk_label_set_text(GTK_LABEL(time_label_), time_buf);
    char session_buf[32]{};
    std::snprintf(session_buf, sizeof(session_buf), "Session %d", state.current_session);
    gtk_label_set_text(GTK_LABEL(session_label_), session_buf);

    GtkWidget* pause_child = gtk_bin_get_child(GTK_BIN(pause_btn_));
    if (GTK_IS_LABEL(pause_child))
        gtk_label_set_text(GTK_LABEL(pause_child), state.running ? "Pause" : "Resume");
}

void PomodoroModal::response_cb(GtkDialog* dialog, gint, gpointer user_data)
{
    auto* self = static_cast<PomodoroModal*>(user_data);
    self->vm_.set_pomodoro_modal_visible(false);
    gtk_widget_hide(GTK_WIDGET(dialog));
}

void PomodoroModal::start_focus_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::Work);
}

void PomodoroModal::start_short_break_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::ShortBreak);
}

void PomodoroModal::start_long_break_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::LongBreak);
}

void PomodoroModal::pause_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.pause_or_resume_pomodoro();
}

void PomodoroModal::stop_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.stop_pomodoro();
}

void PomodoroModal::skip_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.skip_pomodoro_phase();
}

} // namespace cd
