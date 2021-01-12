//
// Created by Wes Brown on 12/31/20.
//

#ifndef VLIW_TEST_COMPILER_H
#define VLIW_TEST_COMPILER_H

#include "../vm_constants.h"
#include "../vm_opcodes.h"
#include "../vm.h"

bool is_null_instruction(instruction ins);
void insert_opcode(context *ctx, instruction op);
bool compile(context *ctx, char* input);
bool compile_word(context *ctx, const char* word);
void insert_literal(context *ctx, int64_t n);
void insert_uint16(context *ctx, uint16_t n);
void insert_uint32(context *ctx, uint32_t n);
void insert_uint64(context *ctx, uint64_t n);
void insert_int64(context *ctx, int64_t n);



#endif //VLIW_TEST_COMPILER_H
