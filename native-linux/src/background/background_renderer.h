#pragma once
#include "background/background_content_builder.h"
#include <string>
namespace cd { bool render_background_png(BackgroundContent const&, std::string const& path, std::string& error); }
