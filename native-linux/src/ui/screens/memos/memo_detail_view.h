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
    std::function<void()> on_copy_link;
    /** Called after archive/delete/restore/edit/comment so the list can sync. */
    std::function<void()> on_memo_changed;

private:
    void rebuild();
    void set_action_sensitivity();

    AppContainer& app_;
    std::optional<MemosMemo> memo_;
    Gtk::Box toolbar_;
    Gtk::Button extract_tasks_btn_{"Extract Tasks"};
    Gtk::Button create_event_btn_{"Create Event"};
    Gtk::Button comment_issue_btn_{"Comment on Issue"};
    Gtk::Button open_url_btn_{"Open URL"};
    Gtk::Button copy_link_btn_{"Copy Link"};
    Gtk::Button edit_btn_{"Edit"};
    Gtk::Button archive_btn_{"Archive"};
    Gtk::Button restore_btn_{"Restore"};
    Gtk::Button delete_btn_{"Delete"};
    Gtk::Label title_;
    Gtk::Label meta_;
    Gtk::ScrolledWindow body_scroll_;
    MarkdownView* markdown_{};
    Gtk::FlowBox attachments_;
    Gtk::Label comments_title_;
    Gtk::ListBox comments_;

    Gtk::Box composer_{Gtk::ORIENTATION_HORIZONTAL, 6};
    Gtk::Entry comment_entry_;
    Gtk::Button comment_send_btn_{"Send"};
};

} // namespace cd
