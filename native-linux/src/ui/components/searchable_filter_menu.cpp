#include "searchable_filter_menu.h"

#include <algorithm>
#include <cctype>
#include <gtkmm/radiobutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

namespace cd {

namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string icon_for_filter(std::string const& title)
{
    if (title == "Tags") return "tag-symbolic";
    if (title == "Time range") return "x-office-calendar-symbolic";
    if (title == "Type") return "view-list-symbolic";
    if (title == "Status") return "emblem-default-symbolic";
    if (title == "Milestone") return "flag-symbolic";
    return "view-more-symbolic";
}

std::string icon_for_choice(std::string const& title, std::string key)
{
    key = lower(std::move(key));
    if (title == "Status") {
        if (key == "open") return "emblem-default-symbolic";
        if (key == "closed" || key == "completed") return "object-select-symbolic";
        if (key == "archived") return "mail-archive-symbolic";
        if (key == "active" || key == "normal") return "media-playback-start-symbolic";
        if (key == "all") return "view-list-symbolic";
    }
    if (title == "Type") {
        if (key == "events") return "x-office-calendar-symbolic";
        if (key == "tasks") return "checkbox-checked-symbolic";
        if (key == "issues") return "dialog-warning-symbolic";
        if (key == "all") return "view-grid-symbolic";
    }
    if (title == "Time range") {
        if (key == "today") return "x-office-calendar-symbolic";
        if (key == "tomorrow") return "appointment-soon-symbolic";
        if (key == "week") return "view-calendar-week-symbolic";
        if (key == "all") return "view-refresh-symbolic";
    }
    if (title == "Milestone") return "flag-symbolic";
    return {};
}

} // namespace

SearchableFilterMenu::SearchableFilterMenu(std::string title, bool multi_select, bool searchable)
    : title_(std::move(title))
    , multi_select_(multi_select)
    , searchable_(searchable)
{
    set_label(title_);
    set_image_from_icon_name(icon_for_filter(title_), Gtk::ICON_SIZE_MENU);
    set_always_show_image(true);
    set_image_position(Gtk::POS_LEFT);
    set_popover(popover_);
    content_.set_border_width(12);
    content_.set_size_request(360, -1);

    search_.set_placeholder_text("Search " + lower(title_));
    search_.signal_changed().connect(sigc::mem_fun(*this, &SearchableFilterMenu::apply_search));
    if (searchable_) content_.pack_start(search_, false, false);

    list_.set_selection_mode(Gtk::SELECTION_NONE);
    scroll_.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scroll_.set_min_content_height(220);
    scroll_.set_max_content_height(420);
    scroll_.add(list_);
    content_.pack_start(scroll_, true, true);
    popover_.add(content_);
    content_.show_all();
}

void SearchableFilterMenu::set_options(
    std::vector<std::pair<std::string, std::string>> options)
{
    options_ = std::move(options);
    rebuild_rows();
    update_button_label();
}

void SearchableFilterMenu::set_selected(std::set<std::string> selected)
{
    selected_ = std::move(selected);
    if (!default_selected_) default_selected_ = selected_;
    rebuild_rows();
    update_button_label();
}

void SearchableFilterMenu::rebuild_rows()
{
    rebuilding_ = true;
    for (Gtk::Widget* child : list_.get_children()) list_.remove(*child);
    for (auto const& [key, label] : options_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow());
        Gtk::ToggleButton* choice = multi_select_
            ? static_cast<Gtk::ToggleButton*>(Gtk::manage(new Gtk::CheckButton()))
            : static_cast<Gtk::ToggleButton*>(Gtk::manage(new Gtk::RadioButton()));
        auto* choice_content = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
        std::string const icon_name = icon_for_choice(title_, key);
        if (!icon_name.empty()) {
            auto* icon = Gtk::manage(new Gtk::Image(icon_name, Gtk::ICON_SIZE_MENU));
            choice_content->pack_start(*icon, false, false);
        }
        auto* choice_label = Gtk::manage(new Gtk::Label(label));
        choice_label->set_halign(Gtk::ALIGN_START);
        choice_content->pack_start(*choice_label, true, true);
        choice->add(*choice_content);
        choice->set_active(selected_.contains(key));
        choice->signal_toggled().connect([this, key, choice] {
            if (rebuilding_) return;
            if (multi_select_) {
                if (choice->get_active()) selected_.insert(key);
                else selected_.erase(key);
            }
            else {
                if (!choice->get_active()) return;
                selected_.clear();
                selected_.insert(key);
                rebuild_rows();
                popover_.popdown();
            }
            update_button_label();
            signal_selection_changed.emit(selected_);
        });
        g_object_set_data_full(G_OBJECT(row->gobj()), "cd-filter-label",
            g_strdup(lower(label).c_str()), g_free);
        row->add(*choice);
        list_.append(*row);
    }
    rebuilding_ = false;
    list_.show_all();
    apply_search();
}

void SearchableFilterMenu::update_button_label()
{
    if (selected_.empty()) {
        set_label(title_);
    }
    else if (selected_.size() == 1) {
        auto const found = std::find_if(options_.begin(), options_.end(), [this](auto const& option) {
            return option.first == *selected_.begin();
        });
        if (found != options_.end()) {
            set_label(title_ + " – " + found->second);
        }
    }
    else {
        set_label(title_ + " – " + std::to_string(selected_.size()) + " selected");
    }

    auto* context = gtk_widget_get_style_context(GTK_WIDGET(gobj()));
    bool const active = default_selected_.has_value() && selected_ != *default_selected_;
    if (active) gtk_style_context_add_class(context, "cd-filter-active");
    else gtk_style_context_remove_class(context, "cd-filter-active");
}

void SearchableFilterMenu::apply_search()
{
    std::string const query = lower(search_.get_text());
    for (Gtk::Widget* child : list_.get_children()) {
        char const* label = static_cast<char const*>(
            g_object_get_data(G_OBJECT(child->gobj()), "cd-filter-label"));
        child->set_visible(query.empty() || (label && std::string(label).find(query) != std::string::npos));
    }
}

} // namespace cd
