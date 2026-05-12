#ifndef INTERPRETER_H
#define INTERPRETER_H


#include <nob.h>

typedef enum TokenKind
{
    NUMBER,
    FWORD
}
TokenKind;

typedef struct
{
    TokenKind kind;
    union
    {
        int64_t number;
        String_View word;
    } as;
    
    String_View raw;
}
Token;

typedef struct
{
    Token *items;
    size_t count;
    size_t capacity;
} 
Tokens;


typedef struct
{
    bool imm;
    char *name;
    String_Builder source;
    void(*codeptr)(int64_t**, double**, int64_t*);
    int64_t *dataptr;
}
Execution_Token;

typedef struct
{
    Execution_Token **items;
    size_t count;
    size_t capacity;
}
Word_Table;

typedef struct
{
    char *kind;
    size_t handle;
}
Control_Flow_Stack_Item;

typedef struct
{
    Word_Table word_table;

    int64_t stack[1024*64];
    int64_t *sp;

    int64_t data[1024*64];
    int64_t *dp;

    String_Builder source;
    size_t parse_offset;

    int64_t compiling;
    Execution_Token *compiling_word;
    Control_Flow_Stack_Item cf_stack[1024];
    size_t cfi;
}
Program_State;


void interpret(Program_State *program_state);

Token next_token(Program_State *ps);
void compile_word(Execution_Token *word);
void add_word(Word_Table *word_table, Execution_Token *word);
void create_word(Word_Table *word_table, const char *name, String_Builder source);
void create_word_imm(Word_Table *word_table, const char *name, String_Builder source);
Execution_Token *get_word(Word_Table *word_table, const char *name);
void call_word(Execution_Token *word, Program_State *program_state);
void sb_insert_FORTH_call(String_Builder *sb, Execution_Token *word);

#endif