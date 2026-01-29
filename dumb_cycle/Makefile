.PHONY: all clean test debug
CC = gcc
CFLAGS = -ffreestanding -Wall -std=c99 -pedantic \
	-fsanitize-trap -fsanitize=undefined -g -O0
LD = ld
LDFLAGS =
AS = as
ASFLAGS =

all: initramfs.cpio
clean: clean_dumb_cycle clean_initramfs.cpio

dumb_cycle: src/main.o src/runtime.o
	$(LD) $(LDFLAGS) -o dumb_cycle src/main.o src/runtime.o
clean_dumb_cycle: clean_main clean_runtime
	rm -f dumb_cycle

src/main.o: src/main.c
	$(CC) $(CFLAGS) -c -o src/main.o src/main.c
clean_main:
	rm -f src/main.o

src/runtime.o: src/runtime.s
	$(AS) $(ASFLAGS) -o src/runtime.o src/runtime.s
clean_runtime:
	rm -f src/runtime.o

initramfs.cpio: dumb_cycle
	mkdir -p fs
	cp dumb_cycle fs/init
	cd fs; find . | cpio -o --format=newc > ../initramfs.cpio
clean_initramfs.cpio:
	rm -f initramfs.cpio
	rm -rf fs

test: initramfs.cpio
	qemu-system-x86_64 \
		-kernel vmlinuz \
		-initrd initramfs.cpio \
		-enable-kvm -display sdl

debug: initramfs.cpio
	qemu-system-x86_64 \
		-kernel vmlinuz \
		-initrd initramfs.cpio \
		-enable-kvm -display sdl -s -S
