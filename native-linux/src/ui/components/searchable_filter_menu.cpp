#include "searchable_filter_menu.h"

#include <algorithm>
#include <cctype>

namespace cd {

namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

SearchableFilterMenu::SearchableFilterMenu(std::string title, bool multi_select, bool searchable)
    : title_(std::move(title))
    , multi_select_(multi_select)
    , searchable_(searchable)
{
    set_label(title_);
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
        auto* choice = Gtk::manage(new Gtk::CheckButton(label));
        choice->set_active(selected_.contains(key));
        choice->signal_toggled().connect([this, key, choice] {
            if (rebuilding_) return;
            if (multi_select_) {
                if (choice->get_active()) selected_.insert(key);
                else selected_.erase(key);
            }
            else {
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
