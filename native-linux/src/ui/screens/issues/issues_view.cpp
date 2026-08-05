#include "issues_view.h"

#include "app_container.h"
#include "background/sync_scheduler.h"
#include "components/attachment_row.h"
#include "components/read_markdown_field.h"
#include "components/tag_flow.h"
#include "data/db/issue_dao.h"
#include "data/prefs/prefs.h"
#include "data/repository/repo_utils.h"

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <fstream>
#include <iterator>
#include <algorithm>
#include <set>
#include <sstream>

#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/separator.h>
#include <gtkmm/textview.h>

namespace {

std::string file_basename(std::string const& path)
{
    auto p = path.find_last_of('/');
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
}

std::vector<std::uint8_t> read_file_bytes(std::string const& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

std::string join_labels(std::vector<std::string> const& labels)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i) out << ", ";
        out << labels[i];
    }
    return out.str();
}

std::vector<std::string> split_labels(std::string const& value)
{
    std::vector<std::string> labels;
    std::set<std::string> seen;
    std::stringstream input(value);
    std::string label;
    while (std::getline(input, label, ',')) {
        auto const first = label.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        auto const last = label.find_last_not_of(" \t\r\n");
        label = label.substr(first, last - first + 1);
        if (seen.insert(label).second) labels.push_back(label);
    }
    return labels;
}

} // namespace

namespace cd {

IssuesView::IssuesView(AppContainer& app, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , app_(app)
    , sync_(sync)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 8)
    , paned_(Gtk::ORIENTATION_HORIZONTAL)
    , scroll_{}
    , list_{}
    , detail_outer_(Gtk::ORIENTATION_VERTICAL, 0)
    , detail_scroll_{}
    , detail_inner_(Gtk::ORIENTATION_VERTICAL, 8)
    , detail_meta_("")
    , att_heading_("")
    , attachments_box_(Gtk::ORIENTATION_VERTICAL, 4)
    , com_heading_("")
    , comments_list_{}
    , composer_(Gtk::ORIENTATION_HORIZONTAL, 6)
    , comment_entry_{}
    , attach_file_btn_("Attach…")
    , send_comment_btn_("Comment")
{
    state_combo_.append("open");
    state_combo_.append("closed");
    state_combo_.set_active(0);
    state_combo_.signal_changed().connect(sigc::mem_fun(*this, &IssuesView::on_filter_changed));
    label_combo_.append("All labels");
    label_combo_.set_active(0);
    label_combo_.signal_changed().connect(sigc::mem_fun(*this, &IssuesView::on_filter_changed));

    // New issue: icon + label button
    new_issue_btn_.set_image_from_icon_name("list-add-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    new_issue_btn_.set_label("New issue");
    new_issue_btn_.set_always_show_image(true);
    new_issue_btn_.signal_clicked().connect(sigc::mem_fun(*this, &IssuesView::on_new_issue));
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(new_issue_btn_.gobj())))
        atk_object_set_name(a, "New issue");

    toggle_state_btn_.set_label("Close issue");
    toggle_state_btn_.set_sensitive(false);
    toggle_state_btn_.signal_clicked().connect(sigc::mem_fun(*this, &IssuesView::on_toggle_issue_state));
    edit_labels_btn_.signal_clicked().connect(sigc::mem_fun(*this, &IssuesView::on_edit_labels));
    edit_labels_btn_.set_sensitive(false);
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(edit_labels_btn_.gobj())))
        atk_object_set_name(a, "Edit issue labels");

    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");
    toolbar_.pack_start(state_combo_, false, false);
    toolbar_.pack_start(label_combo_, false, false);
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh issues");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh issues");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(new_issue_btn_, false, false);
    toolbar_.pack_end(refresh_btn_, false, false);
    pack_start(toolbar_, false, false);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(list_.gobj())), "cd-card-list");
    scroll_.add(list_);

    detail_meta_.set_halign(Gtk::ALIGN_START);
    detail_meta_.set_line_wrap(true);
    detail_meta_.set_selectable(true);

    auto* rm = Gtk::manage(new ReadMarkdownField(MarkdownView::HeightMode::WithFollowingContent));
    body_ = rm;
    rm->set_field_label("Description");
    rm->set_markdown("");

    att_heading_.set_markup("<b>Attachments</b>");
    att_heading_.set_halign(Gtk::ALIGN_START);

    com_heading_.set_markup("<b>Comments</b>");
    com_heading_.set_halign(Gtk::ALIGN_START);

    comments_list_.set_selection_mode(Gtk::SELECTION_NONE);
    comment_entry_.set_placeholder_text("Write a comment…");

    attach_file_btn_.set_image_from_icon_name("mail-attachment-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    attach_file_btn_.set_label("Attach…");
    attach_file_btn_.set_always_show_image(true);
    attach_file_btn_.signal_clicked().connect(sigc::mem_fun(*this, &IssuesView::on_attach_with_comment));

    send_comment_btn_.set_image_from_icon_name("mail-send-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    send_comment_btn_.set_label("Comment");
    send_comment_btn_.set_always_show_image(true);
    send_comment_btn_.signal_clicked().connect(sigc::mem_fun(*this, &IssuesView::on_send_comment));

    composer_.pack_start(comment_entry_, true, true);
    composer_.pack_start(attach_file_btn_, false, false);
    composer_.pack_start(send_comment_btn_, false, false);

    // Detail header row: meta + toggle-state action right-aligned
    auto* detail_hdr = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(detail_hdr->gobj())), "cd-toolbar");
    toggle_state_btn_.set_image_from_icon_name("emblem-default-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    toggle_state_btn_.set_always_show_image(true);
    detail_hdr->pack_start(detail_meta_, true, true);
    detail_hdr->pack_end(toggle_state_btn_, false, false);
    detail_hdr->pack_end(edit_labels_btn_, false, false);

    detail_inner_.pack_start(*detail_hdr, false, false);
    auto* detail_sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    detail_inner_.pack_start(*detail_sep, false, false);
    detail_inner_.pack_start(*rm, false, false);
    detail_inner_.pack_start(att_heading_, false, false);
    detail_inner_.pack_start(attachments_box_, false, false);
    detail_inner_.pack_start(com_heading_, false, false);
    detail_inner_.pack_start(comments_list_, false, false);

    detail_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    detail_scroll_.add(detail_inner_);

    detail_outer_.pack_start(detail_scroll_, true, true);
    detail_outer_.pack_start(composer_, false, false);

    paned_.pack1(scroll_, true, false);
    paned_.pack2(detail_outer_, true, false);
    paned_.set_position(340);
    pack_start(paned_, true, true);

    list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_selection_changed(); });

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(list_.gobj())), "Issue list");

    rebuild();
}

void IssuesView::on_filter_changed()
{
    if (rebuilding_) return;
    int a = state_combo_.get_active_row_number();
    state_filter_ = a == 1 ? "closed" : "open";
    auto selected = label_combo_.get_active_text();
    label_filter_ = selected == "All labels" ? "" : selected.raw();
    rebuild();
}

void IssuesView::clear_detail_panels()
{
    for (Gtk::Widget* w : attachments_box_.get_children()) attachments_box_.remove(*w);
    for (Gtk::Widget* w : comments_list_.get_children()) comments_list_.remove(*w);
    detail_meta_.set_text("");
    if (body_) body_->set_markdown("");
    toggle_state_btn_.set_sensitive(false);
    edit_labels_btn_.set_sensitive(false);
}

void IssuesView::rebuild()
{
    selected_issue_.reset();
    clear_detail_panels();

    for (Gtk::Widget* rw : list_.get_children()) list_.remove(*rw);
    list_ids_.clear();

    IssueDao dao(app_.db());
    auto issues = dao.get_by_state(state_filter_);
    auto const magic_tags = planning_magic_tags(merged_app_preferences(app_.prefs()));

    std::set<std::string> sorted_labels;
    for (auto const& issue : dao.get_all())
        sorted_labels.insert(issue.labels.begin(), issue.labels.end());
    rebuilding_ = true;
    label_combo_.remove_all();
    label_combo_.append("All labels");
    int active_label = 0;
    int label_index = 1;
    for (auto const& label : sorted_labels) {
        label_combo_.append(label);
        if (label == label_filter_) active_label = label_index;
        ++label_index;
    }
    if (active_label == 0) label_filter_.clear();
    label_combo_.set_active(active_label);
    rebuilding_ = false;

    if (!label_filter_.empty()) {
        issues.erase(std::remove_if(issues.begin(), issues.end(), [this](GiteaIssue const& issue) {
            return std::find(issue.labels.begin(), issue.labels.end(), label_filter_) == issue.labels.end();
        }), issues.end());
    }

    std::stable_sort(issues.begin(), issues.end(),
        [](auto const& a, auto const& b) { return a.updated_at > b.updated_at; });

    for (auto const& iss : issues) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow());
        auto* card = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(card->gobj())), "cd-list-card");

        auto* tlab = Gtk::manage(new Gtk::Label());
        tlab->set_halign(Gtk::ALIGN_START);
        tlab->set_line_wrap(true);
        tlab->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
        gchar* et = g_markup_escape_text(iss.title.c_str(), -1);
        tlab->set_markup(std::string("<b><span size=\"large\">") + et + "</span></b>");
        g_free(et);

        std::string const meta = "#" + std::to_string(iss.number) + " · " + iss.repository;
        auto* mlab = Gtk::manage(new Gtk::Label());
        mlab->set_halign(Gtk::ALIGN_START);
        gchar* em = g_markup_escape_text(meta.c_str(), -1);
        mlab->set_markup(std::string("<small><span alpha=\"55%\">") + em + "</span></small>");
        g_free(em);

        card->pack_start(*tlab, false, false);
        card->pack_start(*mlab, false, false);
        if (!iss.labels.empty()) {
            card->pack_start(*make_tag_flow(iss.labels, magic_tags), false, false);
        }
        row->add(*card);
        row->set_tooltip_text(iss.title + "\n" + meta);
        list_.append(*row);
        list_ids_.push_back(iss.id);
    }

    list_.show_all();
}

void IssuesView::load_detail_from_network()
{
    if (!selected_issue_.has_value() || !body_) return;

    GiteaIssue const& iss = *selected_issue_;
    detail_meta_.set_text(iss.repository + " #" + std::to_string(iss.number) + " · " + iss.state);
    if (iss.state == "open")
        toggle_state_btn_.set_label("Close issue");
    else
        toggle_state_btn_.set_label("Reopen issue");
    toggle_state_btn_.set_sensitive(true);
    edit_labels_btn_.set_sensitive(true);
    body_->set_markdown(iss.body.empty() ? "_No description_" : iss.body);

    for (Gtk::Widget* w : attachments_box_.get_children()) attachments_box_.remove(*w);
    for (Gtk::Widget* w : comments_list_.get_children()) comments_list_.remove(*w);

    try {
        auto iatts = app_.gitea().fetch_issue_attachments(iss.repository, iss.number);
        for (auto const& a : iatts) attachments_box_.pack_start(*Gtk::manage(new AttachmentRow(a)), false, false);

        auto comments = app_.issues().fetch_comments(iss.repository, iss.number);

        for (auto const& c : comments) {
            auto* brow = Gtk::manage(new Gtk::ListBoxRow);
            auto* vb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
            auto* who = Gtk::manage(new Gtk::Label());
            gchar* uesc = g_markup_escape_text(c.user.c_str(), -1);
            who->set_markup(std::string("<b>") + (uesc ? uesc : "") + "</b>");
            if (uesc) g_free(uesc);
            who->set_halign(Gtk::ALIGN_START);

            vb->pack_start(*who, false, false);

            auto* crm = Gtk::manage(new ReadMarkdownField(MarkdownView::HeightMode::Embedded));
            crm->set_field_label("");
            crm->set_markdown(c.body.empty() ? "_" : c.body);
            vb->pack_start(*crm, false, false);

            auto cats = app_.gitea().fetch_comment_attachments(iss.repository, c.id);
            for (auto const& a : cats) vb->pack_start(*Gtk::manage(new AttachmentRow(a)), false, false);

            brow->add(*vb);
            comments_list_.append(*brow);
        }
    }
    catch (std::exception const& err) {
        auto* er = Gtk::manage(new Gtk::Label(err.what()));
        er->set_line_wrap(true);
        attachments_box_.pack_start(*er, false, false);
    }

    attachments_box_.show_all();
    comments_list_.show_all();
}

void IssuesView::on_selection_changed()
{
    clear_detail_panels();

    auto* row = list_.get_selected_row();
    if (!row || !body_) {
        selected_issue_.reset();
        return;
    }

    int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row->gobj()));
    if (idx < 0 || static_cast<std::size_t>(idx) >= list_ids_.size()) return;

    IssueDao dao(app_.db());
    auto iss = dao.get_by_id(list_ids_[static_cast<std::size_t>(idx)]);
    if (!iss.has_value()) return;

    selected_issue_ = *iss;
    load_detail_from_network();
}

void IssuesView::on_new_issue()
{
    auto repos = repos_from_cred_string(app_.secrets().get(CredentialKey::GITEA_REPOS));
    if (repos.empty()) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog dlg(*top, "Configure GITEA_REPOS (JSON array or comma-separated).", false,
                Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        return;
    }

    Gtk::Dialog dlg;
    dlg.set_title("New issue");
    dlg.set_modal(true);
    dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("_Create", Gtk::RESPONSE_OK);

    auto* repo_combo = Gtk::manage(new Gtk::ComboBoxText());
    for (auto const& r : repos) repo_combo->append(r);

    auto* title_e = Gtk::manage(new Gtk::Entry());
    title_e->set_placeholder_text("Title");
    auto* tv = Gtk::manage(new Gtk::TextView());
    tv->set_wrap_mode(Gtk::WRAP_WORD);
    Gtk::ScrolledWindow* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_min_content_height(200);
    sc->add(*tv);

    std::vector<PendingAttachment> pending_issue_files;
    auto* add_issue_att = Gtk::manage(new Gtk::Button("Attach files…"));

    Gtk::Box& c = *dlg.get_content_area();
    c.pack_start(*repo_combo, false, false);
    c.pack_start(*title_e, false, false);
    c.pack_start(*sc, true, true);
    c.pack_start(*add_issue_att, false, false);

    add_issue_att->signal_clicked().connect([&]() {
        GtkWindow* pw = GTK_WINDOW(dlg.gobj());
        GtkFileChooserNative* native =
            gtk_file_chooser_native_new("Attach to new issue", pw, GTK_FILE_CHOOSER_ACTION_OPEN, "_Add", "_Cancel");
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(native), TRUE);
        int res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
        if (res != GTK_RESPONSE_ACCEPT) {
            g_object_unref(native);
            return;
        }
        GSList* files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(native));
        for (GSList* l = files; l; l = l->next) {
            GFile* gf = G_FILE(l->data);
            char* path = g_file_get_path(gf);
            if (!path) continue;
            std::string ppath(path);
            g_free(path);
            auto bytes = read_file_bytes(ppath);
            if (bytes.empty()) continue;
            gboolean uncertain{};
            gchar* ct = g_content_type_guess(
                ppath.c_str(), bytes.data(), static_cast<gsize>(bytes.size()), &uncertain);
            std::string mime = ct ? ct : "application/octet-stream";
            if (ct) g_free(ct);
            pending_issue_files.push_back(PendingAttachment{file_basename(ppath), mime, std::move(bytes)});
        }
        if (files) g_slist_free_full(files, reinterpret_cast<GDestroyNotify>(g_object_unref));
        g_object_unref(native);
    });

    dlg.set_default_size(520, 360);
    dlg.show_all_children();

    if (dlg.run() != Gtk::RESPONSE_OK) return;

    std::string repo = repo_combo->get_active_text();
    if (repo.empty() && !repos.empty()) repo = repos[0];

    Glib::RefPtr<Gtk::TextBuffer> buf = tv->get_buffer();
    Gtk::TextBuffer::iterator b, e;
    buf->get_bounds(b, e);
    std::string body = buf->get_text(b, e);

    try {
        GiteaIssue created = app_.issues().create_issue(repo, title_e->get_text(), body);
        for (auto const& pa : pending_issue_files) {
            (void)app_.gitea().upload_issue_attachment(
                repo, created.number, pa.file_name, pa.bytes, pa.mime_type);
        }
        rebuild();
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog e(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            e.run();
        }
    }
}

void IssuesView::on_toggle_issue_state()
{
    if (!selected_issue_.has_value()) return;
    GiteaIssue const& iss = *selected_issue_;
    std::string const new_state = iss.state == "open" ? "closed" : "open";
    try {
        (void)app_.issues().update_issue(iss.repository, iss.number, std::nullopt, std::nullopt, new_state);
        rebuild();
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog dlg(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
    }
}

void IssuesView::on_edit_labels()
{
    if (!selected_issue_.has_value()) return;
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!top) return;

    Gtk::Dialog dialog("Issue labels", *top, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);
    auto* entry = Gtk::manage(new Gtk::Entry());
    entry->set_text(join_labels(selected_issue_->labels));
    entry->set_placeholder_text("bug, planned, do");
    entry->set_activates_default(true);
    auto* help = Gtk::manage(new Gtk::Label(
        "Comma-separated. New labels are created in Gitea; existing Kanban and Covey labels are retained unless removed."));
    help->set_line_wrap(true);
    help->set_halign(Gtk::ALIGN_START);
    dialog.get_content_area()->pack_start(*entry, false, false);
    dialog.get_content_area()->pack_start(*help, false, false);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    dialog.set_default_size(520, 160);
    dialog.show_all_children();
    if (dialog.run() != Gtk::RESPONSE_OK) return;

    try {
        auto const issue = *selected_issue_;
        app_.issues().replace_labels(issue.repository, issue.number, split_labels(entry->get_text()));
        rebuild();
    }
    catch (std::exception const& err) {
        Gtk::MessageDialog error(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
        error.run();
    }
}

void IssuesView::on_send_comment()
{
    if (!selected_issue_.has_value()) return;
    Glib::ustring t = comment_entry_.get_text();
    if (t.empty()) return;

    GiteaIssue const& iss = *selected_issue_;
    try {
        app_.issues().add_comment(iss.repository, iss.number, t.raw());
        comment_entry_.set_text("");
        load_detail_from_network();
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog dlg(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
    }
}

void IssuesView::on_attach_with_comment()
{
    if (!selected_issue_.has_value()) return;

    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(gobj())));
    GtkFileChooserNative* native =
        gtk_file_chooser_native_new("Attach file to comment", parent, GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", "_Attach");

    int res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res != GTK_RESPONSE_ACCEPT) {
        g_object_unref(native);
        return;
    }

    char* path{};
    GFile* gf = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native));
    if (gf) {
        path = g_file_get_path(gf);
        g_object_unref(gf);
    }
    g_object_unref(native);
    if (!path) return;
    std::string ppath{path};
    g_free(path);

    std::vector<std::uint8_t> bytes = read_file_bytes(ppath);
    if (bytes.empty()) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog dlg(*top, "Could not read file.", false, Gtk::MESSAGE_ERROR,
                Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        return;
    }

    GiteaIssue const& iss = *selected_issue_;

    try {
        Glib::ustring ut = comment_entry_.get_text();
        std::string cbody = ut.empty() ? " " : ut.raw();

        GiteaComment cm = app_.gitea().add_comment(iss.repository, iss.number, cbody);

        gboolean uncertain{};
        gchar* ct = g_content_type_guess(ppath.c_str(), bytes.data(), static_cast<gsize>(bytes.size()), &uncertain);
        std::string mime = ct ? ct : "application/octet-stream";
        if (ct) g_free(ct);

        (void)app_.gitea().upload_comment_attachment(
            iss.repository, cm.id, file_basename(ppath), bytes, mime);

        comment_entry_.set_text("");
        load_detail_from_network();
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog dlg(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
    }
}

} // namespace cd
