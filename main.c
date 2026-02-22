#define NOB_IMPLEMENTATION
#include "nob.h"
#undef NOB_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

#include "codegen.h"


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
    size_t handle;
}
Address_Stack_Item;

typedef struct
{
    Word_Table_Item *word_table[1024];
    size_t word_table_size;

    int64_t stack[1024*64];
    int64_t *sp;

    bool should_quit;
    const char *current_word;

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

void printnum(int64_t num)
{
    printf("%" PRId64 " ", num);
    return;
}

void trace_stack(Program_State *program_state)
{
    if (!program_state->should_quit)
    {
        printf("\n\n|--stack-trace------- [ %s ]\n", program_state->current_word);
        for(int i=0; &program_state->stack[i]<program_state->sp; i++)
            printf("| %" PRId64 "\n", program_state->stack[i]);
        printf("|-------------------- [ %s ]\n\n", program_state->current_word);
    }
}

int main(int argc, char **argv)
{
    bool stack_trace = false;
    for(int i=0;i<argc;i++) if(strcmp(argv[i],"-st")==0) stack_trace=true;

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
                    sb_insert_cmpimm(&program_state.word_source, reg_make_ptr(get_register(2),0),1);
                    size_t jmp_handle = sb_start_jz(&program_state.word_source);

                    sb_insert_mov(&program_state.word_source, reg_make_ptr(get_register(1),0), get_register(0));
                    sb_insert_movabs(&program_state.word_source, get_register(5), (void *)token->as.number);
                    sb_insert_mov(&program_state.word_source, get_register(5), reg_make_ptr(get_register(0),0));
                    sb_insert_addimm(&program_state.word_source, reg_make_ptr(get_register(1), 0), 0x8);
                    
                    if(stack_trace)
                    {
                        String_Builder param_code = {0};
                        sb_append_cstr(&param_code, "\x48\xB9");
                        Program_State *ps = &program_state;
                        sb_append_buf(&param_code, &ps, sizeof(Program_State*));
                        sb_insert_call(&program_state.word_source, trace_stack, &param_code);
                    }
                    
                    sb_end_jcond(&program_state.word_source, jmp_handle);
                }
                else
                {
                    *program_state.sp=token->as.number;
                    program_state.sp++;
                    program_state.current_word=token->raw.data;
                    if (stack_trace) trace_stack(&program_state);
                }
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
                        sb_insert_cmpimm(&program_state.word_source, reg_make_ptr(get_register(2),0),1);
                        program_state.address_stack[program_state.ai++] = (Address_Stack_Item){.kind="should_quit", .handle=sb_start_jz(&program_state.word_source)};
                        sb_insert_subimm(&program_state.word_source, reg_make_ptr(get_register(1),0), 0x8);
                        sb_insert_mov(&program_state.word_source, reg_make_ptr(get_register(1),0), get_register(0));
                        sb_insert_mov(&program_state.word_source, reg_make_ptr(get_register(0),0), get_register(5));
                        sb_insert_mov(&program_state.word_source, reg_make_ptr(get_register(0),-8), get_register(6));
                        sb_insert_subimm(&program_state.word_source, reg_make_ptr(get_register(1),0), 0x8);
                        sb_append_cstr(&program_state.word_source, "\x55\x48\x89\xE5\x48\x83\xEC\x30");
                        sb_insert_mov(&program_state.word_source, get_register(5), reg_make_ptr(REG_RBP,-8));
                        sb_insert_mov(&program_state.word_source, get_register(6), reg_make_ptr(REG_RBP,-16));
                        program_state.address_stack[program_state.ai++] = (Address_Stack_Item){.kind="loop", .handle=get_jmp_marker(&program_state.word_source)};
                        sb_insert_mov(&program_state.word_source, reg_make_ptr(REG_RBP,-8), get_register(5));
                        sb_insert_mov(&program_state.word_source, reg_make_ptr(REG_RBP,-16), get_register(6));
                        sb_insert_cmp(&program_state.word_source, get_register(5), get_register(6));
                        program_state.address_stack[program_state.ai++] = (Address_Stack_Item){.kind="?do", .handle=sb_start_jz(&program_state.word_source)};
                        sb_append_cstr(&program_state.word_source, "\x48\xFF\x45\xF8");
                    }
                    else if (strcmp(token->as.word.data, "loop")==0)
                    {
                        Address_Stack_Item item = program_state.address_stack[--program_state.ai];
                        if (strcmp(item.kind, "?do")==0)
                        {
                            Address_Stack_Item loop = program_state.address_stack[--program_state.ai];
                            if (strcmp(loop.kind,"loop")==0)
                            {
                                sb_insert_cmpimm(&program_state.word_source, reg_make_ptr(get_register(2),0),1);
                                sb_insert_jnz(&program_state.word_source, loop.handle);
                            }
                            else UNREACHABLE("invalid address stack: expected kind loop");

                            sb_end_jcond(&program_state.word_source, item.handle);
                            sb_append_cstr(&program_state.word_source, "\x48\x83\xC4\x30\x5D");

                            item = program_state.address_stack[--program_state.ai];
                            if (strcmp(item.kind, "should_quit")==0) sb_end_jcond(&program_state.word_source, item.handle);
                            else UNREACHABLE("invalid address stack: expected kind should_quit");
                        }
                        else UNREACHABLE("invalid address stack");
                    }
                    else
                    {
                        if(program_state.word_name==NULL) program_state.word_name=token->as.word.data;
                        else
                        {
                            Word_Table_Item *word = get_word(program_state.word_table, program_state.word_table_size, token->as.word.data);
                            sb_insert_cmpimm(&program_state.word_source, reg_make_ptr(get_register(2),0),1);
                            size_t jmp_handle = sb_start_jz(&program_state.word_source);

                            sb_insert_call(&program_state.word_source, word->codeptr, NULL);

                            if(stack_trace)
                            {
                                String_Builder param_code = {0};
                                    sb_insert_movabs(&param_code, get_register(1), (void *)&program_state);
                                sb_insert_call(&program_state.word_source, trace_stack, &param_code);
                            }

                            sb_end_jcond(&program_state.word_source, jmp_handle);
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
                        program_state.current_word=word->key;
                        call_word(word->codeptr, &program_state);
                        if (program_state.sp<program_state.stack) {printf("stack underflow"); return 1;}
                        {
                            String_Builder word_ret = {0};
                            sb_append_cstr(&word_ret, word->key);
                            sb_append_cstr(&word_ret, " - ret");
                            sb_append_null(&word_ret);
                            program_state.current_word=word_ret.items;
                        }
                        if (stack_trace) trace_stack(&program_state);
                    }
                }
            }
            
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

void *exallocsb(String_Builder *sb);
void exfreesb(void *ptr, size_t len);
size_t __hash(const char *str);

void populate_builtin_words(Word_Table_Item **word_table, size_t size)
{
    String_Builder bye_src = {0};
        sb_insert_movabs(&bye_src, get_register(0), (void*)0x1);
        sb_insert_mov(&bye_src, get_register(0), reg_make_ptr(get_register(2),0));
        sb_append_cstr(&bye_src, "\xC3");
    add_word(word_table, size, "bye", bye_src);

    String_Builder plus_src = {0};
        sb_insert_subimm(&plus_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&plus_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&plus_src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&plus_src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_add(&plus_src, get_register(5), get_register(6));
        sb_insert_mov(&plus_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&plus_src, "\xC3");
    add_word(word_table, size, "+", plus_src);

    String_Builder minus_src = {0};
        sb_insert_subimm(&minus_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&minus_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&minus_src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&minus_src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_sub(&minus_src, get_register(5), get_register(6));
        sb_insert_mov(&minus_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&minus_src, "\xC3");
    add_word(word_table, size, "-", minus_src);

    String_Builder mul_src = {0};
        sb_insert_subimm(&mul_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&mul_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&mul_src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&mul_src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_imul(&mul_src, get_register(5), get_register(6));
        sb_insert_mov(&mul_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&mul_src, "\xC3");
    add_word(word_table, size, "*", mul_src);

    String_Builder div_src = {0};
        sb_insert_subimm(&div_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&div_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&div_src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&div_src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_idiv(&div_src, get_register(5), get_register(6));
        sb_insert_mov(&div_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&div_src, "\xC3");
    add_word(word_table, size, "/", div_src);
    
    String_Builder dup_src = {0};
        sb_insert_mov(&dup_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&dup_src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&dup_src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&dup_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&dup_src, "\xC3");
    add_word(word_table, size, "dup", dup_src);

    String_Builder drop_src = {0};
        sb_insert_subimm(&drop_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&drop_src, "\xC3");
    add_word(word_table, size, "drop", drop_src);

    String_Builder over_src = {0};
        sb_insert_mov(&over_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&over_src, reg_make_ptr(get_register(0),-16), get_register(5));
        sb_insert_mov(&over_src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&over_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&over_src, "\xC3");
        add_word(word_table, size, "over", over_src);
        
    String_Builder nip_src = {0};
        sb_insert_subimm(&nip_src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&nip_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&nip_src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&nip_src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&nip_src, "\xC3");
    add_word(word_table, size, "nip", nip_src);

    String_Builder swap_src = {0};
        sb_insert_mov(&swap_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&swap_src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&swap_src, reg_make_ptr(get_register(0),-16), get_register(6));
        sb_insert_mov(&swap_src, get_register(5), reg_make_ptr(get_register(0),-16));
        sb_insert_mov(&swap_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&swap_src, "\xC3");
    add_word(word_table, size, "swap", swap_src);

    String_Builder rot_src = {0};
        sb_insert_mov(&rot_src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&rot_src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&rot_src, reg_make_ptr(get_register(0),-24), get_register(6));
        sb_insert_mov(&rot_src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_insert_mov(&rot_src, reg_make_ptr(get_register(0),-16), get_register(6));
        sb_insert_mov(&rot_src, get_register(5), reg_make_ptr(get_register(0),-16));
        sb_insert_mov(&rot_src, get_register(6), reg_make_ptr(get_register(0),-24));
        sb_append_cstr(&rot_src, "\xC3");
    add_word(word_table, size, "rot", rot_src);

    String_Builder dot_src = {0};
        String_Builder param_code = {0};
            sb_insert_subimm(&param_code, reg_make_ptr(get_register(1), 0), 0x8);
            sb_insert_mov(&param_code, reg_make_ptr(get_register(1), 0), get_register(1));
            sb_insert_mov(&param_code, reg_make_ptr(get_register(1), 0), get_register(1));
        sb_insert_call(&dot_src, printnum, &param_code);
        sb_append_cstr(&dot_src, "\xC3");
    add_word(word_table, size, ".", dot_src);
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
        word_table[key]->codeptr = exallocsb(&source);
        word_table[key]->next=NULL;
    }
    else
    {
        Word_Table_Item *item = word_table[key];
        while (item->next!=NULL)
        {
            if (!strcmp(item->key, name))
            {
                exfreesb(item->codeptr, item->source.count);
                item->source = source;
                item->codeptr = exallocsb(&source);
                memcpy(item->codeptr, source.items, source.count);
                return;
            }
            item = item->next;
        }
        if (!strcmp(item->key, name))
        {
            exfreesb(item->codeptr, item->source.count);
            item->source = source;
            item->codeptr = exallocsb(&source);
            memcpy(item->codeptr, source.items, source.count);
            return;
        }
        item->next = malloc(sizeof(Word_Table_Item));
        item = item->next;
        item->key =  malloc(strlen(name));
            memcpy(item->key, name, strlen(name)+1);
        item->source = source;
        item->codeptr = exallocsb(&source);
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


void *exallocsb(String_Builder *sb)
{
#ifdef _WIN32
    void *ptr = VirtualAlloc(NULL, sb->count, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(ptr, sb->items, sb->count);
    return ptr;
#else
    void *ptr = mmap(NULL, sb->count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(ptr, sb->items, sb->count);
    mprotect(ptr, sb->count, PROT_READ | PROT_EXEC);
    return ptr;
#endif
}

// len only needed on unix
void exfreesb(void *ptr, size_t len)
{
#ifdef _WIN32
    (void)len;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, len);
#endif
    return;
}


size_t __hash(const char *str)
{
    size_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}
