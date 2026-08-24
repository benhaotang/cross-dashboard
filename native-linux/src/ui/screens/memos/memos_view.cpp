#include "memos_view.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "comment_on_issue_dialog.h"
#include "create_event_from_memo_dialog.h"
#include "create_memo_dialog.h"
#include "extract_tasks_dialog.h"
#include "memo_detail_view.h"
#include "save_to_karakeep_dialog.h"
#include "background/sync_scheduler.h"
#include "components/searchable_filter_menu.h"
#include "components/tag_flow.h"
#include "data/db/memo_dao.h"
#include "data/prefs/prefs.h"
#include "domain/models.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gtkmm/messagedialog.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <regex>

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
    , paned_(Gtk::ORIENTATION_HORIZONTAL)
{
    status_filter_ = Gtk::manage(new SearchableFilterMenu("Status", false, false));
    status_filter_->set_options({{"normal", "Active captures"}, {"archived", "Archived captures"}, {"all", "All captures"}});
    status_filter_->set_selected({selected_state_});
    status_filter_->signal_selection_changed.connect([this](std::set<std::string> const& selected) {
        if (!selected.empty()) selected_state_ = *selected.begin();
        refresh_visible_rows();
    });
    tag_filter_ = Gtk::manage(new SearchableFilterMenu("Tags", true, true));
    tag_filter_->signal_selection_changed.connect([this](std::set<std::string> const& selected) {
        selected_tags_ = selected;
        refresh_visible_rows();
    });
    clear_filters_btn_.set_image_from_icon_name("edit-clear-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    clear_filters_btn_.set_tooltip_text("Clear capture filters");
    clear_filters_btn_.set_relief(Gtk::RELIEF_NONE);
    clear_filters_btn_.signal_clicked().connect([this] {
        selected_state_ = "normal";
        selected_tags_.clear();
        status_filter_->set_selected({selected_state_});
        tag_filter_->set_selected({});
        refresh_visible_rows();
    });
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(clear_filters_btn_.gobj())))
        atk_object_set_name(a, "Clear capture filters");
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

    // Toolbar: compact searchable filters, search, and actions.
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");

    toolbar_.pack_start(*status_filter_, false, false);
    toolbar_.pack_start(*tag_filter_, false, false);
    toolbar_.pack_start(clear_filters_btn_, false, false);
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
        for (auto const& url : detect_urls(detail_->selected_memo()->content)) {
            GError* error = nullptr;
            gtk_show_uri_on_window(nullptr, url.c_str(), GDK_CURRENT_TIME, &error);
            if (error) g_error_free(error);
        }
    };
    detail_->on_save_karakeep = [this] {
        if (!detail_->selected_memo().has_value()) return;
        auto* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (!top) return;
        SaveToKarakeepDialog dialog(*top, app_, detect_urls(detail_->selected_memo()->content));
        if (dialog.run() != Gtk::RESPONSE_OK) return;
        try {
            dialog.save();
        }
        catch (std::exception const& error) {
            Gtk::MessageDialog message(*top, error.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            message.run();
        }
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

void MemosView::refresh_visible_rows()
{
    for (Gtk::Widget* w : list_.get_children())
        list_.remove(*w);
    rows_.clear();

    std::string const query = search_.get_text();
    MemoDao dao(app_.db());
    auto const all = selected_state_ == "all"
        ? dao.get_all()
        : dao.get_by_state(selected_state_ == "archived" ? MemoState::Archived : MemoState::Normal);
    auto const magic_tags = planning_magic_tags(merged_app_preferences(app_.prefs()));

    std::set<std::string> all_tags;
    for (auto const& memo : dao.get_all())
        all_tags.insert(memo.tags.begin(), memo.tags.end());
    std::vector<std::pair<std::string, std::string>> tag_options;
    for (auto const& tag : all_tags) tag_options.emplace_back(tag, "#" + tag);
    tag_filter_->set_options(std::move(tag_options));
    tag_filter_->set_selected(selected_tags_);
    clear_filters_btn_.set_visible(selected_state_ != "normal" || !selected_tags_.empty());

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
