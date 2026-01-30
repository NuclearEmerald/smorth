#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool debug = false;
    for(int i=0; i<argc; i++) if(strcmp(argv[i],"-dbg")==0) debug=true;

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_inputs(&cmd, "main.c");
    nob_cc_output(&cmd, "smorth");
    if(debug) nob_cmd_append(&cmd, "-ggdb");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}