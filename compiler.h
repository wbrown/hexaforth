//
// Created by Wes Brown on 12/31/20.
//

#ifndef VLIW_TEST_COMPILER_H
#define VLIW_TEST_COMPILER_H

#include "vm_constants.h"
#include "vm_opcodes.h"
#include "vm.h"

instruction* lookup_word(const char* word);
bool is_null_instruction(instruction ins);
void insert_opcode(context *ctx, instruction op);
void compile_word(context *ctx, const char* word);
void insert_literal(context *ctx, int64_t n);


#endif //VLIW_TEST_COMPILER_H
