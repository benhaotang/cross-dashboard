#pragma once

#include <gtkmm/applicationwindow.h>
#include <gtkmm/stack.h>

#include <memory>

typedef struct _GtkListBoxRow GtkListBoxRow;
typedef struct _GtkWidget GtkWidget;

namespace cd {

class AppContainer;
class AppViewModel;
class SyncScheduler;
class DashboardView;
class EventsView;
class InboxView;
class IssuesView;
class NotesView;
class MemosView;
class SettingsView;
class TasksView;
class ViewsView;
class PomodoroBar;
class PomodoroModal;

/** Main window: resizable GtkPaned sidebar + HdyHeaderBar + screen stack. */
class AppWindow final : public Gtk::ApplicationWindow {
public:
    AppWindow(AppContainer&, AppViewModel&, SyncScheduler&);
    AppWindow(AppWindow const&) = delete;
    AppWindow& operator=(AppWindow const&) = delete;

    void on_screen_row(GtkListBoxRow* row);

    [[nodiscard]] GtkWidget* sidebar_box_widget() const { return sidebar_box_; }
    [[nodiscard]] GtkWidget* main_outer_widget() const { return main_outer_; }

private:
    void apply_theme();
    void build_stack_pages();
    void build_sidebar();
    void on_new_task_shortcut();
    void rebuild_navigation();
    void schedule_rebuild_navigation();
    bool on_key_press_event(GdkEventKey* event) override;
    void load_theme_css();
    void focus_search_on_current_screen();
    /** Reloads visible screen from local DB (after sync, tab change, etc.). */
    void refresh_current_screen();

    GtkWidget* paned_{};
    GtkWidget* root_overlay_{};
    GtkWidget* sidebar_box_{};
    GtkWidget* sidebar_list_{};
    GtkWidget* main_outer_{};
    GtkWidget* header_bar_{};

    Gtk::Stack stack_;

    AppContainer& app_;
    AppViewModel& vm_;
    SyncScheduler& sync_;

    std::string current_screen_key_;
    DashboardView* dash_{nullptr};
    TasksView* tasks_{nullptr};
    EventsView* events_{nullptr};
    NotesView* notes_{nullptr};
    MemosView* memos_{nullptr};
    IssuesView* issues_{nullptr};
    InboxView* inbox_{nullptr};
    ViewsView* views_{nullptr};
    SettingsView* settings_{nullptr};
    std::unique_ptr<PomodoroBar> pomodoro_bar_;
    std::unique_ptr<PomodoroModal> pomodoro_modal_;
};

} // namespace cd
