#pragma once

#include "domain/models.h"

#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>

#include <optional>
#include <string>

namespace cd {

class AppContainer;

/** Kanban (per Android: category tags per column) + Covey quadrants (`kCoveyQuadrantTags`). */
class ViewsView final : public Gtk::Box {
public:
    explicit ViewsView(AppContainer&);

    void rebuild();

    void on_kanban_dropped(std::string const& task_uid, int column_code);
    /** `column_code` −1 = untagged, else index into configured Kanban column tags. */

    void on_covey_dropped(std::string const& task_uid, int quadrant_index);

private:
    void fill_kanban(std::vector<CalDavTask> const& open_tasks);
    void fill_covey(std::vector<CalDavTask> const& open_tasks);
    void apply_kanban_tag(std::string const& uid, std::optional<std::string> const& column_tag);
    /** `quadrant_index` -1: strip Covey tags only; 0–3: assign quadrant. */
    void apply_covey_quadrant(std::string const& uid, int quadrant_index);
    void run_assign_dialog(CalDavTask const& task, bool covey_mode);

    AppContainer& app_;
    Gtk::Notebook tabs_{};
    Gtk::ScrolledWindow kanban_scroll_{};
    Gtk::Box kanban_board_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Box covey_outer_{Gtk::ORIENTATION_VERTICAL, 6};
    Gtk::Box covey_unassigned_box_{Gtk::ORIENTATION_VERTICAL, 4};
    Gtk::Box covey_unassigned_slot_{Gtk::ORIENTATION_VERTICAL, 2};
    Gtk::Grid covey_grid_{};
};

} // namespace cd
