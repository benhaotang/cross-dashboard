#pragma once

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/listbox.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrolledwindow.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cd {

/** Searchable dropdown with single- or multi-select behavior. */
class SearchableFilterMenu final : public Gtk::MenuButton {
public:
    explicit SearchableFilterMenu(std::string title, bool multi_select = false, bool searchable = true);

    void set_options(std::vector<std::pair<std::string, std::string>> options);
    void set_selected(std::set<std::string> selected);
    [[nodiscard]] std::set<std::string> const& selected() const { return selected_; }

    sigc::signal<void(std::set<std::string> const&)> signal_selection_changed;

private:
    void rebuild_rows();
    void update_button_label();
    void apply_search();

    std::string title_;
    bool multi_select_{};
    bool searchable_{};
    bool rebuilding_{};
    std::vector<std::pair<std::string, std::string>> options_;
    std::set<std::string> selected_;
    Gtk::Popover popover_{};
    Gtk::Box content_{Gtk::ORIENTATION_VERTICAL, 6};
    Gtk::Entry search_{};
    Gtk::ScrolledWindow scroll_{};
    Gtk::ListBox list_{};
};

} // namespace cd
