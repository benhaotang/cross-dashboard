#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/listbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/searchentry.h>

#include <optional>
#include <set>
#include <vector>

#include "domain/models.h"

namespace cd {

class AppContainer;
class AppViewModel;
class MemoDetailView;
class SearchableFilterMenu;
class SyncScheduler;

class MemosView final : public Gtk::Box {
public:
    MemosView(AppContainer& app, AppViewModel& vm, SyncScheduler& sync);
    void rebuild();
    void focus_search();

private:
    void refresh_visible_rows();
    void select_by_name(std::string const& name);
    void open_create_dialog(std::string initial_text);
    void on_map_cold_start();

    AppContainer& app_;
    AppViewModel& vm_;
    SyncScheduler& sync_;
    Gtk::Box toolbar_;
    Gtk::Button refresh_btn_{};
    SearchableFilterMenu* status_filter_{};
    SearchableFilterMenu* tag_filter_{};
    Gtk::Button clear_filters_btn_{};
    Gtk::SearchEntry search_;
    Gtk::Button new_btn_{};
    Gtk::Paned paned_;
    Gtk::ListBox list_;
    MemoDetailView* detail_{};

    std::string selected_state_{"normal"};
    std::set<std::string> selected_tags_;
    std::vector<MemosMemo> rows_;
    std::optional<std::string> selected_name_;
};

} // namespace cd
