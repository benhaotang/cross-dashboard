#include "application.h"

extern "C" {
#include <handy.h>
}

int main(int argc, char* argv[])
{
    hdy_init();
    auto app = cd::CdApplication::create();
    return app->run(argc, argv);
}
