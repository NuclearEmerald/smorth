#define NOB_IMPLEMENTATION
#include "include/nob.h"

#define SRC "src/"
#define INCLUDE "include/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool debug = false;
    for(int i=0; i<argc; i++) if(strcmp(argv[i],"-dbg")==0) debug=true;

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_inputs(&cmd, SRC"main.c", SRC"codegen.c", SRC"interpreter.c", SRC"libforth.c");
    nob_cc_output(&cmd, "smorth");
    nob_cmd_append(&cmd, "-I"INCLUDE);
    if(debug) nob_cmd_append(&cmd, "-ggdb");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}