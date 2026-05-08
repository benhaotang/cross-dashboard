#pragma once

#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/listbox.h>

#include <optional>
#include <vector>

namespace cd {

class AppContainer;
struct GiteaIssue;

class CommentOnIssueDialog final : public Gtk::Dialog {
public:
    CommentOnIssueDialog(Gtk::Window& parent, AppContainer& app, std::string memo_content);
    bool post_comment();

private:
    void reload_issues();

    AppContainer& app_;
    Gtk::ComboBoxText repo_combo_;
    Gtk::ListBox issue_list_;
    Gtk::Entry body_entry_;
    std::vector<GiteaIssue> open_issues_;
};

} // namespace cd
