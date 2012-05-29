##########################################################################
# Copyright (c) 2009, 2010, 2011 ETH Zurich.
# All rights reserved.
#
# This file is distributed under the terms in the attached LICENSE file.
# If you do not find this file, copies can be found by writing to:
# ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
#
# This file defines symbolic (i.e. non-file) targets for the Makefile
# generated by Hake.  Edit this to add your own symbolic targets.
#
##########################################################################

# Disable built-in implicit rules. GNU make adds environment's MAKEFLAGS too.
MAKEFLAGS=r

# Set default architecture to the first specified by Hake in generated Makefile.
ARCH ?= $(word 1, $(HAKE_ARCHS))

# All binaries of the RCCE LU benchmark
BIN_RCCE_LU= \
	sbin/rcce_lu_A1 \
	sbin/rcce_lu_A2 \
	sbin/rcce_lu_A4 \
	sbin/rcce_lu_A8 \
	sbin/rcce_lu_A16 \
	sbin/rcce_lu_A32 \
	sbin/rcce_lu_A64

# All binaries of the RCCE BT benchmark
BIN_RCCE_BT= \
	sbin/rcce_bt_A1 \
	sbin/rcce_bt_A4 \
	sbin/rcce_bt_A9  \
	sbin/rcce_bt_A16 \
	sbin/rcce_bt_A25 \
	sbin/rcce_bt_A36

# Default list of modules to build/install for all enabled architectures
MODULES_COMMON= \
	sbin/cpu \
	sbin/init_null \
	sbin/init \
	sbin/chips \
	sbin/spawnd \
	sbin/startd \
	sbin/flounder_stubs_empty_bench \
	sbin/flounder_stubs_buffer_bench \
	sbin/flounder_stubs_payload_bench \
	sbin/hellotest \
	sbin/mem_serv \
	sbin/idctest \
	sbin/memtest \
	sbin/fread_test \
	sbin/fscanf_test \
	sbin/monitor \
	sbin/ramfsd \
	sbin/channel_cost_bench \
	sbin/schedtest \
	sbin/testerror \
	sbin/yield_test \
	sbin/xcorecap \
	sbin/xcorecapserv \
	sbin/xcorecapbench \

# List of modules that are arch-independent and always built
MODULES_GENERIC= \
	skb_ramfs.cpio.gz \

# x86_64-specific modules to build by default
# this should shrink as targets are ported and move into the generic list above
MODULES_x86_64= \
	sbin/ahci_bench \
	sbin/ata_rw28_test \
	sbin/apicdrift_bench \
	sbin/bench \
	sbin/bfscope \
	sbin/bomp_benchmark_cg \
	sbin/bomp_benchmark_ft \
	sbin/bomp_benchmark_is \
	sbin/bomp_sync \
	sbin/bomp_cpu_bound \
	sbin/bomp_cpu_bound_progress \
	sbin/bomp_sync_progress \
	sbin/bomp_test \
	sbin/boot_perfmon \
	sbin/bulkbench \
	sbin/datagatherer \
	sbin/ahcid \
	sbin/e1000n \
	sbin/rtl8029 \
	sbin/netd \
	sbin/echoserver \
	sbin/elver \
	sbin/fbdemo \
	sbin/fish \
	sbin/fputest \
	sbin/hpet \
	sbin/lpc_kbd \
	sbin/lpc_timer \
	sbin/lrpc_bench \
	sbin/mem_affinity \
	sbin/mem_serv_dist \
	sbin/net-test \
	sbin/netthroughput \
	sbin/pci \
	sbin/placement_bench \
	sbin/phases_bench \
	sbin/phases_scale_bench \
	sbin/phoenix_kmeans \
	$(BIN_RCCE_BT) \
	$(BIN_RCCE_LU) \
	sbin/rcce_pingpong \
	sbin/serial \
	sbin/shared_mem_clock_bench \
	sbin/sif \
	sbin/slideshow \
	sbin/skb \
	sbin/spantest \
	sbin/testconcurrent \
	sbin/thc_v_flounder_empty \
	sbin/thcidctest \
	sbin/thcminitest \
	sbin/thctest \
	sbin/tsc_bench \
	sbin/tweedtest \
	sbin/udp_throughput \
	sbin/ump_latency \
	sbin/ump_exchange \
	sbin/ump_latency_cache \
	sbin/ump_throughput \
	sbin/ump_send \
	sbin/ump_receive \
	sbin/vbe \
	sbin/vmkitmon \
	sbin/webserver \
	sbin/tlstest \
	sbin/timer_test \
	sbin/net_openport_test \
	sbin/examples/xmpl-perfmon \
	sbin/routing_setup \
	sbin/multihoptest \
	sbin/multihop_latency_bench \
	sbin/cryptotest \
	$(BIN_CONSENSUS) \
	sbin/bcached \

# the following are broken in the newidc system
MODULES_x86_64_broken= \
	sbin/barriers \
	sbin/driver_msd \
	sbin/ehci \
	sbin/ipi_bench \
	sbin/ring_barriers \
	sbin/usb_manager \
	sbin/ssf_bcast \
	sbin/lamport_bcast \

# x86-32-specific module to build by default
MODULES_x86_32=\
	sbin/lpc_kbd \
	sbin/serial \
	$(BIN_RCCE_BT) \
	$(BIN_RCCE_LU) \
	sbin/rcce_pingpong \
	sbin/bench \
	sbin/fbdemo \
	sbin/fish \
	sbin/fputest \
	sbin/pci \
	sbin/skb \
	sbin/slideshow \
	sbin/thc_v_flounder_empty \
	sbin/thcidctest \
	sbin/thcminitest \
	sbin/thctest \
	sbin/vbe \
	sbin/mem_serv_dist \
	sbin/routing_setup \
	sbin/multihoptest \
	sbin/multihop_latency_bench \

# SCC-specific module to build by default
MODULES_scc=\
	$(BIN_RCCE_BT) \
	$(BIN_RCCE_LU) \
	sbin/rcce_pingpong \
	sbin/bench \
	sbin/eMAC \
	sbin/netd \
	sbin/webserver \
	sbin/ipirc_test \
	sbin/thc_v_flounder_empty \
	sbin/thcidctest \
	sbin/thcminitest \
	sbin/thctest \
	sbin/mem_serv_dist \
	sbin/net-test \
	sbin/netthroughput \
	sbin/udp_throughput \

# ARM-specific modules to build by default
MODULES_arm=\
	sbin/cpu.bin

# XScale-specific modules to build by default
MODULES_xscale=\
	sbin/cpu.bin
	
# ArmGem5-specific modules to build by default
#MODULES_arm_gem5=\

# ARM11MP-specific modules to build by default
MODULES_arm11mp=\
	sbin/cpu.bin

# construct list of all modules to be built (arch-specific and common for each arch)
MODULES=$(foreach a,$(HAKE_ARCHS),$(foreach m,$(MODULES_$(a)),$(a)/$(m)) \
                                  $(foreach m,$(MODULES_COMMON),$(a)/$(m))) \
        $(MODULES_GENERIC)

all: $(MODULES) menu.lst
	@echo "You've just run the default ("all") target for Barrelfish"
	@echo "using Hake.  The following modules have been built:"
	@echo $(MODULES)
	@echo "If you want to change this target, edit the file called"
	@echo "'symbolic_targets.mk' in your build directory."
.PHONY: all

# XXX: this should be overridden in some local settings file?
INSTALL_PREFIX ?= /home/netos/tftpboot/$(USER)

# Only install a binary if it doesn't exist in INSTALL_PREFIX or the modification timestamp differs
install: $(MODULES)
	for m in ${MODULES}; do \
	  if [ ! -f ${INSTALL_PREFIX}/$$m ] || \
	      [ $$(stat -c%Y $$m) -ne $$(stat -c%Y ${INSTALL_PREFIX}/$$m) ]; then \
	    echo Installing $$m; \
	    mkdir -p ${INSTALL_PREFIX}/$$(dirname $$m); \
	    install -p $$m ${INSTALL_PREFIX}/$$m; \
	  fi; \
	done;
.PHONY : install

sim: simulate
.PHONY : sim

QEMU=unknown-arch-error
GDB=unknown-arch-error
CLEAN_HD=

DISK=hd.img
AHCI=-device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -drive id=disk,file=$(DISK),if=none

ifeq ($(ARCH),x86_64)
        QEMU_CMD=qemu-system-x86_64 -smp 2 -m 1024 -net nic,model=ne2k_pci -net user $(AHCI) -fda $(SRCDIR)/tools/grub-qemu.img -tftp $(PWD) -nographic
	GDB=x86_64-pc-linux-gdb
	CLEAN_HD=qemu-img create $(DISK) 10M
else ifeq ($(ARCH),x86_32)
        QEMU_CMD=qemu-system-i386 -smp 2 -m 1024 -net nic,model=ne2k_pci -net user -fda $(SRCDIR)/tools/grub-qemu.img -tftp $(PWD) -nographic
	GDB=gdb
else ifeq ($(ARCH),scc)
        QEMU_CMD=qemu-system-i386 -smp 2 -m 1024 -cpu pentium -net nic,model=ne2k_pci -net user -fda $(SRCDIR)/tools/grub-qemu.img -tftp $(PWD) -nographic
	GDB=gdb
else ifeq ($(ARCH),arm)
	ARM_QEMU_CMD=qemu-system-arm -kernel arm/sbin/cpu.bin -nographic -no-reboot -m 256 -initrd arm/romfs.cpio
	GDB=xterm -e arm-none-linux-gnueabi-gdb
simulate: $(MODULES) arm/romfs.cpio
	$(ARM_QEMU_CMD)
.PHONY: simulate

arm/tools/debug.arm.gdb: $(SRCDIR)/tools/debug.arm.gdb
	cp $< $@

debugsim: $(MODULES) arm/romfs.cpio arm/tools/debug.arm.gdb
	$(SRCDIR)/tools/debug.sh "$(ARM_QEMU_CMD) -initrd arm/romfs.cpio" "$(GDB)" "-s $(ARCH)/sbin/cpu -x arm/tools/debug.arm.gdb $(GDB_ARGS)"
.PHONY : debugsim
else ifeq ($(ARCH),arm11mp)
	QEMU_CMD=qemu-system-arm -cpu mpcore -M realview -kernel arm11mp/sbin/cpu.bin
	GDB=arm-none-linux-gnueabi-gdb
endif


ifdef QEMU_CMD

simulate: $(MODULES)
	$(CLEAN_HD)
	$(QEMU_CMD)
.PHONY : simulate

debugsim: $(MODULES) $(ARCH)/debug.gdb
	$(SRCDIR)/tools/debug.sh "$(QEMU_CMD)" "$(GDB)" "-x $(ARCH)/debug.gdb $(GDB_ARGS)" "file:/dev/stdout"
.PHONY : debugsim

debugsimvga: $(MODULES)
	$(QEMU_CMD) -s -S &
	while [ ! `netstat -nlp 2>/dev/null | grep qemu` ]; do sleep 1; done
	$(GDB) -x $(SRCDIR)/debug.gdb.in
.PHONY : debugsimvga

endif



$(ARCH)/menu.lst: $(SRCDIR)/hake/menu.lst.$(ARCH)
	cp $< $@

$(ARCH)/romfs.cpio: $(SRCDIR)/tools/arm-mkbootcpio.sh $(MODULES) $(ARCH)/menu.lst
	$(SRCDIR)/tools/arm-mkbootcpio.sh $(ARCH)/menu.lst $@

# Location of hardcoded size of romfs CPIO image
arm_romfs_cpio = "$(ARCH)/include/romfs_size.h"

# XXX: Horrid hack to hardcode size of romfs CPIO image into ARM kernel
# This works in several recursive make steps:
# 1. Create a dummy romfs_size.h header file
# 2. Compile everything
# 3. Create the romfs CPIO image
# 4. Determine its size and write to romfs_size.h
# 5. Re-compile kernel (but not the romfs) with correct size information
# 6. Install romfs to installation location
arm:
	$(MAKE)
	$(MAKE) $(ARCH)/romfs.cpio
	echo "//Autogenerated size of romfs.cpio because the bootloader cannot calculate it" > $(arm_romfs_cpio)
	echo "size_t romfs_cpio_archive_size = `ls -asl $(ARCH)/romfs.cpio | sed -e 's/ /\n/g' | head -6 | tail -1`;" >> $(arm_romfs_cpio)
	$(MAKE)
.PHONY: arm

# Builds a dummy romfs_size.h
$(ARCH)/include/romfs_size.h:
	echo "size_t romfs_cpio_archive_size = 0; //should not see this" > $@

arminstall:
	$(MAKE) arm
	$(MAKE) install
	install -p $(ARCH)/romfs.cpio ${INSTALL_PREFIX}/$(ARCH)/romfs.cpio
.PHONY: arminstall

# Copy the scc-specific menu.lst from the source directory to the build directory
menu.lst.scc: $(SRCDIR)/hake/menu.lst.scc
	cp $< $@

scc: all tools/bin/dite menu.lst.scc
	$(shell find scc/sbin -type f -print0 | xargs -0 strip -d)
	tools/bin/dite -32 -o bigimage.dat menu.lst.scc
	cp $(SRCDIR)/tools/scc/bootvector.dat .
	bin2obj -m $(SRCDIR)/tools/scc/bigimage.map -o barrelfish0.obj
	@echo Taking the barrelfish.obj file to SCC host
	scp barrelfish0.obj user@tomme1.in.barrelfish.org:

# M5 Simulation targets

menu.lst.m5: $(SRCDIR)/hake/menu.lst.m5
	cp $< $@

m5script.py: $(SRCDIR)/tools/molly/m5script.py
	cp $< $@

m5_kernel: $(MODULES) menu.lst.m5 tools/bin/molly m5script.py
	$(SRCDIR)/tools/molly/build_data_files.sh menu.lst.m5 m5_tmp
	tools/bin/molly menu.lst.m5 m5_kernel.c
	cc -std=c99 -Wl,-N -pie -fno-builtin -nostdlib -Wl,--build-id=none -Wl,--fatal-warnings -m64 -fPIC -T$(SRCDIR)/tools/molly/molly_ld_script -I$(SRCDIR)/include -I$(SRCDIR)/include/arch/x86_64 -I./x86_64/include -imacros $(SRCDIR)/include/deputy/nodeputy.h $(SRCDIR)/tools/molly/molly_boot.S $(SRCDIR)/tools/molly/molly_init.c ./m5_kernel.c $(SRCDIR)/lib/elf/elf64.c ./m5_tmp/* -o m5_kernel
	@echo "Now invoke m5, e.g. as 'm5.fast m5script.py  --num_cpus=2 --kernel=m5_kernel'"

m5: m5_kernel m5script.py
	@echo "** Starting M5.  Note that (i) the number of CPUs started"
	@echo "** on the M5 command line should match the number passed"
	@echo "** to spawnd, and (ii) the mmaps configured in menu.lst.m5"
	@echo "** should match the physical memory configured in the"
	@echo "** simulation script."
	m5.fast m5script.py --num_cpus=2 --kernel=m5_kernel
.PHONY : m5

# Source indexing targets
cscope.files:
	find $(abspath .) $(abspath $(SRCDIR)) -name '*.[ch]' -type f -print | sort | uniq > $@
.PHONY: cscope.files

cscope.out: cscope.files
	cscope -k -b -i$<

TAGS: cscope.files
#	etags - < $<
	etags -L cscope.files # for emacs
	ctags -L cscope.files -o TAGS_VI # for vim

# force rebuild of the Makefile
rehake: ./hake/hake
	./hake/hake --source-dir $(SRCDIR)
.PHONY: rehake

clean::
	$(RM) -r tools $(HAKE_ARCHS)
.PHONY: clean

realclean:: clean
	$(RM) hake/*.o hake/*.hi hake/hake Hakefiles.hs cscope.*
.PHONY: realclean

# Documentation
DOCS= \
	./docs/TN-000-Overview.pdf \
	./docs/TN-001-Glossary.pdf \
	./docs/TN-002-Mackerel.pdf \
	./docs/TN-003-Hake.pdf \
	./docs/TN-004-VirtualMemory.pdf \
	./docs/TN-005-SCC.pdf \
	./docs/TN-006-Routing.pdf \
	./docs/TN-008-Tracing.pdf \
	./docs/TN-009-Notifications.pdf \
	./docs/TN-010-Spec.pdf \
	./docs/TN-011-IDC.pdf \
	./docs/TN-012-Services.pdf \
	./docs/TN-013-CapabilityManagement.pdf \
	./docs/TN-014-bulk-transfer.pdf \
	./docs/TN-015-DiskDriverArchitecture.pdf \

docs doc: $(DOCS)
.PHONY: docs doc

clean::
	$(RM) $(DOCS)
.PHONY: clean

doxygen: Doxyfile
	doxygen $<
.PHONY: doxygen

# pretend to be CMake's CONFIGURE_FILE command
# TODO: clean this up
Doxyfile: $(SRCDIR)/doc/Doxyfile.cmake
	sed -r 's#@CMAKE_SOURCE_DIR@#$(SRCDIR)#g' $< > $@

# Scheduler simulator test cases
RUNTIME = 1000
TESTS = $(addsuffix .txt,$(basename $(wildcard $(SRCDIR)/tools/schedsim/*.cfg)))

schedsim-regen: $(TESTS)

$(TESTS): %.txt: %.cfg tools/bin/simulator
	tools/bin/simulator $< $(RUNTIME) > $@

schedsim-check: $(wildcard $(SRCDIR)/tools/schedsim/*.cfg)
	for f in $^; do tools/bin/simulator $$f $(RUNTIME) | diff -q - `dirname $$f`/`basename $$f .cfg`.txt || exit 1; done
