#pragma once

#include <gtk/gtk.h>

typedef struct _GtkWidget GtkWidget;

namespace cd {

class AppViewModel;
struct PomodoroState;

class PomodoroBar final {
public:
    explicit PomodoroBar(AppViewModel& vm);
    ~PomodoroBar() = default;

    [[nodiscard]] GtkWidget* widget() const { return root_; }

private:
    void update(PomodoroState const& state);
    static void pause_clicked_cb(GtkButton*, gpointer user_data);
    static void stop_clicked_cb(GtkButton*, gpointer user_data);
    static void open_clicked_cb(GtkButton*, gpointer user_data);

    GtkWidget* root_{};
    GtkWidget* phase_label_{};
    GtkWidget* time_label_{};
    GtkWidget* pause_btn_{};
    GtkWidget* stop_btn_{};
    GtkWidget* open_btn_{};
    AppViewModel& vm_;
};

} // namespace cd
