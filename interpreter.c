#include "interpreter.h"
#include "codegen.h"

void interpret(Program_State *program_state)
{
    while (strcmp(program_state->ib.data, "")!=0&&program_state->ib.count!=0)
    {
        Token token = next_token(&program_state->ib);
        if (token.raw.count==0) continue;
        if (token.kind==NUMBER) 
        {
            if (program_state->compiling)
            {
                if (program_state->word_name==NULL) {printf("invalid word declaration"); exit(1);}

                sb_insert_mov(&program_state->word_source, reg_make_ptr(get_register(1),0), get_register(0));
                sb_insert_movabs(&program_state->word_source, get_register(5), (void *)token.as.number);
                sb_insert_mov(&program_state->word_source, get_register(5), reg_make_ptr(get_register(0),0));
                sb_insert_addimm(&program_state->word_source, reg_make_ptr(get_register(1), 0), 0x8);                
            }
            else
            {
                *program_state->sp=token.as.number;
                program_state->sp++;
                program_state->current_word=token.raw.data;
            }
        }
        else if (token.kind==FWORD)
        {
            if(program_state->compiling)
            {
                if (strcmp(token.as.word.data, "immediate")==0) program_state->immediate=true;
                else
                {
                    Execution_Token *word = get_word(&program_state->word_table, token.as.word.data);
                    if(word->imm) call_word(word->codeptr, program_state);
                    else sb_insert_call(&program_state->word_source, word->codeptr);
                }
            }
            else
            {
                if (strcmp(token.as.word.data, "immediate")==0) program_state->word_table.items[program_state->word_table.count-1]->imm=true;
                else
                {
                    Execution_Token *word = get_word(&program_state->word_table, token.as.word.data);
                    if(word==NULL) {printf("word ( %s ) not defined", token.as.word.data); exit(1);}
                    program_state->current_word=word->name;
                    call_word(word->codeptr, program_state);
                    if (program_state->sp<program_state->stack) {printf("stack underflow"); exit(1);}
                    {
                        String_Builder word_ret = {0};
                        sb_append_cstr(&word_ret, word->name);
                        sb_append_cstr(&word_ret, " - ret");
                        sb_append_null(&word_ret);
                        program_state->current_word=word_ret.items;
                    }
                }
            }
        }
        
    }
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

Token next_token(String_View *source)
{
    *source = sv_trim_left(*source);
    if(strcmp(source->data, "")==0||source->count==0) return (Token){0};

    String_Builder rawsb = {0};
    while (source->count>0 && !isspace(source->data[0])) sb_append(&rawsb, *sv_chop_left(source, 1).data);
    if (source->count>0) sb_append_null(&rawsb);
    String_View raw = sb_to_sv(rawsb);
    if (isnumber(raw.data))
    {
        int64_t number = atoll(raw.data);
        if ((strcmp(raw.data, "0") && strcmp(raw.data, "-0") && strcmp(raw.data, "+0")) && number==0) exit(1);
        return (Token){.kind=NUMBER, .raw=raw, .as.number=number};
    }
    else return (Token){.kind=FWORD, .raw=raw, .as.word=raw};
}


void *exallocsb(String_Builder *sb);
void exfreesb(void *ptr, size_t len);

void add_word_impl(Word_Table *word_table, const char *name, String_Builder source, bool immediate)
{
    String_Builder tmp = {0};
    sb_append_buf(&tmp, source.items, source.count);
    source = tmp;
    
    Execution_Token *word = malloc(sizeof(Execution_Token));
       word->imm=immediate;
        if(name)
        {
            word->name = malloc(strlen(name));
            memcpy(word->name, name, strlen(name)+1);
        } else word->name=NULL;
        word->source = source;
        word->codeptr = exallocsb(&source);
    da_append(word_table, word);
}
void add_word(Word_Table *word_table, const char *name, String_Builder source) { add_word_impl(word_table, name, source, false); }
void add_word_imm(Word_Table *word_table, const char *name, String_Builder source) { add_word_impl(word_table, name, source, true); }

Execution_Token *get_word(Word_Table *word_table, const char *name)
{
    if(!name) return NULL;
    for(size_t i=word_table->count; i>0; --i) if(word_table->items[i-1]->name) if(strcmp(word_table->items[i-1]->name, name)==0) return word_table->items[i-1];
    return NULL;
}

void call_word(void(*word)(int64_t**), Program_State *program_state)
{
    word(&program_state->sp);
}


#ifdef __WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

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