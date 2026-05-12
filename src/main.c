#define NOB_IMPLEMENTATION
#include <nob.h>
#undef NOB_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <interpreter.h>
#include <libforth.h>


int main(int argc, char **argv)
{   
    Program_State program_state = {0};
    program_state.sp = program_state.stack;
    program_state.dp = program_state.data;
    sb_append_null(&program_state.source);
    
    populate_builtin_words(&program_state);
    load_library("lib/core.fth", &program_state);

    bool st=false;
    shift(argv, argc);
    while (argc)
    {
        char *arg = shift(argv, argc);
        if (strcmp(arg, "-st")==0) st=true;
        else
        {
            printf("unknown flag(%s)\n", arg);
            return 1;
        }
    }

    printf("SMORTH v0.5\nby (re)tur(n) 0;\nthe bye word can be used at any time to quit\n");

    
    while (true)
    {
        program_state.source.count=0;
        char c;
        while ((c = getchar())!='\n') sb_append(&program_state.source, c);
        sb_append_null(&program_state.source);
        program_state.parse_offset=0;
        
        interpret(&program_state);
        if(st)
        {
            program_state.source.count=0;
            sb_append_cstr(&program_state.source, ".s");
            sb_append_null(&program_state.source);
            program_state.parse_offset=0;
            interpret(&program_state);
        }
        printf("ok\n");
    }
    
    return 0;
}
