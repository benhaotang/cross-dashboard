#include "markdown_view.h"

extern "C" {
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
}

#include <algorithm>
#include <cstring>
#include <string>

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR ""
#endif

namespace cd {

namespace {

constexpr char kHead[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
    "'width=device-width,initial-scale=1'/>"
    "<script>window.MathJax={tex:{displayMath:[[\"$$\",\"$$\"]],inlineMath:[]},options:{skipHtmlTags:"
    "['script','noscript','style','textarea','pre','code']},chtml:{matchFontHeight:true}};</script>"
    "<style>"
    "html,body{margin:0;padding:0;overflow-x:hidden;overflow-y:hidden;background:transparent;"
    "font-family:system-ui,\"Segoe UI\",Roboto,\"Helvetica Neue\",sans-serif;line-height:1.45;"
    "font-size:15px;-webkit-font-smoothing:antialiased;}"
    "a{color:#3584e4;}code,pre{font-family:ui-monospace,\"SF Mono\",Consolas,monospace;font-size:0.9em;}"
    "pre{padding:10px;overflow-x:auto;border-radius:6px;background:alpha(#888,0.08);}"
    "blockquote{margin:6px 0;padding-left:12px;border-left:3px solid alpha(#888,0.35);}"
    "table{border-collapse:collapse;margin:8px 0;width:100%;}"
    "th,td{border:1px solid alpha(#888,0.25);padding:6px 8px;text-align:left;}"
    "ul.contains-task-list{padding-left:1.2em;list-style-type:none;}"
    "li.task-list-item{position:relative;margin:0.2em 0;padding-left:0.1em;}"
    "li.task-list-item>input[type=checkbox]{margin-right:0.45em;vertical-align:-0.15em;pointer-events:none;}"
    "mjx-container{font-size:1.04em!important;line-height:0;}"
    "mjx-container[display=true]{display:block!important;text-align:center;margin:0.85em 0;font-size:1.08em!important;}"
    "</style></head><body>";

constexpr char kMathJaxEnd[] =
    "<script defer src=\"mathjax/tex-chtml.js\" id=\"MathJax-script\"></script>"
    "</body></html>";

std::string html_base_uri_for_mathjax()
{
    if (std::strlen(PACKAGE_DATADIR) > 0) {
        std::string const root(PACKAGE_DATADIR);
        gchar* mj = g_build_filename(PACKAGE_DATADIR, "mathjax", "tex-chtml.js", nullptr);
        if (g_file_test(mj, G_FILE_TEST_IS_REGULAR)) {
            gchar* uri = g_filename_to_uri(root.c_str(), nullptr, nullptr);
            g_free(mj);
            if (uri) {
                std::string o{uri};
                g_free(uri);
                if (!o.empty() && o.back() != '/')
                    o += '/';
                return o;
            }
        }
        g_free(mj);
    }
    gchar* base = g_build_filename(g_get_user_data_dir(), "cross-dashboard", nullptr);
    gchar* mj = g_build_filename(base, "mathjax", "tex-chtml.js", nullptr);
    if (g_file_test(mj, G_FILE_TEST_IS_REGULAR)) {
        gchar* uri = g_filename_to_uri(base, nullptr, nullptr);
        g_free(mj);
        g_free(base);
        if (uri) {
            std::string o{uri};
            g_free(uri);
            if (!o.empty() && o.back() != '/')
                o += '/';
            return o;
        }
    }
    g_free(mj);
    g_free(base);
    return {};
}

bool mathjax_available()
{
    return !html_base_uri_for_mathjax().empty();
}

std::string gfm_markdown_to_html(std::string const& markdown)
{
    cmark_gfm_core_extensions_ensure_registered();
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    char const* ext_names[] = {"tasklist", "table", "strikethrough", "autolink", "tagfilter"};
    for (char const* name : ext_names) {
        if (cmark_syntax_extension* ext = cmark_find_syntax_extension(name))
            cmark_parser_attach_syntax_extension(parser, ext);
    }
    cmark_parser_feed(parser, markdown.data(), markdown.size());
    cmark_node* doc = cmark_parser_finish(parser);
    char* raw = cmark_render_html(doc, CMARK_OPT_UNSAFE | CMARK_OPT_GITHUB_PRE_LANG, nullptr);
    cmark_node_free(doc);
    cmark_parser_free(parser);
    std::string out = raw ? raw : "";
    if (raw)
        free(raw);
    return out;
}

constexpr char kHeadNoMj[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
    "'width=device-width,initial-scale=1'/>"
    "<style>"
    "html,body{margin:0;padding:0;overflow-x:hidden;overflow-y:hidden;background:transparent;"
    "font-family:system-ui,\"Segoe UI\",Roboto,\"Helvetica Neue\",sans-serif;line-height:1.45;"
    "font-size:15px;-webkit-font-smoothing:antialiased;}"
    "a{color:#3584e4;}code,pre{font-family:ui-monospace,\"SF Mono\",Consolas,monospace;font-size:0.9em;}"
    "pre{padding:10px;overflow-x:auto;border-radius:6px;background:alpha(#888,0.08);}"
    "blockquote{margin:6px 0;padding-left:12px;border-left:3px solid alpha(#888,0.35);}"
    "table{border-collapse:collapse;margin:8px 0;width:100%;}"
    "th,td{border:1px solid alpha(#888,0.25);padding:6px 8px;text-align:left;}"
    "ul.contains-task-list{padding-left:1.2em;list-style-type:none;}"
    "li.task-list-item{position:relative;margin:0.2em 0;padding-left:0.1em;}"
    "li.task-list-item>input[type=checkbox]{margin-right:0.45em;vertical-align:-0.15em;pointer-events:none;}"
    "</style></head><body>";

} // namespace

MarkdownView::MarkdownView()
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
{
    webview_ = WEBKIT_WEB_VIEW(webkit_web_view_new());
    GtkWidget* w = GTK_WIDGET(webview_);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_widget_set_vexpand(w, FALSE);
    WebKitSettings* settings = webkit_web_view_get_settings(webview_);
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, FALSE);
    gtk_box_pack_start(GTK_BOX(gobj()), w, FALSE, FALSE, 0);
    g_signal_connect(webview_, "load-changed", G_CALLBACK(load_changed_cb), this);
}

void MarkdownView::cancel_height_tick()
{
    if (height_tick_id_ != 0) {
        g_source_remove(height_tick_id_);
        height_tick_id_ = 0;
    }
}

void MarkdownView::schedule_height_polish()
{
    cancel_height_tick();
    height_tick_count_ = 0;
    max_content_height_ = 0;
    height_tick_id_ = g_timeout_add(60, (GSourceFunc)&MarkdownView::height_tick_impl, this);
}

gboolean MarkdownView::height_tick_impl(gpointer user_data)
{
    auto* self = static_cast<MarkdownView*>(user_data);
    self->measure_height_once();
    self->height_tick_count_++;
    if (self->height_tick_count_ >= 15) {
        self->height_tick_id_ = 0;
        return FALSE;
    }
    return TRUE;
}

void MarkdownView::measure_height_once()
{
    char const* script =
        "Math.max(document.body.scrollHeight||0,document.documentElement.scrollHeight||0,"
        "document.body.offsetHeight||0,document.documentElement.offsetHeight||0)";

    webkit_web_view_run_javascript(webview_, script, nullptr, (GAsyncReadyCallback)&MarkdownView::js_done_trampoline,
        this);
}

void MarkdownView::js_done_trampoline(GObject* wv, GAsyncResult* res, gpointer user_data)
{
    auto* self = static_cast<MarkdownView*>(user_data);
    GError* err = nullptr;
    WebKitJavascriptResult* jr = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(wv), res, &err);
    if (!jr) {
        if (err)
            g_error_free(err);
        return;
    }
    JSCValue* val = webkit_javascript_result_get_js_value(jr);
    if (jsc_value_is_number(val)) {
        int const h = static_cast<int>(jsc_value_to_double(val));
        if (h > self->max_content_height_) {
            self->max_content_height_ = h;
            int const req = std::max(h + 8, 40);
            gtk_widget_set_size_request(GTK_WIDGET(self->webview_), -1, req);
        }
    }
    webkit_javascript_result_unref(jr);
}

void MarkdownView::load_changed_cb(WebKitWebView* wv, int load_event, gpointer user_data)
{
    (void)wv;
    if (load_event != WEBKIT_LOAD_FINISHED)
        return;
    auto* self = static_cast<MarkdownView*>(user_data);
    self->schedule_height_polish();
}

void MarkdownView::load_markdown(std::string const& markdown)
{
    if (!webview_)
        return;
    cancel_height_tick();
    max_content_height_ = 0;
    gtk_widget_set_size_request(GTK_WIDGET(webview_), -1, 120);

    std::string const body = gfm_markdown_to_html(markdown);
    std::string html;
    if (mathjax_available()) {
        html = std::string{kHead} + body + kMathJaxEnd;
        std::string const base = html_base_uri_for_mathjax();
        webkit_web_view_load_html(webview_, html.c_str(), base.c_str());
    }
    else {
        html = std::string{kHeadNoMj} + body + "</body></html>";
        webkit_web_view_load_html(webview_, html.c_str(), nullptr);
    }
}

} // namespace cd
