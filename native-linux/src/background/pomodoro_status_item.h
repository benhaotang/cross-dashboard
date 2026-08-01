#pragma once

#include "domain/models.h"

extern "C" {
#include <glib.h>
#include <gtk/gtk.h>
#if CD_HAVE_APPINDICATOR
#if CD_APPINDICATOR_AYATANA
#include <libayatana-appindicator/app-indicator.h>
#else
#include <libappindicator/app-indicator.h>
#endif
#endif
}

namespace cd {

class AppViewModel;

class PomodoroStatusItem final {
public:
    explicit PomodoroStatusItem(AppViewModel& vm);
    ~PomodoroStatusItem();

private:
    void on_state_changed(PomodoroState const& state);
    void set_label(PomodoroState const& state);

    static void pause_cb(GtkMenuItem*, gpointer user_data);
    static void stop_cb(GtkMenuItem*, gpointer user_data);
    static void skip_cb(GtkMenuItem*, gpointer user_data);
    static void show_window_cb(GtkMenuItem*, gpointer user_data);

    AppViewModel& vm_;
    PomodoroState latest_state_{};
    bool warned_passive_{false};
#if CD_HAVE_APPINDICATOR
    AppIndicator* indicator_{nullptr};
    GtkWidget* menu_{nullptr};
#endif
};

} // namespace cd
