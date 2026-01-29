#define NOB_IMPLEMENTATION
#include "nob.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>


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
    size_t line;
    size_t col;
}
Token;

typedef struct
{
    Token *items;
    size_t count;
    size_t capacity;
} 
Tokens;


typedef struct Item
{
    char *key;
    String_Builder source;
    void(*codeptr)(int64_t**, bool *);
    struct Item *next;
}
Word_Table_Item;

typedef struct
{
    char *kind;
    int32_t old_count;
}
Address_Stack_Item;

typedef struct
{
    Word_Table_Item *word_table[1024];
    size_t word_table_size;

    int64_t stack[1024*64];
    int64_t *sp;

    bool should_quit;

    bool compiling;
    const char *word_name;
    String_Builder word_source;
    Address_Stack_Item address_stack[1024];
    size_t ai;
}
Program_State;


bool isnumber(const char *raw);
void lex(Tokens *tokens, char *source);
void populate_builtin_words(Word_Table_Item **word_table, size_t size);
void add_word(Word_Table_Item **word_table, size_t size, const char *name, String_Builder source);
Word_Table_Item *get_word(Word_Table_Item **word_table, size_t size, const char *name);
void call_word(void(*word)(int64_t**, bool *), Program_State *program_state);
#define PROLOGUE "\x55\x48\x89\xE5\x48\x83\xEC\x30\x48\x89\x4D\xF8\x48\x89\x55\xF0"
#define EPILOGUE "\x48\x8B\x55\xF0\x48\x8B\x4D\xF8\x48\x83\xC4\x30\x5D"

void printnum(int64_t num)
{
    printf("%lli ", num);
    return;
}

int main()
{
    Program_State program_state = {0};
    program_state.word_table_size = sizeof(program_state.word_table)/sizeof(Word_Table_Item*);
    populate_builtin_words(program_state.word_table, program_state.word_table_size);

    program_state.sp = program_state.stack;

    String_Builder line = {0};
    sb_append_null(&line);
    while (!program_state.should_quit)
    {
        line.count=0;
        char c;
        while ((c = getchar())!='\n') sb_append(&line, c);
        sb_append_null(&line);
        
        Tokens tokens = {0};
        lex(&tokens, line.items);
        da_foreach(Token, token, &tokens)
        {
            if (token->kind==NUMBER) 
            {
                if (program_state.compiling)
                {
                    if (program_state.word_name==NULL) {printf("invalid word declaration"); return 1;}
                    sb_append_buf(&program_state.word_source, "\x66\x83\x3A\x01\x0F\x84\x14\x00\x00\x00", 10);
                    sb_append_cstr(&program_state.word_source, "\x48\x8B\x39\x48\xB8");
                    sb_append_buf(&program_state.word_source, &token->as.number, sizeof(int64_t));
                    sb_append_cstr(&program_state.word_source, "\x48\x89\x07\x48\x83\x01\x08");
                }
                else
                    {*program_state.sp=token->as.number; program_state.sp++;}
            }
            else if (token->kind==FWORD)
            {
                if(program_state.compiling)
                {
                    if (strcmp(token->as.word.data, ";")==0)
                    {
                        if (program_state.word_name==NULL) {printf("invalid word declaration"); return 1;}
                        sb_append(&program_state.word_source, '\xC3');
                        add_word(program_state.word_table, program_state.word_table_size, program_state.word_name, program_state.word_source);
                        program_state.word_name=NULL;
                        program_state.word_source.count=0;
                        program_state.compiling=false;
                    }
                    else if (strcmp(token->as.word.data, "?do")==0)
                    {
                        sb_append_buf(&program_state.word_source, "\x66\x83\x3A\x01\x0F\x84\x00\x00\x00\x00", 10);
                        sb_append_cstr(&program_state.word_source, "\x48\x83\x29\x08\x4C\x8B\x21\x49\x8B\x3C\x24\x49\x8B\x74\x24\xF8\x48\x83\x29\x08\x55\x48\x89\xE5\x48\x83\xEC\x30\x48\x89\x7D\xF8\x48\x89\x75\xF0");
                        program_state.address_stack[program_state.ai++] = (Address_Stack_Item){.kind="?do", .old_count=program_state.word_source.count};
                        sb_append_buf(&program_state.word_source, "\x48\x8B\x7D\xF8\x48\x8B\x75\xF0\x48\x39\xF7\x0F\x84\x00\x00\x00\x00\x48\xFF\x45\xF8", 21);
                    }
                    else if (strcmp(token->as.word.data, "loop")==0)
                    {
                        Address_Stack_Item item = program_state.address_stack[--program_state.ai];
                        if (strcmp(item.kind, "?do")==0)
                        {
                            sb_append(&program_state.word_source, '\xE9');
                            int32_t dif = (item.old_count-program_state.word_source.count)-4;
                            sb_append_buf(&program_state.word_source, &dif, sizeof(int32_t));
                            dif = (program_state.word_source.count-item.old_count)-17;
                            int32_t dif2 = dif+58;
                            for(int i=0;i<4;i++)
                            {
                                program_state.word_source.items[item.old_count+13+i] = ((char *)&dif)[i];
                                program_state.word_source.items[item.old_count-40+i] = ((char *)&dif2)[i];
                            }
                            sb_append_cstr(&program_state.word_source, "\x48\x83\xC4\x30\x5D");
                        }
                    }
                    else
                    {
                        if(program_state.word_name==NULL) program_state.word_name=token->as.word.data;
                        else
                        {
                            Word_Table_Item *word = get_word(program_state.word_table, program_state.word_table_size, token->as.word.data);
                            sb_append_buf(&program_state.word_source, "\x66\x83\x3A\x01\x0F\x84\x29\x00\x00\x00", 10);
                            sb_append_cstr(&program_state.word_source, PROLOGUE);
                            sb_append_cstr(&program_state.word_source, "\x48\xB8");
                            sb_append_buf(&program_state.word_source, &word->codeptr, sizeof(void*));
                            sb_append_cstr(&program_state.word_source, "\xFF\xD0");
                            sb_append_cstr(&program_state.word_source, EPILOGUE);
                        }
                    }
                }
                else
                {
                    if (strcmp(token->as.word.data, ":")==0) program_state.compiling=true;
                    else
                    {
                        Word_Table_Item *word = get_word(program_state.word_table, program_state.word_table_size, token->as.word.data);
                        if(word==NULL) {printf("word not defined"); return 1;}
                        call_word(word->codeptr, &program_state);
                        if (program_state.sp<program_state.stack) {printf("stack underflow"); return 1;}
                    }
                }
            }
            
            /*if (program_state.sp!=program_state.stack)
            {
                printf("[ ");
                int i = 0;
                while (&program_state.stack[i]<program_state.sp-1)
                {
                    printf("%lli, ", program_state.stack[i]);
                    i++;
                    }
                    printf("%lli ]\n", program_state.stack[i]);
                    }*/
        }
        printf("ok\n");
    }
    
    return 0;
}


bool isnumber(const char *raw)
{
    size_t i = 0;
    if (raw[i]=='-' || raw[i]=='+') i++;
    while (i<strlen(raw) && isdigit(raw[i])) i++;
    if (raw[i]=='.') while (i<strlen(raw) && isdigit(raw[i])) i++;
    while (i<strlen(raw) && isspace(raw[i])) i++;
    if (i!=strlen(raw) || !isdigit(raw[i-1]) || i==0) return false;
    return true;
}

void lex(Tokens *tokens, char *source)
{
    size_t i = 0;
    size_t line = 1;
    size_t col = 0;
    while (i < strlen(source))
    {
        if (source[i]=='\n') { line++; col=0; }
        if (isspace(source[i])) { i++; col++; continue; }

        String_Builder rawsb = {0};
        while (!isspace(source[i]))
        {
            sb_append(&rawsb, source[i]);
            i++;
        }
        sb_append_null(&rawsb);
        String_View raw = sb_to_sv(rawsb);
        if (isnumber(raw.data))
        {
            int64_t number = atoll(raw.data);
            if ((strcmp(raw.data, "0") && strcmp(raw.data, "-0") && strcmp(raw.data, "+0")) && number==0) return;
            Token token = {.kind=NUMBER, .line=line, .col=col, .raw=raw, .as.number=number};
            da_append(tokens, token);
            col+=strlen(raw.data)-1;
        }
        else
        {
            Token token = {.kind=FWORD, .line=line, .col=col, .raw=raw, .as.word=raw};
            da_append(tokens, token);
            col+=strlen(raw.data)-1;
        }
    }
}

size_t __hash(const char *str);

void populate_builtin_words(Word_Table_Item **word_table, size_t size)
{
    add_word(word_table, size, "bye", (String_Builder){.items = "\x49\xC7\xC3\x01\x00\x00\x00\x4C\x89\x1A\xC3", .count = 11, .capacity = 11});
    add_word(word_table, size, "+", (String_Builder){.items = "\x48\x83\x29\x08\x48\x8B\x39\x4C\x8B\x1F\x4C\x8B\x67\xF8\x4D\x01\xDC\x4C\x89\x67\xF8\xC3", .count = 22, .capacity = 22});
    add_word(word_table, size, "-", (String_Builder){.items = "\x48\x83\x29\x08\x48\x8B\x39\x4C\x8B\x1F\x4C\x8B\x67\xF8\x4D\x29\xDC\x4C\x89\x67\xF8\xC3", .count = 22, .capacity = 22});
    add_word(word_table, size, "*", (String_Builder){.items = "\x48\x83\x29\x08\x48\x8B\x39\x4C\x8B\x1F\x4C\x8B\x67\xF8\x4D\x0F\xAF\xE3\x4C\x89\x67\xF8\xC3", .count = 23, .capacity = 23});
    add_word(word_table, size, "/", (String_Builder){.items = "\x48\x83\x29\x08\x48\x8B\x39\x4C\x8B\x1F\x48\x8B\x47\xF8\x48\x99\x49\xF7\xFB\x48\x89\x47\xF8\xC3", .count = 24, .capacity = 24});
    add_word(word_table, size, "dup", (String_Builder){.items = "\x48\x8B\x39\x4C\x8B\x67\xF8\x4C\x89\x27\x48\x83\x01\x08\xC3", .count = 15, .capacity = 15});
    add_word(word_table, size, "drop", (String_Builder){.items = "\x48\x83\x29\x08\xC3", .count = 5, .capacity = 5});
    add_word(word_table, size, "over", (String_Builder){.items = "\x48\x8B\x39\x4C\x8B\x67\xF0\x4C\x89\x27\x48\x83\x01\x08\xC3", .count = 15, .capacity = 15});
    add_word(word_table, size, "nip", (String_Builder){.items = "\x48\x83\x29\x08\x48\x8B\x39\x4C\x8B\x27\x4C\x89\x67\xF8\xC3", .count = 15, .capacity = 15});
    add_word(word_table, size, "swap", (String_Builder){.items = "\x48\x8B\x39\x4C\x8B\x67\xF8\x4C\x8B\x6F\xF0\x4C\x89\x6F\xF8\x4C\x89\x67\xF0\xC3", .count = 20, .capacity = 20});
    add_word(word_table, size, "rot", (String_Builder){.items = "\x48\x8B\x39\x4C\x8B\x67\xF8\x4C\x8B\x6F\xE8\x4C\x89\x6F\xF8\x4C\x8B\x6F\xF0\x4C\x89\x67\xF0\x4C\x89\x6F\xE8\xC3", .count = 28, .capacity = 28});
    String_Builder dot_source = {0};
        sb_append_cstr(&dot_source, PROLOGUE);
        sb_append_cstr(&dot_source, "\x48\x83\x29\x08\x48\x8B\x09\x48\x8B\x09");
        sb_append_cstr(&dot_source, "\x48\xB8");
        void(*printfn)(int64_t) = printnum;
        sb_append_buf(&dot_source, &printfn, sizeof(void*));
        sb_append_cstr(&dot_source, "\xFF\xD0");
        sb_append_cstr(&dot_source, EPILOGUE);
        sb_append_cstr(&dot_source, "\xC3");
    add_word(word_table, size, ".", dot_source);
}

void add_word(Word_Table_Item **word_table, size_t size, const char *name, String_Builder source)
{
    String_Builder tmp = {0};
    sb_append_buf(&tmp, source.items, source.count);
    source = tmp;
    size_t key = __hash(name)%size;

    if (word_table[key]==NULL)
    {
        word_table[key] = malloc(sizeof(Word_Table_Item));
        word_table[key]->key = malloc(strlen(name));
            memcpy(word_table[key]->key, name, strlen(name)+1);
        word_table[key]->source = source;
        word_table[key]->codeptr = VirtualAlloc(NULL, source.count, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            memcpy(word_table[key]->codeptr, source.items, source.count);
        word_table[key]->next=NULL;
    }
    else
    {
        Word_Table_Item *item = word_table[key];
        while (item->next!=NULL)
        {
            if (!strcmp(item->key, name))
            {
                item->source = source;
                VirtualFree(item->codeptr, 0, MEM_RELEASE);
                item->codeptr = VirtualAlloc(NULL, source.count, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                memcpy(item->codeptr, source.items, source.count);
                return;
            }
            item = item->next;
        }
        item->next = malloc(sizeof(Word_Table_Item));
        item = item->next;
        item->key =  malloc(strlen(name));
            memcpy(item->key, name, strlen(name)+1);
        item->source = source;
        item->codeptr = VirtualAlloc(NULL, source.count, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        memcpy(item->codeptr, source.items, source.count);
        item->next=NULL;
    }
}

Word_Table_Item *get_word(Word_Table_Item **word_table, size_t size, const char *name)
{
    size_t key = __hash(name)%size;
    Word_Table_Item *item = word_table[key];
    while (item!=NULL)
    {
        if (!strcmp(item->key, name)) break;
        item = item->next;
    }
    return item;
}

void call_word(void(*word)(int64_t**, bool *), Program_State *program_state)
{
    word(&program_state->sp, &program_state->should_quit);
}


size_t __hash(const char *str)
{
    size_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}
