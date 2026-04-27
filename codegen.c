#include "codegen.h"

void sb_insert_pushimm(String_Builder *sb, int32_t v)
{
    sb_append(sb, '\x68');
    sb_append_buf(sb, &v, sizeof(int32_t));
}

void sb_insert_push(String_Builder *sb, Register reg)
{
    if(reg.id&8) sb_append(sb, '\x41');
    if(reg.kind==REGISTER) sb_append(sb, '\x50'|(reg.id&7));
    else
    {
        sb_append(sb, '\xFF');
        sb_append(sb, (reg.kind<<6)|(reg.id&7)|(6<<3));
    }

    if(reg.kind==POINTER8) sb_append(sb, reg.as.pointer8);
    if(reg.kind==POINTER32) sb_append_buf(sb, &reg.as.pointer32, sizeof(int32_t));
}

void sb_insert_pop(String_Builder *sb, Register reg)
{
    if(reg.id&8) sb_append(sb, '\x41');
    if(reg.kind==REGISTER) sb_append(sb, '\x58'|(reg.id&7));
    else
    {
        sb_append(sb, '\x8F');
        sb_append(sb, (reg.kind<<6)|(reg.id&7));
    }

    if(reg.kind==POINTER8) sb_append(sb, reg.as.pointer8);
    if(reg.kind==POINTER32) sb_append_buf(sb, &reg.as.pointer32, sizeof(int32_t));
}


void sb_insert_C_call(String_Builder *sb, void *fp, String_Builder *param_code)
{
    PROLOGUE(sb);
    if(param_code!=NULL) sb_append_buf(sb, param_code->items, param_code->count);
    sb_insert_movabs(sb, REG_RAX, fp);
    sb_append(sb, '\xFF');
    sb_append(sb, '\xD0'|RAX);
    EPILOGUE(sb);
    return;
}

void sb_insert_call(String_Builder *sb, void *fp)
{
    sb_insert_movabs(sb, REG_RAX, fp);
    sb_append(sb, '\xFF');
    sb_append(sb, '\xD0'|RAX);
    return;
}

void sb_insert_rel_call(String_Builder *sb, size_t jmp_handle)
{
    int32_t dif = (int32_t)(jmp_handle-(sb->count+5));
    sb_append(sb, '\xE8');
    sb_append_buf(sb, &dif, sizeof(int32_t));
}

void sb_insert_movabs(String_Builder *sb, Register reg, void *v)
{
    if(reg.kind!=REGISTER) UNREACHABLE("unsupported register operand");
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\xB8'|(reg.id&7));
    sb_append_buf(sb, &v, sizeof(void*));
    return;
}

void sb_insert_mov(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot mov mem -> mem");

    bool dist_is_mem = dst.kind!=REGISTER;
    if(dist_is_mem)
    {
        Register tmp = src;
        src = dst;
        dst = tmp;
    }

    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0)|((dst.id&8)?0x4:0x0));
    sb_append(sb, (dist_is_mem)?'\x89':'\x8B');

    sb_append(sb, (src.kind<<6)|(src.id&7)|((dst.id&7)<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
}

void sb_insert_addimm(String_Builder *sb, Register reg, int32_t v)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\x81');
    sb_append(sb, (reg.kind<<6)|(reg.id&7));
    sb_append_buf(sb, &v, sizeof(int32_t));
}

void sb_insert_add(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot mov mem -> mem");

    bool dist_is_mem = dst.kind!=REGISTER;
    if(dist_is_mem)
    {
        Register tmp = src;
        src = dst;
        dst = tmp;
    }

    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0)|((dst.id&8)?0x4:0x0));
    sb_append(sb, (dist_is_mem)?'\x01':'\x03');

    sb_append(sb, (src.kind<<6)|(src.id&7)|((dst.id&7)<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
}

void sb_insert_subimm(String_Builder *sb, Register reg, int32_t v)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\x81');
    sb_append(sb, (reg.kind<<6)|(5<<3)|(reg.id&7));
    sb_append_buf(sb, &v, sizeof(int32_t));
}

void sb_insert_sub(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot mov mem -> mem");

    bool dist_is_mem = dst.kind!=REGISTER;
    if(dist_is_mem)
    {
        Register tmp = src;
        src = dst;
        dst = tmp;
    }

    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0)|((dst.id&8)?0x4:0x0));
    sb_append(sb, (dist_is_mem)?'\x29':'\x2B');

    sb_append(sb, (src.kind<<6)|(src.id&7)|((dst.id&7)<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
}

void sb_insert_inc(String_Builder *sb, Register reg)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\xFF');
    sb_append(sb, (reg.kind<<6)|(reg.id&7));
    if(reg.kind==POINTER8) sb_append(sb, reg.as.pointer8);
    if(reg.kind==POINTER32) sb_append_buf(sb, &reg.as.pointer32, sizeof(int32_t));
}

void sb_insert_dec(String_Builder *sb, Register reg)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\xFF');
    sb_append(sb, (reg.kind<<6)|(1<<3)|(reg.id&7));
    if(reg.kind==POINTER8) sb_append(sb, reg.as.pointer8);
    if(reg.kind==POINTER32) sb_append_buf(sb, &reg.as.pointer32, sizeof(int32_t));
}

void sb_insert_imulimm(String_Builder *sb, Register reg, int32_t v)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x5:0x0));
    sb_append(sb, '\x69');
    sb_append(sb, (reg.kind<<6)|((reg.id&7)<<3)|(reg.id&7));
    sb_append_buf(sb, &v, sizeof(int32_t));
}

void sb_insert_imul(String_Builder *sb, Register src, Register dst)
{
    if(dst.kind!=REGISTER) UNREACHABLE("dst cannot be mem");
    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0)|((dst.id&8)?0x4:0x0));
    sb_append_cstr(sb, "\x0F\xAF");

    sb_append(sb, (src.kind<<6)|(src.id&7)|((dst.id&7)<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
}

void sb_insert_idivabs(String_Builder *sb, Register reg, int64_t v)
{
    sb_append_cstr(sb, "\x50\x51\x52");
    sb_insert_mov(sb, reg, REG_RAX);
    sb_append_cstr(sb, "\x48\x99");
    sb_insert_movabs(sb, (Register){.id=RCX, .kind=REGISTER}, (void*)v);
    sb_append_cstr(sb, "\x48\xF7\xF9");
    sb_insert_mov(sb, REG_RAX, reg);
    sb_append_cstr(sb, "\x5A\x59\x58"); 
}

void sb_insert_idiv(String_Builder *sb, Register src, Register dst)
{
    sb_append_cstr(sb, "\x50\x52");
    sb_insert_mov(sb, dst, REG_RAX);
    sb_append_cstr(sb, "\x48\x99");
    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0));
    sb_append(sb, '\xF7');
    
    sb_append(sb, (src.kind<<6)|(src.id&7)|(7<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
    sb_insert_mov(sb, (Register){.id=RAX, .kind=REGISTER}, dst);
    sb_append_cstr(sb, "\x5A\x58");
}

void sb_insert_cmpimm(String_Builder *sb, Register reg, int32_t v)
{
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0));
    sb_append(sb, '\x81');
    sb_append(sb, (reg.kind<<6)|(7<<3)|(reg.id&7));
    sb_append_buf(sb, &v, sizeof(int32_t));
}

void sb_insert_cmp(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot mov mem -> mem");

    bool dist_is_mem = dst.kind!=REGISTER;
    if(dist_is_mem)
    {
        Register tmp = src;
        src = dst;
        dst = tmp;
    }

    sb_append(sb, '\x48'|((src.id&8)?0x1:0x0)|((dst.id&8)?0x4:0x0));
    sb_append(sb, (dist_is_mem)?'\x39':'\x3B');

    sb_append(sb, (src.kind<<6)|(src.id&7)|((dst.id&7)<<3));
    if(src.kind==POINTER8) sb_append(sb, src.as.pointer8);
    if(src.kind==POINTER32) sb_append_buf(sb, &src.as.pointer32, sizeof(int32_t));
}

void sb_insert_ze(String_Builder *sb, Register reg)
{
    if(reg.kind!=REGISTER) UNREACHABLE("reg cannot be mem");
    sb_append(sb, '\x48'|((reg.id&8)?0x1:0x0)|((reg.id&8)?0x4:0x0));
    sb_append_cstr(sb, "\x0F\xB6");
    sb_append(sb, (3<<6)|(reg.id&7)|((reg.id&7)<<3));
}

void sb_insert_setcc(String_Builder *sb, Register reg, COND_FLAGS flag)
{
    if(reg.id>3) sb_append(sb, '\x40'|((reg.id&8)?1:0));
    sb_append(sb, '\x0F');
    sb_append(sb, '\x90'|flag);
    sb_append(sb, (3<<6)|(reg.id&7));
}

void sb_insert_get_flagimm(String_Builder *sb, Register reg, int32_t v, COND_FLAGS flag)
{
    if(reg.kind!=REGISTER) UNREACHABLE("reg cannot be mem");
    sb_insert_cmpimm(sb, reg, v);
    sb_insert_setcc(sb, reg, flag);
    sb_insert_ze(sb, reg);
}

void sb_insert_get_flag(String_Builder *sb, Register src, Register dst, COND_FLAGS flag)
{
    if(dst.kind!=REGISTER) UNREACHABLE("dst cannot be mem");
    sb_insert_cmp(sb, src, dst);
    sb_insert_setcc(sb, dst, flag);
    sb_insert_ze(sb, dst);
}

size_t sb_start_jmp(String_Builder *sb)
{
    sb_append(sb, '\xE9');
    sb_append_buf(sb, "\x00\x00\x00\x00", 4);
    return sb->count;
}

size_t sb_start_jcc(String_Builder *sb, COND_FLAGS flag)
{
    sb_append(sb, '\x0F');
    sb_append(sb, '\x80'|flag);
    sb_append_buf(sb, "\x00\x00\x00\x00", 4);
    return sb->count;
}

void sb_end_jmp(String_Builder *sb, size_t start_handle)
{
    int32_t dif = (int32_t)(sb->count-start_handle);
    memcpy(&sb->items[start_handle-4], &dif, sizeof(int32_t));
    return;
}


size_t get_jmp_marker(String_Builder *sb) {return sb->count;}

void sb_insert_jmp(String_Builder *sb, size_t jmp_handle)
{
    int32_t dif = (int32_t)(jmp_handle-(sb->count+5));
    sb_append(sb, '\xE9');
    sb_append_buf(sb, &dif, sizeof(int32_t));
}

void sb_insert_jcc(String_Builder *sb, size_t jmp_handle, COND_FLAGS flag)
{
    int32_t dif = (int32_t)(jmp_handle-(sb->count+6));
    sb_append(sb, '\x0F');
    sb_append(sb, '\x80'|flag);
    sb_append_buf(sb, &dif, sizeof(int32_t));
}

// r0 = return register
// r1-4 = paramater registers
// r5 and r6 = general purpose
Register get_register(uint8_t n)
{
#ifdef _WIN32
    return (Register){.id=(uint8_t[]){RAX, RCX, RDX, R8, R9, RDI, RSI}[n], .kind=REGISTER};
#else
    return (Register){.id=(uint8_t[]){RAX, RDI, RSI, RCX, RDX, R8, R9}[n], .kind=REGISTER};
#endif
}

Register reg_make_ptr(Register reg, int32_t offset)
{
    if(offset==0&&reg.id!=RBP)
    {
        reg.kind=POINTER;
        reg.as.pointer32=0;
    }
    else if(offset>=-128&&offset<128)
    {
        reg.kind=POINTER8;
        reg.as.pointer8=(int8_t)offset;
    }
    else
    {
        reg.kind=POINTER32;
        reg.as.pointer32=offset;
    }
    return reg;
}