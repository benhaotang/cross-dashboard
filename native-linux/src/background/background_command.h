#pragma once
#include <string>
#include <vector>
namespace cd { bool expand_background_command(std::string const& command, std::string const& file, std::vector<std::string>& argv, std::string& error); }
