#include "pomodoro_bar.h"

#include "app_viewmodel.h"
#include "domain/models.h"

extern "C" {
#include <gtk/gtk.h>
}

#include <cstdio>

namespace cd {

namespace {

void set_button_label(GtkWidget* btn, char const* label)
{
    GtkWidget* child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_LABEL(child))
        gtk_label_set_text(GTK_LABEL(child), label);
}

void fmt_time(int seconds, char* out, std::size_t out_size)
{
    int const mm = seconds / 60;
    int const ss = seconds % 60;
    std::snprintf(out, out_size, "%02d:%02d", mm, ss);
}

} // namespace

PomodoroBar::PomodoroBar(AppViewModel& vm)
    : vm_(vm)
{
    root_ = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(root_), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_widget_set_halign(root_, GTK_ALIGN_END);
    gtk_widget_set_valign(root_, GTK_ALIGN_END);
    gtk_widget_set_margin_end(root_, 16);
    gtk_widget_set_margin_bottom(root_, 16);

    GtkWidget* frame = gtk_frame_new(nullptr);
    gtk_container_add(GTK_CONTAINER(root_), frame);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(row), 8);
    gtk_container_add(GTK_CONTAINER(frame), row);

    phase_label_ = gtk_label_new("Focus");
    gtk_widget_set_halign(phase_label_, GTK_ALIGN_START);
    time_label_ = gtk_label_new("25:00");
    gtk_widget_set_halign(time_label_, GTK_ALIGN_START);

    pause_btn_ = gtk_button_new_with_label("Pause");
    stop_btn_ = gtk_button_new_with_label("Stop");
    open_btn_ = gtk_button_new_with_label("Open");

    gtk_box_pack_start(GTK_BOX(row), phase_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), time_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), open_btn_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), pause_btn_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), stop_btn_, FALSE, FALSE, 0);

    g_signal_connect(pause_btn_, "clicked", G_CALLBACK(&PomodoroBar::pause_clicked_cb), this);
    g_signal_connect(stop_btn_, "clicked", G_CALLBACK(&PomodoroBar::stop_clicked_cb), this);
    g_signal_connect(open_btn_, "clicked", G_CALLBACK(&PomodoroBar::open_clicked_cb), this);

    vm_.signal_pomodoro_state_changed.connect([this](PomodoroState const& state) { update(state); });
    update(vm_.pomodoro_state());
}

void PomodoroBar::update(PomodoroState const& state)
{
    gtk_revealer_set_reveal_child(GTK_REVEALER(root_), state.active && !vm_.pomodoro_modal_visible());
    gtk_label_set_text(GTK_LABEL(phase_label_), pomodoro_phase_label(state.phase));

    char time_buf[16]{};
    fmt_time(state.seconds_left, time_buf, sizeof(time_buf));
    gtk_label_set_text(GTK_LABEL(time_label_), time_buf);

    set_button_label(pause_btn_, state.running ? "Pause" : "Resume");
}

void PomodoroBar::pause_clicked_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroBar*>(user_data)->vm_.pause_or_resume_pomodoro();
}

void PomodoroBar::stop_clicked_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroBar*>(user_data)->vm_.stop_pomodoro();
}

void PomodoroBar::open_clicked_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroBar*>(user_data)->vm_.set_pomodoro_modal_visible(true);
}

} // namespace cd
