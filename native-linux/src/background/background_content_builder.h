#pragma once
#include "background/background_definition.h"
#include <string>
#include <vector>

namespace cd {
class AppContainer;
struct BackgroundRow { std::string title, subtitle, group; int kind{}; bool overdue{}; };
struct BackgroundContent { std::string title, filters, mode; std::vector<std::string> groups; std::vector<BackgroundRow> rows; int total_minutes{}; };
BackgroundContent build_background_content(AppContainer&, BackgroundTemplate const&);
}
