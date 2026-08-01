#include "markdown_view.h"

extern "C" {
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
}

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR ""
#endif

namespace cd {

namespace {

constexpr char kHead[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
    "'width=device-width,initial-scale=1'/>"
    "<script>window.MathJax={tex:{displayMath:[[\"$$\",\"$$\"]],inlineMath:[]},options:{skipHtmlTags:"
    "['script','noscript','style','textarea','pre','code']},chtml:{fontURL:\"mathjax/output/chtml/fonts/woff-v2\","
    "matchFontHeight:false}};</script>"
    "<style>"
    "html,body{margin:0;padding:0;overflow-x:hidden;overflow-y:auto;background:transparent;"
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
    ".cd-mj-display{display:block;text-align:center;margin:0.7em 0;}"
    "</style></head><body>";

constexpr char kMathJaxEnd[] =
    "<script defer src=\"mathjax/tex-chtml.js\" id=\"MathJax-script\"></script>"
    "</body></html>";

bool mathjax_bundle_complete(char const* crossdashboard_root)
{
    if (!crossdashboard_root || !*crossdashboard_root)
        return false;
    gchar* mj = g_build_filename(crossdashboard_root, "mathjax", "tex-chtml.js", nullptr);
    gchar* wf = g_build_filename(crossdashboard_root, "mathjax", "output", "chtml", "fonts", "woff-v2",
        "MathJax_Math-Italic.woff", nullptr);
    bool const ok = g_file_test(mj, G_FILE_TEST_IS_REGULAR) && g_file_test(wf, G_FILE_TEST_IS_REGULAR);
    g_free(mj);
    g_free(wf);
    return ok;
}

std::string html_base_uri_for_mathjax()
{
    if (std::strlen(PACKAGE_DATADIR) > 0) {
        std::string const root(PACKAGE_DATADIR);
        if (mathjax_bundle_complete(PACKAGE_DATADIR)) {
            gchar* uri = g_filename_to_uri(root.c_str(), nullptr, nullptr);
            if (uri) {
                std::string o{uri};
                g_free(uri);
                if (!o.empty() && o.back() != '/')
                    o += '/';
                return o;
            }
        }
    }
    gchar* base = g_build_filename(g_get_user_data_dir(), "cross-dashboard", nullptr);
    if (mathjax_bundle_complete(base)) {
        gchar* uri = g_filename_to_uri(base, nullptr, nullptr);
        g_free(base);
        if (uri) {
            std::string o{uri};
            g_free(uri);
            if (!o.empty() && o.back() != '/')
                o += '/';
            return o;
        }
    }
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

int find_closing_dollar(std::string const& content, std::size_t start_index, bool is_display)
{
    std::size_t index = start_index;
    while (index < content.size()) {
        if (content[index] == '\\' && index + 1 < content.size() && content[index + 1] == '$') {
            index += 2;
            continue;
        }
        if (content[index] != '$') {
            index += 1;
            continue;
        }
        if (is_display) {
            if (index + 1 < content.size() && content[index + 1] == '$')
                return static_cast<int>(index);
            index += 1;
            continue;
        }
        if (index + 1 < content.size() && content[index + 1] == '$') {
            index += 2;
            continue;
        }
        return static_cast<int>(index);
    }
    return -1;
}

std::string escape_inline_latex_for_md(std::string const& latex)
{
    std::string out = "\\$";
    for (char c : latex) {
        if (c == '_' || c == '*' || c == '[' || c == ']' || c == '~')
            out += '\\';
        out += c;
    }
    out += "\\$";
    return out;
}

std::string html_escape_lt_amp(std::string const& s)
{
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s) {
        if (c == '&')
            o += "&amp;";
        else if (c == '<')
            o += "&lt;";
        else
            o += static_cast<char>(c);
    }
    return o;
}

enum class SplitSegKind { Markdown, DisplayMath };

struct SplitSegment {
    SplitSegKind kind{};
    std::string text;
};

std::vector<SplitSegment> split_latex_segments(std::string const& content)
{
    std::vector<SplitSegment> segments;
    std::string markdown;
    std::size_t index = 0;

    auto flush_md = [&] {
        if (!markdown.empty()) {
            segments.push_back({SplitSegKind::Markdown, std::move(markdown)});
            markdown.clear();
        }
    };

    while (index < content.size()) {
        char const current = content[index];
        if (current == '\\' && index + 1 < content.size() && content[index + 1] == '$') {
            markdown += '$';
            index += 2;
            continue;
        }
        if (current != '$') {
            markdown += current;
            index += 1;
            continue;
        }

        bool const is_display = index + 1 < content.size() && content[index + 1] == '$';
        if (!is_display) {
            int const closing = find_closing_dollar(content, index + 1, false);
            if (closing < 0) {
                markdown += '$';
                index += 1;
                continue;
            }
            std::string inline_latex =
                content.substr(index + 1, static_cast<std::size_t>(closing) - (index + 1));
            markdown += escape_inline_latex_for_md(inline_latex);
            index = static_cast<std::size_t>(closing) + 1;
            continue;
        }

        int const closing_disp = find_closing_dollar(content, index + 2, true);
        if (closing_disp < 0) {
            markdown += '$';
            markdown += '$';
            index += 2;
            continue;
        }

        std::string latex =
            content.substr(index + 2, static_cast<std::size_t>(closing_disp) - (index + 2));
        bool blank = true;
        for (char c : latex) {
            if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')) {
                blank = false;
                break;
            }
        }
        if (blank) {
            markdown += "$$";
            index += 2;
            continue;
        }

        flush_md();
        segments.push_back({SplitSegKind::DisplayMath, std::move(latex)});
        index = static_cast<std::size_t>(closing_disp) + 2;
    }
    flush_md();
    return segments;
}

std::string markdown_with_display_math_to_html(std::string const& markdown)
{
    auto segs = split_latex_segments(markdown);
    std::string html;
    for (auto const& seg : segs) {
        if (seg.kind == SplitSegKind::Markdown) {
            if (!seg.text.empty())
                html += gfm_markdown_to_html(seg.text);
        }
        else {
            html += "<div class=\"cd-mj-display\">$$";
            html += html_escape_lt_amp(seg.text);
            html += "$$</div>";
        }
    }
    return html;
}

constexpr char kHeadNoMj[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
    "'width=device-width,initial-scale=1'/>"
    "<style>"
    "html,body{margin:0;padding:0;overflow-x:hidden;overflow-y:auto;background:transparent;"
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

MarkdownView::MarkdownView(HeightMode height_mode)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
{
    constexpr int kFullHeight = 520;
    constexpr int kFollowingContentHeight = kFullHeight * 4 / 5;
    constexpr int kEmbeddedHeight = 160;
    int const viewport_height = height_mode == HeightMode::Full
        ? 120
        : height_mode == HeightMode::WithFollowingContent ? kFollowingContentHeight : kEmbeddedHeight;
    bool const fills_detail = height_mode == HeightMode::Full;

    webview_ = WEBKIT_WEB_VIEW(webkit_web_view_new());
    GtkWidget* w = GTK_WIDGET(webview_);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_widget_set_vexpand(w, fills_detail ? TRUE : FALSE);
    gtk_widget_set_size_request(w, -1, viewport_height);
    WebKitSettings* settings = webkit_web_view_get_settings(webview_);
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, FALSE);
    gtk_box_pack_start(GTK_BOX(gobj()), w, fills_detail ? TRUE : FALSE, fills_detail ? TRUE : FALSE, 0);
}

void MarkdownView::load_markdown(std::string const& markdown)
{
    if (!webview_)
        return;
    webkit_web_view_stop_loading(webview_);

    std::string const body = markdown_with_display_math_to_html(markdown);
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
