# The hexaforth CPU in Verilog.
#
#   make test             verify the 64-bit CPU against vm.c, check the memory
#                         module, and boot the nucleus on the whole system
#   make test W=32        the same for a 32-bit machine (or W=16)
#   make synth            synthesise for an ECP5 FPGA; reports size and clock
#   make synth W=32 A=24  ... for a 32-bit cell and 24-bit addressing
#
# W is the cell width: 64, 32 or 16.  For simulation the memory is 128KB, or
# 64KB with a 16-bit cell.  For synthesis A is the address width in bits (the
# program counter is A-1 bits of word address) and MEMBITS the log2 of the
# memory actually built.

W       ?= 64
ALIGNED ?= 0
STACKRAM ?= 1
PLANES  ?= 0
SIMMB   := $(if $(filter 16,$(W)),16,17)
B       := build/w$(W)$(if $(filter 1,$(ALIGNED)),-aligned)$(if $(filter 0,$(STACKRAM)),-shift)
TOP     := ..
CASES   := $(B)/cases.bin
HEX     := $(TOP)/build/nuc-$(W)-$(SIMMB).hex
VM      := $(TOP)/build/hexaforth-$(W)-$(SIMMB)
VFLAGS  := -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-WIDTH -O3 \
           --x-assign fast --x-initial fast -CFLAGS "-DW=$(W) -DMB=$(SIMMB) -DSTACKRAM=$(STACKRAM)"

all: $(B)/tb

$(B)/tb: hexaforth.v tb.cpp
	@mkdir -p $(B)
	verilator $(VFLAGS) --cc hexaforth.v -GW=$(W) -GPCW=$$(($(SIMMB)-1)) -GMB=$(SIMMB) -GSTACKRAM=$(STACKRAM) \
	    --exe $(CURDIR)/tb.cpp --Mdir $(B)/cpu -o ../tb --build -j 4 > $(B)/build.log 2>&1 \
	    || (cat $(B)/build.log; false)

$(B)/tb_mem: hf_mem.v tb_mem.cpp
	@mkdir -p $(B)
	verilator $(VFLAGS) --cc hf_mem.v --exe $(CURDIR)/tb_mem.cpp --Mdir $(B)/mem -o ../tb_mem \
	    --build -j 4 > $(B)/build.log 2>&1 || (cat $(B)/build.log; false)

# The nucleus and the reference VM for this cell width.
$(HEX) $(VM):
	$(TOP)/util/build-machine.sh $(W) $(SIMMB)

# The whole system, CPU and lane memory, with the nucleus image built in.
$(B)/tb_soc: soc.v hexaforth.v hf_mem.v tb_soc.cpp lanes.py $(HEX)
	@mkdir -p $(B)
	python3 lanes.py $(HEX) $(B)/lane $(W) $(SIMMB)
	verilator $(VFLAGS) --cc soc.v hexaforth.v hf_mem.v --top-module soc -GW=$(W) -GMB=$(SIMMB) -GALIGNED=$(ALIGNED) -GSTACKRAM=$(STACKRAM) \
	    -GINIT='"$(B)/lane"' --exe $(CURDIR)/tb_soc.cpp --Mdir $(B)/soc -o ../tb_soc \
	    --build -j 4 > $(B)/build.log 2>&1 || (cat $(B)/build.log; false)

# Cases from the reference VM built for this cell width.
$(CASES): $(TOP)/baremetal/difftest_dump.c $(TOP)/vm.c
	@mkdir -p $(B)
	gcc -O1 -w -fcommon -DCELL_BITS=$(W) -o $(B)/dump $(TOP)/baremetal/difftest_dump.c \
	    $(TOP)/vm.c $(TOP)/vm_opcodes.c $(TOP)/vm_debug.c $(TOP)/util/stack.c \
	    $(TOP)/test/compiler.c $(TOP)/test/vm_test.c -lm
	$(B)/dump $@

SESSION := printf '1 2 + .\n: sq dup * ; 12 sq .\n: f dup 1 > if dup 1- recurse * then ; 7 f .\n8000 execute\n'

test: $(B)/tb $(B)/tb_soc $(CASES) $(VM)
	$(B)/tb cases $(CASES)
	@$(SESSION) | $(VM) $(HEX) | tail -n +3 | grep -av "instructions executed" | grep -av '^$$' > $(B)/vm.out
	@$(SESSION) | $(B)/tb run $(HEX) 2> $(B)/cpu.err | grep -av '^$$' > $(B)/cpu.out; tail -1 $(B)/cpu.err
	@$(SESSION) | $(B)/tb_soc 2> /dev/null | grep -av '^$$' > $(B)/soc.out
	@cmp $(B)/vm.out $(B)/cpu.out && cmp $(B)/cpu.out $(B)/soc.out \
	    && echo "nucleus session: CPU and whole system match vm.c" && cat $(B)/cpu.out

test-mem: build/w64/tb_mem
	build/w64/tb_mem

# Synthesis.
A       ?= $(if $(filter 16,$(W)),16,17)
MEMBITS ?= $(if $(filter 16,$(W)),16,17)
S       := build/synth-w$(W)-a$(A)-m$(MEMBITS)$(if $(filter 1,$(ALIGNED)),-aligned)$(if $(filter 0,$(STACKRAM)),-shift)$(if $(filter 1,$(PLANES)),-planes)

synth:
	@mkdir -p $(S)
	yosys -q -l $(S)/yosys.log -p "read_verilog soc.v hexaforth.v hf_mem.v; \
	    chparam -set W $(W) -set MB $(MEMBITS) -set PCW $$(($(A)-1)) -set ALIGNED $(ALIGNED) -set STACKRAM $(STACKRAM) -set PLANES $(PLANES) soc; \
	    synth_ecp5 -top soc -json $(S)/soc.json"
	nextpnr-ecp5 --85k --package CABGA381 --json $(S)/soc.json --lpf-allow-unconstrained \
	    --timing-allow-fail -q -l $(S)/nextpnr.log || true
	@echo "W=$(W) A=$(A) ALIGNED=$(ALIGNED) STACKRAM=$(STACKRAM) PLANES=$(PLANES) memory 2^$(MEMBITS) bytes:" \
	    $$(grep -E "Total LUT4s" $(S)/nextpnr.log | tail -1 | awk '{print $$4}' | cut -d/ -f1) LUTs, \
	    $$(grep -E "TRELLIS_FF:" $(S)/nextpnr.log | tail -1 | awk '{print $$3}' | cut -d/ -f1) flip-flops, \
	    $$(grep -E "Max frequency for clock" $(S)/nextpnr.log | tail -1 | awk '{print $$7}') MHz \
	    | tee $(S)/result.txt

clean:
	rm -rf build

.PHONY: all test test-mem synth clean
