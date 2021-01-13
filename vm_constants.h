//
// Created by Wes Brown on 12/31/20.
//

#ifndef HEXAFORTH_VM_CONSTANTS_H
#define HEXAFORTH_VM_CONSTANTS_H

#include <stdint.h>

#define HX      "$"
#define BYTE    uint8_t
#define SBYTE   int8_t
#define CELL    int64_t
#define WORD    uint16_t
#define VM_ADDR uint64_t
#define TRUE    0xffffffffffffffff
#define FALSE   0x0000000000000000

static const CELL CELL_SZ = 64;
static const WORD UART_D = 0x1000;
static const WORD UART_STATUS = 0x2000;
static const CELL LIT_BITS = 12;
static const CELL LIT_UBITS = CELL_SZ - LIT_BITS;
static const CELL LIT_MASK = 0xffffffffffffffff << LIT_BITS;
static const CELL LIT_UMASK = 0xffffffffffffffff >> (CELL_SZ - LIT_BITS);

#endif //HEXAFORTH_VM_CONSTANTS_H
