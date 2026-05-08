#pragma once

#include <glib.h>
#include <gtkmm/box.h>

#include <string>
#include <vector>

typedef struct _WebKitWebView WebKitWebView;

namespace cd {

/** WebKit preview: cmark-gfm + GFM extensions; height follows document (parent supplies scroll). */
class MarkdownView final : public Gtk::Box {
public:
    MarkdownView();
    ~MarkdownView();
    MarkdownView(MarkdownView const&) = delete;
    MarkdownView& operator=(MarkdownView const&) = delete;

    void load_markdown(std::string const& markdown);

private:
    friend gboolean markdown_view_height_cb(gpointer user_data);
    friend void markdown_measure_js_done(GObject* web_view, GAsyncResult* res, gpointer user_data);

    static void load_changed_cb(WebKitWebView* wv, int load_event, gpointer user_data);

    void cancel_height_polish();
    void schedule_height_polish();
    void measure_height_once();

    WebKitWebView* webview_{};
    std::vector<unsigned> height_timeout_ids_{};
    int max_content_height_{};
    int height_floor_{120};
    unsigned load_generation_{0};
};

} // namespace cd
