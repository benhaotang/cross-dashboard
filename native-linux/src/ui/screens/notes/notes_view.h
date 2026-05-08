#pragma once

#include <gtkmm/box.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/listbox.h>
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

/** VJOURNAL list: grid/list toggle, search; create/edit via dialogs. */
class NotesView final : public Gtk::Box {
public:
    explicit NotesView(AppContainer&);

    void rebuild();
    void focus_search();

private:
    void apply_filter(Glib::ustring const& q);
    void show_detail(Note const&);
    void on_new_note();
    void on_edit_note();

    AppContainer& app_;
    Gtk::Box toolbar_;
    Gtk::SearchEntry search_;
    Gtk::ToggleButton btn_grid_;
    Gtk::ToggleButton btn_list_;
    Gtk::Stack mode_;
    Gtk::FlowBox flow_;
    Gtk::ListBox list_;
    Gtk::Box detail_;
    ReadMarkdownField* body_field_{};

    std::vector<Note> notes_;
    std::optional<std::string> selected_uid_;
};

} // namespace cd
