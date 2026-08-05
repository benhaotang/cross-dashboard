#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include "domain/models.h"

#include <string>
#include <variant>
#include <vector>

namespace cd {

class AppContainer;
class SyncScheduler;

/** Unified events, tasks, issues + estimated time from `#Nm` / `#Nh` tags. */
class InboxView final : public Gtk::Box {
public:
    InboxView(AppContainer&, SyncScheduler&);

    void rebuild();

private:
    struct Row {
        std::variant<CalendarEvent, CalDavTask, GiteaIssue> data;
        std::string title;
        std::string subtitle;
        int estimated_minutes{};
    };

    void populate_rows(std::vector<Row>& out);
    void on_filter_changed();

    AppContainer& app_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Button refresh_btn_{};
    Gtk::ComboBoxText filter_combo_{};
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    Gtk::Label total_line_;
    std::vector<Row> rows_;
    int filter_index_{};
};

} // namespace cd
