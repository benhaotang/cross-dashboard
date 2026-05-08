#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/listbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/togglebutton.h>

#include <optional>
#include <vector>

namespace cd {

class AppContainer;
class AppViewModel;
class MemoDetailView;

class MemosView final : public Gtk::Box {
public:
    MemosView(AppContainer& app, AppViewModel& vm);
    void rebuild();
    void focus_search();

private:
    void rebuild_tag_chips();
    void refresh_visible_rows();
    void select_by_name(std::string const& name);
    void open_create_dialog(std::string initial_text);
    void on_map_cold_start();

    AppContainer& app_;
    AppViewModel& vm_;
    Gtk::Box toolbar_;
    Gtk::ToggleButton normal_btn_{"Normal"};
    Gtk::ToggleButton archived_btn_{"Archived"};
    Gtk::SearchEntry search_;
    Gtk::Button new_btn_{"New capture"};
    Gtk::FlowBox tags_;
    Gtk::Paned paned_;
    Gtk::ListBox list_;
    MemoDetailView* detail_{};

    std::vector<std::string> selected_tags_;
    std::vector<struct MemosMemo> rows_;
    std::optional<std::string> selected_name_;
};

} // namespace cd
