<img src="smorth.png" width="256px"/>

## Introduction
smorth is a small FORTH interpreter that natively JITs x86 for word definitions.

## Warning
smorth is just a small project I like to work on in my free time, and is not intended for production use.

## Compilation instructions
first bootstrap the buildsystem by compiling nob.c \
from there you can just run nob to build the project \
feel free to change nob.c as much as you want, running nob will automatically recompile and run it

## Usage
you can run smorth with -st to display the stack as an expression is interpreted \
running bye will exit the forth repl \
passing the path to any .fth files will automatically interpret their contents \
a full list of supported instructions will be coming later
