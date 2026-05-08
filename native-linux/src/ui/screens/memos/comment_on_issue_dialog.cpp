#include "comment_on_issue_dialog.h"

#include "app_container.h"
#include "data/db/issue_dao.h"
#include "data/repository/repo_utils.h"

namespace cd {

CommentOnIssueDialog::CommentOnIssueDialog(Gtk::Window& parent, AppContainer& app, std::string memo_content)
    : Gtk::Dialog("Comment on issue", parent, true)
    , app_(app)
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Post", Gtk::RESPONSE_OK);

    auto repos = repos_from_cred_string(app_.secrets().get(CredentialKey::GITEA_REPOS));
    for (auto const& repo : repos)
        repo_combo_.append(repo);
    if (!repos.empty())
        repo_combo_.set_active(0);
    repo_combo_.signal_changed().connect(sigc::mem_fun(*this, &CommentOnIssueDialog::reload_issues));

    issue_list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    body_entry_.set_text(memo_content.substr(0, std::min<std::size_t>(memo_content.size(), 500)));

    Gtk::Box& box = *get_content_area();
    box.set_spacing(8);
    box.pack_start(repo_combo_, false, false);
    box.pack_start(issue_list_, true, true);
    box.pack_start(body_entry_, false, false);

    reload_issues();
    set_default_size(560, 420);
    show_all_children();
}

void CommentOnIssueDialog::reload_issues()
{
    for (Gtk::Widget* w : issue_list_.get_children())
        issue_list_.remove(*w);
    open_issues_.clear();

    std::string repo = repo_combo_.get_active_text();
    if (repo.empty())
        return;

    IssueDao dao(app_.db());
    auto all_open = dao.get_by_state("open");
    for (auto const& issue : all_open) {
        if (issue.repository != repo)
            continue;
        open_issues_.push_back(issue);
        auto* row = Gtk::manage(new Gtk::ListBoxRow());
        auto* label = Gtk::manage(new Gtk::Label("#" + std::to_string(issue.number) + " " + issue.title));
        label->set_halign(Gtk::ALIGN_START);
        row->add(*label);
        issue_list_.append(*row);
    }
    issue_list_.show_all();
}

bool CommentOnIssueDialog::post_comment()
{
    Gtk::ListBoxRow* row = issue_list_.get_selected_row();
    if (!row)
        return false;
    int idx = row->get_index();
    if (idx < 0 || static_cast<std::size_t>(idx) >= open_issues_.size())
        return false;
    std::string body = body_entry_.get_text();
    if (body.empty())
        return false;

    GiteaIssue const& issue = open_issues_[static_cast<std::size_t>(idx)];
    app_.issues().add_comment(issue.repository, issue.number, body);
    return true;
}

} // namespace cd
