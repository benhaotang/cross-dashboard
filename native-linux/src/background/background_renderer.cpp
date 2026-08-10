#include "background_renderer.h"

#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <span>

namespace cd {
namespace {
constexpr int W = 3840, H = 2160, BLUR_W = 192, BLUR_H = 108;

void color(cairo_t* c, unsigned rgb) { cairo_set_source_rgb(c, ((rgb >> 16) & 255) / 255.0, ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0); }
void text(cairo_t* c, char const* font, double size, unsigned rgb, double x, double y,
    std::string const& value, int width) {
    auto* layout = pango_cairo_create_layout(c); auto* description = pango_font_description_from_string(font);
    pango_font_description_set_absolute_size(description, size * PANGO_SCALE);
    pango_layout_set_font_description(layout, description); pango_layout_set_text(layout, value.c_str(), -1);
    pango_layout_set_width(layout, width * PANGO_SCALE); pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    color(c, rgb); cairo_move_to(c, x, y); pango_cairo_show_layout(c, layout);
    pango_font_description_free(description); g_object_unref(layout);
}
void rounded(cairo_t* c, double x, double y, double width, double height, double radius) {
    constexpr double k = 0.5522847498; double r = std::min({radius, width / 2, height / 2});
    cairo_new_sub_path(c); cairo_move_to(c, x + r, y); cairo_line_to(c, x + width - r, y);
    cairo_curve_to(c, x + width - r + r * k, y, x + width, y + r - r * k, x + width, y + r);
    cairo_line_to(c, x + width, y + height - r);
    cairo_curve_to(c, x + width, y + height - r + r * k, x + width - r + r * k, y + height, x + width - r, y + height);
    cairo_line_to(c, x + r, y + height);
    cairo_curve_to(c, x + r - r * k, y + height, x, y + height - r + r * k, x, y + height - r);
    cairo_line_to(c, x, y + r); cairo_curve_to(c, x, y + r - r * k, x + r - r * k, y, x + r, y); cairo_close_path(c);
}

struct RenderContext {
    bool dark{}; double opacity{0.8}; cairo_surface_t* blur{};
    unsigned text{}, secondary{}, accent{0xF2B35D};
};

void glass(cairo_t* c, RenderContext const& rc, double x, double y, double width, double height, double radius) {
    cairo_save(c); rounded(c, x, y, width, height, radius); cairo_clip(c);
    if (rc.blur) {
        cairo_scale(c, double(W) / BLUR_W, double(H) / BLUR_H);
        cairo_set_source_surface(c, rc.blur, 0, 0); cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_BILINEAR); cairo_paint(c);
    }
    if (rc.dark) cairo_set_source_rgba(c, 0.02, 0.035, 0.06, rc.opacity);
    else cairo_set_source_rgba(c, 1, 1, 1, rc.opacity);
    cairo_paint(c); cairo_restore(c);
}

cairo_surface_t* load_backdrop(cairo_t* target, BackgroundAppearance const& appearance, unsigned fallback, std::string& error) {
    if (appearance.image_path.empty()) return nullptr;
    GError* load_error{}; auto* image = gdk_pixbuf_new_from_file(appearance.image_path.c_str(), &load_error);
    if (!image) { error = load_error ? load_error->message : "Could not load backdrop picture"; if (load_error) g_error_free(load_error); return nullptr; }
    if (auto* oriented = gdk_pixbuf_apply_embedded_orientation(image)) { g_object_unref(image); image = oriented; }
    auto* photo = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H); auto* pc = cairo_create(photo);
    color(pc, fallback); cairo_paint(pc);
    int iw = gdk_pixbuf_get_width(image), ih = gdk_pixbuf_get_height(image); double dw = W, dh = H, x = 0, y = 0;
    if (appearance.image_fit != "stretch") {
        double sx = double(W) / iw, sy = double(H) / ih;
        double scale = appearance.image_fit == "scale" ? std::min(sx, sy) : std::max(sx, sy);
        dw = iw * scale; dh = ih * scale; x = (W - dw) / 2; y = (H - dh) / 2;
    }
    auto* scaled = gdk_pixbuf_scale_simple(image, std::max(1, int(std::round(dw))), std::max(1, int(std::round(dh))), GDK_INTERP_BILINEAR);
    g_object_unref(image);
    if (!scaled) { cairo_destroy(pc); cairo_surface_destroy(photo); error = "Could not scale backdrop picture"; return nullptr; }
    gdk_cairo_set_source_pixbuf(pc, scaled, x, y); cairo_paint(pc); g_object_unref(scaled); cairo_destroy(pc);
    cairo_set_source_surface(target, photo, 0, 0); cairo_paint(target);
    auto* blur = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, BLUR_W, BLUR_H); auto* bc = cairo_create(blur);
    cairo_scale(bc, double(BLUR_W) / W, double(BLUR_H) / H); cairo_set_source_surface(bc, photo, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(bc), CAIRO_FILTER_BEST); cairo_paint(bc); cairo_destroy(bc); cairo_surface_destroy(photo);
    return blur;
}

void magic_tag(cairo_t* c, RenderContext const& rc, double x, double y, double width, std::string const& value) {
    auto r = ((rc.accent >> 16) & 255) / 255.0, g = ((rc.accent >> 8) & 255) / 255.0, b = (rc.accent & 255) / 255.0;
    cairo_set_source_rgba(c, r, g, b, 0.2); rounded(c, x, y, width, 46, 23); cairo_fill(c);
    text(c, "Sans Bold", 24, rc.accent, x + 14, y + 8, "#" + value, width - 28);
}
void board_panel(cairo_t* c, RenderContext const& rc, BackgroundContent const& content,
    std::string const& group, double x, double y, double width, double height) {
    glass(c, rc, x, y, width, height, 26); magic_tag(c, rc, x + 20, y + 18, width - 90, group);
    std::vector<BackgroundRow const*> rows; for (auto const& row : content.rows) if (row.group == group) rows.push_back(&row);
    text(c, "Sans", 24, rc.secondary, x + width - 52, y + 27, std::to_string(rows.size()), 35);
    int available = std::max(1, int((height - 105) / 92)); double row_y = y + 82;
    for (int i = 0; i < std::min<int>(available, rows.size()); ++i) {
        auto const& row = *rows[i]; cairo_set_source_rgba(c, rc.dark ? .13 : .82, rc.dark ? .20 : .86, rc.dark ? .30 : .92, .34);
        rounded(c, x + 16, row_y, width - 32, 76, 14); cairo_fill(c);
        text(c, "Sans Semi-Bold", 27, rc.text, x + 30, row_y + 10, row.title, width - 60);
        text(c, "Sans", 19, rc.secondary, x + 30, row_y + 45, row.subtitle, width - 60); row_y += 88;
    }
    if (int(rows.size()) > available) text(c, "Sans", 20, rc.secondary, x + 20, y + height - 38,
        "+" + std::to_string(rows.size() - available) + " more", width - 40);
}
}

bool render_background_png(BackgroundContent const& v, BackgroundAppearance const& appearance,
    std::string const& path, std::string& error) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path()); std::string tmp = path + ".tmp";
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H); cairo_t* c = cairo_create(surface);
    RenderContext rc{appearance.dark, std::clamp(appearance.glass_opacity, 0.5, 1.0), nullptr,
        appearance.dark ? 0xEAF0F7u : 0x172235u, appearance.dark ? 0x9AABC2u : 0x52647Au};
    unsigned background = appearance.dark ? 0x000000 : 0xFFFFFF; color(c, background); cairo_paint(c);
    rc.blur = load_backdrop(c, appearance, background, error);
    if (!appearance.image_path.empty() && !rc.blur) { cairo_destroy(c); cairo_surface_destroy(surface); return false; }
    glass(c, rc, 230, 135, 3380, 245, 30);
    auto now = std::time(nullptr); char stamp[16]{}; std::strftime(stamp, sizeof(stamp), "%H:%M", std::localtime(&now));
    text(c, "Sans Bold", 112, rc.text, 255, 170, v.title, 2550);
    text(c, "Sans", 38, rc.secondary, 255, 310, v.filters + (v.title == "VIEWS" ? " · " + v.mode : "") + " · " + std::to_string(v.rows.size()) + " visible", 2750);
    text(c, "Sans", 30, rc.secondary, 3155, 185, std::string("UPDATED ") + stamp, 430);
    if (v.title == "VIEWS" && !v.groups.empty()) {
        if (v.mode == "covey") { double gap = 24, cell_w = (3380 - gap) / 2, cell_h = (1570 - gap) / 2; for (std::size_t i = 0; i < std::min<std::size_t>(4, v.groups.size()); ++i) board_panel(c, rc, v, v.groups[i], 230 + (i % 2) * (cell_w + gap), 430 + (i / 2) * (cell_h + gap), cell_w, cell_h); }
        else { std::size_t count = std::min<std::size_t>(7, v.groups.size()); double gap = 18, col_w = (3380 - gap * (count - 1)) / count; for (std::size_t i = 0; i < count; ++i) board_panel(c, rc, v, v.groups[i], 230 + i * (col_w + gap), 430, col_w, 1570); }
    } else {
        int y = 430; auto rows = v.rows.size() > 12 ? std::span(v.rows.data(), 12) : std::span(v.rows.data(), v.rows.size());
        if (rows.empty()) text(c, "Sans", 48, rc.text, 250, 520, "Nothing matches this snapshot", 2800);
        for (auto const& row : rows) {
            glass(c, rc, 230, y, 3380, 115, 18); unsigned accent = row.overdue ? 0xF27C7C : rc.accent;
            color(c, accent); rounded(c, 230, y, 9, 115, 4); cairo_fill(c);
            text(c, "Sans Semi-Bold", 42, rc.text, 275, y + 18, row.title, 2450);
            text(c, "Sans", 27, rc.secondary, 275, y + 71, (row.group.empty() ? "" : row.group + " · ") + row.subtitle, 2450); y += 132;
        }
        std::string footer; if (v.rows.size() > rows.size()) footer = "+" + std::to_string(v.rows.size() - rows.size()) + " more";
        if (v.total_minutes > 0) { if (!footer.empty()) footer += " · "; footer += std::to_string(v.total_minutes / 60) + "h " + std::to_string(v.total_minutes % 60) + "m estimated"; }
        if (!footer.empty()) text(c, "Sans", 32, rc.secondary, 235, H - 150, footer, 1600);
    }
    cairo_status_t status = cairo_surface_write_to_png(surface, tmp.c_str()); if (rc.blur) cairo_surface_destroy(rc.blur);
    cairo_destroy(c); cairo_surface_destroy(surface); if (status != CAIRO_STATUS_SUCCESS) { error = cairo_status_to_string(status); return false; }
    std::error_code ec; std::filesystem::rename(tmp, path, ec); if (ec) { error = ec.message(); return false; } return true;
}
} // namespace cd
