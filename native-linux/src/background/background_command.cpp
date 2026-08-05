#include "background_command.h"
#include <glib.h>

namespace cd {
bool expand_background_command(std::string const& command,std::string const& file,std::vector<std::string>& out,std::string& error){
    if(command.empty()){error="Background command is disabled";return false;}if(command.find("%f")==std::string::npos){error="Background command must contain %f";return false;}
    gint argc{};gchar** argv{};GError* e{};if(!g_shell_parse_argv(command.c_str(),&argc,&argv,&e)){error=e?e->message:"Invalid command";if(e)g_error_free(e);return false;}
    for(int i=0;i<argc;++i){std::string arg=argv[i];std::size_t p{};while((p=arg.find("%f",p))!=std::string::npos){arg.replace(p,2,file);p+=file.size();}out.push_back(std::move(arg));}g_strfreev(argv);
    if(out.empty()){error="Background command is empty";return false;}return true;
}
}
