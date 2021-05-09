OBJS = \
	bio.o\
	console.o\
	exec.o\
	file.o\
	fs.o\
	ide.o\
	ioapic.o\
	kalloc.o\
	kbd.o\
	lapic.o\
	log.o\
	main.o\
	mp.o\
	mux.o\
	picirq.o\
	pipe.o\
	proc.o\
	spinlock.o\
	string.o\
	swtch.o\
	syscall.o\
	sysfile.o\
	sysproc.o\
	timer.o\
	trapasm.o\
	trap.o\
	uart.o\
	vectors.o\
	vm.o\
	users.o\
	syschroot.o\
	bitbucket.o\
	devsysctl.o\
	kfields.o\

# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf
# Using native tools (e.g., on X86 Linux)
#TOOLPREFIX = /usr/local/bin/
# If the makefile can't find QEMU, specify its path here
QEMU = qemu-system-i386

XV6DIR = $(shell pwd)
MKFSCC = /usr/bin/clang
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
#CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
CFLAGS = -march=i386 -fno-pic -static -fno-builtin -fno-strict-aliasing -fvar-tracking -Wno-unused-variable -fplan9-extensions
CFLAGS += -fvar-tracking-assignments -O0 -g -Wall -MD -gdwarf-2 -m32 -Werror -fno-omit-frame-pointer
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -std=gnu99 -I./
ASFLAGS = -march=i386 -m32 -gdwarf-2 -Wa,-divide
# FreeBSD ld wants ``elf_i386_fbsd'' no it doesn't.
#LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)
# backport from stable
LDFLAGS += -m elf_i386

# xv6.img: bootblock kernel.elf fs.img
#	dd if=/dev/zero of=xv6.img count=10000
#	dd if=bootblock of=xv6.img conv=notrunc
#	dd if=kernel.elf of=xv6.img seek=1 conv=notrunc

bootblock: oldboot/bootasm.S oldboot/bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c oldboot/bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c oldboot/bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./Tools/sign.pl bootblock

vsdmbr: newboot/mbrasm.S newboot/mbrmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c newboot/mbrmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c newboot/mbrasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o vsdmbr.o mbrasm.o mbrmain.o
	$(OBJDUMP) -S vsdmbr.o > vsdmbr.asm
	$(OBJCOPY) -S -O binary -j .text vsdmbr.o vsdmbr
	./Tools/sign.pl vsdmbr

bootelf: newboot/bootelf.c newboot/bootentry.S newboot/smalloc.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c newboot/bootentry.S
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c newboot/bootelf.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c newboot/smalloc.c
	$(LD) $(LDFLAGS) -N -e bootelf_entry -Ttext 0x600000 -o bootelf.o_ bootentry.o bootelf.o smalloc.o
	$(OBJDUMP) -S bootelf.o_ > bootelf.asm
	$(OBJCOPY) -S -O binary -j .text bootelf.o_ bootelf

bootbin: newboot/bootbin.c newboot/bootentry.S newboot/smalloc.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c newboot/bootentry.S
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c newboot/bootbin.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c newboot/smalloc.c
	$(LD) $(LDFLAGS) -N -e bootelf_entry -Ttext 0x600000 -o bootbin.o_ bootentry.o bootbin.o smalloc.o
	$(OBJDUMP) -S bootbin.o_ > bootbin.asm
	$(OBJCOPY) -S -O binary -j .text bootbin.o_ bootbin

entryother: entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c entryother.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o bootblockother.o entryother.o
	$(OBJCOPY) -S -O binary -j .text bootblockother.o entryother
	$(OBJDUMP) -S bootblockother.o > entryother.asm

initcode: initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c initcode.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary initcode.out initcode
	$(OBJDUMP) -S initcode.o > initcode.asm

kernel.elf: $(OBJS) entry.o entryother initcode kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel.elf entry.o $(OBJS) -b binary initcode entryother
	$(OBJDUMP) -t kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym
	$(OBJCOPY) --only-keep-debug kernel.elf kernel.dsyms

kernel.bin: kernel.elf
	$(OBJCOPY) -S -O binary -j .text kernel.elf kernel.bin

tags: $(OBJS) entryother.S _init
	etags *.S *.c

vectors.o: vectors.S
	$(CC) $(CFLAGS) -nostdinc -I. -c vectors.S

sysall: fs.img

all: sysall

qemu: sysall
	sh ./Tools/qemu-nb

qemu-gdb: sysall
	sh ./Tools/qemu-nb-gdb

qemu-nb: sysall
	qemu-system-i386 -serial mon:stdio -hda fs.img -smp 2 -m 256 -no-shutdown -no-reboot

bochs: sysall
	bochs -q -f xv6-vsd.cfg

ULIB = ulib.o usys.o printf.o umalloc.o user_string.o ulocks.o

libc.a: $(ULIB)
	ar -rc libc.a $(ULIB)

_%: %.o libc.a
	$(LD) $(LDFLAGS) -N -e _start -Ttext 0 -o $@ $*.o libc.a
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym
	$(OBJCOPY) --only-keep-debug $@ $*.dsyms
#	$(OBJDUMP) -S $@ > $*.asm

# forktest is just a normal program now
#_forktest: forktest.o libc.a
#	$(LD) $(LDFLAGS) -N -e _start -Ttext 0 -o _forktest forktest.o libc.a
#	$(OBJDUMP) -S _forktest > forktest.asm

mkfs:
	$(MKFSCC) -Werror -Wall -o mkfs Tools/mkfs_new.c

mkfs_vsd:
	$(MKFSCC) -Werror -Wall -o mkfs_vsd Tools/mkfs_vsd.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
	_cat\
	_echo\
	_forktest\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_wc\
	_zombie\
	_login\
	_chroot\
	_cp\
	_uname\
	_errtest\
	_devd\
	_sleep\
	_disallow\
	_reboot\
	_halt\
	_sysctl\
	_usertests\
	_rfperms\
	_tforktest\
	_clonetest\
	_deepthreads\
	_recvtest\

fs.img: mkfs_vsd $(UPROGS) kernel.elf kernel.bin bootelf bootbin vsdmbr
	./mkfs_vsd fs.img VSDSYS $(UPROGS) kernel.elf kernel.bin bootelf bootbin @passwd @devices @rc @motd
	dd if=vsdmbr of=fs.img conv=notrunc

-include *.d

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*.o *.d *.asm *.sym *.dsyms bootblock entryother mkfs_vsd \
	initcode initcode.out kernel.bin kernel.elf xv6.img \
	fs.img kernelmemfs mkfs .gdbinit libc.a vsdmbr bootelf \
	*.o_ bootbin $(UPROGS)

fsclean:
	rm -f mkfs_vsd mkfs fs.img

