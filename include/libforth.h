#ifndef LIBFORTH_H
#define LIBFORTH_H


#include <interpreter.h>

void populate_builtin_words(Program_State *ps);
void load_library(const char *lib_path, Program_State *ps);

#endif