#pragma once

#include "domain/models.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/listbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cd {

class AppContainer;
class ReadMarkdownField;

/** Issue list, detail, Gitea comments, issue/comment attachments, create-issue dialog. */
class IssuesView final : public Gtk::Box {
public:
    explicit IssuesView(AppContainer&);

    void rebuild();

private:
    void on_filter_changed();
    void on_selection_changed();
    void clear_detail_panels();
    void load_detail_from_network();
    void on_new_issue();
    void on_send_comment();
    void on_attach_with_comment();
    void on_toggle_issue_state();

    AppContainer& app_;

    Gtk::Box toolbar_;
    Gtk::ComboBoxText state_combo_;
    Gtk::Button new_issue_btn_;
    Gtk::Button toggle_state_btn_;

    Gtk::Paned paned_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;

    Gtk::ScrolledWindow detail_scroll_;
    Gtk::Box detail_inner_;
    Gtk::Label detail_meta_;
    ReadMarkdownField* body_{};
    Gtk::Label att_heading_;
    Gtk::Box attachments_box_;
    Gtk::Label com_heading_;
    Gtk::ListBox comments_list_;
    Gtk::Box composer_;
    Gtk::Entry comment_entry_;
    Gtk::Button attach_file_btn_;
    Gtk::Button send_comment_btn_;

    std::string state_filter_{"open"};
    std::vector<std::int64_t> list_ids_;
    std::optional<GiteaIssue> selected_issue_;
};

} // namespace cd
