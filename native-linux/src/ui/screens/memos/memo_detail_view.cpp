#include "memo_detail_view.h"

#include "app_container.h"
#include "components/markdown_view.h"
#include "components/memo_auth_image.h"
#include "create_memo_dialog.h"

#include "data/repository/repositories.h"

#include <exception>

#include <gtkmm/messagedialog.h>
#include <gtkmm/window.h>

#include <gtk/gtk.h>

namespace cd {

namespace {

std::optional<std::string> detect_url(std::string const& text)
{
    auto p = text.find("http://");
    if (p == std::string::npos)
        p = text.find("https://");
    if (p == std::string::npos)
        return std::nullopt;
    auto end = text.find_first_of(" \n\t", p);
    return text.substr(p, end == std::string::npos ? std::string::npos : end - p);
}

} // namespace

MemoDetailView::MemoDetailView(AppContainer& app)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8)
    , app_(app)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 6)
    , title_("")
    , meta_("")
    , comments_title_("Comments")
{
    title_.set_halign(Gtk::ALIGN_START);
    title_.set_line_wrap(true);
    title_.set_selectable(true);
    meta_.set_halign(Gtk::ALIGN_START);

    extract_tasks_btn_.signal_clicked().connect([this] {
        if (on_extract_tasks) on_extract_tasks();
    });
    create_event_btn_.signal_clicked().connect([this] {
        if (on_create_event) on_create_event();
    });
    comment_issue_btn_.signal_clicked().connect([this] {
        if (on_comment_issue) on_comment_issue();
    });
    open_url_btn_.signal_clicked().connect([this] {
        if (on_open_url) on_open_url();
    });
    copy_link_btn_.signal_clicked().connect([this] {
        if (on_copy_link) on_copy_link();
    });

    edit_btn_.signal_clicked().connect([this] {
        if (!memo_.has_value()) return;
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top) return;
        CreateMemoDialog dlg(*top, app_, *memo_);
        if (dlg.run() != Gtk::RESPONSE_OK) return;
        std::string body = dlg.content();
        if (body.empty()) return;
        try {
            (void)app_.memos_repository().update_memo_content(memo_->name, body, dlg.visibility());
            if (on_memo_changed) on_memo_changed();
            set_memo(app_.memos_client().get_memo(memo_->name));
        }
        catch (std::exception const& err) {
            if (top) {
                Gtk::MessageDialog m(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                m.run();
            }
        }
    });

    archive_btn_.signal_clicked().connect([this] {
        if (!memo_.has_value()) return;
        try {
            (void)app_.memos_repository().archive_memo(memo_->name);
            if (on_memo_changed) on_memo_changed();
            set_memo(app_.memos_client().get_memo(memo_->name));
        }
        catch (std::exception const& err) {
            if (Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel())) {
                Gtk::MessageDialog m(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                m.run();
            }
        }
    });

    restore_btn_.signal_clicked().connect([this] {
        if (!memo_.has_value()) return;
        try {
            (void)app_.memos_repository().restore_memo(memo_->name);
            if (on_memo_changed) on_memo_changed();
            set_memo(app_.memos_client().get_memo(memo_->name));
        }
        catch (std::exception const& err) {
            if (Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel())) {
                Gtk::MessageDialog m(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                m.run();
            }
        }
    });

    delete_btn_.signal_clicked().connect([this] {
        if (!memo_.has_value()) return;
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top) return;
        Gtk::MessageDialog confirm(*top, "Permanently delete this memo?", false, Gtk::MESSAGE_QUESTION,
            Gtk::BUTTONS_YES_NO);
        if (confirm.run() != Gtk::RESPONSE_YES) return;
        try {
            app_.memos_repository().delete_memo(memo_->name, true);
            set_memo(std::nullopt);
            if (on_memo_changed) on_memo_changed();
        }
        catch (std::exception const& err) {
            Gtk::MessageDialog m(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            m.run();
        }
    });

    comment_send_btn_.signal_clicked().connect([this] {
        if (!memo_.has_value()) return;
        Glib::ustring t = comment_entry_.get_text();
        if (t.empty()) return;
        try {
            app_.memos_repository().add_memo_comment(memo_->name, t.raw(), memo_->visibility);
            comment_entry_.set_text("");
            rebuild();
            if (on_memo_changed) on_memo_changed();
        }
        catch (std::exception const& err) {
            if (Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel())) {
                Gtk::MessageDialog m(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                m.run();
            }
        }
    });

    toolbar_.pack_start(extract_tasks_btn_, false, false);
    toolbar_.pack_start(create_event_btn_, false, false);
    toolbar_.pack_start(comment_issue_btn_, false, false);
    toolbar_.pack_start(open_url_btn_, false, false);
    toolbar_.pack_start(copy_link_btn_, false, false);
    toolbar_.pack_start(edit_btn_, false, false);
    toolbar_.pack_start(archive_btn_, false, false);
    toolbar_.pack_start(restore_btn_, false, false);
    toolbar_.pack_start(delete_btn_, false, false);

    auto* md = Gtk::manage(new MarkdownView());
    markdown_ = md;
    body_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    body_scroll_.set_min_content_height(180);
    body_scroll_.add(*md);

    attachments_.set_selection_mode(Gtk::SELECTION_NONE);
    attachments_.set_max_children_per_line(5);
    comments_.set_selection_mode(Gtk::SELECTION_NONE);

    comment_entry_.set_placeholder_text("Add comment…");
    composer_.pack_start(comment_entry_, true, true);
    composer_.pack_start(comment_send_btn_, false, false);

    pack_start(toolbar_, false, false);
    pack_start(title_, false, false);
    pack_start(meta_, false, false);
    pack_start(body_scroll_, false, false);
    pack_start(attachments_, false, false);
    pack_start(comments_title_, false, false);
    pack_start(comments_, true, true);
    pack_start(composer_, false, false);
}

void MemoDetailView::set_memo(std::optional<MemosMemo> memo)
{
    memo_ = std::move(memo);
    rebuild();
}

void MemoDetailView::set_action_sensitivity()
{
    bool has = memo_.has_value();
    edit_btn_.set_sensitive(has);
    archive_btn_.set_sensitive(has && memo_->state == MemoState::Normal);
    restore_btn_.set_sensitive(has && memo_->state == MemoState::Archived);
    delete_btn_.set_sensitive(has);
    comment_entry_.set_sensitive(has);
    comment_send_btn_.set_sensitive(has);
}

void MemoDetailView::rebuild()
{
    for (Gtk::Widget* w : attachments_.get_children())
        attachments_.remove(*w);
    for (Gtk::Widget* w : comments_.get_children())
        comments_.remove(*w);

    if (!memo_.has_value()) {
        title_.set_text("No memo selected");
        meta_.set_text("");
        if (markdown_)
            markdown_->load_markdown("");
        set_action_sensitivity();
        return;
    }

    MemosMemo const& memo = *memo_;
    title_.set_text(memo.property.title.empty() ? memo.name : memo.property.title);
    meta_.set_text(std::string(memo_state_name(memo.state)) + " · " + memo_visibility_name(memo.visibility));
    if (markdown_)
        markdown_->load_markdown(memo.content.empty() ? "_No content_" : memo.content);

    for (auto const& att : memo.attachments) {
        bool is_image = att.type.find("image/") == 0;
        if (is_image) {
            auto* child = Gtk::manage(new Gtk::FlowBoxChild());
            child->add(*Gtk::manage(new MemoAuthImage(app_, att.name, att.filename)));
            attachments_.add(*child);
        }
        else {
            auto* child = Gtk::manage(new Gtk::FlowBoxChild());
            auto* button = Gtk::manage(new Gtk::Button(att.filename.empty() ? att.name : att.filename));
            std::string base = app_.memos_client().base_url_opt().value_or("");
            std::string url = base.empty() ? std::string{} : base + "/file/" + att.name + "/" + att.filename;
            button->signal_clicked().connect([url] {
                if (url.empty()) return;
                GError* error = nullptr;
                gtk_show_uri_on_window(nullptr, url.c_str(), GDK_CURRENT_TIME, &error);
                if (error) g_error_free(error);
            });
            child->add(*button);
            attachments_.add(*child);
        }
    }

    try {
        auto comment_list = app_.memos_client().list_memo_comments(memo.name);
        for (auto const& c : comment_list) {
            auto* row = Gtk::manage(new Gtk::ListBoxRow());
            auto* label = Gtk::manage(new Gtk::Label(c.content));
            label->set_halign(Gtk::ALIGN_START);
            label->set_line_wrap(true);
            row->add(*label);
            comments_.append(*row);
        }
    }
    catch (...) {
    }

    open_url_btn_.set_sensitive(detect_url(memo.content).has_value());
    set_action_sensitivity();
    show_all_children();
}

} // namespace cd
