#pragma once
#include "background/background_content_builder.h"
#include <string>
namespace cd {
struct BackgroundAppearance {
    std::string image_path;
    std::string image_fit{"fill"};
    double glass_opacity{0.8};
    bool dark{true};
};
bool render_background_png(BackgroundContent const&, BackgroundAppearance const&, std::string const& path, std::string& error);
}
