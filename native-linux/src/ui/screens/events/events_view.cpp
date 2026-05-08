#include "events_view.h"

#include "app_container.h"
#include "components/markdown_view.h"
#include "data/db/event_dao.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstring>

namespace cd {

namespace {

EpochMillis day_start_ms(GDateTime* day_local_midnight)
{
    if (!day_local_midnight) return 0;
    gint64 u = g_date_time_to_unix(day_local_midnight);
    return u * 1000;
}

} // namespace

EventsView::EventsView(AppContainer& app)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , app_(app)
    , paned_(Gtk::ORIENTATION_HORIZONTAL)
    , filter_box_(Gtk::ORIENTATION_HORIZONTAL, 8)
    , rb_group_{}
    , rb_day_{rb_group_, Glib::ustring("Day")}
    , rb_week_{rb_group_, Glib::ustring("Week")}
    , rb_month_{rb_group_, Glib::ustring("Month")}
    , scroll_{}
    , list_{}
    , detail_(Gtk::ORIENTATION_VERTICAL, 8)
    , detail_title_("")
    , detail_when_("")
{
    rb_day_.set_active(true);
    filter_box_.pack_start(rb_day_, false, false);
    filter_box_.pack_start(rb_week_, false, false);
    filter_box_.pack_start(rb_month_, false, false);
    pack_start(filter_box_, false, false);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    scroll_.add(list_);

    detail_title_.set_halign(Gtk::ALIGN_START);
    detail_title_.set_line_wrap(true);
    detail_title_.set_selectable(true);
    detail_when_.set_halign(Gtk::ALIGN_START);
    detail_when_.set_line_wrap(true);

    auto* md = Gtk::manage(new MarkdownView());
    detail_body_ = md;
    md->set_min_content_height(160);

    detail_.pack_start(detail_title_, false, false);
    detail_.pack_start(detail_when_, false, false);
    detail_.pack_start(*md, true, true);

    paned_.pack1(scroll_, true, false);
    paned_.pack2(detail_, true, false);
    paned_.set_position(360);
    pack_start(paned_, true, true);

    rb_day_.signal_toggled().connect(sigc::mem_fun(*this, &EventsView::on_filter_changed));
    rb_week_.signal_toggled().connect(sigc::mem_fun(*this, &EventsView::on_filter_changed));
    rb_month_.signal_toggled().connect(sigc::mem_fun(*this, &EventsView::on_filter_changed));
    list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_selection_changed(); });

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(list_.gobj())), "Event list");
}

void EventsView::range_for_filter(TimeFilter f, EpochMillis* out_begin, EpochMillis* out_end)
{
    GDateTime* now = g_date_time_new_now_local();
    if (!now) {
        *out_begin = *out_end = 0;
        return;
    }

    if (f == TimeFilter::Day) {
        gint y{}, m{}, d{};
        g_date_time_get_ymd(now, &y, &m, &d);
        GDateTime* s = g_date_time_new_local(y, m, d, 0, 0, 0.0);
        GDateTime* e = g_date_time_add_days(s, 1);
        *out_begin = day_start_ms(s);
        *out_end = day_start_ms(e) - 1;
        g_date_time_unref(e);
        g_date_time_unref(s);
        g_date_time_unref(now);
        return;
    }

    if (f == TimeFilter::Week) {
        int dow = g_date_time_get_day_of_week(now);
        GDateTime* mon = g_date_time_add_days(now, -(dow - 1));
        gint y{}, m{}, d{};
        g_date_time_get_ymd(mon, &y, &m, &d);
        GDateTime* s = g_date_time_new_local(y, m, d, 0, 0, 0.0);
        GDateTime* next = g_date_time_add_days(s, 7);
        *out_begin = day_start_ms(s);
        *out_end = day_start_ms(next) - 1;
        g_date_time_unref(next);
        g_date_time_unref(s);
        g_date_time_unref(mon);
        g_date_time_unref(now);
        return;
    }

    gint y{}, m{}, d{};
    g_date_time_get_ymd(now, &y, &m, &d);
    GDateTime* s = g_date_time_new_local(y, m, 1, 0, 0, 0.0);
    GDateTime* next_month = g_date_time_add_months(s, 1);
    *out_begin = day_start_ms(s);
    *out_end = day_start_ms(next_month) - 1;
    g_date_time_unref(next_month);
    g_date_time_unref(s);
    g_date_time_unref(now);
}

GdkRGBA EventsView::color_for_calendar(std::optional<std::string> const& calendar_href)
{
    GdkRGBA c{};
    std::string const key = calendar_href.value_or("");
    unsigned h = 0;
    for (unsigned char ch : key)
        h = h * 131u + static_cast<unsigned>(ch);

    double hue = (h % 360) / 360.0;
    double const s = 0.55;
    double const v = 0.85;
    int i = static_cast<int>(hue * 6.0);
    double f = hue * 6.0 - static_cast<double>(i);
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    double r{}, g{}, b{};
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    c.red = r;
    c.green = g;
    c.blue = b;
    c.alpha = 1.0;
    return c;
}

void EventsView::on_filter_changed()
{
    if (rb_day_.get_active())
        filter_ = TimeFilter::Day;
    else if (rb_week_.get_active())
        filter_ = TimeFilter::Week;
    else
        filter_ = TimeFilter::Month;
    rebuild();
}

void EventsView::refresh()
{
    rebuild();
}

void EventsView::rebuild()
{
    for (Gtk::Widget* rw : list_.get_children()) list_.remove(*rw);

    EventDao dao(app_.db());
    auto all = dao.get_all();

    EpochMillis b{}, e{};
    range_for_filter(filter_, &b, &e);

    filtered_.clear();
    for (auto const& ev : all) {
        if (ev.start >= b && ev.start <= e)
            filtered_.push_back(ev);
    }

    std::stable_sort(filtered_.begin(), filtered_.end(),
        [](CalendarEvent const& a, CalendarEvent const& b) { return a.start < b.start; });

    for (auto const& ev : filtered_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow);
        auto* hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));

        GdkRGBA const dot_rgba = color_for_calendar(ev.calendar_href);
        gchar* col = gdk_rgba_to_string(&dot_rgba);
        auto* dot_l = Gtk::manage(new Gtk::Label());
        if (col) {
            dot_l->set_markup(std::string("<span foreground='") + col + "' font_size='large'>●</span>");
            g_free(col);
        }
        else
            dot_l->set_text("●");

        GDateTime* sdt = g_date_time_new_from_unix_local(static_cast<gint64>(ev.start / 1000));
        gchar* tstr{};
        if (sdt) tstr = g_date_time_format(sdt, "%a %F %R");
        if (sdt) g_date_time_unref(sdt);

        auto* lbl = Gtk::manage(new Gtk::Label());
        gchar* esc = g_markup_escape_text(ev.summary.c_str(), -1);
        Glib::ustring mk = esc ? esc : "";
        if (esc) g_free(esc);
        if (tstr) {
            gchar* te = g_markup_escape_text(tstr, -1);
            mk += "\n<span size='smaller'><i>" + Glib::ustring(te ? te : "") + "</i></span>";
            g_free(tstr);
            if (te) g_free(te);
        }
        lbl->set_markup(mk);
        lbl->set_halign(Gtk::ALIGN_START);

        hb->pack_start(*dot_l, false, false);
        hb->pack_start(*lbl, true, true);
        row->add(*hb);
        list_.append(*row);
    }

    detail_title_.set_text("");
    detail_when_.set_text("");
    if (detail_body_) detail_body_->load_markdown("");

    list_.show_all();
}

void EventsView::on_selection_changed()
{
    auto* row = list_.get_selected_row();
    if (!row) {
        detail_title_.set_text("");
        detail_when_.set_text("");
        if (detail_body_) detail_body_->load_markdown("");
        return;
    }

    int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row->gobj()));
    if (idx < 0 || static_cast<std::size_t>(idx) >= filtered_.size())
        return;

    CalendarEvent const& ev = filtered_[static_cast<std::size_t>(idx)];

    detail_title_.set_text(ev.summary);

    GDateTime* sdt = g_date_time_new_from_unix_local(static_cast<gint64>(ev.start / 1000));
    GDateTime* edt = g_date_time_new_from_unix_local(static_cast<gint64>(ev.end / 1000));
    std::string when;
    if (sdt && edt) {
        gchar* a = g_date_time_format(sdt, "%F %R");
        gchar* b = g_date_time_format(edt, "%R");
        if (a && b) when = std::string("Starts: ") + a + "\nEnds: " + b;
        if (a) g_free(a);
        if (b) g_free(b);
    }
    if (sdt) g_date_time_unref(sdt);
    if (edt) g_date_time_unref(edt);
    if (ev.location && !ev.location->empty())
        when += std::string("\n") + *ev.location;

    detail_when_.set_text(when);

    std::string md;
    if (ev.description && !ev.description->empty())
        md = *ev.description;
    if (detail_body_) detail_body_->load_markdown(md);
}

} // namespace cd
