#include "memos_view.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "comment_on_issue_dialog.h"
#include "create_event_from_memo_dialog.h"
#include "create_memo_dialog.h"
#include "extract_tasks_dialog.h"
#include "memo_detail_view.h"
#include "background/sync_scheduler.h"
#include "components/tag_flow.h"
#include "data/db/memo_dao.h"
#include "data/prefs/prefs.h"
#include "domain/models.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cctype>
#include <optional>
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

std::string memo_preview_text(MemosMemo const& m)
{
    std::string raw = !m.snippet.empty() ? m.snippet : m.content;
    std::string line;
    for (char c : raw) {
        if (c == '\n' || c == '\r') {
            if (!line.empty())
                break;
            continue;
        }
        if (c == '\t')
            c = ' ';
        line += c;
        if (line.size() >= 140)
            break;
    }
    while (!line.empty() && line.back() == ' ')
        line.pop_back();
    if (line.size() >= 140) {
        line.resize(137);
        line += "...";
    }
    if (line.empty())
        line = "(Empty capture)";
    return line;
}

std::string memo_created_str(EpochMillis ms)
{
    if (ms <= 0)
        return "";
    GDateTime* dt =
        g_date_time_new_from_unix_local(static_cast<gint64>(ms / 1000));
    if (!dt)
        return "";
    gchar* s = g_date_time_format(dt, "%b %e, %Y · %H:%M");
    g_date_time_unref(dt);
    if (!s)
        return "";
    std::string out{s};
    g_free(s);
    return out;
}

} // namespace

MemosView::MemosView(AppContainer& app, AppViewModel& vm, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8)
    , app_(app)
    , vm_(vm)
    , sync_(sync)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 8)
    , tag_bar_(Gtk::ORIENTATION_HORIZONTAL, 6)
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
    search_.set_placeholder_text("Search capture…");
    search_.signal_changed().connect([this] { refresh_visible_rows(); });

    // New capture: icon-only button
    new_btn_.set_label("");
    new_btn_.set_image_from_icon_name("list-add-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    // Match the working labelled New Issue button: GTK may globally hide
    // button images unless this property is explicitly enabled.
    new_btn_.set_always_show_image(true);
    new_btn_.set_tooltip_text("New capture");
    new_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(new_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(new_btn_.gobj())))
        atk_object_set_name(a, "New capture");
    new_btn_.signal_clicked().connect([this] { open_create_dialog({}); });

    // Toolbar: [Normal][Archived linked] | [search expanding] | [+]
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");

    auto* state_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(state_box->gobj())), "linked");
    state_box->pack_start(normal_btn_, false, false);
    state_box->pack_start(archived_btn_, false, false);
    toolbar_.pack_start(*state_box, false, false);

    toolbar_.pack_start(search_, true, true);
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh captures");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh captures");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(new_btn_, false, false);
    toolbar_.pack_end(refresh_btn_, false, false);
    pack_start(toolbar_, false, false);

    tags_.set_selection_mode(Gtk::SELECTION_NONE);
    tags_.set_max_children_per_line(18);
    tags_.set_hexpand(true);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(tags_.gobj())), "cd-memo-tags-flow");

    clear_tag_filters_btn_.set_relief(Gtk::RELIEF_NONE);
    clear_tag_filters_btn_.set_image_from_icon_name("edit-clear-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    clear_tag_filters_btn_.set_tooltip_text("Clear tag filters");
    clear_tag_filters_btn_.set_visible(false);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(GTK_WIDGET(clear_tag_filters_btn_.gobj())), "cd-icon-btn");
    clear_tag_filters_btn_.signal_clicked().connect([this] {
        if (selected_tags_.empty())
            return;
        selected_tags_.clear();
        refresh_visible_rows();
    });
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(clear_tag_filters_btn_.gobj())))
        atk_object_set_name(a, "Clear tag filters");

    tag_bar_.pack_start(tags_, true, true);
    tag_bar_.pack_end(clear_tag_filters_btn_, false, false);
    pack_start(tag_bar_, false, false);

    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(list_.gobj())), "cd-card-list");
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

void MemosView::sync_clear_tag_filters_ui()
{
    bool const any = !selected_tags_.empty();
    clear_tag_filters_btn_.set_visible(any);
    clear_tag_filters_btn_.set_sensitive(any);
}

void MemosView::rebuild_tag_chips(std::vector<MemosMemo> const& memos_for_tag_universe)
{
    struct Lock {
        bool& ref;
        explicit Lock(bool& b)
            : ref(b)
        {
            ref = true;
        }
        ~Lock() { ref = false; }
    } guard(updating_tag_chips_);
    auto const magic_tags = planning_magic_tags(merged_app_preferences(app_.prefs()));

    for (Gtk::Widget* w : tags_.get_children())
        tags_.remove(*w);

    std::set<std::string> all_tags;
    for (auto const& m : memos_for_tag_universe) {
        for (auto const& t : m.tags)
            all_tags.insert(t);
    }

    selected_tags_.erase(
        std::remove_if(selected_tags_.begin(), selected_tags_.end(),
            [&all_tags](std::string const& t) { return all_tags.find(t) == all_tags.end(); }),
        selected_tags_.end());

    for (auto const& tag : all_tags) {
        auto* child = Gtk::manage(new Gtk::FlowBoxChild());
        auto* btn = Gtk::manage(new Gtk::ToggleButton("#" + tag));
        auto* context = gtk_widget_get_style_context(GTK_WIDGET(btn->gobj()));
        gtk_style_context_add_class(context, "cd-memo-tag-chip");
        switch (classify_tag(tag, magic_tags)) {
        case TagKind::Time:
            gtk_style_context_add_class(context, "cd-memo-tag-chip-time");
            break;
        case TagKind::Magic:
            gtk_style_context_add_class(context, "cd-memo-tag-chip-magic");
            break;
        case TagKind::Neutral:
            gtk_style_context_add_class(context, "cd-memo-tag-chip-neutral");
            break;
        }
        bool const sel =
            std::find(selected_tags_.begin(), selected_tags_.end(), tag) != selected_tags_.end();
        btn->set_active(sel);
        btn->signal_toggled().connect([this, btn, tag] {
            if (updating_tag_chips_)
                return;
            if (btn->get_active()) {
                if (std::find(selected_tags_.begin(), selected_tags_.end(), tag) == selected_tags_.end())
                    selected_tags_.push_back(tag);
            }
            else {
                selected_tags_.erase(
                    std::remove(selected_tags_.begin(), selected_tags_.end(), tag), selected_tags_.end());
            }
            sync_clear_tag_filters_ui();
            refresh_visible_rows();
        });
        child->add(*btn);
        tags_.add(*child);
    }
    tags_.show_all();
    sync_clear_tag_filters_ui();
}

void MemosView::refresh_visible_rows()
{
    for (Gtk::Widget* w : list_.get_children())
        list_.remove(*w);
    rows_.clear();

    MemoState const target = archived_btn_.get_active() ? MemoState::Archived : MemoState::Normal;
    std::string const query = search_.get_text();
    MemoDao dao(app_.db());
    auto const all = dao.get_by_state(target);
    auto const magic_tags = planning_magic_tags(merged_app_preferences(app_.prefs()));

    std::vector<MemosMemo> candidates;
    candidates.reserve(all.size());
    for (auto const& memo : all) {
        if (!contains_ci(memo.content + " " + memo.snippet + " " + memo.property.title, query))
            continue;
        candidates.push_back(memo);
    }

    for (auto const& memo : candidates) {
        bool tag_ok = true;
        for (auto const& tag : selected_tags_) {
            if (std::find(memo.tags.begin(), memo.tags.end(), tag) == memo.tags.end()) {
                tag_ok = false;
                break;
            }
        }
        if (!tag_ok)
            continue;
        rows_.push_back(memo);
    }

    for (auto const& memo : rows_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow());
        auto* card = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(card->gobj())), "cd-list-card");

        auto* text_col = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        std::string const prev = memo_preview_text(memo);
        gchar* ep = g_markup_escape_text(prev.c_str(), -1);
        auto* prev_lab = Gtk::manage(new Gtk::Label());
        prev_lab->set_halign(Gtk::ALIGN_START);
        prev_lab->set_line_wrap(true);
        prev_lab->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
        prev_lab->set_markup(std::string(ep));
        g_free(ep);

        auto* date_lab = Gtk::manage(new Gtk::Label());
        date_lab->set_halign(Gtk::ALIGN_START);
        std::string const ds = memo_created_str(memo.create_time);
        if (!ds.empty()) {
            gchar* ed = g_markup_escape_text(ds.c_str(), -1);
            date_lab->set_markup(std::string("<small><span alpha=\"55%\">") + ed + "</span></small>");
            g_free(ed);
        }

        text_col->pack_start(*prev_lab, false, false);
        if (!memo.tags.empty())
            text_col->pack_start(*make_tag_flow(memo.tags, magic_tags), false, false);
        if (!ds.empty())
            text_col->pack_start(*date_lab, false, false);

        auto* badge = Gtk::manage(new Gtk::Image());
        std::string icon = "changes-prevent-symbolic";
        if (memo.visibility == MemoVisibility::Public)
            icon = "emblem-shared-symbolic";
        else if (memo.visibility == MemoVisibility::Protected)
            icon = "changes-allow-symbolic";
        badge->set_from_icon_name(icon, Gtk::ICON_SIZE_MENU);

        card->pack_start(*text_col, true, true);
        card->pack_start(*badge, false, false);
        row->add(*card);
        row->set_tooltip_text(prev + (ds.empty() ? "" : "\n" + ds));
        list_.append(*row);
    }
    list_.show_all();
    rebuild_tag_chips(candidates);

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
