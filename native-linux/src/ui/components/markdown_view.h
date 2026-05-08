#pragma once

#include <gtkmm/scrolledwindow.h>

#include <string>

typedef struct _WebKitWebView WebKitWebView;

namespace cd {

/** WebKit-backed preview; Markdown via cmark-gfm (MathJax wired in Phase 3 bundle). */
class MarkdownView final : public Gtk::ScrolledWindow {
public:
    MarkdownView();
    MarkdownView(MarkdownView const&) = delete;
    MarkdownView& operator=(MarkdownView const&) = delete;

    void load_markdown(std::string const& markdown);

private:
    WebKitWebView* webview_{};
};

} // namespace cd
