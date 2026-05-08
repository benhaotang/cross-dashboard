#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include "domain/models.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include <string>
#include <variant>
#include <vector>

namespace cd {

class AppContainer;

/** Unified events, tasks, issues + estimated time from `#Nm` / `#Nh` tags. */
class InboxView final : public Gtk::Box {
public:
    explicit InboxView(AppContainer&);

    void rebuild();

private:
    struct Row {
        std::variant<CalendarEvent, CalDavTask, GiteaIssue> data;
        std::string title;
        std::string subtitle;
        int estimated_minutes{};
    };

    void populate_rows(std::vector<Row>& out);

    AppContainer& app_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    Gtk::Label total_line_;
    std::vector<Row> rows_;
};

} // namespace cd
