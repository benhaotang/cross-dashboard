#include "pomodoro_status_item.h"

#include "ui/app_viewmodel.h"

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

PomodoroStatusItem::PomodoroStatusItem(AppViewModel& vm)
    : vm_(vm)
{
    vm_.signal_pomodoro_state_changed.connect(
        [this](PomodoroState const& state) { on_state_changed(state); });

#if CD_HAVE_APPINDICATOR
    indicator_ = app_indicator_new(
        "crossdashboard-pomodoro", "alarm-symbolic", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if (!indicator_)
        return;

    menu_ = gtk_menu_new();
    GtkWidget* pause_item = gtk_menu_item_new_with_label("Pause/Resume");
    GtkWidget* stop_item = gtk_menu_item_new_with_label("Stop");
    GtkWidget* skip_item = gtk_menu_item_new_with_label("Skip");
    GtkWidget* show_item = gtk_menu_item_new_with_label("Show Window");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), pause_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), stop_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), skip_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), show_item);
    gtk_widget_show_all(menu_);

    g_signal_connect(pause_item, "activate", G_CALLBACK(&PomodoroStatusItem::pause_cb), this);
    g_signal_connect(stop_item, "activate", G_CALLBACK(&PomodoroStatusItem::stop_cb), this);
    g_signal_connect(skip_item, "activate", G_CALLBACK(&PomodoroStatusItem::skip_cb), this);
    g_signal_connect(show_item, "activate", G_CALLBACK(&PomodoroStatusItem::show_window_cb), this);

    app_indicator_set_menu(indicator_, GTK_MENU(menu_));
    app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);
    if (app_indicator_get_status(indicator_) == APP_INDICATOR_STATUS_PASSIVE) {
        if (!warned_passive_) {
            std::fprintf(stderr,
                "pomodoro status item unavailable (appindicator service passive); using in-window bar only\n");
            warned_passive_ = true;
        }
        app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_PASSIVE);
        indicator_ = nullptr;
    }
#endif
}

PomodoroStatusItem::~PomodoroStatusItem()
{
#if CD_HAVE_APPINDICATOR
    if (indicator_)
        app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_PASSIVE);
#endif
}

void PomodoroStatusItem::on_state_changed(PomodoroState const& state)
{
    latest_state_ = state;
    set_label(state);
}

void PomodoroStatusItem::set_label(PomodoroState const& state)
{
#if CD_HAVE_APPINDICATOR
    if (!indicator_)
        return;
    if (!state.active) {
        app_indicator_set_label(indicator_, "", "");
        return;
    }
    char buf[16]{};
    fmt_time(state.seconds_left, buf, sizeof(buf));
    app_indicator_set_label(indicator_, buf, "");
#else
    (void)state;
#endif
}

void PomodoroStatusItem::pause_cb(GtkMenuItem*, gpointer user_data)
{
    static_cast<PomodoroStatusItem*>(user_data)->vm_.pause_or_resume_pomodoro();
}

void PomodoroStatusItem::stop_cb(GtkMenuItem*, gpointer user_data)
{
    static_cast<PomodoroStatusItem*>(user_data)->vm_.stop_pomodoro();
}

void PomodoroStatusItem::skip_cb(GtkMenuItem*, gpointer user_data)
{
    static_cast<PomodoroStatusItem*>(user_data)->vm_.skip_pomodoro_phase();
}

void PomodoroStatusItem::show_window_cb(GtkMenuItem*, gpointer user_data)
{
    static_cast<PomodoroStatusItem*>(user_data)->vm_.request_present_window();
}

} // namespace cd
