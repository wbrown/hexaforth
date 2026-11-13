# Bare-metal AArch64 hexaforth for QEMU `virt`.
#
#   make            build build/hexaforth.elf and .bin around ../build/nuc.hex
#   make run        boot it in QEMU (Ctrl-A X to quit)
#   make size       native code size
#   make test       differential test against vm.c (needs python3 + unicorn)
#   make KERNEL=hexaforth-fast ...   any target, with the table-decoded kernel
#   make demo       boot into the desktop, with a disk, keyboard and pointer
#   make demo-test  drive the desktop headless and check the screen

# KERNEL picks the native kernel: hexaforth (small) or hexaforth-fast.
KERNEL ?= hexaforth
HEX    ?= ../build/nuc.hex
LOAD   ?= 0x40080000
B      := build
# Memory is 2^ADDR_BITS cells, as in vm_constants.h.
ADDR_BITS ?= 22
TARGET := --target=aarch64-none-elf

all: $(B)/$(KERNEL).bin

$(B)/image.bin: $(HEX) hex2bin.py
	@mkdir -p $(B)
	python3 hex2bin.py $(HEX) $@

# STRICT=1 builds the small kernel to halt on a fetch or store outside memory.
$(B)/%.o: %.S $(B)/image.bin
	clang $(TARGET) -c -DIMAGE='"$(B)/image.bin"' $(if $(STRICT),-DSTRICT) \
	    -DADDR_BITS=$(ADDR_BITS) $< -o $@

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
	KERNEL=$(KERNEL) ADDR_BITS=$(ADDR_BITS) python3 difftest.py

# The Mandrill test image comes from the baboon-image npm package.
$(B)/baboon.png:
	@mkdir -p $(B)
	cd $(B) && npm pack --silent baboon-image && tar xzf baboon-image-*.tgz package/baboon.png \
	    && mv package/baboon.png . && rm -rf package baboon-image-*.tgz

$(B)/disk.img: $(B)/baboon.png mkdisk.py block1.fs
	python3 mkdisk.py $(B)/baboon.png $@ block1.fs

DEVICES := -device ramfb -global virtio-mmio.force-legacy=false \
           -drive file=$(B)/disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 \
           -device virtio-keyboard-device -device virtio-tablet-device

# Boot into the desktop.  Needs a QEMU with a graphical display; the serial
# console also types into the front window.  ^Q leaves the desktop for the
# serial prompt.
demo: $(B)/$(KERNEL).elf $(B)/disk.img
	qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256 -serial mon:stdio -nic none $(DEVICES) -kernel $<

# Boot headless, drive the desktop with key and pointer events, and check
# the screen.  Screenshots are left in $(B)/gui.
demo-test: $(B)/$(KERNEL).elf $(B)/disk.img
	cp $(B)/disk.img $(B)/gui.img
	python3 guitest.py $< $(B)/gui.img $(B)/disk.tiff $(B)/gui

.PHONY: all size run test demo demo-test clean
