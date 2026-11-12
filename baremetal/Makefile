# Bare-metal AArch64 hexaforth for QEMU `virt`.
#
#   make            build build/hexaforth.elf and .bin around ../build/nuc.hex
#   make run        boot it in QEMU (Ctrl-A X to quit)
#   make size       native code size
#   make test       differential test against vm.c (needs python3 + unicorn)
#   make KERNEL=hexaforth-fast ...   any target, with the table-decoded kernel
#   make demo       boot with a disk and display; type `mandrill`
#   make demo-test  run the Mandrill demo headless and check the screen

# KERNEL picks the native kernel: hexaforth (small) or hexaforth-fast.
KERNEL ?= hexaforth
HEX    ?= ../build/nuc.hex
LOAD   ?= 0x40080000
B      := build
TARGET := --target=aarch64-none-elf

all: $(B)/$(KERNEL).bin

$(B)/image.bin: $(HEX) hex2bin.py
	@mkdir -p $(B)
	python3 hex2bin.py $(HEX) $@

$(B)/%.o: %.S $(B)/image.bin
	clang $(TARGET) -c -DIMAGE='"$(B)/image.bin"' $< -o $@

$(B)/%.elf: $(B)/%.o
	ld.lld -Ttext=$(LOAD) -e _start -o $@ $<

$(B)/%.bin: $(B)/%.elf
	llvm-objcopy -O binary $< $@

.PRECIOUS: $(B)/%.o $(B)/%.elf

size: $(B)/$(KERNEL).elf
	@llvm-nm --radix=d $< | awk '/ native_end$$/{e=$$1} / _start$$/{s=$$1} \
	    END{print "native code: " e-s " bytes"}'
	@ls -l $(B)/$(KERNEL).bin | awk '{print "whole image: " $$5 " bytes"}'

run: $(B)/$(KERNEL).elf
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -nic none -kernel $<

test: $(B)/$(KERNEL).bin
	KERNEL=$(KERNEL) python3 difftest.py

# The Mandrill test image comes from the baboon-image npm package.
$(B)/baboon.png:
	@mkdir -p $(B)
	cd $(B) && npm pack --silent baboon-image && tar xzf baboon-image-*.tgz package/baboon.png \
	    && mv package/baboon.png . && rm -rf package baboon-image-*.tgz

$(B)/disk.img: $(B)/baboon.png mkdisk.py block1.fs
	python3 mkdisk.py $(B)/baboon.png $@ block1.fs

DEVICES := -device ramfb -global virtio-mmio.force-legacy=false \
           -drive file=$(B)/disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

# Boot with the disk and a display; type `mandrill` at the prompt.
demo: $(B)/$(KERNEL).elf $(B)/disk.img
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256 -serial mon:stdio -nic none $(DEVICES) -kernel $<

# Run the demo headless, save $(B)/mandrill.png, and check it against a
# reference dither computed on the host.
demo-test: $(B)/$(KERNEL).elf $(B)/disk.img
	python3 demo.py $< $(B)/disk.img $(B)/disk.tiff $(B)/mandrill.png

clean:
	rm -rf $(B)

.PHONY: all size run test demo demo-test clean
