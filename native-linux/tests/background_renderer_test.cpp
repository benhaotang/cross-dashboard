#include "background/background_renderer.h"

#include <cassert>
#include <cairo.h>
#include <filesystem>

int main()
{
    auto directory = std::filesystem::temp_directory_path() / "crossdashboard-background-renderer-test";
    std::filesystem::create_directories(directory);
    auto source = directory / "source.png";
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 96, 64);
    auto* context = cairo_create(surface);
    cairo_set_source_rgb(context, 0.15, 0.45, 0.75); cairo_paint(context);
    assert(cairo_surface_write_to_png(surface, source.c_str()) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(context); cairo_surface_destroy(surface);

    cd::BackgroundContent content{"INBOX", "Tasks · Today", "", {},
        {{"Review release", "Today 16:00", "", 1, false}}, 30};
    cd::BackgroundAppearance appearance{source.string(), "fill", 0.8, true};
    std::string error; auto output = directory / "background.png";
    assert(cd::render_background_png(content, appearance, output.string(), error));
    assert(std::filesystem::file_size(output) > 1'000);

    appearance.image_path.clear(); appearance.dark = false; appearance.image_fit = "stretch";
    assert(cd::render_background_png(content, appearance, output.string(), error));
    std::filesystem::remove_all(directory);
}
