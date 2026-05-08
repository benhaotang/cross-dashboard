#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>

namespace cd {

class AppContainer;
class SyncScheduler;

/** Dashboard: weekly stats, activity bars, upcoming events, due-soon tasks, open issues (mirrors macOS structure). */
class DashboardView final : public Gtk::Box {
public:
    DashboardView(AppContainer&, SyncScheduler&);

    /** Refresh all cards from SQLite. */
    void refresh();

private:
    AppContainer& app_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Button refresh_btn_{};
    Gtk::ScrolledWindow scroll_{};
    Gtk::Box content_{Gtk::ORIENTATION_VERTICAL, 20};
};

} // namespace cd
