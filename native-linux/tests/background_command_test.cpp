#include "background/background_command.h"
#include <cassert>
#include <string>
#include <vector>

int main()
{
    std::vector<std::string> args; std::string error;
    assert(cd::expand_background_command("tool --image %f", "/tmp/a b.png", args, error));
    assert(args.size() == 3 && args[2] == "/tmp/a b.png");
    args.clear(); error.clear();
    assert(cd::expand_background_command("tool --label 'daily board' %f", "/tmp/bg.png", args, error));
    assert(args.size() == 4 && args[2] == "daily board" && args[3] == "/tmp/bg.png");
    args.clear(); error.clear();
    assert(!cd::expand_background_command("tool --default", "/tmp/bg.png", args, error));
    args.clear(); error.clear();
    assert(cd::expand_background_command("tool %f | ignored", "/tmp/bg.png", args, error));
    assert(args[2] == "|" && args[3] == "ignored"); // argv, never a shell pipeline
}
