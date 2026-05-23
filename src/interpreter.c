#include <interpreter.h>
#include <codegen.h>

void interpret(Program_State *program_state)
{
    while (strcmp(program_state->source.items+program_state->parse_offset, "")!=0&&program_state->parse_offset<program_state->source.count)
    {
        Token token = next_token(program_state);
        if (token.raw.count==0) continue;
        if (token.kind==NUMBER) 
        {
            if (program_state->compiling)
            {
                sb_insert_mov(&program_state->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
                sb_insert_movabs(&program_state->compiling_word->source, get_register(5), (void *)token.as.number);
                sb_insert_mov(&program_state->compiling_word->source, get_register(5), reg_make_ptr(get_register(0),0));
                sb_insert_addimm(&program_state->compiling_word->source, reg_make_ptr(get_register(1), 0), 0x8);                
            }
            else
            {
                *program_state->sp=token.as.number;
                program_state->sp++;
            }
        }
        else if (token.kind==FWORD)
        {
            if(program_state->compiling)
            {
                Execution_Token *word = get_word(&program_state->word_table, token.as.word.data);
                if(word->imm) call_word(word, program_state);
                else sb_insert_FORTH_call(&program_state->compiling_word->source, word);
            }
            else
            {
                Execution_Token *word = get_word(&program_state->word_table, token.as.word.data);
                call_word(word, program_state);
                if (program_state->sp<program_state->stack) {printf("stack underflow\n"); exit(1);}
            }
        }
        sb_free(token.raw);
    }
}


bool forth_isnumber(const char *raw)
{
    size_t i = 0;
    if (raw[i]=='-' || raw[i]=='+') i++;
    while (i<strlen(raw) && isdigit(raw[i])) i++;
    if (raw[i]=='.') while (i<strlen(raw) && isdigit(raw[i])) i++;
    while (i<strlen(raw) && isspace(raw[i])) i++;
    if (i!=strlen(raw) || !isdigit(raw[i-1]) || i==0) return false;
    return true;
}

Token next_token(Program_State  *ps)
{
    while (ps->parse_offset<ps->source.count && isspace(ps->source.items[ps->parse_offset])) ps->parse_offset++;
    if(strcmp(ps->source.items+ps->parse_offset, "")==0||ps->parse_offset>=ps->source.count) return (Token){0};

    String_Builder raw = {0};
    while (ps->parse_offset<ps->source.count && !isspace(ps->source.items[ps->parse_offset])) sb_append(&raw, ps->source.items[ps->parse_offset++]);
    if (ps->parse_offset<ps->source.count) 
    {
        ps->parse_offset++;
        sb_append_null(&raw);
    }

    if (get_word(&ps->word_table, raw.items)) return (Token){.kind=FWORD, .raw=raw, .as.word=sb_to_sv(raw)};
    if (forth_isnumber(raw.items))
    {
        int64_t number = atoll(raw.items);
        if ((strcmp(raw.items, "0") && strcmp(raw.items, "-0") && strcmp(raw.items, "+0")) && number==0) exit(1);
        return (Token){.kind=NUMBER, .raw=raw, .as.number=number};
    }
    printf("word ( %s ) not defined\n", raw.items); 
    exit(1);
}


void *exallocsb(String_Builder *sb);
void exfreesb(void *ptr, size_t len);


void compile_word(Execution_Token *word) { word->codeptr = exallocsb(&word->source); }

void add_word(Word_Table *word_table, Execution_Token *word) { da_append(word_table, word); }

void create_word_impl(Word_Table *word_table, const char *name, String_Builder source, bool immediate)
{
    String_Builder tmp = {0};
    sb_append_buf(&tmp, source.items, source.count);
    source = tmp;
    
    Execution_Token *word = calloc(1, sizeof(Execution_Token));
       word->imm=immediate;
        if(name)
        {
            word->name = malloc(strlen(name)+1);
            memcpy(word->name, name, strlen(name)+1);
        } else word->name=NULL;
        word->source = source;
        word->codeptr = exallocsb(&source);
    da_append(word_table, word);
}
void create_word(Word_Table *word_table, const char *name, String_Builder source) { create_word_impl(word_table, name, source, false); }
void create_word_imm(Word_Table *word_table, const char *name, String_Builder source) { create_word_impl(word_table, name, source, true); }

Execution_Token *get_word(Word_Table *word_table, const char *name)
{
    if(!name) return NULL;
    for(size_t i=word_table->count; i>0; --i) if(word_table->items[i-1]->name) if(strcmp(word_table->items[i-1]->name, name)==0) return word_table->items[i-1];
    return NULL;
}

void call_word(Execution_Token *word, Program_State *program_state)
{
    word->codeptr(&program_state->sp, NULL, word->dataptr);
}

void sb_insert_FORTH_call(String_Builder *sb, Execution_Token *word)
{
    sb_insert_push(sb, get_register(3));
    sb_insert_movabs(sb, get_register(3), word->dataptr);
    sb_insert_call(sb, word->codeptr);
    sb_insert_pop(sb, get_register(3));
    return;
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