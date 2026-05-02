#ifndef CODEGEN_H
#define CODEGEN_H

#include <nob.h>

#ifdef __aarch64__
#define PROLOGUE(sb) sb_insert_push_pair(sb, get_register(1), get_register(2))
#define EPILOGUE(sb) sb_insert_pop_pair(sb, get_register(1), get_register(2))

typedef enum
{
    X0,
    X1,
    X2,
    X3,
    X4,
    X5,
    X6,
    X7,
    X8,
    X9,
    X10,
    X11,
    X12,
    X13,
    X14,
    X15,
    X16,
    X17,
    X18,
    X19,
    X20,
    X21,
    X22,
    X23,
    X24,
    X25,
    X26,
    X27,
    X28,
    X29,
    X30,
}
REGISTERS;

typedef enum
{
    POINTER,
    POINTER8,
    POINTER32,
    REGISTER,
}
REGISTER_TYPES;

typedef struct
{
    uint8_t id;
    REGISTER_TYPES kind;
    union
    {
        int8_t pointer8;
        int32_t pointer32;
    }
    as;
}
Register;

typedef enum
{
    EQ=0x0,
    NE=0x1,
    GE=0xA,
    LT=0xB,
    GT=0xC,
    LE=0xD,
}
COND_FLAGS;

#define REG_RAX (Register){.id=X8, .kind=REGISTER}
#define REG_RBP (Register){.id=X20, .kind=REGISTER}
#define RET_STACK_SLOT_SIZE 16
#else
#define PROLOGUE(sb) sb_append_cstr(sb, "\x55\x48\x89\xE5\x48\x83\xEC\x30"); sb_insert_mov(sb, get_register(1), reg_make_ptr(REG_RBP,-8)); sb_insert_mov(sb, get_register(2), reg_make_ptr(REG_RBP,-16))
#define EPILOGUE(sb) sb_insert_mov(sb, reg_make_ptr(REG_RBP,-8), get_register(1)); sb_insert_mov(sb, reg_make_ptr(REG_RBP,-16), get_register(2)); sb_append_cstr(sb, "\x48\x83\xC4\x30\x5D")

typedef enum
{
    RAX,
    RCX,
    RDX,
    RBP=5,
    RSI,
    RDI,
    R8,
    R9,
}
REGISTERS;

typedef enum
{
    POINTER,
    POINTER8,
    POINTER32,
    REGISTER,
}
REGISTER_TYPES;

typedef struct
{
    uint8_t id;
    REGISTER_TYPES kind;
    union
    {
        int8_t pointer8;
        int32_t pointer32;
    } 
    as;
}
Register;

typedef enum
{
    EQ=0x4,
    NE,
    LT=0xC,
    GE,
    LE,
    GT,
}
COND_FLAGS;

#define REG_RAX (Register){.id=RAX, .kind=REGISTER}
#define REG_RBP (Register){.id=RBP, .kind=REGISTER}
#define RET_STACK_SLOT_SIZE 8
#endif

#define LOOP_LIMIT_OFFSET (RET_STACK_SLOT_SIZE)
#define LOOP_INDEX_OFFSET (2*RET_STACK_SLOT_SIZE)
#define LOOP_OUTER_INDEX_OFFSET (5*RET_STACK_SLOT_SIZE)

void sb_insert_C_call(String_Builder *sb, void *fp, String_Builder *param_code);
void sb_insert_call(String_Builder *sb, void *fp);
void sb_insert_rel_call(String_Builder *sb, size_t jmp_handle);
void sb_insert_current_address(String_Builder *sb, Register reg);

void sb_insert_movabs(String_Builder *sb, Register reg, void *v);
void sb_insert_mov(String_Builder *sb, Register src, Register dst);
void sb_insert_inc(String_Builder *sb, Register reg);
void sb_insert_addimm(String_Builder *sb, Register reg, int32_t v);
void sb_insert_add(String_Builder *sb, Register src, Register dst);
void sb_insert_dec(String_Builder *sb, Register reg);
void sb_insert_subimm(String_Builder *sb, Register reg, int32_t v);
void sb_insert_sub(String_Builder *sb, Register src, Register dst);
void sb_insert_imulimm(String_Builder *sb, Register reg, int32_t v);
void sb_insert_imul(String_Builder *sb, Register src, Register dst);
void sb_insert_idivabs(String_Builder *sb, Register reg, int64_t v);
void sb_insert_idiv(String_Builder *sb, Register src, Register dst);
void sb_insert_cmpimm(String_Builder *sb, Register reg, int32_t v);
void sb_insert_cmp(String_Builder *sb, Register src, Register dst);

void sb_insert_pushimm(String_Builder *sb, int32_t v);
void sb_insert_push(String_Builder *sb, Register reg);
void sb_insert_pop(String_Builder *sb, Register reg);
void sb_insert_push_pair(String_Builder *sb, Register a, Register b);
void sb_insert_pop_pair(String_Builder *sb, Register a, Register b);
void sb_insert_ret(String_Builder *sb);
void sb_insert_word_prologue(String_Builder *sb);
size_t sb_word_prologue_size(void);
void sb_insert_set_frame_pointer(String_Builder *sb);

void sb_insert_get_flagimm(String_Builder *sb, Register reg, int32_t v, COND_FLAGS flag);
void sb_insert_get_flag(String_Builder *sb, Register src, Register dst, COND_FLAGS flag);

size_t sb_start_jmp(String_Builder *sb);
size_t sb_start_jcc(String_Builder *sb, COND_FLAGS flag);
void sb_end_jmp(String_Builder *sb, size_t start_handle);

size_t get_jmp_marker(String_Builder *sb);
void sb_insert_jmp(String_Builder *sb, size_t jmp_handle);
void sb_insert_jcc(String_Builder *sb, size_t jmp_handle, COND_FLAGS flag);

Register get_register(uint8_t n);
Register reg_make_ptr(Register reg, int32_t offset);

#endif
