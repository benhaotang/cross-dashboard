#include "memo_detail_view.h"

#include "app_container.h"
#include "components/markdown_view.h"
#include "components/memo_auth_image.h"
#include "components/read_markdown_field.h"
#include "create_memo_dialog.h"

#include "data/repository/repositories.h"

#include <exception>
#include <algorithm>
#include <regex>
#include <vector>

#include <gtkmm/messagedialog.h>
#include <gtkmm/separator.h>
#include <gtkmm/window.h>

#include <gtk/gtk.h>

namespace cd {

namespace {

std::vector<std::string> detect_urls(std::string const& text)
{
    static std::regex const pattern(R"(https?://[^\s]+)");
    std::vector<std::string> urls;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
        std::string url = it->str();
        while (!url.empty() && std::string(".,;:!?)]}").find(url.back()) != std::string::npos) url.pop_back();
        if (!url.empty() && std::find(urls.begin(), urls.end(), url) == urls.end()) urls.push_back(url);
    }
    return urls;
}

} // namespace

namespace {
// Helper: configure a button as an icon-only flat action button
void setup_icon_btn(Gtk::Button& btn, const char* icon_name, const char* tooltip, const char* ax_name)
{
    btn.set_image_from_icon_name(icon_name, Gtk::ICON_SIZE_SMALL_TOOLBAR);
    btn.set_tooltip_text(tooltip);
    btn.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(btn.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(btn.gobj())))
        atk_object_set_name(a, ax_name);
}
} // anonymous namespace

MemoDetailView::MemoDetailView(AppContainer& app)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
    , app_(app)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 2)
    , title_("")
    , meta_("")
    , comments_title_("Comments")
{
    title_.set_halign(Gtk::ALIGN_START);
    title_.set_line_wrap(true);
    title_.set_selectable(true);
    meta_.set_halign(Gtk::ALIGN_START);
    title_.set_margin_top(8);
    meta_.set_margin_bottom(6);

    // Configure all action buttons as icon-only
    setup_icon_btn(extract_tasks_btn_, "emblem-default-symbolic",   "Extract tasks",       "Extract tasks");
    setup_icon_btn(create_event_btn_,  "appointment-new-symbolic",  "Create event",        "Create event");
    setup_icon_btn(comment_issue_btn_, "insert-text-symbolic",      "Comment on issue",    "Comment on issue");
    setup_icon_btn(open_url_btn_,      "web-browser-symbolic",      "Open URL",            "Open URL");
    setup_icon_btn(save_karakeep_btn_, "bookmark-new-symbolic",     "Save to Karakeep",     "Save links to Karakeep");
    setup_icon_btn(copy_link_btn_,     "edit-copy-symbolic",        "Copy link",           "Copy link");
    setup_icon_btn(edit_btn_,          "document-edit-symbolic",    "Edit memo",           "Edit memo");
    setup_icon_btn(archive_btn_,       "mail-mark-read-symbolic",   "Archive",             "Archive");
    setup_icon_btn(restore_btn_,       "document-revert-symbolic",  "Restore",             "Restore");
    setup_icon_btn(delete_btn_,        "user-trash-symbolic",       "Delete permanently",  "Delete permanently");
    setup_icon_btn(comment_send_btn_,  "mail-send-symbolic",        "Post comment",        "Post comment");

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
    save_karakeep_btn_.signal_clicked().connect([this] {
        if (on_save_karakeep) on_save_karakeep();
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

    // Toolbar layout: [create group | sep | link group | sep | memo ops group (right)]
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");

    // Group 1: content-creation actions
    toolbar_.pack_start(extract_tasks_btn_, false, false);
    toolbar_.pack_start(create_event_btn_,  false, false);
    toolbar_.pack_start(comment_issue_btn_, false, false);

    auto* sep1 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    sep1->set_margin_start(4);
    sep1->set_margin_end(4);
    toolbar_.pack_start(*sep1, false, false);

    // Group 2: link actions
    toolbar_.pack_start(open_url_btn_,  false, false);
    toolbar_.pack_start(save_karakeep_btn_, false, false);
    toolbar_.pack_start(copy_link_btn_, false, false);

    // Group 3: memo state actions — right aligned
    toolbar_.pack_end(delete_btn_,   false, false);
    toolbar_.pack_end(restore_btn_,  false, false);
    toolbar_.pack_end(archive_btn_,  false, false);

    auto* sep2 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    sep2->set_margin_start(4);
    sep2->set_margin_end(4);
    toolbar_.pack_end(*sep2, false, false);

    toolbar_.pack_end(edit_btn_, false, false);

    auto* md = Gtk::manage(new MarkdownView(MarkdownView::HeightMode::WithFollowingContent));
    markdown_ = md;
    content_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    content_column_.set_margin_start(8);
    content_column_.set_margin_end(8);
    content_column_.pack_start(title_, false, false);
    content_column_.pack_start(meta_, false, false);
    content_column_.pack_start(*md, false, false);
    content_column_.pack_start(attachments_, false, false);
    comments_title_.set_markup("<b>Comments</b>");
    comments_title_.set_halign(Gtk::ALIGN_START);
    content_column_.pack_start(comments_title_, false, false);
    content_column_.pack_start(comments_, false, false);
    content_scroll_.add(content_column_);

    comment_entry_.set_placeholder_text("Add comment…");
    composer_.pack_start(comment_entry_, true, true);
    composer_.pack_start(comment_send_btn_, false, false);
    composer_.set_margin_top(4);
    composer_.set_margin_bottom(4);
    composer_.set_margin_start(8);
    composer_.set_margin_end(8);

    pack_start(toolbar_, false, false);
    auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    pack_start(*sep, false, false);
    pack_start(content_scroll_, true, true);
    pack_start(composer_, false, false);
    signal_map().connect([this] { rebuild(); });
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
            auto* crm = Gtk::manage(new ReadMarkdownField(MarkdownView::HeightMode::Embedded));
            crm->set_field_label("");
            crm->set_markdown(c.content.empty() ? "_" : c.content);
            row->add(*crm);
            comments_.append(*row);
        }
    }
    catch (...) {
    }

    auto urls = detect_urls(memo.content);
    open_url_btn_.set_sensitive(!urls.empty());
    set_action_sensitivity();
    show_all_children();
    bool karakeep_configured = app_.secrets().get(CredentialKey::KARAKEEP_HOST).has_value()
        && app_.secrets().get(CredentialKey::KARAKEEP_TOKEN).has_value();
    save_karakeep_btn_.set_visible(!urls.empty() && karakeep_configured);
}

} // namespace cd
