#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include "domain/models.h"

#include <string>
#include <variant>
#include <vector>

namespace cd {

class AppContainer;
class SearchableFilterMenu;
class SyncScheduler;

/** Unified events, tasks, issues + estimated time from `#Nm` / `#Nh` tags. */
class InboxView final : public Gtk::Box {
public:
    InboxView(AppContainer&, SyncScheduler&);

    void rebuild();

    sigc::signal<void(std::string const&)> signal_event_requested;
    sigc::signal<void(std::string const&)> signal_task_requested;
    sigc::signal<void(std::int64_t)> signal_issue_requested;

private:
    struct Row {
        std::variant<CalendarEvent, CalDavTask, GiteaIssue> data;
        std::string title;
        std::string subtitle;
        int estimated_minutes{};
    };

    void populate_rows(std::vector<Row>& out);
    void on_filter_changed();
    void on_row_activated(Gtk::ListBoxRow* row);

    AppContainer& app_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Button refresh_btn_{};
    Gtk::Button snapshot_btn_{};
    SearchableFilterMenu* type_filter_{};
    SearchableFilterMenu* date_filter_{};
    Gtk::Button clear_filters_btn_{};
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    Gtk::Label total_line_;
    std::vector<Row> rows_;
    std::string type_filter_key_{"all"};
    std::string date_filter_key_{"all"};
};

} // namespace cd
