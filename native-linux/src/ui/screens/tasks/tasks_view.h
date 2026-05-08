#pragma once

#include "domain/models.h"
#include "quick_input_bar.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

namespace cd {

class AppContainer;
class AppViewModel;
class SyncScheduler;

/** Task list + quick input (nested subtasks rendered with indentation). */
class TasksView final : public Gtk::Box {
public:
    TasksView(AppContainer&, AppViewModel&, SyncScheduler&);

    void rebuild();
    void focus_quick_input();

private:
    AppContainer& app_;
    AppViewModel& vm_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Button refresh_btn_{};
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    QuickInputBar input_;

    void on_quick_submit(Glib::ustring const&);

    Gtk::ListBoxRow* make_row(CalDavTask const& task, int depth);
};

} // namespace cd
