# The hexaforth CPU in Verilog.
#
#   make test             verify the 64-bit CPU against vm.c, check the memory
#                         module, and boot the nucleus on the whole system
#   make test W=32        the same for a 32-bit machine (or W=16)
#   make synth            synthesise for an ECP5 FPGA; reports size and clock
#   make synth W=32 AB=16 ... for a 32-bit cell and 2^16 cells of memory
#   make bench            how many cycles the drawing in bench.txt takes, on
#                         the 64-bit CPU with all its memory (LAT=2 for the
#                         registered memory)
#
# W is the cell width: 64, 32 or 16.  AB is the log2 of the memory's size in
# cells; the default is 128KB at every width.  The program counter is 16
# bits, which is all an execution token holds.

W       ?= 64
LAT     ?= 1
KROM    ?= 0
AB      ?= $(if $(filter 64,$(W)),14,$(if $(filter 32,$(W)),15,16))
# The nucleus is built for the machine's default size in vm_constants.h.
NUCAB   := $(if $(filter 64,$(W)),22,$(if $(filter 32,$(W)),17,16))
B       := build/c$(W)$(if $(filter 2,$(LAT)),-lat2)$(if $(filter 1,$(KROM)),-krom)
TOP     := ..
CASES   := $(B)/cases.bin
HEX     := $(TOP)/build/nuc-$(W)-$(NUCAB).hex
VM      := $(TOP)/build/hexaforth-$(W)-$(NUCAB)
VFLAGS  := -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-WIDTH -O3 \
           --x-assign fast --x-initial fast -CFLAGS "-DW=$(W) -DAB=$(AB) -DLAT=$(LAT)"

all: $(B)/tb

# Runtime, not instructions: an instruction that reads memory, shifts or
# multiplies takes more than a cycle, and with LAT=2 so does every jump.
# The display is above the 128KB the tests give the CPU, so this is built
# with the memory the nucleus is built for.
bench:
	@$(MAKE) --no-print-directory W=64 AB=22 B=$(B)-bench $(B)-bench/tb
	@$(B)-bench/tb run $(TOP)/build/nuc.hex < bench.txt 2>&1 > /dev/null | tail -1

$(B)/tb: hexaforth.v tb.cpp
	@mkdir -p $(B)
	verilator $(VFLAGS) --cc hexaforth.v -GW=$(W) -GPCW=16 -GLAT=$(LAT) \
	    --exe $(CURDIR)/tb.cpp --Mdir $(B)/cpu -o ../tb --build -j 4 > $(B)/build.log 2>&1 \
	    || (cat $(B)/build.log; false)

$(B)/tb_mem: hf_mem.v tb_mem.cpp
	@mkdir -p $(B)
	verilator $(VFLAGS) --cc hf_mem.v --exe $(CURDIR)/tb_mem.cpp --Mdir $(B)/mem -o ../tb_mem \
	    --build -j 4 > $(B)/build.log 2>&1 || (cat $(B)/build.log; false)

# The nucleus and the reference VM for this cell width.
$(HEX) $(VM):
	$(TOP)/util/build-machine.sh $(W)

# The whole system, CPU and memory, with the nucleus image built in.
$(B)/tb_soc: soc.v hexaforth.v hf_mem.v tb_soc.cpp memimage.py $(HEX)
	@mkdir -p $(B)
	python3 memimage.py $(HEX) $(B) $(W) $(AB)
	verilator $(VFLAGS) --cc soc.v hexaforth.v hf_mem.v --top-module soc -GW=$(W) -GAB=$(AB) -GLAT=$(LAT) \
	    -GINIT='"$(B)/mem.hex"' $(if $(filter 1,$(KROM)),+define+HF_KROM -I$(B)) --exe $(CURDIR)/tb_soc.cpp --Mdir $(B)/soc -o ../tb_soc \
	    --build -j 4 > $(B)/build.log 2>&1 || (cat $(B)/build.log; false)

# Cases from the reference VM built for this cell width.
$(CASES): $(TOP)/baremetal/difftest_dump.c $(TOP)/vm.c
	@mkdir -p $(B)
	gcc -O1 -w -fcommon -DCELL_BITS=$(W) -o $(B)/dump $(TOP)/baremetal/difftest_dump.c \
	    $(TOP)/vm.c $(TOP)/vm_opcodes.c $(TOP)/vm_debug.c $(TOP)/util/stack.c \
	    $(TOP)/test/compiler.c $(TOP)/test/vm_test.c -lm
	$(B)/dump $@

SESSION := printf '1 2 + .\n: sq dup * ; 12 sq .\n: f dup 1 > if dup 1- recurse * then ; 7 f .\n: g 0 swap begin dup while tuck + swap 1- repeat drop ; 10 g .\n: h 0= if 7 else 9 then ; 0 h . 1 h . hex ff . decimal\ncreate t 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,\ncreate n 0 ,\n:noname 1 n +! ; 128 128 t spawn pause pause pause n @ .\n: lp 3 0 do i . pause loop ; lp n @ .\n8000 execute\n'

test: $(B)/tb $(B)/tb_soc $(CASES) $(VM)
	$(B)/tb cases $(CASES)
	@$(SESSION) | $(VM) $(HEX) | tail -n +3 | grep -av "instructions executed" | grep -av '^$$' > $(B)/vm.out
	@$(SESSION) | $(B)/tb run $(HEX) 2> $(B)/cpu.err | grep -av '^$$' > $(B)/cpu.out; tail -1 $(B)/cpu.err
	@$(SESSION) | $(B)/tb_soc 2> /dev/null | grep -av '^$$' > $(B)/soc.out
	@cmp $(B)/vm.out $(B)/cpu.out && cmp $(B)/cpu.out $(B)/soc.out \
	    && echo "nucleus session: CPU and whole system match vm.c" && cat $(B)/cpu.out

test-mem: build/c64/tb_mem
	build/c64/tb_mem

# Synthesis.
S       := build/synth-c$(W)-ab$(AB)$(if $(filter 2,$(LAT)),-lat2)$(if $(filter 1,$(KROM)),-krom)

synth: $(if $(filter 1,$(KROM)),$(HEX))
	@mkdir -p $(S)
	$(if $(filter 1,$(KROM)),python3 memimage.py $(HEX) $(S) $(W) $(AB))
	yosys -q -l $(S)/yosys.log -p "read_verilog $(if $(filter 1,$(KROM)),-DHF_KROM -I$(S)) soc.v hexaforth.v hf_mem.v; \
	    chparam -set W $(W) -set AB $(AB) -set LAT $(LAT) soc; \
	    synth_ecp5 -top soc -json $(S)/soc.json"
	nextpnr-ecp5 --85k --package CABGA381 --json $(S)/soc.json --lpf-allow-unconstrained \
	    --timing-allow-fail -q -l $(S)/nextpnr.log || true
	@echo "W=$(W) LAT=$(LAT) KROM=$(KROM) memory 2^$(AB) cells:" \
	    $$(grep -E "Total LUT4s" $(S)/nextpnr.log | tail -1 | awk '{print $$4}' | cut -d/ -f1) LUTs, \
	    $$(grep -E "TRELLIS_FF:" $(S)/nextpnr.log | tail -1 | awk '{print $$3}' | cut -d/ -f1) flip-flops, \
	    $$(grep -E "Max frequency for clock" $(S)/nextpnr.log | tail -1 | awk '{print $$7}') MHz \
	    | tee $(S)/result.txt

clean:
	rm -rf build

.PHONY: all test test-mem synth clean
