#include <libforth.h>
#include <codegen.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stddef.h> // for ptrdiff_t on linux


void populate_core_words(Program_State *ps);
void populate_tool_words(Program_State *ps);
void populate_builtin_words(Program_State *ps)
{
    populate_core_words(ps);
    populate_tool_words(ps);
}

void load_library(const char *lib_path, Program_State *ps)
{
    ps->source.count=0;
    read_entire_file(lib_path, &ps->source);
    for(size_t i=0;i<ps->source.count;i++) 
    {
        if(ps->source.items[i]=='\n') ps->source.items[i]='\0';
        else if(isspace(ps->source.items[i])) ps->source.items[i]=' ';
    }
    sb_append_null(&ps->source);

    ps->parse_offset=0;

    while(ps->parse_offset<ps->source.count)
    {
        interpret(ps);
        if(ps->parse_offset<ps->source.count) ps->parse_offset++;
    }
    return;
}

// core words
// --------------------------------------------------------------------------------------
void dot_impl(int64_t num)
{
    printf("%" PRId64 " ", num);
    return;
}

void colon_impl(Program_State *ps)
{
    while (ps->parse_offset<ps->source.count && isspace(ps->source.items[ps->parse_offset])) ps->parse_offset++;
    if(ps->parse_offset>=ps->source.count) exit(1);

    String_Builder ccc = {0};
    while (ps->parse_offset<ps->source.count && !isspace(ps->source.items[ps->parse_offset])) sb_append(&ccc, ps->source.items[ps->parse_offset++]);
    if (ps->parse_offset<ps->source.count) 
    {
        sb_append_null(&ccc);
        ps->parse_offset++;
    }

    Execution_Token *word = calloc(1,sizeof(Execution_Token));
    word->name=ccc.items;
    ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind = "colon-sys", .handle=get_jmp_marker(&word->source)};
    ps->compiling_word=word;
    ps->compiling=true;
}

void immediate_impl(Program_State *ps)
{
    if(ps->compiling) ps->compiling_word->imm=true;
    else ps->word_table.items[ps->word_table.count-1]->imm=true;
}

void semicolon_impl(Program_State *ps)
{
    if(strcmp(ps->cf_stack[--ps->cfi].kind, "colon-sys")!=0) UNREACHABLE("invalid address stack");
    sb_append(&ps->compiling_word->source, '\xC3');

    compile_word(ps->compiling_word);
    add_word(&ps->word_table, ps->compiling_word);

    if (ps->compiling_word->name==NULL) *(ps->sp++) = (int64_t)&ps->word_table.items[ps->word_table.count-1];
    ps->compiling_word=NULL;
    ps->compiling=false;
}

void create_impl(Program_State *ps)
{
    while (ps->parse_offset<ps->source.count && isspace(ps->source.items[ps->parse_offset])) ps->parse_offset++;
    if(ps->parse_offset>=ps->source.count) exit(1);

    String_Builder ccc = {0};
    while (ps->parse_offset<ps->source.count && !isspace(ps->source.items[ps->parse_offset])) sb_append(&ccc, ps->source.items[ps->parse_offset++]);
    if (ps->parse_offset<ps->source.count) 
    {
        sb_append_null(&ccc);
        ps->parse_offset++;
    }

    Execution_Token *word = calloc(1,sizeof(Execution_Token));
    word->name=ccc.items;
    word->dataptr=ps->dp;

    sb_insert_mov(&word->source, reg_make_ptr(get_register(1),0), get_register(0));
    sb_insert_mov(&word->source, get_register(3), reg_make_ptr(get_register(0),0));
    sb_insert_addimm(&word->source, reg_make_ptr(get_register(1), 0), 0x8);
    sb_append(&word->source, '\xC3');

    compile_word(word);
    add_word(&ps->word_table, word);
}

void exfreesb(void *ptr, size_t len);
void __does_runtime(Program_State *ps, void *addr)
{
    //exfreesb(ps->word_table.items[ps->word_table.count-1]->codeptr, ps->word_table.items[ps->word_table.count-1]->source.count);

    String_Builder source = {0};
    sb_insert_call(&source, addr);
    sb_append(&source, '\xC3');
    
    sb_free(ps->word_table.items[ps->word_table.count-1]->source);
    ps->word_table.items[ps->word_table.count-1]->source=source;
    compile_word(ps->word_table.items[ps->word_table.count-1]);
}
void does_impl(Program_State *ps)
{
    if(strcmp(ps->cf_stack[--ps->cfi].kind, "colon-sys")!=0) UNREACHABLE("invalid address stack");

    sb_insert_rel_call(&ps->compiling_word->source, ps->compiling_word->source.count+5);
    sb_insert_pop(&ps->compiling_word->source, get_register(0));
    
    String_Builder blob = {0};
        sb_insert_pop(&blob, get_register(0));
        sb_insert_movabs(&blob, get_register(0), NULL);
        sb_insert_addimm(&blob, get_register(0), 0);
        sb_insert_movabs(&blob, get_register(0), NULL);
        sb_insert_C_call(&blob, NULL, NULL);
    size_t offset = blob.count+1;
    sb_free(blob);

    String_Builder param_code = {0};
        sb_insert_movabs(&param_code, get_register(1), ps);
        sb_insert_addimm(&param_code, get_register(0), offset);
        sb_insert_mov(&param_code, get_register(0), get_register(2));
        sb_insert_C_call(&ps->compiling_word->source, __does_runtime, &param_code);
    sb_free(param_code);
    sb_append(&ps->compiling_word->source, '\xC3');

    ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind = "colon-sys", .handle=get_jmp_marker(&ps->compiling_word->source)};

    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
    sb_insert_mov(&ps->compiling_word->source, get_register(3), reg_make_ptr(get_register(0),0));
    sb_insert_addimm(&ps->compiling_word->source, reg_make_ptr(get_register(1), 0), 0x8);
}

void to_body_impl(Program_State *ps)
{
    Execution_Token *word = *(Execution_Token**)(--ps->sp);
    *(ps->sp++) = (int64_t)word->dataptr;
}

void begin_impl(Program_State *ps)
{
    ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind="dist", .handle=get_jmp_marker(&ps->compiling_word->source)};
}

void while_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if (strcmp(item.kind, "dist")==0)
    {
        sb_insert_subimm(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), 0x8);
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0),0), get_register(0));
        sb_insert_cmpimm(&ps->compiling_word->source, get_register(0), 0);
        ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind="orig", .handle=sb_start_jcc(&ps->compiling_word->source, EQ)};
        ps->cf_stack[ps->cfi++] = item;
    }
    else UNREACHABLE("invalid address stack");
}

void repeat_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if (strcmp(item.kind, "dist")==0)
    {
        sb_insert_jmp(&ps->compiling_word->source, item.handle);
        item = ps->cf_stack[--ps->cfi];
        if (strcmp(item.kind, "orig")==0) sb_end_jmp(&ps->compiling_word->source, item.handle);
        else UNREACHABLE("invalid address stack");
    }
    else UNREACHABLE("invalid address stack");
}

void until_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if (strcmp(item.kind, "dist")==0)
    {
        sb_insert_subimm(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), 0x8);
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0),0), get_register(0));
        sb_insert_cmpimm(&ps->compiling_word->source, get_register(0), 0);
        sb_insert_jcc(&ps->compiling_word->source, item.handle, EQ);
    }
    else UNREACHABLE("invalid address stack");
}

void do_init_impl(Program_State *ps)
{
    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0),-8), get_register(5));
    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0),-16), get_register(6));
    sb_insert_subimm(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), 0x10);

    sb_insert_rel_call(&ps->compiling_word->source, ps->compiling_word->source.count+5);
    size_t diff = get_jmp_marker(&ps->compiling_word->source);
    sb_insert_pop(&ps->compiling_word->source, get_register(0));
    sb_insert_addimm(&ps->compiling_word->source, get_register(0), 0);
    ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind="do-sys", .handle=ps->compiling_word->source.count};
    sb_insert_addimm(&ps->compiling_word->source, get_register(0), get_jmp_marker(&ps->compiling_word->source)-diff);
    sb_insert_push(&ps->compiling_word->source, get_register(0));

    sb_insert_push(&ps->compiling_word->source, get_register(5));
    sb_insert_push(&ps->compiling_word->source, get_register(6));
    sb_insert_push(&ps->compiling_word->source, REG_RBP);
    sb_append_cstr(&ps->compiling_word->source, "\x48\x89\xE5");
}

void plus_loop_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if (strcmp(item.kind, "dist")==0)
    {
        sb_insert_subimm(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), 0x8);
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_add(&ps->compiling_word->source, get_register(5), reg_make_ptr(REG_RBP,0x10));
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(REG_RBP,0x8), get_register(5));
        sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(REG_RBP,0x10), get_register(6));
        sb_insert_cmp(&ps->compiling_word->source, get_register(5), get_register(6));
        sb_insert_jcc(&ps->compiling_word->source, item.handle, NE);
        sb_insert_pop(&ps->compiling_word->source, REG_RBP);
        sb_insert_pop(&ps->compiling_word->source, get_register(0));
        sb_insert_pop(&ps->compiling_word->source, get_register(0));
        sb_insert_pop(&ps->compiling_word->source, get_register(0));
    }
    else UNREACHABLE("invalid address stack");

     item = ps->cf_stack[--ps->cfi];
     if(strcmp(item.kind, "do-sys")==0) sb_end_jmp(&ps->compiling_word->source, item.handle);
     else UNREACHABLE("invalid address stack");
}

void if_impl(Program_State *ps)
{
    sb_insert_subimm(&ps->compiling_word->source, reg_make_ptr(get_register(1), 0), 8);
    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1), 0), get_register(0));
    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(0), 0), get_register(0));
    sb_insert_cmpimm(&ps->compiling_word->source, get_register(0), 0);
    ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind="orig", .handle=sb_start_jcc(&ps->compiling_word->source, EQ)};
}

void else_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if(strcmp(item.kind, "orig")==0)
    {
        ps->cf_stack[ps->cfi++] = (Control_Flow_Stack_Item){.kind="orig", .handle=sb_start_jmp(&ps->compiling_word->source)};
        sb_end_jmp(&ps->compiling_word->source, item.handle);
    }
    else UNREACHABLE("invalid address stack");
}

void then_impl(Program_State *ps)
{
    Control_Flow_Stack_Item item = ps->cf_stack[--ps->cfi];
    if(strcmp(item.kind, "orig")==0) sb_end_jmp(&ps->compiling_word->source, item.handle);
    else UNREACHABLE("invalid address stack");
}

void parse_impl(Program_State *ps)
{
    char delim = *(--ps->sp);

    while (ps->parse_offset<ps->source.count && isspace(ps->source.items[ps->parse_offset])) ps->parse_offset++;
    if(ps->parse_offset>=ps->source.count) exit(1);

    String_Builder ccc = {0};
    while (ps->parse_offset<ps->source.count && ps->source.items[ps->parse_offset]!=delim) sb_append(&ccc, ps->source.items[ps->parse_offset++]);
    if (ps->parse_offset<ps->source.count) 
    {
        sb_append_null(&ccc);
        ps->parse_offset++;
    }
    
    *(ps->sp++) = (int64_t)ccc.items;
    *(ps->sp++) = ccc.count;

    return;
}

void find_impl(Program_State *ps)
{
    const char *name = *(const char **)(--ps->sp);

    Execution_Token *word = get_word(&ps->word_table, name);
    if(word!=NULL)
    {
        *(ps->sp++) = (int64_t)word;
        *(ps->sp++) = (word->imm) ? 1:-1;
    }
    else
    {
        *(ps->sp++) = (int64_t)name;
        *(ps->sp++) = 0;
    }

    return;
}

void compile_comma_impl(Program_State *ps)
{
    Execution_Token *word = *(Execution_Token**)(--ps->sp);
    sb_insert_FORTH_call(&ps->compiling_word->source, word);
}

void aligned_impl(Program_State *ps) 
{
    int64_t *addr = *(int64_t**)(--ps->sp);
    uint64_t phase = (uint64_t)ps->dp%8;
    if(phase!=0) addr = (int64_t *)((char *)addr+(8-phase));
    *(ps->sp++) = (int64_t)addr;
}

void execute_impl(Program_State *ps)
{
    Execution_Token *word = (Execution_Token*)(--ps->sp);
    call_word(word, ps);
}

void literal_impl(Program_State *ps)
{
    int64_t x = *(--ps->sp);

    sb_insert_mov(&ps->compiling_word->source, reg_make_ptr(get_register(1),0), get_register(0));
    sb_insert_movabs(&ps->compiling_word->source, get_register(5), (void *)x);
    sb_insert_mov(&ps->compiling_word->source, get_register(5), reg_make_ptr(get_register(0),0));
    sb_insert_addimm(&ps->compiling_word->source, reg_make_ptr(get_register(1), 0), 0x8);    
}

void recurse_impl(Program_State *ps)
{
    Control_Flow_Stack_Item *item=NULL;
    for(ptrdiff_t i=ps->cfi-1; i>=0;--i) if(strcmp("colon-sys", ps->cf_stack[i].kind)==0) {item=&ps->cf_stack[i]; break;}
    if(item) sb_insert_rel_call(&ps->compiling_word->source, item->handle);
    else UNREACHABLE("invalid address stack");
}

void type_impl(Program_State *ps)
{
    uint64_t len = *(--ps->sp);
    char *buf = *(char **)(--ps->sp);

    if(len>0)
    {
        String_Builder slice = {0};
        sb_append_buf(&slice, buf, len);
        sb_append_null(&slice);
        printf("%s", slice.items);
    }
}

void populate_core_words(Program_State *ps)
{
    String_Builder src = {0};

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(5),0));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "!", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_movabs(&src, get_register(6), &ps->dp);
        sb_insert_addimm(&src, reg_make_ptr(get_register(6), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(6),0), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(6),-8));
        sb_insert_subimm(&src, reg_make_ptr(get_register(1),0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, ",", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(5),0), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "@", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_add(&src, get_register(5), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "+", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),8), get_register(6));
        sb_insert_add(&src, get_register(5), reg_make_ptr(get_register(6),0));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "+!", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_sub(&src, get_register(5), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "-", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_imul(&src, get_register(5), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "*", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_idiv(&src, get_register(5), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "/", src);
    
    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "dup", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x10), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),0));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),8));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "2dup", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "drop", src);

    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "2drop", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-16), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "over", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x18), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x20), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),0));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),8));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "2over", src);
        
    src.count = 0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "nip", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-16), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-16));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "swap", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x08), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x18), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-0x18));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-0x08));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x10), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-0x20), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-0x20));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-0x10));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "2swap", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-24), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-16), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-16));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-24));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "rot", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-16), get_register(6));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-16));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "tuck", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(5));
        sb_insert_movabs(&src, get_register(6), ps->stack);
        sb_insert_sub(&src, get_register(6), get_register(5));
        sb_insert_idivabs(&src, get_register(5), sizeof(int64_t));
        sb_insert_subimm(&src, get_register(5), 1);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "depth", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_subimm(&param_code, reg_make_ptr(get_register(1), 0), 0x8);
            sb_insert_mov(&param_code, reg_make_ptr(get_register(1), 0), get_register(1));
            sb_insert_mov(&param_code, reg_make_ptr(get_register(1), 0), get_register(1));
            sb_insert_C_call(&src, dot_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, ".", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, colon_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, ":", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, immediate_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "immediate", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, semicolon_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, ";", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, create_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "create", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, does_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "does>", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, to_body_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, ">body", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, begin_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "begin", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, while_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "while", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, repeat_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "repeat", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, until_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "until", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, do_init_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "(do)", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(REG_RBP,0x10), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "i", src);

    src.count = 0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(REG_RBP,0x28), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "j", src);

    src.count = 0;
        sb_insert_pop(&src, get_register(0));
        sb_insert_pop(&src, REG_RBP);
        sb_insert_pop(&src, get_register(0));
        sb_insert_pop(&src, get_register(0));
        sb_append(&src, '\xC3');
    create_word(&ps->word_table, "leave", src);

    src.count = 0;
        sb_insert_pop(&src, get_register(5));
        sb_insert_pop(&src, REG_RBP);
        sb_insert_pop(&src, get_register(0));
        sb_insert_pop(&src, get_register(0));
        sb_insert_pop(&src, get_register(0));
        sb_insert_push(&src, get_register(5));
        sb_append(&src, '\xC3');
    create_word(&ps->word_table, "unloop", src);

    src.count = 0;
        sb_insert_pop(&src, get_register(0));
        sb_append(&src, '\xC3');
    create_word(&ps->word_table, "exit", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, plus_loop_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "+loop", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, if_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "if", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, else_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "else", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, then_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "then", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, parse_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "parse", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, find_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "find", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, compile_comma_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "compile,", src);

    src.count = 0;
        sb_insert_pop(&src, get_register(5));
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_push(&src, reg_make_ptr(get_register(0),0));
        sb_insert_push(&src, get_register(5));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, ">r", src);

    src.count = 0;
        sb_insert_pop(&src, get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_pop(&src, reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_push(&src, get_register(5));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "r>", src);
    
    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_get_flagimm(&src, get_register(5), 0, LT);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "0<", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_get_flagimm(&src, get_register(5), 0, EQ);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "0=", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_get_flagimm(&src, get_register(5), 0, NE);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "0<>", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(5));
        sb_insert_get_flagimm(&src, get_register(5), 0, GT);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "0>", src);

    src.count=0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_get_flag(&src, get_register(5), get_register(6), LT);
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "<", src);

    src.count=0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_get_flag(&src, get_register(5), get_register(6), EQ);
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "=", src);

    src.count=0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_get_flag(&src, get_register(5), get_register(6), NE);
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "<>", src);

    src.count=0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),-8), get_register(6));
        sb_insert_get_flag(&src, get_register(5), get_register(6), GT);
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, ">", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_inc(&src, reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "1+", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_dec(&src, reg_make_ptr(get_register(0),-8));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "1-", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, aligned_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "aligned", src);

    src.count=0;
        sb_insert_subimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_mov(&src, reg_make_ptr(get_register(0),0), get_register(5));
        sb_insert_movabs(&src, get_register(6), &ps->dp);
        sb_insert_add(&src, get_register(5), reg_make_ptr(get_register(6),0));
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "allot", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, execute_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "execute", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_movabs(&src, get_register(5), &ps->dp);
        sb_insert_mov(&src, reg_make_ptr(get_register(5),0), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "here", src);

    src.count=0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, literal_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "literal", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_movabs(&src, get_register(5), &ps->source.items);
        sb_insert_mov(&src, reg_make_ptr(get_register(5),0), get_register(5));
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_movabs(&src, get_register(6), &ps->source.count);
        sb_insert_mov(&src, reg_make_ptr(get_register(6),0), get_register(6));
        sb_insert_mov(&src, get_register(6), reg_make_ptr(get_register(0),8));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x10);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "source", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_movabs(&src, get_register(5), &ps->parse_offset);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, ">in", src);

    src.count=0;
        sb_insert_mov(&src, reg_make_ptr(get_register(1),0), get_register(0));
        sb_insert_movabs(&src, get_register(5), &ps->compiling);
        sb_insert_mov(&src, get_register(5), reg_make_ptr(get_register(0),0));
        sb_insert_addimm(&src, reg_make_ptr(get_register(1), 0), 0x8);
        sb_append_cstr(&src, "\xC3");
    create_word(&ps->word_table, "state", src);

    src.count=0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, recurse_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word_imm(&ps->word_table, "recurse", src);

    src.count=0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, type_impl, &param_code);
        sb_free(param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "type", src);

    sb_free(src);
}
// --------------------------------------------------------------------------------------


// tool words
// --------------------------------------------------------------------------------------
void dot_s_impl(Program_State *ps)
{
    printf("\n<%" PRIu64 "> [ ", ps->sp-ps->stack);
    for(int64_t *i=ps->sp-1; i>=ps->stack; --i) printf("%" PRId64 " ", *i);
    printf("]\n");
}

void populate_tool_words(Program_State *ps)
{
    String_Builder src = {0};
    
    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), 0);
            sb_insert_C_call(&src, exit, &param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, "bye", src);

    src.count = 0;
    {
        String_Builder param_code = {0};
            sb_insert_movabs(&param_code, get_register(1), ps);
            sb_insert_C_call(&src, dot_s_impl, &param_code);
        sb_append_cstr(&src, "\xC3");
    }
    create_word(&ps->word_table, ".s", src);

    sb_free(src);
}