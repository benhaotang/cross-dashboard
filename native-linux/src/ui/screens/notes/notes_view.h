#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/stack.h>
#include <gtkmm/togglebutton.h>

#include <glibmm/ustring.h>

#include "domain/models.h"

#include <optional>
#include <vector>

namespace cd {

class AppContainer;
class ReadMarkdownField;
class SyncScheduler;

/** VJOURNAL list: grid/list toggle, search; create/edit via dialogs. */
class NotesView final : public Gtk::Box {
public:
    NotesView(AppContainer&, SyncScheduler&);

    void rebuild();
    void focus_search();

private:
    void apply_filter(Glib::ustring const& q);
    void show_detail(Note const&);
    void on_new_note();
    void on_edit_note();

    AppContainer& app_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_;
    Gtk::Button refresh_btn_{};
    Gtk::SearchEntry search_;
    Gtk::ToggleButton btn_grid_;
    Gtk::ToggleButton btn_list_;
    Gtk::Stack mode_;
    Gtk::FlowBox flow_;
    Gtk::ListBox list_;
    Gtk::ScrolledWindow detail_scroll_{};
    Gtk::Box detail_;
    ReadMarkdownField* body_field_{};

    std::vector<Note> notes_;
    std::optional<std::string> selected_uid_;
};

} // namespace cd
