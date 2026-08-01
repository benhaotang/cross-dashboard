#include "notes_view.h"

#include "app_container.h"
#include "components/read_markdown_field.h"
#include "background/sync_scheduler.h"
#include "data/db/note_dao.h"
#include "data/prefs/prefs.h"
#include "data/repository/repositories.h"

#include <glib.h>
#include <gtk/gtk.h>

extern "C" {
#include <handy.h>
}

#include <algorithm>
#include <cctype>
#include <chrono>

#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/paned.h>
#include <gtkmm/separator.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool contains_ci(std::string const& hay, std::string const& needle)
{
    if (needle.empty()) return true;
    auto h = hay;
    auto n = needle;
    for (char& c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return h.find(n) != std::string::npos;
}

std::string pick_note_calendar(AppContainer& app)
{
    auto hrefs = app.events().selected_calendar_hrefs();
    if (!hrefs.empty())
        return hrefs[0];
    if (auto s = app.secrets().get(CredentialKey::CALDAV_DEFAULT_EVENT_CALENDAR))
        if (!s->empty()) return *s;
    return {};
}

} // namespace

NotesView::NotesView(AppContainer& app, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
    , app_(app)
    , sync_(sync)
    , toolbar_(Gtk::ORIENTATION_HORIZONTAL, 6)
    , search_{}
    , btn_grid_{}
    , btn_list_{}
    , mode_{}
    , flow_{}
    , list_{}
    , detail_(Gtk::ORIENTATION_VERTICAL, 0)
{
    // New note: icon-only button
    auto* new_btn = Gtk::manage(new Gtk::Button());
    new_btn->set_image_from_icon_name("document-new-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    new_btn->set_tooltip_text("New note (Ctrl+N)");
    new_btn->set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(new_btn->gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(new_btn->gobj())))
        atk_object_set_name(a, "New note");

    // Grid/list toggle buttons with icons
    btn_grid_.set_image_from_icon_name("view-grid-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    btn_grid_.set_always_show_image(true);
    btn_grid_.set_tooltip_text("Grid view");
    btn_list_.set_image_from_icon_name("view-list-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    btn_list_.set_always_show_image(true);
    btn_list_.set_tooltip_text("List view");
    btn_grid_.set_mode(true);
    btn_list_.set_mode(true);
    btn_grid_.signal_toggled().connect([this] {
        if (btn_grid_.get_active()) {
            btn_list_.set_active(false);
            mode_.set_visible_child(Glib::ustring("grid"));
        }
    });
    btn_list_.signal_toggled().connect([this] {
        if (btn_list_.get_active()) {
            btn_grid_.set_active(false);
            mode_.set_visible_child(Glib::ustring("list"));
        }
    });
    btn_grid_.set_active(true);

    search_.signal_changed().connect([this] { apply_filter(search_.get_text()); });
    search_.set_placeholder_text("Search notes…");

    // Toolbar: [+ new] [search expanding] | [grid][list]
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");
    toolbar_.pack_start(*new_btn, false, false);

    GtkWidget* hbar = GTK_WIDGET(hdy_search_bar_new());
    gtk_container_add(GTK_CONTAINER(hbar), GTK_WIDGET(search_.gobj()));
    hdy_search_bar_connect_entry(HDY_SEARCH_BAR(hbar), GTK_ENTRY(search_.gobj()));
    gtk_box_pack_start(GTK_BOX(toolbar_.gobj()), hbar, TRUE, TRUE, 0);

    // Linked view-toggle group
    auto* view_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(view_box->gobj())), "linked");
    view_box->pack_start(btn_grid_, false, false);
    view_box->pack_start(btn_list_, false, false);
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh notes");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh notes");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(refresh_btn_, false, false);
    toolbar_.pack_end(*view_box, false, false);

    pack_start(toolbar_, false, false);

    Gtk::ScrolledWindow* sc_flow = Gtk::manage(new Gtk::ScrolledWindow());
    sc_flow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    flow_.set_selection_mode(Gtk::SELECTION_NONE);
    flow_.set_homogeneous(false);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow_.gobj()), 4);
    sc_flow->add(flow_);

    Gtk::ScrolledWindow* sc_list = Gtk::manage(new Gtk::ScrolledWindow());
    sc_list->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    sc_list->add(list_);

    mode_.add(*sc_flow, Glib::ustring("grid"));
    mode_.add(*sc_list, Glib::ustring("list"));
    mode_.set_visible_child(Glib::ustring("grid"));

    auto* body = Gtk::manage(new ReadMarkdownField());
    body_field_ = body;
    body->set_field_label("Body");
    body->set_markdown("");

    // Detail header: right-aligned Edit icon button
    auto* detail_hdr = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(detail_hdr->gobj())), "cd-toolbar");
    auto* edit_btn = Gtk::manage(new Gtk::Button());
    edit_btn->set_image_from_icon_name("document-edit-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    edit_btn->set_tooltip_text("Edit note");
    edit_btn->set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(edit_btn->gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(edit_btn->gobj())))
        atk_object_set_name(a, "Edit note");
    edit_btn->signal_clicked().connect(sigc::mem_fun(*this, &NotesView::on_edit_note));
    detail_hdr->pack_end(*edit_btn, false, false);

    detail_.pack_start(*detail_hdr, false, false);
    auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    detail_.pack_start(*sep, false, false);
    detail_.pack_start(*body, true, true);

    detail_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    detail_scroll_.add(detail_);

    Gtk::Paned* paned = Gtk::manage(new Gtk::Paned(Gtk::ORIENTATION_HORIZONTAL));
    Gtk::Box* left = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    left->pack_start(mode_, true, true);
    paned->pack1(*left, true, false);
    paned->pack2(detail_scroll_, true, false);
    paned->set_position(360);
    pack_start(*paned, true, true);

    new_btn->signal_clicked().connect(sigc::mem_fun(*this, &NotesView::on_new_note));

    list_.signal_row_selected().connect([this](Gtk::ListBoxRow* row) {
        if (!row) {
            selected_uid_.reset();
            if (body_field_) body_field_->set_markdown("");
            return;
        }
        int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row->gobj()));
        if (idx < 0 || static_cast<std::size_t>(idx) >= notes_.size())
            return;
        selected_uid_ = notes_[static_cast<std::size_t>(idx)].uid;
        show_detail(notes_[static_cast<std::size_t>(idx)]);
    });

    rebuild();
}

void NotesView::apply_filter(Glib::ustring const& q)
{
    std::string const qs = q.raw();

    NoteDao dao(app_.db());
    auto const all = dao.get_all();
    notes_.clear();
    for (auto const& n : all) {
        if (contains_ci(n.summary + " " + n.body, qs))
            notes_.push_back(n);
    }

    for (Gtk::Widget* rw : flow_.get_children()) flow_.remove(*rw);
    for (Gtk::Widget* rw : list_.get_children()) list_.remove(*rw);

    for (auto const& n : notes_) {
        auto* b = Gtk::manage(new Gtk::Button());
        b->set_label(n.summary);
        b->set_relief(Gtk::RELIEF_NONE);
        b->set_tooltip_text(n.summary);

        Note snap = n;
        b->signal_clicked().connect([this, snap] {
            selected_uid_ = snap.uid;
            show_detail(snap);
        });

        auto* fbc = Gtk::manage(new Gtk::FlowBoxChild());
        fbc->add(*b);
        flow_.add(*fbc);

        auto* row = Gtk::manage(new Gtk::ListBoxRow);
        auto* lab = Gtk::manage(new Gtk::Label(n.summary));
        lab->set_halign(Gtk::ALIGN_START);
        lab->set_tooltip_text(n.summary);
        row->add(*lab);
        list_.append(*row);
    }

    flow_.show_all();
    list_.show_all();
}

void NotesView::show_detail(Note const& n)
{
    if (!body_field_) return;
    body_field_->set_markdown(n.body.empty() ? "_No body_" : n.body);
}

void NotesView::rebuild()
{
    apply_filter(search_.get_text());
}

void NotesView::focus_search()
{
    search_.grab_focus();
}

void NotesView::on_new_note()
{
    std::string cal = pick_note_calendar(app_);
    if (cal.empty()) {
        Gtk::Window* transient = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (transient) {
            Gtk::MessageDialog dlg(
                *transient, "Set CalDAV selected calendars or default event calendar.", false,
                Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        return;
    }

    Gtk::Dialog dlg;
    dlg.set_title("New note");
    dlg.set_modal(true);
    dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("_Create", Gtk::RESPONSE_OK);

    Gtk::Box& content = *dlg.get_content_area();
    auto* sum = Gtk::manage(new Gtk::Entry());
    sum->set_placeholder_text("Title");
    Gtk::ScrolledWindow* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_min_content_height(180);
    auto* tv = Gtk::manage(new Gtk::TextView());
    tv->set_wrap_mode(Gtk::WRAP_WORD);
    sc->add(*tv);

    content.pack_start(*sum, false, false);
    content.pack_start(*sc, true, true);
    dlg.set_default_size(480, 320);
    dlg.show_all_children();

    if (dlg.run() != Gtk::RESPONSE_OK) return;

    Glib::RefPtr<Gtk::TextBuffer> buf = tv->get_buffer();
    Gtk::TextBuffer::iterator b{}, e{};
    buf->get_bounds(b, e);

    Note n{};
    n.summary = sum->get_text();
    n.body = buf->get_text(b, e);
    EpochMillis ms = millis_now_wall();
    n.created = ms;
    n.last_modified = ms;

    try {
        app_.notes().create(n, cal);
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog e(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            e.run();
        }
        return;
    }

    rebuild();
}

void NotesView::on_edit_note()
{
    if (!selected_uid_.has_value()) return;
    NoteDao dao(app_.db());
    auto cur = dao.get_by_uid(*selected_uid_);
    if (!cur.has_value()) return;

    std::string cal = cur->calendar_href.value_or(pick_note_calendar(app_));
    if (cal.empty()) {
        Gtk::Window* transient = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (transient) {
            Gtk::MessageDialog dlg(*transient, "Note has no calendar URL on disk.", false,
                Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        return;
    }

    Gtk::Dialog dlg;
    dlg.set_title("Edit note");
    dlg.set_modal(true);
    dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("_Save", Gtk::RESPONSE_OK);

    Gtk::Box& content = *dlg.get_content_area();
    auto* sum = Gtk::manage(new Gtk::Entry());
    sum->set_text(cur->summary);
    Gtk::ScrolledWindow* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_min_content_height(180);
    auto* tv = Gtk::manage(new Gtk::TextView());
    tv->set_wrap_mode(Gtk::WRAP_WORD);
    tv->get_buffer()->set_text(cur->body);
    sc->add(*tv);
    content.pack_start(*sum, false, false);
    content.pack_start(*sc, true, true);
    dlg.set_default_size(480, 320);
    dlg.show_all_children();

    if (dlg.run() != Gtk::RESPONSE_OK) return;
    Glib::RefPtr<Gtk::TextBuffer> buf = tv->get_buffer();
    Gtk::TextBuffer::iterator b{}, e{};
    buf->get_bounds(b, e);

    Note upd = *cur;
    upd.summary = sum->get_text();
    upd.body = buf->get_text(b, e);
    upd.last_modified = millis_now_wall();

    try {
        app_.notes().update(upd);
    }
    catch (std::exception const& err) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (top) {
            Gtk::MessageDialog e(*top, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            e.run();
        }
        return;
    }

    rebuild();
}

} // namespace cd
