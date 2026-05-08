#include "views_view.h"

#include "app_container.h"
#include "background/sync_scheduler.h"
#include "data/db/task_dao.h"
#include "data/prefs/prefs.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/separator.h>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace cd {

namespace {

GtkTargetEntry drag_targets[] = {
    {(gchar*)"text/plain", 0, 0},
};

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool ci_eq(std::string a, std::string b)
{
    for (char& c : a)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : b)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return a == b;
}

bool has_any_ci(std::vector<std::string> const& cats, std::vector<std::string> const& tags)
{
    for (auto const& c : cats) {
        for (auto const& t : tags) {
            if (ci_eq(c, t)) return true;
        }
    }
    return false;
}

std::vector<std::string> strip_matching_tags(
    std::vector<std::string> cats, std::vector<std::string> const& to_strip)
{
    std::vector<std::string> out;
    for (auto const& c : cats) {
        bool strip = false;
        for (auto const& t : to_strip) {
            if (ci_eq(c, t)) {
                strip = true;
                break;
            }
        }
        if (!strip) out.push_back(c);
    }
    return out;
}

std::vector<std::string> covey_tag_strings()
{
    std::vector<std::string> o;
    for (auto* p : kCoveyQuadrantTags) o.emplace_back(p);
    return o;
}

extern "C" void views_drag_data_get(GtkWidget* widget, GdkDragContext*, GtkSelectionData* data, guint, guint,
    gpointer)
{
    char const* uid = static_cast<char const*>(g_object_get_data(G_OBJECT(widget), "cd-task-uid"));
    if (uid) gtk_selection_data_set_text(data, uid, -1);
}

extern "C" void views_kanban_drag_recv(GtkWidget* widget, GdkDragContext*, gint, gint, GtkSelectionData* sel, guint,
    guint32, gpointer user_data)
{
    auto* self = static_cast<ViewsView*>(user_data);
    if (!self || !sel) return;
    guchar* raw = gtk_selection_data_get_text(sel);
    if (!raw) return;
    std::string uid{reinterpret_cast<char*>(raw)};
    g_free(raw);
    gpointer p = g_object_get_data(G_OBJECT(widget), "cd-kanban-col");
    int col = p ? GPOINTER_TO_INT(p) : -999;
    self->on_kanban_dropped(uid, col);
}

extern "C" void views_covey_drag_recv(GtkWidget* widget, GdkDragContext*, gint, gint, GtkSelectionData* sel, guint,
    guint32, gpointer user_data)
{
    auto* self = static_cast<ViewsView*>(user_data);
    if (!self || !sel) return;
    guchar* raw = gtk_selection_data_get_text(sel);
    if (!raw) return;
    std::string uid{reinterpret_cast<char*>(raw)};
    g_free(raw);
    int q = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "cd-covey-q"));
    self->on_covey_dropped(uid, q);
}

} // namespace

ViewsView::ViewsView(AppContainer& app, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
    , app_(app)
    , sync_(sync)
{
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh views");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh views");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(refresh_btn_, false, false);
    pack_start(toolbar_, false, false);

    // Kanban: horizontally scrollable board
    kanban_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    kanban_board_.set_spacing(8);
    kanban_board_.set_margin_start(12);
    kanban_board_.set_margin_end(12);
    kanban_board_.set_margin_top(12);
    kanban_board_.set_margin_bottom(12);
    kanban_scroll_.add(kanban_board_);
    kanban_scroll_.set_vexpand(true);
    tabs_.append_page(kanban_scroll_, "Kanban");

    // Covey: unassigned strip + 2×2 grid filling the full area
    {
        auto* unlab = Gtk::manage(new Gtk::Label());
        unlab->set_markup("<small><b>UNASSIGNED</b></small>");
        unlab->set_halign(Gtk::ALIGN_START);
        unlab->set_margin_start(10);
        unlab->set_margin_top(8);
        unlab->set_margin_bottom(4);
        covey_unassigned_box_.pack_start(*unlab, false, false);
    }
    covey_unassigned_box_.pack_start(covey_unassigned_slot_, false, false);
    covey_unassigned_box_.set_margin_bottom(6);

    covey_grid_.set_column_spacing(10);
    covey_grid_.set_row_spacing(10);
    covey_grid_.set_column_homogeneous(true);
    covey_grid_.set_row_homogeneous(true);
    covey_grid_.set_hexpand(true);
    covey_grid_.set_vexpand(true);
    covey_grid_.set_margin_start(10);
    covey_grid_.set_margin_end(10);
    covey_grid_.set_margin_top(6);
    covey_grid_.set_margin_bottom(10);

    covey_outer_.set_vexpand(true);
    covey_outer_.set_hexpand(true);
    covey_outer_.pack_start(covey_unassigned_box_, false, false);
    covey_outer_.pack_start(covey_grid_, true, true);
    tabs_.append_page(covey_outer_, "Covey");

    pack_start(tabs_, true, true);
    rebuild();
}

void ViewsView::on_kanban_dropped(std::string const& task_uid, int column_code)
{
    AppSettings const cfg = merged_app_preferences(app_.prefs());
    if (column_code == -999) return;
    if (column_code < -1 || column_code >= static_cast<int>(cfg.kanban_columns.size())) return;

    if (column_code == -1) apply_kanban_tag(task_uid, std::nullopt);
    else apply_kanban_tag(task_uid, cfg.kanban_columns[static_cast<std::size_t>(column_code)]);
}

void ViewsView::on_covey_dropped(std::string const& task_uid, int quadrant_index)
{
    if (quadrant_index == -1) {
        apply_covey_quadrant(task_uid, -1);
        return;
    }
    if (quadrant_index < 0 || quadrant_index > 3) return;
    apply_covey_quadrant(task_uid, quadrant_index);
}

void ViewsView::apply_kanban_tag(std::string const& uid, std::optional<std::string> const& column_tag)
{
    AppSettings const cfg = merged_app_preferences(app_.prefs());
    TaskDao dao(app_.db());
    auto opt = dao.get_by_uid(uid);
    if (!opt.has_value()) return;
    CalDavTask t = *opt;
    std::vector<std::string> cats = strip_matching_tags(std::move(t.categories), cfg.kanban_columns);
    if (column_tag.has_value() && !column_tag->empty()) cats.push_back(*column_tag);
    t.categories = std::move(cats);
    t.last_modified = millis_now_wall();
    try {
        app_.tasks().update(t);
    }
    catch (...) {
    }
    rebuild();
}

void ViewsView::apply_covey_quadrant(std::string const& uid, int quadrant_index)
{
    TaskDao dao(app_.db());
    auto opt = dao.get_by_uid(uid);
    if (!opt.has_value()) return;
    CalDavTask t = *opt;
    auto ctags = covey_tag_strings();
    std::vector<std::string> cats = strip_matching_tags(std::move(t.categories), ctags);
    if (quadrant_index >= 0 && quadrant_index < 4)
        cats.emplace_back(kCoveyQuadrantTags[static_cast<std::size_t>(quadrant_index)]);
    t.categories = std::move(cats);
    t.last_modified = millis_now_wall();
    try {
        app_.tasks().update(t);
    }
    catch (...) {
    }
    rebuild();
}

void ViewsView::run_assign_dialog(CalDavTask const& task, bool covey_mode)
{
    Gtk::Window* win = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!win) return;

    Gtk::Dialog dlg(covey_mode ? "Assign quadrant" : "Assign Kanban column", *win, true);
    dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dlg.add_button("_Apply", Gtk::RESPONSE_OK);
    Gtk::ComboBoxText combo;
    AppSettings const cfg = merged_app_preferences(app_.prefs());

    if (covey_mode) {
        for (auto* tag : kCoveyQuadrantTags) combo.append(tag);
        int active = 0;
        for (std::size_t i = 0; i < kCoveyQuadrantTags.size(); ++i) {
            if (has_any_ci(task.categories, {std::string{kCoveyQuadrantTags[i]}})) {
                active = static_cast<int>(i);
                break;
            }
        }
        combo.set_active(active);
    }
    else {
        combo.append("(Untagged)");
        for (auto const& c : cfg.kanban_columns) combo.append(c);
        int active = 0;
        bool found = false;
        for (std::size_t i = 0; i < cfg.kanban_columns.size(); ++i) {
            if (has_any_ci(task.categories, {cfg.kanban_columns[i]})) {
                active = static_cast<int>(i) + 1;
                found = true;
                break;
            }
        }
        if (!found) active = 0;
        combo.set_active(active);
    }

    dlg.get_content_area()->pack_start(combo, true, true);
    dlg.show_all_children();
    if (dlg.run() != Gtk::RESPONSE_OK) return;

    if (covey_mode) {
        int a = combo.get_active_row_number();
        if (a < 0) return;
        apply_covey_quadrant(task.uid, a);
    }
    else {
        int a = combo.get_active_row_number();
        if (a == 0) apply_kanban_tag(task.uid, std::nullopt);
        else if (a > 0 && static_cast<std::size_t>(a - 1) < cfg.kanban_columns.size())
            apply_kanban_tag(task.uid, cfg.kanban_columns[static_cast<std::size_t>(a - 1)]);
    }
}

void ViewsView::fill_kanban(std::vector<CalDavTask> const& open_tasks)
{
    for (Gtk::Widget* w : kanban_board_.get_children()) kanban_board_.remove(*w);

    AppSettings const cfg = merged_app_preferences(app_.prefs());
    std::vector<std::string> const& cols = cfg.kanban_columns;

    auto make_column = [&](std::string const& title, int col_code, std::vector<CalDavTask> const& tasks_in_col) {
        // Outer column container — styled card
        auto* col_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(col_box->gobj())), "cd-kanban-col");
        gtk_widget_set_size_request(GTK_WIDGET(col_box->gobj()), 220, -1);
        col_box->set_vexpand(true);

        // Column header: title + count badge
        auto* hdr = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(hdr->gobj())), "cd-kanban-col-hdr");

        std::string cap_title = title;
        if (!cap_title.empty())
            cap_title.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(cap_title.front())));
        auto* title_l = Gtk::manage(new Gtk::Label());
        title_l->set_markup(("<b>" + cap_title + "</b>").c_str());
        title_l->set_halign(Gtk::ALIGN_START);
        title_l->set_hexpand(true);

        auto* badge = Gtk::manage(new Gtk::Label(std::to_string(tasks_in_col.size()).c_str()));
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(badge->gobj())), "cd-count-badge");

        hdr->pack_start(*title_l, true, true);
        hdr->pack_start(*badge, false, false);
        col_box->pack_start(*hdr, false, false);

        auto* hdr_sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
        col_box->pack_start(*hdr_sep, false, false);

        // Card list
        auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
        sc->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        sc->set_vexpand(true);
        auto* lb = Gtk::manage(new Gtk::ListBox());
        lb->set_selection_mode(Gtk::SELECTION_NONE);
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(lb->gobj())), "cd-kanban-list");
        GtkWidget* lw = GTK_WIDGET(lb->gobj());
        g_object_set_data(G_OBJECT(lw), "cd-kanban-col", GINT_TO_POINTER(col_code));
        gtk_drag_dest_set(lw, GTK_DEST_DEFAULT_ALL, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
        g_signal_connect(lw, "drag-data-received", G_CALLBACK(views_kanban_drag_recv), this);

        for (auto const& task : tasks_in_col) {
            auto* row = Gtk::manage(new Gtk::ListBoxRow);
            gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(row->gobj())), "cd-kanban-card");

            auto* card = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 3));
            auto* lab = Gtk::manage(new Gtk::Label(task.summary));
            lab->set_halign(Gtk::ALIGN_START);
            lab->set_line_wrap(true);
            lab->set_xalign(0.0f);

            auto* btn = Gtk::manage(new Gtk::Button());
            btn->set_image_from_icon_name("document-properties-symbolic", Gtk::ICON_SIZE_MENU);
            btn->set_tooltip_text("Assign column");
            btn->set_relief(Gtk::RELIEF_NONE);
            btn->signal_clicked().connect([this, task] { run_assign_dialog(task, false); });

            auto* row_hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
            row_hb->pack_start(*lab, true, true);
            row_hb->pack_start(*btn, false, false);
            card->pack_start(*row_hb, false, false);

            // Priority indicator
            if (task.priority > 0) {
                auto* prio = Gtk::manage(new Gtk::Label());
                std::string ptext = task.priority <= 4 ? "High" : task.priority == 5 ? "Medium" : "Low";
                prio->set_markup(("<small>" + ptext + " priority</small>").c_str());
                prio->set_halign(Gtk::ALIGN_START);
                gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(prio->gobj())), "dim-label");
                card->pack_start(*prio, false, false);
            }

            row->add(*card);
            lb->append(*row);
            GtkWidget* rw = GTK_WIDGET(row->gobj());
            g_object_set_data_full(G_OBJECT(rw), "cd-task-uid", g_strdup(task.uid.c_str()), g_free);
            gtk_drag_source_set(rw, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
            g_signal_connect(rw, "drag-data-get", G_CALLBACK(views_drag_data_get), nullptr);
        }

        if (tasks_in_col.empty()) {
            auto* empty_row = Gtk::manage(new Gtk::ListBoxRow());
            auto* empty_lab = Gtk::manage(new Gtk::Label("No tasks"));
            gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(empty_lab->gobj())), "dim-label");
            empty_lab->set_margin_top(16);
            empty_lab->set_margin_bottom(16);
            empty_row->add(*empty_lab);
            lb->append(*empty_row);
        }

        sc->add(*lb);
        col_box->pack_start(*sc, true, true);
        kanban_board_.pack_start(*col_box, false, false);
        lb->show_all();
    };

    std::vector<CalDavTask> untagged;
    for (auto const& t : open_tasks) {
        if (!has_any_ci(t.categories, cols)) untagged.push_back(t);
    }
    if (!untagged.empty()) make_column("Untagged", -1, untagged);

    for (std::size_t i = 0; i < cols.size(); ++i) {
        std::vector<CalDavTask> col_tasks;
        for (auto const& t : open_tasks) {
            if (has_any_ci(t.categories, {cols[i]})) col_tasks.push_back(t);
        }
        std::string hdr = cols[i];
        if (!hdr.empty()) hdr.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(hdr.front())));
        make_column(hdr, static_cast<int>(i), col_tasks);
    }

    kanban_board_.show_all();
}

void ViewsView::fill_covey(std::vector<CalDavTask> const& open_tasks)
{
    for (Gtk::Widget* w : covey_unassigned_slot_.get_children()) covey_unassigned_slot_.remove(*w);
    for (Gtk::Widget* w : covey_grid_.get_children()) covey_grid_.remove(*w);

    auto ctags = covey_tag_strings();
    std::vector<CalDavTask> unassigned;
    for (auto const& t : open_tasks) {
        if (!has_any_ci(t.categories, ctags)) unassigned.push_back(t);
    }

    if (!unassigned.empty()) {
        auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
        sc->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        sc->set_min_content_height(64);
        sc->set_max_content_height(96);
        auto* lb = Gtk::manage(new Gtk::ListBox());
        lb->set_selection_mode(Gtk::SELECTION_NONE);
        GtkWidget* lwg = GTK_WIDGET(lb->gobj());
        g_object_set_data(G_OBJECT(lwg), "cd-covey-q", GINT_TO_POINTER(-1));
        gtk_drag_dest_set(lwg, GTK_DEST_DEFAULT_ALL, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
        g_signal_connect(lwg, "drag-data-received", G_CALLBACK(views_covey_drag_recv), this);
        for (auto const& t : unassigned) {
            auto* row = Gtk::manage(new Gtk::ListBoxRow);
            auto* hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
            auto* lab = Gtk::manage(new Gtk::Label(t.summary));
            lab->set_halign(Gtk::ALIGN_START);
            lab->set_hexpand(true);
            auto* btn = Gtk::manage(new Gtk::Button());
            btn->set_image_from_icon_name("document-properties-symbolic", Gtk::ICON_SIZE_MENU);
            btn->set_relief(Gtk::RELIEF_NONE);
            btn->signal_clicked().connect([this, t] { run_assign_dialog(t, true); });
            hb->pack_start(*lab, true, true);
            hb->pack_start(*btn, false, false);
            row->add(*hb);
            lb->append(*row);
            GtkWidget* rw = GTK_WIDGET(row->gobj());
            g_object_set_data_full(G_OBJECT(rw), "cd-task-uid", g_strdup(t.uid.c_str()), g_free);
            gtk_drag_source_set(rw, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
            g_signal_connect(rw, "drag-data-get", G_CALLBACK(views_drag_data_get), nullptr);
        }
        sc->add(*lb);
        covey_unassigned_slot_.pack_start(*sc, false, false);
    }

    // Quadrant metadata: label, subtitle, CSS class
    struct QuadrantMeta {
        const char* label;
        const char* subtitle;
        const char* css_class;
    };
    static constexpr QuadrantMeta meta[4] = {
        {"Do First",  "Important & Urgent",         "cd-covey-do"},
        {"Schedule",  "Important & Not Urgent",      "cd-covey-delay"},
        {"Delegate",  "Not Important & Urgent",      "cd-covey-delegate"},
        {"Eliminate", "Not Important & Not Urgent",  "cd-covey-eliminate"},
    };

    for (int q = 0; q < 4; ++q) {
        auto* vb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
        vb->set_hexpand(true);
        vb->set_vexpand(true);
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(vb->gobj())), "cd-covey-quad");
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(vb->gobj())), meta[q].css_class);

        // Header: label (bold, colored) + subtitle (small, muted)
        auto* hdr = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
        hdr->set_margin_start(10);
        hdr->set_margin_end(10);
        hdr->set_margin_top(8);
        hdr->set_margin_bottom(6);

        auto* qlabel = Gtk::manage(new Gtk::Label());
        qlabel->set_markup(("<b>" + std::string(meta[q].label) + "</b>").c_str());
        qlabel->set_halign(Gtk::ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(qlabel->gobj())), "cd-covey-title");

        auto* qsub = Gtk::manage(new Gtk::Label());
        qsub->set_markup(("<small>" + std::string(meta[q].subtitle) + "</small>").c_str());
        qsub->set_halign(Gtk::ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(qsub->gobj())), "dim-label");

        hdr->pack_start(*qlabel, false, false);
        hdr->pack_start(*qsub, false, false);
        vb->pack_start(*hdr, false, false);

        auto* hdr_sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
        vb->pack_start(*hdr_sep, false, false);

        // Task list in scrolled window
        auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
        sc->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        sc->set_vexpand(true);
        auto* lb = Gtk::manage(new Gtk::ListBox());
        lb->set_selection_mode(Gtk::SELECTION_NONE);
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(lb->gobj())), "cd-covey-list");
        GtkWidget* lw = GTK_WIDGET(lb->gobj());
        g_object_set_data(G_OBJECT(lw), "cd-covey-q", GINT_TO_POINTER(q));
        gtk_drag_dest_set(lw, GTK_DEST_DEFAULT_ALL, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
        g_signal_connect(lw, "drag-data-received", G_CALLBACK(views_covey_drag_recv), this);

        std::string const qtag{kCoveyQuadrantTags[static_cast<std::size_t>(q)]};
        bool has_tasks = false;
        for (auto const& task : open_tasks) {
            if (has_any_ci(task.categories, {qtag})) {
                has_tasks = true;
                auto* row = Gtk::manage(new Gtk::ListBoxRow);
                auto* hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
                hb->set_margin_start(8);
                hb->set_margin_end(4);
                hb->set_margin_top(4);
                hb->set_margin_bottom(4);
                auto* lab = Gtk::manage(new Gtk::Label(task.summary));
                lab->set_halign(Gtk::ALIGN_START);
                lab->set_hexpand(true);
                lab->set_line_wrap(true);
                auto* btn = Gtk::manage(new Gtk::Button());
                btn->set_image_from_icon_name("document-properties-symbolic", Gtk::ICON_SIZE_MENU);
                btn->set_relief(Gtk::RELIEF_NONE);
                btn->signal_clicked().connect([this, task] { run_assign_dialog(task, true); });
                hb->pack_start(*lab, true, true);
                hb->pack_start(*btn, false, false);
                row->add(*hb);
                lb->append(*row);
                GtkWidget* rw = GTK_WIDGET(row->gobj());
                g_object_set_data_full(G_OBJECT(rw), "cd-task-uid", g_strdup(task.uid.c_str()), g_free);
                gtk_drag_source_set(rw, GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
                g_signal_connect(rw, "drag-data-get", G_CALLBACK(views_drag_data_get), nullptr);
            }
        }
        if (!has_tasks) {
            auto* empty_row = Gtk::manage(new Gtk::ListBoxRow());
            auto* empty_lab = Gtk::manage(new Gtk::Label("No tasks"));
            gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(empty_lab->gobj())), "dim-label");
            empty_lab->set_margin_top(12);
            empty_lab->set_margin_bottom(12);
            empty_row->add(*empty_lab);
            lb->append(*empty_row);
        }

        sc->add(*lb);
        vb->pack_start(*sc, true, true);

        covey_grid_.attach(*vb, q % 2, q / 2, 1, 1);
    }

    covey_unassigned_slot_.show_all();
    covey_grid_.show_all();
}

void ViewsView::rebuild()
{
    TaskDao dao(app_.db());
    auto all = dao.get_all();
    std::vector<CalDavTask> open;
    for (auto const& t : all) {
        if (t.status != TaskStatus::Completed && t.status != TaskStatus::Cancelled) open.push_back(t);
    }
    std::stable_sort(open.begin(), open.end(),
        [](CalDavTask const& a, CalDavTask const& b) { return a.summary < b.summary; });

    fill_kanban(open);
    fill_covey(open);
}

} // namespace cd
