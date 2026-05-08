#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>

namespace cd {

class AppContainer;

/** Summary dashboard (local DB snapshots — sync in Phase 4 background). */
class DashboardView final : public Gtk::Box {
public:
    explicit DashboardView(AppContainer&);

    /** Refresh counters and lists from SQLite. */
    void refresh();

private:
    AppContainer& app_;
    Gtk::Label headline_;
};

} // namespace cd
