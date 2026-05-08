#pragma once

#include <gtkmm/box.h>

#include <string>

typedef struct _WebKitWebView WebKitWebView;

namespace cd {

/** WebKit preview: cmark-gfm + GFM extensions; height follows document (parent supplies scroll). */
class MarkdownView final : public Gtk::Box {
public:
    MarkdownView();
    MarkdownView(MarkdownView const&) = delete;
    MarkdownView& operator=(MarkdownView const&) = delete;

    void load_markdown(std::string const& markdown);

private:
    static void load_changed_cb(WebKitWebView* wv, int load_event, gpointer user_data);
    static gboolean height_tick_impl(gpointer user_data);
    static void js_done_trampoline(GObject* web_view, GAsyncResult* res, gpointer user_data);

    void cancel_height_tick();
    void schedule_height_polish();
    void measure_height_once();

    WebKitWebView* webview_{};
    guint height_tick_id_{};
    int height_tick_count_{};
    int max_content_height_{};
};

} // namespace cd
