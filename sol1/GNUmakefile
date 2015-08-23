#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
OBJDIR := obj

ifdef GCCPREFIX
SETTINGGCCPREFIX := true
else
-include conf/gcc.mk
endif

ifdef LAB
SETTINGLAB := true
else
-include conf/lab.mk
endif

-include conf/env.mk

ifndef SOL
SOL := 0
endif
ifndef LABADJUST
LABADJUST := 0
endif


TOP = .

# Cross-compiler jos toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'i386-jos-elf-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly by doing
#
#	make 'GCCPREFIX=i386-jos-elf-' gccsetup
#

CC	:= $(GCCPREFIX)gcc -pipe
GCC_LIB := $(shell $(CC) -print-libgcc-file-name)
AS	:= $(GCCPREFIX)as
AR	:= $(GCCPREFIX)ar
LD	:= $(GCCPREFIX)ld
OBJCOPY	:= $(GCCPREFIX)objcopy
OBJDUMP	:= $(GCCPREFIX)objdump
NM	:= $(GCCPREFIX)nm

# Native commands
NCC	:= gcc $(CC_VER) -pipe
TAR	:= gtar
PERL	:= perl

# Compiler flags
# Note that -O2 is required for the boot loader to fit within 512 bytes;
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
CFLAGS	:= $(CFLAGS) $(DEFS) -O2 -fno-builtin -I$(TOP) -MD -Wall -Wno-format -ggdb

# Linker flags for user programs
ULDFLAGS := -Ttext 0x800020

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=


# Make sure that 'all' is the first target
all:


# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/%.o $(OBJDIR)/boot/%.o $(OBJDIR)/fs/%.o \
	$(OBJDIR)/user/%.o

# Rules for building regular object files
$(OBJDIR)/%.o: %.c
	@echo + cc $<
	$(V)mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.S
	@echo + as $<
	$(V)mkdir -p $(@D)
	$(V)$(CC) -nostdinc $(CFLAGS) -c -o $@ $<

# try to infer the correct GCCPREFIX
conf/gcc.mk:
	@if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=i386-jos-elf-' >conf/gcc.mk; \
	elif objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=' >conf/gcc.mk; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'echo GCCPREFIX= >conf/gcc.mk'." 1>&2; \
	echo "***" 1>&2; exit 1; fi
	@f=`grep GCCPREFIX conf/gcc.mk | sed 's/.*=//'`; if echo $$f | grep '^[12]\.' >/dev/null 2>&1; then echo "***" 1>&2; \
	echo "*** Error: Your gcc compiler is too old." 1>&2; \
	echo "*** The labs will only work with gcc-3.0 or later, and are only" 1>&2; \
	echo "*** tested on gcc-3.3 and later." 1>&2; \
	echo "***" 1>&2; exit 1; fi


# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
include lib/Makefrag
include user/Makefrag
include fs/Makefrag


bochs: $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs-nogui

# For deleting the build
clean:
	rm -rf $(OBJDIR) lab$(LAB).tar.gz

grade:
	$(V)$(MAKE) clean >/dev/null 2>/dev/null
	$(MAKE) all
	sh grade.sh

HANDIN_CMD = tar cf - . | gzip | uuencode lab$(LAB).tar.gz | mail $(HANDIN_EMAIL)
handin: clean
	$(HANDIN_CMD)
tarball: clean
	tar cf - `ls -a | grep -v '^\.*$$' | grep -v '^lab$(LAB)\.tar\.gz'` | gzip > lab$(LAB).tar.gz

# For test runs
run-%:
	$(V)rm -f $(OBJDIR)/kern/init.o $(OBJDIR)/kern/bochs.img
	$(V)$(MAKE) "DEFS=-DTEST=binary_user_$*_start -DTESTSIZE=binary_user_$*_size" $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs-nogui

xrun-%:
	$(V)rm -f $(OBJDIR)/kern/init.o $(OBJDIR)/kern/bochs.img
	$(V)$(MAKE) "DEFS=-DTEST=binary_user_$*_start -DTESTSIZE=binary_user_$*_size" $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

# Create a patch from ../lab$(LAB).tar.gz.
patch patch.diff:
	@test -r ../lab$(LAB).tar.gz || (echo "***" 1>&2; \
	echo "*** Can't find '../lab$(LAB).tar.gz'.  Download it" 1>&2; \
	echo "*** into my parent directory and try again." 1>&2; \
	echo "***" 1>&2; false)
	(gzcat ../lab$(LAB).tar.gz 2>/dev/null || zcat ../lab$(LAB).tar.gz) | tar xf -
	@pkgdate=`grep PACKAGEDATE lab$(LAB)/conf/lab.mk | sed 's/.*=//'`; \
	test "$(PACKAGEDATE)" = "$$pkgdate" || (echo "***" 1>&2; \
	echo "*** The ../lab$(LAB).tar.gz tarball was created on $$pkgdate," 1>&2; \
	echo "*** but your work directory was expanded from a tarball created" 1>&2; \
	echo "*** on $(PACKAGEDATE)!  I can't tell the difference" 1>&2; \
	echo "*** between your changes and the changes between the tarballs," 1>&2; \
	echo "*** so I won't create an automatic patch." 1>&2; \
	echo "***" 1>&2; false)
	@rm -f patch.diff
	@for f in `cd lab$(LAB) && find . -type f -print`; do \
	diff -u lab$(LAB)/$$f $$f >>patch.diff || echo "*** $$f differs; appending to patch.diff" 1>&2; done
	@test -n patch.diff || echo "*** No differences found" 1>&2
	@rm -rf lab$(LAB)

apply-patch:
	@test -r patch.diff || (echo "***" 1>&2; \
	echo "*** No 'patch.diff' file found!  Did you remember to" 1>&2; \
	echo "*** run 'make patch'?" 1>&2; \
	echo "***" 1>&2; false)
	patch -p0 <patch.diff

always:

.PHONY: all always patch apply-patch handin tarball grade
