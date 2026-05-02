#include <codegen.h>
#include <stdint.h>

#ifdef __aarch64__

#define ARM_SP 31
#define ARM_TMP0 X16
#define ARM_TMP1 X17

static void arm_emit32(String_Builder *sb, uint32_t word)
{
    sb_append_buf(sb, &word, sizeof(word));
}

static int32_t arm_mem_offset(Register reg)
{
    if(reg.kind==POINTER) return 0;
    if(reg.kind==POINTER8) return reg.as.pointer8;
    if(reg.kind==POINTER32) return reg.as.pointer32;
    UNREACHABLE("register is not a memory operand");
}

static void arm_check_reg(Register reg)
{
    if(reg.kind!=REGISTER) UNREACHABLE("expected register operand");
}

static uint32_t arm_adr(uint8_t rd, int32_t offset)
{
    if(offset < -(1 << 20) || offset >= (1 << 20)) UNREACHABLE("adr offset out of range");
    uint32_t imm = (uint32_t)offset & 0x1FFFFF;
    return 0x10000000u | ((imm & 0x3u) << 29) | (((imm >> 2) & 0x7FFFFu) << 5) | rd;
}

static uint32_t arm_b(int32_t offset)
{
    if((offset & 3)!=0) UNREACHABLE("branch target is not 4-byte aligned");
    int32_t imm = offset / 4;
    if(imm < -(1 << 25) || imm >= (1 << 25)) UNREACHABLE("branch offset out of range");
    return 0x14000000u | ((uint32_t)imm & 0x03FFFFFFu);
}

static uint32_t arm_bl(int32_t offset)
{
    if((offset & 3)!=0) UNREACHABLE("call target is not 4-byte aligned");
    int32_t imm = offset / 4;
    if(imm < -(1 << 25) || imm >= (1 << 25)) UNREACHABLE("call offset out of range");
    return 0x94000000u | ((uint32_t)imm & 0x03FFFFFFu);
}

static uint32_t arm_b_cond(int32_t offset, COND_FLAGS cond)
{
    if((offset & 3)!=0) UNREACHABLE("conditional branch target is not 4-byte aligned");
    int32_t imm = offset / 4;
    if(imm < -(1 << 18) || imm >= (1 << 18)) UNREACHABLE("conditional branch offset out of range");
    return 0x54000000u | (((uint32_t)imm & 0x7FFFFu) << 5) | (uint32_t)cond;
}

static void arm_emit_addsub_imm(String_Builder *sb, uint8_t rd, uint8_t rn, int32_t v, bool sub)
{
    if(v < 0)
    {
        arm_emit_addsub_imm(sb, rd, rn, -v, !sub);
        return;
    }

    uint32_t shift = 0;
    uint32_t imm = (uint32_t)v;
    if(imm > 4095 && (imm & 0xFFFu)==0)
    {
        imm >>= 12;
        shift = 1;
    }
    if(imm > 4095) UNREACHABLE("add/sub immediate out of range");

    uint32_t base = sub ? 0xD1000000u : 0x91000000u;
    arm_emit32(sb, base | (shift << 22) | (imm << 10) | ((uint32_t)rn << 5) | rd);
}

static void arm_emit_mov_reg(String_Builder *sb, uint8_t dst, uint8_t src)
{
    arm_emit32(sb, 0xAA0003E0u | ((uint32_t)src << 16) | dst);
}

static void arm_emit_mov_sp_to_reg(String_Builder *sb, uint8_t dst)
{
    arm_emit32(sb, 0x910003E0u | dst);
}

static void arm_emit_load(String_Builder *sb, uint8_t dst, Register src)
{
    int32_t offset = arm_mem_offset(src);
    if(offset < -256 || offset > 255) UNREACHABLE("load offset out of range");
    arm_emit32(sb, 0xF8400000u | (((uint32_t)offset & 0x1FFu) << 12) | ((uint32_t)src.id << 5) | dst);
}

static void arm_emit_store(String_Builder *sb, uint8_t src, Register dst)
{
    int32_t offset = arm_mem_offset(dst);
    if(offset < -256 || offset > 255) UNREACHABLE("store offset out of range");
    arm_emit32(sb, 0xF8000000u | (((uint32_t)offset & 0x1FFu) << 12) | ((uint32_t)dst.id << 5) | src);
}

static void arm_emit_push_reg(String_Builder *sb, uint8_t reg)
{
    arm_emit_addsub_imm(sb, ARM_SP, ARM_SP, RET_STACK_SLOT_SIZE, true);
    arm_emit32(sb, 0xF8000000u | ((uint32_t)ARM_SP << 5) | reg);
}

static void arm_emit_pop_reg(String_Builder *sb, uint8_t reg)
{
    arm_emit32(sb, 0xF8400000u | ((uint32_t)ARM_SP << 5) | reg);
    arm_emit_addsub_imm(sb, ARM_SP, ARM_SP, RET_STACK_SLOT_SIZE, false);
}

void sb_insert_pushimm(String_Builder *sb, int32_t v)
{
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, (void *)(intptr_t)v);
    arm_emit_push_reg(sb, ARM_TMP1);
}

void sb_insert_push(String_Builder *sb, Register reg)
{
    if(reg.kind==REGISTER) arm_emit_push_reg(sb, reg.id);
    else
    {
        arm_emit_load(sb, ARM_TMP1, reg);
        arm_emit_push_reg(sb, ARM_TMP1);
    }
}

void sb_insert_pop(String_Builder *sb, Register reg)
{
    if(reg.kind==REGISTER) arm_emit_pop_reg(sb, reg.id);
    else
    {
        arm_emit_pop_reg(sb, ARM_TMP1);
        arm_emit_store(sb, ARM_TMP1, reg);
    }
}

void sb_insert_push_pair(String_Builder *sb, Register a, Register b)
{
    arm_check_reg(a);
    arm_check_reg(b);
    arm_emit_addsub_imm(sb, ARM_SP, ARM_SP, RET_STACK_SLOT_SIZE, true);
    arm_emit32(sb, 0xF8000000u | ((uint32_t)ARM_SP << 5) | a.id);
    arm_emit32(sb, 0xF8000000u | (8u << 12) | ((uint32_t)ARM_SP << 5) | b.id);
}

void sb_insert_pop_pair(String_Builder *sb, Register a, Register b)
{
    arm_check_reg(a);
    arm_check_reg(b);
    arm_emit32(sb, 0xF8400000u | ((uint32_t)ARM_SP << 5) | a.id);
    arm_emit32(sb, 0xF8400000u | (8u << 12) | ((uint32_t)ARM_SP << 5) | b.id);
    arm_emit_addsub_imm(sb, ARM_SP, ARM_SP, RET_STACK_SLOT_SIZE, false);
}

void sb_insert_ret(String_Builder *sb)
{
    arm_emit_pop_reg(sb, ARM_TMP1);
    arm_emit32(sb, 0xD61F0000u | ((uint32_t)ARM_TMP1 << 5));
}

void sb_insert_word_prologue(String_Builder *sb)
{
    arm_emit_push_reg(sb, X30);
}

size_t sb_word_prologue_size(void)
{
    return 8;
}

void sb_insert_set_frame_pointer(String_Builder *sb)
{
    arm_emit_mov_sp_to_reg(sb, REG_RBP.id);
}

void sb_insert_C_call(String_Builder *sb, void *fp, String_Builder *param_code)
{
    PROLOGUE(sb);
    if(param_code!=NULL) sb_append_buf(sb, param_code->items, param_code->count);
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, fp);
    arm_emit32(sb, 0xD63F0000u | ((uint32_t)ARM_TMP1 << 5));
    EPILOGUE(sb);
}

void sb_insert_call(String_Builder *sb, void *fp)
{
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, fp);
    arm_emit32(sb, 0xD63F0000u | ((uint32_t)ARM_TMP1 << 5));
}

void sb_insert_rel_call(String_Builder *sb, size_t jmp_handle)
{
    int32_t target = (int32_t)jmp_handle - (int32_t)sb_word_prologue_size();
    int32_t offset = target - (int32_t)sb->count;
    arm_emit32(sb, arm_bl(offset));
}

void sb_insert_current_address(String_Builder *sb, Register reg)
{
    arm_check_reg(reg);
    arm_emit32(sb, arm_adr(reg.id, 4));
}

void sb_insert_movabs(String_Builder *sb, Register reg, void *v)
{
    arm_check_reg(reg);
    uintptr_t value = (uintptr_t)v;
    arm_emit32(sb, 0xD2800000u | (((uint32_t)(value & 0xFFFFu)) << 5) | reg.id);
    arm_emit32(sb, 0xF2800000u | (1u << 21) | (((uint32_t)((value >> 16) & 0xFFFFu)) << 5) | reg.id);
    arm_emit32(sb, 0xF2800000u | (2u << 21) | (((uint32_t)((value >> 32) & 0xFFFFu)) << 5) | reg.id);
    arm_emit32(sb, 0xF2800000u | (3u << 21) | (((uint32_t)((value >> 48) & 0xFFFFu)) << 5) | reg.id);
}

void sb_insert_mov(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot mov mem -> mem");
    if(src.kind==REGISTER && dst.kind==REGISTER) arm_emit_mov_reg(sb, dst.id, src.id);
    else if(src.kind!=REGISTER) arm_emit_load(sb, dst.id, src);
    else arm_emit_store(sb, src.id, dst);
}

void sb_insert_addimm(String_Builder *sb, Register reg, int32_t v)
{
    if(reg.kind==REGISTER) arm_emit_addsub_imm(sb, reg.id, reg.id, v, false);
    else
    {
        arm_emit_load(sb, ARM_TMP1, reg);
        arm_emit_addsub_imm(sb, ARM_TMP1, ARM_TMP1, v, false);
        arm_emit_store(sb, ARM_TMP1, reg);
    }
}

void sb_insert_add(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot add mem -> mem");

    if(dst.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP1, dst);
        uint8_t src_reg = src.id;
        if(src.kind!=REGISTER)
        {
            arm_emit_load(sb, ARM_TMP0, src);
            src_reg = ARM_TMP0;
        }
        arm_emit32(sb, 0x8B000000u | ((uint32_t)src_reg << 16) | ((uint32_t)ARM_TMP1 << 5) | ARM_TMP1);
        arm_emit_store(sb, ARM_TMP1, dst);
    }
    else
    {
        uint8_t src_reg = src.id;
        if(src.kind!=REGISTER)
        {
            arm_emit_load(sb, ARM_TMP1, src);
            src_reg = ARM_TMP1;
        }
        arm_emit32(sb, 0x8B000000u | ((uint32_t)src_reg << 16) | ((uint32_t)dst.id << 5) | dst.id);
    }
}

void sb_insert_subimm(String_Builder *sb, Register reg, int32_t v)
{
    if(reg.kind==REGISTER) arm_emit_addsub_imm(sb, reg.id, reg.id, v, true);
    else
    {
        arm_emit_load(sb, ARM_TMP1, reg);
        arm_emit_addsub_imm(sb, ARM_TMP1, ARM_TMP1, v, true);
        arm_emit_store(sb, ARM_TMP1, reg);
    }
}

void sb_insert_sub(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot sub mem -> mem");

    if(dst.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP1, dst);
        uint8_t src_reg = src.id;
        if(src.kind!=REGISTER)
        {
            arm_emit_load(sb, ARM_TMP0, src);
            src_reg = ARM_TMP0;
        }
        arm_emit32(sb, 0xCB000000u | ((uint32_t)src_reg << 16) | ((uint32_t)ARM_TMP1 << 5) | ARM_TMP1);
        arm_emit_store(sb, ARM_TMP1, dst);
    }
    else
    {
        uint8_t src_reg = src.id;
        if(src.kind!=REGISTER)
        {
            arm_emit_load(sb, ARM_TMP1, src);
            src_reg = ARM_TMP1;
        }
        arm_emit32(sb, 0xCB000000u | ((uint32_t)src_reg << 16) | ((uint32_t)dst.id << 5) | dst.id);
    }
}

void sb_insert_inc(String_Builder *sb, Register reg)
{
    sb_insert_addimm(sb, reg, 1);
}

void sb_insert_dec(String_Builder *sb, Register reg)
{
    sb_insert_subimm(sb, reg, 1);
}

void sb_insert_imulimm(String_Builder *sb, Register reg, int32_t v)
{
    arm_check_reg(reg);
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, (void *)(intptr_t)v);
    sb_insert_imul(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, reg);
}

void sb_insert_imul(String_Builder *sb, Register src, Register dst)
{
    if(dst.kind!=REGISTER) UNREACHABLE("dst cannot be mem");
    uint8_t src_reg = src.id;
    if(src.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP1, src);
        src_reg = ARM_TMP1;
    }
    arm_emit32(sb, 0x9B007C00u | ((uint32_t)src_reg << 16) | ((uint32_t)dst.id << 5) | dst.id);
}

void sb_insert_idivabs(String_Builder *sb, Register reg, int64_t v)
{
    arm_check_reg(reg);
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, (void *)(intptr_t)v);
    arm_emit32(sb, 0x9AC00C00u | ((uint32_t)ARM_TMP1 << 16) | ((uint32_t)reg.id << 5) | reg.id);
}

void sb_insert_idiv(String_Builder *sb, Register src, Register dst)
{
    if(dst.kind!=REGISTER) UNREACHABLE("dst cannot be mem");
    uint8_t src_reg = src.id;
    if(src.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP1, src);
        src_reg = ARM_TMP1;
    }
    arm_emit32(sb, 0x9AC00C00u | ((uint32_t)src_reg << 16) | ((uint32_t)dst.id << 5) | dst.id);
}

void sb_insert_cmpimm(String_Builder *sb, Register reg, int32_t v)
{
    arm_check_reg(reg);
    sb_insert_movabs(sb, (Register){.id=ARM_TMP1, .kind=REGISTER}, (void *)(intptr_t)v);
    arm_emit32(sb, 0xEB00001Fu | ((uint32_t)ARM_TMP1 << 16) | ((uint32_t)reg.id << 5));
}

void sb_insert_cmp(String_Builder *sb, Register src, Register dst)
{
    if(src.kind!=REGISTER&&dst.kind!=REGISTER) UNREACHABLE("cannot cmp mem -> mem");

    uint8_t dst_reg = dst.id;
    uint8_t src_reg = src.id;
    if(dst.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP0, dst);
        dst_reg = ARM_TMP0;
    }
    if(src.kind!=REGISTER)
    {
        arm_emit_load(sb, ARM_TMP1, src);
        src_reg = ARM_TMP1;
    }

    arm_emit32(sb, 0xEB00001Fu | ((uint32_t)src_reg << 16) | ((uint32_t)dst_reg << 5));
}

static void sb_insert_setcc(String_Builder *sb, Register reg, COND_FLAGS flag)
{
    arm_check_reg(reg);
    COND_FLAGS inverted = (COND_FLAGS)((uint32_t)flag ^ 1u);
    sb_insert_movabs(sb, reg, 0);
    arm_emit32(sb, arm_b_cond(8, inverted));
    sb_insert_movabs(sb, reg, (void *)1);
}

void sb_insert_get_flagimm(String_Builder *sb, Register reg, int32_t v, COND_FLAGS flag)
{
    arm_check_reg(reg);
    sb_insert_cmpimm(sb, reg, v);
    sb_insert_setcc(sb, reg, flag);
}

void sb_insert_get_flag(String_Builder *sb, Register src, Register dst, COND_FLAGS flag)
{
    if(dst.kind!=REGISTER) UNREACHABLE("dst cannot be mem");
    sb_insert_cmp(sb, src, dst);
    sb_insert_setcc(sb, dst, flag);
}

size_t sb_start_jmp(String_Builder *sb)
{
    arm_emit32(sb, arm_b(0));
    return sb->count;
}

size_t sb_start_jcc(String_Builder *sb, COND_FLAGS flag)
{
    arm_emit32(sb, arm_b_cond(0, flag));
    return sb->count;
}

void sb_end_jmp(String_Builder *sb, size_t start_handle)
{
    uint32_t insn = 0;
    size_t insn_offset = start_handle - 4;
    memcpy(&insn, &sb->items[insn_offset], sizeof(insn));

    if((insn & 0x7C000000u)==0x14000000u)
    {
        int32_t offset = (int32_t)sb->count - (int32_t)insn_offset;
        insn = (insn & 0xFC000000u) | (arm_b(offset) & 0x03FFFFFFu);
    }
    else if((insn & 0xFF000010u)==0x54000000u)
    {
        int32_t offset = (int32_t)sb->count - (int32_t)insn_offset;
        insn = (insn & 0xFF00001Fu) | (arm_b_cond(offset, (COND_FLAGS)(insn & 0xFu)) & 0x00FFFFE0u);
    }
    else if((insn & 0xFFC00000u)==0x91000000u)
    {
        uint32_t diff = (uint32_t)(sb->count - start_handle);
        if(diff > 4095) UNREACHABLE("patched add immediate out of range");
        insn = (insn & ~(0xFFFu << 10)) | (diff << 10);
    }
    else UNREACHABLE("unsupported patch target");

    memcpy(&sb->items[insn_offset], &insn, sizeof(insn));
}

size_t get_jmp_marker(String_Builder *sb) {return sb->count;}

void sb_insert_jmp(String_Builder *sb, size_t jmp_handle)
{
    int32_t offset = (int32_t)jmp_handle - (int32_t)sb->count;
    arm_emit32(sb, arm_b(offset));
}

void sb_insert_jcc(String_Builder *sb, size_t jmp_handle, COND_FLAGS flag)
{
    int32_t offset = (int32_t)jmp_handle - (int32_t)sb->count;
    arm_emit32(sb, arm_b_cond(offset, flag));
}

Register get_register(uint8_t n)
{
    return (Register){.id=(uint8_t[]){X8, X0, X1, X2, X3, X4, X9, X10}[n], .kind=REGISTER};
}

Register reg_make_ptr(Register reg, int32_t offset)
{
    if(offset==0&&reg.id!=REG_RBP.id)
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

#else

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

void sb_insert_push_pair(String_Builder *sb, Register a, Register b)
{
    sb_insert_push(sb, a);
    sb_insert_push(sb, b);
}

void sb_insert_pop_pair(String_Builder *sb, Register a, Register b)
{
    sb_insert_pop(sb, b);
    sb_insert_pop(sb, a);
}

void sb_insert_ret(String_Builder *sb)
{
    sb_append(sb, '\xC3');
}

void sb_insert_word_prologue(String_Builder *sb)
{
    (void)sb;
}

size_t sb_word_prologue_size(void)
{
    return 0;
}

void sb_insert_current_address(String_Builder *sb, Register reg)
{
    if(reg.kind!=REGISTER) UNREACHABLE("reg cannot be mem");
    sb_append(sb, '\x48'|((reg.id&8)?0x4:0x0));
    sb_append(sb, '\x8D');
    sb_append(sb, (0<<6)|((reg.id&7)<<3)|5);
    sb_append_buf(sb, "\x00\x00\x00\x00", 4);
}

void sb_insert_set_frame_pointer(String_Builder *sb)
{
    sb_append_cstr(sb, "\x48\x89\xE5");
}

#endif
