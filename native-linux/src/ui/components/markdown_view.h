#pragma once

#include <gtkmm/box.h>

#include <string>

typedef struct _WebKitWebView WebKitWebView;

namespace cd {

/** WebKit preview: cmark-gfm + GFM extensions in a fixed, internally scrollable viewport. */
class MarkdownView final : public Gtk::Box {
public:
    enum class HeightMode {
        /** A detail whose markdown is the primary/only vertically expanding content. */
        Full,
        /** A detail with attachments, comments, or other content following the markdown. */
        WithFollowingContent,
        /** A markdown fragment embedded in a repeating row, such as a comment. */
        Embedded,
    };

    explicit MarkdownView(HeightMode height_mode = HeightMode::Full);
    ~MarkdownView() = default;
    MarkdownView(MarkdownView const&) = delete;
    MarkdownView& operator=(MarkdownView const&) = delete;

    void load_markdown(std::string const& markdown);

private:
    WebKitWebView* webview_{};
};

} // namespace cd
