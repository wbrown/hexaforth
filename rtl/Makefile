# The hexaforth CPU in Verilog.
#
#   make test     replay the differential cases against vm.c, check the memory
#                 module, and boot the nucleus in simulation
#   make synth    synthesise the CPU and memory for an ECP5 FPGA and report
#                 size and estimated clock speed

B     := build
CASES := ../baremetal/build/cases.bin
HEX   := ../build/nuc.hex
VFLAGS := -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -O3 --x-assign fast --x-initial fast

all: $(B)/tb

$(B)/tb: hexaforth.v tb.cpp
	verilator $(VFLAGS) --cc hexaforth.v --exe $(CURDIR)/tb.cpp --Mdir $(B)/cpu -o ../tb --build -j 4 > $(B).log 2>&1 \
	    || (cat $(B).log; false)

$(B)/tb_mem: hf_mem.v tb_mem.cpp
	verilator $(VFLAGS) --cc hf_mem.v --exe $(CURDIR)/tb_mem.cpp --Mdir $(B)/mem -o ../tb_mem --build -j 4 > $(B).log 2>&1 \
	    || (cat $(B).log; false)

$(CASES):
	$(MAKE) -C ../baremetal test

# The whole system, CPU and lane memory, with the nucleus image built in.
$(B)/tb_soc: soc.v hexaforth.v hf_mem.v tb_soc.cpp lanes.py $(HEX)
	python3 lanes.py $(HEX) $(B)/lane
	verilator $(VFLAGS) --cc soc.v hexaforth.v hf_mem.v --top-module soc -GINIT='"$(B)/lane"' \
	    --exe $(CURDIR)/tb_soc.cpp --Mdir $(B)/soc -o ../tb_soc --build -j 4 > $(B).log 2>&1 \
	    || (cat $(B).log; false)

test: $(B)/tb $(B)/tb_mem $(B)/tb_soc $(CASES)
	$(B)/tb cases $(CASES)
	$(B)/tb_mem
	printf '1 2 + .\n: sq dup * ; 12 sq .\n8000 execute\n' | $(B)/tb_soc

synth:
	@mkdir -p $(B)
	yosys -q -l $(B)/yosys.log -p "read_verilog soc.v hexaforth.v hf_mem.v; \
	    synth_ecp5 -top soc -json $(B)/soc.json"
	@sed -n '/Number of cells/,$$p' $(B)/yosys.log | head -40
	nextpnr-ecp5 --85k --package CABGA381 --json $(B)/soc.json --lpf-allow-unconstrained \
	    --timing-allow-fail -q -l $(B)/nextpnr.log || true
	@grep -E "Max frequency for clock|Total LUT4s|TRELLIS_FF|DP16KD|MULT18" $(B)/nextpnr.log | tail -8

clean:
	rm -rf $(B) $(B).log

.PHONY: all test synth clean
