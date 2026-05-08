#include "memos_view.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "comment_on_issue_dialog.h"
#include "create_event_from_memo_dialog.h"
#include "create_memo_dialog.h"
#include "extract_tasks_dialog.h"
#include "memo_detail_view.h"
#include "data/db/memo_dao.h"

#include <gtk/gtk.h>
#include <algorithm>
#include <cctype>
#include <set>

namespace cd {

namespace {

bool contains_ci(std::string text, std::string needle)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : needle)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return needle.empty() || text.find(needle) != std::string::npos;
}

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

MemosView::MemosView(AppContainer& app, AppViewModel& vm)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8)
    , app_(app)
    , vm_(vm)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 8)
    , paned_(Gtk::ORIENTATION_HORIZONTAL)
{
    normal_btn_.set_active(true);
    normal_btn_.signal_toggled().connect([this] {
        if (normal_btn_.get_active()) {
            archived_btn_.set_active(false);
            refresh_visible_rows();
        }
    });
    archived_btn_.signal_toggled().connect([this] {
        if (archived_btn_.get_active()) {
            normal_btn_.set_active(false);
            refresh_visible_rows();
        }
    });
    search_.set_placeholder_text("Search capture...");
    search_.signal_changed().connect([this] { refresh_visible_rows(); });
    new_btn_.signal_clicked().connect([this] { open_create_dialog({}); });

    toolbar_.pack_start(normal_btn_, false, false);
    toolbar_.pack_start(archived_btn_, false, false);
    toolbar_.pack_start(search_, true, true);
    toolbar_.pack_start(new_btn_, false, false);
    pack_start(toolbar_, false, false);

    tags_.set_selection_mode(Gtk::SELECTION_NONE);
    tags_.set_max_children_per_line(10);
    pack_start(tags_, false, false);

    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    list_.signal_row_selected().connect([this](Gtk::ListBoxRow* row) {
        if (!row) {
            selected_name_.reset();
            detail_->set_memo(std::nullopt);
            return;
        }
        int idx = row->get_index();
        if (idx < 0 || static_cast<std::size_t>(idx) >= rows_.size())
            return;
        selected_name_ = rows_[static_cast<std::size_t>(idx)].name;
        detail_->set_memo(rows_[static_cast<std::size_t>(idx)]);
    });

    auto* detail = Gtk::manage(new MemoDetailView(app_));
    detail_ = detail;
    detail_->on_extract_tasks = [this] {
        if (!detail_->selected_memo().has_value())
            return;
        auto* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top)
            return;
        ExtractTasksDialog dlg(*top, app_, detail_->selected_memo()->content);
        if (dlg.run() == Gtk::RESPONSE_OK)
            dlg.create_checked_tasks();
    };
    detail_->on_create_event = [this] {
        if (!detail_->selected_memo().has_value())
            return;
        auto* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top)
            return;
        CreateEventFromMemoDialog dlg(*top, app_, detail_->selected_memo()->content);
        if (dlg.run() == Gtk::RESPONSE_OK)
            dlg.create_event();
    };
    detail_->on_comment_issue = [this] {
        if (!detail_->selected_memo().has_value())
            return;
        auto* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top)
            return;
        CommentOnIssueDialog dlg(*top, app_, detail_->selected_memo()->content);
        if (dlg.run() == Gtk::RESPONSE_OK)
            dlg.post_comment();
    };
    detail_->on_open_url = [this] {
        if (!detail_->selected_memo().has_value())
            return;
        auto url = detect_url(detail_->selected_memo()->content);
        if (!url.has_value())
            return;
        GError* error = nullptr;
        gtk_show_uri_on_window(nullptr, url->c_str(), GDK_CURRENT_TIME, &error);
        if (error)
            g_error_free(error);
    };
    detail_->on_copy_link = [this] {
        if (!detail_->selected_memo().has_value())
            return;
        auto base = app_.memos_client().base_url_opt();
        if (!base.has_value())
            return;
        std::string link = *base + "/" + detail_->selected_memo()->name;
        GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(cb, link.c_str(), link.size());
    };
    detail_->on_memo_changed = [this] { rebuild(); };

    auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    sc->add(list_);
    paned_.pack1(*sc, true, false);
    paned_.pack2(*detail, true, false);
    paned_.set_position(360);
    pack_start(paned_, true, true);

    vm_.signal_capture_initial_text.connect([this](std::string const& text) { open_create_dialog(text); });
    signal_map().connect(sigc::mem_fun(*this, &MemosView::on_map_cold_start));

    rebuild();
}

void MemosView::on_map_cold_start()
{
    std::string const pending = vm_.capture_initial_text();
    if (!pending.empty())
        open_create_dialog(pending);
}

void MemosView::rebuild()
{
    try {
        app_.memos_repository().sync_all();
    }
    catch (...) {
    }
    refresh_visible_rows();
}

void MemosView::focus_search()
{
    search_.grab_focus();
}

void MemosView::rebuild_tag_chips()
{
    for (Gtk::Widget* w : tags_.get_children())
        tags_.remove(*w);
    std::set<std::string> all_tags;
    for (auto const& m : rows_) {
        for (auto const& t : m.tags)
            all_tags.insert(t);
    }
    for (auto const& tag : all_tags) {
        auto* child = Gtk::manage(new Gtk::FlowBoxChild());
        auto* btn = Gtk::manage(new Gtk::ToggleButton(tag));
        btn->signal_toggled().connect([this, btn, tag] {
            if (btn->get_active()) {
                selected_tags_.push_back(tag);
            }
            else {
                selected_tags_.erase(std::remove(selected_tags_.begin(), selected_tags_.end(), tag), selected_tags_.end());
            }
            refresh_visible_rows();
        });
        child->add(*btn);
        tags_.add(*child);
    }
    tags_.show_all();
}

void MemosView::refresh_visible_rows()
{
    for (Gtk::Widget* w : list_.get_children())
        list_.remove(*w);
    rows_.clear();

    MemoState target = archived_btn_.get_active() ? MemoState::Archived : MemoState::Normal;
    std::string query = search_.get_text();
    MemoDao dao(app_.db());
    auto all = dao.get_by_state(target);
    for (auto const& memo : all) {
        bool tag_ok = true;
        for (auto const& tag : selected_tags_) {
            if (std::find(memo.tags.begin(), memo.tags.end(), tag) == memo.tags.end()) {
                tag_ok = false;
                break;
            }
        }
        if (!tag_ok)
            continue;
        if (!contains_ci(memo.content + " " + memo.snippet + " " + memo.property.title, query))
            continue;
        rows_.push_back(memo);
    }

    for (auto const& memo : rows_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow());
        auto* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        auto* title = Gtk::manage(new Gtk::Label(memo.property.title.empty() ? memo.name : memo.property.title));
        title->set_halign(Gtk::ALIGN_START);
        title->set_ellipsize(Pango::ELLIPSIZE_END);
        auto* badge = Gtk::manage(new Gtk::Image());
        std::string icon = "changes-prevent-symbolic";
        if (memo.visibility == MemoVisibility::Public)
            icon = "emblem-shared-symbolic";
        else if (memo.visibility == MemoVisibility::Protected)
            icon = "changes-allow-symbolic";
        badge->set_from_icon_name(icon, Gtk::ICON_SIZE_MENU);
        box->pack_start(*title, true, true);
        box->pack_start(*badge, false, false);
        row->add(*box);
        list_.append(*row);
    }
    list_.show_all();
    rebuild_tag_chips();

    if (selected_name_.has_value())
        select_by_name(*selected_name_);
    else
        detail_->set_memo(std::nullopt);
}

void MemosView::select_by_name(std::string const& name)
{
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].name == name) {
            Gtk::ListBoxRow* row = list_.get_row_at_index(static_cast<int>(i));
            if (row)
                list_.select_row(*row);
            detail_->set_memo(rows_[i]);
            return;
        }
    }
}

void MemosView::open_create_dialog(std::string initial_text)
{
    auto* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!top)
        return;
    CreateMemoDialog dialog(*top, app_, std::move(initial_text));
    if (dialog.run() != Gtk::RESPONSE_OK)
        return;
    std::string body = dialog.content();
    if (body.empty())
        return;
    auto created = app_.memos_repository().create_memo(body, dialog.visibility(), dialog.attachments());
    if (!created.has_value())
        return;
    selected_name_ = created->name;
    rebuild();
}

} // namespace cd
