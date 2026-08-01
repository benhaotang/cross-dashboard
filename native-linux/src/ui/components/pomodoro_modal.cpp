#include "pomodoro_modal.h"

#include "app_viewmodel.h"
#include "domain/models.h"

#include <gtk/gtk.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace cd {

namespace {

void fmt_time(int seconds, char* out, std::size_t out_size)
{
    if (seconds < 0)
        seconds = 0;
    int const mm = seconds / 60;
    int const ss = seconds % 60;
    std::snprintf(out, out_size, "%02d:%02d", mm, ss);
}

GtkWidget* make_labeled_icon_button(char const* icon_name, char const* label_utf8, char const* css_class = nullptr)
{
    GtkWidget* btn = gtk_button_new();
    if (css_class && *css_class)
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), css_class);
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(row, GTK_ALIGN_CENTER);
    GtkWidget* img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(row), img, FALSE, FALSE, 0);
    GtkWidget* lab = gtk_label_new_with_mnemonic(label_utf8);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lab), btn);
    gtk_box_pack_start(GTK_BOX(row), lab, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(btn), row);
    gtk_widget_show_all(btn);
    return btn;
}

GtkWidget* section_caption(char const* markup_utf8)
{
    GtkWidget* l = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(l), markup_utf8);
    gtk_label_set_xalign(GTK_LABEL(l), 0.5f);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "cd-pomodoro-section-label");
    return l;
}

} // namespace

gboolean PomodoroModal::delete_event_cb(GtkWidget* dlg, GdkEventAny*, gpointer user_data)
{
    auto* self = static_cast<PomodoroModal*>(user_data);
    self->vm_.set_pomodoro_modal_visible(false);
    gtk_widget_hide(dlg);
    return TRUE;
}

PomodoroModal::PomodoroModal(AppViewModel& vm)
    : vm_(vm)
{
    dialog_ = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog_), "Pomodoro");
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 400, 0);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(dialog_), "cd-pomodoro-dialog");

    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Pomodoro");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Focus timer");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(dialog_), header);

    GtkWidget* outer = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    gtk_container_set_border_width(GTK_CONTAINER(outer), 0);
    gtk_widget_set_margin_start(outer, 26);
    gtk_widget_set_margin_end(outer, 26);
    gtk_widget_set_margin_top(outer, 10);
    gtk_widget_set_margin_bottom(outer, 22);

    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(outer), col, TRUE, TRUE, 0);

    phase_label_ = gtk_label_new("Focus");
    gtk_widget_set_halign(phase_label_, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(phase_label_), 0.5f);
    gtk_style_context_add_class(gtk_widget_get_style_context(phase_label_), "cd-pomodoro-phase");

    time_label_ = gtk_label_new("25:00");
    gtk_widget_set_halign(time_label_, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(time_label_), 0.5f);
    gtk_style_context_add_class(gtk_widget_get_style_context(time_label_), "cd-pomodoro-time");

    session_label_ = gtk_label_new("Round 1 of 4");
    gtk_widget_set_halign(session_label_, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(session_label_), 0.5f);
    gtk_style_context_add_class(gtk_widget_get_style_context(session_label_), "cd-pomodoro-session");

    task_label_ = gtk_label_new("");
    gtk_widget_set_halign(task_label_, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(task_label_), 0.5f);
    gtk_label_set_line_wrap(GTK_LABEL(task_label_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(task_label_), 36);
    gtk_label_set_justify(GTK_LABEL(task_label_), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(task_label_), "cd-pomodoro-task");
    gtk_widget_set_no_show_all(task_label_, TRUE);

    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 16);
    gtk_widget_set_margin_bottom(sep, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(sep), "cd-pomodoro-sep");

    gtk_box_pack_start(GTK_BOX(col), phase_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), time_label_, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(col), session_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), task_label_, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(col), sep, FALSE, FALSE, 0);

    idle_section_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(idle_section_, 14);

    gtk_box_pack_start(GTK_BOX(idle_section_), section_caption("<span size='smaller' weight='600' alpha='60%'>LINK TO</span>"), FALSE, FALSE, 0);
    target_combo_ = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(target_combo_, TRUE);
    gtk_widget_set_tooltip_text(target_combo_, "Choose a task or open issue for this focus session");
    gtk_box_pack_start(GTK_BOX(idle_section_), target_combo_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(idle_section_), section_caption("<span size='smaller' weight='600' alpha='60%'>CHOOSE PHASE</span>"), FALSE, FALSE, 0);

    GtkWidget* start_linked = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(start_linked), "linked");

    GtkWidget* start_focus =
        make_labeled_icon_button("media-playback-start-symbolic", "_Focus", "suggested-action");
    gtk_widget_set_tooltip_text(start_focus, "Start a focus session");
    GtkWidget* start_short = make_labeled_icon_button("alarm-symbolic", "_Short", nullptr);
    gtk_widget_set_tooltip_text(start_short, "Short break");
    GtkWidget* start_long = make_labeled_icon_button("weather-clear-night-symbolic", "_Long", nullptr);
    gtk_widget_set_tooltip_text(start_long, "Long break");
    gtk_style_context_add_class(gtk_widget_get_style_context(start_short), "cd-pomodoro-linked-quiet");
    gtk_style_context_add_class(gtk_widget_get_style_context(start_long), "cd-pomodoro-linked-quiet");

    gtk_box_pack_start(GTK_BOX(start_linked), start_focus, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(start_linked), start_short, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(start_linked), start_long, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(idle_section_), start_linked, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), idle_section_, FALSE, FALSE, 0);

    active_section_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(active_section_, 14);
    gtk_widget_set_no_show_all(active_section_, TRUE);
    gtk_box_pack_start(GTK_BOX(active_section_), section_caption("<span size='smaller' weight='600' alpha='60%'>TIMER</span>"), FALSE, FALSE, 0);

    GtkWidget* transport = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(transport, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(transport, TRUE);

    pause_btn_ = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(pause_btn_), "cd-pomodoro-transport");
    {
        GtkWidget* prow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_valign(prow, GTK_ALIGN_CENTER);
        GtkWidget* pimg = gtk_image_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
        g_object_set_data(G_OBJECT(pause_btn_), "cd-pause-icon", pimg);
        GtkWidget* plab = gtk_label_new("Pause");
        g_object_set_data(G_OBJECT(pause_btn_), "cd-pause-label", plab);
        gtk_box_pack_start(GTK_BOX(prow), pimg, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(prow), plab, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(pause_btn_), prow);
        gtk_widget_show_all(pause_btn_);
    }
    gtk_widget_set_tooltip_text(pause_btn_, "Pause or resume");

    GtkWidget* stop_btn = gtk_button_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_widget_set_tooltip_text(stop_btn, "Stop timer");
    gtk_style_context_add_class(gtk_widget_get_style_context(stop_btn), "destructive-action");

    GtkWidget* skip_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(skip_btn), "cd-pomodoro-transport");
    {
        GtkWidget* srow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* simg = gtk_image_new_from_icon_name("media-seek-forward-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
        GtkWidget* slab = gtk_label_new("Skip");
        gtk_box_pack_start(GTK_BOX(srow), simg, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(srow), slab, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(skip_btn), srow);
        gtk_widget_show_all(skip_btn);
    }
    gtk_widget_set_tooltip_text(skip_btn, "Skip to the next phase");

    gtk_box_pack_start(GTK_BOX(transport), pause_btn_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(transport), stop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(transport), skip_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(active_section_), transport, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), active_section_, FALSE, FALSE, 0);

    g_signal_connect(dialog_, "delete-event", G_CALLBACK(delete_event_cb), this);

    g_signal_connect(target_combo_, "changed", G_CALLBACK(target_changed_cb), this);
    g_signal_connect(start_focus, "clicked", G_CALLBACK(start_focus_cb), this);
    g_signal_connect(start_short, "clicked", G_CALLBACK(start_short_break_cb), this);
    g_signal_connect(start_long, "clicked", G_CALLBACK(start_long_break_cb), this);
    g_signal_connect(pause_btn_, "clicked", G_CALLBACK(pause_cb), this);
    g_signal_connect(stop_btn, "clicked", G_CALLBACK(stop_cb), this);
    g_signal_connect(skip_btn, "clicked", G_CALLBACK(skip_cb), this);

    vm_.signal_pomodoro_state_changed.connect([this](PomodoroState const& state) { update(state); });
    gtk_widget_show_all(outer);
    // active_section_ deliberately ignores outer's show_all so it stays hidden while idle.
    // Realize all of its children once so making the section visible also shows its controls.
    gtk_widget_set_no_show_all(active_section_, FALSE);
    gtk_widget_show_all(active_section_);
    gtk_widget_set_no_show_all(active_section_, TRUE);
    reload_targets();
    update(vm_.pomodoro_state());
}

void PomodoroModal::present(GtkWindow* parent)
{
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent);
    reload_targets();
    vm_.set_pomodoro_modal_visible(true);
    gtk_widget_show_all(dialog_);
    gtk_window_present(GTK_WINDOW(dialog_));
}

void PomodoroModal::update(PomodoroState const& state)
{
    gtk_label_set_text(GTK_LABEL(phase_label_), pomodoro_phase_label(state.phase));

    char time_buf[16]{};
    fmt_time(state.seconds_left, time_buf, sizeof(time_buf));
    gtk_label_set_text(GTK_LABEL(time_label_), time_buf);

    int const n = std::max(1, state.settings.sessions_until_long_break);
    int const pos = (state.completed_sessions % n) + 1;
    char session_buf[48]{};
    std::snprintf(session_buf, sizeof(session_buf), "Round %d of %d", pos, n);
    gtk_label_set_text(GTK_LABEL(session_label_), session_buf);

    if (state.item_title.empty()) {
        gtk_widget_hide(task_label_);
    }
    else {
        if (gchar* esc = g_markup_escape_text(state.item_title.c_str(), -1)) {
            std::string const mk = "<span weight='600' alpha='90%'>" + std::string(esc) + "</span>";
            g_free(esc);
            gtk_label_set_markup(GTK_LABEL(task_label_), mk.c_str());
        }
        gtk_widget_show(task_label_);
    }

    GtkWidget* pimg = GTK_WIDGET(g_object_get_data(G_OBJECT(pause_btn_), "cd-pause-icon"));
    GtkWidget* plab = GTK_WIDGET(g_object_get_data(G_OBJECT(pause_btn_), "cd-pause-label"));
    if (state.running) {
        if (plab)
            gtk_label_set_text(GTK_LABEL(plab), "Pause");
        if (pimg)
            gtk_image_set_from_icon_name(GTK_IMAGE(pimg), "media-playback-pause-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    }
    else {
        if (plab)
            gtk_label_set_text(GTK_LABEL(plab), "Resume");
        if (pimg)
            gtk_image_set_from_icon_name(GTK_IMAGE(pimg), "media-playback-start-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    }

    gboolean const session_active = state.active;
    gtk_widget_set_visible(idle_section_, !session_active);
    gtk_widget_set_visible(active_section_, session_active);
}

void PomodoroModal::reload_targets()
{
    reloading_targets_ = true;
    targets_ = vm_.pomodoro_target_options();
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(target_combo_));
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(target_combo_), "timer:", "No linked task or issue");
    for (auto const& target : targets_)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(target_combo_), target.key.c_str(), target.display.c_str());

    std::string const selected = vm_.pomodoro_target_key();
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(target_combo_), selected.c_str()))
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(target_combo_), "timer:");
    reloading_targets_ = false;
}

void PomodoroModal::target_changed_cb(GtkComboBox* combo, gpointer user_data)
{
    auto* self = static_cast<PomodoroModal*>(user_data);
    if (self->reloading_targets_) return;
    char const* key = gtk_combo_box_get_active_id(combo);
    if (!key || std::string{key} == "timer:") {
        self->vm_.set_pomodoro_target("timer", "", "");
        return;
    }
    auto const it = std::find_if(self->targets_.begin(), self->targets_.end(),
        [key](PomodoroTargetOption const& option) { return option.key == key; });
    if (it != self->targets_.end())
        self->vm_.set_pomodoro_target(it->kind, it->id, it->title);
}

void PomodoroModal::start_focus_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::Work);
}

void PomodoroModal::start_short_break_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::ShortBreak);
}

void PomodoroModal::start_long_break_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.start_pomodoro(PomodoroPhase::LongBreak);
}

void PomodoroModal::pause_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.pause_or_resume_pomodoro();
}

void PomodoroModal::stop_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.stop_pomodoro();
}

void PomodoroModal::skip_cb(GtkButton*, gpointer user_data)
{
    static_cast<PomodoroModal*>(user_data)->vm_.skip_pomodoro_phase();
}

} // namespace cd
