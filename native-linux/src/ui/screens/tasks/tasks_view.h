#pragma once

#include "domain/models.h"
#include "quick_input_bar.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/listbox.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>

#include <set>

namespace cd {

class AppContainer;
class AppViewModel;
class SyncScheduler;
class SearchableFilterMenu;

/** Task list + quick input (nested subtasks rendered with indentation). */
class TasksView final : public Gtk::Box {
public:
    TasksView(AppContainer&, AppViewModel&, SyncScheduler&);

    void rebuild();
    void focus_quick_input();
    bool reveal_task(std::string const& uid);

private:
    enum class TaskListFilter { Active, Completed, All };

    AppContainer& app_;
    AppViewModel& vm_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    SearchableFilterMenu* status_filter_{};
    SearchableFilterMenu* tag_filter_{};
    Gtk::Button clear_filters_btn_{};
    TaskListFilter task_filter_{TaskListFilter::Active};
    std::set<std::string> selected_tags_{};
    Gtk::Button refresh_btn_{};
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    QuickInputBar input_;

    void on_quick_submit(Glib::ustring const&);
    void on_filter_changed();
    void on_row_activated(Gtk::ListBoxRow* row);
    bool task_matches_filter(CalDavTask const&, TaskListFilter);

    Gtk::ListBoxRow* make_row(
        CalDavTask const& task, int depth, std::vector<std::string> const& magic_tags);
};

} // namespace cd
