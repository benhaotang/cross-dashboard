#include "markdown_view.h"

extern "C" {
#include <cmark-gfm.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
}

#include <cstring>
#include <string>

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR ""
#endif

namespace cd {

namespace {

constexpr char kHead[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
    "'width=device-width'/>"
    "<script>window.MathJax="
    "{tex:{displayMath:[[\"$$\",\"$$\"]],inlineMath:[]},options:{skipHtmlTags:"
    "['script','noscript','style','textarea','pre','code']}};</script>"
    "<style>body{font-family:sans-serif;padding:12px;line-height:1.4;}</style></head><body>";

constexpr char kMathJaxEnd[] =
    "<script defer src=\"mathjax/tex-chtml.js\" id=\"MathJax-script\"></script>"
    "</body></html>";

std::string html_base_uri_for_mathjax()
{
#ifdef PACKAGE_DATADIR
    if (std::strlen(PACKAGE_DATADIR) > 0) {
        std::string const root(PACKAGE_DATADIR);
        gchar* mj = g_build_filename(PACKAGE_DATADIR, "mathjax", "tex-chtml.js", nullptr);
        if (g_file_test(mj, G_FILE_TEST_IS_REGULAR)) {
            gchar* uri = g_filename_to_uri(root.c_str(), nullptr, nullptr);
            g_free(mj);
            if (uri) {
                std::string o{uri};
                g_free(uri);
                if (!o.empty() && o.back() != '/') o += '/';
                return o;
            }
        }
        g_free(mj);
    }
#endif
    gchar* base = g_build_filename(g_get_user_data_dir(), "cross-dashboard", nullptr);
    gchar* mj = g_build_filename(base, "mathjax", "tex-chtml.js", nullptr);
    if (g_file_test(mj, G_FILE_TEST_IS_REGULAR)) {
        gchar* uri = g_filename_to_uri(base, nullptr, nullptr);
        g_free(mj);
        g_free(base);
        if (uri) {
            std::string o{uri};
            g_free(uri);
            if (!o.empty() && o.back() != '/') o += '/';
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

} // namespace

MarkdownView::MarkdownView()
    : Gtk::ScrolledWindow{}
{
    set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    webview_ = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(gobj()), GTK_WIDGET(webview_));
}

void MarkdownView::load_markdown(std::string const& markdown)
{
    if (!webview_) return;
    constexpr int opts = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE;
    char* raw = cmark_markdown_to_html(markdown.data(), markdown.size(), opts);
    std::string body = raw ? raw : "";
    if (raw) free(raw);

    std::string html;
    if (mathjax_available()) {
        html = std::string{kHead} + body + kMathJaxEnd;
        std::string const base = html_base_uri_for_mathjax();
        webkit_web_view_load_html(webview_, html.c_str(), base.c_str());
    }
    else {
        html = "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content="
               "'width=device-width'/>"
               "<style>body{font-family:sans-serif;padding:12px;line-height:1.4;}"
               "</style></head><body>"
            + body + "</body></html>";
        webkit_web_view_load_html(webview_, html.c_str(), nullptr);
    }
}

} // namespace cd
