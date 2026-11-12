#!/usr/bin/env python3
"""Differential test: replay the cases dumped by difftest_dump.c on the
AArch64 kernel under Unicorn and compare the halted machine state (both
stacks, low memory, UART output) against the reference VM in vm.c."""
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import *

B = "build"
MEM_DUMP = 0x2000
DSTACK, RSTACK = 0xF000, 0xF800
UART = 0x09000000

subprocess.check_call(
    ["gcc", "-O1", "-w", "-fcommon", "-o", f"{B}/difftest_dump", "difftest_dump.c",
     "../vm.c", "../vm_opcodes.c", "../vm_debug.c", "../util/stack.c",
     "../test/compiler.c", "../test/vm_test.c", "-lm"])
subprocess.check_call([f"{B}/difftest_dump", f"{B}/cases.bin"])

syms = {}
for line in subprocess.check_output(["llvm-nm", f"{B}/hexaforth.elf"], text=True).splitlines():
    addr, _, name = line.split()
    syms[name] = int(addr, 16)
base = syms["_start"]
code = open(f"{B}/hexaforth.bin", "rb").read()
mem = syms["image"]

uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
uc.mem_map(0x40000000, 0x200000)
uc.mem_map(UART, 0x1000)
uc.mem_write(base, code)
rx, tx = [], bytearray()


def uart(uc, access, addr, size, value, _):
    off = addr - UART
    if access == 16 and off == 0x18:        # FR read: RXFE when no input left
        uc.mem_write(addr, struct.pack("<I", 0 if rx else 0x10))
    elif access == 16 and off == 0:
        uc.mem_write(addr, struct.pack("<I", rx.pop(0) if rx else 0))
    elif off == 0:
        tx.append(value & 0xFF)


uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, uart, begin=UART, end=UART + 0xFFF)
uc.emu_start(base, syms["next"])            # run the boot stub once
boot = uc.context_save()

data = open(f"{B}/cases.bin", "rb").read()
pos = 0


def take(n):
    global pos
    pos += n
    return data[pos - n:pos]


def u32():
    return struct.unpack("<I", take(4))[0]


def i32():
    return struct.unpack("<i", take(4))[0]


def stack(base_off, sp_reg, top_reg):
    depth = (uc.reg_read(sp_reg) - mem - base_off) // 8
    raw = uc.mem_read(mem + base_off + 8, 8 * max(depth - 1, 0))
    cells = list(struct.unpack(f"<{max(depth - 1, 0)}q", raw))
    if depth > 0:
        cells.append(struct.unpack("<q", struct.pack("<Q", uc.reg_read(top_reg)))[0])
    return cells


total = failed = skipped = 0
while pos < len(data):
    label = take(u32()).decode()
    eip = u32()
    image = take(MEM_DUMP)
    rx[:] = take(u32())
    sp, rsp = i32(), i32()
    want_d = list(struct.unpack(f"<{max(sp, 0)}q", take(8 * max(sp, 0))))
    want_r = list(struct.unpack(f"<{max(rsp, 0)}q", take(8 * max(rsp, 0))))
    want_out = take(u32())
    want_mem = take(MEM_DUMP)
    total += 1

    uc.context_restore(boot)
    uc.mem_write(mem, image)
    uc.mem_write(mem + MEM_DUMP, bytes(0x20000 - MEM_DUMP))
    uc.reg_write(UC_ARM64_REG_X21, eip)
    tx.clear()
    try:
        uc.emu_start(syms["next"], syms["halt"], count=100000)
        halted = uc.reg_read(UC_ARM64_REG_PC) == syms["halt"]
    except Exception as e:
        halted = False
        label += f" ({e})"

    got_d = stack(DSTACK, UC_ARM64_REG_X24, UC_ARM64_REG_X22)
    got_r = stack(RSTACK, UC_ARM64_REG_X25, UC_ARM64_REG_X23)
    got_mem = bytes(uc.mem_read(mem, MEM_DUMP))
    problems = []
    if not halted:
        problems.append("did not halt")
    if got_d != want_d:
        problems.append(f"dstack {got_d} != {want_d}")
    if got_r != want_r:
        problems.append(f"rstack {got_r} != {want_r}")
    if got_mem != want_mem:
        at = next(i for i in range(MEM_DUMP) if got_mem[i] != want_mem[i])
        problems.append(f"memory differs at 0x{at:x}")
    if bytes(tx) != want_out:
        problems.append(f"output {bytes(tx)!r} != {want_out!r}")
    if problems:
        failed += 1
        if failed <= 40:
            print(f"FAIL {label}: " + "; ".join(problems))

print(f"{total - failed}/{total} cases match vm.c")

# Code window (no counterpart in vm.c): map the block at 0x10000 over word
# addresses 0x1000+, jump into it, and check that execution is redirected
# while a data read of byte 0x2000 still sees the unmapped contents.
words = lambda *w: struct.pack(f"<{len(w)}H", *w)
uc.context_restore(boot)
uc.mem_write(mem, bytes(0x20000))
uc.mem_write(mem, words(0x9010, 0x80B0, 0x6208, 0x1000))   # h# 10000 h# b0 io!  jump 0x1000
uc.mem_write(mem + 0x2000, words(0x8007))                  # identity window: 7
uc.mem_write(mem + 0x10000, words(0x802A, 0x9002, 0x6800)) # banked block: 42 h# 2000 @
uc.emu_start(syms["next"], syms["halt"], count=1000)
got = stack(DSTACK, UC_ARM64_REG_X24, UC_ARM64_REG_X22)
if got == [42, 0x8007]:
    print("code window: ok")
else:
    print(f"FAIL code window: dstack {got} != [42, 32775]")
    failed += 1
sys.exit(1 if failed else 0)
