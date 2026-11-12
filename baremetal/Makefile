# Bare-metal AArch64 hexaforth for QEMU `virt`.
#
#   make            build build/hexaforth.elf and .bin around ../build/nuc.hex
#   make run        boot it in QEMU (Ctrl-A X to quit)
#   make size       native code size
#   make test       differential test against vm.c (needs python3 + unicorn)

HEX    ?= ../build/nuc.hex
LOAD   ?= 0x40080000
B      := build
TARGET := --target=aarch64-none-elf

all: $(B)/hexaforth.bin

$(B)/image.bin: $(HEX) hex2bin.py
	@mkdir -p $(B)
	python3 hex2bin.py $(HEX) $@

$(B)/hexaforth.o: hexaforth.S $(B)/image.bin
	clang $(TARGET) -c -DIMAGE='"$(B)/image.bin"' hexaforth.S -o $@

$(B)/hexaforth.elf: $(B)/hexaforth.o
	ld.lld -Ttext=$(LOAD) -e _start -o $@ $<

$(B)/hexaforth.bin: $(B)/hexaforth.elf
	llvm-objcopy -O binary $< $@

size: $(B)/hexaforth.elf
	@llvm-nm --radix=d $< | awk '/ native_end$$/{e=$$1} / _start$$/{s=$$1} \
	    END{print "native code: " e-s " bytes"}'
	@ls -l $(B)/hexaforth.bin | awk '{print "whole image: " $$5 " bytes"}'

run: $(B)/hexaforth.elf
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -nic none -kernel $<

test: $(B)/hexaforth.bin
	python3 difftest.py

clean:
	rm -rf $(B)

.PHONY: all size run test clean
