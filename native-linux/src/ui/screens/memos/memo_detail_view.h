#pragma once

#include "domain/models.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include <functional>
#include <optional>

namespace cd {

class AppContainer;
class MarkdownView;

class MemoDetailView final : public Gtk::Box {
public:
    explicit MemoDetailView(AppContainer& app);

    void set_memo(std::optional<MemosMemo> memo);
    [[nodiscard]] std::optional<MemosMemo> selected_memo() const { return memo_; }

    std::function<void()> on_extract_tasks;
    std::function<void()> on_create_event;
    std::function<void()> on_comment_issue;
    std::function<void()> on_open_url;
    std::function<void()> on_save_karakeep;
    std::function<void()> on_copy_link;
    /** Called after archive/delete/restore/edit/comment so the list can sync. */
    std::function<void()> on_memo_changed;

private:
    void rebuild();
    void set_action_sensitivity();

    AppContainer& app_;
    std::optional<MemosMemo> memo_;
    Gtk::Box toolbar_;
    Gtk::Button extract_tasks_btn_{};
    Gtk::Button create_event_btn_{};
    Gtk::Button comment_issue_btn_{};
    Gtk::Button open_url_btn_{};
    Gtk::Button save_karakeep_btn_{};
    Gtk::Button copy_link_btn_{};
    Gtk::Button edit_btn_{};
    Gtk::Button archive_btn_{};
    Gtk::Button restore_btn_{};
    Gtk::Button delete_btn_{};
    Gtk::Label title_;
    Gtk::Label meta_;
    Gtk::ScrolledWindow content_scroll_;
    Gtk::Box content_column_{Gtk::ORIENTATION_VERTICAL, 6};
    MarkdownView* markdown_{};
    Gtk::FlowBox attachments_;
    Gtk::Label comments_title_;
    Gtk::ListBox comments_;

    Gtk::Box composer_{Gtk::ORIENTATION_HORIZONTAL, 6};
    Gtk::Entry comment_entry_;
    Gtk::Button comment_send_btn_{};
};

} // namespace cd
